// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <cassert>

#include "google_breakpad/processor/minidump_processor.h"
#include "google_breakpad/processor/call_stack.h"
#include "google_breakpad/processor/minidump.h"
#include "google_breakpad/processor/process_state.h"
#include "processor/scoped_ptr.h"
#include "processor/stackwalker_x86.h"

namespace google_breakpad {

MinidumpProcessor::MinidumpProcessor(SymbolSupplier *supplier,
                                     SourceLineResolverInterface *resolver)
    : supplier_(supplier), resolver_(resolver) {
}

MinidumpProcessor::~MinidumpProcessor() {
}

MinidumpProcessor::ProcessResult MinidumpProcessor::Process(
    const string &minidump_file, ProcessState *process_state) {
  Minidump dump(minidump_file);
  if (!dump.Read()) {
    return PROCESS_ERROR;
  }

  process_state->Clear();

  const MDRawHeader *header = dump.header();
  assert(header);
  process_state->time_date_stamp_ = header->time_date_stamp;

  GetCPUInfo(&dump, &process_state->system_info_);
  GetOSInfo(&dump, &process_state->system_info_);

  u_int32_t dump_thread_id = 0;
  bool has_dump_thread = false;
  u_int32_t requesting_thread_id = 0;
  bool has_requesting_thread = false;

  MinidumpBreakpadInfo *breakpad_info = dump.GetBreakpadInfo();
  if (breakpad_info) {
    has_dump_thread = breakpad_info->GetDumpThreadID(&dump_thread_id);
    has_requesting_thread =
        breakpad_info->GetRequestingThreadID(&requesting_thread_id);
  }

  MinidumpException *exception = dump.GetException();
  if (exception) {
    process_state->crashed_ = true;
    has_requesting_thread = exception->GetThreadID(&requesting_thread_id);

    process_state->crash_reason_ = GetCrashReason(
        &dump, &process_state->crash_address_);
  }

  MinidumpModuleList *module_list = dump.GetModuleList();

  // Put a copy of the module list into ProcessState object.  This is not
  // necessarily a MinidumpModuleList, but it adheres to the CodeModules
  // interface, which is all that ProcessState needs to expose.
  if (module_list)
    process_state->modules_ = module_list->Copy();

  MinidumpThreadList *threads = dump.GetThreadList();
  if (!threads) {
    return PROCESS_ERROR;
  }

  bool found_requesting_thread = false;
  unsigned int thread_count = threads->thread_count();
  for (unsigned int thread_index = 0;
       thread_index < thread_count;
       ++thread_index) {
    MinidumpThread *thread = threads->GetThreadAtIndex(thread_index);
    if (!thread) {
      return PROCESS_ERROR;
    }

    u_int32_t thread_id;
    if (!thread->GetThreadID(&thread_id)) {
      return PROCESS_ERROR;
    }

    // If this thread is the thread that produced the minidump, don't process
    // it.  Because of the problems associated with a thread producing a
    // dump of itself (when both its context and its stack are in flux),
    // processing that stack wouldn't provide much useful data.
    if (has_dump_thread && thread_id == dump_thread_id) {
      continue;
    }

    MinidumpContext *context = thread->GetContext();

    if (has_requesting_thread && thread_id == requesting_thread_id) {
      if (found_requesting_thread) {
        // There can't be more than one requesting thread.
        return PROCESS_ERROR;
      }

      // Use processed_state->threads_.size() instead of thread_index.
      // thread_index points to the thread index in the minidump, which
      // might be greater than the thread index in the threads vector if
      // any of the minidump's threads are skipped and not placed into the
      // processed threads vector.  The thread vector's current size will
      // be the index of the current thread when it's pushed into the
      // vector.
      process_state->requesting_thread_ = process_state->threads_.size();

      found_requesting_thread = true;

      if (process_state->crashed_) {
        // Use the exception record's context for the crashed thread, instead
        // of the thread's own context.  For the crashed thread, the thread's
        // own context is the state inside the exception handler.  Using it
        // would not result in the expected stack trace from the time of the
        // crash.
        context = exception->GetContext();
      }
    }

    MinidumpMemoryRegion *thread_memory = thread->GetMemory();
    if (!thread_memory) {
      return PROCESS_ERROR;
    }

    // Use process_state->modules_ instead of module_list, because the
    // |modules| argument will be used to populate the |module| fields in
    // the returned StackFrame objects, which will be placed into the
    // returned ProcessState object.  module_list's lifetime is only as
    // long as the Minidump object: it will be deleted when this function
    // returns.  process_state->modules_ is owned by the ProcessState object
    // (just like the StackFrame objects), and is much more suitable for this
    // task.
    scoped_ptr<Stackwalker> stackwalker(
        Stackwalker::StackwalkerForCPU(process_state->system_info(),
                                       context,
                                       thread_memory,
                                       process_state->modules_,
                                       supplier_,
                                       resolver_));
    if (!stackwalker.get()) {
      return PROCESS_ERROR;
    }

    scoped_ptr<CallStack> stack(new CallStack());
    if (!stackwalker->Walk(stack.get())) {
      return PROCESS_INTERRUPTED;
    }
    process_state->threads_.push_back(stack.release());
  }

  // If a requesting thread was indicated, it must be present.
  if (has_requesting_thread && !found_requesting_thread) {
    // Don't mark as an error, but invalidate the requesting thread
    process_state->requesting_thread_ = -1;
  }

  return PROCESS_OK;
}

// Returns the MDRawSystemInfo from a minidump, or NULL if system info is
// not available from the minidump.  If system_info is non-NULL, it is used
// to pass back the MinidumpSystemInfo object.
static const MDRawSystemInfo* GetSystemInfo(Minidump *dump,
                                            MinidumpSystemInfo **system_info) {
  MinidumpSystemInfo *minidump_system_info = dump->GetSystemInfo();
  if (!minidump_system_info)
    return NULL;

  if (system_info)
    *system_info = minidump_system_info;

  return minidump_system_info->system_info();
}

// static
void MinidumpProcessor::GetCPUInfo(Minidump *dump, SystemInfo *info) {
  assert(dump);
  assert(info);

  info->cpu.clear();
  info->cpu_info.clear();

  MinidumpSystemInfo *system_info;
  const MDRawSystemInfo *raw_system_info = GetSystemInfo(dump, &system_info);
  if (!raw_system_info)
    return;

  switch (raw_system_info->processor_architecture) {
    case MD_CPU_ARCHITECTURE_X86: {
      info->cpu = "x86";
      const string *cpu_vendor = system_info->GetCPUVendor();
      if (cpu_vendor) {
        info->cpu_info = *cpu_vendor;
        info->cpu_info.append(" ");
      }

      char x86_info[36];
      snprintf(x86_info, sizeof(x86_info), "family %u model %u stepping %u",
               raw_system_info->processor_level,
               raw_system_info->processor_revision >> 8,
               raw_system_info->processor_revision & 0xff);
      info->cpu_info.append(x86_info);
      break;
    }

    case MD_CPU_ARCHITECTURE_PPC: {
      info->cpu = "ppc";
      break;
    }

    default: {
      // Assign the numeric architecture ID into the CPU string.
      char cpu_string[7];
      snprintf(cpu_string, sizeof(cpu_string), "0x%04x",
               raw_system_info->processor_architecture);
      info->cpu = cpu_string;
      break;
    }
  }
}

// static
void MinidumpProcessor::GetOSInfo(Minidump *dump, SystemInfo *info) {
  assert(dump);
  assert(info);

  info->os.clear();
  info->os_short.clear();
  info->os_version.clear();

  MinidumpSystemInfo *system_info;
  const MDRawSystemInfo *raw_system_info = GetSystemInfo(dump, &system_info);
  if (!raw_system_info)
    return;

  info->os_short = system_info->GetOS();

  switch (raw_system_info->platform_id) {
    case MD_OS_WIN32_NT: {
      info->os = "Windows NT";
      break;
    }

    case MD_OS_WIN32_WINDOWS: {
      info->os = "Windows";
      break;
    }

    case MD_OS_MAC_OS_X: {
      info->os = "Mac OS X";
      break;
    }

    case MD_OS_LINUX: {
      info->os = "Linux";
      break;
    }

    default: {
      // Assign the numeric platform ID into the OS string.
      char os_string[11];
      snprintf(os_string, sizeof(os_string), "0x%08x",
               raw_system_info->platform_id);
      info->os = os_string;
      break;
    }
  }

  char os_version_string[33];
  snprintf(os_version_string, sizeof(os_version_string), "%u.%u.%u",
           raw_system_info->major_version,
           raw_system_info->minor_version,
           raw_system_info->build_number);
  info->os_version = os_version_string;

  const string *csd_version = system_info->GetCSDVersion();
  if (csd_version) {
    info->os_version.append(" ");
    info->os_version.append(*csd_version);
  }
}

// static
string MinidumpProcessor::GetCrashReason(Minidump *dump, u_int64_t *address) {
  MinidumpException *exception = dump->GetException();
  if (!exception)
    return "";

  const MDRawExceptionStream *raw_exception = exception->exception();
  if (!raw_exception)
    return "";

  if (address)
    *address = raw_exception->exception_record.exception_address;

  // The reason value is OS-specific and possibly CPU-specific.  Set up
  // sensible numeric defaults for the reason string in case we can't
  // map the codes to a string (because there's no system info, or because
  // it's an unrecognized platform, or because it's an unrecognized code.)
  char reason_string[24];
  u_int32_t exception_code = raw_exception->exception_record.exception_code;
  u_int32_t exception_flags = raw_exception->exception_record.exception_flags;
  snprintf(reason_string, sizeof(reason_string), "0x%08x / 0x%08x",
           exception_code, exception_flags);
  string reason = reason_string;

  const MDRawSystemInfo *raw_system_info = GetSystemInfo(dump, NULL);
  if (!raw_system_info)
    return reason;

  switch (raw_system_info->platform_id) {
    case MD_OS_MAC_OS_X: {
      char flags_string[11];
      snprintf(flags_string, sizeof(flags_string), "0x%08x", exception_flags);
      switch (exception_code) {
        case MD_EXCEPTION_MAC_BAD_ACCESS:
          reason = "EXC_BAD_ACCESS / ";
          switch (exception_flags) {
            case MD_EXCEPTION_CODE_MAC_INVALID_ADDRESS:
              reason.append("KERN_INVALID_ADDRESS");
              break;
            case MD_EXCEPTION_CODE_MAC_PROTECTION_FAILURE:
              reason.append("KERN_PROTECTION_FAILURE");
              break;
            case MD_EXCEPTION_CODE_MAC_NO_ACCESS:
              reason.append("KERN_NO_ACCESS");
              break;
            case MD_EXCEPTION_CODE_MAC_MEMORY_FAILURE:
              reason.append("KERN_MEMORY_FAILURE");
              break;
            case MD_EXCEPTION_CODE_MAC_MEMORY_ERROR:
              reason.append("KERN_MEMORY_ERROR");
              break;
            // These are ppc only but shouldn't be a problem as they're
            // unused on x86
            case MD_EXCEPTION_CODE_MAC_PPC_VM_PROT_READ:
              reason.append("EXC_PPC_VM_PROT_READ");
              break;
            case MD_EXCEPTION_CODE_MAC_PPC_BADSPACE:
              reason.append("EXC_PPC_BADSPACE");
              break;
            case MD_EXCEPTION_CODE_MAC_PPC_UNALIGNED:
              reason.append("EXC_PPC_UNALIGNED");
              break;
            default:
              reason.append(flags_string);
              break;
          }
          break;
        case MD_EXCEPTION_MAC_BAD_INSTRUCTION:
          reason = "EXC_BAD_INSTRUCTION / ";
          switch (raw_system_info->processor_architecture) {
            case MD_CPU_ARCHITECTURE_PPC: {
              switch (exception_flags) {
                case MD_EXCEPTION_CODE_MAC_PPC_INVALID_SYSCALL:
                  reason.append("EXC_PPC_INVALID_SYSCALL");
                  break;
                case MD_EXCEPTION_CODE_MAC_PPC_UNIMPLEMENTED_INSTRUCTION:
                  reason.append("EXC_PPC_UNIPL_INST");
                  break;
                case MD_EXCEPTION_CODE_MAC_PPC_PRIVILEGED_INSTRUCTION:
                  reason.append("EXC_PPC_PRIVINST");
                  break;
                case MD_EXCEPTION_CODE_MAC_PPC_PRIVILEGED_REGISTER:
                  reason.append("EXC_PPC_PRIVREG");
                  break;
                case MD_EXCEPTION_CODE_MAC_PPC_TRACE:
                  reason.append("EXC_PPC_TRACE");
                  break;
                case MD_EXCEPTION_CODE_MAC_PPC_PERFORMANCE_MONITOR:
                  reason.append("EXC_PPC_PERFMON");
                  break;
                default:
                  reason.append(flags_string);
                  break;
              }
              break;
            }
            case MD_CPU_ARCHITECTURE_X86: {
              switch (exception_flags) {
                case MD_EXCEPTION_CODE_MAC_X86_INVALID_OPERATION:
                  reason.append("EXC_I386_INVOP");
                  break;
                case MD_EXCEPTION_CODE_MAC_X86_INVALID_TASK_STATE_SEGMENT:
                  reason.append("EXC_INVTSSFLT");
                  break;
                case MD_EXCEPTION_CODE_MAC_X86_SEGMENT_NOT_PRESENT:
                  reason.append("EXC_SEGNPFLT");
                  break;
                case MD_EXCEPTION_CODE_MAC_X86_STACK_FAULT:
                  reason.append("EXC_STKFLT");
                  break;
                case MD_EXCEPTION_CODE_MAC_X86_GENERAL_PROTECTION_FAULT:
                  reason.append("EXC_GPFLT");
                  break;
                case MD_EXCEPTION_CODE_MAC_X86_ALIGNMENT_FAULT:
                  reason.append("EXC_ALIGNFLT");
                  break;
                default:
                  reason.append(flags_string);
                  break;
              }
              break;
            }
            default:
              reason.append(flags_string);
              break;
          }
          break;
        case MD_EXCEPTION_MAC_ARITHMETIC:
          reason = "EXC_ARITHMETIC / ";
          switch (raw_system_info->processor_architecture) {
            case MD_CPU_ARCHITECTURE_PPC: {
              switch (exception_flags) {
                case MD_EXCEPTION_CODE_MAC_PPC_OVERFLOW:
                  reason.append("EXC_PPC_OVERFLOW");
                  break;
                case MD_EXCEPTION_CODE_MAC_PPC_ZERO_DIVIDE:
                  reason.append("EXC_PPC_ZERO_DIVIDE");
                  break;
                case MD_EXCEPTION_CODE_MAC_PPC_FLOAT_INEXACT:
                  reason.append("EXC_FLT_INEXACT");
                  break;
                case MD_EXCEPTION_CODE_MAC_PPC_FLOAT_ZERO_DIVIDE:
                  reason.append("EXC_PPC_FLT_ZERO_DIVIDE");
                  break;
                case MD_EXCEPTION_CODE_MAC_PPC_FLOAT_UNDERFLOW:
                  reason.append("EXC_PPC_FLT_UNDERFLOW");
                  break;
                case MD_EXCEPTION_CODE_MAC_PPC_FLOAT_OVERFLOW:
                  reason.append("EXC_PPC_FLT_OVERFLOW");
                  break;
                case MD_EXCEPTION_CODE_MAC_PPC_FLOAT_NOT_A_NUMBER:
                  reason.append("EXC_PPC_FLT_NOT_A_NUMBER");
                  break;
                case MD_EXCEPTION_CODE_MAC_PPC_NO_EMULATION:
                  reason.append("EXC_PPC_NOEMULATION");
                  break;
                case MD_EXCEPTION_CODE_MAC_PPC_ALTIVEC_ASSIST:
                  reason.append("EXC_PPC_ALTIVECASSIST");
                default:
                  reason.append(flags_string);
                  break;
              }
              break;
            }
            case MD_CPU_ARCHITECTURE_X86: {
              switch (exception_flags) {
                case MD_EXCEPTION_CODE_MAC_X86_DIV:
                  reason.append("EXC_I386_DIV");
                  break;
                case MD_EXCEPTION_CODE_MAC_X86_INTO:
                  reason.append("EXC_I386_INTO");
                  break;
                case MD_EXCEPTION_CODE_MAC_X86_NOEXT:
                  reason.append("EXC_I386_NOEXT");
                  break;
                case MD_EXCEPTION_CODE_MAC_X86_EXTOVR:
                  reason.append("EXC_I386_EXTOVR");
                  break;
                case MD_EXCEPTION_CODE_MAC_X86_EXTERR:
                  reason.append("EXC_I386_EXTERR");
                  break;
                case MD_EXCEPTION_CODE_MAC_X86_EMERR:
                  reason.append("EXC_I386_EMERR");
                  break;
                case MD_EXCEPTION_CODE_MAC_X86_BOUND:
                  reason.append("EXC_I386_BOUND");
                  break;
                case MD_EXCEPTION_CODE_MAC_X86_SSEEXTERR:
                  reason.append("EXC_I386_SSEEXTERR");
                  break;
                default:
                  reason.append(flags_string);
                  break;
              }
              break;
            }
            default:
              reason.append(flags_string);
              break;
          }
          break;
        case MD_EXCEPTION_MAC_EMULATION:
          reason = "EXC_EMULATION / ";
          reason.append(flags_string);
          break;
        case MD_EXCEPTION_MAC_SOFTWARE:
          reason = "EXC_SOFTWARE / ";
          switch (exception_flags) {
            // These are ppc only but shouldn't be a problem as they're
            // unused on x86
            case MD_EXCEPTION_CODE_MAC_PPC_TRAP:
              reason.append("EXC_PPC_TRAP");
              break;
            case MD_EXCEPTION_CODE_MAC_PPC_MIGRATE:
              reason.append("EXC_PPC_MIGRATE");
              break;
            default:
              reason.append(flags_string);
              break;
          }
          break;
        case MD_EXCEPTION_MAC_BREAKPOINT:
          reason = "EXC_BREAKPOINT / ";
          switch (raw_system_info->processor_architecture) {
            case MD_CPU_ARCHITECTURE_PPC: {
              switch (exception_flags) {
                case MD_EXCEPTION_CODE_MAC_PPC_BREAKPOINT:
                  reason.append("EXC_PPC_BREAKPOINT");
                  break;
                default:
                  reason.append(flags_string);
                  break;
              }
              break;
            }
            case MD_CPU_ARCHITECTURE_X86: {
              switch (exception_flags) {
                case MD_EXCEPTION_CODE_MAC_X86_SGL:
                  reason.append("EXC_I386_SGL");
                  break;
                case MD_EXCEPTION_CODE_MAC_X86_BPT:
                  reason.append("EXC_I386_BPT");
                  break;
                default:
                  reason.append(flags_string);
                  break;
              }
              break;
            }
            default:
              reason.append(flags_string);
              break;
          }
          break;
        case MD_EXCEPTION_MAC_SYSCALL:
          reason = "EXC_SYSCALL / ";
          reason.append(flags_string);
          break;
        case MD_EXCEPTION_MAC_MACH_SYSCALL:
          reason = "EXC_MACH_SYSCALL / ";
          reason.append(flags_string);
          break;
        case MD_EXCEPTION_MAC_RPC_ALERT:
          reason = "EXC_RPC_ALERT / ";
          reason.append(flags_string);
          break;
      }
      break;
    }

    case MD_OS_WIN32_NT:
    case MD_OS_WIN32_WINDOWS: {
      switch (exception_code) {
        case MD_EXCEPTION_CODE_WIN_CONTROL_C:
          reason = "DBG_CONTROL_C";
          break;
        case MD_EXCEPTION_CODE_WIN_GUARD_PAGE_VIOLATION:
          reason = "EXCEPTION_GUARD_PAGE";
          break;
        case MD_EXCEPTION_CODE_WIN_DATATYPE_MISALIGNMENT:
          reason = "EXCEPTION_DATATYPE_MISALIGNMENT";
          break;
        case MD_EXCEPTION_CODE_WIN_BREAKPOINT:
          reason = "EXCEPTION_BREAKPOINT";
          break;
        case MD_EXCEPTION_CODE_WIN_SINGLE_STEP:
          reason = "EXCEPTION_SINGLE_STEP";
          break;
        case MD_EXCEPTION_CODE_WIN_ACCESS_VIOLATION:
          // For EXCEPTION_ACCESS_VIOLATION, Windows puts the address that
          // caused the fault in exception_information[1].
          // exception_information[0] is 0 if the violation was caused by
          // an attempt to read data and 1 if it was an attempt to write
          // data.
          // This information is useful in addition to the code address, which
          // will be present in the crash thread's instruction field anyway.
          reason = "EXCEPTION_ACCESS_VIOLATION";
          if (address &&
              raw_exception->exception_record.number_parameters >= 2) {
            *address =
                raw_exception->exception_record.exception_information[1];
          }
          break;
        case MD_EXCEPTION_CODE_WIN_IN_PAGE_ERROR:
          reason = "EXCEPTION_IN_PAGE_ERROR";
          break;
        case MD_EXCEPTION_CODE_WIN_INVALID_HANDLE:
          reason = "EXCEPTION_INVALID_HANDLE";
          break;
        case MD_EXCEPTION_CODE_WIN_ILLEGAL_INSTRUCTION:
          reason = "EXCEPTION_ILLEGAL_INSTRUCTION";
          break;
        case MD_EXCEPTION_CODE_WIN_NONCONTINUABLE_EXCEPTION:
          reason = "EXCEPTION_NONCONTINUABLE_EXCEPTION";
          break;
        case MD_EXCEPTION_CODE_WIN_INVALID_DISPOSITION:
          reason = "EXCEPTION_INVALID_DISPOSITION";
          break;
        case MD_EXCEPTION_CODE_WIN_ARRAY_BOUNDS_EXCEEDED:
          reason = "EXCEPTION_BOUNDS_EXCEEDED";
          break;
        case MD_EXCEPTION_CODE_WIN_FLOAT_DENORMAL_OPERAND:
          reason = "EXCEPTION_FLT_DENORMAL_OPERAND";
          break;
        case MD_EXCEPTION_CODE_WIN_FLOAT_DIVIDE_BY_ZERO:
          reason = "EXCEPTION_FLT_DIVIDE_BY_ZERO";
          break;
        case MD_EXCEPTION_CODE_WIN_FLOAT_INEXACT_RESULT:
          reason = "EXCEPTION_FLT_INEXACT_RESULT";
          break;
        case MD_EXCEPTION_CODE_WIN_FLOAT_INVALID_OPERATION:
          reason = "EXCEPTION_FLT_INVALID_OPERATION";
          break;
        case MD_EXCEPTION_CODE_WIN_FLOAT_OVERFLOW:
          reason = "EXCEPTION_FLT_OVERFLOW";
          break;
        case MD_EXCEPTION_CODE_WIN_FLOAT_STACK_CHECK:
          reason = "EXCEPTION_FLT_STACK_CHECK";
          break;
        case MD_EXCEPTION_CODE_WIN_FLOAT_UNDERFLOW:
          reason = "EXCEPTION_FLT_UNDERFLOW";
          break;
        case MD_EXCEPTION_CODE_WIN_INTEGER_DIVIDE_BY_ZERO:
          reason = "EXCEPTION_INT_DIVIDE_BY_ZERO";
          break;
        case MD_EXCEPTION_CODE_WIN_INTEGER_OVERFLOW:
          reason = "EXCEPTION_INT_OVERFLOW";
          break;
        case MD_EXCEPTION_CODE_WIN_PRIVILEGED_INSTRUCTION:
          reason = "EXCEPTION_PRIV_INSTRUCTION";
          break;
        case MD_EXCEPTION_CODE_WIN_STACK_OVERFLOW:
          reason = "EXCEPTION_STACK_OVERFLOW";
          break;
        case MD_EXCEPTION_CODE_WIN_POSSIBLE_DEADLOCK:
          reason = "EXCEPTION_POSSIBLE_DEADLOCK";
          break;
      }
    }
  }

  return reason;
}

}  // namespace google_breakpad
