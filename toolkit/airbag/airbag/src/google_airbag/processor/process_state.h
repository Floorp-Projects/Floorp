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

// process_state.h: A snapshot of a process, in a fully-digested state.
//
// Author: Mark Mentovai

#ifndef GOOGLE_AIRBAG_PROCESSOR_PROCESS_STATE_H__
#define GOOGLE_AIRBAG_PROCESSOR_PROCESS_STATE_H__

#include <string>
#include <vector>

namespace google_airbag {

using std::string;
using std::vector;

class CallStack;

class ProcessState {
 public:
  ~ProcessState();

  // Accessors.  See the data declarations below.
  bool crashed() const { return crashed_; }
  string crash_reason() const { return crash_reason_; }
  u_int64_t crash_address() const { return crash_address_; }
  int requesting_thread() const { return requesting_thread_; }
  const vector<CallStack*>* threads() const { return &threads_; }
  string os() const { return os_; }
  string os_version() const { return os_version_; }
  string cpu() const { return cpu_; }
  string cpu_info() const { return cpu_info_; }

 private:
  // MinidumpProcessor is responsible for building ProcessState objects.
  friend class MinidumpProcessor;

  // Disallow instantiation other than by friends.
  ProcessState() : crashed_(false), crash_reason_(), crash_address_(0),
                   requesting_thread_(-1), threads_(), os_(), os_version_(),
                   cpu_(), cpu_info_() {}

  // True if the process crashed, false if the dump was produced outside
  // of an exception handler.
  bool crashed_;

  // If the process crashed, the type of crash.  OS- and possibly CPU-
  // specific.  For example, "EXCEPTION_ACCESS_VIOLATION" (Windows),
  // "EXC_BAD_ACCESS / KERN_INVALID_ADDRESS" (Mac OS X), "SIGSEGV"
  // (other Unix).
  string crash_reason_;

  // If the process crashed, and if crash_reason implicates memory,
  // the memory address that caused the crash.  For data access errors,
  // this will be the data address that caused the fault.  For code errors,
  // this will be the address of the instruction that caused the fault.
  u_int64_t crash_address_;

  // The index of the thread that requested a dump be written in the
  // threads vector.  If a dump was produced as a result of a crash, this
  // will point to the thread that crashed.  If the dump was produced as
  // by user code without crashing, and the dump contains extended Airbag
  // information, this will point to the thread that requested the dump.
  // If the dump was not produced as a result of an exception and no
  // extended Airbag information is present, this field will be set to -1,
  // indicating that the dump thread is not available.
  int requesting_thread_;

  // Stacks for each thread (except possibly the exception handler
  // thread) at the time of the crash.
  vector<CallStack*> threads_;

  // A string identifying the operating system, such as "Windows NT",
  // "Mac OS X", or "Linux".  If the information is present in the dump but
  // its value is unknown, this field will contain a numeric value.  If
  // the information is not present in the dump, this field will be empty.
  string os_;

  // A string identifying the version of the operating system, such as
  // "5.1.2600 Service Pack 2" or "10.4.8 8L2127".  If the dump does not
  // contain this information, this field will be empty.
  string os_version_;

  // A string identifying the basic CPU family, such as "x86" or "ppc".
  // If this information is present in the dump but its value is unknown,
  // this field will contain a numeric value.  If the information is not
  // present in the dump, this field will be empty.
  string cpu_;

  // A string further identifying the specific CPU, such as
  // "GenuineIntel level 6 model 13 stepping 8".  If the information is not
  // present in the dump, or additional identifying information is not
  // defined for the CPU family, this field will be empty.
  string cpu_info_;
};

}  // namespace google_airbag

#endif  // GOOGLE_AIRBAG_PROCESSOR_CALL_STACK_H__
