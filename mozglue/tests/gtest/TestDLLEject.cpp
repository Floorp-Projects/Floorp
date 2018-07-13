/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <Windows.h>
#include <winternl.h>
#include "gtest/gtest.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsDllBlocklist.h"

static HANDLE sThreadWasBlocked = 0;
static HANDLE sThreadWasAllowed = 0;
static HANDLE sDllWasLoaded = 0;
static uintptr_t sStartAddress = 0;

static const int sTimeoutMS = 10000;

#define DLL_LEAF_NAME (u"InjectorDLL.dll")

static nsString
makeString(PUNICODE_STRING aOther)
{
  size_t numChars = aOther->Length / sizeof(WCHAR);
  return nsString((const char16_t *)aOther->Buffer, numChars);
}

static void
DllLoadHook(bool aDllLoaded, NTSTATUS aStatus, HANDLE aDllBase,
            PUNICODE_STRING aDllName)
{
  nsString str = makeString(aDllName);

  nsString dllName = nsString(DLL_LEAF_NAME);
  if (StringEndsWith(str, dllName, nsCaseInsensitiveStringComparator())) {
    if (aDllLoaded) {
      SetEvent(sDllWasLoaded);
    }
  }
}

static void
CreateThreadHook(bool aWasAllowed, void* aStartAddress)
{
  if (sStartAddress == (uintptr_t)aStartAddress) {
    if (!aWasAllowed) {
      SetEvent(sThreadWasBlocked);
    } else {
      SetEvent(sThreadWasAllowed);
    }
  }
}

/**
 * This function tests that we correctly block DLLs injected into this process
 * via an injection technique which calls CreateRemoteThread with LoadLibrary*()
 * as the thread start address, and the path to the DLL as the thread param.
 *
 * We prevent this technique by blocking threads with a start address in any
 * LoadLibrary*() APIs.
 *
 * This function launches Injector.exe which simulates a 3rd-party application
 * executing this technique.
 *
 * @param aGetArgsProc  A callable procedure that specifies the thread start
 *                      address and thread param passed as arguments to
 *                      Injector.exe, which are in turn passed as arguments to
 *                      CreateRemoteThread. This procedure is defined as such:
 *
 *                      void (*aGetArgsProc)(const nsString& aDllPath,
 *                                           const nsCString& aDllPathC,
 *                                           uintptr_t& startAddress,
 *                                           uintptr_t& threadParam);
 *
 *                      aDllPath is a WCHAR-friendly path to InjectorDLL.dll.
 *                      Its memory will persist during the injection attempt.
 *
 *                      aDllPathC is the equivalent char-friendly path.
 *
 *                      startAddress and threadParam are passed into
 *                      CreateRemoteThread as arguments.
 */
template<typename TgetArgsProc>
static void
DoTest_CreateRemoteThread_LoadLibrary(TgetArgsProc aGetArgsProc)
{
  sThreadWasBlocked = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!sThreadWasBlocked) {
    EXPECT_TRUE(!"Unable to create sThreadWasBlocked event");
    ASSERT_EQ(GetLastError(), ERROR_SUCCESS);
  }

  sThreadWasAllowed = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!sThreadWasAllowed) {
    EXPECT_TRUE(!"Unable to create sThreadWasAllowed event");
    ASSERT_EQ(GetLastError(), ERROR_SUCCESS);
  }

  sDllWasLoaded = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!sDllWasLoaded) {
    EXPECT_TRUE(!"Unable to create sDllWasLoaded event");
    ASSERT_EQ(GetLastError(), ERROR_SUCCESS);
  }

  auto closeEvents = mozilla::MakeScopeExit([&](){
    CloseHandle(sThreadWasAllowed);
    CloseHandle(sThreadWasBlocked);
    CloseHandle(sDllWasLoaded);
  });

  // Hook into our DLL and thread blocking routines during this test.
  DllBlocklist_SetDllLoadHook(DllLoadHook);
  DllBlocklist_SetCreateThreadHook(CreateThreadHook);
  auto undoHooks = mozilla::MakeScopeExit([&](){
    DllBlocklist_SetDllLoadHook(nullptr);
    DllBlocklist_SetCreateThreadHook(nullptr);
  });

  // Launch Injector.exe.
  STARTUPINFOW si = { 0 };
  si.cb = sizeof(si);
  ::GetStartupInfoW(&si);
  PROCESS_INFORMATION pi = { 0 };

  nsString path(u"Injector.exe");
  nsString dllPath(DLL_LEAF_NAME);
  nsCString dllPathC = NS_ConvertUTF16toUTF8(dllPath);

  uintptr_t threadParam;
  aGetArgsProc(dllPath, dllPathC, sStartAddress, threadParam);

  path.AppendPrintf(" %lu 0x%p 0x%p", GetCurrentProcessId(), sStartAddress,
                    threadParam);
  if (::CreateProcessW(NULL, path.get(), 0, 0, FALSE, 0, NULL, NULL,
    &si, &pi) == FALSE) {
    EXPECT_TRUE(!"Error in CreateProcessW() launching Injector.exe");
    ASSERT_EQ(GetLastError(), ERROR_SUCCESS);
    return;
  }

  // Ensure Injector.exe doesn't stay running after this test finishes.
  auto cleanup = mozilla::MakeScopeExit([&](){
    CloseHandle(pi.hThread);
    EXPECT_TRUE("Shutting down.");
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
  });

  // Wait for information to come in and complete the test.
  HANDLE handles[] = {
    sThreadWasBlocked,
    sThreadWasAllowed,
    sDllWasLoaded,
    pi.hProcess
  };
  int handleCount = mozilla::ArrayLength(handles);
  bool keepGoing = true; // Set to false to signal that the test is over.

  while(keepGoing) {
    switch(WaitForMultipleObjectsEx(handleCount, handles,
                                    FALSE, sTimeoutMS, FALSE)) {
      case WAIT_OBJECT_0: { // sThreadWasBlocked
        EXPECT_TRUE("Thread was blocked successfully.");
        // No need to continue testing; blocking was successful.
        keepGoing = false;
        break;
      }
      case WAIT_OBJECT_0 + 1: { // sThreadWasAllowed
        EXPECT_TRUE(!"Thread was allowed but should have been blocked.");
        // No need to continue testing; blocking failed.
        keepGoing = false;
        break;
      }
      case WAIT_OBJECT_0 + 2: { // sDllWasLoaded
        EXPECT_TRUE(!"DLL was loaded.");
        // No need to continue testing; blocking failed and the DLL was
        // consequently loaded. In theory we should never see this fire, because
        // the thread being allowed should already trigger a test failure.
        keepGoing = false;
        break;
      }
      case WAIT_OBJECT_0 + 3: { // pi.hProcess
        // Check to see if we got an error code from Injector.exe, in which case
        // fail the test and exit.
        DWORD exitCode;
        if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
          EXPECT_TRUE(!"Injector.exe exited but we were unable to get the exit code.");
          keepGoing = false;
          break;
        }
        EXPECT_EQ(exitCode, 0);
        if (exitCode != 0) {
          EXPECT_TRUE(!"Injector.exe returned non-zero exit code");
          keepGoing = false;
          break;
        }
        // Process exited successfully. This can be ignored; we expect to get an
        // event whether the DLL was loaded or blocked.
        EXPECT_TRUE("Process exited as expected.");
        handleCount--;
        break;
      }
      case WAIT_TIMEOUT:
      default: {
        EXPECT_TRUE(!"An error or timeout occurred while waiting for activity "
                    "from Injector.exe");
        keepGoing = false;
        break;
      }
    }
  }

  // Double-check that injectordll is not loaded.
  auto hExisting = GetModuleHandleW(dllPath.get());
  EXPECT_TRUE(!hExisting);

  // If the DLL was erroneously loaded, attempt to unload it before exiting.
  if (hExisting) {
    FreeLibrary(hExisting);
  }

  return;
}

#if defined(NIGHTLY_BUILD)
TEST(TestInjectEject, CreateRemoteThread_LoadLibraryA)
{
  DoTest_CreateRemoteThread_LoadLibrary([](const nsString& dllPath,
                                           const nsCString& dllPathC,
                                           uintptr_t& aStartAddress,
                                           uintptr_t& aThreadParam){
    HMODULE hKernel32 = GetModuleHandleW(L"Kernel32");
    aStartAddress = (uintptr_t)GetProcAddress(hKernel32, "LoadLibraryA");
    aThreadParam = (uintptr_t)dllPathC.get();
  });
}

TEST(TestInjectEject, CreateRemoteThread_LoadLibraryW)
{
  DoTest_CreateRemoteThread_LoadLibrary([](const nsString& dllPath,
                                           const nsCString& dllPathC,
                                           uintptr_t& aStartAddress,
                                           uintptr_t& aThreadParam){
    HMODULE hKernel32 = GetModuleHandleW(L"Kernel32");
    aStartAddress = (uintptr_t)GetProcAddress(hKernel32, "LoadLibraryW");
    aThreadParam = (uintptr_t)dllPath.get();
  });
}

TEST(TestInjectEject, CreateRemoteThread_LoadLibraryExW)
{
  DoTest_CreateRemoteThread_LoadLibrary([](const nsString& dllPath,
                                           const nsCString& dllPathC,
                                           uintptr_t& aStartAddress,
                                           uintptr_t& aThreadParam){
    HMODULE hKernel32 = GetModuleHandleW(L"Kernel32");
    // LoadLibraryEx requires three arguments so this injection method may not
    // be viable. It's certainly not an allowable thread start in any case.
    aStartAddress = (uintptr_t)GetProcAddress(hKernel32, "LoadLibraryExW");
    aThreadParam = (uintptr_t)dllPath.get();
  });
}

TEST(TestInjectEject, CreateRemoteThread_LoadLibraryExA)
{
  DoTest_CreateRemoteThread_LoadLibrary([](const nsString& dllPath,
                                           const nsCString& dllPathC,
                                           uintptr_t& aStartAddress,
                                           uintptr_t& aThreadParam){
    HMODULE hKernel32 = GetModuleHandleW(L"Kernel32");
    aStartAddress = (uintptr_t)GetProcAddress(hKernel32, "LoadLibraryExA");
    aThreadParam = (uintptr_t)dllPathC.get();
  });
}
#endif //  defined(NIGHTLY_BUILD)
