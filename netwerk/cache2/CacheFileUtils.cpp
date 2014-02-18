/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheFileUtils.h"
#include "LoadContextInfo.h"
#include "nsCOMPtr.h"
#include "nsString.h"


namespace mozilla {
namespace net {
namespace CacheFileUtils {

nsresult ParseKey(const nsACString &aKey,
                  nsILoadContextInfo **aInfo,
                  nsACString *aURL)
{
  bool isPrivate;
  bool isAnonymous;
  bool isInBrowser;
  uint32_t appId;

  if (aKey.Length() < 4) {
    return NS_ERROR_FAILURE;
  }

  if (aKey[0] == '-') {
    isPrivate = false;
  }
  else if (aKey[0] == 'P') {
    isPrivate = true;
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (aKey[1] == '-') {
    isAnonymous = false;
  }
  else if (aKey[1] == 'A') {
    isAnonymous = true;
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (aKey[2] != ':') {
    return NS_ERROR_FAILURE;
  }

  int32_t appIdEndIdx = aKey.FindChar(':', 3);
  if (appIdEndIdx == kNotFound) {
    return NS_ERROR_FAILURE;
  }

  if (aKey[appIdEndIdx - 1] == 'B') {
    isInBrowser = true;
    appIdEndIdx--;
  } else {
    isInBrowser = false;
  }

  if (appIdEndIdx < 3) {
    return NS_ERROR_FAILURE;
  }

  if (appIdEndIdx == 3) {
    appId = nsILoadContextInfo::NO_APP_ID;
  }
  else {
    nsAutoCString appIdStr(Substring(aKey, 3, appIdEndIdx - 3));
    nsresult rv;
    int64_t appId64 = appIdStr.ToInteger64(&rv);
    if (NS_FAILED(rv) || appId64 > PR_UINT32_MAX)
      return NS_ERROR_FAILURE;
    appId = appId64;
  }

  if (aInfo) {
    nsCOMPtr<nsILoadContextInfo> info;
    info = GetLoadContextInfo(isPrivate, appId, isInBrowser, isAnonymous);
    info.forget(aInfo);
  }

  if (aURL) {
    *aURL = Substring(aKey, appIdEndIdx + 1);
  }

  return NS_OK;
}

void CreateKeyPrefix(nsILoadContextInfo* aInfo, nsACString &_retval)
{
  /**
   * This key is used to salt file hashes.  When form of the key is changed
   * cache entries will fail to find on disk.
   */
  _retval.Assign(aInfo->IsPrivate() ? 'P' : '-');
  _retval.Append(aInfo->IsAnonymous() ? 'A' : '-');
  _retval.Append(':');
  if (aInfo->AppId() != nsILoadContextInfo::NO_APP_ID) {
    _retval.AppendInt(aInfo->AppId());
  }
  if (aInfo->IsInBrowserElement()) {
    _retval.Append('B');
  }
}


} // CacheFileUtils
} // net
} // mozilla

