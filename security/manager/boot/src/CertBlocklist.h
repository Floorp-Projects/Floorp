/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CertBlocklist_h
#define CertBlocklist_h

#include "mozilla/Mutex.h"
#include "nsClassHashtable.h"
#include "nsCOMPtr.h"
#include "nsICertBlocklist.h"
#include "nsIOutputStream.h"
#include "nsTHashtable.h"
#include "nsIX509CertDB.h"
#include "pkix/Input.h"

#define NS_CERT_BLOCKLIST_CID \
{0x11aefd53, 0x2fbb, 0x4c92, {0xa0, 0xc1, 0x05, 0x32, 0x12, 0xae, 0x42, 0xd0} }

enum CertBlocklistItemState {
  CertNewFromBlocklist,
  CertOldFromLocalCache
};

class CertBlocklistItem
{
public:
  CertBlocklistItem(mozilla::pkix::Input aIssuer, mozilla::pkix::Input aSerial);
  CertBlocklistItem(const CertBlocklistItem& aItem);
  ~CertBlocklistItem();
  nsresult ToBase64(nsACString& b64IssuerOut, nsACString& b64SerialOut);
  bool operator==(const CertBlocklistItem& aItem) const;
  uint32_t Hash() const;
  bool mIsCurrent;

private:
  mozilla::pkix::Input mIssuer;
  uint8_t* mIssuerData;
  mozilla::pkix::Input mSerial;
  uint8_t* mSerialData;
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
  nsresult AddRevokedCertInternal(const char* aIssuer,
                                  const char* aSerial,
                                  CertBlocklistItemState aItemState,
                                  mozilla::MutexAutoLock& /*proofOfLock*/);
  mozilla::Mutex mMutex;
  bool mModified;
  nsCOMPtr<nsIFile> mBackingFile;

protected:
  virtual ~CertBlocklist();
};

#endif // CertBlocklist_h
