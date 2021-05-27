/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TestShellEx_RegUtils_h
#define mozilla_TestShellEx_RegUtils_h

#include <string>

class RegKey final {
  HKEY mKey;

  bool SetStringInternal(const wchar_t* aValueName, const wchar_t* aValueData,
                         DWORD aValueDataLength);

 public:
  RegKey() : mKey(nullptr) {}
  RegKey(HKEY root, const wchar_t* aSubkey);
  ~RegKey();

  RegKey(RegKey&& aOther) = delete;
  RegKey& operator=(RegKey&& aOther) = delete;
  RegKey(const RegKey&) = delete;
  RegKey& operator=(const RegKey&) = delete;

  explicit operator bool() const { return !!mKey; }
  operator HKEY() const { return mKey; }

  bool SetString(const wchar_t* aValueName,
                 const wchar_t* aValueData = nullptr);
  bool SetString(const wchar_t* aValueName, const std::wstring& aValueData);
  std::wstring GetString(const wchar_t* aValueName);
};

class ComRegisterer final {
  RegKey mClassRoot;
  std::wstring mClsId;
  std::wstring mFriendlyName;

 public:
  ComRegisterer(const GUID& aClsId, const wchar_t* aFriendlyName);
  ~ComRegisterer() = default;

  bool UnregisterAll();
  bool RegisterObject(const wchar_t* aThreadModel);
  bool RegisterExtensions();
};

#endif  // mozilla_TestShellEx_RegUtils_h
