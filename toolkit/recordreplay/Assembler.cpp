/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Assembler.h"

#include "ProcessRecordReplay.h"
#include "udis86/types.h"

#include <sys/mman.h>

namespace mozilla {
namespace recordreplay {

Assembler::Assembler()
  : mCursor(nullptr)
  , mCursorEnd(nullptr)
  , mCanAllocateStorage(true)
{}

Assembler::Assembler(uint8_t* aStorage, size_t aSize)
  : mCursor(aStorage)
  , mCursorEnd(aStorage + aSize)
  , mCanAllocateStorage(false)
{}

Assembler::~Assembler()
{
  // Patch each jump to the point where the jump's target was copied, if there
  // is one.
  for (auto pair : mJumps) {
    uint8_t* source = pair.first;
    uint8_t* target = pair.second;

    for (auto copyPair : mCopiedInstructions) {
      if (copyPair.first == target) {
        PatchJump(source, copyPair.second);
	break;
      }
    }
  }
}

void
Assembler::NoteOriginalInstruction(uint8_t* aIp)
{
  mCopiedInstructions.emplaceBack(aIp, Current());
}

void
Assembler::Advance(size_t aSize)
{
  MOZ_RELEASE_ASSERT(aSize <= MaximumAdvance);
  mCursor += aSize;
}

static const size_t JumpBytes = 17;

uint8_t*
Assembler::Current()
{
  // Reallocate the buffer if there is not enough space. We need enough for the
  // maximum space used by any of the assembling functions, as well as for a
  // following jump for fallthrough to the next allocated space.
  if (size_t(mCursorEnd - mCursor) <= MaximumAdvance + JumpBytes) {
    MOZ_RELEASE_ASSERT(mCanAllocateStorage);

    // Allocate some writable, executable memory.
    static const size_t BufferSize = PageSize;
    uint8_t* buffer = new uint8_t[PageSize];
    UnprotectExecutableMemory(buffer, PageSize);

    if (mCursor) {
      // Patch a jump for fallthrough from the last allocation.
      MOZ_RELEASE_ASSERT(size_t(mCursorEnd - mCursor) >= JumpBytes);
      PatchJump(mCursor, buffer);
    }

    mCursor = buffer;
    mCursorEnd = &buffer[BufferSize];
  }

  return mCursor;
}

static void
Push16(uint8_t** aIp, uint16_t aValue)
{
  (*aIp)[0] = 0x66;
  (*aIp)[1] = 0x68;
  *reinterpret_cast<uint16_t*>(*aIp + 2) = aValue;
  (*aIp) += 4;
}

/* static */ void
Assembler::PatchJump(uint8_t* aIp, void* aTarget)
{
  // Push the target literal onto the stack, 2 bytes at a time. This is
  // apparently the best way of getting an arbitrary 8 byte literal onto the
  // stack, as 4 byte literals we push will be sign extended to 8 bytes.
  size_t ntarget = reinterpret_cast<size_t>(aTarget);
  Push16(&aIp, ntarget >> 48);
  Push16(&aIp, ntarget >> 32);
  Push16(&aIp, ntarget >> 16);
  Push16(&aIp, ntarget);
  *aIp = 0xC3; // ret
}

void
Assembler::Jump(void* aTarget)
{
  PatchJump(Current(), aTarget);
  mJumps.emplaceBack(Current(), (uint8_t*) aTarget);
  Advance(JumpBytes);
}

static uint8_t
OppositeJump(uint8_t aOpcode)
{
  // Get the opposite single byte jump opcode for a one or two byte conditional
  // jump. Opposite opcodes are adjacent, e.g. 0x7C -> jl and 0x7D -> jge.
  if (aOpcode >= 0x80 && aOpcode <= 0x8F) {
    aOpcode -= 0x10;
  } else {
    MOZ_RELEASE_ASSERT(aOpcode >= 0x70 && aOpcode <= 0x7F);
  }
  return (aOpcode & 1) ? aOpcode - 1 : aOpcode + 1;
}

void
Assembler::ConditionalJump(uint8_t aCode, void* aTarget)
{
  uint8_t* ip = Current();
  ip[0] = OppositeJump(aCode);
  ip[1] = (uint8_t) JumpBytes;
  Advance(2);
  Jump(aTarget);
}

void
Assembler::CopyInstruction(uint8_t* aIp, size_t aSize)
{
  MOZ_RELEASE_ASSERT(aSize <= MaximumInstructionLength);
  memcpy(Current(), aIp, aSize);
  Advance(aSize);
}

void
Assembler::PushRax()
{
  NewInstruction(0x50);
}

void
Assembler::PopRax()
{
  NewInstruction(0x58);
}

void
Assembler::JumpToRax()
{
  NewInstruction(0xFF, 0xE0);
}

void
Assembler::CallRax()
{
  NewInstruction(0xFF, 0xD0);
}

void
Assembler::LoadRax(size_t aWidth)
{
  switch (aWidth) {
  case 1: NewInstruction(0x8A, 0x00); break;
  case 2: NewInstruction(0x66, 0x8B, 0x00); break;
  case 4: NewInstruction(0x8B, 0x00); break;
  case 8: NewInstruction(0x48, 0x8B, 0x00); break;
  default: MOZ_CRASH();
  }
}

void
Assembler::CompareRaxWithTopOfStack()
{
  NewInstruction(0x48, 0x39, 0x04, 0x24);
}

void
Assembler::PushRbx()
{
  NewInstruction(0x53);
}

void
Assembler::PopRbx()
{
  NewInstruction(0x5B);
}

void
Assembler::StoreRbxToRax(size_t aWidth)
{
  switch (aWidth) {
  case 1: NewInstruction(0x88, 0x18); break;
  case 2: NewInstruction(0x66, 0x89, 0x18); break;
  case 4: NewInstruction(0x89, 0x18); break;
  case 8: NewInstruction(0x48, 0x89, 0x18); break;
  default: MOZ_CRASH();
  }
}

void
Assembler::CompareValueWithRax(uint8_t aValue, size_t aWidth)
{
  switch (aWidth) {
  case 1: NewInstruction(0x3C, aValue); break;
  case 2: NewInstruction(0x66, 0x83, 0xF8, aValue); break;
  case 4: NewInstruction(0x83, 0xF8, aValue); break;
  case 8: NewInstruction(0x48, 0x83, 0xF8, aValue); break;
  default: MOZ_CRASH();
  }
}

static const size_t MoveImmediateBytes = 10;

/* static */ void
Assembler::PatchMoveImmediateToRax(uint8_t* aIp, void* aValue)
{
  aIp[0] = 0x40 | (1 << 3);
  aIp[1] = 0xB8;
  *reinterpret_cast<void**>(aIp + 2) = aValue;
}

void
Assembler::MoveImmediateToRax(void* aValue)
{
  PatchMoveImmediateToRax(Current(), aValue);
  Advance(MoveImmediateBytes);
}

void
Assembler::MoveRaxToRegister(/*ud_type*/ int aRegister)
{
  MOZ_RELEASE_ASSERT(aRegister == NormalizeRegister(aRegister));

  uint8_t* ip = Current();
  if (aRegister <= UD_R_RDI) {
    ip[0] = 0x48;
    ip[1] = 0x89;
    ip[2] = 0xC0 + aRegister - UD_R_RAX;
  } else {
    ip[0] = 0x49;
    ip[1] = 0x89;
    ip[2] = 0xC0 + aRegister - UD_R_R8;
  }
  Advance(3);
}

void
Assembler::MoveRegisterToRax(/*ud_type*/ int aRegister)
{
  MOZ_RELEASE_ASSERT(aRegister == NormalizeRegister(aRegister));

  uint8_t* ip = Current();
  if (aRegister <= UD_R_RDI) {
    ip[0] = 0x48;
    ip[1] = 0x89;
    ip[2] = 0xC0 + (aRegister - UD_R_RAX) * 8;
  } else {
    ip[0] = 0x4C;
    ip[1] = 0x89;
    ip[2] = 0xC0 + (aRegister - UD_R_R8) * 8;
  }
  Advance(3);
}

/* static */ /*ud_type*/ int
Assembler::NormalizeRegister(/*ud_type*/ int aRegister)
{
  if (aRegister >= UD_R_AL && aRegister <= UD_R_R15B) {
    return aRegister - UD_R_AL + UD_R_RAX;
  }
  if (aRegister >= UD_R_AX && aRegister <= UD_R_R15W) {
    return aRegister - UD_R_AX + UD_R_RAX;
  }
  if (aRegister >= UD_R_EAX && aRegister <= UD_R_R15D) {
    return aRegister - UD_R_EAX + UD_R_RAX;
  }
  if (aRegister >= UD_R_RAX && aRegister <= UD_R_R15) {
    return aRegister;
  }
  return UD_NONE;
}

/* static */ bool
Assembler::CanPatchShortJump(uint8_t* aIp, void* aTarget)
{
  return (aIp + 2 - 128 <= aTarget) && (aIp + 2 + 127 >= aTarget);
}

/* static */ void
Assembler::PatchShortJump(uint8_t* aIp, void* aTarget)
{
  MOZ_RELEASE_ASSERT(CanPatchShortJump(aIp, aTarget));
  aIp[0] = 0xEB;
  aIp[1] = uint8_t(static_cast<uint8_t*>(aTarget) - aIp - 2);
}

/* static */ void
Assembler::PatchJumpClobberRax(uint8_t* aIp, void* aTarget)
{
  PatchMoveImmediateToRax(aIp, aTarget);
  aIp[10] = 0x50; // push %rax
  aIp[11] = 0xC3; // ret
}

/* static */ void
Assembler::PatchClobber(uint8_t* aIp)
{
  aIp[0] = 0xCC; // int3
}

static uint8_t*
PageStart(uint8_t* aPtr)
{
  static_assert(sizeof(size_t) == sizeof(uintptr_t), "Unsupported Platform");
  return reinterpret_cast<uint8_t*>(reinterpret_cast<size_t>(aPtr) & ~(PageSize - 1));
}

void
UnprotectExecutableMemory(uint8_t* aAddress, size_t aSize)
{
  MOZ_ASSERT(aSize);
  uint8_t* pageStart = PageStart(aAddress);
  uint8_t* pageEnd = PageStart(aAddress + aSize - 1) + PageSize;
  int ret = mprotect(pageStart, pageEnd - pageStart, PROT_READ | PROT_EXEC | PROT_WRITE);
  MOZ_RELEASE_ASSERT(ret >= 0);
}

} // namespace recordreplay
} // namespace mozilla
