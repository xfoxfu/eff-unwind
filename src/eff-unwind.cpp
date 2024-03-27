#include "eff-unwind.hpp"
#include <libunwind.h>
#include <iostream>
#include <typeindex>

thread_local std::vector<char> SAVED_STACK;

thread_local uint64_t last_frame_id = 0;

// TODO: add mutex guard for frame
thread_local std::vector<std::unique_ptr<handler_frame_base>> frames;

handler_frame_base::handler_frame_base(std::type_index effect,
                                       uintptr_t resume_fp)
    : effect(effect), resume_fp(resume_fp) {
  id = last_frame_id++;
}

_Unwind_Reason_Code eff_stop_fn(int version,
                                _Unwind_Action actions,
                                _Unwind_Exception_Class exceptionClass,
                                _Unwind_Exception* exceptionObject,
                                struct _Unwind_Context* context,
                                void* stop_parameter) {
  unw_word_t cur_fp;
  unw_get_reg(reinterpret_cast<unw_cursor_t*>(context), UNW_AARCH64_FP,
              &cur_fp);

  if (cur_fp == exceptionObject->private_2) {
    auto frame_ptr = reinterpret_cast<handler_frame_found*>(
        exceptionObject->exception_cleanup);
    auto frame = *frame_ptr;
    delete frame_ptr;

    exceptionObject->exception_cleanup = nullptr;

    auto cursor = reinterpret_cast<unw_cursor_t*>(context);
    // TODO: support more complex calling conventions for >64bit values.
    unw_set_reg(cursor, UNW_AARCH64_X0, frame.set_x0);
    unw_resume(cursor);
    assert(false);
  }
  return _URC_NO_REASON;  // unreachable
}
