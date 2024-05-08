#include <libunwind.h>
#include <unwind.h>
#include <unwind_itanium.h>
#include <any>
#include <cassert>
#include <csetjmp>
#include <cstddef>
#include <functional>
#include <optional>
#include <type_traits>
#include <typeindex>
#include <variant>
#include "fmt/core.h"
#include "scope_guard.hpp"

#ifdef EFF_UNWIND_TRACE
void print_frames(const char* prefix);

void print_memory(const char* start, const char* end);
#endif

#define RESUME(v)  \
  {                \
    ctx.resume(v); \
    return 0;      \
  }
#define RESUME_THEN_BREAK(v) \
  {                          \
    ctx.break_resume(v);     \
    return 0;                \
  }
#define BREAK(v) return (v);

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
  bool has_resume = false;
  char resume_value[std::max({sizeof(typename Effects::resume_t)...})];

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

struct handler_frame_base;

template <typename Effect>
  requires is_effect<Effect>
class resume_context {
  // TODO: try zero-copy
  bool* ctx_has_resume;
  bool* ctx_is_tail_resume;
  char* ctx_resume_value;
  handler_frame_base& handler_frame;

 public:
  resume_context(bool* ctx_has_resume,
                 bool* ctx_is_tail_resume,
                 char* ctx_resume_value,
                 handler_frame_base& handler_sp)
      : ctx_has_resume(ctx_has_resume),
        ctx_is_tail_resume(ctx_is_tail_resume),
        ctx_resume_value(ctx_resume_value),
        handler_frame(handler_sp) {}

  void break_resume(Effect::resume_t value);
  bool resume(Effect::resume_t value);
};

// TODO: check return type of handler equal to parent function
template <typename Effect, typename F>
concept is_handler_of =
    is_effect<Effect> &&
    std::is_invocable<F, typename Effect::resume_t, resume_context<Effect>>();

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
  uintptr_t resume_fp;
  uintptr_t handler_sp;

  handler_frame_base(std::type_index effect,
                     uintptr_t resume_fp,
                     uintptr_t handler_sp);

  virtual ~handler_frame_base() {}
};

template <typename Effect>
struct handler_frame_invoke : public handler_frame_base {
  handler_frame_invoke(std::type_index effect,
                       uintptr_t resume_fp,
                       uintptr_t handler_sp)
      : handler_frame_base(effect, resume_fp, handler_sp) {}

  virtual Effect::resume_t invoke(Effect::raise_t in,
                                  resume_context<Effect> ctx) = 0;
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
                uintptr_t resume_fp,
                uintptr_t handler_sp)
      : handler_frame_invoke<Effect>(effect, resume_fp, handler_sp),
        handler(handler) {}

  virtual Effect::resume_t invoke(Effect::raise_t in,
                                  resume_context<Effect> ctx) {
    return handler(in, ctx);
  }
};

// FIXME: non-global jmp_buf, saved stack
extern thread_local jmp_buf SAVED_JMP;
extern thread_local std::vector<char> SAVED_STACK;
extern thread_local bool NO_SAVED;

template <typename Effect>
  requires is_effect<Effect>
void resume_context<Effect>::break_resume(typename Effect::resume_t value) {
  *ctx_has_resume = true;
  *ctx_is_tail_resume = true;
  *reinterpret_cast<Effect::resume_t*>(ctx_resume_value) = value;
}

template <typename Effect>
  requires is_effect<Effect>
bool resume_context<Effect>::resume(typename Effect::resume_t value) {
  *ctx_has_resume = true;
  *ctx_is_tail_resume = false;
  *reinterpret_cast<Effect::resume_t*>(ctx_resume_value) = value;
  unw_word_t sp;
  unw_cursor_t cur_cursor;
  unw_context_t uc;
  unw_getcontext(&uc);
  unw_init_local(&cur_cursor, &uc);
  unw_get_reg(&cur_cursor, UNW_AARCH64_SP, &sp);
  // https://community.arm.com/arm-community-blogs/b/architectures-and-processors-blog/posts/using-the-stack-in-aarch32-and-aarch64
  // The stack is full-descending, meaning that sp – the stack pointer –
  // points to the most recently pushed object on the stack, and it grows
  // downwards, towards lower addresses.
  auto sp2 = handler_frame.handler_sp;
  SAVED_STACK.clear();
  SAVED_STACK.resize(sp2 - sp);
  std::copy(reinterpret_cast<char*>(sp), reinterpret_cast<char*>(sp2),
            SAVED_STACK.begin());
  if (setjmp(SAVED_JMP) != 0) {
    *ctx_has_resume = false;
    return false;
  }
  return true;
}

template <typename Effect, typename F>
  requires is_handler_of<Effect, F>
__attribute__((always_inline)) auto handle(F handler) {
  // The frame pointer (x29) is required for compatibility with fast stack
  // walking used by ETW and other services. It must point to the previous {x29,
  // x30} pair on the stack.
  uint64_t fp;
  asm volatile("mov %0, fp" : "=r"(fp));

  uint64_t sp;
  asm volatile("mov %0, sp" : "=r"(sp));

  auto frame = std::make_unique<handler_frame<Effect, F>>(
      typeid(Effect), handler, *reinterpret_cast<uint64_t*>(fp), sp);
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

  resume_context<Effect> rctx(&has_resume, &NO_SAVED,
                              reinterpret_cast<char*>(&resume_value), **frame);
  auto result =
      static_cast<handler_frame_invoke<Effect>&>(**frame).invoke(in, rctx);

  // resume is called
  if (has_resume) {
    has_resume = false;
    return *reinterpret_cast<uint64_t*>(&resume_value);
  }

  // resume is not called, meaning breaking
  // when break, need to do unwinding
  // step more to get to parent function to make caller returns
  auto target_fp = (**frame).resume_fp;
  unw_cursor_t cur_cursor;
  unw_context_t uc;
  unw_getcontext(&uc);
  unw_init_local(&cur_cursor, &uc);
  std::unique_ptr<_Unwind_Exception> ex = std::make_unique<_Unwind_Exception>();
  ex->exception_class = EXCEPTION_CLASS;
  ex->private_1 = reinterpret_cast<uintptr_t>(eff_stop_fn);
  unw_word_t sp;
  unw_get_reg(&cur_cursor, UNW_AARCH64_SP, &sp);
  ex->private_2 = target_fp;
  ex->exception_cleanup =
      (decltype(ex->exception_cleanup))(new handler_frame_found(
          (*frame)->effect.name(), static_cast<unw_word_t>(result)));
  int unw_error;
  while ((unw_error = unw_step(&cur_cursor)) > 0) {
    unw_proc_info_t pi;
    unw_get_proc_info(&cur_cursor, &pi);

    unw_word_t fp;
    unw_get_reg(&cur_cursor, UNW_AARCH64_FP, &fp);

    // cannot find frame by comparing cursor, so compare by sp
    if (fp == target_fp) {
      break;
    }

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

  fmt::println("unreachable! = {}", unw_error);
  assert(false);  // unreachable
  return static_cast<typename Effect::resume_t>(NULL);
}

static void __resume_nontail(uint64_t sp) {
  if (NO_SAVED) {
    return;
  }
  assert(!SAVED_STACK.empty());
  // asm volatile("sub sp, sp, %0" : : "r"(SAVED_STACK.size()));
  std::copy(SAVED_STACK.begin(), SAVED_STACK.end(),
            reinterpret_cast<char*>(sp));
  // trick the compiler it may not jump and thus could return
  static volatile bool always_jump = true;
  if (always_jump) {
    longjmp(SAVED_JMP, 1);
  }
}

__attribute__((always_inline)) inline void resume_nontail() {
  auto stack_size = SAVED_STACK.size();
  asm volatile("sub sp, sp, %0" : : "r"(stack_size));
  uint64_t sp;
  asm volatile("mov %0, sp" : "=r"(sp));
  __resume_nontail(sp);
}
