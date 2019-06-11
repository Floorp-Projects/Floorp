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

TEST(TestDllBlocklist, BlockDllByName) {
  // The DLL name has capital letters, so this also tests that the comparison
  // is case-insensitive.
  NS_NAMED_LITERAL_STRING(kLeafName, "TestDllBlocklist_MatchByName.dll");
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!hDll);
  EXPECT_TRUE(!::GetModuleHandleW(kLeafName.get()));
}

TEST(TestDllBlocklist, BlockDllByVersion) {
  NS_NAMED_LITERAL_STRING(kLeafName, "TestDllBlocklist_MatchByVersion.dll");
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!hDll);
  EXPECT_TRUE(!::GetModuleHandleW(kLeafName.get()));
}

TEST(TestDllBlocklist, AllowDllByVersion) {
  NS_NAMED_LITERAL_STRING(kLeafName, "TestDllBlocklist_AllowByVersion.dll");
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!!hDll);
  EXPECT_TRUE(!!::GetModuleHandleW(kLeafName.get()));
}

#define DLL_BLOCKLIST_ENTRY(name, ...) {name, __VA_ARGS__},
#define DLL_BLOCKLIST_STRING_TYPE const char*
#include "mozilla/WindowsDllBlocklistDefs.h"

TEST(TestDllBlocklist, BlocklistIntegrity) {
  nsTArray<DLL_BLOCKLIST_STRING_TYPE> dupes;
  DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY(pFirst);
  DECLARE_POINTER_TO_LAST_DLL_BLOCKLIST_ENTRY(pLast);

  EXPECT_FALSE(pLast->name || pLast->maxVersion || pLast->flags);

  for (size_t i = 0; i < mozilla::ArrayLength(gWindowsDllBlocklist) - 1; ++i) {
    auto pEntry = pFirst + i;

    // Validate name
    EXPECT_TRUE(!!pEntry->name);
    EXPECT_GT(strlen(pEntry->name), 3);

    // Check the filename for valid characters.
    for (auto pch = pEntry->name; *pch != 0; ++pch) {
      EXPECT_FALSE(*pch >= 'A' && *pch <= 'Z');
    }

    // Check for duplicate entries
    for (auto&& dupe : dupes) {
      EXPECT_NE(stricmp(dupe, pEntry->name), 0);
    }

    dupes.AppendElement(pEntry->name);
  }
}

TEST(TestDllBlocklist, BlockThreadWithLoadLibraryEntryPoint) {
  // Only supported on Nightly
#if defined(NIGHTLY_BUILD)
  using ThreadProc = unsigned(__stdcall*)(void*);

  NS_NAMED_LITERAL_STRING(kLeafNameW, "TestDllBlocklist_MatchByVersion.dll");

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

