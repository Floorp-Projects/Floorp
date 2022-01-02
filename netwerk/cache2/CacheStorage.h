/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheStorage__h__
#define CacheStorage__h__

#include "nsICacheStorage.h"
#include "CacheEntry.h"
#include "LoadContextInfo.h"

#include "nsRefPtrHashtable.h"
#include "nsThreadUtils.h"
#include "nsCOMPtr.h"
#include "nsILoadContextInfo.h"

class nsIURI;

namespace mozilla {
namespace net {

// This dance is needed to make CacheEntryTable declarable-only in headers
// w/o exporting CacheEntry.h file to make nsNetModule.cpp compilable.
using TCacheEntryTable = nsRefPtrHashtable<nsCStringHashKey, CacheEntry>;
class CacheEntryTable : public TCacheEntryTable {
 public:
  enum EType { MEMORY_ONLY, ALL_ENTRIES };

  explicit CacheEntryTable(EType aType) : mType(aType) {}
  EType Type() const { return mType; }

 private:
  EType const mType;
  CacheEntryTable() = delete;
};

class CacheStorage : public nsICacheStorage {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICACHESTORAGE

 public:
  CacheStorage(nsILoadContextInfo* aInfo, bool aAllowDisk, bool aSkipSizeCheck,
               bool aPinning);

 protected:
  virtual ~CacheStorage() = default;

  RefPtr<LoadContextInfo> mLoadContextInfo;
  bool mWriteToDisk : 1;
  bool mSkipSizeCheck : 1;
  bool mPinning : 1;

 public:
  nsILoadContextInfo* LoadInfo() const { return mLoadContextInfo; }
  bool WriteToDisk() const {
    return mWriteToDisk &&
           (!mLoadContextInfo || !mLoadContextInfo->IsPrivate());
  }
  bool SkipSizeCheck() const { return mSkipSizeCheck; }
  bool Pinning() const { return mPinning; }
};

}  // namespace net
}  // namespace mozilla

#endif
