/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the gopher-directory to http-index code.
 *
 * The Initial Developer of the Original Code is
 * Bradley Baetz.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bradley Baetz <bbaetz@student.usyd.edu.au>
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

/* This code is heavily based on nsFTPDirListingConv.{cpp,h} */

#ifndef __nsgopherdirlistingconv__h__
#define __nsgopherdirlistingconv__h__

#include "nspr.h"
#include "prtypes.h"
#include "nsIStreamConverter.h"
#include "nsIChannel.h"
#include "nsString.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"

#include "nsIFactory.h"

#define NS_GOPHERDIRLISTINGCONVERTER_CID \
 { /* ea617873-3b73-4efd-a2c4-fc39bfab809d */ \
    0xea617873, \
    0x3b73, \
    0x4efd, \
    { 0xa2, 0xc4, 0xfc, 0x39, 0xbf, 0xab, 0x80, 0x9d} \
}

#define GOPHER_PORT 70
 
class nsGopherDirListingConv : public nsIStreamConverter {
public:
    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIStreamConverter methods
    NS_DECL_NSISTREAMCONVERTER

    // nsIStreamListener methods
    NS_DECL_NSISTREAMLISTENER

    // nsIRequestObserver methods
    NS_DECL_NSIREQUESTOBSERVER

    nsGopherDirListingConv();
    virtual ~nsGopherDirListingConv();
    nsresult Init();

    // For factory creation.
    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
        nsresult rv;
        if (aOuter)
            return NS_ERROR_NO_AGGREGATION;

        nsGopherDirListingConv* _s = new nsGopherDirListingConv();
        if (!_s)
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

private:
    char* DigestBufferLines(char *aBuffer, nsCAutoString& aString);

    nsCOMPtr<nsIURI>    mUri;

    nsCAutoString       mBuffer;            // buffered data.
    PRBool              mSentHeading;
    nsIStreamListener   *mFinalListener; // this guy gets the converted data via his OnDataAvailable()
    nsIChannel          *mPartChannel;  // the channel for the given part we're processing.
};  

#endif /* __nsgopherdirlistingdconv__h__ */
