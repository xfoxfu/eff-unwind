#include "eff-unwind.hpp"
#include <libunwind.h>
#include <iostream>
#include <typeindex>

thread_local std::vector<char> SAVED_STACK;

thread_local uint64_t last_frame_id = 0;

// TODO: add mutex guard for frame
thread_local std::vector<std::unique_ptr<handler_frame_base>> frames;

handler_frame_base::handler_frame_base(std::type_index effect,
                                       unw_cursor_t resume_cursor)
    : effect(effect), resume_cursor(resume_cursor) {
  id = last_frame_id++;
}

_Unwind_Reason_Code eff_stop_fn(int version,
                                _Unwind_Action actions,
                                _Unwind_Exception_Class exceptionClass,
                                _Unwind_Exception* exceptionObject,
                                struct _Unwind_Context* context,
                                void* stop_parameter) {
  unw_word_t cur_sp;
  unw_get_reg(reinterpret_cast<unw_cursor_t*>(context), UNW_AARCH64_SP,
              &cur_sp);

  if (cur_sp == exceptionObject->private_2) {
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

void resume_nontail() {
  assert(!SAVED_STACK.empty());
  unw_cursor_t cur_cursor;
  unw_context_t uc;
  unw_getcontext(&uc);
  unw_init_local(&cur_cursor, &uc);
  unw_step(&cur_cursor);
  unw_word_t sp;
  unw_get_reg(&cur_cursor, UNW_AARCH64_SP, &sp);
  fmt::println("end of stack = {:#x} - {:#x}", sp, sp - SAVED_STACK.size());
  std::copy(SAVED_STACK.begin(), SAVED_STACK.end(),
            reinterpret_cast<char*>(sp) - SAVED_STACK.size());
}
