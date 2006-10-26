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

#ifndef GOOGLE_MINIDUMP_PROCESSOR_H__
#define GOOGLE_MINIDUMP_PROCESSOR_H__

#include <string>

namespace google_airbag {

using std::string;

class Minidump;
class ProcessState;
class SymbolSupplier;

class MinidumpProcessor {
 public:
  // Initializes this MinidumpProcessor.  supplier should be an
  // implementation of the SymbolSupplier abstract base class.
  explicit MinidumpProcessor(SymbolSupplier *supplier);
  ~MinidumpProcessor();

  // Returns a new ProcessState object produced by processing the minidump
  // file.  The caller takes ownership of the ProcessState.  Returns NULL on
  // failure.
  ProcessState* Process(const string &minidump_file);

  // Returns a textual representation of the base CPU type that the minidump
  // in dump was produced on.  Returns an empty string if this information
  // cannot be determined.  If cpu_info is non-NULL, it will be set to
  // contain additional identifying information about the CPU, or it will
  // be set empty if additional information cannot be determined.
  static string GetCPUInfo(Minidump *dump, string *cpu_info);

  // Returns a textual representation of the operating system that the
  // minidump in dump was produced on.  Returns an empty string if this
  // information cannot be determined.  If os_version is non-NULL, it
  // will be set to contain information about the specific version of the
  // OS, or it will be set empty if version information cannot be determined.
  static string GetOSInfo(Minidump *dump, string *os_version);

  // Returns a textual representation of the reason that a crash occurred,
  // if the minidump in dump was produced as a result of a crash.  Returns
  // an empty string if this information cannot be determined.  If address
  // is non-NULL, it will be set to contain the address that caused the
  // exception, if this information is available.  This will be a code
  // address when the crash was caused by problems such as illegal
  // instructions or divisions by zero, or a data address when the crash
  // was caused by a memory access violation.
  static string GetCrashReason(Minidump *dump, u_int64_t *address);

 private:
  SymbolSupplier *supplier_;
};

}  // namespace google_airbag

#endif  // GOOGLE_MINIDUMP_PROCESSOR_H__
