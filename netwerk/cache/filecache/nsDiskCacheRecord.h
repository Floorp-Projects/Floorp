/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator.
 * 
 * The Initial Developer of the Original Code is Intel Corp.
 * Portions created by Intel Corp. are
 * Copyright (C) 1999, 1999 Intel Corp.  All
 * Rights Reserved.
 * 
 * Contributor(s): Yixiong Zou <yixiong.zou@intel.com>
 *                 Carl Wong <carl.wong@intel.com>
 */

#ifndef _NET_CACHEDDISKDATA_H_
#define _NET_CACHEDDISKDATA_H_

#include "nsINetDataCacheRecord.h"
#include "nsCOMPtr.h"
#include "nsIDBAccessor.h"
#include "prtypes.h"
#include "nsILoadGroup.h"
#include "nsIChannel.h" 
#include "nsNetDiskCache.h"

class WriteStreamWrapper;

class nsDiskCacheRecord : public nsINetDataCacheRecord
{
  friend WriteStreamWrapper;

  public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSINETDATACACHERECORD

  protected: 

  nsDiskCacheRecord(nsIDBAccessor* db, nsNetDiskCache* aCache) ;
  virtual ~nsDiskCacheRecord() ;

  NS_IMETHOD RetrieveInfo(void* aInfo, PRUint32 aInfoLength) ;
  NS_IMETHOD Init(const char* key, PRUint32 length, PRInt32 ID) ;

  nsresult GenInfo(void) ;

  // Notification to us that the cache file was just written to.
  // This is used to update the nsIFile (mFile) that we hold
  // after we wrote new data to the file. This is needed because
  // if we write to the file and the filesize/date has changed,
  // we need to invalidate the mFile's stat cache.
  NS_IMETHOD WriteComplete();

  // The mFile we store might be out of date. Atlease
  // we know that our implementation of nsIFile stores
  // stat data in a cache. We need to invalidate that cache
  // at critical points or atleast when we know we changed
  // file info.
  NS_IMETHOD UpdateFileInfo();

  private:

  char*                     mKey ;
  PRUint32                  mKeyLength ;
  PRInt32                   mRecordID ;
  char*                     mMetaData ;
  PRUint32                  mMetaDataLength ;
  nsCOMPtr<nsIFile>    		mFile ;
  nsCOMPtr<nsIDBAccessor>            mDB ; 
  void*                     mInfo ;
  PRUint32                  mInfoSize ;
  PRUint32                  mNumChannels ;
  nsNetDiskCache*           mDiskCache ;

  friend class nsDiskCacheRecordChannel ;
  friend class nsDBEnumerator ;
  friend class nsNetDiskCache ;
} ;

#endif // _NET_CACHEDDISKDATA_H_
