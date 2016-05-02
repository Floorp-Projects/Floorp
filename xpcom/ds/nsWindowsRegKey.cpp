/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <shlwapi.h>
#include <stdlib.h>
#include "nsWindowsRegKey.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"

//-----------------------------------------------------------------------------

// According to MSDN, the following limits apply (in characters excluding room
// for terminating null character):
#define MAX_KEY_NAME_LEN     255
#define MAX_VALUE_NAME_LEN   16383

class nsWindowsRegKey final : public nsIWindowsRegKey
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINDOWSREGKEY

  nsWindowsRegKey()
    : mKey(nullptr)
    , mWatchEvent(nullptr)
    , mWatchRecursive(FALSE)
  {
  }

private:
  ~nsWindowsRegKey()
  {
    Close();
  }

  HKEY   mKey;
  HANDLE mWatchEvent;
  BOOL   mWatchRecursive;
};

NS_IMPL_ISUPPORTS(nsWindowsRegKey, nsIWindowsRegKey)

NS_IMETHODIMP
nsWindowsRegKey::GetKey(HKEY* aKey)
{
  *aKey = mKey;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::SetKey(HKEY aKey)
{
  // We do not close the older aKey!
  StopWatching();

  mKey = aKey;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::Close()
{
  StopWatching();

  if (mKey) {
    RegCloseKey(mKey);
    mKey = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::Open(uint32_t aRootKey, const nsAString& aPath,
                      uint32_t aMode)
{
  Close();

  LONG rv = RegOpenKeyExW((HKEY)(intptr_t)aRootKey,
                          PromiseFlatString(aPath).get(), 0, (REGSAM)aMode,
                          &mKey);
  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::Create(uint32_t aRootKey, const nsAString& aPath,
                        uint32_t aMode)
{
  Close();

  DWORD disposition;
  LONG rv = RegCreateKeyExW((HKEY)(intptr_t)aRootKey,
                            PromiseFlatString(aPath).get(), 0, nullptr,
                            REG_OPTION_NON_VOLATILE, (REGSAM)aMode, nullptr,
                            &mKey, &disposition);
  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::OpenChild(const nsAString& aPath, uint32_t aMode,
                           nsIWindowsRegKey** aResult)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<nsIWindowsRegKey> child = new nsWindowsRegKey();

  nsresult rv = child->Open((uintptr_t)mKey, aPath, aMode);
  if (NS_FAILED(rv)) {
    return rv;
  }

  child.swap(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::CreateChild(const nsAString& aPath, uint32_t aMode,
                             nsIWindowsRegKey** aResult)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<nsIWindowsRegKey> child = new nsWindowsRegKey();

  nsresult rv = child->Create((uintptr_t)mKey, aPath, aMode);
  if (NS_FAILED(rv)) {
    return rv;
  }

  child.swap(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetChildCount(uint32_t* aResult)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  DWORD numSubKeys;
  LONG rv = RegQueryInfoKeyW(mKey, nullptr, nullptr, nullptr, &numSubKeys,
                             nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr);
  if (rv != ERROR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  *aResult = numSubKeys;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetChildName(uint32_t aIndex, nsAString& aResult)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  FILETIME lastWritten;

  wchar_t nameBuf[MAX_KEY_NAME_LEN + 1];
  DWORD nameLen = sizeof(nameBuf) / sizeof(nameBuf[0]);

  LONG rv = RegEnumKeyExW(mKey, aIndex, nameBuf, &nameLen, nullptr, nullptr,
                          nullptr, &lastWritten);
  if (rv != ERROR_SUCCESS) {
    return NS_ERROR_NOT_AVAILABLE;  // XXX what's the best error code here?
  }

  aResult.Assign(nameBuf, nameLen);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::HasChild(const nsAString& aName, bool* aResult)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Check for the existence of a child key by opening the key with minimal
  // rights.  Perhaps there is a more efficient way to do this?

  HKEY key;
  LONG rv = RegOpenKeyExW(mKey, PromiseFlatString(aName).get(), 0,
                          STANDARD_RIGHTS_READ, &key);

  if ((*aResult = (rv == ERROR_SUCCESS && key))) {
    RegCloseKey(key);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetValueCount(uint32_t* aResult)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  DWORD numValues;
  LONG rv = RegQueryInfoKeyW(mKey, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr, &numValues, nullptr, nullptr,
                             nullptr, nullptr);
  if (rv != ERROR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  *aResult = numValues;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetValueName(uint32_t aIndex, nsAString& aResult)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  wchar_t nameBuf[MAX_VALUE_NAME_LEN];
  DWORD nameLen = sizeof(nameBuf) / sizeof(nameBuf[0]);

  LONG rv = RegEnumValueW(mKey, aIndex, nameBuf, &nameLen, nullptr, nullptr,
                          nullptr, nullptr);
  if (rv != ERROR_SUCCESS) {
    return NS_ERROR_NOT_AVAILABLE;  // XXX what's the best error code here?
  }

  aResult.Assign(nameBuf, nameLen);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::HasValue(const nsAString& aName, bool* aResult)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(aName).get(), 0, nullptr,
                             nullptr, nullptr);

  *aResult = (rv == ERROR_SUCCESS);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::RemoveChild(const nsAString& aName)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  LONG rv = RegDeleteKeyW(mKey, PromiseFlatString(aName).get());

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::RemoveValue(const nsAString& aName)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  LONG rv = RegDeleteValueW(mKey, PromiseFlatString(aName).get());

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::GetValueType(const nsAString& aName, uint32_t* aResult)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(aName).get(), 0,
                             (LPDWORD)aResult, nullptr, nullptr);
  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadStringValue(const nsAString& aName, nsAString& aResult)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  DWORD type, size;

  const nsString& flatName = PromiseFlatString(aName);

  LONG rv = RegQueryValueExW(mKey, flatName.get(), 0, &type, nullptr, &size);
  if (rv != ERROR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  // This must be a string type in order to fetch the value as a string.
  // We're being a bit forgiving here by allowing types other than REG_SZ.
  if (type != REG_SZ && type != REG_EXPAND_SZ && type != REG_MULTI_SZ) {
    return NS_ERROR_FAILURE;
  }

  // The buffer size must be a multiple of 2.
  if (size % 2 != 0) {
    return NS_ERROR_UNEXPECTED;
  }

  if (size == 0) {
    aResult.Truncate();
    return NS_OK;
  }

  // |size| may or may not include the terminating null character.
  DWORD resultLen = size / 2;

  aResult.SetLength(resultLen);
  nsAString::iterator begin;
  aResult.BeginWriting(begin);
  if (begin.size_forward() != resultLen) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = RegQueryValueExW(mKey, flatName.get(), 0, &type, (LPBYTE)begin.get(),
                        &size);

  if (!aResult.CharAt(resultLen - 1)) {
    // The string passed to us had a null terminator in the final position.
    aResult.Truncate(resultLen - 1);
  }

  // Expand the environment variables if needed
  if (type == REG_EXPAND_SZ) {
    const nsString& flatSource = PromiseFlatString(aResult);
    resultLen = ExpandEnvironmentStringsW(flatSource.get(), nullptr, 0);
    if (resultLen > 1) {
      nsAutoString expandedResult;
      // |resultLen| includes the terminating null character
      --resultLen;
      expandedResult.SetLength(resultLen);
      nsAString::iterator begin;
      expandedResult.BeginWriting(begin);
      if (begin.size_forward() != resultLen) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      resultLen = ExpandEnvironmentStringsW(flatSource.get(),
                                            wwc(begin.get()),
                                            resultLen + 1);
      if (resultLen <= 0) {
        rv = ERROR_UNKNOWN_FEATURE;
        aResult.Truncate();
      } else {
        rv = ERROR_SUCCESS;
        aResult = expandedResult;
      }
    } else if (resultLen == 1) {
      // It apparently expands to nothing (just a null terminator).
      aResult.Truncate();
    }
  }

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadIntValue(const nsAString& aName, uint32_t* aResult)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  DWORD size = sizeof(*aResult);
  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(aName).get(), 0, nullptr,
                             (LPBYTE)aResult, &size);
  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadInt64Value(const nsAString& aName, uint64_t* aResult)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  DWORD size = sizeof(*aResult);
  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(aName).get(), 0, nullptr,
                             (LPBYTE)aResult, &size);
  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadBinaryValue(const nsAString& aName, nsACString& aResult)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  DWORD size;
  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(aName).get(), 0,
                             nullptr, nullptr, &size);

  if (rv != ERROR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  if (!size) {
    aResult.Truncate();
    return NS_OK;
  }

  aResult.SetLength(size);
  nsACString::iterator begin;
  aResult.BeginWriting(begin);
  if (begin.size_forward() != size) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = RegQueryValueExW(mKey, PromiseFlatString(aName).get(), 0, nullptr,
                        (LPBYTE)begin.get(), &size);
  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteStringValue(const nsAString& aName,
                                  const nsAString& aValue)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Need to indicate complete size of buffer including null terminator.
  const nsString& flatValue = PromiseFlatString(aValue);

  LONG rv = RegSetValueExW(mKey, PromiseFlatString(aName).get(), 0, REG_SZ,
                           (const BYTE*)flatValue.get(),
                           (flatValue.Length() + 1) * sizeof(char16_t));
  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteIntValue(const nsAString& aName, uint32_t aValue)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  LONG rv = RegSetValueExW(mKey, PromiseFlatString(aName).get(), 0, REG_DWORD,
                           (const BYTE*)&aValue, sizeof(aValue));
  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteInt64Value(const nsAString& aName, uint64_t aValue)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  LONG rv = RegSetValueExW(mKey, PromiseFlatString(aName).get(), 0, REG_QWORD,
                           (const BYTE*)&aValue, sizeof(aValue));
  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteBinaryValue(const nsAString& aName,
                                  const nsACString& aValue)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  const nsCString& flatValue = PromiseFlatCString(aValue);
  LONG rv = RegSetValueExW(mKey, PromiseFlatString(aName).get(), 0, REG_BINARY,
                           (const BYTE*)flatValue.get(), flatValue.Length());
  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::StartWatching(bool aRecurse)
{
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mWatchEvent) {
    return NS_OK;
  }

  mWatchEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  if (!mWatchEvent) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  DWORD filter = REG_NOTIFY_CHANGE_NAME |
                 REG_NOTIFY_CHANGE_ATTRIBUTES |
                 REG_NOTIFY_CHANGE_LAST_SET |
                 REG_NOTIFY_CHANGE_SECURITY;

  LONG rv = RegNotifyChangeKeyValue(mKey, aRecurse, filter, mWatchEvent, TRUE);
  if (rv != ERROR_SUCCESS) {
    StopWatching();
    // On older versions of Windows, this call is not implemented, so simply
    // return NS_OK in those cases and pretend that the watching is happening.
    return (rv == ERROR_CALL_NOT_IMPLEMENTED) ? NS_OK : NS_ERROR_FAILURE;
  }

  mWatchRecursive = aRecurse;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::StopWatching()
{
  if (mWatchEvent) {
    CloseHandle(mWatchEvent);
    mWatchEvent = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::HasChanged(bool* aResult)
{
  if (mWatchEvent && WaitForSingleObject(mWatchEvent, 0) == WAIT_OBJECT_0) {
    // An event only gets signaled once, then it's done, so we have to set up
    // another event to watch.
    StopWatching();
    StartWatching(mWatchRecursive);
    *aResult = true;
  } else {
    *aResult = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::IsWatching(bool* aResult)
{
  *aResult = (mWatchEvent != nullptr);
  return NS_OK;
}

//-----------------------------------------------------------------------------

void
NS_NewWindowsRegKey(nsIWindowsRegKey** aResult)
{
  RefPtr<nsWindowsRegKey> key = new nsWindowsRegKey();
  key.forget(aResult);
}

//-----------------------------------------------------------------------------

nsresult
nsWindowsRegKeyConstructor(nsISupports* aDelegate, const nsIID& aIID,
                           void** aResult)
{
  if (aDelegate) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsCOMPtr<nsIWindowsRegKey> key;
  NS_NewWindowsRegKey(getter_AddRefs(key));
  return key->QueryInterface(aIID, aResult);
}
