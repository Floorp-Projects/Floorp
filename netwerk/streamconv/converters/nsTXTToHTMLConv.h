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

#ifndef ____nstxttohtmlconv___h___
#define ____nstxttohtmlconv___h___

#include "nsITXTToHTMLConv.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsIFactory.h"
#include "nsString.h"

#define NS_NSTXTTOHTMLCONVERTER_CID                         \
{ /* 9ef9fa14-1dd1-11b2-9d65-d72d6d1f025e */ \
    0x9ef9fa14, \
    0x1dd1, \
    0x11b2, \
    {0x9d, 0x65, 0xd7, 0x2d, 0x6d, 0x1f, 0x02, 0x5e} \
}

// Internal representation of a "token"
typedef struct convToken {
    nsString token;     // the actual string (i.e. "http://")
    nsString modText;   // replacement text or href prepend text.
    PRBool   prepend;   // flag indicating how the modText should be used.
} convToken;
    
class nsTXTToHTMLConv : public nsITXTToHTMLConv {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMCONVERTER
    NS_DECL_NSITXTTOHTMLCONV
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER

    nsTXTToHTMLConv();
    virtual ~nsTXTToHTMLConv();
    nsresult Init();

    // For factory creation.
    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
        nsresult rv;
        if (aOuter)
            return NS_ERROR_NO_AGGREGATION;

        nsTXTToHTMLConv* _s = new nsTXTToHTMLConv();
        if (_s == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(_s);
        rv = _s->Init();
        if (NS_FAILED(rv)) {
            delete _s;
            return rv;
        }
        rv = _s->QueryInterface(aIID, aResult);
        NS_RELEASE(_s);
        return rv;
    }


protected:
    // return the token and it's location in the underlying buffer.
    PRInt32 FindToken(PRInt32 cursor, convToken* *_retval);

    // return the cursor location after munging HTML into the 
    // underlying buffer.
    PRInt32 CatHTML(PRInt32 front, PRInt32 back);

    nsCOMPtr<nsIStreamListener>     mListener; // final listener (consumer)
    nsString                        mBuffer;   // any carry over data
    nsVoidArray                     mTokens;   // list of tokens to search for
    convToken                       *mToken;   // current token (if any)
    nsString                        mPageTitle; // Page title
    PRBool                          mPreFormatHTML; // Whether to use <pre> tags
};

#endif // ____nstxttohtmlconv___h___

