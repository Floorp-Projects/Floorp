/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WindowsBCryptInitialization.h"

#include "mozilla/RandomNum.h"
#include "nsWindowsDllInterceptor.h"

#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace mozilla {

static WindowsDllInterceptor BCryptIntercept;
static WindowsDllInterceptor::FuncHookType<decltype(&::BCryptGenRandom)>
    stub_BCryptGenRandom;

NTSTATUS WINAPI patched_BCryptGenRandom(BCRYPT_ALG_HANDLE aAlgorithm,
                                        PUCHAR aBuffer, ULONG aSize,
                                        ULONG aFlags) {
  // If we are using the hook, we know that BCRYPT_USE_SYSTEM_PREFERRED_RNG is
  // broken, so let's use the fallback directly in that case.
  if (!aAlgorithm && (aFlags & BCRYPT_USE_SYSTEM_PREFERRED_RNG) && aBuffer &&
      aSize && mozilla::GenerateRandomBytesFromOS(aBuffer, aSize)) {
    return STATUS_SUCCESS;
  }
  return stub_BCryptGenRandom(aAlgorithm, aBuffer, aSize, aFlags);
}

bool WindowsBCryptInitialization() {
  UCHAR buffer[32];
  NTSTATUS status = ::BCryptGenRandom(nullptr, buffer, sizeof(buffer),
                                      BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (NT_SUCCESS(status)) {
    return true;
  }

  BCryptIntercept.Init(L"bcrypt.dll");
  if (!stub_BCryptGenRandom.Set(BCryptIntercept, "BCryptGenRandom",
                                patched_BCryptGenRandom)) {
    return false;
  }

  status = ::BCryptGenRandom(nullptr, buffer, sizeof(buffer),
                             BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  return NT_SUCCESS(status);
}

}  // namespace mozilla
