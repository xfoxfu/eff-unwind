#include <libunwind.h>
#include <unwind.h>
#include <unwind_itanium.h>
#include <cassert>
#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <scope_guard.hpp>
#include <type_traits>
#include <typeindex>
#include <vector>
#include "fmt/core.h"

#ifdef EFF_UNWIND_TRACE
void print_frames(const char* prefix);

void print_memory(const char* start, const char* end);

void print_proc(const char* name, unw_cursor_t& proc_info);
#endif

// FIXME: make the templates support `void`
class unit_t {
 public:
  operator unw_word_t() { return 0; }
  operator char() { return '-'; }
};
#ifdef EFF_UNWIND_TRACE
template <>
struct fmt::formatter<unit_t> : formatter<string_view> {
  // parse is inherited from formatter<string_view>.

  auto format(unit_t c, format_context& ctx) const -> format_context::iterator;
};
#endif

template <typename Raise, typename Resume>
class effect {
 public:
  typedef Raise raise_t;
  typedef Resume resume_t;
};

template <typename T>
concept is_effect =
    std::is_base_of_v<effect<typename T::raise_t, typename T::resume_t>, T>;

template <typename Effect>
  requires(is_effect<Effect>)
Effect::resume_t raise(typename Effect::raise_t value);

struct handler_frame_base;

struct handler_resumption_frame;

template <typename resume_t>
struct raise_context;

template <typename resume_t, typename yield_t>
class resume_context {
  handler_frame_base& handler_frame;
  raise_context<resume_t>& ctx;

 public:
  resume_context(handler_frame_base& handler_sp, raise_context<resume_t>& ctx)
      : handler_frame(handler_sp), ctx(ctx) {}
  yield_t operator()(resume_t value);
};

template <typename yield_t>
class yield_context {
  handler_frame_base& frame;

 public:
  yield_context(handler_frame_base& frame);
  [[noreturn]] void operator()(yield_t value);
};

template <typename Effect, typename yield_t, typename F>
concept is_handler_of_in_fn = is_effect<Effect> &&
    std::is_invocable_v<F,
        typename Effect::raise_t,
        resume_context<Effect, yield_t>,
        yield_context<yield_t>> &&
    std::is_same_v<std::invoke_result_t<F,
                       typename Effect::raise_t,
                       resume_context<Effect, yield_t>,
                       yield_context<yield_t>>,
        typename Effect::resume_t>;

template <typename return_t, typename Effect, typename F, typename H>
  requires(is_effect<Effect> && is_handler_of_in_fn<Effect, return_t, H>)
return_t do_handle(F doo, H handler);

// ===== implementation =====

template <typename resume_t>
struct raise_context {
  jmp_buf jmp;
  resume_t value;
};

struct handler_frame_found {
  const char* effect_typename;
  unw_word_t set_x0;

  handler_frame_found(const char* effect_typename, unw_word_t set_x0)
      : effect_typename(effect_typename), set_x0(set_x0) {}
};

struct handler_resumption_frame {
  jmp_buf saved_jmp;
  std::vector<char> saved_stack;
};

struct handler_frame_base {
  std::type_index effect;
  uint64_t id;
  uintptr_t resume_fp;
  uintptr_t handler_sp;
  std::vector<handler_resumption_frame> resumption_frames;

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
      raise_context<typename Effect::resume_t>& ctx) = 0;
};

template <typename Effect>
  requires is_effect<Effect>
struct can_handle {};

extern uint64_t last_frame_id;
extern std::vector<std::unique_ptr<handler_frame_base>> frames;

template <typename Effect, typename Handler, typename yield_t>
  requires is_handler_of_in_fn<Effect, yield_t, Handler>
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
      raise_context<typename Effect::resume_t>& ctx) override {
    return handler(in,
        resume_context<typename Effect::resume_t, yield_t>(*this, ctx),
        yield_context<yield_t>(*this));
  }
};

template <typename resume_t, typename yield_t>
yield_t resume_context<resume_t, yield_t>::operator()(resume_t value) {
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
  handler_frame.resumption_frames.push_back(handler_resumption_frame());
  auto& resumption_frame = handler_frame.resumption_frames.back();
  resumption_frame.saved_stack.resize(sp2 - sp);
#ifdef EFF_UNWIND_TRACE
  print_frames("save_stack");
  fmt::println("saved stack = {:#x} - {:#x}", sp, sp2);
#endif
  std::copy(reinterpret_cast<char*>(sp), reinterpret_cast<char*>(sp2),
      resumption_frame.saved_stack.begin());
  ctx.value = value;
#ifdef EFF_UNWIND_TRACE
  fmt::println("set resume value = {}", value);
#endif
  if (setjmp(resumption_frame.saved_jmp) == 0) {
    longjmp(ctx.jmp, 1);
  } else {
#ifdef EFF_UNWIND_TRACE
    fmt::println("after resume");
#endif
    // TODO: give me yield value
    return std::move(ctx.value);
  }
}

__attribute__((always_inline)) inline void resume_nontail(ptrdiff_t,
    handler_resumption_frame*);

template <typename return_t, typename Effect, typename F, typename H>
  requires(is_effect<Effect> && is_handler_of_in_fn<Effect, return_t, H>)
return_t do_handle(F doo, H handler) {
  // The frame pointer (x29) is required for compatibility with fast stack
  // walking used by ETW and other services. It must point to the previous {x29,
  // x30} pair on the stack.
  uint64_t fp;
  asm volatile("mov %0, fp" : "=r"(fp));
  uint64_t sp;
  asm volatile("mov %0, sp" : "=r"(sp));

  auto frame = std::make_unique<handler_frame<Effect, H, return_t>>(
      typeid(Effect), handler, *reinterpret_cast<uint64_t*>(fp), sp);
  auto frame_id = frame->id;
  frames.push_back(std::move(frame));

  auto guard = sg::make_scope_guard([&] {
#ifdef EFF_UNWIND_TRACE
    print_frames("destructor");
    fmt::println("frame_count = {}", frames.back()->resumption_frames.size());
#endif
    if (frames.back()->resumption_frames.size() > 0) {
      handler_resumption_frame* frame;
      frame = new handler_resumption_frame;
      *frame = std::move(frames.back()->resumption_frames.back());
      frames.back()->resumption_frames.pop_back();

      uint64_t nsp;
      asm volatile("mov %0, sp" : "=r"(nsp));
      ptrdiff_t sp_delta = nsp - sp + frame->saved_stack.size();

#ifdef EFF_UNWIND_TRACE
      fmt::println(
          "sp = {:#x}, nsp = {:#x}, sp_delta = {:#x}", sp, nsp, sp_delta);
      fmt::println("resumption frame size = {:#x}", frame->saved_stack.size());
#endif

      resume_nontail(sp_delta, frame);
    }

    auto pop_frame_id = frames.back()->id;

    frames.pop_back();
    assert(pop_frame_id == frame_id);
  });

  return_t value = doo();

  return value;
}

template <typename Effect>
  requires(is_effect<Effect>)
Effect::resume_t raise(typename Effect::raise_t value) {
  // trick the compiler to generate exception tables
  static volatile bool never_throw = false;
  if (never_throw) {
    throw 42;
  }
  auto frame = std::find_if(frames.rbegin(), frames.rend(),
      [&](const auto& frame) { return frame->effect == typeid(Effect); });

  if (frame == frames.rend()) {
#ifdef EFF_UNWIND_TRACE
    fmt::println("reached frame end, handler not found, aborting");
#endif
    abort();
  }

  raise_context<typename Effect::resume_t> ctx;
  if (setjmp(ctx.jmp) == 0) {
    return static_cast<handler_frame_invoke<Effect>&>(**frame).invoke(
        value, ctx);
  } else {
    return std::move(ctx.value);
  }
}

constexpr _Unwind_Exception_Class EXCEPTION_CLASS = 0x58464f5845480000;

_Unwind_Reason_Code eff_stop_fn(int version,
    _Unwind_Action actions,
    _Unwind_Exception_Class exceptionClass,
    _Unwind_Exception* exceptionObject,
    struct _Unwind_Context* context,
    void* stop_parameter);

template <typename yield_t>
yield_context<yield_t>::yield_context(handler_frame_base& frame)
    : frame(frame) {}

template <typename yield_t>
void yield_context<yield_t>::operator()(yield_t value) {
  // resume is not called, meaning breaking
  // when break, need to do unwinding
  // step more to get to parent function to make caller returns
  auto target_fp = frame.resume_fp;
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
          frame.effect.name(), static_cast<unw_word_t>(value)));
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

#ifdef EFF_UNWIND_TRACE
  print_proc("unw_resume", cur_cursor);
#endif
  unw_set_reg(&cur_cursor, UNW_AARCH64_X0, static_cast<unw_word_t>(value));
  unw_resume(&cur_cursor);

  abort();  // unreachable
}

static void __resume_nontail(uint64_t sp, handler_resumption_frame* frame) {
  assert(!frame->saved_stack.empty());
#ifdef EFF_UNWIND_TRACE
  fmt::println("restore stack = {:#x} - {:#x} ({:#x})", sp,
      sp + frame->saved_stack.size(), frame->saved_stack.size());
#endif
  std::copy(frame->saved_stack.begin(), frame->saved_stack.end(),
      reinterpret_cast<char*>(sp));
  frame->saved_stack.clear();
  jmp_buf saved_jmp;
  memcpy(saved_jmp, frame->saved_jmp, sizeof(jmp_buf));
  delete frame;
  // trick the compiler it may not jump and thus could return
  static volatile bool always_jump = true;
  if (always_jump) {
    longjmp(saved_jmp, 1);
  }
}

__attribute__((always_inline)) inline void resume_nontail(ptrdiff_t sp_delta,
    handler_resumption_frame* frame) {
  asm volatile("sub sp, sp, %0" : : "r"(sp_delta));
  uint64_t sp;
  asm volatile("mov %0, sp" : "=r"(sp));
  __resume_nontail(sp, frame);
}
