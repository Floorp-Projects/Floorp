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

// x86-64 general purpose registers.
enum class Register {
  RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15,
};

// Assembler for x64 instructions. This is a simple assembler that is primarily
// designed for use in copying instructions from a function that is being
// redirected.
class Assembler {
 public:
  Assembler(uint8_t* aStorage, size_t aSize);

  // Get the address where the next assembled instruction will be placed.
  uint8_t* Current();

  ///////////////////////////////////////////////////////////////////////////////
  // Routines for assembling instructions in new instruction storage
  ///////////////////////////////////////////////////////////////////////////////

  // Jump to aTarget. If aTarget is in the range of instructions being copied,
  // the target will be the copy of aTarget instead.
  void Jump(void* aTarget);

  // The number of instruction bytes required by Jump().
  static const size_t JumpBytes = 17;

  // Push aValue onto the stack.
  void PushImmediate(void* aValue);

  // The number of instruction bytes required by PushImmediate().
  static const size_t PushImmediateBytes = 16;

  // Return to the address at the top of the stack.
  void Return();

  // For debugging, insert a breakpoint instruction.
  void Breakpoint();

  // push/pop %rax
  void PushRax();
  void PopRax();

  void PopRegister(Register aRegister);

  // movq $value, %rax
  void MoveImmediateToRax(void* aValue);

  // movq %rax, register
  void MoveRaxToRegister(Register aRegister);

  // movq register, %rax
  void MoveRegisterToRax(Register aRegister);

 private:
  // Consume some instruction storage.
  void Advance(size_t aSize);

  // Push a 2 byte immediate onto the stack.
  void Push16(uint16_t aValue);

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
    uint8_t* ip = Current();
    CopyBytes(ip, aBytes...);
    Advance(numBytes);
  }

  // Storage for assembling new instructions.
  uint8_t* mCursor;
  uint8_t* mCursorEnd;
};

// Make a region of memory RWX.
void UnprotectExecutableMemory(uint8_t* aAddress, size_t aSize);

}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_Assembler_h
