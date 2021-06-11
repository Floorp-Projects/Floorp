/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <winternl.h>

#include <process.h>

#include "gtest/gtest.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Char16.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWindowsHelpers.h"

static nsString GetFullPath(const nsAString& aLeaf) {
  nsCOMPtr<nsIFile> f;

  EXPECT_TRUE(NS_SUCCEEDED(
      NS_GetSpecialDirectory(NS_OS_CURRENT_WORKING_DIR, getter_AddRefs(f))));

  EXPECT_TRUE(NS_SUCCEEDED(f->Append(aLeaf)));

  bool exists;
  EXPECT_TRUE(NS_SUCCEEDED(f->Exists(&exists)) && exists);

  nsString ret;
  EXPECT_TRUE(NS_SUCCEEDED(f->GetPath(ret)));
  return ret;
}

TEST(TestDllBlocklist, BlockDllByName)
{
  // The DLL name has capital letters, so this also tests that the comparison
  // is case-insensitive.
  constexpr auto kLeafName = u"TestDllBlocklist_MatchByName.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!hDll);
  EXPECT_TRUE(!::GetModuleHandleW(kLeafName.get()));

  hDll.own(::LoadLibraryExW(dllPath.get(), nullptr, LOAD_LIBRARY_AS_DATAFILE));
  // Mapped as MEM_MAPPED + PAGE_READONLY
  EXPECT_TRUE(hDll);
}

TEST(TestDllBlocklist, BlockDllByVersion)
{
  constexpr auto kLeafName = u"TestDllBlocklist_MatchByVersion.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!hDll);
  EXPECT_TRUE(!::GetModuleHandleW(kLeafName.get()));

  hDll.own(
      ::LoadLibraryExW(dllPath.get(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE));
  // Mapped as MEM_IMAGE + PAGE_READONLY
  EXPECT_TRUE(hDll);
}

TEST(TestDllBlocklist, AllowDllByVersion)
{
  constexpr auto kLeafName = u"TestDllBlocklist_AllowByVersion.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!!hDll);
  EXPECT_TRUE(!!::GetModuleHandleW(kLeafName.get()));
}

// RedirectToNoOpEntryPoint needs the launcher process.
#if defined(MOZ_LAUNCHER_PROCESS)
TEST(TestDllBlocklist, NoOpEntryPoint)
{
  // DllMain of this dll has MOZ_RELEASE_ASSERT.  This test makes sure we load
  // the module successfully without running DllMain.
  constexpr auto kLeafName = u"TestDllBlocklist_NoOpEntryPoint.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

#  if defined(MOZ_ASAN)
  // With ASAN, the test uses mozglue's blocklist where
  // REDIRECT_TO_NOOP_ENTRYPOINT is ignored.  So LoadLibraryW
  // is expected to fail.
  EXPECT_TRUE(!hDll);
  EXPECT_TRUE(!::GetModuleHandleW(kLeafName.get()));
#  else
  EXPECT_TRUE(!!hDll);
  EXPECT_TRUE(!!::GetModuleHandleW(kLeafName.get()));
#  endif
}
#endif  // defined(MOZ_LAUNCHER_PROCESS)

#define DLL_BLOCKLIST_ENTRY(name, ...) {name, __VA_ARGS__},
#define DLL_BLOCKLIST_STRING_TYPE const char*
#include "mozilla/WindowsDllBlocklistLegacyDefs.h"

TEST(TestDllBlocklist, BlocklistIntegrity)
{
  nsTArray<DLL_BLOCKLIST_STRING_TYPE> dupes;
  DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY(pFirst);
  DECLARE_POINTER_TO_LAST_DLL_BLOCKLIST_ENTRY(pLast);

  EXPECT_FALSE(pLast->mName || pLast->mMaxVersion || pLast->mFlags);

  for (size_t i = 0; i < mozilla::ArrayLength(gWindowsDllBlocklist) - 1; ++i) {
    auto pEntry = pFirst + i;

    // Validate name
    EXPECT_TRUE(!!pEntry->mName);
    EXPECT_GT(strlen(pEntry->mName), 3);

    // Check the filename for valid characters.
    for (auto pch = pEntry->mName; *pch != 0; ++pch) {
      EXPECT_FALSE(*pch >= 'A' && *pch <= 'Z');
    }

    // Check for duplicate entries
    for (auto&& dupe : dupes) {
      EXPECT_NE(stricmp(dupe, pEntry->mName), 0);
    }

    dupes.AppendElement(pEntry->mName);
  }
}

TEST(TestDllBlocklist, BlockThreadWithLoadLibraryEntryPoint)
{
  // Only supported on Nightly
#if defined(NIGHTLY_BUILD)
  using ThreadProc = unsigned(__stdcall*)(void*);

  constexpr auto kLeafNameW = u"TestDllBlocklist_MatchByVersion.dll"_ns;

  nsString fullPathW = GetFullPath(kLeafNameW);
  EXPECT_FALSE(fullPathW.IsEmpty());

  nsAutoHandle threadW(reinterpret_cast<HANDLE>(
      _beginthreadex(nullptr, 0, reinterpret_cast<ThreadProc>(&::LoadLibraryW),
                     (void*)fullPathW.get(), 0, nullptr)));

  EXPECT_TRUE(!!threadW);
  EXPECT_EQ(::WaitForSingleObject(threadW, INFINITE), WAIT_OBJECT_0);

  DWORD exitCode;
  EXPECT_TRUE(::GetExitCodeThread(threadW, &exitCode) && !exitCode);
  EXPECT_TRUE(!::GetModuleHandleW(kLeafNameW.get()));

  const NS_LossyConvertUTF16toASCII fullPathA(fullPathW);
  EXPECT_FALSE(fullPathA.IsEmpty());

  nsAutoHandle threadA(reinterpret_cast<HANDLE>(
      _beginthreadex(nullptr, 0, reinterpret_cast<ThreadProc>(&::LoadLibraryA),
                     (void*)fullPathA.get(), 0, nullptr)));

  EXPECT_TRUE(!!threadA);
  EXPECT_EQ(::WaitForSingleObject(threadA, INFINITE), WAIT_OBJECT_0);
  EXPECT_TRUE(::GetExitCodeThread(threadA, &exitCode) && !exitCode);
  EXPECT_TRUE(!::GetModuleHandleW(kLeafNameW.get()));
#endif  // defined(NIGHTLY_BUILD)
}
