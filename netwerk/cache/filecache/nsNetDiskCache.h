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

// FUR - Add overall description comment here

#ifndef __gen_nsNetDiskCache_h__
#define __gen_nsNetDiskCache_h__

#include "nsINetDataDiskCache.h"
#include "nsNetDiskCacheCID.h"
#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsDBAccessor.h"

class nsIURI; /* forward decl */
class nsICachedNetData; /* forward decl */
class nsISimpleEnumerator; /* forward decl */
class nsIFileSpec; /* forward decl */

/* starting interface:    nsNetDiskCache */

class nsNetDiskCache : public nsINetDataDiskCache {
  public: 

  NS_DECL_ISUPPORTS
  NS_DECL_NSINETDATACACHE
  NS_DECL_NSINETDATADISKCACHE

  NS_IMETHOD Init(void) ;

  nsNetDiskCache() ;
  virtual ~nsNetDiskCache() ;

  protected:

  NS_IMETHOD InitDB(void) ;
  NS_IMETHOD CreateDir(nsIFileSpec* dir_spec) ;
  NS_IMETHOD UpdateInfo(void) ;

  NS_IMETHOD RenameCacheSubDirs(void) ;
  NS_IMETHOD DBRecovery(void) ;
  NS_IMETHOD RemoveDirs(PRUint32 aNum) ;

  private:

  PRBool 			                m_Enabled ;
  PRUint32 			                m_NumEntries ;
  nsCOMPtr<nsINetDataCache> 	    m_pNextCache ;
  nsCOMPtr<nsIFileSpec>   	        m_pDiskCacheFolder ;
  nsCOMPtr<nsIFileSpec>             m_DBFile ;

  PRUint32                          m_MaxEntries ;
  PRUint32                          m_StorageInUse ;
  nsIDBAccessor*                    m_DB ;

  // this is used to indicate a db corruption
  PRInt32                           m_BaseDirNum ;

  friend class nsDiskCacheRecord ;
  friend class nsDiskCacheRecordChannel ;
} ;

#endif /* __gen_nsNetDiskCache_h__ */
