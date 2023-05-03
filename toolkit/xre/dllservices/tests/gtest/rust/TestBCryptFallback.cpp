/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <ntstatus.h>

#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

#include "gtest/gtest.h"

#include "nsWindowsDllInterceptor.h"

#define RtlGenRandom SystemFunction036
extern "C" BOOLEAN NTAPI RtlGenRandom(PVOID aRandomBuffer,
                                      ULONG aRandomBufferLength);

static mozilla::WindowsDllInterceptor BCryptIntercept;
static mozilla::WindowsDllInterceptor::FuncHookType<
    decltype(&::BCryptGenRandom)>
    stub_BCryptGenRandom;

static mozilla::WindowsDllInterceptor AdvApiIntercept;
static mozilla::WindowsDllInterceptor::FuncHookType<decltype(&RtlGenRandom)>
    stub_RtlGenRandom;

volatile bool gAreHooksActive = false;
volatile bool gHasPanicked = false;
volatile bool gHasReachedBCryptGenRandom = false;
volatile bool gHasReachedRtlGenRandom = false;

NTSTATUS WINAPI patched_BCryptGenRandom(BCRYPT_ALG_HANDLE aAlgorithm,
                                        PUCHAR aBuffer, ULONG aSize,
                                        ULONG aFlags) {
  if (gAreHooksActive) {
    gHasReachedBCryptGenRandom = true;
    // Force BCryptGenRandom failures when the hook is active.
    return STATUS_UNSUCCESSFUL;
  }
  return stub_BCryptGenRandom(aAlgorithm, aBuffer, aSize, aFlags);
}

BOOLEAN NTAPI patched_RtlGenRandom(PVOID aRandomBuffer,
                                   ULONG aRandomBufferLength) {
  if (gAreHooksActive) {
    gHasReachedRtlGenRandom = true;
  }
  return stub_RtlGenRandom(aRandomBuffer, aRandomBufferLength);
}

bool InitInterception() {
  static bool sSuccess = []() {
    BCryptIntercept.Init(L"bcrypt.dll");
    AdvApiIntercept.Init(L"advapi32.dll");
    return stub_BCryptGenRandom.SetDetour(BCryptIntercept, "BCryptGenRandom",
                                          patched_BCryptGenRandom) &&
           stub_RtlGenRandom.SetDetour(AdvApiIntercept, "SystemFunction036",
                                       patched_RtlGenRandom);
  }();
  gAreHooksActive = true;
  return sSuccess;
}

void ExitInterception() { gAreHooksActive = false; }

DWORD WINAPI TestIsFallbackTriggeredThreadProc(LPVOID aParameter) {
  auto testedFunction = reinterpret_cast<void (*)()>(aParameter);
  EXPECT_TRUE(InitInterception());
  MOZ_SEH_TRY { testedFunction(); }
  MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {
    // Catch a potential Rust panic
    gHasPanicked = true;
  }
  ExitInterception();
  return 0;
}

// This function hooks BCryptGenRandom to make it fail, and hooks RtlGenRandom
// to allow us to ensure that it gets visited as a fallback for
// BCryptGenRandom.
void TestIsFallbackTriggered(void (*aTestedFunction)()) {
  gHasPanicked = false;
  gHasReachedBCryptGenRandom = false;
  gHasReachedRtlGenRandom = false;

  // The HashMap test must run on a new thread, because some random bytes have
  // already been collected but not used on the current thread by previous
  // calls to HashMap::new in various locations of the code base. These bytes
  // would be recycled instead of calling into BCryptGenRandom and RtlGenRandom
  // if running the HashMap test on the current thread.
  auto thread =
      ::CreateThread(nullptr, 0, TestIsFallbackTriggeredThreadProc,
                     reinterpret_cast<void*>(aTestedFunction), 0, nullptr);
  EXPECT_TRUE(bool(thread));
  EXPECT_EQ(::WaitForSingleObject(thread, 5000),
            static_cast<DWORD>(WAIT_OBJECT_0));

  EXPECT_FALSE(gHasPanicked);
  EXPECT_TRUE(gHasReachedBCryptGenRandom);
  EXPECT_TRUE(gHasReachedRtlGenRandom);
}

extern "C" void Rust_TriggerGenRandomFromHashMap();
extern "C" void Rust_TriggerGenRandomFromUuid();

TEST(TestBCryptFallback, HashMapTriggersFallback)
{ TestIsFallbackTriggered(Rust_TriggerGenRandomFromHashMap); }

TEST(TestBCryptFallback, UuidTriggersFallback)
{ TestIsFallbackTriggered(Rust_TriggerGenRandomFromUuid); }
