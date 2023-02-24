/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <windows.h>

#include <array>
#include <utility>

#include "mozilla/WindowsStackCookie.h"

#if defined(DEBUG) && defined(_M_X64) && !defined(__MINGW64__)

uint64_t NoStackCookie(const uint64_t* aArray, size_t aSize) {
  uint64_t result = 0;
  for (size_t i = 0; i < aSize; ++i) {
    result += aArray[i];
  }
  result /= aSize;
  return result;
}

// We expect the following instructions to be generated:
//
// 48 8b 05 XX XX XX XX  mov     rax,qword ptr [__security_cookie]
// 48 31 e0              xor     rax,rsp 48
// 89 44 24 38           mov     qword ptr [rsp+38h],rax
uint64_t StackCookieWithSmallStackSpace(const uint64_t* aArray, size_t aSize) {
  uint64_t array[0x2]{};
  for (size_t i = 0; i < aSize; ++i) {
    array[aArray[i]] += aArray[i];
  }
  return array[0] + array[1];
}

// We expect the following instructions to be generated:
//
// 48 8b 05 XX XX XX XX     mov     rax,qword ptr [__security_cookie]
// 48 31 e0                 xor     rax,rsp
// 48 89 84 24 28 40 00 00  mov     qword ptr [rsp+4028h],rax
uint64_t StackCookieWithLargeStackSpace(const uint64_t* aArray, size_t aSize) {
  uint64_t array[0x800]{};
  for (size_t i = 0; i < aSize; ++i) {
    array[aArray[i]] += aArray[i];
  }
  return array[0] + array[0x7FF];
}

bool TestStackCookieCheck() {
  std::array<std::pair<uintptr_t, bool>, 3> testCases{
      std::make_pair<uintptr_t, bool>(
          reinterpret_cast<uintptr_t>(NoStackCookie), false),
      std::make_pair<uintptr_t, bool>(
          reinterpret_cast<uintptr_t>(StackCookieWithSmallStackSpace), true),
      std::make_pair<uintptr_t, bool>(
          reinterpret_cast<uintptr_t>(StackCookieWithLargeStackSpace), true),
  };
  for (auto [functionAddress, expectStackCookieCheck] : testCases) {
    if (mozilla::HasStackCookieCheck(functionAddress) !=
        expectStackCookieCheck) {
      printf(
          "TEST-FAILED | StackCookie | Wrong output from HasStackCookieCheck "
          "for function at %p (expected %d).\n",
          reinterpret_cast<void*>(functionAddress), expectStackCookieCheck);
      return false;
    }
    printf(
        "TEST-PASS | StackCookie | Correct output from HasStackCookieCheck for "
        "function at %p (expected %d).\n",
        reinterpret_cast<void*>(functionAddress), expectStackCookieCheck);
  }
  return true;
}

#endif  // defined(DEBUG) && defined(_M_X64) && !defined(__MINGW64__)

int wmain(int argc, wchar_t* argv[]) {
#if defined(DEBUG) && defined(_M_X64) && !defined(__MINGW64__)
  if (!TestStackCookieCheck()) {
    return 1;
  }
#endif  // defined(DEBUG) && defined(_M_X64) && !defined(__MINGW64__)

  printf("TEST-PASS | StackCookie | All tests ran successfully\n");
  return 0;
}
