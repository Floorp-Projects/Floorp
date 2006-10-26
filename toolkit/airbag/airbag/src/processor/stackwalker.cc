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

// stackwalker.cc: Generic stackwalker.
//
// See stackwalker.h for documentation.
//
// Author: Mark Mentovai


#include "processor/stackwalker.h"
#include "google/call_stack.h"
#include "google/stack_frame.h"
#include "google/symbol_supplier.h"
#include "processor/linked_ptr.h"
#include "processor/minidump.h"
#include "processor/scoped_ptr.h"
#include "processor/source_line_resolver.h"
#include "processor/stack_frame_info.h"
#include "processor/stackwalker_ppc.h"
#include "processor/stackwalker_x86.h"

namespace google_airbag {


Stackwalker::Stackwalker(MemoryRegion *memory, MinidumpModuleList *modules,
                         SymbolSupplier *supplier)
    : memory_(memory), modules_(modules), supplier_(supplier) {
}


CallStack* Stackwalker::Walk() {
  SourceLineResolver resolver;

  scoped_ptr<CallStack> stack(new CallStack());

  // stack_frame_info parallels the CallStack.  The vector is passed to the
  // GetCallerFrame function.  It contains information that may be helpful
  // for stackwalking.
  vector< linked_ptr<StackFrameInfo> > stack_frame_info;

  // Begin with the context frame, and keep getting callers until there are
  // no more.

  // Take ownership of the pointer returned by GetContextFrame.
  scoped_ptr<StackFrame> frame(GetContextFrame());

  while (frame.get()) {
    // frame already contains a good frame with properly set instruction and
    // frame_pointer fields.  The frame structure comes from either the
    // context frame (above) or a caller frame (below).

    linked_ptr<StackFrameInfo> frame_info;

    // Resolve the module information, if a module map was provided.
    if (modules_) {
      MinidumpModule *module =
          modules_->GetModuleForAddress(frame->instruction);
      if (module) {
        frame->module_name = *(module->GetName());
        frame->module_base = module->base_address();
        if (!resolver.HasModule(frame->module_name) && supplier_) {
          string symbol_file = supplier_->GetSymbolFile(module);
          if (!symbol_file.empty()) {
            resolver.LoadModule(frame->module_name, symbol_file);
          }
        }
        frame_info.reset(resolver.FillSourceLineInfo(frame.get()));
      }
    }

    // Add the frame to the call stack.  Relinquish the ownership claim
    // over the frame, because the stack now owns it.
    stack->frames_.push_back(frame.release());

    // Add the frame info to the parallel stack.
    stack_frame_info.push_back(frame_info);
    frame_info.reset(NULL);

    // Get the next frame and take ownership.
    frame.reset(GetCallerFrame(stack.get(), stack_frame_info));
  }

  return stack.release();
}


// static
Stackwalker* Stackwalker::StackwalkerForCPU(MinidumpContext *context,
                                            MemoryRegion *memory,
                                            MinidumpModuleList *modules,
                                            SymbolSupplier *supplier) {
  Stackwalker *cpu_stackwalker = NULL;

  u_int32_t cpu = context->GetContextCPU();
  switch (cpu) {
    case MD_CONTEXT_X86:
      cpu_stackwalker = new StackwalkerX86(context->GetContextX86(),
                                           memory, modules, supplier);
      break;

    case MD_CONTEXT_PPC:
      cpu_stackwalker = new StackwalkerPPC(context->GetContextPPC(),
                                           memory, modules, supplier);
      break;
  }

  return cpu_stackwalker;
}


}  // namespace google_airbag
