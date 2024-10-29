/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* These assembly functions represent patterns that were already hooked by
 * another application before our detour.
 */

#ifndef mozilla_AssemblyPayloads_h
#define mozilla_AssemblyPayloads_h

#include "mozilla/Attributes.h"

#include <cstdint>

#define PADDING_256_NOP                                              \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;" \
  "nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;"

extern "C" {

#if defined(__clang__)
#  if defined(_M_X64)
constexpr uintptr_t JumpDestination = 0x7fff00000000;

__declspec(dllexport) MOZ_NAKED void MovPushRet() {
  asm volatile(
      "mov %0, %%rax;"
      "push %%rax;"
      "ret;"
      :
      : "i"(JumpDestination));
}

__declspec(dllexport) MOZ_NAKED void MovRaxJump() {
  asm volatile(
      "mov %0, %%rax;"
      "jmpq *%%rax;"
      :
      : "i"(JumpDestination));
}

__declspec(dllexport) MOZ_NAKED void DoubleJump() {
  asm volatile(
      "jmp label1;"

      "label2:"
      "mov %0, %%rax;"
      "jmpq *%%rax;"

      // 0x100 bytes padding to generate jmp rel32 instead of jmp rel8
      PADDING_256_NOP

      "label1:"
      "jmp label2;"
      :
      : "i"(JumpDestination));
}

__declspec(dllexport) MOZ_NAKED void NearJump() {
  asm volatile(
      "jae label3;"
      "je  label3;"
      "jne label3;"

      "label4:"
      "mov %0, %%rax;"
      "jmpq *%%rax;"

      // 0x100 bytes padding to generate jae rel32 instead of jae rel8
      PADDING_256_NOP

      "label3:"
      "jmp label4;"
      :
      : "i"(JumpDestination));
}

__declspec(dllexport) MOZ_NAKED void OpcodeFF() {
  // Skip PUSH (FF /6) because clang prefers Opcode 50+rd
  // to translate PUSH r64 rather than Opcode FF.
  asm volatile(
      "incl %eax;"
      "decl %ebx;"
      "call *%rcx;"
      "jmp *(%rip);"  // Indirect jump to 0xcccccccc`cccccccc
      "int $3;int $3;int $3;int $3;"
      "int $3;int $3;int $3;int $3;");
}

__declspec(dllexport) MOZ_NAKED void IndirectCall() {
  asm volatile(
      "call *(%rip);"  // Indirect call to 0x90909090`90909090
      "nop;nop;nop;nop;nop;nop;nop;nop;"
      "ret;");
}

__declspec(dllexport) MOZ_NAKED void MovImm64() {
  asm volatile(
      "mov $0x1234567812345678, %r10;"
      "nop;nop;nop");
}

static unsigned char __attribute__((used)) gGlobalValue = 0;

__declspec(dllexport) MOZ_NAKED void RexCmpRipRelativeBytePtr() {
  asm volatile(
      "cmpb %sil, gGlobalValue(%rip);"
      "nop;nop;nop;nop;nop;nop;nop;nop;");
}

// A valid function that uses "cmp byte ptr [rip + offset], sil". It returns
// true if and only if gGlobalValue is equal to aValue.
__declspec(dllexport) MOZ_NAKED bool IsEqualToGlobalValue(
    unsigned char aValue) {
  asm volatile(
      "xorl %eax, %eax;"
      "pushq %rsi;"
      "pushq %rcx;"
      "popq %rsi;"
      "cmpb %sil, gGlobalValue(%rip);"
      "nop;"
      // end of 13 first bytes
      "movq $1, %rsi;"
      "cmoveq %rsi, %rax;"
      "popq %rsi;"
      "retq;");
}

// This code reproduces bug 1798787: it uses the same prologue, the same unwind
// info, and it has a call instruction that starts within the 13 first bytes.
MOZ_NAKED void DetouredCallCode(uintptr_t aCallee) {
  asm volatile(
      "subq $0x28, %rsp;"
      "testq %rcx, %rcx;"
      "jz exit;"
      "callq *%rcx;"
      "exit:"
      "addq $0x28, %rsp;"
      "retq;");
}
constexpr uint8_t gDetouredCallCodeSize = 16;  // size of function in bytes
alignas(uint32_t) uint8_t gDetouredCallUnwindInfo[] = {
    0x01,  // Version (1), Flags (0)
    0x04,  // SizeOfProlog (4)
    0x01,  // CountOfUnwindCodes (1)
    0x00,  // FrameRegister (0), FrameOffset (0)
    // UnwindCodes[0]
    0x04,  // .OffsetInProlog (4)
    0x42,  // .UnwindOpCode(UWOP_ALLOC_SMALL=2), .UnwindInfo (4)
};

// This points to the same code as DetouredCallCode, but dynamically generated
// so that it can have custom unwinding info. See TestDllInterceptor.cpp.
extern decltype(&DetouredCallCode) gDetouredCall;

// This is just a jumper: our hooking code will thus detour the jump target
// -- it will not detour DetouredCallJumper. We need to do this to point our
// hooking code to the dynamic code, because our hooking API works with an
// exported function name.
MOZ_NAKED __declspec(dllexport noinline) void DetouredCallJumper(
    uintptr_t aCallee) {
  // Ideally we would want this to be:
  //   jmp qword ptr [rip + offset gDetouredCall]
  // Unfortunately, it is unclear how to do that with inline assembly, so we
  // use a zero offset and patch it before the test.
  asm volatile("jmpq *0(%rip)");
}

#  elif defined(_M_IX86)
constexpr uintptr_t JumpDestination = 0x7fff0000;

__declspec(dllexport) MOZ_NAKED void PushRet() {
  asm volatile(
      "push %0;"
      "ret;"
      :
      : "i"(JumpDestination));
}

__declspec(dllexport) MOZ_NAKED void MovEaxJump() {
  asm volatile(
      "mov %0, %%eax;"
      "jmp *%%eax;"
      :
      : "i"(JumpDestination));
}

__declspec(dllexport) MOZ_NAKED void Opcode83() {
  asm volatile(
      "xor $0x42, %eax;"
      "cmpl $1, 0xc(%ebp);");
}

__declspec(dllexport) MOZ_NAKED void LockPrefix() {
  // Test an instruction with a LOCK prefix (0xf0) at a non-zero offset
  asm volatile(
      "push $0x7c;"
      "lock push $0x7c;");
}

__declspec(dllexport) MOZ_NAKED void LooksLikeLockPrefix() {
  // This is for a regression scenario of bug 1625452, where we double-counted
  // the offset in CountPrefixBytes.  When we count prefix bytes in front of
  // the 2nd PUSH located at offset 2, we mistakenly started counting from
  // the byte 0xf0 at offset 4, which is considered as LOCK, thus we try to
  // detour the next byte 0xcc and it fails.
  //
  // 0: 6a7c       push 7Ch
  // 2: 68ccf00000 push 0F0CCh
  //
  asm volatile(
      "push $0x7c;"
      "push $0x0000f0cc;");
}

__declspec(dllexport) MOZ_NAKED void DoubleJump() {
  asm volatile(
      "jmp label1;"

      "label2:"
      "mov %0, %%eax;"
      "jmp *%%eax;"

      // 0x100 bytes padding to generate jmp rel32 instead of jmp rel8
      PADDING_256_NOP

      "label1:"
      "jmp label2;"
      :
      : "i"(JumpDestination));
}
#  endif

#  if !defined(_M_ARM64)
__declspec(dllexport) MOZ_NAKED void UnsupportedOp() {
  asm volatile(
      "ud2;"
      "nop;nop;nop;nop;nop;nop;nop;nop;"
      "nop;nop;nop;nop;nop;nop;nop;nop;");
}

// bug 1816936
// Make sure no instruction ends at 5 (for x86) or 13 (for x64) bytes
__declspec(dllexport) MOZ_NAKED void SpareBytesAfterDetour() {
  asm volatile(
      "incl %eax;"                // 2 bytes on x64, 1 byte on x86
      "mov $0x01234567, %eax;"    // 5 bytes
      "mov $0xfedcba98, %eax;"    // 5 bytes
      "mov $0x01234567, %eax;"    // 5 bytes
      "mov $0xfedcba98, %eax;");  // 5 bytes
}

// bug 1816936
// Make sure no instruction ends at 10 (for x64) bytes
// This is slightly different than SpareBytesAfterDetour so the compiler doesn't
// combine them, which would make the test that detours this one behave
// unexpectedly since it is already detoured.
__declspec(dllexport) MOZ_NAKED void SpareBytesAfterDetourFor10BytePatch() {
  asm volatile(
      "incl %eax;"                // 2 bytes
      "mov $0x01234567, %ecx;"    // 5 bytes
      "mov $0xfedcba98, %ebx;"    // 5 bytes
      "mov $0x01234567, %eax;"    // 5 bytes
      "mov $0xfedcba98, %edx;");  // 5 bytes
}
#  endif  // !defined(_M_ARM64)

#endif  // defined(__clang__)

}  // extern "C"

#endif  // mozilla_AssemblyPayloads_h
