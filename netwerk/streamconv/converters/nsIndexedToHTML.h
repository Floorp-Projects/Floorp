/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef ____nsindexedtohtml___h___
#define ____nsindexedtohtml___h___

#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsIFactory.h"
#include "nsString.h"
#include "nsIStreamConverter.h"
#include "nsXPIDLString.h"

#define NS_NSINDEXEDTOHTMLCONVERTER_CID \
{ 0xcf0f71fd, 0xfafd, 0x4e2b, {0x9f, 0xdc, 0x13, 0x4d, 0x97, 0x2e, 0x16, 0xe2} }


class nsIndexedToHTML : public nsIStreamConverter 
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMCONVERTER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    nsIndexedToHTML();
    virtual ~nsIndexedToHTML();

    // For factory creation.
    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
        nsresult rv;
        if (aOuter)
            return NS_ERROR_NO_AGGREGATION;

        nsIndexedToHTML* _s = new nsIndexedToHTML();
        if (_s == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = _s->QueryInterface(aIID, aResult);
        return rv;
    }

protected:
    nsresult Handle201(char* buffer, nsString &pushBuffer);

    nsCOMPtr<nsIStreamListener>     mListener; // final listener (consumer)
    nsXPIDLCString mCurrentPath;
};

#endif

