/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MsiDatabase.h"

#ifdef UNICODE
#  define MSIDBOPEN_READONLY_W MSIDBOPEN_READONLY
#  define INSTALLPROPERTY_LOCALPACKAGE_W INSTALLPROPERTY_LOCALPACKAGE
#else
// MSIDBOPEN_READONLY is defined as `(LPCTSTR)0` in msiquery.h, so we need to
// cast it to LPCWSTR.
#  define MSIDBOPEN_READONLY_W reinterpret_cast<LPCWSTR>(MSIDBOPEN_READONLY)
// INSTALLPROPERTY_LOCALPACKAGE is defined as `__TEXT("LocalPackage")` in msi.h,
// so we need to define a wchar_t version.
#  define INSTALLPROPERTY_LOCALPACKAGE_W L"LocalPackage"
#endif  // UNICODE

namespace mozilla {

/*static*/
UniquePtr<wchar_t[]> MsiDatabase::GetRecordString(MSIHANDLE aRecord,
                                                  UINT aFieldIndex) {
  // The 3rd parameter of MsiRecordGetStringW must not be nullptr.
  wchar_t kEmptyString[] = L"";
  DWORD len = 0;
  UINT ret = ::MsiRecordGetStringW(aRecord, aFieldIndex, kEmptyString, &len);
  if (ret != ERROR_MORE_DATA) {
    return nullptr;
  }

  // |len| returned from MsiRecordGetStringW does not include
  // a null-character, but a length to pass to MsiRecordGetStringW
  // needs to include a null-character.
  ++len;

  auto buf = MakeUnique<wchar_t[]>(len);
  ret = ::MsiRecordGetStringW(aRecord, aFieldIndex, buf.get(), &len);
  if (ret != ERROR_SUCCESS) {
    return nullptr;
  }

  return buf;
}

MsiDatabase::MsiDatabase(const wchar_t* aDatabasePath) {
  MSIHANDLE handle = 0;
  UINT ret = ::MsiOpenDatabaseW(aDatabasePath, MSIDBOPEN_READONLY_W, &handle);
  if (ret != ERROR_SUCCESS) {
    return;
  }

  mDatabase.own(handle);
}

Maybe<MsiDatabase> MsiDatabase::FromProductId(const wchar_t* aProductId) {
  DWORD len = MAX_PATH;
  wchar_t bufStack[MAX_PATH];
  UINT ret = ::MsiGetProductInfoW(aProductId, INSTALLPROPERTY_LOCALPACKAGE_W,
                                  bufStack, &len);
  if (ret == ERROR_SUCCESS) {
    return Some(MsiDatabase(bufStack));
  }

  if (ret != ERROR_MORE_DATA) {
    return Nothing();
  }

  // |len| returned from MsiGetProductInfoW does not include
  // a null-character, but a length to pass to MsiGetProductInfoW
  // needs to include a null-character.
  ++len;

  std::unique_ptr<wchar_t[]> bufHeap(new wchar_t[len]);
  ret = ::MsiGetProductInfoW(aProductId, INSTALLPROPERTY_LOCALPACKAGE_W,
                             bufHeap.get(), &len);
  if (ret == ERROR_SUCCESS) {
    return Some(MsiDatabase(bufHeap.get()));
  }

  return Nothing();
}

MsiDatabase::operator bool() const { return !!mDatabase; }

}  // namespace mozilla
