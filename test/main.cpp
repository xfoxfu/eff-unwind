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
  unw_word_t resume_unw_ptr;

  handler_frame(std::string effect,
                handler_t handler,
                unw_word_t resume_unw_ptr)
      : effect(effect), handler(handler), resume_unw_ptr(resume_unw_ptr) {
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
  handler_result_break() {}
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

  auto frame = handler_frame(effect, handler, proc_info.start_ip);
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
    // TODO: pass return value
    fmt::println("before resume");
    unw_cursor_t cursor;
    unw_context_t uc;
    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    while (unw_step(&cursor) > 0) {
      unw_proc_info_t proc_info;
      unw_get_proc_info(&cursor, &proc_info);
      // TODO: here it uses start_ip since clang-aarch64-macos does not have
      // gp nor unwind_info pointers, and unw_cursor_t does not perserve after
      // stack changes.
      // further: unw_cursor_t will preserve ip, so it will resume to the point
      // where handler is registered.
      // TODO: need to investigate if multiple handlers are registered
      fmt::println("compare proc_info={:#x} to resume_ptr={:#x}",
                   proc_info.start_ip, frame->resume_unw_ptr);
      if (proc_info.start_ip == frame->resume_unw_ptr) {
        break;
      }
    }
    unw_resume(&cursor);
    assert(false);  // unreachable
  } else if (result->is_resume()) {
    // resume is treated as normal function return
    return dynamic_cast<handler_result_resume&>(*result).value;
  } else {
    assert(false);  // unreachable
  }
}

// user program

struct RAII {
  ~RAII() {}
};

void foobar() {
  fmt::println("before raise");
  auto num = raise("exception");
  fmt::println("after raise: {}", std::any_cast<int>(num));
  fmt::println("foobar continue");
}

void has_handler() {
  auto guard = handle("exception", []() {
    return std::make_unique<handler_result_resume>(42);
    // return std::make_unique<handler_result_break>();
  });
  int num = 42;
  RAII raii;
  fmt::println("has_handler begin");
  foobar();
  fmt::println("has_handler end: {}", num);
}

int main() {
  has_handler();
}
