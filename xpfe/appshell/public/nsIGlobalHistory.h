/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

  The global history data source.

 */

#ifndef nsIGlobalHistory_h__
#define nsIGlobalHistory_h__
#include "nscore.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "prtime.h"


// {9491C383-E3C4-11d2-BDBE-0050040A9B44} 
#define NS_IGLOBALHISTORY_IID \
{ 0x9491c383, 0xe3c4, 0x11d2, { 0xbd, 0xbe, 0x0, 0x50, 0x4, 0xa, 0x9b, 0x44 } \
 }
  

#define NS_GLOBALHISTORY_CID \
{ 0x9491c382, 0xe3c4, 0x11d2, { 0xbd, 0xbe, 0x0, 0x50, 0x4, 0xa, 0x9b, 0x44} \
 }


class nsIGlobalHistory : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IGLOBALHISTORY_IID; return iid; }

    /**
     * Add the specified item to history
     */
    NS_IMETHOD AddPage (const char* aURI, const char* aRefererURI, PRTime aDate) = 0;

    /**
     * Set the title of the page
     */
    NS_IMETHOD SetPageTitle (const char* aURI, const PRUnichar* aTitle) = 0;

    /**
     * Remove the page from history
     */
    NS_IMETHOD RemovePage (const char* aURI) = 0;

    /**
     * Get the uri's last visit date
     */
    NS_IMETHOD GetLastVisitDate (const char* aURI, uint32 *aDate) = 0;

    /** 
     * Get the preferred completion 
     */
    NS_IMETHOD CompleteURL (const char* aPrefix, char** aPreferredCompletions) = 0;

    /** 
     * Initiliaze 
     */
    NS_IMETHOD Init () = 0;
};

nsresult
NS_NewGlobalHistoryFactory(nsIFactory** result);

#endif /* nsIGlobalHistory_h__ */



