/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* 
 * nsDiskModule. The disk cache module that stores the cache objects
 * on the disk. 
 *
 * Gagan Saksena 02/03/98
 * 
 */
#ifndef _nsDiskModule_H_
#define _nsDiskModule_H_

#include "nsICacheModule.h"
#include "nsIterator.h"
#include "nsCacheIterator.h"
#include "nsEnumeration.h"
#include "nsCachePref.h"
#include "nsMonitorable.h"
#include "mcom_db.h"


class nsDiskModule : public nsMonitorable, public nsICacheModule
{

public:
	NS_DECL_ISUPPORTS


    //////////////////////////////////////////////////////////////////////////
    // nsICacheModule Methods:

    NS_IMETHOD  AddObject(nsICacheObject* i_pObject);
    
    NS_IMETHOD  ContainsURL(const char* i_url, PRBool* o_bContain);

    NS_IMETHOD  ContainsCacheObj(nsICacheObject* i_pObject, PRBool* o_bContain) ;
    
    NS_IMETHOD  Enable (PRBool i_Enable);
    
    NS_IMETHOD  GetNumOfEntries (PRUint32* o_entry);

    NS_IMETHOD  GetEnumerator (nsEnumeration** o_enum);
    /* Enumerations with a function pointer - TODO */

    //TODO move to own interface for both Garbage Collection and Revalidation
    NS_IMETHOD  GarbageCollect (void);

    NS_IMETHOD  GetObjectByURL (const char* i_url, nsICacheObject** o_pObj);

    NS_IMETHOD  GetObjectByIndex (const PRUint32 i_index, nsICacheObject** o_pObj);

    NS_IMETHOD  GetStreamFor (const nsICacheObject* i_pObject, nsStream** o_pStream);

    NS_IMETHOD  IsEnabled (PRBool* o_bEnabled);

    /* Can't do additions, deletions, validations, expirations */
    NS_IMETHOD  IsReadOnly (PRBool* o_bReadOnly); 

    NS_IMETHOD  GetNextModule (nsICacheModule** o_pCacheModule);
    NS_IMETHOD  SetNextModule (nsICacheModule* i_pCacheModule);

    NS_IMETHOD  RemoveByURL (const char* i_url);

    NS_IMETHOD  RemoveByIndex (const PRUint32 i_index);

    NS_IMETHOD  RemoveByObject (nsICacheObject* i_pObject);

    NS_IMETHOD  RemoveAll (void);

    NS_IMETHOD  Revalidate (void);

    NS_IMETHOD	ReduceSizeTo (const PRUint32 i_newsize);

    NS_IMETHOD  GetSize (PRUint32* o_size);

    NS_IMETHOD  SetSize (const PRUint32 i_size);

    NS_IMETHOD  GetSizeInUse (PRUint32* o_size);

	/////////////////////////////////////////////////////////////////////////
	// nsDiskModule Methods
    nsDiskModule();
    nsDiskModule(const PRUint32 size);
    virtual ~nsDiskModule();

    PRUint32        AverageSize(void) const;

    static NS_METHOD
    Create (nsISupports *aOuter, REFNSIID aIID, void** aResult);

private:
    enum sync_frequency
    {
        EVERYTIME,
        IDLE,
        NEVER
    } m_Sync;

    PRBool          InitDB(void);

    nsDiskModule(const nsDiskModule& dm);
    nsDiskModule& operator=(const nsDiskModule& dm);


    PRUint32            m_Entries;
    PRUint32            m_Size;
    PRUint32            m_SizeInUse;
    PRBool              m_Enabled;
    nsEnumeration*      m_pEnumeration;
    nsCacheIterator*    m_pIterator;
    nsICacheModule*     m_pNext;
    DB* m_pDB;

};

#endif
