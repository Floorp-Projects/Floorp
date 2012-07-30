/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ____nstxttohtmlconv___h___
#define ____nstxttohtmlconv___h___

#include "nsITXTToHTMLConv.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
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
    bool     prepend;   // flag indicating how the modText should be used.
} convToken;
    
/**
 * Convert plain text to HTML.
 *
 * OVERVIEW OF HOW THIS CLASS WORKS:
 *
 * This class stores an array of tokens that should be replaced by something,
 * or something that should be prepended.
 * The "token" member of convToken is the text to search for. This is a
 * substring of the desired token. Tokens are delimited by TOKEN_DELIMITERS.
 * That entire token will be replaced by modText (if prepend is false); or it
 * will be linkified and modText will be prepended to the token if prepend is
 * true.
 *
 * Note that all of the text will be in a preformatted block, so there is no
 * need to emit line-end tags, or set the font face to monospace.
 *
 * This works as a stream converter, so data will arrive by
 * OnStartRequest/OnDataAvailable/OnStopRequest calls.
 *
 * OStopR will possibly process a remaining token.
 *
 * If the data of one pass contains a part of a token, that part will be stored
 * in mBuffer. The rest of the data will be sent to the next listener.
 * 
 * XXX this seems suboptimal. this means that this design will only work for
 * links. and it is impossible to append anything to the token. this means that,
 * for example, making *foo* bold is not possible.
 */
class nsTXTToHTMLConv : public nsITXTToHTMLConv {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMCONVERTER
    NS_DECL_NSITXTTOHTMLCONV
    NS_DECL_NSIREQUESTOBSERVER
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
        if (_s == nullptr)
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
    // underlying buffer, according to mToken
    PRInt32 CatHTML(PRInt32 front, PRInt32 back);

    nsCOMPtr<nsIStreamListener>     mListener; // final listener (consumer)
    nsString                        mBuffer;   // any carry over data
    nsTArray<nsAutoPtr<convToken> > mTokens;   // list of tokens to search for
    convToken                       *mToken;   // current token (if any)
    nsString                        mPageTitle; // Page title
    bool                            mPreFormatHTML; // Whether to use <pre> tags
};

#endif // ____nstxttohtmlconv___h___

