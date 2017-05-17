/* This Source Code Form is subject to the terms of the Mozilla
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierInfo_h_
#define nsUrlClassifierInfo_h_

#include "nsIUrlClassifierInfo.h"
#include "nsString.h"

class nsUrlClassifierPositiveCacheEntry final : public nsIUrlClassifierPositiveCacheEntry
{
public:
  nsUrlClassifierPositiveCacheEntry();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERPOSITIVECACHEENTRY

private:
  ~nsUrlClassifierPositiveCacheEntry() {}

public:
  nsCString fullhash;

  int64_t expirySec;
};

class nsUrlClassifierCacheEntry final : public nsIUrlClassifierCacheEntry
{
public:
  nsUrlClassifierCacheEntry();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERCACHEENTRY

private:
  ~nsUrlClassifierCacheEntry() {}

public:
  nsCString prefix;

  int64_t expirySec;

  nsTArray<nsCOMPtr<nsIUrlClassifierPositiveCacheEntry>> matches;
};

class nsUrlClassifierCacheInfo final : public nsIUrlClassifierCacheInfo
{
public:
  nsUrlClassifierCacheInfo();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERCACHEINFO

private:
  ~nsUrlClassifierCacheInfo() {}

public:
  nsCString table;

  nsTArray<nsCOMPtr<nsIUrlClassifierCacheEntry>> entries;
};

#endif // nsUrlClassifierInfo_h_
