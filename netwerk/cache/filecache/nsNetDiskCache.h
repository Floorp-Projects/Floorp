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

/*
 * This file is part of filecache implementation.
 *
 * nsNetDiskCache is the main disk cache module that will create 
 * the cache database, and then store and retrieve nsDiskCacheRecord 
 * objects from it. It also contains some basic error recovery procedure.
 */

#ifndef __gen_nsNetDiskCache_h__
#define __gen_nsNetDiskCache_h__

#include "nsINetDataDiskCache.h"
#include "nsNetDiskCacheCID.h"
#include "nsCOMPtr.h"
#include "nsDBAccessor.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

class nsIURI; /* forward decl */
class nsICachedNetData; /* forward decl */
class nsISimpleEnumerator; /* forward decl */
class nsIFile; /* forward decl */

// 2 : cache that never deleted entires
// 3 : Evict works. Number of records and entries matched up and limited.
// 4 : Detection of corrupt cache works
#define CURRENT_CACHE_VERSION   4

/* starting interface:    nsNetDiskCache */

class nsNetDiskCache : public nsINetDataDiskCache, public nsIObserver, public nsSupportsWeakReference {
  public: 

  NS_DECL_ISUPPORTS
  NS_DECL_NSINETDATACACHE
  NS_DECL_NSINETDATADISKCACHE
  NS_DECL_NSIOBSERVER

  NS_IMETHOD Init(void) ;

  nsNetDiskCache() ;
  virtual ~nsNetDiskCache() ;

  protected:
  NS_IMETHOD ShutdownDB();
  NS_IMETHOD InitDB(void) ;
  NS_IMETHOD CreateDir(nsIFile* dir_spec) ;
  NS_IMETHOD GetSizeEntry(void) ;
  NS_IMETHOD SetSizeEntry(void) ;
  NS_IMETHOD SetSizeEntryExplicit(PRUint32 numEntries, PRUint32 storageInUse) ;

  NS_IMETHOD RenameCacheSubDirs(void) ;
  NS_IMETHOD DBRecovery(void) ;

  private:
  NS_IMETHODIMP InitCacheFolder();
		
  PRBool    fixCacheVersion (const char *cache_dir, unsigned version);
  void      removeAllFiles  (const char *dir, const char *tagFile = NULL);

  PRBool 			                mEnabled ;
  PRUint32 			                mNumEntries ;
  nsCOMPtr<nsINetDataCache> 	    mpNextCache ;
  nsCOMPtr<nsIFile>   	        	mDiskCacheFolder ;
  nsCOMPtr<nsIFile>            		mDBFile ;

  PRUint32                          mMaxEntries ;
  PRUint32                          mStorageInUse ;
  nsIDBAccessor*                    mDB ;

  // this is used to indicate a db corruption 
  PRBool                            mDBCorrupted ;

  friend class nsDiskCacheRecord ;
  friend class nsDiskCacheRecordChannel ;
  friend class nsDBEnumerator ;
} ;

#endif /* __gen_nsNetDiskCache_h__ */
