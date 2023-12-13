#include <libunwind.h>
#include <any>
#include <cassert>
#include <cstdlib>
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
    // TODO: support more complex calling conventions of returning >64-bit
    // values.
    unw_set_reg(
        &frame->resume_cursor, UNW_AARCH64_X0,
        std::any_cast<int>(dynamic_cast<handler_result_break&>(*result).value));
    unw_resume(&frame->resume_cursor);
    assert(false);  // unreachable
  } else if (result->is_resume()) {
    // resume is treated as normal function return
    return dynamic_cast<handler_result_resume&>(*result).value;
  }
  assert(false);  // unreachable
  return -1;      // unreachable
}

// user program

struct RAII {
  ~RAII() { fmt::println("RAII destructor"); }
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
  auto val = has_handler();
  // FIXME: if Release mode is specified, cannot be handled correctly
  fmt::println("main end: {}", val);
}
