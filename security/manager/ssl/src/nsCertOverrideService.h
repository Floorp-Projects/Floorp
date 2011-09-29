/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kengert@redhat.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  PRInt32 mPort;
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
class nsCertOverrideEntry : public PLDHashEntryHdr
{
  public:
    // Hash methods
    typedef const char* KeyType;
    typedef const char* KeyTypePointer;

    // do nothing with aHost - we require mHead to be set before we're live!
    nsCertOverrideEntry(KeyTypePointer aHostWithPortUTF8)
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
      // pass nsnull
      return PL_DHashStringKey(nsnull, aKey);
    }

    enum { ALLOW_MEMMOVE = PR_FALSE };

    // get methods
    inline const nsCString &HostWithPort() const { return mHostWithPort; }

    inline KeyTypePointer HostWithPortPtr() const
    {
      return mHostWithPort.get();
    }

    nsCertOverride mSettings;
    nsCString mHostWithPort;
};

class nsCertOverrideService : public nsICertOverrideService
                            , public nsIObserver
                            , public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICERTOVERRIDESERVICE
  NS_DECL_NSIOBSERVER

  nsCertOverrideService();
  ~nsCertOverrideService();

  nsresult Init();
  void RemoveAllTemporaryOverrides();

  typedef void 
  (*PR_CALLBACK CertOverrideEnumerator)(const nsCertOverride &aSettings,
                                        void *aUserData);

  // aCert == null: return all overrides
  // aCert != null: return overrides that match the given cert
  nsresult EnumerateCertOverrides(nsIX509Cert *aCert,
                                  CertOverrideEnumerator enumerator,
                                  void *aUserData);

    // Concates host name and the port number. If the port number is -1 then
    // port 443 is automatically used. This method ensures there is always a port
    // number separated with colon.
    static void GetHostWithPort(const nsACString & aHostName, PRInt32 aPort, nsACString& _retval);

protected:
    mozilla::ReentrantMonitor monitor;
    nsCOMPtr<nsIFile> mSettingsFile;
    nsTHashtable<nsCertOverrideEntry> mSettingsTable;

    SECOidTag mOidTagForStoringNewHashes;
    nsCString mDottedOidForStoringNewHashes;

    void RemoveAllFromMemory();
    nsresult Read();
    nsresult Write();
    nsresult AddEntryToList(const nsACString &host, PRInt32 port,
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
