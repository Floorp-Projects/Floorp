/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NSCLIENTAUTHREMEMBER_H__
#define __NSCLIENTAUTHREMEMBER_H__

#include <utility>

#include "mozilla/Attributes.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsIClientAuthRememberService.h"
#include "nsIDataStorage.h"
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

  nsClientAuthRemember(const nsACString& aHostName,
                       const OriginAttributes& aOriginAttributes) {
    mAsciiHost.Assign(aHostName);
    aOriginAttributes.CreateSuffix(mOriginAttributesSuffix);
  }

  nsClientAuthRemember(const nsCString& aEntryKey, const nsCString& aDBKey) {
    if (!aDBKey.Equals(nsClientAuthRemember::SentinelValue)) {
      mDBKey = aDBKey;
    }

    size_t field_index = 0;
    for (const auto& field : aEntryKey.Split(',')) {
      switch (field_index) {
        case 0:
          mAsciiHost.Assign(field);
          break;
        case 1:
          break;
        case 2:
          mOriginAttributesSuffix.Assign(field);
          break;
        default:
          break;
      }
      field_index++;
    }
  }

  nsCString mAsciiHost;
  nsCString mOriginAttributesSuffix;
  nsCString mDBKey;
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

  static bool IsPrivateBrowsingKey(const nsCString& entryKey);

 protected:
  ~nsClientAuthRememberService() = default;

  static nsIDataStorage::DataType GetDataStorageType(
      const OriginAttributes& aOriginAttributes);

  nsCOMPtr<nsIDataStorage> mClientAuthRememberList;

  nsresult AddEntryToList(const nsACString& aHost,
                          const OriginAttributes& aOriginAttributes,
                          const nsACString& aDBKey);

  void Migrate();
};

#endif
