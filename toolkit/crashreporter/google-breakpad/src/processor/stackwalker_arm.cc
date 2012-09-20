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

// stackwalker_arm.cc: arm-specific stackwalker.
//
// See stackwalker_arm.h for documentation.
//
// Author: Mark Mentovai, Ted Mielczarek, Jim Blandy


#include "google_breakpad/processor/call_stack.h"
#include "google_breakpad/processor/memory_region.h"
#include "google_breakpad/processor/source_line_resolver_interface.h"
#include "google_breakpad/processor/stack_frame_cpu.h"
#include "processor/cfi_frame_info.h"
#include "processor/logging.h"
#include "processor/scoped_ptr.h"
#include "processor/stackwalker_arm.h"

namespace google_breakpad {


StackwalkerARM::StackwalkerARM(const SystemInfo *system_info,
                               const MDRawContextARM *context,
                               MemoryRegion *memory,
                               const CodeModules *modules,
                               SymbolSupplier *supplier,
                               SourceLineResolverInterface *resolver)
    : Stackwalker(system_info, memory, modules, supplier, resolver),
      context_(context), 
      context_frame_validity_(StackFrameARM::CONTEXT_VALID_ALL) { }


StackFrame* StackwalkerARM::GetContextFrame() {
  if (!context_ || !memory_) {
    BPLOG(ERROR) << "Can't get context frame without context or memory";
    return NULL;
  }

  StackFrameARM *frame = new StackFrameARM();

  // The instruction pointer is stored directly in a register (r15), so pull it
  // straight out of the CPU context structure.
  frame->context = *context_;
  frame->context_validity = context_frame_validity_;
  frame->instruction = frame->context.iregs[15];

  return frame;
}


StackFrame* StackwalkerARM::GetCallerFrame(const CallStack *stack) {
  if (!memory_ || !stack) {
    BPLOG(ERROR) << "Can't get caller frame without memory or stack";
    return NULL;
  }

  const vector<StackFrame *> &frames = *stack->frames();
  StackFrameARM *last_frame = static_cast<StackFrameARM *>(frames.back());

  // See if we have DWARF call frame information covering this address.
  scoped_ptr<CFIFrameInfo> cfi_frame_info(resolver_
                                          ->FindCFIFrameInfo(last_frame));
  if (cfi_frame_info == NULL)
    // Unfortunately, CFI is our only option on the ARM for now. If we
    // add a second strategy, we should put each one in its own function.
    return NULL;

  static const char *register_names[] = {
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8",  "r9",  "r10", "r11", "r12", "sp",  "lr",  "pc",
    "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
    "fps", "cpsr",
    NULL
  };

  // Populate a dictionary with the valid register values in last_frame.
  CFIFrameInfo::RegisterValueMap<u_int32_t> callee_registers;
  for (int i = 0; register_names[i]; i++)
    if (last_frame->context_validity & StackFrameARM::RegisterValidFlag(i))
      callee_registers[register_names[i]] = last_frame->context.iregs[i];

  // Use the STACK CFI data to recover the caller's register values.
  CFIFrameInfo::RegisterValueMap<u_int32_t> caller_registers;
  if (!cfi_frame_info->FindCallerRegs(callee_registers, *memory_,
                                      &caller_registers))
    return NULL;

  // Construct a new stack frame given the values the CFI recovered.
  scoped_ptr<StackFrameARM> frame(new StackFrameARM());
  for (int i = 0; register_names[i]; i++) {
    CFIFrameInfo::RegisterValueMap<u_int32_t>::iterator entry =
      caller_registers.find(register_names[i]);
    if (entry != caller_registers.end()) {
      // We recovered the value of this register; fill the context with the
      // value from caller_registers.
      frame->context_validity |= StackFrameARM::RegisterValidFlag(i);
      frame->context.iregs[i] = entry->second;
    } else if (4 <= i && i <= 11 && (last_frame->context_validity &
                                     StackFrameARM::RegisterValidFlag(i))) {
      // If the STACK CFI data doesn't mention some callee-saves register, and
      // it is valid in the callee, assume the callee has not yet changed it.
      // Registers r4 through r11 are callee-saves, according to the Procedure
      // Call Standard for the ARM Architecture, which the Linux ABI follows.
      frame->context_validity |= StackFrameARM::RegisterValidFlag(i);
      frame->context.iregs[i] = last_frame->context.iregs[i];
    }
  }
  // If the CFI doesn't recover the PC explicitly, then use .ra.
  if (! (frame->context_validity & StackFrameARM::CONTEXT_VALID_PC)) {
    CFIFrameInfo::RegisterValueMap<u_int32_t>::iterator entry =
      caller_registers.find(".ra");
    if (entry != caller_registers.end()) {
      frame->context_validity |= StackFrameARM::CONTEXT_VALID_PC;
      frame->context.iregs[MD_CONTEXT_ARM_REG_PC] = entry->second;
    }
  }
  // If the CFI doesn't recover the SP explicitly, then use .cfa.
  if (! (frame->context_validity & StackFrameARM::CONTEXT_VALID_SP)) {
    CFIFrameInfo::RegisterValueMap<u_int32_t>::iterator entry =
      caller_registers.find(".cfa");
    if (entry != caller_registers.end()) {
      frame->context_validity |= StackFrameARM::CONTEXT_VALID_SP;
      frame->context.iregs[MD_CONTEXT_ARM_REG_SP] = entry->second;
    }
  }

  // If we didn't recover the PC and the SP, then the frame isn't very useful.
  static const int essentials = (StackFrameARM::CONTEXT_VALID_SP
                                 | StackFrameARM::CONTEXT_VALID_PC);
  if ((frame->context_validity & essentials) != essentials)
    return NULL;

  // An instruction address of zero marks the end of the stack.
  if (frame->context.iregs[MD_CONTEXT_ARM_REG_PC] == 0)
    return NULL;

  // If the new stack pointer is at a lower address than the old, then
  // that's clearly incorrect. Treat this as end-of-stack to enforce
  // progress and avoid infinite loops.
  if (frame->context.iregs[MD_CONTEXT_ARM_REG_SP]
      < last_frame->context.iregs[MD_CONTEXT_ARM_REG_SP])
    return NULL;

  // The new frame's context's PC is the return address, which is one
  // instruction past the instruction that caused us to arrive at the
  // callee. Set new_frame->instruction to one less than the PC. This won't
  // reference the beginning of the call instruction, but it's at least
  // within it, which is sufficient to get the source line information to
  // match up with the line that contains the function call. Callers that
  // require the exact return address value may access
  // frame->context.iregs[MD_CONTEXT_ARM_REG_PC].
  frame->instruction = frame->context.iregs[MD_CONTEXT_ARM_REG_PC] - 1;

  return frame.release();
}


}  // namespace google_breakpad
