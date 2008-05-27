// Copyright (c) 2007, Google Inc.
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

// stackwalker_amd64.cc: amd64-specific stackwalker.
//
// See stackwalker_amd64.h for documentation.
//
// Author: Mark Mentovai, Ted Mielczarek


#include "processor/stackwalker_amd64.h"
#include "google_breakpad/processor/call_stack.h"
#include "google_breakpad/processor/memory_region.h"
#include "google_breakpad/processor/stack_frame_cpu.h"
#include "processor/logging.h"

namespace google_breakpad {


StackwalkerAMD64::StackwalkerAMD64(const SystemInfo *system_info,
                                   const MDRawContextAMD64 *context,
                                   MemoryRegion *memory,
                                   const CodeModules *modules,
                                   SymbolSupplier *supplier,
                                   SourceLineResolverInterface *resolver)
    : Stackwalker(system_info, memory, modules, supplier, resolver),
      context_(context) {
}


StackFrame* StackwalkerAMD64::GetContextFrame() {
  if (!context_ || !memory_) {
    BPLOG(ERROR) << "Can't get context frame without context or memory";
    return NULL;
  }

  StackFrameAMD64 *frame = new StackFrameAMD64();

  // The instruction pointer is stored directly in a register, so pull it
  // straight out of the CPU context structure.
  frame->context = *context_;
  frame->context_validity = StackFrameAMD64::CONTEXT_VALID_ALL;
  frame->instruction = frame->context.rip;

  return frame;
}


StackFrame* StackwalkerAMD64::GetCallerFrame(
    const CallStack *stack,
    const vector< linked_ptr<StackFrameInfo> > &stack_frame_info) {
  if (!memory_ || !stack) {
    BPLOG(ERROR) << "Can't get caller frame without memory or stack";
    return NULL;
  }

  StackFrameAMD64 *last_frame = static_cast<StackFrameAMD64*>(
      stack->frames()->back());

  //FIXME: this pretty much doesn't work at all due to FPO
  // being enabled by default.
  // Brain-dead stackwalking:
  // %rip_new = *(%rbp_old + 8)
  // %rsp_new = %rbp_old + 16
  // %rbp_new = *(%rbp_old)

  // A caller frame must reside higher in memory than its callee frames.
  // Anything else is an error, or an indication that we've reached the
  // end of the stack.
  u_int64_t stack_pointer = last_frame->context.rbp + 16;
  if (stack_pointer <= last_frame->context.rsp) {
    return NULL;
  }

  u_int64_t instruction;
  if (!memory_->GetMemoryAtAddress(last_frame->context.rbp + 8,
                                   &instruction) ||
      instruction <= 1) {
    return NULL;
  }

  u_int64_t stack_base;
  if (!memory_->GetMemoryAtAddress(last_frame->context.rbp,
                                   &stack_base) ||
      stack_base <= 1) {
    return NULL;
  }

  StackFrameAMD64 *frame = new StackFrameAMD64();

  frame->context = last_frame->context;
  frame->context.rip = instruction;
  frame->context.rsp = stack_pointer;
  frame->context.rbp = stack_base;
  frame->context_validity = StackFrameAMD64::CONTEXT_VALID_RIP |
                            StackFrameAMD64::CONTEXT_VALID_RSP |
                            StackFrameAMD64::CONTEXT_VALID_RBP;

  frame->instruction = frame->context.rip - 1;

  return frame;
}


}  // namespace google_breakpad
