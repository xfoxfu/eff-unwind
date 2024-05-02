#include "eff-unwind.hpp"
#include <libunwind.h>
#include <sys/_types/_uintptr_t.h>
#include <csetjmp>
#include <iostream>
#include <typeindex>
#include "fmt/core.h"

thread_local std::vector<char> SAVED_STACK;
thread_local jmp_buf SAVED_JMP;

thread_local uint64_t last_frame_id = 0;

// TODO: add mutex guard for frame
thread_local std::vector<std::unique_ptr<handler_frame_base>> frames;

handler_frame_base::handler_frame_base(std::type_index effect,
                                       uintptr_t resume_fp,
                                       uintptr_t handler_sp)
    : effect(effect), resume_fp(resume_fp), handler_sp(handler_sp) {
  id = last_frame_id++;
}

_Unwind_Reason_Code eff_stop_fn(int version,
                                _Unwind_Action actions,
                                _Unwind_Exception_Class exceptionClass,
                                _Unwind_Exception* exceptionObject,
                                struct _Unwind_Context* context,
                                void* stop_parameter) {
  auto cursor = reinterpret_cast<unw_cursor_t*>(context);
  unw_word_t cur_fp;
  unw_get_reg(cursor, UNW_AARCH64_FP, &cur_fp);
#ifdef EFF_UNWIND_TRACE
  unw_word_t off;
  unw_proc_info_t proc_info;
  unw_get_proc_info(cursor, &proc_info);
  char* proc_name = new char[1000];
  unw_get_proc_name(cursor, proc_name, 1000, &off);
  fmt::println("unwind at fp={:#x}<>{:#x} {} +{:#x}", cur_fp,
               exceptionObject->private_2, proc_name, off);
  delete[] proc_name;
#endif

  if (cur_fp == exceptionObject->private_2) {
    auto frame_ptr = reinterpret_cast<handler_frame_found*>(
        exceptionObject->exception_cleanup);
    auto frame = *frame_ptr;
    delete frame_ptr;

    exceptionObject->exception_cleanup = nullptr;

    auto cursor = reinterpret_cast<unw_cursor_t*>(context);
    // TODO: support more complex calling conventions for >64bit values.
    unw_set_reg(cursor, UNW_AARCH64_X0, frame.set_x0);
#ifdef EFF_UNWIND_TRACE
    unw_word_t ip;
    unw_get_reg(cursor, UNW_AARCH64_PC, &ip);
    fmt::println("unwind jump to ip={:#x}", ip);
#endif
    unw_resume(cursor);
    assert(false);
  }
  return _URC_NO_REASON;  // unreachable
}

#ifdef EFF_UNWIND_TRACE
void print_frames(const char* prefix) {
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  int i = 0;
  while (unw_step(&cursor)) {
    unw_word_t sp, ip, fp, off;
    unw_get_reg(&cursor, UNW_AARCH64_SP, &sp);
    unw_get_reg(&cursor, UNW_AARCH64_PC, &ip);
    unw_get_reg(&cursor, UNW_AARCH64_FP, &fp);
    unw_proc_info_t proc_info;
    unw_get_proc_info(&cursor, &proc_info);
    char* proc_name = new char[1000];
    unw_get_proc_name(&cursor, proc_name, 1000, &off);
    fmt::println("{:16} [{}] sp={:#x} ip={:#x} fp={:#x} {} +{:#x}", prefix, i++,
                 sp, ip, fp, proc_name, off);
    delete[] proc_name;
  }
}

void print_memory(const char* start, const char* end) {
  fmt::println("stack over {:#x}-{:#x}", reinterpret_cast<uintptr_t>(start),
               reinterpret_cast<uintptr_t>(end));
  for (auto i = start; i < end; i++) {
    fmt::print("{:02x}", *i);
    if ((i - start + 1) % 8 == 0) {
      fmt::print(" ");
    }
    if ((i - start + 1) % 64 == 0) {
      fmt::print("\n");
    }
  }
  fmt::print("\n");
}
#endif
