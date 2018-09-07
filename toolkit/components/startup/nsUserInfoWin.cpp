/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUserInfo.h"

#include "mozilla/ArrayUtils.h" // ArrayLength
#include "mozilla/Span.h"       // MakeStringSpan
#include "nsString.h"
#include "windows.h"
#include "nsCRT.h"

#define SECURITY_WIN32
#include "lm.h"
#include "security.h"

nsUserInfo::nsUserInfo()
{
}

nsUserInfo::~nsUserInfo()
{
}

NS_IMPL_ISUPPORTS(nsUserInfo, nsIUserInfo)

NS_IMETHODIMP
nsUserInfo::GetUsername(nsACString& aUsername)
{
  // UNLEN is the max username length as defined in lmcons.h
  wchar_t username[UNLEN + 1];
  DWORD size = mozilla::ArrayLength(username);
  if (!GetUserNameW(username, &size))
    return NS_ERROR_FAILURE;

  CopyUTF16toUTF8(nsDependentString(username), aUsername);
  return NS_OK;
}

NS_IMETHODIMP
nsUserInfo::GetFullname(nsAString& aFullname)
{
  wchar_t fullName[512];
  DWORD size = mozilla::ArrayLength(fullName);

  if (GetUserNameExW(NameDisplay, fullName, &size)) {
    aFullname.Assign(fullName);
  } else {
    DWORD getUsernameError = GetLastError();

    // Try to use the net APIs regardless of the error because it may be
    // able to obtain the information.
    wchar_t username[UNLEN + 1];
    size = mozilla::ArrayLength(username);
    if (!GetUserNameW(username, &size)) {
      // ERROR_NONE_MAPPED means the user info is not filled out on this computer
      return getUsernameError == ERROR_NONE_MAPPED ?
             NS_ERROR_NOT_AVAILABLE : NS_ERROR_FAILURE;
    }

    const DWORD level = 2;
    LPBYTE info;
    // If the NetUserGetInfo function has no full name info it will return
    // success with an empty string.
    NET_API_STATUS status = NetUserGetInfo(nullptr, username, level, &info);
    if (status != NERR_Success) {
      // We have an error with NetUserGetInfo but we know the info is not
      // filled in because GetUserNameExW returned ERROR_NONE_MAPPED.
      return getUsernameError == ERROR_NONE_MAPPED ?
             NS_ERROR_NOT_AVAILABLE : NS_ERROR_FAILURE;
    }

    nsDependentString fullName =
      nsDependentString(reinterpret_cast<USER_INFO_2 *>(info)->usri2_full_name);

    // NetUserGetInfo returns an empty string if the full name is not filled out
    if (fullName.Length() == 0) {
      NetApiBufferFree(info);
      return NS_ERROR_NOT_AVAILABLE;
    }

    aFullname.Assign(fullName);
    NetApiBufferFree(info);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUserInfo::GetDomain(nsACString& aDomain)
{
  const DWORD level = 100;
  LPBYTE info;
  NET_API_STATUS status = NetWkstaGetInfo(nullptr, level, &info);
  if (status != NERR_Success) {
    return NS_ERROR_FAILURE;
  }

  CopyUTF16toUTF8(
    nsDependentString(
      reinterpret_cast<WKSTA_INFO_100*>(info)->wki100_langroup),
    aDomain);
  NetApiBufferFree(info);

  return NS_OK;
}

NS_IMETHODIMP
nsUserInfo::GetEmailAddress(nsACString& aEmailAddress)
{
  // RFC3696 says max length of an email address is 254
  wchar_t emailAddress[255];
  DWORD size = mozilla::ArrayLength(emailAddress);

  if (!GetUserNameExW(NameUserPrincipal, emailAddress, &size)) {
    DWORD getUsernameError = GetLastError();
    return getUsernameError == ERROR_NONE_MAPPED ?
           NS_ERROR_NOT_AVAILABLE : NS_ERROR_FAILURE;
  }

  CopyUTF16toUTF8(nsDependentString(emailAddress), aEmailAddress);
  return NS_OK;
}
