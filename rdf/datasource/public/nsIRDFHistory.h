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

#ifndef nsIRDFHistory_h__
#define nsIRDFHistory_h__

#include "nscore.h"
#include "nsISupports.h"

// {115CE051-A59D-11d2-80B6-006097B76B8E}
#define NS_IRDFWEBPAGE_IID \
{ 0x115ce051, 0xa59d, 0x11d2, { 0x80, 0xb6, 0x0, 0x60, 0x97, 0xb7, 0x6b, 0x8e } };

// {1A880051-A59D-11d2-80B6-006097B76B8E}
#define NS_IRDFHISTORYDATASOURCE_IID \
{ 0x1a880051, 0xa59d, 0x11d2, { 0x80, 0xb6, 0x0, 0x60, 0x97, 0xb7, 0x6b, 0x8e } };


class nsIRDFHistoryDataSource : public nsIRDFDataSource {
public:

    /**
     * Add the specified item to history
     */
    NS_IMETHOD AddPage (const char* uri) = 0;

    /**
     * Set the title of the page
     */
    NS_IMETHOD SetPageTitle (const char* uri, PRUnichar* title) = 0;

    /**
     * Remove the page from history
     */
    NS_IMETHOD RemovePage (nsIRDFResource* page) = 0;

    /**
     * Get the uri's last visit date
     */
    NS_IMETHOD LastVisitDate (const char* uri, unit32 *date) = 0;
};
