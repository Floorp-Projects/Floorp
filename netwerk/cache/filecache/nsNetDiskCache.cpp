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
#include "prlog.h"

#include "nsIObserverService.h"

#include "nsIPref.h"
#include "mcom_db.h"
#include "nsDBEnumerator.h"

#include "nsDiskCacheRecord.h"
#include "netCore.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsString.h"
#include "nsReadableUtils.h"

#if !defined(IS_LITTLE_ENDIAN) && !defined(IS_BIG_ENDIAN)
ERROR! Must have a byte order
#endif

#include "nsXPIDLString.h" 

#ifdef IS_LITTLE_ENDIAN
#define COPY_INT32(_a,_b)  memcpy(_a, _b, sizeof(int32))
#else
#define COPY_INT32(_a,_b)  /* swap */                   \
    do {                                                \
    ((char *)(_a))[0] = ((char *)(_b))[3];              \
    ((char *)(_a))[1] = ((char *)(_b))[2];              \
    ((char *)(_a))[2] = ((char *)(_b))[1];              \
    ((char *)(_a))[3] = ((char *)(_b))[0];              \
    } while(0)
#endif

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID) ;
static NS_DEFINE_CID(kDBAccessorCID, NS_DBACCESSOR_CID) ;

static const PRUint32 DISK_CACHE_SIZE_DEFAULT = 5*1024*1024 ; // 5MB
static const char * const DISK_CACHE_SIZE_PREF  = "browser.cache.disk_cache_size";
static const char * const CACHE_DIR_PREF   = "browser.cache.directory";
static const char * const CACHE_ENABLE_PREF   = "browser.cache.disk.enable";

// This number should be one less than MAX_DISK_CACHE_ENTRIES
// defined in nsCacheManager.cpp
#define MAX_DISK_CACHE_RECORDS   512


static int PR_CALLBACK folderChanged(const char *pref, void *closure)
{
	nsresult rv;
	nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
	if ( NS_FAILED (rv ) )
		return rv;
			
	nsCOMPtr<nsILocalFile> cacheFolder;
    rv = prefs->GetFileXPref(CACHE_DIR_PREF, getter_AddRefs( cacheFolder ));
    if (NS_FAILED(rv)) return rv;
  
	return ( (nsNetDiskCache*)closure )->SetDiskCacheFolder( cacheFolder );	
}

static int PR_CALLBACK enableChanged(const char *pref, void *closure)
{
	nsresult rv;
	nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
	if ( NS_FAILED (rv ) )
		return rv;
		
	PRBool enabled = PR_FALSE;
	rv = prefs->GetBoolPref(CACHE_ENABLE_PREF, &enabled   );
	if ( NS_FAILED( rv ) )
		return rv;
		
	return ( (nsNetDiskCache*)closure )->SetEnabled( enabled );	
}

class nsDiskCacheRecord ;

nsNetDiskCache::nsNetDiskCache() :
  mEnabled(PR_FALSE) ,
  mNumEntries(0) ,
  mpNextCache(0) ,
  mDiskCacheFolder(0) ,
  mStorageInUse(0) ,
  mDB(0) ,
  mDBCorrupted(PR_FALSE) 
{
  mMaxEntries = MAX_DISK_CACHE_RECORDS ;

  NS_INIT_REFCNT();

}

nsNetDiskCache::~nsNetDiskCache()
{
  ShutdownDB();

  nsresult rv = NS_OK;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
	if ( NS_SUCCEEDED (rv ) )
	{
		 prefs->UnregisterCallback( CACHE_DIR_PREF, folderChanged, this ); 
		 prefs->UnregisterCallback( CACHE_ENABLE_PREF, enableChanged, this ); 
	}
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


  if(mDBCorrupted) {
    
    nsCOMPtr<nsISimpleEnumerator> directoryEnumerator;
    rv = mDiskCacheFolder->GetDirectoryEntries( getter_AddRefs( directoryEnumerator) ) ;
    if ( NS_FAILED ( rv ) )
    	return;

    nsCString trash("trash") ;
	  nsCOMPtr<nsIFile> file;
	  
	  PRBool hasMore;
    while( NS_SUCCEEDED(directoryEnumerator->HasMoreElements( &hasMore ) ) && hasMore)
    {
    	rv = directoryEnumerator->GetNext( getter_AddRefs(file) );
    	if ( NS_FAILED( rv ) )
    		return ;
    		
      char* filename;
      rv = file->GetLeafName( &filename );
      if ( NS_FAILED( rv ) )
    		return;
    	

      if( trash.CompareWithConversion( filename, PR_FALSE, 5 ) == 0)
        file->Remove( PR_TRUE );
      
      nsCRT::free(filename) ;  
    }
  }
}

NS_IMETHODIMP
nsNetDiskCache::ShutdownDB()
{
  if (mDB)
  {
    SetSizeEntry();
    NS_RELEASE(mDB);
    mDB = nsnull;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNetDiskCache::Observe(nsISupports *aSubject, const PRUnichar *aTopic,
                        const PRUnichar *someData)
{
  return ShutdownDB();
}


NS_IMETHODIMP nsNetDiskCache::InitCacheFolder()
{
#ifdef DEBUG_dp
  printf("CACHE: InitCacheFolder()\n");
#endif /* DEBUG_dp */

// don't initialize if no cache folder is set. 
 if(!mDiskCacheFolder) return NS_OK ;
	nsresult rv;

    nsXPIDLCString path;
    rv = mDiskCacheFolder -> GetPath (getter_Copies (path));
    if (NS_FAILED (rv))
        return rv;

	fixCacheVersion (path, CURRENT_CACHE_VERSION);

	if(!mDB) {
    mDB = new nsDBAccessor() ;
    if(!mDB)
      return NS_ERROR_OUT_OF_MEMORY ;
    else
      NS_ADDREF(mDB) ;
  }


  rv = InitDB();
  if ( NS_FAILED( rv ) )
  	rv = DBRecovery();
  if ( NS_FAILED( rv ) )
  	return rv;
  
  // create cache sub directories
  // Do this after initializing the database to avoid the situtation where on the first initialization
  // YOu create the folders and then immediately mark them for deletion.
  nsCOMPtr<nsIFile> cacheSubDir;

  for (int i=0; i < 32; i++) {
   	rv = mDiskCacheFolder->Clone( getter_AddRefs( cacheSubDir ) );
    if(NS_FAILED(rv))
      return rv ;

    char dirName[3];
    PR_snprintf (dirName, 3, "%0.2x", i);
    cacheSubDir->Append (dirName) ;
    CreateDir(cacheSubDir);
  }
  return rv;
}

NS_IMETHODIMP
nsNetDiskCache::Init(void) 
{
	nsresult rv;
	nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
	if ( NS_FAILED (rv ) )
		return rv; 
	rv = prefs->RegisterCallback( CACHE_DIR_PREF, folderChanged, this); 
	if ( NS_FAILED( rv ) )
		return rv;
	
	rv = prefs->RegisterCallback( CACHE_ENABLE_PREF, enableChanged, this); 
	if ( NS_FAILED( rv ) )
		return rv;
	
	rv = folderChanged(CACHE_DIR_PREF , this );	
	enableChanged(CACHE_ENABLE_PREF , this );
	
	return rv;
}

NS_IMETHODIMP
nsNetDiskCache::InitDB(void)
{
  nsresult rv ; 

	rv =mDiskCacheFolder->Clone( getter_AddRefs( mDBFile ) );
  if(NS_FAILED(rv))
    return rv ;

  mDBFile->Append("cache.db") ;
  PRBool exists = PR_FALSE;
  rv = mDBFile->Exists(&exists);
#ifdef XP_MAC
  if (NS_SUCCEEDED(rv) && !exists)
    {
      mDBFile->Create( nsIFile::NORMAL_FILE_TYPE, PR_IRUSR | PR_IWUSR );
              }
#endif
  rv = mDB->Init(mDBFile) ;
  if (NS_FAILED(rv)) return rv;
  rv = GetSizeEntry();
  if (NS_FAILED(rv)) return rv;
  
  // Detect cache corruption conditions
  // ----------------------------------------------------------------------
 
  if (exists && mNumEntries == 0)
  {
    // Cache db didn't shutdown properly. We should reinitialize.
    // NOTE: This wont cause an infinite loop as after this, the db
    // file wont exist and the (exists) condition will prevent this when
    // InitDB() again happens on DBRecovery()
    return NS_ERROR_FAILURE;
  }

  // Corruption. We cannot have a cache that has more entries than the max.
  if (mNumEntries > mMaxEntries)
    return NS_ERROR_FAILURE;


  // Normal startup
  // ----------------------------------------------------------------------
  //
  // Become an xpcom shutdown observer so we can flush data to disk not only on destructor
  // but also on the shutdown to protect against leaks. Here is a list of all failsafes for
  // flushing cache counts.
  // 1. This observer of xpcom shutdown
  // 2. Destructor of nsNetDiskCache
  // 3. On startup, if (1) or (2) didn't happen, we clear the cache
  //
  nsresult rvLocal = NS_OK;
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rvLocal);
  if (NS_SUCCEEDED(rvLocal))
  {
    nsAutoString topic;
    topic.AssignWithConversion(NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    (void) observerService->AddObserver(NS_STATIC_CAST(nsIObserver *, this),
                                        topic.get());
  }

  // Set size to 0 to detect if we didnt shutdown properly.
  // This will be reset to the right value by the shutdown observer.
  rv = SetSizeEntryExplicit(0, 0);
  return rv;
}
  
//////////////////////////////////////////////////////////////////////////
// nsISupports methods
  
NS_IMPL_THREADSAFE_ISUPPORTS4(nsNetDiskCache, nsINetDataDiskCache,
  nsINetDataCache, nsIObserver, nsISupportsWeakReference)
  
///////////////////////////////////////////////////////////////////////////
// nsINetDataCache Method 


NS_IMETHODIMP
nsNetDiskCache::GetDescription(PRUnichar* *aDescription) 
{
  nsAutoString description ;
  description.AssignWithConversion("Disk Cache") ;

  *aDescription = ToNewUnicode(description) ;
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

  NS_ASSERTION(mDB, "no db.") ;

  PRInt32 id = 0 ;
  nsresult rv = mDB->GetID(key, length, &id) ;

  if(NS_FAILED(rv)) {
    // try recovery if error 
    DBRecovery() ;
    return rv ;
  }

  void* info = 0 ;
  PRUint32 info_size = 0 ;

  rv = mDB->Get(id, &info, &info_size) ;
  if(NS_SUCCEEDED(rv) && info) 
    *_retval = PR_TRUE ;
        
  if(NS_FAILED(rv)) {
    // try recovery if error 
    DBRecovery() ;
  }

  return rv ;
}

/* regardless if it's cached or not, a copy of nsNetDiskCache would 
 * always be returned. so release it appropriately. 
 * if mem allocated, update mNumEntries also.
 * for now, the new nsCachedNetData is not written into db yet since
 * we have nothing to write.
 */
NS_IMETHODIMP
nsNetDiskCache::GetCachedNetData(const char* key, PRUint32 length, nsINetDataCacheRecord **_retval) 
{
  NS_ASSERTION(mDB, "no db.");

  nsresult rv = 0;
  if (!_retval)
    return NS_ERROR_NULL_POINTER;

  *_retval = nsnull;

  PRInt32 id = 0;
  rv = mDB->GetID(key, length, &id);

	if ( NS_SUCCEEDED ( rv )) {
	  // construct an empty record
	  nsDiskCacheRecord* newRecord = new nsDiskCacheRecord(mDB, this);
	  if (!newRecord) 
	    return NS_ERROR_OUT_OF_MEMORY;

	  rv = newRecord->Init(key, length, id);
	  if ( NS_FAILED( rv) ) {
	    delete newRecord;
	    return rv;
	  }

	  NS_ADDREF(newRecord); // addref for _retval 
	  *_retval = (nsINetDataCacheRecord*) newRecord;
	  
	  void* info = 0;
	  PRUint32 info_size = 0;

	  rv = mDB->Get(id, &info, &info_size);
	  if ( NS_SUCCEEDED( rv ) ) {
			if ( info ) {
		    // this is a previously cached record    
		    rv = newRecord->RetrieveInfo(info, info_size);
		    if ( NS_FAILED( rv ) ) {  
		      // probably a bad one
		      NS_RELEASE(newRecord);
		      *_retval = nsnull;
		      return rv;
		    }   
	 		} else {
		    // this is a new record. 
		    mNumEntries ++;
	    }
	  } 
  }
  
  if ( NS_FAILED( rv ) ) 
  	DBRecovery();

#ifdef DEBUG_dp
  printf("CACHE: GetCachedNetData(%s) created nsDiskCacheRecord %p id=%d\n", key, _retval, id);
#endif /* DEBUG_dp */

  return rv;
}

NS_IMETHODIMP
nsNetDiskCache::GetCachedNetDataByID(PRInt32 RecordID, nsINetDataCacheRecord **_retval) 
{
  NS_ASSERTION(mDB, "no db.");

	nsresult rv;
  void* info = 0;
  PRUint32 info_size = 0;
	
	if (!_retval)
    return NS_ERROR_NULL_POINTER;
	*_retval = nsnull;
	 
  rv = mDB->Get(RecordID, &info, &info_size);
  if(NS_SUCCEEDED(rv) && info) {
    // construct an empty record if only found in db
    nsDiskCacheRecord* newRecord = new nsDiskCacheRecord(mDB, this);
    if(!newRecord) 
      return NS_ERROR_OUT_OF_MEMORY;
   
    rv = newRecord->RetrieveInfo(info, info_size);
    if(NS_SUCCEEDED(rv)) {
      *_retval = (nsINetDataCacheRecord*) newRecord;
      NS_ADDREF(*_retval);
    } else {
      delete newRecord; 
    }
#ifdef DEBUG_dp
    printf("CACHE: GetCachedNetDataByID(id=%d) created nsDiskCacheRecord %p\n", RecordID, *_retval);
#endif /* DEBUG_dp */
    return rv;
  }

  // At this point, we didnt succeed in getting the record. This could be caused
  // by two cases:
  // 1. mDB->Get() returns NS_OK but info is NULL
  //		This will be a non-fatal error. Just pass the error up.
  //
  // 2. mDB->Get() return NS_ERROR and info is untouched -
  //		High possibility that DB is corrupted. Go to dev con 3 immediately.

  if (NS_FAILED(rv))
  {
    DBRecovery();
  }
  else
  {
    // We got NS_OK from mDB->Get() but info is null. Return error.
    rv = NS_ERROR_FAILURE;
  }

#ifdef DEBUG_dp
    printf("CACHE: GetCachedNetDataByID(id=%d) = RecordID not in DB\n", RecordID);
#endif /* DEBUG_dp */

  NS_WARNING("Error: RecordID not in DB\n");
  return rv;
}

NS_IMETHODIMP
nsNetDiskCache::GetEnabled(PRBool *aEnabled) 
{
  *aEnabled = mEnabled ;
  return NS_OK ;
}

NS_IMETHODIMP
nsNetDiskCache::SetEnabled(PRBool aEnabled) 
{
  mEnabled = aEnabled ;
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
  *aNumEntries = mNumEntries ;
  return NS_OK ;
}

NS_IMETHODIMP
nsNetDiskCache::GetMaxEntries(PRUint32 *aMaxEntries)
{
  *aMaxEntries = mMaxEntries ;
  return NS_OK ;
}

NS_IMETHODIMP
nsNetDiskCache::NewCacheEntryIterator(nsISimpleEnumerator **_retval) 
{
  NS_ASSERTION(mDB, "no db.") ;

  if(!_retval)
    return NS_ERROR_NULL_POINTER ;

  *_retval = nsnull ;

  nsDBEnumerator* enumerator = new nsDBEnumerator(mDB, this) ;
  if(enumerator)
  	return  enumerator->QueryInterface( nsISimpleEnumerator::GetIID(), (void**)_retval );
  
  return NS_ERROR_OUT_OF_MEMORY ;
}

NS_IMETHODIMP
nsNetDiskCache::GetNextCache(nsINetDataCache * *aNextCache)
{
  if(!aNextCache)
    return NS_ERROR_NULL_POINTER ;

  *aNextCache = mpNextCache ;
  return NS_OK ;
}

NS_IMETHODIMP
nsNetDiskCache::SetNextCache(nsINetDataCache *aNextCache)
{
  mpNextCache = aNextCache ;
  return NS_OK ;
}

// db size can always be measured at the last minute. Since it's hard
// to know before hand. 
NS_IMETHODIMP
nsNetDiskCache::GetStorageInUse(PRUint32 *aStorageInUse)
{
  NS_ASSERTION(mDB, "no db.");

  PRUint32 total_size = mStorageInUse;

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
#ifdef DEBUG_dp
  printf("CACHE: RemoveAll()\n");
#endif /* DEBUG_dp */
  NS_ASSERTION(mDB, "no db.") ;
  NS_ASSERTION(mDiskCacheFolder, "no cache folder.") ;
  nsresult rv;



  // Shutdown the database
  mDB->Shutdown() ;
  
  // Delete all the files in the cache directory
  // Could I just delete the cache folder????? --DJM
  nsCOMPtr<nsISimpleEnumerator> directoryEnumerator;
  nsresult res = mDiskCacheFolder->GetDirectoryEntries( getter_AddRefs( directoryEnumerator) ) ;
  if ( NS_FAILED ( res ) )
  	return res;

  nsCOMPtr<nsIFile> file;
  
  PRBool hasMore;
  while( NS_SUCCEEDED(directoryEnumerator->HasMoreElements( &hasMore ) ) && hasMore)
  {
  	rv = directoryEnumerator->GetNext( getter_AddRefs(file) );
  	if ( NS_FAILED( rv ) )
  		return rv ;
  	PRBool isDirectory;		
		if ( NS_SUCCEEDED(file->IsDirectory( &isDirectory )) && isDirectory )
    	file->Remove( PR_TRUE );
     
  }
  if ( mDBFile )
  {
 	 mDBFile->Remove(PR_FALSE) ;
	 if (mDBFile)
	 {
		PRBool exists = PR_FALSE;
		rv = mDBFile->Exists(&exists);
		if (exists)
			return NS_ERROR_FAILURE;
	 }
  }
  // reinitilize
  return InitCacheFolder() ;
}

//////////////////////////////////////////////////////////////////
// nsINetDataDiskCache methods

NS_IMETHODIMP
nsNetDiskCache::GetDiskCacheFolder(nsIFile * *aDiskCacheFolder)
{
  *aDiskCacheFolder = nsnull ;
  NS_ASSERTION(mDiskCacheFolder, "no cache folder.") ;

  *aDiskCacheFolder = mDiskCacheFolder ;
  NS_ADDREF(*aDiskCacheFolder) ;
  return NS_OK ;
}

NS_IMETHODIMP
nsNetDiskCache::SetDiskCacheFolder(nsIFile * aDiskCacheFolder)
{
  if(!mDiskCacheFolder){
    mDiskCacheFolder = aDiskCacheFolder ;
    return InitCacheFolder() ;
  } 
  else {
	PRBool isEqual;
    if( NS_SUCCEEDED( mDiskCacheFolder->Equals( aDiskCacheFolder, &isEqual ) ) && !isEqual ) {
      mDiskCacheFolder = aDiskCacheFolder ;

      // do we need to blow away old cache before building a new one?
     	// mDiskFolder points to the old cache so you can't call RemoveALL
     	// need to refactor.
     // return RemoveAll() ;

      mDB->Shutdown() ;
      return InitCacheFolder() ;

    } else 
      return NS_OK ;
  }
}

//////////////////////////////////////////////////////////////////
// nsNetDiskCache methods

// create a directory (recursively)
NS_IMETHODIMP
nsNetDiskCache::CreateDir(nsIFile* dir_spec) 
{
  PRBool does_exist ;
  nsCOMPtr<nsIFile> p_spec ;

  dir_spec->Exists(&does_exist) ;
  if(does_exist) 
    return NS_OK ;

  nsresult rv = dir_spec->GetParent(getter_AddRefs(p_spec)) ;
  if(NS_FAILED(rv))
    return rv ;

  p_spec->Exists(&does_exist) ;
  if(!does_exist) {
    rv = CreateDir(p_spec) ;
  	if ( NS_FAILED( rv ))
  		return rv;
  }
  
  rv = dir_spec->Create( nsIFile::DIRECTORY_TYPE, PR_IRWXU) ;  
  return rv ;
}

// We can't afford to make a *separate* pass over the whole db on every
// startup, just to figure out mNumEntries and mStorageInUse.  (This is a
// several second operation on a large db).  We'll likely need to store
// distinguished keys in the db that contain these values and update them
// incrementally, except when failure to shut down the db cleanly is detected.

NS_IMETHODIMP
nsNetDiskCache::GetSizeEntry(void)
{
  void* pInfo ;
  PRUint32 InfoSize ;

  nsresult rv = mDB->GetSizeEntry(&pInfo, &InfoSize) ;
  if(NS_FAILED(rv))
    return rv ;

  if(!pInfo && InfoSize == 0) {
    // must be a new DB
    mNumEntries = 0 ;
    mStorageInUse = 0 ;
  } 
  else {
    char * cur_ptr = NS_STATIC_CAST(char*, pInfo) ;
    
    // get mNumEntries
    COPY_INT32(&mNumEntries, cur_ptr) ;
    cur_ptr += sizeof(PRUint32) ;
    
    // get mStorageInUse
    COPY_INT32(&mStorageInUse, cur_ptr) ;
    cur_ptr += sizeof(PRUint32) ;
    
    PR_ASSERT(cur_ptr == NS_STATIC_CAST(char*, pInfo) + InfoSize);
  }

  return NS_OK ;
}

NS_IMETHODIMP
nsNetDiskCache::SetSizeEntryExplicit(PRUint32 numEntries, PRUint32 storageInUse)
{

  PRUint32 InfoSize ;
  
  InfoSize = sizeof numEntries ;
  InfoSize += sizeof storageInUse ;
  
  void* pInfo = nsMemory::Alloc(InfoSize*sizeof(char)) ;
  if(!pInfo) 
    return NS_ERROR_OUT_OF_MEMORY ;
  
  char* cur_ptr = NS_STATIC_CAST(char*, pInfo) ;

  COPY_INT32(cur_ptr, &numEntries) ;
  cur_ptr += sizeof(PRUint32) ;

  COPY_INT32(cur_ptr, &storageInUse) ;
  cur_ptr += sizeof(PRUint32) ;
  
  PR_ASSERT(cur_ptr == NS_STATIC_CAST(char*, pInfo) + InfoSize);
  
  return mDB->SetSizeEntry(pInfo, InfoSize) ;
}

NS_IMETHODIMP
nsNetDiskCache::SetSizeEntry(void)
{
  return SetSizeEntryExplicit(mNumEntries, mStorageInUse);
}

// this routine will be called everytime we have a db corruption. 
// mDB will be re-initialized, mStorageInUse and mNumEntries will
// be reset.
NS_IMETHODIMP
nsNetDiskCache::DBRecovery(void)
{
  return RemoveAll();
}



// this routine will add string "trash" to current CacheSubDir names. 
// e.g. 00->trash00, 1f->trash1f. and update the mDBCorrupted. 

NS_IMETHODIMP
nsNetDiskCache::RenameCacheSubDirs(void) 
{
  nsCOMPtr<nsIFile> cacheSubDir;
  nsresult rv;

  for (int i=0; i < 32; i++) {
  	rv = mDiskCacheFolder->Clone( getter_AddRefs( cacheSubDir ) );
    if(NS_FAILED(rv))
      return rv ;

    char oldName[3], newName[8];
    PR_snprintf(oldName, 3, "%0.2x", i) ;
    cacheSubDir->Append(oldName) ;

    // re-name the directory
    PR_snprintf(newName, 8, "trash%0.2x", i) ;
    rv = cacheSubDir->MoveTo( mDiskCacheFolder, newName );
    if(NS_FAILED(rv))
      // TODO, error checking
      return NS_ERROR_FAILURE ;
  }

  // update mDBCorrupted 
  mDBCorrupted = PR_TRUE ;

  return NS_OK ;
}

#define	VERSION_FILE	"Version"

PRBool
nsNetDiskCache::fixCacheVersion (const char *cache_dir, unsigned version)
{
	nsCString versionName;

	versionName.Append (  cache_dir );
	versionName.Append ("/");
	versionName.Append (VERSION_FILE);

	PRFileDesc *vf;
	char  vBuf[12];
	unsigned fVersion = 0;

	memset (vBuf, 0, sizeof (vBuf));

	if ((vf = PR_Open (versionName, PR_RDWR, 0)) != NULL)
	{
		PR_Read (vf, vBuf, 12);
		fVersion = atoi (vBuf);
	}
	else
	{
		if ((vf = PR_Open (versionName, PR_CREATE_FILE|PR_RDWR, 0777)) == NULL)
			return PR_FALSE;
	}

	if (fVersion < version)
	{
    // Make sure we reset fileptr to the beginning
    PR_Seek(vf, 0, PR_SEEK_SET);

    // Write the new version number
		sprintf  (vBuf, "%u", version);
		if (PR_Write (vf, vBuf, sizeof (vBuf)) != sizeof (vBuf))
		{
			PR_Close (vf);
			return PR_FALSE;
		}

		removeAllFiles (cache_dir, VERSION_FILE);
	}

	PR_Close (vf);
	
	return PR_TRUE;
}

void
nsNetDiskCache::removeAllFiles (const char *dir, const char *tagFile)
{
	PRDir *dp = PR_OpenDir (dir);
	PRDirEntry *de;
	PRFileInfo	finfo;

	if (dp == NULL)
		return;

	nsCString fileName;	// don't make it static here (stack may overflow)

	while ((de = PR_ReadDir (dp, PR_SKIP_BOTH)) != NULL)
	{
		if (tagFile != NULL
#if defined(VMS)
			&& !strcasecmp (de -> name, tagFile))
#else
			&& !strcmp (de -> name, tagFile))
#endif
			continue;

		fileName.Truncate ();
		fileName.Append (dir);
		fileName.Append ("/");
		fileName.Append (de -> name);

		if (PR_GetFileInfo (fileName, &finfo) != PR_SUCCESS)
			continue;

		if (finfo.type == PR_FILE_DIRECTORY)
		{
			removeAllFiles (fileName);
			PR_RmDir	   (fileName);
		}
		else
			PR_Delete (fileName);

	}

	PR_CloseDir (dp);
}
