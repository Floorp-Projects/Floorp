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

#ifndef nsIHistoryDataSource_h__
#define nsIHistoryDataSource_h__

#include "nscore.h"
#include "nsIRDFDataSource.h"
#include "prtime.h"

// {1A880051-A59D-11d2-80B6-006097B76B8E}
#define NS_IHISTORYDATASOURCE_IID \
{ 0x1a880051, 0xa59d, 0x11d2, { 0x80, 0xb6, 0x0, 0x60, 0x97, 0xb7, 0x6b, 0x8e } }

class nsIHistoryDataSource : public nsIRDFDataSource
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IHISTORYDATASOURCE_IID; return iid; }

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
};

nsresult
NS_NewHistoryDataSource(nsIHistoryDataSource** result);

#endif nsIHistoryDataSource_h__

