#include <libunwind.h>
#include <unwind.h>
#include <unwind_itanium.h>
#include <any>
#include <cassert>
#include <functional>
#include <optional>
#include <type_traits>
#include <typeindex>
#include "scope_guard.hpp"

template <typename Raise, typename Resume>
class effect {
 public:
  typedef Raise raise_t;
  typedef Resume resume_t;
};

template <typename T>
concept is_effect =
    std::is_base_of<effect<typename T::raise_t, typename T::resume_t>, T>();

template <typename Return, typename... Effects>
  requires(is_effect<Effects> && ...)
class with_effect;

template <typename Return, typename... Effects>
  requires(is_effect<Effects> && ...)
class effect_ctx {
 public:
  typedef Return return_t;

  template <typename E>
    requires is_effect<E> && (std::is_same<Effects, E>() || ...)
  E::resume_t raise(E::raise_t in);

  // Effect::resume_t raise(Effect::raise_t in);

  with_effect<Return, Effects...> ret(Return val) {
    return with_effect<Return, Effects...>(*this, std::move(val));
  }
};

template <typename Return, typename... Effects>
  requires(is_effect<Effects> && ...)
class with_effect {
 public:
  Return value;

  with_effect(effect_ctx<Return, Effects...> ctx, Return&& value)
      : value(value) {}
};

// TODO: enable result have different type for return and resume, and enforce
// type checking
template <typename Result>
struct handler_result {
  virtual bool is_resume() = 0;
  virtual bool is_break() = 0;
  virtual ~handler_result() {}

  static std::unique_ptr<handler_result<Result>> resume(Result value);
};

template <typename Result>
struct handler_result_resume : public handler_result<Result> {
  Result value;
  handler_result_resume(Result value) : value(value) {}
  virtual bool is_resume() { return true; }
  virtual bool is_break() { return false; }
};

template <typename Result>
struct handler_result_break : public handler_result<Result> {
  Result value;
  handler_result_break(Result value) : value(value) {}
  virtual bool is_resume() { return false; }
  virtual bool is_break() { return true; }
};

template <typename Result>
std::unique_ptr<handler_result<Result>> handler_resume(Result value) {
  return std::make_unique<handler_result_resume<Result>>(value);
}

template <typename Result>
std::unique_ptr<handler_result<Result>> handler_return(Result value) {
  return std::make_unique<handler_result_break<Result>>(value);
}

template <typename Effect, typename F>
concept is_handler_of =
    is_effect<Effect> && std::is_invocable<F, typename Effect::resume_t>() &&
    std::is_same<
        std::unique_ptr<handler_result<typename Effect::resume_t>>,
        typename std::invoke_result<F, typename Effect::raise_t>::type>();

template <typename Effect, typename F>
  requires is_handler_of<Effect, F>
auto handle(F handler);

// ===== implementation =====

struct handler_frame_found {
  const char* effect_typename;
  unw_word_t set_x0;

  handler_frame_found(const char* effect_typename, unw_word_t set_x0)
      : effect_typename(effect_typename), set_x0(set_x0) {}
};

struct handler_frame_base {
  std::type_index effect;
  uint64_t id;
  unw_cursor_t resume_cursor;

  handler_frame_base(std::type_index effect, unw_cursor_t resume_cursor);

  virtual ~handler_frame_base() {}
};

template <typename Effect>
struct handler_frame_invoke : public handler_frame_base {
  handler_frame_invoke(std::type_index effect, unw_cursor_t resume_cursor)
      : handler_frame_base(effect, resume_cursor) {}

  virtual std::unique_ptr<handler_result<typename Effect::resume_t>> invoke(
      Effect::raise_t in) = 0;
};

template <typename Effect>
  requires is_effect<Effect>
struct can_handle {};

extern thread_local uint64_t last_frame_id;
extern thread_local std::vector<std::unique_ptr<handler_frame_base>> frames;

template <typename Effect, typename Handler>
  requires is_handler_of<Effect, Handler>
struct handler_frame : public can_handle<Effect>,
                       public handler_frame_invoke<Effect> {
  Handler handler;

  handler_frame(std::type_index effect,
                Handler handler,
                unw_cursor_t resume_cursor)
      : handler_frame_invoke<Effect>(effect, resume_cursor), handler(handler) {}

  virtual std::unique_ptr<handler_result<typename Effect::resume_t>> invoke(
      Effect::raise_t in) {
    return handler(in);
  }
};

template <typename Effect, typename F>
  requires is_handler_of<Effect, F>
auto handle(F handler) {
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  // step to go to the caller function
  unw_step(&cursor);
  unw_proc_info_t proc_info;
  unw_get_proc_info(&cursor, &proc_info);

  auto frame = std::make_unique<handler_frame<Effect, F>>(typeid(Effect),
                                                          handler, cursor);
  auto frame_id = frame->id;
  frames.push_back(std::move(frame));
  return sg::make_scope_guard([frame_id]() {
    auto pop_frame_id = frames.back()->id;

    frames.pop_back();
    assert(pop_frame_id == frame_id);
  });
}

constexpr _Unwind_Exception_Class EXCEPTION_CLASS = 0x58464f5845480000;

_Unwind_Reason_Code eff_stop_fn(int version,
                                _Unwind_Action actions,
                                _Unwind_Exception_Class exceptionClass,
                                _Unwind_Exception* exceptionObject,
                                struct _Unwind_Context* context,
                                void* stop_parameter);

template <typename Value, typename... Effects>
  requires(is_effect<Effects> && ...)
template <typename Effect>
  requires is_effect<Effect> && (std::is_same<Effects, Effect>() || ...)
Effect::resume_t effect_ctx<Value, Effects...>::raise(Effect::raise_t in) {
  // trick the compiler to generate exception tables
  static volatile bool never_throw = false;
  if (never_throw) {
    throw 42;
  }
  auto frame = std::find_if(
      frames.rbegin(), frames.rend(),
      [&](const auto& frame) { return frame->effect == typeid(Effect); });

  if (frame == frames.rend()) {
    std::abort();
  }

  auto result = static_cast<handler_frame_invoke<Effect>&>(**frame).invoke(in);

  if (result->is_break()) {
    // when break, need to do unwinding
    // step more to get to parent function to make caller returns
    // unw_step(&(*frame)->resume_cursor);

    unw_word_t target_sp;
    unw_get_reg(&(*frame)->resume_cursor, UNW_AARCH64_SP, &target_sp);
    unw_cursor_t cur_cursor;
    unw_context_t uc;
    unw_getcontext(&uc);
    unw_init_local(&cur_cursor, &uc);
    std::unique_ptr<_Unwind_Exception> ex =
        std::make_unique<_Unwind_Exception>();
    ex->exception_class = EXCEPTION_CLASS;
    ex->private_1 = reinterpret_cast<uintptr_t>(eff_stop_fn);  // 0;
    unw_word_t sp;
    unw_get_reg(&cur_cursor, UNW_AARCH64_SP, &sp);
    ex->private_2 = target_sp;  // sp;
    ex->exception_cleanup =
        (decltype(ex->exception_cleanup))(new handler_frame_found(
            (*frame)->effect.name(),
            dynamic_cast<handler_result_break<typename Effect::resume_t>&>(
                *result)
                .value));
    while (unw_step(&cur_cursor)) {
      unw_proc_info_t pi;
      unw_get_proc_info(&cur_cursor, &pi);

      unw_word_t fp;
      unw_get_reg(&cur_cursor, UNW_AARCH64_SP, &fp);

      // cannot find frame by comparing cursor, so compare by sp
      if (fp == target_sp) {
        break;
      }
      unw_word_t ip;
      unw_word_t sp;
      unw_word_t x0;
      unw_get_reg(&cur_cursor, UNW_AARCH64_PC, &ip);
      unw_get_reg(&cur_cursor, UNW_AARCH64_SP, &sp);
      unw_get_reg(&cur_cursor, UNW_AARCH64_X0, &x0);

      // if not break, perform cleanup
      _Unwind_Personality_Fn personality =
          reinterpret_cast<_Unwind_Personality_Fn>(pi.handler);
      if (personality != nullptr) {
        _Unwind_Reason_Code personalityResult =
            personality(1, _UA_CLEANUP_PHASE, EXCEPTION_CLASS, ex.get(),
                        reinterpret_cast<struct _Unwind_Context*>(&cur_cursor));

        if (personalityResult == _URC_INSTALL_CONTEXT) {
          unw_resume(&cur_cursor);
        }
      }
    }

    assert(false);  // unreachable
  } else if (result->is_resume()) {
    auto res = dynamic_cast<handler_result_resume<typename Effect::resume_t>&>(
        *result);
    // resume is treated as normal function return
    return res.value;
  }
  assert(false);  // unreachable
  return -1;      // unreachable
}
