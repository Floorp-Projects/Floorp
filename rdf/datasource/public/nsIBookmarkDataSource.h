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

  The global bookmark data source.

 */

#ifndef nsIRDFBookmarkDataSource_h__
#define nsIRDFBookmarkDataSource_h__

#include "nscore.h"
#include "nsIRDFDataSource.h"

// {a82e9300-e4af-11d2-8fdf-0008c70adc7b}
#define NS_IRDFBOOKMARKDATASOURCE_IID \
{ 0xa82e9300, 0xe4af, 0x11d2, { 0x8f, 0xdf, 0x0, 0x08, 0xc7, 0x0a, 0xdc, 0x7b } }

class nsIRDFBookmarkDataSource : public nsIRDFDataSource
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IRDFBOOKMARKDATASOURCE_IID; return iid; }

    /**
     * Add the specified item to bookmarks
     */
    NS_IMETHOD AddBookmark (const char *aURI, const char *optionalTitle) = 0;
};

#endif nsIRDFBookmarkDataSource_h__

