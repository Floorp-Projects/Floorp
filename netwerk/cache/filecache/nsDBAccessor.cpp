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

#include "nsDBAccessor.h"
#include "nscore.h"

#include "prtypes.h"
#include "plhash.h"
#include "nsCRT.h"
#include "nsAutoLock.h"

nsDBAccessor::nsDBAccessor() :
  mDB(0) ,
  mSessionID(0) ,
  mSessionCntr(0)
{

  NS_INIT_REFCNT();
}

nsDBAccessor::~nsDBAccessor()
{
  printf(" ~nsDBAccessor\n") ;
  Shutdown() ;
}

//
// Implement nsISupports methods
//
NS_IMPL_ISUPPORTS(nsDBAccessor, NS_GET_IID(nsIDBAccessor))


///////////////////////////////////////////////////////////
// nsIDBAccessor methods 

NS_IMETHODIMP
nsDBAccessor::Init(nsIFileSpec* dbfile)
{
  // FUR - lock not needed
  m_Lock = PR_NewLock() ;
  if(!m_Lock)
    return NS_ERROR_OUT_OF_MEMORY ;

  char* dbname ;

  // this should cover all platforms. 
  dbfile->GetNativePath(&dbname) ;

  // FUR - how is page size chosen ?  It's worth putting a comment
  // in here about the possible usefulness of tuning these parameters
  HASHINFO hash_info = {
    16*1024 , /* bucket size */
    0 ,       /* fill factor */
    0 ,       /* number of elements */
    0 ,       /* bytes to cache */
    0 ,       /* hash function */
    0} ;      /* byte order */

  // FUR - lock not needed
  nsAutoLock lock(m_Lock) ;

  mDB = dbopen(dbname,
               O_RDWR | O_CREAT ,
               0600 ,
               DB_HASH ,
               & hash_info) ;

  // FUR - does dbname have to be free'ed ?

  if(!mDB)
    return NS_ERROR_FAILURE ;

  // set mSessionID
  // FUR - Why the +1 ?  (No need for key to be NUL-terminated string.)
  PRUint32 len = PL_strlen(SessionKey)+1 ;
  DBT db_key, db_data ;

  db_key.data = NS_CONST_CAST(char*, SessionKey) ;
  db_key.size = len ;

  int status = (*mDB->get)(mDB, &db_key, &db_data, 0) ;
  if(status == -1) {
    NS_ERROR("ERROR: failed get session id in database.") ;
    return NS_ERROR_FAILURE ;
  }

  if(status == 0) {
    // get the last session id
    PRInt16 *old_ID = NS_STATIC_CAST(PRInt16*, db_data.data) ;
    if(*old_ID < ini_sessionID) {
      NS_ERROR("ERROR: Bad Session ID in database, corrupted db.") ;
      return NS_ERROR_FAILURE ;
    }

    // FUR - need to comment out all printfs, or turn them into PR_LOG statements
    printf("found previous session, id = %d\n", *old_ID) ;
    mSessionID = *old_ID + 1 ;
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
    return NS_OK ;
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

  // FUR - locks not necessary
  if(m_Lock) 
    PR_DestroyLock(m_Lock);
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

  // Lock the db
  nsAutoLock lock(m_Lock) ;
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

  // Lock the db
  nsAutoLock lock(m_Lock) ;
  DBT db_key, db_data ;

  db_key.data = NS_REINTERPRET_CAST(void*, &aID) ;
  db_key.size = sizeof(PRInt32) ;

  db_data.data = anEntry ;
  db_data.size = aLength ;

  if(0 == (*mDB->put)(mDB, &db_key, &db_data, 0)) {
    // FUR - I would avoid unnecessary sync'ing for performance's
    // sake.  Maybe you could limit sync to max rate of, say, once
    // every few seconds by keeping track of last sync time, using PR_Now().
    (*mDB->sync)(mDB, 0) ;
    return NS_OK ;
  }
  else {
    // FUR - Try to avoid using NS_ERROR unless error is unrecoverable and serious
    NS_ERROR("ERROR: Failed to put anEntry into db.\n") ;
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

  // FUR - no locks necessary
  // Lock the db
  nsAutoLock lock(m_Lock) ;
  DBT db_key ;

  // delete recordID->metadata
  db_key.data = NS_REINTERPRET_CAST(void*, &aID) ;
  db_key.size = sizeof(PRInt32) ;

  PRInt32 status = -1 ;
  status = (*mDB->del)(mDB, &db_key, 0) ;

  if(-1 == status) {
    // FUR - no printf's, use PR_LOG, NS_WARNING, or NS_ASSERTION, as the situation warrants
    printf(" delete error\n") ;
    return NS_ERROR_FAILURE ;
  } 

  // delete key->recordID
  db_key.data = anEntry ;
  db_key.size = aLength ;
  status = (*mDB->del)(mDB, &db_key, 0) ;
  if(-1 == status) {
    // FUR - no printf's
    printf(" delete error\n") ;
    return NS_ERROR_FAILURE ;
  } 

  // FUR - Defer sync ?  See above
  (*mDB->sync)(mDB, 0) ;

  return NS_OK ;
}

NS_IMETHODIMP
nsDBAccessor::GetID(const char* key, PRUint32 length, PRInt32* aID) 
{
  NS_ASSERTION(mDB, "no database") ;

  // Lock the db
  nsAutoLock lock(m_Lock) ;

  DBT db_key, db_data ;

  db_key.data = NS_CONST_CAST(char*, key) ;
  db_key.size = length ;

  int status = (*mDB->get)(mDB, &db_key, &db_data, 0) ;
  if(status == 0) {
    // found recordID
    *aID = *(NS_REINTERPRET_CAST(PRInt32*, db_data.data)) ;
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
    // FUR - defer sync ?
    (*mDB->sync)(mDB, 0) ;
    *aID = id ;
    return NS_OK ;
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

  PRUint32 flag ;

  if(bReset) 
    flag = R_FIRST ;
  else
    flag = R_NEXT ;

  // Lock the db
  nsAutoLock lock(m_Lock) ;
  DBT db_key, db_data ;

  // FUR - +1 unnecessary ?
  PRUint32 len = PL_strlen(SessionKey)+1 ;

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
