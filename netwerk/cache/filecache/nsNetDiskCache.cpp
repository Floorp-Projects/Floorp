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

#include "nsNetDiskCache.h"
#include "nscore.h"

#include "plstr.h"
#include "prprf.h"
#include "prtypes.h"
#include "prio.h"
#include "prsystem.h" // Directory Seperator
#include "plhash.h"
#include "prclist.h"
#include "prmem.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsIPref.h"
#include "mcom_db.h"
#include "nsDBEnumerator.h"

#include "nsDiskCacheRecord.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID) ;
static NS_DEFINE_CID(kDBAccessorCID, NS_DBACCESSOR_CID) ;

static const PRUint32 DISK_CACHE_SIZE_DEFAULT = 5*1024*1024 ; // 5MB
static const char * const DISK_CACHE_PREF  = "browser.cache.disk_cache_size";
static const char * const CACHE_DIR_PREF   = "browser.cache.directory";

class nsDiskCacheRecord ;

nsNetDiskCache::nsNetDiskCache() :
  m_Enabled(PR_TRUE) ,
  m_NumEntries(0) ,
  m_pNextCache(0) ,
  m_pDiskCacheFolder(0) ,
  m_StorageInUse(0) ,
  m_DB(0) ,
  m_BaseDirNum(32) 
{
  // set it to INF for now 
  m_MaxEntries = (PRUint32)-1 ;

  NS_INIT_REFCNT();

}

nsNetDiskCache::~nsNetDiskCache()
{
  printf("~nsNetDiskCache\n") ;

  NS_IF_RELEASE(m_DB) ;

  // FUR!!
  // You shouldn't rely on the value of m_BaseDirNum to diagnose whether or not
  // a cache corruption has occurred since it's possible that the app does not
  // shut down cleanly and a corrupted cache has still not been cleaned up from
  // a previous session.  My suggestion is that you pick a different scheme for
  // renaming the dirs, e.g. rename them as "trash*" and remove all directories
  // with this name pattern on shutdown.

  // FUR
  // I think that, eventually, we also want a distinguished key in the DB which
  // means "clean cache shutdown".  You clear this flag when the db is first
  // opened and set it just before the db is closed.  If the db wasn't shutdown
  // cleanly in a prior session, i.e. because the app crashed, on startup you
  // scan all the individual files in directories and look for "orphans",
  // i.e. cache files which don't have corresponding entries in the db.  That's
  // also when storage-in-use and number of entries would be recomputed.
  //
  // We don't necessarily need all this functionality immediately, though.

  if(m_BaseDirNum > 32)
    RemoveDirs(32) ;
}

NS_IMETHODIMP
nsNetDiskCache::Init(void) 
{
  nsresult rv ;

  // FUR!!
  // I really don't think prefs belong here, since that breaks modularity.  It
  // presupposes that the file cache code is embedded in the browser or some
  // other application that uses the prefs, i.e. the code might be used in a
  // standalone cache manipulation tool or, someday, in server code.  Pref
  // reading belongs at a higher level, either in the application itself or
  // possibly the I/O manager.


  // Also, Init() needs to be lazy, since folder name is not set on startup,
  // i.e. need a call to MaybeInit() at the beginning of every public method

  NS_WITH_SERVICE(nsIPref, pref, kPrefCID, &rv) ;
  if (NS_FAILED(rv)) 
    NS_ERROR("Failed to get globle preference!\n") ;

  rv = NS_NewFileSpec(getter_AddRefs(m_pDiskCacheFolder));
  if (!m_pDiskCacheFolder) {
    NS_ERROR("ERROR: Could not make a file spec.\n") ;
    return NS_ERROR_OUT_OF_MEMORY ;
  }

  if(pref) {

    /*
    rv = pref->CopyCharPref(CACHE_DIR_PREF, &tempPref) ;
    if (NS_SUCCEEDED(rv)) {
      printf("cache dir is %s\n", tempPref) ;
      m_pDiskCacheFolder->SetUnixStyleFilePath(tempPref) ;
	  PR_Free(tempPref) ;
    } else */
    {
      m_pDiskCacheFolder->SetUnixStyleFilePath("/tmp") ;
      printf("using default folder, /tmp\n") ;
    }
  }
  else {
    // temp hack for now. change later for other platform
    m_pDiskCacheFolder->SetUnixStyleFilePath("/tmp") ;
  }

  // FUR - suggest you use nsCOMPtr for m_DB - it will eliminate
  //       manual addref/release and reduce likelihood of bugs
  NS_IF_RELEASE(m_DB) ;
  m_DB = new nsDBAccessor() ;
  if(!m_DB)
    return NS_ERROR_OUT_OF_MEMORY ;
  else
    NS_ADDREF(m_DB) ;

  rv = InitDB() ;

  // try once for recovery
  if(rv == NS_ERROR_FAILURE) {
    rv = DBRecovery() ;
    return rv ;
  }

  rv = UpdateInfo() ;
  return rv ;
}

NS_IMETHODIMP
nsNetDiskCache::InitDB(void)
{
  // create cache sub directories
  nsresult rv ; 
  nsCOMPtr<nsIFileSpec> cacheSubDir;
  rv = NS_NewFileSpec(getter_AddRefs(cacheSubDir));

  // FUR - any way to avoid doing this, if it's already been done ?
  for (int i=0; i < 32; i++) {
    rv = cacheSubDir->FromFileSpec(m_pDiskCacheFolder) ;
    if(NS_FAILED(rv))
      return rv ;

    char dirName[3];
    PR_snprintf (dirName, 3, "%0.2x", i);
    cacheSubDir->AppendRelativeUnixPath (dirName) ;
    CreateDir(cacheSubDir);
  }

  NS_NewFileSpec(getter_AddRefs(m_DBFile)) ;
  // FUR - check for NS_NewFileSpec failure
  rv = m_DBFile->FromFileSpec(m_pDiskCacheFolder) ;
  if(NS_FAILED(rv))
    return rv ;

  m_DBFile->AppendRelativeUnixPath("cache.db") ;

  rv = m_DB->Init(m_DBFile) ;
  return rv ;
}

//////////////////////////////////////////////////////////////////////////
// nsISupports methods

// FUR - Suggest you use NS_IMPL_ISUPPORTS3() macro instead
NS_IMETHODIMP
nsNetDiskCache::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_ASSERTION(aInstancePtr, "no instance pointer");
  if(aIID.Equals(NS_GET_IID(nsINetDataDiskCache)) ||
     aIID.Equals(NS_GET_IID(nsINetDataCache)) ||
     aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(nsINetDataDiskCache*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else
    return NS_NOINTERFACE ;
}

NS_IMPL_ADDREF(nsNetDiskCache) ;
NS_IMPL_RELEASE(nsNetDiskCache) ;

///////////////////////////////////////////////////////////////////////////
// nsINetDataCache Method 

NS_IMETHODIMP
nsNetDiskCache::GetDescription(PRUnichar* *aDescription) 
{
  nsAutoString description("Disk Cache") ;
  *aDescription = description.ToNewUnicode() ;
  if(!*aDescription)
    return NS_ERROR_OUT_OF_MEMORY ;

  return NS_OK ;
}

/* don't alloc mem for nsICachedNetData. 
 * RecordID is generated using the same scheme in nsCacheDiskData,
 * see GetCachedNetData() for detail.
 */
NS_IMETHODIMP
nsNetDiskCache::Contains(const char* key, PRUint32 length, PRBool *_retval) 
{
  *_retval = PR_FALSE ;

  NS_ASSERTION(m_DB, "no db.") ;

  PRInt32 id = 0 ;
  m_DB->GetID(key, length, &id) ;
  // FUR - Check for GetID failure ?

  void* info = 0 ;
  PRUint32 info_size = 0 ;

  nsresult rv = m_DB->Get(id, &info, &info_size) ;
  if(NS_SUCCEEDED(rv) && info) 
    *_retval = PR_TRUE ;
        
  return NS_OK ;
}

/* regardless if it's cached or not, a copy of nsNetDiskCache would 
 * always be returned. so release it appropriately. 
 * if mem alloced, updata m_NumEntries also.
 * for now, the new nsCachedNetData is not written into db yet since
 * we have nothing to write.
 */
NS_IMETHODIMP
nsNetDiskCache::GetCachedNetData(const char* key, PRUint32 length, nsINetDataCacheRecord **_retval) 
{
  NS_ASSERTION(m_DB, "no db.") ;

  nsresult rv = 0 ;
  if (!_retval)
    return NS_ERROR_NULL_POINTER ;

  *_retval = nsnull ;

  PRInt32 id = 0 ;
  m_DB->GetID(key, length, &id) ;
  // FUR - Check for GetID failure ?

  // construct an empty record
  nsDiskCacheRecord* newRecord = new nsDiskCacheRecord(m_DB, this) ;
  if(!newRecord) 
    return NS_ERROR_OUT_OF_MEMORY ;

  rv = newRecord->Init(key, length) ;
  if(NS_FAILED(rv)) {
    delete newRecord ;
    return rv ;
  }

  NS_ADDREF(newRecord) ; // addref for _retval 
  *_retval = (nsINetDataCacheRecord*) newRecord ;
  
  void* info = 0 ;
  PRUint32 info_size = 0 ;

  rv = m_DB->Get(id, &info, &info_size) ;
  if(NS_SUCCEEDED(rv) && info) {

    nsresult r1 ;
    r1 = newRecord->RetrieveInfo(info, info_size) ;

    // FUR!! need to release and return error if RetrieveInfo() fails
    if(NS_SUCCEEDED(rv)) 
      return NS_OK ;
    else 
      return r1;
    
  } else if (NS_SUCCEEDED(rv) && !info) {
    // this is a new record. 
    m_NumEntries ++ ;
    return NS_OK ;
  } else 
    return rv ;
}

/* get an nsICachedNetData, mem needs to be de-alloced if not found. */
NS_IMETHODIMP
nsNetDiskCache::GetCachedNetDataByID(PRInt32 RecordID, nsINetDataCacheRecord **_retval) 
{
  NS_ASSERTION(m_DB, "no db.") ;

  if (!_retval)
    return NS_ERROR_NULL_POINTER ;

  *_retval = nsnull ;
  
  nsresult rv ;

  void* info = 0 ;
  PRUint32 info_size = 0 ;

  rv = m_DB->Get(RecordID, &info, &info_size) ;
  if(NS_SUCCEEDED(rv) && info) {

    // construct an empty record if only found in db
    nsDiskCacheRecord* newRecord = new nsDiskCacheRecord(m_DB, this) ;
    if(!newRecord) 
      return NS_ERROR_OUT_OF_MEMORY ;

    NS_ADDREF(newRecord) ; // addref for _retval
    rv = newRecord->RetrieveInfo(info, info_size) ;

    if(NS_SUCCEEDED(rv)) {
      *_retval = (nsINetDataCacheRecord*) newRecord ;
      return NS_OK ;
    } 
    else {
      // bad record, I guess
      NS_RELEASE(newRecord) ; // release if bad things happen
      return rv ;
    }
  } else {
    NS_ERROR("Error: RecordID not in DB\n") ;
    return rv ;
  }
}

NS_IMETHODIMP
nsNetDiskCache::GetEnabled(PRBool *aEnabled) 
{
  *aEnabled = m_Enabled ;
  return NS_OK ;
}

NS_IMETHODIMP
nsNetDiskCache::SetEnabled(PRBool aEnabled) 
{
  m_Enabled = aEnabled ;
  return NS_OK ;
}

NS_IMETHODIMP
nsNetDiskCache::GetFlags(PRUint32 *aFlags) 
{
  *aFlags = FILE_PER_URL_CACHE;
  return NS_OK ;
}

NS_IMETHODIMP
nsNetDiskCache::GetNumEntries(PRUint32 *aNumEntries) 
{
  *aNumEntries = m_NumEntries ;
  return NS_OK ;
}

NS_IMETHODIMP
nsNetDiskCache::GetMaxEntries(PRUint32 *aMaxEntries)
{
  *aMaxEntries = m_MaxEntries ;
  return NS_OK ;
}

NS_IMETHODIMP
nsNetDiskCache::NewCacheEntryIterator(nsISimpleEnumerator **_retval) 
{
  if(!_retval)
    return NS_ERROR_NULL_POINTER ;

  *_retval = nsnull ;

  nsISimpleEnumerator* enumerator = new nsDBEnumerator(m_DB, this) ;
  if(enumerator) {
    NS_ADDREF(enumerator) ;
    *_retval = enumerator ;
    return NS_OK ;
  }
  else 
    return NS_ERROR_OUT_OF_MEMORY ;
}

NS_IMETHODIMP
nsNetDiskCache::GetNextCache(nsINetDataCache * *aNextCache)
{
  if(!aNextCache)
    return NS_ERROR_NULL_POINTER ;

  *aNextCache = m_pNextCache ;
  return NS_OK ;
}

NS_IMETHODIMP
nsNetDiskCache::SetNextCache(nsINetDataCache *aNextCache)
{
  m_pNextCache = aNextCache ;
  return NS_OK ;
}

// db size can always be measured at the last minute. Since it's hard
// to know before hand. 
NS_IMETHODIMP
nsNetDiskCache::GetStorageInUse(PRUint32 *aStorageInUse)
{
  PRUint32 total_size = m_StorageInUse;

  // FUR!!
  // GetStorageInUse() can be called hundreds of times per second, i.e. every
  // time a buffer of data is written to the cache, so we can't afford to stat
  // the db file on every call.  I would suggest caching the size of the db and
  // invalidating that cached value every time a record is written to the db,
  // or even every ten written records.

  // add the size of the db.
  //  m_DBFile->GetFileSize(&len) ;
  //  total_size += len ;

  // we need size in kB
  total_size = total_size >> 10 ;

  *aStorageInUse = total_size ;
  return NS_OK ;
}

/*
 * The whole cache dirs can be whiped clean since all the cache 
 * files are resides in seperate hashed dirs. It's safe to do so.
 */
 
NS_IMETHODIMP
nsNetDiskCache::RemoveAll(void)
{
  nsresult rv = RemoveDirs(0) ;
  if(NS_FAILED(rv))
    return rv ;

  // don't forget the db file itself
  m_DB->Shutdown() ;
  nsFileSpec dbfile ;
  m_DBFile->GetFileSpec(&dbfile) ;
  dbfile.Delete(PR_TRUE) ;

  // reinitilize
  rv = InitDB() ;
  if(NS_FAILED(rv))
    return rv ;

  rv = UpdateInfo() ;
  return rv ;
}

//////////////////////////////////////////////////////////////////
// nsINetDataDiskCache methods

NS_IMETHODIMP
nsNetDiskCache::GetDiskCacheFolder(nsIFileSpec * *aDiskCacheFolder)
{
  *aDiskCacheFolder = m_pDiskCacheFolder ;
  NS_ADDREF(*aDiskCacheFolder) ;
  return NS_OK ;
}

NS_IMETHODIMP
nsNetDiskCache::SetDiskCacheFolder(nsIFileSpec * aDiskCacheFolder)
{
  char *newfolder, *oldfolder ;
  m_pDiskCacheFolder->GetNativePath(&oldfolder) ;
  aDiskCacheFolder->GetNativePath(&newfolder) ;

  if(PL_strcmp(newfolder, oldfolder) == 0) {
    m_pDiskCacheFolder = aDiskCacheFolder ;

    // should we do this? 
    // FUR - no
    nsresult rv = RemoveAll() ;
    return rv ;
  }
  else 
    // FUR
    // Need to blow away old cache, build new one
    return NS_OK ;
}

//////////////////////////////////////////////////////////////////
// nsNetDiskCache methods

// create a directory (recursively)
NS_IMETHODIMP
nsNetDiskCache::CreateDir(nsIFileSpec* dir_spec) 
{
  PRBool does_exist ;
  nsCOMPtr<nsIFileSpec> p_spec ;

  dir_spec->Exists(&does_exist) ;
  if(does_exist) 
    return NS_OK ;

  dir_spec->GetParent(getter_AddRefs(p_spec)) ;
  // FUR - check return value
  p_spec->Exists(&does_exist) ;
  if(!does_exist) {
    CreateDir(p_spec) ;
    dir_spec->CreateDir() ;
    // FUR - check return value
  }
  else {
    dir_spec->CreateDir() ;
    // FUR - check return value
  }
  
  return NS_OK ;
}

// FUR!!
// We can't afford to make a *separate* pass over the whole db on every
// startup, just to figure out m_NumEntries and m_StorageInUse.  (This is a
// several second operation on a large db).  We'll likely need to store
// distinguished keys in the db that contain these values and update them
// incrementally, except when failure to shut down the db cleanly is detected.

// this will walk through db and update m_NumEntries and m_StorageInUse
NS_IMETHODIMP
nsNetDiskCache::UpdateInfo(void)
{
  // count num of entries in db
//  NS_ADDREF(this) ; // addref before assign to a nsCOMPtr. 
  nsISimpleEnumerator* dbEnumerator = new nsDBEnumerator(m_DB, this) ;
  if(dbEnumerator)
    NS_ADDREF(dbEnumerator) ;
  else
    return NS_ERROR_FAILURE ;

  PRUint32 numEntries = 0, storageInUse = 0, len = 0 ;
  PRBool more = PR_FALSE ;

  do {
    dbEnumerator->HasMoreElements(&more) ;
    if(more) {
      // update entry number
      numEntries++ ;

      // update storage in use
      nsINetDataCacheRecord* record ;
      dbEnumerator->GetNext((nsISupports**)&record) ;
      record->GetStoredContentLength(&len) ;
      storageInUse += len ;
      NS_IF_RELEASE(record) ;
    }
  } while (more) ;

  NS_IF_RELEASE(dbEnumerator) ;

  m_NumEntries = numEntries ;
  m_StorageInUse = storageInUse ;

  printf(" m_NumEntries = %d, size is %d.\n", m_NumEntries, m_StorageInUse) ;

  return NS_OK ;
}

// this routine will add m_BaseDirNum to current CacheSubDir names. 
// e.g. 00->20, 1f->5f. and update the m_BaseDirNum to another 32. 
// the idea is as long as we remember the base number,
// we know how many dirs needs to be removed during shutdown period
// it will be from 0x20 to m_BaseDirNum. 
// also, we assume that this operation will not be performed 3 times more
// within a single session. it is part of scavenging routine. 

NS_IMETHODIMP
nsNetDiskCache::RenameCacheSubDirs(void) 
{
  nsCOMPtr<nsIFileSpec> cacheSubDir;
  nsresult rv = NS_NewFileSpec(getter_AddRefs(cacheSubDir)) ;

  for (int i=0; i < 32; i++) {
    rv = cacheSubDir->FromFileSpec(m_pDiskCacheFolder) ;
    if(NS_FAILED(rv))
      return rv ;

    char dirName[3];
    PR_snprintf(dirName, 3, "%0.2x", i) ;
    cacheSubDir->AppendRelativeUnixPath(dirName) ;

    // re-name the directory
    PR_snprintf(dirName, 3, "%0.2x", i+m_BaseDirNum) ;
    rv = cacheSubDir->Rename(dirName) ;
    if(NS_FAILED(rv))
      return NS_ERROR_FAILURE ;
  }

  // update m_BaseDirNum 
  m_BaseDirNum += 32 ;

  return NS_OK ;
}

// this routine will be called everytime we have a db corruption. 
NS_IMETHODIMP
nsNetDiskCache::DBRecovery(void)
{
  nsresult rv = RenameCacheSubDirs() ;
  if(NS_FAILED(rv))
    return rv ;

  // remove corrupted db file
  rv = m_DB->Shutdown() ;
  
  // FUR!!
  // You shouldn't return if this fails.  Otherwise, it might prevent db deletion
  if(NS_FAILED(rv))
    return rv ;

  nsFileSpec dbfile ;
  m_DBFile->GetFileSpec(&dbfile) ;
  dbfile.Delete(PR_TRUE) ;

  // make sure it's not there any more
  PRBool exists = dbfile.Exists() ;
  if(exists) {
    NS_ERROR("can't remove old db.") ;
    return NS_ERROR_FAILURE ;
  }

  // reinitilize
  rv = InitDB() ;
  if(NS_FAILED(rv))
    return rv ;

  rv = UpdateInfo() ;
  return rv ;
}

// this routine is used by dtor and RemoveAll() to clean up dirs.
// All directory named from aNum - m_BasedDirNum will be deleted. 
NS_IMETHODIMP
nsNetDiskCache::RemoveDirs(PRUint32 aNum)
{
  nsCOMPtr<nsIFileSpec> cacheSubDir;
  nsresult rv = NS_NewFileSpec(getter_AddRefs(cacheSubDir));
  if(NS_FAILED(rv))
    return NS_ERROR_FAILURE ;

  for (int i=aNum; i < m_BaseDirNum; i++) {
    cacheSubDir->FromFileSpec(m_pDiskCacheFolder) ;

    char dirName[3];
    PR_snprintf (dirName, 3, "%0.2x", i);
    cacheSubDir->AppendRelativeUnixPath (dirName) ;

    nsFileSpec subdir ;
    cacheSubDir->GetFileSpec(&subdir) ;

    for(nsDirectoryIterator di(subdir, PR_FALSE); di.Exists(); di++) {
      di.Spec().Delete(PR_TRUE) ;
    }

    subdir.Delete(PR_FALSE) ; // recursive delete
  }

  return NS_OK ;
}
