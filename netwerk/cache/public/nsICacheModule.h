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
 * nsCacheModule. A class that defines the way a cache module 
 * should be written. Its the super class for any new modules. 
 * Two sample modules derived from this one are nsMemModule and nsDiskModule.
 *
 * Gagan Saksena 02/03/98
 * 
 */

#ifndef nsICacheModule_h__
#define nsICacheModule_h__

#include <nsISupports.h>
#include "nsICacheObject.h"
#include "nsEnumeration.h"

// {5D51B24F-E6C2-11d1-AFE5-006097BFC036}
#define NS_ICACHEMODULE_IID \
{ 0x5d51b24f, 0xe6c2, 0x11d1, \
{ 0xaf, 0xe5, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };


class nsICacheModule : public nsISupports 
{

public:
    NS_DEFINE_STATIC_IID_ACCESSOR (NS_ICACHEMODULE_IID);


    NS_IMETHOD  AddObject(nsICacheObject* i_pObject) = 0;
    
    NS_IMETHOD  ContainsURL(const char* i_url, PRBool* o_bContain) = 0;

    NS_IMETHOD  ContainsCacheObj(nsICacheObject* i_pObject, PRBool* o_bContain) = 0;
    
    NS_IMETHOD  Enable (PRBool i_bEnable) = 0;
    
    NS_IMETHOD  GetNumOfEntries (PRUint32* o_nEntries) = 0;

    NS_IMETHOD  GetEnumerator (nsEnumeration** o_enum) = 0;
    /* Enumerations with a function pointer - TODO */

    //TODO move to own interface for both Garbage Collection and Revalidation
    NS_IMETHOD  GarbageCollect (void) = 0;

    NS_IMETHOD  GetObjectByURL (const char* i_url, nsICacheObject** o_pObj) = 0;

    NS_IMETHOD  GetObjectByIndex (const PRUint32 i_index, nsICacheObject** o_pObj) = 0;

    NS_IMETHOD  GetStreamFor (const nsICacheObject* i_pObject, nsStream** o_pStream) = 0;

    NS_IMETHOD  IsEnabled (PRBool* o_bEnabled) = 0;

    /* Can't do additions, deletions, validations, expirations */
    NS_IMETHOD  IsReadOnly (PRBool* o_bReadOnly) = 0; 

    NS_IMETHOD  GetNextModule (nsICacheModule** o_pCacheModule) = 0;
    NS_IMETHOD  SetNextModule (nsICacheModule* i_pCacheModule) = 0;

    NS_IMETHOD  RemoveByURL (const char* i_url) = 0;

    NS_IMETHOD  RemoveByIndex (const PRUint32 i_index) = 0;

    NS_IMETHOD  RemoveByObject (nsICacheObject* i_pObject) = 0;

    NS_IMETHOD  RemoveAll (void) = 0;

    NS_IMETHOD  Revalidate (void) = 0;

    NS_IMETHOD	ReduceSizeTo (const PRUint32 i_newsize) = 0;

    NS_IMETHOD  GetSize (PRUint32* o_size) = 0;

    NS_IMETHOD  SetSize (const PRUint32 i_size) = 0;

    NS_IMETHOD  GetSizeInUse (PRUint32* o_size) = 0;

};

// {5D51B24E-E6C1-11d0-AFE5-006097BFC036}
#define NS_DISKMODULE_CID \
{ 0x5d51b24e, 0xe6c1, 0x11d0, \
{ 0xaf, 0xe5, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };

// {5D51B250-E6C2-11d1-AFE5-006097BFC036}
#define NS_MEMMODULE_CID \
{ 0x5d51b250, 0xe6c2, 0x11d1, \
{ 0xaf, 0xe5, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };

#endif
