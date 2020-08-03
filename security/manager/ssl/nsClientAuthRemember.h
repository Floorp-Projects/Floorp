/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NSCLIENTAUTHREMEMBER_H__
#define __NSCLIENTAUTHREMEMBER_H__

#include <utility>

#include "mozilla/Attributes.h"
#include "mozilla/DataStorage.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsIClientAuthRememberService.h"
#include "nsIObserver.h"
#include "nsNSSCertificate.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "nsWeakReference.h"

namespace mozilla {
class OriginAttributes;
}

using mozilla::OriginAttributes;

class nsClientAuthRemember final : public nsIClientAuthRememberRecord {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICLIENTAUTHREMEMBERRECORD

  nsClientAuthRemember(const nsCString& aEntryKey, const nsCString& aDBKey) {
    mEntryKey = aEntryKey;
    if (!aDBKey.Equals(nsClientAuthRemember::SentinelValue)) {
      mDBKey = aDBKey;
    }

    nsTArray<nsCString*> fields = {&mAsciiHost, &mFingerprint};

    auto fieldsIter = fields.begin();
    auto splitter = aEntryKey.Split(',');
    auto splitterIter = splitter.begin();
    for (; fieldsIter != fields.end() && splitterIter != splitter.end();
         ++fieldsIter, ++splitterIter) {
      (*fieldsIter)->Assign(*splitterIter);
    }
  }

  nsCString mAsciiHost;
  nsCString mFingerprint;
  nsCString mDBKey;
  nsCString mEntryKey;
  static const nsCString SentinelValue;

 protected:
  ~nsClientAuthRemember() = default;
};

class nsClientAuthRememberService final : public nsIClientAuthRememberService {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICLIENTAUTHREMEMBERSERVICE

  nsClientAuthRememberService() = default;

  nsresult Init();

  static void GetEntryKey(const nsACString& aHostName,
                          const OriginAttributes& aOriginAttributes,
                          const nsACString& aFingerprint,
                          /*out*/ nsACString& aEntryKey);

  static bool IsPrivateBrowsingKey(const nsCString& entryKey);

 protected:
  ~nsClientAuthRememberService() = default;

  static mozilla::DataStorageType GetDataStorageType(
      const OriginAttributes& aOriginAttributes);

  RefPtr<mozilla::DataStorage> mClientAuthRememberList;

  nsresult AddEntryToList(const nsACString& aHost,
                          const OriginAttributes& aOriginAttributes,
                          const nsACString& aServerFingerprint,
                          const nsACString& aDBKey);
};

#endif
