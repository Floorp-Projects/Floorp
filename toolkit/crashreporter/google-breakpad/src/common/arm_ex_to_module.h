
/* libunwind - a platform-independent unwind library
   Copyright 2011 Linaro Limited

This file is part of libunwind.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

// Copyright (c) 2010 Google Inc.
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


// Derived from libunwind, with extensive modifications.

#ifndef COMMON_ARM_EX_TO_MODULE__
#define COMMON_ARM_EX_TO_MODULE__

#include "common/module.h"

#include <string.h>

namespace arm_ex_to_module {

using google_breakpad::Module;

typedef enum extab_cmd {
  ARM_EXIDX_CMD_FINISH,
  ARM_EXIDX_CMD_SUB_FROM_VSP,
  ARM_EXIDX_CMD_ADD_TO_VSP,
  ARM_EXIDX_CMD_REG_POP,
  ARM_EXIDX_CMD_REG_TO_SP,
  ARM_EXIDX_CMD_VFP_POP,
  ARM_EXIDX_CMD_WREG_POP,
  ARM_EXIDX_CMD_WCGR_POP,
  ARM_EXIDX_CMD_RESERVED,
  ARM_EXIDX_CMD_REFUSED,
} extab_cmd_t;

struct exidx_entry {
  uint32_t addr;
  uint32_t data;
};

struct extab_data {
  extab_cmd_t cmd;
  uint32_t data;
};

enum extab_cmd_flags {
  ARM_EXIDX_VFP_SHIFT_16 = 1 << 16,
  ARM_EXIDX_VFP_FSTMD = 1 << 17, // distinguishes FSTMxxD from FSTMxxX
};

const string kRA = ".ra";
const string kCFA = ".cfa";

static const char* const regnames[] = {
 "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
 "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc",
 "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
 "fps", "cpsr"
};

// Receives information from arm_ex_reader::ExceptionTableInfo
// and adds it to the Module object
class ARMExToModule {
 public:
  ARMExToModule(Module* module)
      : module_(module) { }
  ~ARMExToModule() { }
  void AddStackFrame(uintptr_t addr, size_t size);
  int ImproveStackFrame(const struct extab_data* edata);
  void DeleteStackFrame();
  void SubmitStackFrame();
 private:
  Module* module_;
  Module::StackFrameEntry* stack_frame_entry_;
  string vsp_;
  int TranslateCmd(const struct extab_data* edata,
                   Module::StackFrameEntry* entry,
                   string& vsp);
};

} // namespace arm_ex_to_module

#endif // COMMON_ARM_EX_TO_MODULE__
