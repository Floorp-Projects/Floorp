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

extern "C" {

#if defined(__clang__)
#  if defined(_M_X64)
constexpr uintptr_t JumpDestination = 0x7fff00000000;

__declspec(dllexport) __attribute__((naked)) void MovPushRet() {
  asm volatile(
      "mov %0, %%rax;"
      "push %%rax;"
      "ret;"
      :
      : "i"(JumpDestination));
}

__declspec(dllexport) __attribute__((naked)) void MovRaxJump() {
  asm volatile(
      "mov %0, %%rax;"
      "jmpq *%%rax;"
      :
      : "i"(JumpDestination));
}
#  elif defined(_M_IX86)
constexpr uintptr_t JumpDestination = 0x7fff0000;

__declspec(dllexport) __attribute__((naked)) void PushRet() {
  asm volatile(
      "push %0;"
      "ret;"
      :
      : "i"(JumpDestination));
}

__declspec(dllexport) __attribute__((naked)) void MovEaxJump() {
  asm volatile(
      "mov %0, %%eax;"
      "jmp *%%eax;"
      :
      : "i"(JumpDestination));
}

__declspec(dllexport) __attribute__((naked)) void Opcode83() {
  asm volatile(
      "xor $0x42, %eax;"
      "cmpl $1, 0xc(%ebp);");
}

__declspec(dllexport) __attribute__((naked)) void LockPrefix() {
  // Test an instruction with a LOCK prefix (0xf0) at a non-zero offset
  asm volatile(
      "push $0x7c;"
      "lock push $0x7c;");
}

__declspec(dllexport) __attribute__((naked)) void LooksLikeLockPrefix() {
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
#  endif
#endif  // defined(__clang__)

}  // extern "C"

#endif  // mozilla_AssemblyPayloads_h
