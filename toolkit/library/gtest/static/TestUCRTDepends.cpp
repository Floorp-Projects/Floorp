/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Unused.h"
#include "mozilla/WindowsVersion.h"
#include "nsDependentString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsWindowsHelpers.h"

using namespace mozilla;

constexpr const WCHAR pattern[] = L"\\api-*.dll";

static LPWSTR GetModuleFileDir(HMODULE module, LPWSTR path, DWORD size) {
  DWORD chars = GetModuleFileNameW(module, path, size);
  if (chars <= 0 || chars >= MAX_PATH) {
    return nullptr;
  }

  // Split the base name from the directory.
  LPWSTR basename = wcsrchr(path, L'\\');
  if (!basename) {
    return nullptr;  // at least one path separator must be present
  }
  *basename++ = L'\0';
  return basename;
}

// Make sure that Universal CRT forwarder DLLs are not in app directory if it
// is in Api Sets.
TEST(TestUCRTDepends, AppDir)
{
  WCHAR appdir[MAX_PATH];
  ASSERT_TRUE(GetModuleFileDir(nullptr, appdir, MAX_PATH));

  WCHAR path[MAX_PATH + ArrayLength(pattern)];
  swprintf(path, L"%s%s", appdir, pattern);

  WIN32_FIND_DATAW wfd;
  HANDLE hFind = FindFirstFileW(path, &wfd);
#if defined(_M_ARM64)  // We do not ship Universal CRT DLLs on aarch64.
  if (hFind == INVALID_HANDLE_VALUE) {
    EXPECT_EQ(GetLastError(), ERROR_FILE_NOT_FOUND);
    return;
  }
#else
  ASSERT_NE(hFind, INVALID_HANDLE_VALUE);
#endif
  do {
    nsModuleHandle module(LoadLibraryW(wfd.cFileName));
    EXPECT_TRUE(module);
    if (!module) {
      continue;
    }

    // Get a full path of the loaded module.
    LPWSTR basename = GetModuleFileDir(module, path, MAX_PATH);
    ASSERT_TRUE(basename);

    // If the module is in Api Sets, GetModuleFileName returns the redirected
    // DLL path, so filenames will not match.
    bool inApiSets = wcsicmp(wfd.cFileName, basename);
    if (IsWin10OrLater()) {
      // All files must be in Api Sets on Windows 10.
      EXPECT_TRUE(inApiSets);
      continue;
    }
    if (IsWin8OrLater()) {
      if (inApiSets) {
        continue;  // This file is in Api Sets, OK.
      }
      // Universal CRT files are not in Api Sets on Windows 8.
      EXPECT_TRUE(StringBeginsWith(nsDependentString(wfd.cFileName),
                                   NS_LITERAL_STRING("api-ms-win-crt-"),
                                   nsCaseInsensitiveStringComparator()));
    } else {  // Windows 7
      // All files must not be in Api Sets on Windows 7.
      EXPECT_FALSE(inApiSets);
    }
    // Files must be loaded from appdir
    EXPECT_TRUE(!wcsicmp(path, appdir));
  } while (FindNextFileW(hFind, &wfd));
  EXPECT_EQ(GetLastError(), ERROR_NO_MORE_FILES);
  BOOL ret = FindClose(hFind);
  EXPECT_TRUE(ret);
}

// Make sure that we do not depend on Universal CRT forwarder DLLs in the
// system directory.
TEST(TestUCRTDepends, SystemDir)
{
  WCHAR appdir[MAX_PATH];
  ASSERT_TRUE(GetModuleFileDir(nullptr, appdir, MAX_PATH));

  WCHAR path[MAX_PATH + ArrayLength(pattern)];
  UINT chars = GetSystemDirectoryW(path, MAX_PATH);
  ASSERT_TRUE(chars > 0 && chars < MAX_PATH);
  wcscat(path, pattern);

  WIN32_FIND_DATAW wfd;
  HANDLE hFind = FindFirstFileW(path, &wfd);
  if (hFind == INVALID_HANDLE_VALUE) {
    EXPECT_EQ(GetLastError(), ERROR_FILE_NOT_FOUND);
    EXPECT_TRUE(IsWin8OrLater());
    return;  // Not found in the system directory, OK.
  }
  // Api Sets forwarders must not be present on Windows 10.
  EXPECT_FALSE(IsWin10OrLater());
  do {
    HMODULE module = GetModuleHandleW(wfd.cFileName);
    if (!module) {
      continue;  // We are not using this file, OK.
    }

    // Get a full path of the loaded module.
    LPWSTR basename = GetModuleFileDir(module, path, MAX_PATH);
    ASSERT_TRUE(basename);

    // If the module is in Api Sets, GetModuleFileName returns the redirected
    // DLL path, so filenames will not match.
    if (wcsicmp(wfd.cFileName, basename)) {
      // If this file is in Api Sets, it must not be present in appdir.
      swprintf(path, L"%s\\%s", appdir, wfd.cFileName);
      EXPECT_EQ(GetFileAttributesW(path), INVALID_FILE_ATTRIBUTES);
    } else {
      // If this file is not in Api Sets, it must be loaded from appdir.
      EXPECT_TRUE(!wcsicmp(path, appdir));
    }
  } while (FindNextFileW(hFind, &wfd));
  EXPECT_EQ(GetLastError(), ERROR_NO_MORE_FILES);
  BOOL ret = FindClose(hFind);
  EXPECT_TRUE(ret);
}
