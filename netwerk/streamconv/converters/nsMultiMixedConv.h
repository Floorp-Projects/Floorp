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
#ifndef __nsmultimixedconv__h__
#define __nsmultimixedconv__h__

#include "nsIStreamConverter.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIIOService.h"

#define NS_MULTIMIXEDCONVERTER_CID                         \
{ /* 7584CE90-5B25-11d3-A175-0050041CAF44 */         \
    0x7584ce90,                                      \
    0x5b25,                                          \
    0x11d3,                                          \
    {0xa1, 0x75, 0x0, 0x50, 0x4, 0x1c, 0xaf, 0x44}       \
}
static NS_DEFINE_CID(kMultiMixedConverterCID,          NS_MULTIMIXEDCONVERTER_CID);

// The nsMultiMixedConv stream converter converts a stream of type "multipart/x-mixed-replace"
// to it's subparts. There was some debate as to whether or not the functionality desired
// when HTTP confronted this type required a stream converter. After all, this type really
// prompts various viewer related actions rather than stream conversion. There simply needs
// to be a piece in place that can strip out the multiple parts of a stream of this type, and 
// "display" them accordingly.
//
// With that said, this "stream converter" spends more time packaging up the sub parts of the
// main stream and sending them off the destination stream listener, than doing any real
// stream parsing/converting.
//
// WARNING: This converter requires that it's destination stream listener be able to handle
//   multiple OnStartRequest(), OnDataAvailable(), and OnStopRequest() call combinations.
//   Each series represents the beginning, data production, and ending phase of each sub-
//   part of the original stream.
//
// NOTE: this MIME-type is used by HTTP, *not* SMTP, or IMAP.
//
// NOTE: For reference, a general description of how this MIME type should be handled via
//   HTTP, see http://home.netscape.com/assist/net_sites/pushpull.html . Note that
//   real world server content deviates considerably from this overview.
//
// Implementation assumptions:
//  Assumed structue:
//  --BoundaryToken[\r]\n
//  content-type: foo/bar[\r]\n
//  ... (other headers if any)
//  [\r]\n (second line feed to delimit end of headers)
//  data
//  --BoundaryToken-- (end delimited by final "--")
//
// linebreaks can be either CRLF or LFLF. linebreaks preceeding
// boundary tokens are considered part of the data. BoundaryToken
// is any opaque string.
//  
//

class nsMultiMixedConv : public nsIStreamConverter {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMCONVERTER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISTREAMOBSERVER

    nsMultiMixedConv();
    virtual ~nsMultiMixedConv();

    nsresult Init();

    // For factory creation.
    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
        nsresult rv;
        if (aOuter)
            return NS_ERROR_NO_AGGREGATION;

        nsMultiMixedConv* _s = new nsMultiMixedConv();
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
    nsresult SendStart(nsIChannel *aChannel);
    nsresult SendStop();
    nsresult SendData(char *aBuffer, PRUint32 aLen);
    nsresult ParseHeaders(nsIChannel *aChannel, char *&aPtr,
                          PRUint32 &aLen, PRBool *_retval);
    PRInt8 PushOverLine(char *&aPtr, PRUint32 &aLen);
    char *FindToken(char *aCursor, PRUint32 aLen);
    nsresult BufferData(char *aData, PRUint32 aLen);

    // member data
    PRBool              mNewPart;        // Are we processing the beginning of a part?
	PRBool				mProcessingHeaders;
	PRBool				mEndingNewline;  // Did our last header parsing pass end with a newline?
    nsCOMPtr<nsIStreamListener> mFinalListener; // this guy gets the converted data via his OnDataAvailable()
    nsCOMPtr<nsIIOService>  mIOService;

	nsXPIDLCString		mToken;
    PRUint32            mTokenLen;

    PRUint16            mPartCount;     // the number of parts we've seen so far
    nsCOMPtr<nsIChannel>mPartChannel;   // the channel for the given part we're processing.
                                        // one channel per part.
    nsCOMPtr<nsISupports> mContext;
    nsCString           mContentType;
    PRInt32             mContentLength;
    
    char                *mBuffer;
    PRUint32            mBufLen;
};

#endif /* __nsmultimixedconv__h__ */
