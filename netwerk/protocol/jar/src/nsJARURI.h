/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsJARURI_h__
#define nsJARURI_h__

#include "nsIJARURI.h"
#include "nsCOMPtr.h"

#define NS_JARURI_CID					 \
{ /* 0xc7e410d7-0x85f2-11d3-9f63-006008a6efe9 */     \
    0xc7e410d7,                                      \
    0x85f2,                                          \
    0x11d3,                                          \
    {0x9f, 0x63, 0x00, 0x60, 0x08, 0xa6, 0xef, 0xe9} \
}

class nsJARURI : public nsIJARURI
{
public:    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURI
    NS_DECL_NSIJARURI

    // nsJARURI
    nsJARURI();
    virtual ~nsJARURI();
   
    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsresult Init();
    nsresult FormatSpec(const char* entryPath, char* *result);

protected:
    nsCOMPtr<nsIURI> mJARFile;
    char *mJAREntry;
    PRUint32 mSchemeType;
};

#endif // nsJARURI_h__
