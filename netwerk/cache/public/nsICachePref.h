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

/* nsICachePref. A class to separate the preference related code to 
 * one place. This is an incomplete implementation, I need to access
 * the libprefs directly, but that would add another dependency. And 
 * libpref comes with a JS dependency... so holding this off for the 
 * moment. 
 * 
 * -Gagan Saksena 09/15/98
 */
#ifndef nsICachePref_h__
#define nsICachePref_h__

#include "nsISupports.h"
#include "prtypes.h"
#include "prlog.h"


// {7C3ED031-45E4-11d3-9B7F-0004ACB74CEC}
#define NS_ICACHEPREF_IID \
{ 0x7c3ed031, 0x45e4, 0x11d3, \
{ 0x9b, 0x7f, 0x0, 0x4, 0xac, 0xb7, 0x4c, 0xec } };

// myCachePrefCID {99E9C911-46D9-11d3-87EF-000629D01344}
#define NS_CACHEPREF_CID \
{ 0x99e9c911, 0x46d9, 0x11d3, \
{ 0x87, 0xef, 0x0, 0x6, 0x29, 0xd0, 0x13, 0x44}} ;

class nsICachePref : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR (NS_ICACHEPREF_IID);

    //  Was "static nsCachePref* GetInstance(void)"
    //  Do I need this GetInstance??????
//    NS_IMETHOD	GetInstance(nsICachePref** o_pICachePref ) = 0;

    enum Refresh 
    {
        ONCE,
        ALWAYS,
        NEVER
    };

    NS_IMETHOD	GetBkgSleepTime (PRUint32* o_time) = 0;
    
    NS_IMETHOD	GetDiskCacheDBFilename (char** o_name) = 0; /* like Fat.db */

    NS_IMETHOD	GetDiskCacheFolder (char** o_folder) = 0;   /* Cache dir */

    NS_IMETHOD	GetDiskCacheSSL (PRBool* o_bSet) = 0;

    NS_IMETHOD	SetDiskCacheSSL (PRBool i_bSet) = 0;

    NS_IMETHOD	GetDiskCacheSize (PRUint32* o_size) = 0;
    NS_IMETHOD	SetDiskCacheSize (const PRUint32 i_size) = 0;

    NS_IMETHOD	GetMemCacheSize (PRUint32 * o_size) = 0;
    NS_IMETHOD	SetMemCacheSize (const PRUint32 i_size) = 0;

    NS_IMETHOD	GetFrequency (nsICachePref::Refresh* o_frequency) = 0;

    /* Revalidating in background, makes IMS calls in the bkg thread to 
       update cache entries. TODO, this should be at a bigger time period
       than the cache cleanup routine */
    NS_IMETHOD	RevalidateInBkg (PRBool* i_bRevalidateInBkg) = 0;

    /* Setup all prefs */
    NS_IMETHOD	SetupPrefs(const char* i_Pref) = 0;

};

#endif // nsICachePref_h__

