#include <libunwind.h>
#include <unwind.h>
#include <unwind_itanium.h>
#include <any>
#include <atomic>
#include <boost/stacktrace.hpp>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "fmt/core.h"
#include "scope_guard.hpp"

thread_local uint64_t last_frame_id = 0;

struct handler_result;

typedef std::function<std::unique_ptr<handler_result>()> handler_t;

struct handler_frame {
  std::string effect;
  handler_t handler;
  uint64_t id;
  unw_word_t resume_proc_start;
  unw_word_t resume_proc_end;
  unw_cursor_t resume_cursor;

  handler_frame(std::string effect,
                handler_t handler,
                unw_word_t resume_proc_start,
                unw_word_t resume_proc_end,
                unw_cursor_t resume_cursor)
      : effect(effect),
        handler(handler),
        resume_proc_start(resume_proc_start),
        resume_proc_end(resume_proc_end),
        resume_cursor(resume_cursor) {
    id = last_frame_id++;
  }
};

struct handler_result {
  virtual bool is_resume() = 0;
  virtual bool is_break() = 0;
  virtual ~handler_result() {}
};

struct handler_result_resume : handler_result {
  std::any value;
  handler_result_resume(std::any value) : value(value) {}
  virtual bool is_resume() { return true; }
  virtual bool is_break() { return false; }
};

struct handler_result_break : handler_result {
  std::any value;
  handler_result_break(std::any value) : value(value) {}
  virtual bool is_resume() { return false; }
  virtual bool is_break() { return true; }
};

// TODO: add mutex guard for frame
thread_local std::vector<handler_frame> frames;

void sigserv_handler(int sig) {
  std::cout << boost::stacktrace::stacktrace();
  exit(1);
}

auto handle(std::string effect, handler_t handler) {
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  unw_step(&cursor);
  unw_proc_info_t proc_info;
  unw_get_proc_info(&cursor, &proc_info);

  auto frame = handler_frame(effect, handler, proc_info.start_ip,
                             proc_info.end_ip, cursor);
  auto frame_id = frame.id;
  frames.push_back(std::move(frame));
  return sg::make_scope_guard([frame_id]() {
    auto pop_frame_id = frames.back().id;
    frames.pop_back();
    assert(pop_frame_id == frame_id);
  });
}

constexpr _Unwind_Exception_Class EXCEPTION_CLASS = 0x58464f5845480000;

__attribute__((noinline)) void resume_cleanup(unw_cursor_t* cursor) {
  // std::cout << boost::stacktrace::stacktrace();
  unw_word_t ip;
  unw_word_t sp;
  unw_word_t x0;
  unw_get_reg(cursor, UNW_AARCH64_PC, &ip);
  unw_get_reg(cursor, UNW_AARCH64_SP, &sp);
  unw_get_reg(cursor, UNW_AARCH64_X0, &x0);
  fmt::println("resuming ip = {:#x} sp = {:#x} x0 = {:#x}", ip, sp, x0);
  unw_resume(cursor);
}

_Unwind_Reason_Code eff_stop_fn(int version,
                                _Unwind_Action actions,
                                _Unwind_Exception_Class exceptionClass,
                                _Unwind_Exception* exceptionObject,
                                struct _Unwind_Context* context,
                                void* stop_parameter) {
  // unw_step(reinterpret_cast<unw_cursor_t*>(context));
  unw_word_t cur_sp;
  unw_get_reg(reinterpret_cast<unw_cursor_t*>(context), UNW_AARCH64_SP,
              &cur_sp);
  fmt::println("stop_fn: {:#x} <=> {:#x}", cur_sp, exceptionObject->private_2);
  if (cur_sp == exceptionObject->private_2) {
    auto frame_id =
        reinterpret_cast<uintptr_t>(exceptionObject->exception_cleanup);
    auto& frame = frames[frame_id];
    fmt::println("stop_fn: resume to handler of '{}'", frame.effect);
    exceptionObject->exception_cleanup = nullptr;
    unw_context_t uc;
    unw_getcontext(&uc);
    unw_set_reg(&frame.resume_cursor, UNW_AARCH64_X0, 20);
    unw_resume(&frame.resume_cursor);
    assert(false);
  }
  return _URC_NO_REASON;  // unreachable
}

// TODO: use real static type
std::any raise(std::string effect) {
  // trick the compiler to generate exception tables
  static volatile bool never_throw = false;
  if (never_throw) {
    throw 42;
  }
  auto frame = std::find_if(frames.rbegin(), frames.rend(), [&](auto frame) {
    fmt::println("compare handler={}, effect={}", frame.effect, effect);
    return frame.effect == effect;
  });
  fmt::println("after find_if");
  if (frame == frames.rend()) {
    fmt::println("no handler found!");
    std::abort();
  }
  fmt::println("before handler");
  auto result = frame->handler();
  fmt::println("after handler");
  if (result->is_break()) {
    // when break, need to do unwinding
    // step more to get to parent function to make caller returns
    unw_step(&frame->resume_cursor);
    fmt::println("before resume");

    unw_word_t target_sp;
    unw_get_reg(&frame->resume_cursor, UNW_AARCH64_SP, &target_sp);
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
        (decltype(ex->exception_cleanup))(frame - frames.rbegin());
    while (unw_step(&cur_cursor)) {
      unw_proc_info_t pi;
      unw_get_proc_info(&cur_cursor, &pi);
      fmt::print("handler={:#x}", pi.handler);
      unw_word_t fp;
      unw_get_reg(&cur_cursor, UNW_AARCH64_SP, &fp);
      fmt::println(", sp = {:#x}", fp);
      // cannot find frame by comparing cursor
      // if (memcmp(&cur_cursor, &frame->resume_cursor, sizeof(unw_cursor_t)) ==
      // 0) {
      //   break;
      // }
      // compare by sp
      if (fp == target_sp) {
        fmt::println("found = {:#x} == {:#x}", fp, target_sp);
        break;
      }
      unw_word_t ip;
      unw_word_t sp;
      unw_word_t x0;
      unw_get_reg(&cur_cursor, UNW_AARCH64_PC, &ip);
      unw_get_reg(&cur_cursor, UNW_AARCH64_SP, &sp);
      unw_get_reg(&cur_cursor, UNW_AARCH64_X0, &x0);
      fmt::println("current ip = {:#x} sp = {:#x} x0 = {:#x}", ip, sp, x0);

      // unw_cursor_t dup_cursor;
      // memcpy(&dup_cursor, &cur_cursor, sizeof(unw_cursor_t));

      // if not break, perform cleanup
      _Unwind_Personality_Fn personality =
          reinterpret_cast<_Unwind_Personality_Fn>(pi.handler);
      _Unwind_Reason_Code personalityResult =
          personality(1, _UA_CLEANUP_PHASE, EXCEPTION_CLASS, ex.get(),
                      reinterpret_cast<struct _Unwind_Context*>(&cur_cursor));
      fmt::println("cleanup result = {}", (int)personalityResult);
      if (personalityResult == _URC_INSTALL_CONTEXT) {
        fmt::println("install context");
        std::cout << boost::stacktrace::stacktrace();
        resume_cleanup(&cur_cursor);
        // _Unwind_Resume(ex.get());
        fmt::println("after cleanup for {:#x}", fp);
      }
    }

    // // TODO: support more complex calling conventions of returning >64-bit
    // // values.
    assert(false);
    unw_set_reg(
        &frame->resume_cursor, UNW_AARCH64_X0,
        std::any_cast<int>(dynamic_cast<handler_result_break&>(*result).value));
    resume_cleanup(&frame->resume_cursor);
    assert(false);  // unreachable
  } else if (result->is_resume()) {
    // resume is treated as normal function return
    return dynamic_cast<handler_result_resume&>(*result).value;
  }
  assert(false);  // unreachable
  return -1;      // unreachable
}

void xx_Unwind_Resume(_Unwind_Exception* exception_object) {
  if (exception_object->exception_class == EXCEPTION_CLASS) {
    fmt::println("unwind_resume {:#x}", (uintptr_t)exception_object);
    unw_cursor_t cur_cursor;
    unw_context_t uc;
    unw_getcontext(&uc);
    unw_init_local(&cur_cursor, &uc);
    unw_word_t target_sp = exception_object->private_2;
    while (unw_step(&cur_cursor)) {
      unw_word_t sp;
      unw_get_reg(&cur_cursor, UNW_AARCH64_SP, &sp);
      if (sp != target_sp) {
        fmt::println("not found = {:#x} != {:#x}", sp, target_sp);
        continue;
      }
      fmt::println("found = {:#x} == {:#x}", sp, target_sp);
      unw_resume(&cur_cursor);
    }
    fmt::println("expected find sp of raise, not found!");
    std::cout << boost::stacktrace::stacktrace();
    std::abort();
  } else {
    fmt::println("non-exception!");
    std::cout << boost::stacktrace::stacktrace();
  }
}

// user program

struct RAII {
  ~RAII() {
    std::cout << boost::stacktrace::stacktrace();
    fmt::println("RAII destructor");
  }
};

int foobar() {
  fmt::println("before raise");
  auto num = raise("exception");
  // FIXME: if compiled as separate library, maybe clang will mark all as
  // throwing
  fmt::println("after raise: {}", std::any_cast<int>(num));
  fmt::println("foobar continue");
  return 56;
}

int has_handler() {
  auto guard = handle("exception", []() {
    // return std::make_unique<handler_result_resume>(42);
    return std::make_unique<handler_result_break>(72);
  });
  int num = 42;
  RAII raii;
  fmt::println("has_handler begin");
  int ret = foobar();
  fmt::println("has_handler end: {} ret={}", num, ret);
  return 24;
}

int main() {
  // signal(SIGSEGV, sigserv_handler);
  auto val = has_handler();
  // FIXME: if Release mode is specified, cannot be handled correctly
  // need to tell the C++ optimizer that return value is not a constant
  fmt::println("main end: {}", val);
}
