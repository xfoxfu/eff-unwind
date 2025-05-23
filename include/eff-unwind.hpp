#include <libunwind.h>
#include <setjmp.h>
#include <unwind.h>
#include <unwind_itanium.h>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <scope_guard.hpp>
#include <type_traits>
#include <typeindex>
#include <vector>

#ifdef EFF_UNWIND_TRACE
#include "fmt/core.h"
#endif

#ifdef EFF_UNWIND_TRACE
void print_frames(const char* prefix);
void print_memory(const char* start, const char* end);
void print_proc(const char* name, unw_cursor_t& proc_info);
std::string demangle(const char* name);
#endif

// TODO: make the templates support `void`
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

template <typename Effect>
  requires is_effect<Effect>
class saved_resumption {
  sigjmp_buf saved_jmp;
  std::vector<char> saved_stack;

 public:
  void resume(Effect::resume_t value) &&;
};

template <typename Effect, typename yield_t>
  requires is_effect<Effect>
class resume_context {
  handler_frame_base& handler_frame;
  raise_context<typename Effect::resume_t>& ctx;

 public:
  resume_context(handler_frame_base& handler_sp,
      raise_context<typename Effect::resume_t>& ctx)
      : handler_frame(handler_sp), ctx(ctx) {}
  yield_t operator()(Effect::resume_t value);

  saved_resumption<Effect> save();
};

template <typename Effect, typename yield_t>
  requires(is_effect<Effect> && sizeof(yield_t) <= sizeof(uint64_t) * 2)
class yield_context {
  handler_frame_base& frame;

 public:
  yield_context(handler_frame_base& frame);
  [[noreturn]] void operator()(yield_t value);
};

template <typename Effect, typename yield_t, typename F>
concept is_handler_of_in_fn = is_effect<Effect> &&
    (std::is_same_v<std::invoke_result_t<F,
                        typename Effect::raise_t,
                        resume_context<Effect, yield_t>,
                        yield_context<Effect, yield_t>>,
         typename Effect::resume_t> ||
        std::is_same_v<std::invoke_result_t<F,
                           typename Effect::raise_t,
                           yield_context<Effect, yield_t>>,
            typename Effect::resume_t> ||
        std::is_same_v<std::invoke_result_t<F,
                           typename Effect::raise_t,
                           resume_context<Effect, yield_t>>,
            typename Effect::resume_t> ||
        std::is_same_v<std::invoke_result_t<F, typename Effect::raise_t>,
            typename Effect::resume_t>);

template <typename return_t, typename Effect, typename F, typename H>
  requires(is_effect<Effect> && is_handler_of_in_fn<Effect, return_t, H>)
// set optnone to prevent inlining & IPA const fold
__attribute__((optnone)) return_t do_handle(F&& doo, H&& handler);

// ===== implementation =====

template <typename resume_t>
struct raise_context {
  sigjmp_buf jmp;
  resume_t value;
};

struct handler_frame_found {
  const char* effect_typename;
  unw_word_t set_x0;
  unw_word_t set_x1;

  handler_frame_found(const char* effect_typename,
      unw_word_t set_x0,
      unw_word_t set_x1)
      : effect_typename(effect_typename), set_x0(set_x0), set_x1(set_x1) {}
};

struct handler_resumption_frame {
  sigjmp_buf saved_jmp;
  std::vector<char> saved_stack;
  std::vector<std::shared_ptr<handler_frame_base>> saved_frames;
};

struct handler_frame_base {
  std::type_index effect_type;
  uint64_t id;
  uintptr_t resume_fp;
  uintptr_t handler_sp;
  std::vector<handler_resumption_frame> resumption_frames;
  uintptr_t parent_delta;
  bool is_tail_resumptive;

  handler_frame_base(std::type_index effect,
      uintptr_t resume_fp,
      uintptr_t handler_sp);

  virtual ~handler_frame_base();
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

template <typename Effect, typename yield_t>
struct handler_frame_yield : public handler_frame_invoke<Effect> {
  yield_t yield_value;

  handler_frame_yield(std::type_index effect,
      uintptr_t resume_fp,
      uintptr_t handler_sp)
      : handler_frame_invoke<Effect>(effect, resume_fp, handler_sp) {}
};

extern uint64_t last_frame_id;
extern std::vector<std::shared_ptr<handler_frame_base>> frames;

template <typename Effect, typename Handler, typename yield_t>
  requires is_handler_of_in_fn<Effect, yield_t, Handler> &&
    std::invocable<Handler,
        typename Effect::raise_t,
        resume_context<Effect, yield_t>,
        yield_context<Effect, yield_t>>
struct handler_frame : public handler_frame_yield<Effect, yield_t> {
  Handler handler;

  handler_frame(std::type_index effect,
      Handler&& handler,
      uintptr_t resume_fp,
      uintptr_t handler_sp)
      : handler_frame_yield<Effect, yield_t>(effect, resume_fp, handler_sp),
        handler(handler) {
    handler_frame_base::is_tail_resumptive = false;
  }

  virtual Effect::resume_t invoke(Effect::raise_t in,
      raise_context<typename Effect::resume_t>& ctx) override {
    return handler(in, resume_context<Effect, yield_t>(*this, ctx),
        yield_context<Effect, yield_t>(*this));
  }
};

template <typename Effect, typename Handler, typename yield_t>
  requires is_handler_of_in_fn<Effect, yield_t, Handler> &&
    std::invocable<Handler, typename Effect::raise_t>
struct handler_frame2 : public handler_frame_yield<Effect, yield_t> {
  Handler handler;

  handler_frame2(std::type_index effect,
      Handler&& handler,
      uintptr_t resume_fp,
      uintptr_t handler_sp)
      : handler_frame_yield<Effect, yield_t>(effect, resume_fp, handler_sp),
        handler(handler) {
    handler_frame_base::is_tail_resumptive = true;
  }

  virtual Effect::resume_t invoke(Effect::raise_t in,
      raise_context<typename Effect::resume_t>&) override {
    return handler(in);
  }
};

template <typename Effect, typename Handler, typename yield_t>
  requires is_handler_of_in_fn<Effect, yield_t, Handler> &&
    std::invocable<Handler,
        typename Effect::raise_t,
        resume_context<Effect, yield_t>>
struct handler_frame3 : public handler_frame_yield<Effect, yield_t> {
  Handler handler;

  handler_frame3(std::type_index effect,
      Handler&& handler,
      uintptr_t resume_fp,
      uintptr_t handler_sp)
      : handler_frame_yield<Effect, yield_t>(effect, resume_fp, handler_sp),
        handler(handler) {
    handler_frame_base::is_tail_resumptive = false;
  }

  virtual Effect::resume_t invoke(Effect::raise_t in,
      raise_context<typename Effect::resume_t>& ctx) override {
    return handler(in, resume_context<Effect, yield_t>(*this, ctx));
  }
};

template <typename Effect, typename Handler, typename yield_t>
  requires is_handler_of_in_fn<Effect, yield_t, Handler> &&
    std::invocable<Handler,
        typename Effect::raise_t,
        yield_context<Effect, yield_t>>
struct handler_frame4 : public handler_frame_yield<Effect, yield_t> {
  Handler handler;

  handler_frame4(std::type_index effect,
      Handler&& handler,
      uintptr_t resume_fp,
      uintptr_t handler_sp)
      : handler_frame_yield<Effect, yield_t>(effect, resume_fp, handler_sp),
        handler(handler) {
    handler_frame_base::is_tail_resumptive = true;
  }

  virtual Effect::resume_t invoke(Effect::raise_t in,
      raise_context<typename Effect::resume_t>&) override {
    return handler(in, yield_context<Effect, yield_t>(*this));
  }
};

inline uintptr_t get_fp() {
  uint64_t fp;
  asm volatile("mov %0, fp" : "=r"(fp));
  return fp;
}

inline uintptr_t get_sp() {
  uint64_t sp;
  asm volatile("mov %0, sp" : "=r"(sp));
  return sp;
}

template <typename Effect, typename yield_t>
  requires is_effect<Effect>
yield_t resume_context<Effect, yield_t>::operator()(Effect::resume_t value) {
  auto sp = get_sp();
  // https://community.arm.com/arm-community-blogs/b/architectures-and-processors-blog/posts/using-the-stack-in-aarch32-and-aarch64
  // The stack is full-descending, meaning that sp – the stack pointer –
  // points to the most recently pushed object on the stack, and it grows
  // downwards, towards lower addresses.
  auto sp2 = handler_frame.handler_sp;
  handler_frame.resumption_frames.push_back(handler_resumption_frame());
  auto& resumption_frame = handler_frame.resumption_frames.back();
  resumption_frame.saved_stack.resize(sp2 - sp);
  resumption_frame.saved_frames = frames;
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
  if (sigsetjmp(resumption_frame.saved_jmp, 0) == 0) {
    siglongjmp(ctx.jmp, 1);
  } else {
#ifdef EFF_UNWIND_TRACE
    fmt::println("after resume");
#endif
    auto yield_value =
        static_cast<handler_frame_yield<Effect, yield_t>&>(handler_frame)
            .yield_value;
#ifdef EFF_UNWIND_TRACE
    fmt::println("resume_context returning = {}", yield_value);
#endif
    return std::move(yield_value);
  }
}

__attribute__((always_inline)) inline void resume_remain(ptrdiff_t,
    handler_resumption_frame*);

template <typename yield_t, typename Effect, typename Handler>
  requires is_handler_of_in_fn<Effect, yield_t, Handler> &&
    std::invocable<Handler,
        typename Effect::raise_t,
        resume_context<Effect, yield_t>,
        yield_context<Effect, yield_t>>
inline std::shared_ptr<handler_frame_base> make_handler_frame(Handler&& handler,
    uint64_t fp,
    uint64_t sp) {
  return std::make_shared<handler_frame<Effect, Handler, yield_t>>(
      typeid(Effect), std::forward<Handler>(handler),
      *reinterpret_cast<uint64_t*>(fp), sp);
}
template <typename yield_t, typename Effect, typename Handler>
  requires is_handler_of_in_fn<Effect, yield_t, Handler> &&
    std::invocable<Handler, typename Effect::raise_t>
inline std::shared_ptr<handler_frame_base> make_handler_frame(Handler&& handler,
    uint64_t fp,
    uint64_t sp) {
  return std::make_shared<handler_frame2<Effect, Handler, yield_t>>(
      typeid(Effect), std::forward<Handler>(handler),
      *reinterpret_cast<uint64_t*>(fp), sp);
}
template <typename yield_t, typename Effect, typename Handler>
  requires is_handler_of_in_fn<Effect, yield_t, Handler> &&
    std::invocable<Handler,
        typename Effect::raise_t,
        resume_context<Effect, yield_t>>
inline std::shared_ptr<handler_frame_base> make_handler_frame(Handler&& handler,
    uint64_t fp,
    uint64_t sp) {
  return std::make_shared<handler_frame3<Effect, Handler, yield_t>>(
      typeid(Effect), std::forward<Handler>(handler),
      *reinterpret_cast<uint64_t*>(fp), sp);
}
template <typename yield_t, typename Effect, typename Handler>
  requires is_handler_of_in_fn<Effect, yield_t, Handler> &&
    std::invocable<Handler,
        typename Effect::raise_t,
        yield_context<Effect, yield_t>>
inline std::shared_ptr<handler_frame_base> make_handler_frame(Handler&& handler,
    uint64_t fp,
    uint64_t sp) {
  return std::make_shared<handler_frame4<Effect, Handler, yield_t>>(
      typeid(Effect), std::forward<Handler>(handler),
      *reinterpret_cast<uint64_t*>(fp), sp);
}

template <typename return_t, typename Effect, typename F, typename H>
  requires(is_effect<Effect> && is_handler_of_in_fn<Effect, return_t, H>)
return_t do_handle(F&& doo, H&& handler) {
  // The frame pointer (x29) is required for compatibility with fast stack
  // walking used by ETW and other services. It must point to the previous {x29,
  // x30} pair on the stack.
  auto fp = get_fp();
  auto sp = get_sp();

  auto frame = make_handler_frame<return_t, Effect>(handler, fp, sp);
#ifndef NDEBUG
  auto frame_id = frame->id;
#endif
  frames.push_back(std::move(frame));
  auto& frame_yield =
      static_cast<handler_frame_yield<Effect, return_t>&>(*frames.back());

  auto guard = sg::make_scope_guard([=] {
#ifdef EFF_UNWIND_TRACE
    auto& frame = frames.back();
    fmt::println("destructor for handler of {} [{:#x}]",
        demangle(frame->effect.name()), fp);
    print_frames("destructor");
    fmt::println(
        "resumption frame_count = {}", frame->resumption_frames.size());
#endif
    if (frames.back()->resumption_frames.size() > 0) {
      handler_resumption_frame* rframe;
      rframe = new handler_resumption_frame;
      *rframe = std::move(frames.back()->resumption_frames.back());
      frames.back()->resumption_frames.pop_back();
      frames = rframe->saved_frames;

      uint64_t nsp;
      asm volatile("mov %0, sp" : "=r"(nsp));
      ptrdiff_t sp_delta = nsp - sp + rframe->saved_stack.size();

#ifdef EFF_UNWIND_TRACE
      fmt::println(
          "sp = {:#x}, nsp = {:#x}, sp_delta = {:#x}", sp, nsp, sp_delta);
      fmt::println("resumption frame size = {:#x}", rframe->saved_stack.size());
#endif

      resume_remain(sp_delta, rframe);
    }

#ifndef NDEBUG
    auto pop_frame_id = frames.back()->id;
#endif
#ifdef EFF_UNWIND_TRACE
    fmt::println(
        "remove handler for {}", demangle(frames.back()->effect.name()));
#endif
    frames.pop_back();
    assert(pop_frame_id == frame_id);
  });

  return_t value = doo();
  frame_yield.yield_value = std::move(value);

  return frame_yield.yield_value;
}

template <typename Effect>
  requires(is_effect<Effect>)
Effect::resume_t raise(typename Effect::raise_t value) {
  // trick the compiler to generate exception tables
  static volatile bool never_throw = false;
  if (never_throw) {
    throw 42;
  }
#ifdef EFF_UNWIND_TRACE
  fmt::println("finding handler for {}", demangle(typeid(Effect).name()));
#endif
  auto frame = frames.rbegin();
  for (; frame != frames.rend(); frame++) {
#ifdef EFF_UNWIND_TRACE
    fmt::println("try handler for {}, found {}",
        demangle(typeid(Effect).name()), demangle((*frame)->effect.name()));
#endif
    if ((*frame)->parent_delta > 0) {
#ifdef EFF_UNWIND_TRACE
      fmt::println("search backward by {}", (*frame)->parent - 1);
#endif
      frame += (*frame)->parent_delta - 1;
    } else if ((*frame)->effect_type == typeid(Effect)) {
      break;
    }
  }

  if (frame == frames.rend()) {
#ifdef EFF_UNWIND_TRACE
    fmt::println("reached frame end, handler not found, aborting");
#endif
    abort();
  }

  auto& hframe = static_cast<handler_frame_invoke<Effect>&>(**frame);
  raise_context<typename Effect::resume_t> ctx;
  // This will affect performance by
  // 2.320 s ±  0.010 s vs 2.510 s ±  0.039 s
  auto delta = frame - frames.rbegin() + 1;
  auto& pframe = frames.back();
  auto eparent = pframe->parent_delta;
  pframe->parent_delta = delta;
  auto sg = pframe->parent_delta != eparent
      ? std::make_optional(sg::make_scope_guard(
            [&pframe, eparent]() { pframe->parent_delta = eparent; }))
      : std::nullopt;
  // avoid setjmp if tail-resuming
  if ((*frame)->is_tail_resumptive) {
    return hframe.invoke(value, ctx);
  }
  // 1.580 s ±  0.006 s vs 69.526 s (setjmp)
  // use sigsetjmp to avoid sigprocmask to be faster (2.360 s ±  0.027 s)
  if (sigsetjmp(ctx.jmp, 0) == 0) {
    return hframe.invoke(value, ctx);
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

template <typename Effect, typename yield_t>
  requires(is_effect<Effect> && sizeof(yield_t) <= sizeof(uint64_t) * 2)
yield_context<Effect, yield_t>::yield_context(handler_frame_base& frame)
    : frame(frame) {}

template <typename Effect, typename yield_t>
  requires(is_effect<Effect> && sizeof(yield_t) <= sizeof(uint64_t) * 2)
void yield_context<Effect, yield_t>::operator()(yield_t value) {
  static_cast<handler_frame_yield<effect<Effect, yield_t>, yield_t>&>(frame)
      .yield_value = value;
#ifdef EFF_UNWIND_TRACE
  fmt::println("yielding [{}]", demangle(frame.effect.name()));
  print_frames("yield");
#endif
  // resume is not called, meaning breaking
  // when break, need to do unwinding
  // step more to get to parent function to make caller returns
  auto target_fp = frame.resume_fp;
#ifdef EFF_UNWIND_TRACE
  fmt::println("target fp={:#x}", target_fp);
#endif
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
  uint64_t set_x[2];
  memcpy(set_x, &value, sizeof(value));
  unw_word_t set_x0 = set_x[0];
  unw_word_t set_x1 = set_x[1];
#ifdef EFF_UNWIND_TRACE
  fmt::println("set_x0 = {:#x}, set_x1 = {:#x}", set_x0, set_x1);
#endif
  ex->exception_cleanup =
      (decltype(ex->exception_cleanup))(new handler_frame_found(
          frame.effect_type.name(), set_x0, set_x1));
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
#ifdef EFF_UNWIND_TRACE
        fmt::println("install context for fp={:#x}", fp);
#endif
        unw_resume(&cur_cursor);
      }
    }
  }

#ifdef EFF_UNWIND_TRACE
  print_proc("unw_resume", cur_cursor);
  fmt::println("unw_resume set_x0 = {:#x}, set_x1 = {:#x}", set_x0, set_x1);
#endif
  unw_set_reg(&cur_cursor, UNW_AARCH64_X0, set_x0);
  unw_set_reg(&cur_cursor, UNW_AARCH64_X1, set_x1);
  unw_resume(&cur_cursor);

  assert(false);  // unreachable
  abort();
}

static void __resume_remain(uint64_t sp, handler_resumption_frame* frame) {
  assert(!frame->saved_stack.empty());
#ifdef EFF_UNWIND_TRACE
  fmt::println("restore stack = {:#x} - {:#x} ({:#x})", sp,
      sp + frame->saved_stack.size(), frame->saved_stack.size());
#endif
  std::copy(frame->saved_stack.begin(), frame->saved_stack.end(),
      reinterpret_cast<char*>(sp));
  frame->saved_stack.clear();
  sigjmp_buf saved_jmp;
  memcpy(saved_jmp, frame->saved_jmp, sizeof(sigjmp_buf));
  delete frame;
  // trick the compiler it may not jump and thus could return
  static volatile bool always_jump = true;
  if (always_jump) {
    siglongjmp(saved_jmp, 1);
  }
}

__attribute__((always_inline)) inline void resume_remain(ptrdiff_t sp_delta,
    handler_resumption_frame* frame) {
  asm volatile("sub sp, sp, %0" : : "r"(sp_delta));
  uint64_t sp;
  asm volatile("mov %0, sp" : "=r"(sp));
  __resume_remain(sp, frame);
}

template <typename Effect, typename yield_t>
  requires is_effect<Effect>
saved_resumption<Effect> resume_context<Effect, yield_t>::save() {
  auto sp = get_sp();
  auto sp2 = handler_frame.handler_sp;

  saved_resumption<Effect> resumption;
  resumption.saved_stack.resize(sp2 - sp);
  resumption.saved_frames = frames;
#ifdef EFF_UNWIND_TRACE
  print_frames("save_stack");
  fmt::println("saved stack = {:#x} - {:#x}", sp, sp2);
#endif
  std::copy(reinterpret_cast<char*>(sp), reinterpret_cast<char*>(sp2),
      resumption.saved_stack.begin());

  return resumption;
}

template <typename Effect>
  requires is_effect<Effect>
void saved_resumption<Effect>::resume(Effect::resume_t value) && {}
