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

class nsClientAuthRemember
{
public:

  nsClientAuthRemember()
  {
  }
  
  nsClientAuthRemember(const nsClientAuthRemember &other)
  {
    this->operator=(other);
  }

  nsClientAuthRemember &operator=(const nsClientAuthRemember &other)
  {
    mAsciiHost = other.mAsciiHost;
    mFingerprint = other.mFingerprint;
    mDBKey = other.mDBKey;
    return *this;
  }

  nsCString mAsciiHost;
  nsCString mFingerprint;
  nsCString mDBKey;
};


// hash entry class
class nsClientAuthRememberEntry MOZ_FINAL : public PLDHashEntryHdr
{
  public:
    // Hash methods
    typedef const char* KeyType;
    typedef const char* KeyTypePointer;

    // do nothing with aHost - we require mHead to be set before we're live!
    nsClientAuthRememberEntry(KeyTypePointer aHostWithCertUTF8)
    {
    }

    nsClientAuthRememberEntry(const nsClientAuthRememberEntry& toCopy)
    {
      mSettings = toCopy.mSettings;
    }

    ~nsClientAuthRememberEntry()
    {
    }

    KeyType GetKey() const
    {
      return HostWithCertPtr();
    }

    KeyTypePointer GetKeyPointer() const
    {
      return HostWithCertPtr();
    }

    bool KeyEquals(KeyTypePointer aKey) const
    {
      return !strcmp(HostWithCertPtr(), aKey);
    }

    static KeyTypePointer KeyToPointer(KeyType aKey)
    {
      return aKey;
    }

    static PLDHashNumber HashKey(KeyTypePointer aKey)
    {
      // PL_DHashStringKey doesn't use the table parameter, so we can safely
      // pass nullptr
      return PL_DHashStringKey(nullptr, aKey);
    }

    enum { ALLOW_MEMMOVE = false };

    // get methods
    inline const nsCString &HostWithCert() const { return mHostWithCert; }

    inline KeyTypePointer HostWithCertPtr() const
    {
      return mHostWithCert.get();
    }

    nsClientAuthRemember mSettings;
    nsCString mHostWithCert;
};

class nsClientAuthRememberService MOZ_FINAL : public nsIObserver,
                                              public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsClientAuthRememberService();
  ~nsClientAuthRememberService();

  nsresult Init();

  static void GetHostWithCert(const nsACString & aHostName, 
                              const nsACString & nickname, nsACString& _retval);

  nsresult RememberDecision(const nsACString & aHostName, 
                            CERTCertificate *aServerCert, CERTCertificate *aClientCert);
  nsresult HasRememberedDecision(const nsACString & aHostName, 
                                 CERTCertificate *aServerCert, 
                                 nsACString & aCertDBKey, bool *_retval);

  void ClearRememberedDecisions();
  static void ClearAllRememberedDecisions();

protected:
    mozilla::ReentrantMonitor monitor;
    nsTHashtable<nsClientAuthRememberEntry> mSettingsTable;

    void RemoveAllFromMemory();
    nsresult AddEntryToList(const nsACString &host, 
                            const nsACString &server_fingerprint,
                            const nsACString &db_key);
};

#endif
