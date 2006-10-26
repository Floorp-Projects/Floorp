// Copyright (C) 2006 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// stackwalker_selftest.cc: Tests StackwalkerX86 or StackwalkerPPC using the
// running process' stack as test data, if running on an x86 or ppc and
// compiled with gcc.  This test is not enabled in the "make check" suite
// by default, because certain optimizations interfere with its proper
// operation.  To turn it on, configure with --enable-selftest.
//
// Optimizations that cause problems:
//  - stack frame reuse.  The Recursor function here calls itself with
//    |return Recursor|.  When the caller's frame is reused, it will cause
//    CountCallerFrames to correctly return the same number of frames
//    in both the caller and callee.  This is considered an unexpected
//    condition in the test, which expects a callee to have one more
//    caller frame in the stack than its caller.
//  - frame pointer omission.  Even with a stackwalker that understands
//    this optimization, the code to harness debug information currently
//    only exists to retrieve it from minidumps, not the current process.
//
// This test can also serve as a developmental and debugging aid if
// PRINT_STACKS is defined.
//
// Author: Mark Mentovai

#if defined(__GNUC__) && (defined(__i386__) || defined(__ppc__))


#include <cstdio>

#include "google/airbag_types.h"
#include "google/call_stack.h"
#include "google/stack_frame.h"
#include "google/stack_frame_cpu.h"
#include "processor/memory_region.h"
#include "processor/minidump_format.h"
#include "processor/scoped_ptr.h"

using google_airbag::CallStack;
using google_airbag::MemoryRegion;
using google_airbag::scoped_ptr;
using google_airbag::StackFrame;
using google_airbag::StackFramePPC;
using google_airbag::StackFrameX86;

#if defined(__i386__)
#include "processor/stackwalker_x86.h"
using google_airbag::StackwalkerX86;
#elif defined(__ppc__)
#include "processor/stackwalker_ppc.h"
using google_airbag::StackwalkerPPC;
#endif  // __i386__ || __ppc__

#define RECURSION_DEPTH 100


// A simple MemoryRegion subclass that provides direct access to this
// process' memory space by pointer.
class SelfMemoryRegion : public MemoryRegion {
 public:
  virtual u_int64_t GetBase() { return 0; }
  virtual u_int32_t GetSize() { return 0xffffffff; }

  bool GetMemoryAtAddress(u_int64_t address, u_int8_t*  value) {
      return GetMemoryAtAddressInternal(address, value); }
  bool GetMemoryAtAddress(u_int64_t address, u_int16_t* value) {
      return GetMemoryAtAddressInternal(address, value); }
  bool GetMemoryAtAddress(u_int64_t address, u_int32_t* value) {
      return GetMemoryAtAddressInternal(address, value); }
  bool GetMemoryAtAddress(u_int64_t address, u_int64_t* value) {
      return GetMemoryAtAddressInternal(address, value); }

 private:
  template<typename T> bool GetMemoryAtAddressInternal(u_int64_t address,
                                                       T*        value) {
    // Without knowing what addresses are actually mapped, just assume that
    // everything low is not mapped.  This helps the stackwalker catch the
    // end of a stack when it tries to dereference a null or low pointer
    // in an attempt to find the caller frame.  Other unmapped accesses will
    // cause the program to crash, but that would properly be a test failure.
    if (address < 0x100)
      return false;

    u_int8_t* memory = 0;
    *value = *reinterpret_cast<const T*>(&memory[address]);
    return true;
  }
};


#if defined(__i386__)

// GetEBP returns the current value of the %ebp register.  Because it's
// implemented as a function, %ebp itself contains GetEBP's frame pointer
// and not the caller's frame pointer.  Dereference %ebp to obtain the
// caller's frame pointer, which the compiler-generated preamble stored
// on the stack (provided frame pointers are not being omitted.)  Because
// this function depends on the compiler-generated preamble, inlining is
// disabled.
static u_int32_t GetEBP() __attribute__((noinline));
static u_int32_t GetEBP() {
  u_int32_t ebp;
  __asm__ __volatile__(
    "movl (%%ebp), %0"
    : "=a" (ebp)
  );
  return ebp;
}


// The caller's %esp is 8 higher than the value of %ebp in this function,
// assuming that it's not inlined and that the standard prolog is used.
// The CALL instruction places a 4-byte return address on the stack above
// the caller's %esp, and this function's prolog will save the caller's %ebp
// on the stack as well, for another 4 bytes, before storing %esp in %ebp.
static u_int32_t GetESP() __attribute__((noinline));
static u_int32_t GetESP() {
  u_int32_t ebp;
  __asm__ __volatile__(
    "movl %%ebp, %0"
    : "=a" (ebp)
  );
  return ebp + 8;
}


// GetEIP returns the instruction pointer identifying the next instruction
// to execute after GetEIP returns.  It obtains this information from the
// stack, where it was placed by the call instruction that called GetEIP.
// This function depends on frame pointers not being omitted.  It is possible
// to write a pure asm version of this routine that has no compiler-generated
// preamble and uses %esp instead of %ebp; that would function in the
// absence of frame pointers.  However, the simpler approach is used here
// because GetEBP and stackwalking necessarily depends on access to frame
// pointers.  Because this function depends on a call instruction and the
// compiler-generated preamble, inlining is disabled.
static u_int32_t GetEIP() __attribute__((noinline));
static u_int32_t GetEIP() {
  u_int32_t eip;
  __asm__ __volatile__(
    "movl 4(%%ebp), %0"
    : "=a" (eip)
  );
  return eip;
}


#elif defined(__ppc__)


// GetSP returns the current value of the %r1 register, which by convention,
// is the stack pointer on ppc.  Because it's implemented as a function,
// %r1 itself contains GetSP's own stack pointer and not the caller's stack
// pointer.  Dereference %r1 to obtain the caller's stack pointer, which the
// compiler-generated prolog stored on the stack.  Because this function
// depends on the compiler-generated prolog, inlining is disabled.
static u_int32_t GetSP() __attribute__((noinline));
static u_int32_t GetSP() {
  u_int32_t sp;
  __asm__ __volatile__(
    "lwz %0, 0(r1)"
    : "=r" (sp)
  );
  return sp;
}


// GetPC returns the program counter identifying the next instruction to
// execute after GetPC returns.  It obtains this information from the
// link register, where it was placed by the branch instruction that called
// GetPC.  Because this function depends on the caller's use of a branch
// instruction, inlining is disabled.
static u_int32_t GetPC() __attribute__((noinline));
static u_int32_t GetPC() {
  u_int32_t lr;
  __asm__ __volatile__(
    "mflr %0"
    : "=r" (lr)
  );
  return lr;
}


#endif  // __i386__ || __ppc__


// CountCallerFrames returns the number of stack frames beneath the function
// that called CountCallerFrames.  Because this function's return value
// is dependent on the size of the stack beneath it, inlining is disabled,
// and any function that calls this should not be inlined either.
static unsigned int CountCallerFrames() __attribute__((noinline));
static unsigned int CountCallerFrames() {
  SelfMemoryRegion memory;

#if defined(__i386__)
  MDRawContextX86 context = MDRawContextX86();
  context.eip = GetEIP();
  context.ebp = GetEBP();
  context.esp = GetESP();

  StackwalkerX86 stackwalker = StackwalkerX86(&context, &memory, NULL, NULL);
#elif defined(__ppc__)
  MDRawContextPPC context = MDRawContextPPC();
  context.srr0 = GetPC();
  context.gpr[1] = GetSP();

  StackwalkerPPC stackwalker = StackwalkerPPC(&context, &memory, NULL, NULL);
#endif  // __i386__ || __ppc__

  scoped_ptr<CallStack> stack(stackwalker.Walk());

#ifdef PRINT_STACKS
  printf("\n");
  for (unsigned int frame_index = 0;
      frame_index < stack->frames()->size();
      ++frame_index) {
    StackFrame *frame = stack->frames()->at(frame_index);
    printf("frame %-3d  instruction = 0x%08llx",
           frame_index, frame->instruction);
#if defined(__i386__)
    StackFrameX86 *frame_x86 = reinterpret_cast<StackFrameX86*>(frame);
    printf("  esp = 0x%08x  ebp = 0x%08x\n",
           frame_x86->context.esp, frame_x86->context.ebp);
#elif defined(__ppc__)
    StackFramePPC *frame_ppc = reinterpret_cast<StackFramePPC*>(frame);
    printf("  gpr[1] = 0x%08x\n", frame_ppc->context.gpr[1]);
#endif  // __i386__ || __ppc__
  }
#endif  // PRINT_STACKS

  // Subtract 1 because the caller wants the number of frames beneath
  // itself.  Because the caller called us, subract two for our frame and its
  // frame, which are included in stack->size().
  return stack->frames()->size() - 2;
}


// Recursor verifies that the number stack frames beneath itself is one more
// than the number of stack frames beneath its parent.  When depth frames
// have been reached, Recursor stops checking and returns success.  If the
// frame count check fails at any depth, Recursor will stop and return false.
// Because this calls CountCallerFrames, inlining is disabled.
static bool Recursor(unsigned int depth, unsigned int parent_callers)
    __attribute__((noinline));
static bool Recursor(unsigned int depth, unsigned int parent_callers) {
  unsigned int callers = CountCallerFrames();
  if (callers != parent_callers + 1)
    return false;

  if (depth)
    return Recursor(depth - 1, callers);

  // depth == 0
  return true;
}


// Because this calls CountCallerFrames, inlining is disabled - but because
// it's main (and nobody calls it other than the entry point), it wouldn't
// be inlined anyway.
int main(int argc, char** argv) __attribute__((noinline));
int main(int argc, char** argv) {
  return Recursor(RECURSION_DEPTH, CountCallerFrames()) ? 0 : 1;
}


#else  // __GNUC__ && (__i386__ || __ppc__)
// Not gcc?  We use gcc's __asm__.
// Not i386 or ppc?  We can only test stacks we know how to walk.


int main(int argc, char **argv) {
  // "make check" interprets an exit status of 77 to mean that the test is
  // not supported.
  return 77;
}


#endif  // __GNUC__ && (__i386__ || __ppc__)
