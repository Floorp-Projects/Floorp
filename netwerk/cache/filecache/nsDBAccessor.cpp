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

/*
 * This file is part of filecache implementation.
 * 
 * nsIDBAccessor is a interface that shields all the direct database access 
 * method from nsNetDiskCache. 
 * 
 * nsDBAccessor is a implementation of the nsIDBAccessor interface. It
 * uses dbm(Berkely) as the database. 
 * 
 * a nsDiskCacheRecord is mapped into two entries in the database, 
 *    key->recordID
 *    recordID->metadata
 */

#include "nsDBAccessor.h"
#include "nscore.h"

#include "prtypes.h"
#include "plhash.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "nslog.h"

NS_IMPL_LOG(nsDBAccessorLog)
#define PRINTF NS_LOG_PRINTF(nsDBAccessorLog)
#define FLUSH  NS_LOG_FLUSH(nsDBAccessorLog)

nsDBAccessor::nsDBAccessor() :
  mDB(0) ,
  mDBFile(0) ,
  mSessionID(0) ,
  mSessionCntr(0)  
{
	 mDBFilesize = LL_Zero();
  mLastSyncTime = PR_IntervalNow() ;

  NS_INIT_REFCNT();
}

nsDBAccessor::~nsDBAccessor()
{
  Shutdown() ;
}

//
// Implement nsISupports methods
//
NS_IMPL_THREADSAFE_ISUPPORTS(nsDBAccessor, NS_GET_IID(nsIDBAccessor))


///////////////////////////////////////////////////////////
// nsIDBAccessor methods 

NS_IMETHODIMP
nsDBAccessor::Init(nsIFile* dbfile)
{
  char* dbname ;

  // this should cover all platforms. 
  dbfile->GetPath(&dbname) ;
  
  mDBFile = dbfile ;

  // FUR - how is page size chosen ?  It's worth putting a comment
  // in here about the possible usefulness of tuning these parameters
  HASHINFO hash_info = {
    16*1024 , /* bucket size */
    0 ,       /* fill factor */
    0 ,       /* number of elements */
    0 ,       /* bytes to cache */
    0 ,       /* hash function */
    0} ;      /* byte order */

  mDB = dbopen(dbname,
               O_RDWR | O_CREAT ,
               0600 ,
               DB_HASH ,
               & hash_info) ;

  nsCRT::free(dbname) ;

  if(!mDB)
    return NS_ERROR_FAILURE ;

  // set mSessionID
  DBT db_key, db_data ;

  db_key.data = NS_CONST_CAST(char*, SessionKey) ;
  db_key.size = PL_strlen(SessionKey) ;

  int status = (*mDB->get)(mDB, &db_key, &db_data, 0) ;
  if(status == -1) {
    NS_ERROR("ERROR: failed get session id in database.") ;
    return NS_ERROR_FAILURE ;
  }

  if(status == 0) {
    // get the last session id
    PRInt16 old_ID;
    memcpy(&old_ID, db_data.data, sizeof(PRInt16));
    if(old_ID < ini_sessionID) {
      NS_ERROR("ERROR: Bad Session ID in database, corrupted db.") ;
      return NS_ERROR_FAILURE ;
    }

    mSessionID = old_ID + 1 ;
  } 
  else if(status == 1) {
    // must be a new db
    mSessionID = ini_sessionID ;
  }
  db_data.data = NS_REINTERPRET_CAST(void*, &mSessionID) ;
  db_data.size = sizeof(PRInt16) ;

  // store the new session id
  status = (*mDB->put)(mDB, &db_key, &db_data, 0) ;

  if(status == 0) {
    (*mDB->sync)(mDB, 0) ;

    // initialize database filesize
    return mDBFile->GetFileSize(&mDBFilesize) ;
  } 
  else {
    NS_ERROR("reset session ID failure.") ;
    return NS_ERROR_FAILURE ;
  }
  
}

NS_IMETHODIMP
nsDBAccessor::Shutdown(void)
{
  if(mDB) {
    (*mDB->sync)(mDB, 0) ;
    (*mDB->close)(mDB) ;
    mDB = nsnull ;
  }

  return NS_OK ;
}
  
NS_IMETHODIMP
nsDBAccessor::Get(PRInt32 aID, void** anEntry, PRUint32 *aLength) 
{
  if(!anEntry)
    return NS_ERROR_NULL_POINTER ;

  *anEntry = nsnull ;
  *aLength = 0 ;

  NS_ASSERTION(mDB, "no database") ;
  if(!mDB)
    return NS_ERROR_FAILURE ;

  DBT db_key, db_data ;

  db_key.data = NS_REINTERPRET_CAST(void*, &aID) ;
  db_key.size = sizeof(PRInt32) ;
  
  int status = 0 ;
  status = (*mDB->get)(mDB, &db_key, &db_data, 0) ;

  if(status == 0) {
    *anEntry = db_data.data ;
    *aLength = db_data.size ;
    return NS_OK ;
  } 
  else if(status == 1)
    return NS_OK ;
  else
    return NS_ERROR_FAILURE ;

}

NS_IMETHODIMP
nsDBAccessor::Put(PRInt32 aID, void* anEntry, PRUint32 aLength)
{
  NS_ASSERTION(mDB, "no database") ;
  if(!mDB)
    return NS_ERROR_FAILURE ;

  DBT db_key, db_data ;

  db_key.data = NS_REINTERPRET_CAST(void*, &aID) ;
  db_key.size = sizeof(PRInt32) ;

  db_data.data = anEntry ;
  db_data.size = aLength ;

  if(0 == (*mDB->put)(mDB, &db_key, &db_data, 0)) {
    return Sync() ;
  }
  else {
    return NS_ERROR_FAILURE ;
  }
}

/*
 * It's more important to remove the id->metadata entry first since
 * key->id mapping is just a reference
 */
NS_IMETHODIMP
nsDBAccessor::Del(PRInt32 aID, void* anEntry, PRUint32 aLength)
{
  NS_ASSERTION(mDB, "no database") ;
  if(!mDB)
    return NS_ERROR_FAILURE ;

  DBT db_key ;

  // delete recordID->metadata
  db_key.data = NS_REINTERPRET_CAST(void*, &aID) ;
  db_key.size = sizeof(PRInt32) ;

  PRInt32 status = -1 ;
  status = (*mDB->del)(mDB, &db_key, 0) ;

  if(-1 == status) {
    return NS_ERROR_FAILURE ;
  } 

  // delete key->recordID
  db_key.data = anEntry ;
  db_key.size = aLength ;
  status = (*mDB->del)(mDB, &db_key, 0) ;
  if(-1 == status) {
    return NS_ERROR_FAILURE ;
  } 

  return Sync() ;
}

NS_IMETHODIMP
nsDBAccessor::GetID(const char* key, PRUint32 length, PRInt32* aID) 
{
  NS_ASSERTION(mDB, "no database") ;
  if(!mDB)
    return NS_ERROR_FAILURE ;

  DBT db_key, db_data ;

  db_key.data = NS_CONST_CAST(char*, key) ;
  db_key.size = length ;

  int status = (*mDB->get)(mDB, &db_key, &db_data, 0) ;
  if(status == 0) {
    // found recordID
    memcpy(aID, db_data.data, sizeof(PRInt32));
    return NS_OK ;
  }
  else if(status == 1) {
    // create a new one
    PRInt32 id = 0 ;
    id = mSessionID << 16 | mSessionCntr++ ;

    // add new id into mDB
    db_data.data = NS_REINTERPRET_CAST(void*, &id) ;
    db_data.size = sizeof(PRInt32) ;

    status = (*mDB->put)(mDB, &db_key, &db_data, 0) ;
    if(status != 0) {
      NS_ERROR("updating db failure.") ;
      return NS_ERROR_FAILURE ;
    }
    *aID = id ;
    return Sync() ;
  } 
  else {
    NS_ERROR("ERROR: keydb failure.") ;
    return NS_ERROR_FAILURE ;
  }
}

NS_IMETHODIMP
nsDBAccessor::EnumEntry(void** anEntry, PRUint32* aLength, PRBool bReset) 
{
  if(!anEntry)
    return NS_ERROR_NULL_POINTER ;

  *anEntry = nsnull ;
  *aLength = 0 ;

  NS_ASSERTION(mDB, "no database") ;
  if(!mDB)
    return NS_ERROR_FAILURE ;

  PRUint32 flag ;

  if(bReset) 
    flag = R_FIRST ;
  else
    flag = R_NEXT ;

  DBT db_key, db_data ;

  PRUint32 len = PL_strlen(SessionKey) ;

  int status ;

  do {
    status = (*mDB->seq)(mDB, &db_key, &db_data, flag) ;
    flag = R_NEXT ;
    if(status == -1)
      return NS_ERROR_FAILURE ;
    // get next if it's a key->recordID 
    if(db_key.size > sizeof(PRInt32) && db_data.size == sizeof(PRInt32))
      continue ;
    // get next if it's a sessionID entry
    if(db_key.size == len && db_data.size == sizeof(PRInt16))
      continue ;
    // recordID is always 32 bits long
    if(db_key.size == sizeof(PRInt32))
      break ;
  } while(!status) ;

  if (0 == status) {
    *anEntry = db_data.data ;
    *aLength = db_data.size ;
  }
  return NS_OK ;
}
/*
 * returns the cached database file size.
 * mDBFilesize will be updated during Sync().
 */
NS_IMETHODIMP
nsDBAccessor::GetDBFilesize(PRUint64* aSize)
{
  *aSize = mDBFilesize ;
  return NS_OK ; 
}

NS_IMETHODIMP
nsDBAccessor::GetSizeEntry(void** anEntry, PRUint32* aLength)
{
  if(!anEntry)
    return NS_ERROR_NULL_POINTER ;

  NS_ASSERTION(mDB, "no database") ;
  if(!mDB)
    return NS_ERROR_FAILURE ;

  *anEntry = nsnull ;
  *aLength = 0 ;

  DBT db_key, db_data ;

  db_key.data = NS_CONST_CAST(char*, SizeEntry) ;
  db_key.size = PL_strlen(SizeEntry) ;

  int status = (*mDB->get)(mDB, &db_key, &db_data, 0) ;

  if(status == -1) {
    NS_ERROR("ERROR: failed get special entry in database.") ;
    return NS_ERROR_FAILURE ;
  }

  if(status == 0) {
    *anEntry = db_data.data ;
    *aLength = db_data.size ;
  } 
  
  return NS_OK ;
}

NS_IMETHODIMP
nsDBAccessor::SetSizeEntry(void* anEntry, PRUint32 aLength)
{
  NS_ASSERTION(mDB, "no database") ;
  if( !mDB )
  	return NS_ERROR_FAILURE;
  DBT db_key, db_data ;

  db_key.data = NS_CONST_CAST(char*, SizeEntry) ;
  db_key.size = PL_strlen(SizeEntry) ;

  db_data.data = anEntry ;
  db_data.size = aLength ;

  if(0 == (*mDB->put)(mDB, &db_key, &db_data, 0)) {
    (*mDB->sync)(mDB, 0) ;
    return NS_OK ;
  }
  else {
    return NS_ERROR_FAILURE ;
  }
}

/*
 * sync routine is only called when the SyncInterval is reached. Otherwise
 * it just returns. If db synced, the filesize will be updated at the
 * same time.
 */
nsresult
nsDBAccessor::Sync(void)
{
  PRIntervalTime time = PR_IntervalNow() ;
  PRIntervalTime duration = time - mLastSyncTime ;

  NS_ASSERTION(mDB, "no database") ;
  if(!mDB)
    return NS_ERROR_FAILURE ;
  
  if (PR_IntervalToMilliseconds(duration) > SyncInterval) {
    int status = (*mDB->sync)(mDB, 0) ;
    if(status == 0) {
      PRINTF("\tsynced\n") ;
      mLastSyncTime = time ;
      
      // update db filesize here
      return mDBFile->GetFileSize(&mDBFilesize) ;
      
    } else
      return NS_ERROR_FAILURE ;
  } else {
    PRINTF("\tnot synced\n") ;
    return NS_OK ;
  }
}
