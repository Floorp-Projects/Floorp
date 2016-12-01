/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NSCLIENTAUTHREMEMBER_H__
#define __NSCLIENTAUTHREMEMBER_H__

#include "mozilla/ReentrantMonitor.h"
#include "nsTHashtable.h"
#include "nsIObserver.h"
#include "nsIX509Cert.h"
#include "nsNSSCertificate.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "mozilla/Attributes.h"

namespace mozilla {
  class NeckoOriginAttributes;
}

using mozilla::NeckoOriginAttributes;

class nsClientAuthRemember
{
public:

  nsClientAuthRemember()
  {
  }

  nsClientAuthRemember(const nsClientAuthRemember& aOther)
  {
    this->operator=(aOther);
  }

  nsClientAuthRemember& operator=(const nsClientAuthRemember& aOther)
  {
    mAsciiHost = aOther.mAsciiHost;
    mFingerprint = aOther.mFingerprint;
    mDBKey = aOther.mDBKey;
    return *this;
  }

  nsCString mAsciiHost;
  nsCString mFingerprint;
  nsCString mDBKey;
};


// hash entry class
class nsClientAuthRememberEntry final : public PLDHashEntryHdr
{
  public:
    // Hash methods
    typedef const char* KeyType;
    typedef const char* KeyTypePointer;

    // do nothing with aHost - we require mHead to be set before we're live!
    explicit nsClientAuthRememberEntry(KeyTypePointer aHostWithCertUTF8)
    {
    }

    nsClientAuthRememberEntry(const nsClientAuthRememberEntry& aToCopy)
    {
      mSettings = aToCopy.mSettings;
    }

    ~nsClientAuthRememberEntry()
    {
    }

    KeyType GetKey() const
    {
      return EntryKeyPtr();
    }

    KeyTypePointer GetKeyPointer() const
    {
      return EntryKeyPtr();
    }

    bool KeyEquals(KeyTypePointer aKey) const
    {
      return !strcmp(EntryKeyPtr(), aKey);
    }

    static KeyTypePointer KeyToPointer(KeyType aKey)
    {
      return aKey;
    }

    static PLDHashNumber HashKey(KeyTypePointer aKey)
    {
      return PLDHashTable::HashStringKey(aKey);
    }

    enum { ALLOW_MEMMOVE = false };

    // get methods
    inline const nsCString& GetEntryKey() const { return mEntryKey; }

    inline KeyTypePointer EntryKeyPtr() const
    {
      return mEntryKey.get();
    }

    nsClientAuthRemember mSettings;
    nsCString mEntryKey;
};

class nsClientAuthRememberService final : public nsIObserver,
                                          public nsSupportsWeakReference
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsClientAuthRememberService();

  nsresult Init();

  static void GetEntryKey(const nsACString& aHostName,
                          const NeckoOriginAttributes& aOriginAttributes,
                          const nsACString& aFingerprint,
                          /*out*/ nsACString& aEntryKey);

  nsresult RememberDecision(const nsACString& aHostName,
                            const NeckoOriginAttributes& aOriginAttributes,
                            CERTCertificate* aServerCert,
                            CERTCertificate* aClientCert);

  nsresult HasRememberedDecision(const nsACString& aHostName,
                                 const NeckoOriginAttributes& aOriginAttributes,
                                 CERTCertificate* aServerCert,
                                 nsACString& aCertDBKey, bool* aRetVal);

  void ClearRememberedDecisions();
  static void ClearAllRememberedDecisions();

protected:
    ~nsClientAuthRememberService();

    mozilla::ReentrantMonitor monitor;
    nsTHashtable<nsClientAuthRememberEntry> mSettingsTable;

    void RemoveAllFromMemory();
    nsresult AddEntryToList(const nsACString& aHost,
                            const NeckoOriginAttributes& aOriginAttributes,
                            const nsACString& aServerFingerprint,
                            const nsACString& aDBKey);
};

#endif
