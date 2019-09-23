/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ModuleVersionInfo.h"

#include "mozilla/UniquePtr.h"

namespace mozilla {

/**
 * Gets a string value from a version info block with the specified translation
 * and field name.
 *
 * @param  aBlock       [in] The binary version resource block
 * @param  aTranslation [in] The translation ID as obtained in the version
 *                      translation list.
 * @param  aFieldName   [in] Null-terminated name of the desired field
 * @param  aResult      [out] Receives the string value, if successful.
 * @return true if successful. aResult is unchanged upon failure.
 */
static bool QueryStringValue(const void* aBlock, DWORD aTranslation,
                             const wchar_t* aFieldName, nsAString& aResult) {
  nsAutoString path;
  path.AppendPrintf("\\StringFileInfo\\%02X%02X%02X%02X\\%S",
                    (aTranslation & 0x0000ff00) >> 8,
                    (aTranslation & 0x000000ff),
                    (aTranslation & 0xff000000) >> 24,
                    (aTranslation & 0x00ff0000) >> 16, aFieldName);

  wchar_t* lpBuffer = nullptr;
  UINT len = 0;
  if (!::VerQueryValueW(aBlock, path.get(), (PVOID*)&lpBuffer, &len)) {
    return false;
  }
  aResult.Assign(lpBuffer, (size_t)len - 1);
  return true;
}

/**
 * Searches through translations in the version resource for the requested
 * field. English(US) is preferred, otherwise we take the first translation
 * that succeeds.
 *
 * @param  aBlock        [in] The binary version resource block
 * @param  aTranslations [in] The list of translation IDs available
 * @param  aNumTrans     [in] Number of items in aTranslations
 * @param  aFieldName    [in] Null-terminated name of the desired field
 * @param  aResult       [in] Receives the string value, if successful.
 * @return true if successful. aResult is unchanged upon failure.
 */
static bool QueryStringValue(const void* aBlock, const DWORD* aTranslations,
                             size_t aNumTrans, const wchar_t* aFieldName,
                             nsAString& aResult) {
  static const DWORD kPreferredTranslation =
      0x04b00409;  // English (US), Unicode
  if (QueryStringValue(aBlock, kPreferredTranslation, aFieldName, aResult)) {
    return true;
  }
  for (size_t i = 0; i < aNumTrans; ++i) {
    if (QueryStringValue(aBlock, aTranslations[i], aFieldName, aResult)) {
      return true;
    }
  }
  return false;
}

bool ModuleVersionInfo::GetFromImage(const nsAString& aPath) {
  nsString path(aPath);
  DWORD infoSize = GetFileVersionInfoSizeW(path.get(), nullptr);
  if (!infoSize) {
    return false;
  }

  auto verInfo = MakeUnique<BYTE[]>(infoSize);
  if (!::GetFileVersionInfoW(path.get(), 0, infoSize, verInfo.get())) {
    return false;
  }

  VS_FIXEDFILEINFO* vInfo = nullptr;
  UINT vInfoLen = 0;
  if (::VerQueryValueW(verInfo.get(), L"\\", (LPVOID*)&vInfo, &vInfoLen)) {
    mFileVersion =
        VersionNumber(vInfo->dwFileVersionMS, vInfo->dwFileVersionLS);
    mProductVersion =
        VersionNumber(vInfo->dwProductVersionMS, vInfo->dwProductVersionLS);
  }

  // Note that regardless the character set indicated, strings are always
  // returned as Unicode by the Windows APIs.
  DWORD* pTrans = nullptr;
  UINT cbTrans = 0;
  if (::VerQueryValueW(verInfo.get(), L"\\VarFileInfo\\Translation",
                       (PVOID*)&pTrans, &cbTrans)) {
    size_t numTrans = cbTrans / sizeof(DWORD);
    QueryStringValue(verInfo.get(), pTrans, numTrans, L"CompanyName",
                     mCompanyName);
    QueryStringValue(verInfo.get(), pTrans, numTrans, L"ProductName",
                     mProductName);
    QueryStringValue(verInfo.get(), pTrans, numTrans, L"LegalCopyright",
                     mLegalCopyright);
    QueryStringValue(verInfo.get(), pTrans, numTrans, L"FileDescription",
                     mFileDescription);
  }

  return true;
}

}  // namespace mozilla
