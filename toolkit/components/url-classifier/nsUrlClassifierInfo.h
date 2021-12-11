/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierInfo_h_
#define nsUrlClassifierInfo_h_

#include "nsIUrlClassifierInfo.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"

class nsUrlClassifierPositiveCacheEntry final
    : public nsIUrlClassifierPositiveCacheEntry {
 public:
  nsUrlClassifierPositiveCacheEntry();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERPOSITIVECACHEENTRY

 private:
  ~nsUrlClassifierPositiveCacheEntry() = default;

 public:
  nsCString fullhash;

  int64_t expirySec;
};

class nsUrlClassifierCacheEntry final : public nsIUrlClassifierCacheEntry {
 public:
  nsUrlClassifierCacheEntry();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERCACHEENTRY

 private:
  ~nsUrlClassifierCacheEntry() = default;

 public:
  nsCString prefix;

  int64_t expirySec;

  nsTArray<nsCOMPtr<nsIUrlClassifierPositiveCacheEntry>> matches;
};

class nsUrlClassifierCacheInfo final : public nsIUrlClassifierCacheInfo {
 public:
  nsUrlClassifierCacheInfo();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERCACHEINFO

 private:
  ~nsUrlClassifierCacheInfo() = default;

 public:
  nsCString table;

  nsTArray<nsCOMPtr<nsIUrlClassifierCacheEntry>> entries;
};

#endif  // nsUrlClassifierInfo_h_
