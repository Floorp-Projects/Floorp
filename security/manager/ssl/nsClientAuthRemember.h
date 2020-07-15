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

  nsClientAuthRemember(const nsACString& aAsciiHost,
                       const nsACString& aFingerprint, const nsACString& aDBKey,
                       const nsACString& aEntryKey) {
    mAsciiHost = aAsciiHost;
    mFingerprint = aFingerprint;
    mDBKey = aDBKey;
    mEntryKey = aEntryKey;
  }

  nsCString mAsciiHost;
  nsCString mFingerprint;
  nsCString mDBKey;
  nsCString mEntryKey;

 protected:
  ~nsClientAuthRemember() = default;
};

// hash entry class
class nsClientAuthRememberEntry final : public PLDHashEntryHdr {
 public:
  // Hash methods
  typedef const char* KeyType;
  typedef const char* KeyTypePointer;

  // do nothing with aHost - we require mHead to be set before we're live!
  explicit nsClientAuthRememberEntry(KeyTypePointer aHostWithCertUTF8) {}

  nsClientAuthRememberEntry(nsClientAuthRememberEntry&& aToMove)
      : PLDHashEntryHdr(std::move(aToMove)),
        mSettings(std::move(aToMove.mSettings)),
        mEntryKey(std::move(aToMove.mEntryKey)) {}

  ~nsClientAuthRememberEntry() = default;

  KeyType GetKey() const { return EntryKeyPtr(); }

  KeyTypePointer GetKeyPointer() const { return EntryKeyPtr(); }

  bool KeyEquals(KeyTypePointer aKey) const {
    return !strcmp(EntryKeyPtr(), aKey);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }

  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return mozilla::HashString(aKey);
  }

  enum { ALLOW_MEMMOVE = false };

  // get methods
  inline const nsCString& GetEntryKey() const { return mEntryKey; }

  inline KeyTypePointer EntryKeyPtr() const { return mEntryKey.get(); }

  nsCOMPtr<nsIClientAuthRememberRecord> mSettings;
  nsCString mEntryKey;
};

class nsClientAuthRememberService final : public nsIObserver,
                                          public nsIClientAuthRememberService {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSICLIENTAUTHREMEMBERSERVICE

  nsClientAuthRememberService();

  nsresult Init();

  static void GetEntryKey(const nsACString& aHostName,
                          const OriginAttributes& aOriginAttributes,
                          const nsACString& aFingerprint,
                          /*out*/ nsACString& aEntryKey);

  static bool IsPrivateBrowsingKey(const nsCString& entryKey);

 protected:
  ~nsClientAuthRememberService();

  mozilla::ReentrantMonitor monitor;
  nsTHashtable<nsClientAuthRememberEntry> mSettingsTable;

  void RemoveAllFromMemory();

  nsresult ClearPrivateDecisions();

  nsresult AddEntryToList(const nsACString& aHost,
                          const OriginAttributes& aOriginAttributes,
                          const nsACString& aServerFingerprint,
                          const nsACString& aDBKey);
};

#endif
