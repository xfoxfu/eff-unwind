#include "eff-unwind.hpp"
#include <libunwind.h>
#include <cstdint>
#include <typeindex>

#ifdef EFF_UNWIND_TRACE
#include <cxxabi.h>
#include "fmt/core.h"
#endif

#ifdef EFF_UNWIND_TRACE
std::string demangle(const char* name) {
  int status;
  char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
  if (status == 0) {
    std::string result(demangled);
    free(demangled);
    return result;
  } else {
    return name;
  }
}

auto fmt::formatter<unit_t>::format(unit_t c, format_context& ctx) const
    -> format_context::iterator {
  string_view name = "unknown";
  return formatter<string_view>::format(name, ctx);
}
#endif

uint64_t last_frame_id = 0;

// TODO: add mutex guard for frame
std::vector<std::shared_ptr<handler_frame_base>> frames;

handler_frame_base::handler_frame_base(std::type_index effect,
    uintptr_t resume_fp,
    uintptr_t handler_sp)
    : effect(effect), resume_fp(resume_fp), handler_sp(handler_sp) {
  id = last_frame_id++;
  parent = 0;
}

handler_frame_base::~handler_frame_base() {}

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
      exceptionObject->private_2, demangle(proc_name), off);
  delete[] proc_name;
#endif

  if (cur_fp == exceptionObject->private_2) {
    auto frame_ptr = reinterpret_cast<handler_frame_found*>(
        exceptionObject->exception_cleanup);
    auto frame = *frame_ptr;
    delete frame_ptr;

    exceptionObject->exception_cleanup = nullptr;

    auto cursor = reinterpret_cast<unw_cursor_t*>(context);

#ifdef EFF_UNWIND_TRACE
    fmt::println("set_x0 = {:#x}, set_x1 = {:#x}", frame.set_x0, frame.set_x1);
#endif
    unw_set_reg(cursor, UNW_AARCH64_X0, frame.set_x0);
    unw_set_reg(cursor, UNW_AARCH64_X1, frame.set_x1);
#ifdef EFF_UNWIND_TRACE
    unw_word_t ip;
    unw_get_reg(cursor, UNW_AARCH64_PC, &ip);
    fmt::println("unwind jump to ip={:#x}", ip);

    unw_proc_info_t proc_info;
    unw_get_proc_info(cursor, &proc_info);
    char* proc_name = new char[1000];
    unw_get_proc_name(cursor, proc_name, 1000, &off);
    fmt::println("target function: [fp={:#x}] {} +{:#x} [sp={:#x}]", cur_fp,
        demangle(proc_name), off, exceptionObject->private_2);
    delete[] proc_name;
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
        sp, ip, fp, demangle(proc_name), off);
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

void print_proc(const char* name, unw_cursor_t& cursor) {
  unw_proc_info_t proc_info;
  unw_word_t off;
  unw_get_proc_info(&cursor, &proc_info);
  char* proc_name = new char[1000];
  unw_get_proc_name(&cursor, proc_name, 1000, &off);
  fmt::println("{}: {} +{:#x}", name, proc_name, off);
  delete[] proc_name;
}
#endif
