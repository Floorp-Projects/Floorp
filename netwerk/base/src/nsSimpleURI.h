/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsSimpleURI_h__
#define nsSimpleURI_h__

#include "nsIURL.h"
#include "nsAgg.h"

#define NS_THIS_SIMPLEURI_IMPLEMENTATION_CID         \
{ /* 22b8f64a-2f7b-11d3-8cd0-0060b0fc14a3 */         \
    0x22b8f64a,                                      \
    0x2f7b,                                          \
    0x11d3,                                          \
    {0x8c, 0xd0, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

class nsSimpleURI : public nsIURI
{
public:
    NS_DECL_AGGREGATED
    NS_DECL_NSIURI

    // nsSimpleURI methods:

    nsSimpleURI(nsISupports* outer);
    virtual ~nsSimpleURI();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    char*       mScheme;
    char*       mPath;
    PRUint32    mSchemeType;
};

#endif // nsSimpleURI_h__
