/*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

#ifndef _NSIDBACCESSOR_H_
#define _NSIDBACCESSOR_H_

#include "nsIDBAccessor.h"
#include "mcom_db.h"
#include "prinrval.h"
#include "nsCOMPtr.h"
class nsIFile;

// bogus string for the key of session id 
static const char * const SessionKey = "SK" ;

// bogus string for the size 
static const char * const SizeEntry = "SE" ;

// initial session id number 
static const PRInt16 ini_sessionID = 0xff ;

static const PRUint16 SyncInterval = 1000 ;

class nsDBAccessor : public nsIDBAccessor
{
  public:
  NS_DECL_ISUPPORTS

  nsDBAccessor() ;
  virtual ~nsDBAccessor() ;
  
  NS_IMETHOD Init(nsIFile* dbfile) ;
  NS_IMETHOD Shutdown(void) ; 

  NS_IMETHOD Put(PRInt32 aID, void* anEntry, PRUint32 aLength) ;

  NS_IMETHOD Get(PRInt32 aID, void** anEntry, PRUint32 *aLength) ;

  NS_IMETHOD Del(PRInt32 aID, void* anEntry, PRUint32 aLength) ;

  NS_IMETHOD GetID(const char* key, PRUint32 length, PRInt32* aID) ;

  NS_IMETHOD EnumEntry(void* *anEntry, PRUint32* aLength, PRBool bReset) ;
  
  NS_IMETHOD GetDBFilesize(PRUint64* aSize) ;
  
  NS_IMETHOD GetSizeEntry(void** anEntry, PRUint32 *aLength) ;
  NS_IMETHOD SetSizeEntry(void* anEntry, PRUint32 aLength) ;

  protected:
  nsresult Sync(void) ;

  private:
  DB *                          mDB ;
  nsCOMPtr<nsIFile>         mDBFile ;
  PRInt16                       mSessionID ;
  PRInt16                       mSessionCntr ;
  PRIntervalTime                mLastSyncTime ;
  PRInt64                      mDBFilesize ; // cached DB filesize, 
                                              // updated on every sync for now
} ;

#endif // _NSIDBACCESSOR_H_
