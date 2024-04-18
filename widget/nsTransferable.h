/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTransferable_h__
#define nsTransferable_h__

#include "nsICookieJarSettings.h"
#include "nsIFormatConverter.h"
#include "nsITransferable.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIPrincipal.h"
#include "nsIReferrerInfo.h"
#include "prio.h"
#include "mozilla/Maybe.h"

class nsIMutableArray;

//
// DataStruct
//
// Holds a flavor (a mime type) that describes the data and the associated data.
//
struct DataStruct {
  explicit DataStruct(const char* aFlavor)
      : mCacheFD(nullptr), mFlavor(aFlavor) {}
  DataStruct(DataStruct&& aRHS);
  ~DataStruct();

  const nsCString& GetFlavor() const { return mFlavor; }
  void SetData(nsISupports* aData, bool aIsPrivateData);
  void GetData(nsISupports** aData);
  void ClearData();
  bool IsDataAvailable() const { return mData || mCacheFD; }

 protected:
  enum {
    // The size of data over which we write the data to disk rather than
    // keep it around in memory.
    kLargeDatasetSize = 1000000  // 1 million bytes
  };

  nsresult WriteCache(void* aData, uint32_t aDataLen);
  nsresult ReadCache(nsISupports** aData);

  // mData OR mCacheFD should be used, not both.
  nsCOMPtr<nsISupports> mData;  // OWNER - some varient of primitive wrapper
  PRFileDesc* mCacheFD;
  const nsCString mFlavor;

 private:
  DataStruct(const DataStruct&) = delete;
  DataStruct& operator=(const DataStruct&) = delete;
};

/**
 * XP Transferable wrapper
 */

class nsTransferable : public nsITransferable {
 public:
  nsTransferable();

  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSITRANSFERABLE

 protected:
  virtual ~nsTransferable();

  // Get flavors w/out converter
  void GetTransferDataFlavors(nsTArray<nsCString>& aFlavors);

  // Find index for data with the matching flavor in mDataArray.
  mozilla::Maybe<size_t> FindDataFlavor(const char* aFlavor);

  nsTArray<DataStruct> mDataArray;
  nsCOMPtr<nsIFormatConverter> mFormatConv;
  bool mPrivateData;
  nsCOMPtr<nsIPrincipal> mDataPrincipal;
  nsContentPolicyType mContentPolicyType;
  nsCOMPtr<nsICookieJarSettings> mCookieJarSettings;
  nsCOMPtr<nsIReferrerInfo> mReferrerInfo;
#if DEBUG
  bool mInitialized;
#endif
};

#endif  // nsTransferable_h__
