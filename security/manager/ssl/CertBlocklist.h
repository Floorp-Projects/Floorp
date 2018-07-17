/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CertBlocklist_h
#define CertBlocklist_h

#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsICertBlocklist.h"
#include "nsIOutputStream.h"
#include "nsIX509CertDB.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "pkix/Input.h"

#define NS_CERT_BLOCKLIST_CID \
{0x11aefd53, 0x2fbb, 0x4c92, {0xa0, 0xc1, 0x05, 0x32, 0x12, 0xae, 0x42, 0xd0} }

enum CertBlocklistItemMechanism {
  BlockByIssuerAndSerial,
  BlockBySubjectAndPubKey
};

enum CertBlocklistItemState {
  CertNewFromBlocklist,
  CertOldFromLocalCache
};

class CertBlocklistItem
{
public:
  CertBlocklistItem(const uint8_t*  DNData, size_t DNLength,
                    const uint8_t* otherData, size_t otherLength,
                    CertBlocklistItemMechanism itemMechanism);
  CertBlocklistItem(const CertBlocklistItem& aItem);
  ~CertBlocklistItem();
  nsresult ToBase64(nsACString& b64IssuerOut, nsACString& b64SerialOut);
  bool operator==(const CertBlocklistItem& aItem) const;
  uint32_t Hash() const;
  bool mIsCurrent;
  CertBlocklistItemMechanism mItemMechanism;

private:
  size_t mDNLength;
  uint8_t* mDNData;
  size_t mOtherLength;
  uint8_t* mOtherData;
};

typedef nsGenericHashKey<CertBlocklistItem> BlocklistItemKey;
typedef nsTHashtable<BlocklistItemKey> BlocklistTable;
typedef nsTHashtable<nsCStringHashKey> BlocklistStringSet;
typedef nsClassHashtable<nsCStringHashKey, BlocklistStringSet> IssuerTable;

class CertBlocklist : public nsICertBlocklist
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICERTBLOCKLIST
  CertBlocklist();
  nsresult Init();

private:
  BlocklistTable mBlocklist;
  nsresult AddRevokedCertInternal(const nsACString& aEncodedDN,
                                  const nsACString& aEncodedOther,
                                  CertBlocklistItemMechanism aMechanism,
                                  CertBlocklistItemState aItemState,
                                  mozilla::MutexAutoLock& /*proofOfLock*/);
  mozilla::Mutex mMutex;
  bool mModified;
  bool mBackingFileIsInitialized;
  // call EnsureBackingFileInitialized before operations that read or
  // modify CertBlocklist data
  nsresult EnsureBackingFileInitialized(mozilla::MutexAutoLock& lock);
  nsCOMPtr<nsIFile> mBackingFile;

protected:
  static void PreferenceChanged(const char* aPref, CertBlocklist* aBlocklist);
  static uint32_t sLastBlocklistUpdate;
  static uint32_t sLastKintoUpdate;
  static uint32_t sMaxStaleness;
  static bool sUseAMO;
  virtual ~CertBlocklist();
};

#endif // CertBlocklist_h
