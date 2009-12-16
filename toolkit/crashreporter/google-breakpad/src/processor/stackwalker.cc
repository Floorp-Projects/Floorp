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


#include <cassert>

#include "google_breakpad/processor/stackwalker.h"
#include "google_breakpad/processor/call_stack.h"
#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/code_modules.h"
#include "google_breakpad/processor/minidump.h"
#include "google_breakpad/processor/source_line_resolver_interface.h"
#include "google_breakpad/processor/stack_frame.h"
#include "google_breakpad/processor/symbol_supplier.h"
#include "processor/linked_ptr.h"
#include "processor/logging.h"
#include "processor/scoped_ptr.h"
#include "processor/stack_frame_info.h"
#include "processor/stackwalker_ppc.h"
#include "processor/stackwalker_sparc.h"
#include "processor/stackwalker_x86.h"
#include "processor/stackwalker_amd64.h"

namespace google_breakpad {


Stackwalker::Stackwalker(const SystemInfo *system_info,
                         MemoryRegion *memory,
                         const CodeModules *modules,
                         SymbolSupplier *supplier,
                         SourceLineResolverInterface *resolver)
    : system_info_(system_info),
      memory_(memory),
      modules_(modules),
      supplier_(supplier),
      resolver_(resolver) {
}


bool Stackwalker::Walk(CallStack *stack) {
  BPLOG_IF(ERROR, !stack) << "Stackwalker::Walk requires |stack|";
  assert(stack);
  stack->Clear();

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
      const CodeModule *module =
          modules_->GetModuleForAddress(frame->instruction);
      if (module) {
        frame->module = module;
        if (resolver_ &&
            !resolver_->HasModule(frame->module->code_file()) &&
            supplier_) {
          string symbol_data, symbol_file;
          SymbolSupplier::SymbolResult symbol_result =
              supplier_->GetSymbolFile(module, system_info_,
                                       &symbol_file, &symbol_data);

          switch (symbol_result) {
            case SymbolSupplier::FOUND:
              resolver_->LoadModuleUsingMapBuffer(frame->module->code_file(),
                                                  symbol_data);
              break;
            case SymbolSupplier::NOT_FOUND:
              break;  // nothing to do
            case SymbolSupplier::INTERRUPT:
              return false;
          }
        }
        frame_info.reset(resolver_->FillSourceLineInfo(frame.get()));
      }
    }

    // Add the frame to the call stack.  Relinquish the ownership claim
    // over the frame, because the stack now owns it.
    stack->frames_.push_back(frame.release());

    // Add the frame info to the parallel stack.
    stack_frame_info.push_back(frame_info);
    frame_info.reset(NULL);

    // Get the next frame and take ownership.
    frame.reset(GetCallerFrame(stack, stack_frame_info));
  }

  return true;
}


// static
Stackwalker* Stackwalker::StackwalkerForCPU(
    const SystemInfo *system_info,
    MinidumpContext *context,
    MemoryRegion *memory,
    const CodeModules *modules,
    SymbolSupplier *supplier,
    SourceLineResolverInterface *resolver) {
  if (!context) {
    BPLOG(ERROR) << "Can't choose a stackwalker implementation without context";
    return NULL;
  }

  Stackwalker *cpu_stackwalker = NULL;

  u_int32_t cpu = context->GetContextCPU();
  switch (cpu) {
    case MD_CONTEXT_X86:
      cpu_stackwalker = new StackwalkerX86(system_info,
                                           context->GetContextX86(),
                                           memory, modules, supplier,
                                           resolver);
      break;

    case MD_CONTEXT_PPC:
      cpu_stackwalker = new StackwalkerPPC(system_info,
                                           context->GetContextPPC(),
                                           memory, modules, supplier,
                                           resolver);
      break;

    case MD_CONTEXT_AMD64:
      cpu_stackwalker = new StackwalkerAMD64(system_info,
                                             context->GetContextAMD64(),
                                             memory, modules, supplier,
                                             resolver);
      break;
  
    case MD_CONTEXT_SPARC:
      cpu_stackwalker = new StackwalkerSPARC(system_info,
                                             context->GetContextSPARC(),
                                             memory, modules, supplier,
                                             resolver);
      break;
  }

  BPLOG_IF(ERROR, !cpu_stackwalker) << "Unknown CPU type " << HexString(cpu) <<
                                       ", can't choose a stackwalker "
                                       "implementation";
  return cpu_stackwalker;
}

bool Stackwalker::InstructionAddressSeemsValid(u_int64_t address) {
  const CodeModule *module = modules_->GetModuleForAddress(address);
  if (!module) {
    // not inside any loaded module
    return false;
  }

  if (!resolver_ || !supplier_) {
    // we don't have a resolver and or symbol supplier,
    // but we're inside a known module
    return true;
  }

  if (!resolver_->HasModule(module->code_file())) {
    string symbol_data, symbol_file;
    SymbolSupplier::SymbolResult symbol_result =
      supplier_->GetSymbolFile(module, system_info_,
                               &symbol_file, &symbol_data);

    if (symbol_result != SymbolSupplier::FOUND ||
        !resolver_->LoadModuleUsingMapBuffer(module->code_file(),
                                             symbol_data)) {
      // we don't have symbols, but we're inside a loaded module
      return true;
    }
  }

  StackFrame frame;
  frame.module = module;
  frame.instruction = address;
  resolver_->FillSourceLineInfo(&frame);
  // we have symbols, so return true if inside a function
  return !frame.function_name.empty();
}

}  // namespace google_breakpad
