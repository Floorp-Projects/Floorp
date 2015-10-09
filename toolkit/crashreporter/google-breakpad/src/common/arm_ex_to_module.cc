
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

#include "common/arm_ex_to_module.h"

#include <stdio.h>
#include <assert.h>

// For big-picture comments on how the EXIDX reader works,
// see arm_ex_reader.cc.

#define ARM_EXBUF_START(x) (((x) >> 4) & 0x0f)
#define ARM_EXBUF_COUNT(x) ((x) & 0x0f)
#define ARM_EXBUF_END(x)   (ARM_EXBUF_START(x) + ARM_EXBUF_COUNT(x))

using google_breakpad::Module;

namespace arm_ex_to_module {

static const char* const regnames[] = {
 "r0", "r1", "r2",  "r3",  "r4",  "r5", "r6", "r7",
 "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc",
 "f0", "f1", "f2",  "f3",  "f4",  "f5", "f6", "f7",
 "fps", "cpsr"
};

// Translate command from extab_data to command for Module.
int ARMExToModule::TranslateCmd(const struct extab_data* edata,
                                Module::StackFrameEntry* entry, string& vsp) {
  int ret = 0;
  switch (edata->cmd) {
    case ARM_EXIDX_CMD_FINISH:
      /* Copy LR to PC if there isn't currently a rule for PC in force. */
      if (entry->initial_rules.find("pc")
          == entry->initial_rules.end()) {
        if (entry->initial_rules.find("lr")
            == entry->initial_rules.end()) {
          entry->initial_rules["pc"] = "lr";
        } else {
          entry->initial_rules["pc"] = entry->initial_rules["lr"];
        }
      }
      break;
    case ARM_EXIDX_CMD_SUB_FROM_VSP:
      {
        char c[16];
        sprintf(c, " %d -", edata->data);
        vsp += c;
      }
      break;
    case ARM_EXIDX_CMD_ADD_TO_VSP:
      {
        char c[16];
        sprintf(c, " %d +", edata->data);
        vsp += c;
      }
      break;
    case ARM_EXIDX_CMD_REG_POP:
      for (unsigned int i = 0; i < 16; i++) {
        if (edata->data & (1 << i)) {
          entry->initial_rules[regnames[i]]
            = vsp + " ^";
          vsp += " 4 +";
        }
      }
      /* Set cfa in case the SP got popped. */
      if (edata->data & (1 << 13)) {
        vsp = entry->initial_rules["sp"];
      }
      break;
    case ARM_EXIDX_CMD_REG_TO_SP: {
      assert (edata->data < 16);
      const char* const regname = regnames[edata->data];
      if (entry->initial_rules.find(regname) == entry->initial_rules.end()) {
        entry->initial_rules["sp"] = regname;
      } else {
        entry->initial_rules["sp"] = entry->initial_rules[regname];
      }
      vsp = entry->initial_rules["sp"];
      break;
    }
    case ARM_EXIDX_CMD_VFP_POP:
      /* Don't recover VFP registers, but be sure to adjust the stack
         pointer. */
      for (unsigned int i = ARM_EXBUF_START(edata->data);
           i <= ARM_EXBUF_END(edata->data); i++) {
        vsp += " 8 +";
      }
      if (!(edata->data & ARM_EXIDX_VFP_FSTMD)) {
        vsp += " 4 +";
      }
      break;
    case ARM_EXIDX_CMD_WREG_POP:
      for (unsigned int i = ARM_EXBUF_START(edata->data);
           i <= ARM_EXBUF_END(edata->data); i++) {
        vsp += " 8 +";
      }
      break;
    case ARM_EXIDX_CMD_WCGR_POP:
      // Pop wCGR registers under mask {wCGR3,2,1,0}, hence "i < 4"
      for (unsigned int i = 0; i < 4; i++) {
        if (edata->data & (1 << i)) {
          vsp += " 4 +";
        }
      }
      break;
    case ARM_EXIDX_CMD_REFUSED:
    case ARM_EXIDX_CMD_RESERVED:
      ret = -1;
      break;
  }
  return ret;
}

bool ARMExToModule::HasStackFrame(uintptr_t addr, size_t size) {
  // Invariant: the range [addr,covered) is covered by existing stack
  // frame entries.
  uintptr_t covered = addr;
  while (covered < addr + size) {
    const Module::StackFrameEntry *old_entry =
      module_->FindStackFrameEntryByAddress(covered);
    if (!old_entry) {
      return false;
    }
    covered = old_entry->address + old_entry->size;
  }
  return true;
}

void ARMExToModule::AddStackFrame(uintptr_t addr, size_t size) {
  stack_frame_entry_ = new Module::StackFrameEntry;
  stack_frame_entry_->address = addr;
  stack_frame_entry_->size = size;
  stack_frame_entry_->initial_rules[".cfa"] = "sp";
  vsp_ = "sp";
}

int ARMExToModule::ImproveStackFrame(const struct extab_data* edata) {
  return TranslateCmd(edata, stack_frame_entry_, vsp_) ;
}

void ARMExToModule::DeleteStackFrame() {
  delete stack_frame_entry_;
}

void ARMExToModule::SubmitStackFrame() {
  // return address always winds up in pc
  stack_frame_entry_->initial_rules[".ra"]
    = stack_frame_entry_->initial_rules["pc"];
  // the final value of vsp is the new value of sp
  stack_frame_entry_->initial_rules["sp"] = vsp_;
  module_->AddStackFrameEntry(stack_frame_entry_);
}

}  // namespace arm_ex_to_module
