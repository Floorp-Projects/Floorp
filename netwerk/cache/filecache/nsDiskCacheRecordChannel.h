/*
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

#ifndef _ns_DiskCacheRecordChannel_h_
#define _ns_DiskCacheRecordChannel_h_

#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsDiskCacheRecord.h"

/*
 * This class is plagiarized from nsMemCacheChannel
 */

class nsDiskCacheRecordChannel : public nsIChannel
{
  public:

  nsDiskCacheRecordChannel(nsDiskCacheRecord *aRecord, nsILoadGroup *aLoadGroup);
  virtual ~nsDiskCacheRecordChannel() ;

  // Declare nsISupports methods
  NS_DECL_ISUPPORTS

  // Declare nsIRequest methods
  NS_DECL_NSIREQUEST

  // Declare nsIChannel methods
  NS_DECL_NSICHANNEL

  nsresult Init(void) ;

  private:

  nsresult NotifyStorageInUse(PRInt32 aBytesUsed) ;

  nsCOMPtr<nsDiskCacheRecord>           mRecord ;
  nsCOMPtr<nsILoadGroup>                mLoadGroup ;
  nsCOMPtr<nsISupports>                 mOwner ;
  nsCOMPtr<nsIChannel>                  mFileTransport ;

  friend class WriteStreamWrapper ;
} ;

#endif // _ns_DiskCacheRecordChannel_h_

