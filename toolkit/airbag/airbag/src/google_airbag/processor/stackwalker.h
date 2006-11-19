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

// stackwalker.h: Generic stackwalker.
//
// The Stackwalker class is an abstract base class providing common generic
// methods that apply to stacks from all systems.  Specific implementations
// will extend this class by providing GetContextFrame and GetCallerFrame
// methods to fill in system-specific data in a StackFrame structure.
// Stackwalker assembles these StackFrame strucutres into a CallStack.
//
// Author: Mark Mentovai


#ifndef GOOGLE_AIRBAG_PROCESSOR_STACKWALKER_H__
#define GOOGLE_AIRBAG_PROCESSOR_STACKWALKER_H__

#include <vector>

namespace google_airbag {

class CallStack;
template<typename T> class linked_ptr;
class MemoryRegion;
class MinidumpContext;
class MinidumpModuleList;
struct StackFrame;
struct StackFrameInfo;
class SymbolSupplier;

using std::vector;


class Stackwalker {
 public:
  virtual ~Stackwalker() {}

  // Creates a new CallStack and populates it by calling GetContextFrame and
  // GetCallerFrame.  The frames are further processed to fill all available
  // data.  The caller takes ownership of the CallStack returned by Walk.
  CallStack* Walk();

  // Returns a new concrete subclass suitable for the CPU that a stack was
  // generated on, according to the CPU type indicated by the context
  // argument.  If no suitable concrete subclass exists, returns NULL.
  static Stackwalker* StackwalkerForCPU(MinidumpContext *context,
                                        MemoryRegion *memory,
                                        MinidumpModuleList *modules,
                                        SymbolSupplier *supplier);

 protected:
  // memory identifies a MemoryRegion that provides the stack memory
  // for the stack to walk.  modules, if non-NULL, is a MinidumpModuleList
  // that is used to look up which code module each stack frame is
  // associated with.  supplier is an optional caller-supplied SymbolSupplier
  // implementation.  If supplier is NULL, source line info will not be
  // resolved.
  Stackwalker(MemoryRegion *memory,
              MinidumpModuleList *modules,
              SymbolSupplier *supplier);

  // The stack memory to walk.  Subclasses will require this region to
  // get information from the stack.
  MemoryRegion *memory_;

 private:
  // Obtains the context frame, the innermost called procedure in a stack
  // trace.  Returns NULL on failure.  GetContextFrame allocates a new
  // StackFrame (or StackFrame subclass), ownership of which is taken by
  // the caller.
  virtual StackFrame* GetContextFrame() = 0;

  // Obtains a caller frame.  Each call to GetCallerFrame should return the
  // frame that called the last frame returned by GetContextFrame or
  // GetCallerFrame.  To aid this purpose, stack contains the CallStack
  // made of frames that have already been walked.  GetCallerFrame should
  // return NULL on failure or when there are no more caller frames (when
  // the end of the stack has been reached).  GetCallerFrame allocates a new
  // StackFrame (or StackFrame subclass), ownership of which is taken by
  // the caller.
  virtual StackFrame* GetCallerFrame(
      const CallStack *stack,
      const vector< linked_ptr<StackFrameInfo> > &stack_frame_info) = 0;

  // A list of modules, for populating each StackFrame's module information.
  // This field is optional and may be NULL.
  MinidumpModuleList *modules_;

  // The optional SymbolSupplier for resolving source line info.
  SymbolSupplier *supplier_;
};


}  // namespace google_airbag


#endif  // GOOGLE_AIRBAG_PROCESSOR_STACKWALKER_H__
