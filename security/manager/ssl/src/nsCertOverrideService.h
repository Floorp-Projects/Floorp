/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NSCERTOVERRIDESERVICE_H__
#define __NSCERTOVERRIDESERVICE_H__

#include "mozilla/ReentrantMonitor.h"
#include "nsICertOverrideService.h"
#include "nsTHashtable.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsIFile.h"
#include "secoidt.h"
#include "nsWeakReference.h"
#include "mozilla/Attributes.h"

class nsCertOverride
{
public:

  enum OverrideBits { ob_None=0, ob_Untrusted=1, ob_Mismatch=2,
                      ob_Time_error=4 };

  nsCertOverride()
  :mPort(-1)
  ,mOverrideBits(ob_None)
  {
  }

  nsCertOverride(const nsCertOverride &other)
  {
    this->operator=(other);
  }

  nsCertOverride &operator=(const nsCertOverride &other)
  {
    mAsciiHost = other.mAsciiHost;
    mPort = other.mPort;
    mIsTemporary = other.mIsTemporary;
    mFingerprintAlgOID = other.mFingerprintAlgOID;
    mFingerprint = other.mFingerprint;
    mOverrideBits = other.mOverrideBits;
    mDBKey = other.mDBKey;
    mCert = other.mCert;
    return *this;
  }

  nsCString mAsciiHost;
  int32_t mPort;
  bool mIsTemporary; // true: session only, false: stored on disk
  nsCString mFingerprint;
  nsCString mFingerprintAlgOID;
  OverrideBits mOverrideBits;
  nsCString mDBKey;
  nsCOMPtr <nsIX509Cert> mCert;

  static void convertBitsToString(OverrideBits ob, nsACString &str);
  static void convertStringToBits(const nsACString &str, OverrideBits &ob);
};


// hash entry class
class nsCertOverrideEntry MOZ_FINAL : public PLDHashEntryHdr
{
  public:
    // Hash methods
    typedef const char* KeyType;
    typedef const char* KeyTypePointer;

    // do nothing with aHost - we require mHead to be set before we're live!
    explicit nsCertOverrideEntry(KeyTypePointer aHostWithPortUTF8)
    {
    }

    nsCertOverrideEntry(const nsCertOverrideEntry& toCopy)
    {
      mSettings = toCopy.mSettings;
      mHostWithPort = toCopy.mHostWithPort;
    }

    ~nsCertOverrideEntry()
    {
    }

    KeyType GetKey() const
    {
      return HostWithPortPtr();
    }

    KeyTypePointer GetKeyPointer() const
    {
      return HostWithPortPtr();
    }

    bool KeyEquals(KeyTypePointer aKey) const
    {
      return !strcmp(HostWithPortPtr(), aKey);
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
    inline const nsCString &HostWithPort() const { return mHostWithPort; }

    inline KeyTypePointer HostWithPortPtr() const
    {
      return mHostWithPort.get();
    }

    nsCertOverride mSettings;
    nsCString mHostWithPort;
};

class nsCertOverrideService MOZ_FINAL : public nsICertOverrideService
                                      , public nsIObserver
                                      , public nsSupportsWeakReference
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICERTOVERRIDESERVICE
  NS_DECL_NSIOBSERVER

  nsCertOverrideService();

  nsresult Init();
  void RemoveAllTemporaryOverrides();

  typedef void 
  (*CertOverrideEnumerator)(const nsCertOverride &aSettings,
                            void *aUserData);

  // aCert == null: return all overrides
  // aCert != null: return overrides that match the given cert
  nsresult EnumerateCertOverrides(nsIX509Cert *aCert,
                                  CertOverrideEnumerator enumerator,
                                  void *aUserData);

    // Concates host name and the port number. If the port number is -1 then
    // port 443 is automatically used. This method ensures there is always a port
    // number separated with colon.
    static void GetHostWithPort(const nsACString & aHostName, int32_t aPort, nsACString& _retval);

protected:
    ~nsCertOverrideService();

    mozilla::ReentrantMonitor monitor;
    nsCOMPtr<nsIFile> mSettingsFile;
    nsTHashtable<nsCertOverrideEntry> mSettingsTable;

    SECOidTag mOidTagForStoringNewHashes;
    nsCString mDottedOidForStoringNewHashes;

    void CountPermanentOverrideTelemetry();

    void RemoveAllFromMemory();
    nsresult Read();
    nsresult Write();
    nsresult AddEntryToList(const nsACString &host, int32_t port,
                            nsIX509Cert *aCert,
                            const bool aIsTemporary,
                            const nsACString &algo_oid, 
                            const nsACString &fingerprint,
                            nsCertOverride::OverrideBits ob,
                            const nsACString &dbKey);
};

#define NS_CERTOVERRIDE_CID { /* 67ba681d-5485-4fff-952c-2ee337ffdcd6 */ \
    0x67ba681d,                                                        \
    0x5485,                                                            \
    0x4fff,                                                            \
    {0x95, 0x2c, 0x2e, 0xe3, 0x37, 0xff, 0xdc, 0xd6}                   \
  }

#endif
