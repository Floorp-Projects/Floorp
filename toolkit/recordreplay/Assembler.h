/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_Assembler_h
#define mozilla_recordreplay_Assembler_h

#include "InfallibleVector.h"

#include <utility>

namespace mozilla {
namespace recordreplay {

// Assembler for x64 instructions. This is a simple assembler that is primarily
// designed for use in copying instructions from a function that is being
// redirected.
class Assembler
{
public:
  // Create an assembler that allocates its own instruction storage. Assembled
  // code will never be reclaimed by the system.
  Assembler();

  // Create an assembler that uses the specified memory range for instruction
  // storage.
  Assembler(uint8_t* aStorage, size_t aSize);

  ~Assembler();

  // Mark the point at which we start copying an instruction in the original
  // range.
  void NoteOriginalInstruction(uint8_t* aIp);

  // Get the address where the next assembled instruction will be placed.
  uint8_t* Current();

///////////////////////////////////////////////////////////////////////////////
// Routines for assembling instructions in new instruction storage
///////////////////////////////////////////////////////////////////////////////

  // Jump to aTarget. If aTarget is in the range of instructions being copied,
  // the target will be the copy of aTarget instead.
  void Jump(void* aTarget);

  // Conditionally jump to aTarget, depending on the short jump opcode aCode.
  // If aTarget is in the range of instructions being copied, the target will
  // be the copy of aTarget instead.
  void ConditionalJump(uint8_t aCode, void* aTarget);

  // Copy an instruction verbatim from aIp.
  void CopyInstruction(uint8_t* aIp, size_t aSize);

  // push/pop %rax
  void PushRax();
  void PopRax();

  // jump *%rax
  void JumpToRax();

  // call *%rax
  void CallRax();

  // movq/movl/movb 0(%rax), %rax
  void LoadRax(size_t aWidth);

  // cmpq %rax, 0(%rsp)
  void CompareRaxWithTopOfStack();

  // push/pop %rbx
  void PushRbx();
  void PopRbx();

  // movq/movl/movb %rbx, 0(%rax)
  void StoreRbxToRax(size_t aWidth);

  // cmpq/cmpb $literal8, %rax
  void CompareValueWithRax(uint8_t aValue, size_t aWidth);

  // movq $value, %rax
  void MoveImmediateToRax(void* aValue);

  // movq %rax, register
  void MoveRaxToRegister(/*ud_type*/ int aRegister);

  // movq register, %rax
  void MoveRegisterToRax(/*ud_type*/ int aRegister);

  // Normalize a Udis86 register to its 8 byte version, returning UD_NONE/zero
  // for unexpected registers.
  static /*ud_type*/ int NormalizeRegister(/*ud_type*/ int aRegister);

///////////////////////////////////////////////////////////////////////////////
// Routines for assembling instructions at arbitrary locations
///////////////////////////////////////////////////////////////////////////////

  // Return whether it is possible to patch a short jump to aTarget from aIp.
  static bool CanPatchShortJump(uint8_t* aIp, void* aTarget);

  // Patch a short jump to aTarget at aIp.
  static void PatchShortJump(uint8_t* aIp, void* aTarget);

  // Patch a long jump to aTarget at aIp. Rax may be clobbered.
  static void PatchJumpClobberRax(uint8_t* aIp, void* aTarget);

  // Patch the value used in an earlier MoveImmediateToRax call.
  static void PatchMoveImmediateToRax(uint8_t* aIp, void* aValue);

  // Patch an int3 breakpoint instruction at Ip.
  static void PatchClobber(uint8_t* aIp);

private:
  // Patch a jump that doesn't clobber any instructions.
  static void PatchJump(uint8_t* aIp, void* aTarget);

  // Consume some instruction storage.
  void Advance(size_t aSize);

  // The maximum amount we can write at a time without a jump potentially
  // being introduced into the instruction stream.
  static const size_t MaximumAdvance = 20;

  inline size_t CountBytes() { return 0; }

  template <typename... Tail>
  inline size_t CountBytes(uint8_t aByte, Tail... aMoreBytes) {
    return 1 + CountBytes(aMoreBytes...);
  }

  inline void CopyBytes(uint8_t* aIp) {}

  template <typename... Tail>
  inline void CopyBytes(uint8_t* aIp, uint8_t aByte, Tail... aMoreBytes) {
    *aIp = aByte;
    CopyBytes(aIp + 1, aMoreBytes...);
  }

  // Write a complete instruction with bytes specified as parameters.
  template <typename... ByteList>
  inline void NewInstruction(ByteList... aBytes) {
    size_t numBytes = CountBytes(aBytes...);
    MOZ_ASSERT(numBytes <= MaximumAdvance);
    uint8_t* ip = Current();
    CopyBytes(ip, aBytes...);
    Advance(numBytes);
  }

  // Storage for assembling new instructions.
  uint8_t* mCursor;
  uint8_t* mCursorEnd;
  bool mCanAllocateStorage;

  // Association between the instruction original and copy pointers, for all
  // instructions that have been copied.
  InfallibleVector<std::pair<uint8_t*,uint8_t*>> mCopiedInstructions;

  // For jumps we have copied, association between the source (in generated
  // code) and target (in the original code) of the jump. These will be updated
  // to refer to their copy (if there is one) in generated code in the
  // assembler's destructor.
  InfallibleVector<std::pair<uint8_t*,uint8_t*>> mJumps;
};

// The number of instruction bytes required for a short jump.
static const size_t ShortJumpBytes = 2;

// The number of instruction bytes required for a jump that may clobber rax.
static const size_t JumpBytesClobberRax = 12;

// The maximum byte length of an x86/x64 instruction.
static const size_t MaximumInstructionLength = 15;

// Make a region of memory RWX.
void UnprotectExecutableMemory(uint8_t* aAddress, size_t aSize);

} // recordreplay
} // mozilla

#endif // mozilla_recordreplay_Assembler_h
