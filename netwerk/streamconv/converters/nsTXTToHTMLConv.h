/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

