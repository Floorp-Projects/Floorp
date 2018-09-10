/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <Windows.h>
#include <shlwapi.h>
#include <winternl.h>
#include "gtest/gtest.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/WindowsDllBlocklist.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsLiteralString.h"
#include "nsUnicharUtils.h"
#include "mozilla/Char16.h"

static bool sDllWasBlocked = false;
static bool sDllWasLoaded = false;
static const char16_t* sDllName;

static nsDependentSubstring
MakeString(PUNICODE_STRING aOther)
{
  size_t numChars = aOther->Length / sizeof(WCHAR);
  return nsDependentSubstring((const char16_t *)aOther->Buffer, numChars);
}

static void
DllLoadHook(bool aDllLoaded, NTSTATUS aStatus, HANDLE aDllBase,
            PUNICODE_STRING aDllName)
{
  nsDependentSubstring str = MakeString(aDllName);
  if (StringEndsWith(str, nsDependentString(sDllName),
                     nsCaseInsensitiveStringComparator())) {
    if (aDllLoaded) {
      sDllWasLoaded = true;
    } else {
      sDllWasBlocked = true;
    }
  }
}

static nsString
GetFullPath(const char16_t* leaf)
{
  nsCOMPtr<nsIFile> f;
  EXPECT_TRUE(NS_SUCCEEDED(NS_GetSpecialDirectory(NS_OS_CURRENT_WORKING_DIR, getter_AddRefs(f))));
  EXPECT_TRUE(NS_SUCCEEDED(f->Append(nsDependentString(leaf))));

  bool exists;
  EXPECT_TRUE(NS_SUCCEEDED(f->Exists(&exists)));
  EXPECT_TRUE(exists);

  nsString ret;
  EXPECT_TRUE(NS_SUCCEEDED(f->GetPath(ret)));
  return ret;
}

TEST(TestDllBlocklist, BlockDllByName)
{
  sDllWasBlocked = false;
  sDllWasLoaded = false;
  DllBlocklist_SetDllLoadHook(DllLoadHook);
  auto undoHooks = mozilla::MakeScopeExit([&](){
    DllBlocklist_SetDllLoadHook(nullptr);
  });

  // The DLL name has capital letters, so this also tests that the comparison
  // is case-insensitive.
  sDllName = u"TestDllBlocklist_MatchByName.dll";
  nsString dllPath = GetFullPath(sDllName);

  auto hDll = ::LoadLibraryW((char16ptr_t)dllPath.get());
  if (hDll) {
    EXPECT_TRUE(!"DLL was loaded but should have been blocked.");
  }

  EXPECT_FALSE(sDllWasLoaded);
  EXPECT_TRUE(sDllWasBlocked);

  if (hDll) {
    FreeLibrary(hDll);
  }
}

TEST(TestDllBlocklist, BlockDllByVersion)
{
  sDllWasBlocked = false;
  sDllWasLoaded = false;
  DllBlocklist_SetDllLoadHook(DllLoadHook);
  auto undoHooks = mozilla::MakeScopeExit([&](){
    DllBlocklist_SetDllLoadHook(nullptr);
  });

  sDllName = u"TestDllBlocklist_MatchByVersion.dll";
  nsString dllPath = GetFullPath(sDllName);

  auto hDll = ::LoadLibraryW((char16ptr_t)dllPath.get());
  if (hDll) {
    EXPECT_TRUE(!"DLL was loaded but should have been blocked.");
  }

  EXPECT_FALSE(sDllWasLoaded);
  EXPECT_TRUE(sDllWasBlocked);

  if (hDll) {
    FreeLibrary(hDll);
  }
}

TEST(TestDllBlocklist, AllowDllByVersion)
{
  sDllWasBlocked = false;
  sDllWasLoaded = false;
  DllBlocklist_SetDllLoadHook(DllLoadHook);
  auto undoHooks = mozilla::MakeScopeExit([&](){
    DllBlocklist_SetDllLoadHook(nullptr);
  });

  sDllName = u"TestDllBlocklist_AllowByVersion.dll";
  nsString dllPath = GetFullPath(sDllName);

  auto hDll = ::LoadLibraryW((char16ptr_t)dllPath.get());
  if (!hDll) {
    EXPECT_TRUE(!"DLL was blocked but should have been loaded.");
  }

  EXPECT_TRUE(sDllWasLoaded);
  EXPECT_FALSE(sDllWasBlocked);

  if (hDll) {
    FreeLibrary(hDll);
  }
}

TEST(TestDllBlocklist, BlocklistIntegrity)
{
  auto msg = DllBlocklist_TestBlocklistIntegrity();
  EXPECT_FALSE(msg) << msg;
}
