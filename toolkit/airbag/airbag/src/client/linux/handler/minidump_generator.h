// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Author: Li Liu
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

#ifndef CLIENT_LINUX_HANDLER_MINIDUMP_GENERATOR_H__
#define CLIENT_LINUX_HANDLER_MINIDUMP_GENERATOR_H__

#include "google_breakpad/common/breakpad_types.h"
#include "processor/scoped_ptr.h"

struct sigcontext;

namespace google_breakpad {

//
// MinidumpGenerator
//
// Write a minidump to file based on the signo and sig_ctx.
// A minidump generator should be created before any exception happen.
//
class MinidumpGenerator {
  public:
   MinidumpGenerator();

   ~MinidumpGenerator();

   // Write minidump.
   bool WriteMinidumpToFile(const char *file_pathname,
                            int signo,
                            const struct sigcontext *sig_ctx) const;
  private:
   // Allocate memory for stack.
   void AllocateStack();

  private:
   // Stack size of the writer thread.
   static const int kStackSize = 1024 * 1024;
   scoped_array<char> stack_;
};

}  // namespace google_breakpad

#endif   // CLIENT_LINUX_HANDLER_MINIDUMP_GENERATOR_H__
