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

#ifndef _NSIDBACCESSOR_H_
#define _NSIDBACCESSOR_H_

#include "nsIDBAccessor.h"
#include "mcom_db.h"

// bogus string for the key of session id 
// FUR - suggest "SK" instead of "^^"
static const char * const SessionKey = "^^" ;

// initial session id number 
static const PRInt16 ini_sessionID = 0xff ;

class nsDBAccessor : public nsIDBAccessor
{
  public:
  NS_DECL_ISUPPORTS

  nsDBAccessor() ;
  virtual ~nsDBAccessor() ;
  
  NS_IMETHOD Init(nsIFileSpec* dbfile) ;
  NS_IMETHOD Shutdown(void) ; 

  NS_IMETHOD Put(PRInt32 aID, void* anEntry, PRUint32 aLength) ;

  NS_IMETHOD Get(PRInt32 aID, void** anEntry, PRUint32 *aLength) ;

  NS_IMETHOD Del(PRInt32 aID, void* anEntry, PRUint32 aLength) ;

  NS_IMETHOD GetID(const char* key, PRUint32 length, PRInt32* aID) ;

  NS_IMETHOD EnumEntry(void* *anEntry, PRUint32* aLength, PRBool bReset) ;

  protected:

  private:
  DB *                          mDB ;
  PRInt16                       mSessionID ;
  PRInt16                       mSessionCntr ;
  PRLock *                      m_Lock ;
} ;

#endif // _NSIDBACCESSOR_H_
