/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <windows.h>
#include <shlwapi.h>
#include <stdlib.h>
#include "nsWindowsRegKey.h"
#include "nsNativeCharsetUtils.h"
#include "nsString.h"
#include "nsCOMPtr.h"

// The Platform SDK included with VC6 does not define REG_QWORD.  VC 7's
// WinNT.h defines REG_QWORD as follows:
#ifndef REG_QWORD
# define REG_QWORD 11
#endif

//-----------------------------------------------------------------------------

// This class simplifies conversion from unicode to native charset somewhat.
class PromiseNativeString : public nsCAutoString
{
public:
  PromiseNativeString(const nsAString &input)
  {
    NS_CopyUnicodeToNative(input, *this);
  }
};

//-----------------------------------------------------------------------------

// According to MSDN, the following limits apply (in characters excluding room
// for terminating null character):
#define MAX_KEY_NAME_LEN     255
#define MAX_VALUE_NAME_LEN   16383

class nsWindowsRegKey : public nsIWindowsRegKey
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINDOWSREGKEY

  nsWindowsRegKey()
    : mKey(NULL)
    , mWatchEvent(NULL)
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

NS_IMPL_ISUPPORTS1(nsWindowsRegKey, nsIWindowsRegKey)

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
    mKey = NULL;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::Open(PRUint32 rootKey, const nsAString &path, PRUint32 mode)
{
  Close();

  LONG rv = RegOpenKeyExW((HKEY) rootKey, PromiseFlatString(path).get(), 0,
                          (REGSAM) mode, &mKey);

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::Create(PRUint32 rootKey, const nsAString &path, PRUint32 mode)
{
  Close();

  DWORD disposition;
  LONG rv = RegCreateKeyExW((HKEY) rootKey, PromiseFlatString(path).get(), 0,
                            NULL, REG_OPTION_NON_VOLATILE, (REGSAM) mode, NULL,
                            &mKey, &disposition);

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::OpenChild(const nsAString &path, PRUint32 mode,
                           nsIWindowsRegKey **result)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIWindowsRegKey> child = new nsWindowsRegKey();
  if (!child)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult rv = child->Open((PRUint32) mKey, path, mode);
  if (NS_FAILED(rv))
    return rv;

  child.swap(*result);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::CreateChild(const nsAString &path, PRUint32 mode,
                             nsIWindowsRegKey **result)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIWindowsRegKey> child = new nsWindowsRegKey();
  if (!child)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult rv = child->Create((PRUint32) mKey, path, mode);
  if (NS_FAILED(rv))
    return rv;

  child.swap(*result);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetChildCount(PRUint32 *result)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  DWORD numSubKeys;
  LONG rv = RegQueryInfoKeyW(mKey, NULL, NULL, NULL, &numSubKeys, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL);
  NS_ENSURE_STATE(rv == ERROR_SUCCESS);

  *result = numSubKeys;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetChildName(PRUint32 index, nsAString &result)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  FILETIME lastWritten;

  PRUnichar nameBuf[MAX_KEY_NAME_LEN + 1];
  DWORD nameLen = sizeof(nameBuf) / sizeof(nameBuf[0]);

  LONG rv = RegEnumKeyExW(mKey, index, nameBuf, &nameLen, NULL, NULL, NULL,
                          &lastWritten);
  if (rv != ERROR_SUCCESS)
    return NS_ERROR_NOT_AVAILABLE;  // XXX what's the best error code here?

  result.Assign(nameBuf, nameLen);
 
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::HasChild(const nsAString &name, PRBool *result)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  // Check for the existance of a child key by opening the key with minimal
  // rights.  Perhaps there is a more efficient way to do this?

  HKEY key;
  LONG rv = RegOpenKeyExW(mKey, PromiseFlatString(name).get(), 0,
                          STANDARD_RIGHTS_READ, &key);

  if (*result = (rv == ERROR_SUCCESS && key))
    RegCloseKey(key);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetValueCount(PRUint32 *result)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  DWORD numValues;
  LONG rv = RegQueryInfoKeyW(mKey, NULL, NULL, NULL, NULL, NULL, NULL,
                             &numValues, NULL, NULL, NULL, NULL);
  NS_ENSURE_STATE(rv == ERROR_SUCCESS);

  *result = numValues;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetValueName(PRUint32 index, nsAString &result)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  PRUnichar nameBuf[MAX_VALUE_NAME_LEN];
  DWORD nameLen = sizeof(nameBuf) / sizeof(nameBuf[0]);

  LONG rv = RegEnumValueW(mKey, index, nameBuf, &nameLen, NULL, NULL, NULL,
                          NULL);
  if (rv != ERROR_SUCCESS)
    return NS_ERROR_NOT_AVAILABLE;  // XXX what's the best error code here?

  result.Assign(nameBuf, nameLen);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::HasValue(const nsAString &name, PRBool *result)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(name).get(), 0, NULL, NULL,
                             NULL);

  *result = (rv == ERROR_SUCCESS);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::RemoveChild(const nsAString &name)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  LONG rv = RegDeleteKeyW(mKey, PromiseFlatString(name).get());

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::RemoveValue(const nsAString &name)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  LONG rv = RegDeleteValueW(mKey, PromiseFlatString(name).get());

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::GetValueType(const nsAString &name, PRUint32 *result)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(name).get(), 0,
                             (LPDWORD) result, NULL, NULL);

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadStringValue(const nsAString &name, nsAString &result)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  DWORD type, size;

  const nsString &flatName = PromiseFlatString(name);

  LONG rv = RegQueryValueExW(mKey, flatName.get(), 0, &type, NULL, &size);
  if (rv != ERROR_SUCCESS)
    return NS_ERROR_FAILURE;

  // This must be a string type in order to fetch the value as a string.
  // We're being a bit forgiving here by allowing types other than REG_SZ.
  NS_ENSURE_STATE(type == REG_SZ ||
                  type == REG_EXPAND_SZ ||
                  type == REG_MULTI_SZ);

  // The buffer size must be a multiple of 2.
  NS_ENSURE_STATE(size % 2 == 0);

  if (size == 0) {
    result.Truncate();
    return NS_OK;
  }

  // |size| includes room for the terminating null character
  DWORD resultLen = size / 2 - 1;

  result.SetLength(resultLen);
  nsAString::iterator begin;
  result.BeginWriting(begin);
  if (begin.size_forward() != resultLen)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = RegQueryValueExW(mKey, flatName.get(), 0, NULL, (LPBYTE) begin.get(),
                        &size);

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadIntValue(const nsAString &name, PRUint32 *result)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  DWORD size = sizeof(*result);
  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(name).get(), 0, NULL,
                             (LPBYTE) result, &size);

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadInt64Value(const nsAString &name, PRUint64 *result)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  DWORD size = sizeof(*result);
  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(name).get(), 0, NULL,
                             (LPBYTE) result, &size);

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadBinaryValue(const nsAString &name, nsACString &result)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  DWORD size;
  LONG rv = RegQueryValueExW(mKey, PromiseFlatString(name).get(), 0,
                             NULL, NULL, &size);

  if (rv != ERROR_SUCCESS)
    return NS_ERROR_FAILURE;

  result.SetLength(size);
  nsACString::iterator begin;
  result.BeginWriting(begin);
  if (begin.size_forward() != size)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = RegQueryValueExW(mKey, PromiseFlatString(name).get(), 0, NULL,
                        (LPBYTE) begin.get(), &size);

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteStringValue(const nsAString &name, const nsAString &value)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  // Need to indicate complete size of buffer including null terminator.
  const nsString &flatValue = PromiseFlatString(value);

  LONG rv = RegSetValueExW(mKey, PromiseFlatString(name).get(), 0, REG_SZ,
                           (const BYTE *) flatValue.get(),
                           (flatValue.Length() + 1) * sizeof(PRUnichar));

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteIntValue(const nsAString &name, PRUint32 value)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  LONG rv = RegSetValueExW(mKey, PromiseFlatString(name).get(), 0, REG_DWORD,
                           (const BYTE *) &value, sizeof(value));

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteInt64Value(const nsAString &name, PRUint64 value)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  LONG rv = RegSetValueExW(mKey, PromiseFlatString(name).get(), 0, REG_QWORD,
                           (const BYTE *) &value, sizeof(value));

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteBinaryValue(const nsAString &name, const nsACString &value)
{
  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  const nsCString &flatValue = PromiseFlatCString(value);
  LONG rv = RegSetValueExW(mKey, PromiseFlatString(name).get(), 0, REG_BINARY,
                           (const BYTE *) flatValue.get(), flatValue.Length());

  return (rv == ERROR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::StartWatching(PRBool recurse)
{
#ifdef WINCE
  return NS_ERROR_NOT_IMPLEMENTED;
#else

  NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);

  if (mWatchEvent)
    return NS_OK;
  
  mWatchEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
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
#endif
}

NS_IMETHODIMP
nsWindowsRegKey::StopWatching()
{
  if (mWatchEvent) {
    CloseHandle(mWatchEvent);
    mWatchEvent = NULL;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::HasChanged(PRBool *result)
{
  if (mWatchEvent && WaitForSingleObject(mWatchEvent, 0) == WAIT_OBJECT_0) {
    // An event only gets signaled once, then it's done, so we have to set up
    // another event to watch.
    StopWatching();
    StartWatching(mWatchRecursive);
    *result = PR_TRUE;
  } else {
    *result = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::IsWatching(PRBool *result)
{
  *result = (mWatchEvent != NULL);
  return NS_OK;
}

//-----------------------------------------------------------------------------

nsresult
NS_NewWindowsRegKey(nsIWindowsRegKey **result)
{
  *result = new nsWindowsRegKey();
  if (!*result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*result);
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_METHOD
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
