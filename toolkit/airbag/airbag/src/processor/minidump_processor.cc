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

#include "google/minidump_processor.h"
#include "google/call_stack.h"
#include "google/process_state.h"
#include "processor/minidump.h"
#include "processor/scoped_ptr.h"
#include "processor/stackwalker_x86.h"

namespace google_airbag {

MinidumpProcessor::MinidumpProcessor(SymbolSupplier *supplier)
    : supplier_(supplier) {
}

MinidumpProcessor::~MinidumpProcessor() {
}

ProcessState* MinidumpProcessor::Process(const string &minidump_file) {
  Minidump dump(minidump_file);
  if (!dump.Read()) {
    return NULL;
  }

  scoped_ptr<ProcessState> process_state(new ProcessState());

  process_state->cpu_ = GetCPUInfo(&dump, &process_state->cpu_info_);
  process_state->os_ = GetOSInfo(&dump, &process_state->os_version_);

  u_int32_t exception_thread_id = 0;
  MinidumpException *exception = dump.GetException();
  if (exception) {
    process_state->crashed_ = true;
    exception_thread_id = exception->GetThreadID();

    process_state->crash_reason_ = GetCrashReason(
        &dump, &process_state->crash_address_);
  }

  MinidumpThreadList *threads = dump.GetThreadList();
  if (!threads) {
    return NULL;
  }

  bool found_crash_thread = false;
  unsigned int thread_count = threads->thread_count();
  for (unsigned int thread_index = 0;
       thread_index < thread_count;
       ++thread_index) {
    MinidumpThread *thread = threads->GetThreadAtIndex(thread_index);
    if (!thread) {
      return NULL;
    }

    if (process_state->crashed_ &&
        thread->GetThreadID() == exception_thread_id) {
      if (found_crash_thread) {
        // There can't be more than one crash thread.
        return NULL;
      }

      process_state->crash_thread_ = thread_index;
      found_crash_thread = true;
    }

    MinidumpMemoryRegion *thread_memory = thread->GetMemory();
    if (!thread_memory) {
      return NULL;
    }

    scoped_ptr<Stackwalker> stackwalker(
        Stackwalker::StackwalkerForCPU(exception->GetContext(),
                                       thread_memory,
                                       dump.GetModuleList(),
                                       supplier_));
    if (!stackwalker.get()) {
      return NULL;
    }

    scoped_ptr<CallStack> stack(stackwalker->Walk());
    if (!stack.get()) {
      return NULL;
    }

    process_state->threads_.push_back(stack.release());
  }

  // If the process crashed, there must be a crash thread.
  if (process_state->crashed_ && !found_crash_thread) {
    return NULL;
  }

  return process_state.release();
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
string MinidumpProcessor::GetCPUInfo(Minidump *dump, string *cpu_info) {
  if (cpu_info)
    cpu_info->clear();

  MinidumpSystemInfo *system_info;
  const MDRawSystemInfo *raw_system_info = GetSystemInfo(dump, &system_info);
  if (!raw_system_info)
    return "";

  string cpu;
  switch (raw_system_info->processor_architecture) {
    case MD_CPU_ARCHITECTURE_X86: {
      cpu = "x86";
      if (cpu_info) {
        const string *cpu_vendor = system_info->GetCPUVendor();
        if (cpu_vendor) {
          cpu_info->assign(*cpu_vendor);
          cpu_info->append(" ");
        }

        char x86_info[36];
        snprintf(x86_info, sizeof(x86_info), "family %u model %u stepping %u",
                 raw_system_info->processor_level,
                 raw_system_info->processor_revision >> 8,
                 raw_system_info->processor_revision & 0xff);
        cpu_info->append(x86_info);
      }
      break;
    }

    case MD_CPU_ARCHITECTURE_PPC: {
      cpu = "ppc";
      break;
    }

    default: {
      // Assign the numeric architecture ID into the CPU string.
      char cpu_string[7];
      snprintf(cpu_string, sizeof(cpu_string), "0x%04x",
               raw_system_info->processor_architecture);
      cpu = cpu_string;
      break;
    }
  }

  return cpu;
}

// static
string MinidumpProcessor::GetOSInfo(Minidump *dump, string *os_version) {
  if (os_version)
    os_version->clear();

  MinidumpSystemInfo *system_info;
  const MDRawSystemInfo *raw_system_info = GetSystemInfo(dump, &system_info);
  if (!raw_system_info)
    return "";

  string os;
  switch (raw_system_info->platform_id) {
    case MD_OS_WIN32_NT: {
      os = "Windows NT";
      break;
    }

    case MD_OS_WIN32_WINDOWS: {
      os = "Windows";
      break;
    }

    case MD_OS_MAC_OS_X: {
      os = "Mac OS X";
      break;
    }

    case MD_OS_LINUX: {
      os = "Linux";
      break;
    }

    default: {
      // Assign the numeric platform ID into the OS string.
      char os_string[11];
      snprintf(os_string, sizeof(os_string), "0x%08x",
               raw_system_info->platform_id);
      os = os_string;
      break;
    }
  }

  if (os_version) {
    char os_version_string[33];
    snprintf(os_version_string, sizeof(os_version_string), "%u.%u.%u",
             raw_system_info->major_version,
             raw_system_info->minor_version,
             raw_system_info->build_number);
    os_version->assign(os_version_string);

    const string *csd_version = system_info->GetCSDVersion();
    if (csd_version) {
      os_version->append(" ");
      os_version->append(*csd_version);
    }
  }

  return os;
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
  snprintf(reason_string, sizeof(reason_string), "0x%08x / 0x%08x",
           raw_exception->exception_record.exception_code,
           raw_exception->exception_record.exception_flags);
  string reason = reason_string;

  const MDRawSystemInfo *raw_system_info = GetSystemInfo(dump, NULL);
  if (!raw_system_info)
    return reason;

  switch (raw_system_info->platform_id) {
    case MD_OS_WIN32_NT:
    case MD_OS_WIN32_WINDOWS: {
      switch (raw_exception->exception_record.exception_code) {
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

}  // namespace google_airbag
