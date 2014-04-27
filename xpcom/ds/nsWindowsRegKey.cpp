/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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

class nsWindowsRegKey MOZ_FINAL : public nsIWindowsRegKey
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
nsWindowsRegKey::GetKey(HKEY *key)
{
  *key = mKey;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::SetKey(HKEY key)
{
  // We do not close the older key!
  StopWatching();

  mKey = key;
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
nsWindowsRegKey::Open(uint32_t rootKey, const nsAString &path, uint32_t mode)
{
  Close();

  LONG rv = RegOpenKeyExW((HKEY)(intptr_t) rootKey, PromiseFlatString(path).get(), 0,
                          (REGSAM) mode, &mKey);

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::Create(uint32_t rootKey, const nsAString &path, uint32_t mode)
{
  Close();

  DWORD disposition;
  LONG rv = RegCreateKeyExW((HKEY)(intptr_t) rootKey, PromiseFlatString(path).get(), 0,
                            nullptr, REG_OPTION_NON_VOLATILE, (REGSAM) mode, nullptr,
                            &mKey, &disposition);

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::OpenChild(const nsAString &path, uint32_t mode,
                           nsIWindowsRegKey **result)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIWindowsRegKey> child = new nsWindowsRegKey();
  
  nsresult rv = child->Open((uintptr_t) mKey, path, mode);
  if (NS_FAILED(rv))
    return rv;

  child.swap(*result);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::CreateChild(const nsAString &path, uint32_t mode,
                             nsIWindowsRegKey **result)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIWindowsRegKey> child = new nsWindowsRegKey();
  
  nsresult rv = child->Create((uintptr_t) mKey, path, mode);
  if (NS_FAILED(rv))
    return rv;

  child.swap(*result);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetChildCount(uint32_t *result)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  DWORD numSubKeys;
  LONG rv = RegQueryInfoKeyW(mKey, nullptr, nullptr, nullptr, &numSubKeys,
                             nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr);
  if (rv != ERROR_SUCCESS)
    return NS_ERROR_FAILURE;

  *result = numSubKeys;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetChildName(uint32_t index, nsAString &result)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  FILETIME lastWritten;

  wchar_t nameBuf[MAX_KEY_NAME_LEN + 1];
  DWORD nameLen = sizeof(nameBuf) / sizeof(nameBuf[0]);

  LONG rv = RegEnumKeyExW(mKey, index, nameBuf, &nameLen, nullptr, nullptr,
                          nullptr, &lastWritten);
  if (rv != ERROR_SUCCESS)
    return NS_ERROR_NOT_AVAILABLE;  // XXX what's the best error code here?

  result.Assign(nameBuf, nameLen);
 
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::HasChild(const nsAString &name, bool *result)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  // Check for the existence of a child key by opening the key with minimal
  // rights.  Perhaps there is a more efficient way to do this?

  HKEY key;
  LONG rv = RegOpenKeyExW(mKey, PromiseFlatString(name).get(), 0,
                          STANDARD_RIGHTS_READ, &key);

  if ((*result = (rv == ERROR_SUCCESS && key)))
    RegCloseKey(key);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetValueCount(uint32_t *result)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  DWORD numValues;
  LONG rv = RegQueryInfoKeyW(mKey, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr, &numValues, nullptr, nullptr,
                             nullptr, nullptr);
  if (rv != ERROR_SUCCESS)
    return NS_ERROR_FAILURE;

  *result = numValues;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetValueName(uint32_t index, nsAString &result)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  wchar_t nameBuf[MAX_VALUE_NAME_LEN];
  DWORD nameLen = sizeof(nameBuf) / sizeof(nameBuf[0]);

  LONG rv = RegEnumValueW(mKey, index, nameBuf, &nameLen, nullptr, nullptr,
                          nullptr, nullptr);
  if (rv != ERROR_SUCCESS)
    return NS_ERROR_NOT_AVAILABLE;  // XXX what's the best error code here?

  result.Assign(nameBuf, nameLen);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::HasValue(const nsAString &name, bool *result)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(name).get(), 0, nullptr,
                             nullptr, nullptr);

  *result = (rv == ERROR_SUCCESS);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::RemoveChild(const nsAString &name)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  LONG rv = RegDeleteKeyW(mKey, PromiseFlatString(name).get());

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::RemoveValue(const nsAString &name)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  LONG rv = RegDeleteValueW(mKey, PromiseFlatString(name).get());

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::GetValueType(const nsAString &name, uint32_t *result)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(name).get(), 0,
                             (LPDWORD) result, nullptr, nullptr);

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadStringValue(const nsAString &name, nsAString &result)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  DWORD type, size;

  const nsString &flatName = PromiseFlatString(name);

  LONG rv = RegQueryValueExW(mKey, flatName.get(), 0, &type, nullptr, &size);
  if (rv != ERROR_SUCCESS)
    return NS_ERROR_FAILURE;

  // This must be a string type in order to fetch the value as a string.
  // We're being a bit forgiving here by allowing types other than REG_SZ.
  if (type != REG_SZ && type == REG_EXPAND_SZ && type == REG_MULTI_SZ)
    return NS_ERROR_FAILURE;

  // The buffer size must be a multiple of 2.
  if (size % 2 != 0)
    return NS_ERROR_UNEXPECTED;

  if (size == 0) {
    result.Truncate();
    return NS_OK;
  }

  // |size| may or may not include the terminating null character.
  DWORD resultLen = size / 2;

  result.SetLength(resultLen);
  nsAString::iterator begin;
  result.BeginWriting(begin);
  if (begin.size_forward() != resultLen)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = RegQueryValueExW(mKey, flatName.get(), 0, &type, (LPBYTE) begin.get(),
                        &size);

  if (!result.CharAt(resultLen-1)) {
    // The string passed to us had a null terminator in the final position.
    result.Truncate(resultLen-1);
  }

  // Expand the environment variables if needed
  if (type == REG_EXPAND_SZ) {
    const nsString &flatSource = PromiseFlatString(result);
    resultLen = ExpandEnvironmentStringsW(flatSource.get(), nullptr, 0);
    if (resultLen > 1) {
      nsAutoString expandedResult;
      // |resultLen| includes the terminating null character
      --resultLen;
      expandedResult.SetLength(resultLen);
      nsAString::iterator begin;
      expandedResult.BeginWriting(begin);
      if (begin.size_forward() != resultLen)
        return NS_ERROR_OUT_OF_MEMORY;

      resultLen = ExpandEnvironmentStringsW(flatSource.get(),
                                            wwc(begin.get()),
                                            resultLen + 1);
      if (resultLen <= 0) {
        rv = ERROR_UNKNOWN_FEATURE;
        result.Truncate();
      } else {
        rv = ERROR_SUCCESS;
        result = expandedResult;
      }
    } else if (resultLen == 1) {
      // It apparently expands to nothing (just a null terminator).
      result.Truncate();
    }
  }

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadIntValue(const nsAString &name, uint32_t *result)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  DWORD size = sizeof(*result);
  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(name).get(), 0, nullptr,
                             (LPBYTE) result, &size);

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadInt64Value(const nsAString &name, uint64_t *result)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  DWORD size = sizeof(*result);
  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(name).get(), 0, nullptr,
                             (LPBYTE) result, &size);

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadBinaryValue(const nsAString &name, nsACString &result)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  DWORD size;
  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(name).get(), 0,
                             nullptr, nullptr, &size);

  if (rv != ERROR_SUCCESS)
    return NS_ERROR_FAILURE;

  if (!size) {
    result.Truncate();
    return NS_OK;
  }

  result.SetLength(size);
  nsACString::iterator begin;
  result.BeginWriting(begin);
  if (begin.size_forward() != size)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = RegQueryValueExW(mKey, PromiseFlatString(name).get(), 0, nullptr,
                        (LPBYTE) begin.get(), &size);

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteStringValue(const nsAString &name, const nsAString &value)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  // Need to indicate complete size of buffer including null terminator.
  const nsString &flatValue = PromiseFlatString(value);

  LONG rv = RegSetValueExW(mKey, PromiseFlatString(name).get(), 0, REG_SZ,
                           (const BYTE *) flatValue.get(),
                           (flatValue.Length() + 1) * sizeof(char16_t));

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteIntValue(const nsAString &name, uint32_t value)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  LONG rv = RegSetValueExW(mKey, PromiseFlatString(name).get(), 0, REG_DWORD,
                           (const BYTE *) &value, sizeof(value));

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteInt64Value(const nsAString &name, uint64_t value)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  LONG rv = RegSetValueExW(mKey, PromiseFlatString(name).get(), 0, REG_QWORD,
                           (const BYTE *) &value, sizeof(value));

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteBinaryValue(const nsAString &name, const nsACString &value)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  const nsCString &flatValue = PromiseFlatCString(value);
  LONG rv = RegSetValueExW(mKey, PromiseFlatString(name).get(), 0, REG_BINARY,
                           (const BYTE *) flatValue.get(), flatValue.Length());

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::StartWatching(bool recurse)
{
  if (NS_WARN_IF(!mKey))
    return NS_ERROR_NOT_INITIALIZED;

  if (mWatchEvent)
    return NS_OK;
  
  mWatchEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  if (!mWatchEvent)
    return NS_ERROR_OUT_OF_MEMORY;
  
  DWORD filter = REG_NOTIFY_CHANGE_NAME |
                 REG_NOTIFY_CHANGE_ATTRIBUTES |
                 REG_NOTIFY_CHANGE_LAST_SET |
                 REG_NOTIFY_CHANGE_SECURITY;

  LONG rv = RegNotifyChangeKeyValue(mKey, recurse, filter, mWatchEvent, TRUE);
  if (rv != ERROR_SUCCESS) {
    StopWatching();
    // On older versions of Windows, this call is not implemented, so simply
    // return NS_OK in those cases and pretend that the watching is happening.
    return (rv == ERROR_CALL_NOT_IMPLEMENTED) ? NS_OK : NS_ERROR_FAILURE;
  }

  mWatchRecursive = recurse;
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
nsWindowsRegKey::HasChanged(bool *result)
{
  if (mWatchEvent && WaitForSingleObject(mWatchEvent, 0) == WAIT_OBJECT_0) {
    // An event only gets signaled once, then it's done, so we have to set up
    // another event to watch.
    StopWatching();
    StartWatching(mWatchRecursive);
    *result = true;
  } else {
    *result = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::IsWatching(bool *result)
{
  *result = (mWatchEvent != nullptr);
  return NS_OK;
}

//-----------------------------------------------------------------------------

nsresult
NS_NewWindowsRegKey(nsIWindowsRegKey **result)
{
  nsRefPtr<nsWindowsRegKey> key = new nsWindowsRegKey();
  key.forget(result);
  return NS_OK;
}

//-----------------------------------------------------------------------------

nsresult
nsWindowsRegKeyConstructor(nsISupports *delegate, const nsIID &iid,
                           void **result)
{
  if (delegate)
    return NS_ERROR_NO_AGGREGATION;

  nsCOMPtr<nsIWindowsRegKey> key;
  nsresult rv = NS_NewWindowsRegKey(getter_AddRefs(key));
  if (NS_SUCCEEDED(rv))
    rv = key->QueryInterface(iid, result);
  return rv;
}
