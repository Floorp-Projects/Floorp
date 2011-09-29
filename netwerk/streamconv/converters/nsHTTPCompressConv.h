/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   David Dick <ddick@cpan.org>
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

#if !defined (__nsHTTPCompressConv__h__)
#define	__nsHTTPCompressConv__h__	1

#include "nsIStreamConverter.h"
#include "nsIStringStream.h"
#include "nsCOMPtr.h"

#include "zlib.h"

#define NS_HTTPCOMPRESSCONVERTER_CID                \
{                                                   \
    /* 66230b2b-17fa-4bd3-abf4-07986151022d */      \
    0x66230b2b,                                     \
    0x17fa,                                         \
    0x4bd3,                                         \
    {0xab, 0xf4, 0x07, 0x98, 0x61, 0x51, 0x02, 0x2d}\
}


#define	HTTP_DEFLATE_TYPE		"deflate"
#define	HTTP_GZIP_TYPE	        "gzip"
#define	HTTP_X_GZIP_TYPE	    "x-gzip"
#define	HTTP_COMPRESS_TYPE	    "compress"
#define	HTTP_X_COMPRESS_TYPE	"x-compress"
#define	HTTP_IDENTITY_TYPE	    "identity"
#define	HTTP_UNCOMPRESSED_TYPE	"uncompressed"

typedef enum    {
        HTTP_COMPRESS_GZIP,
        HTTP_COMPRESS_DEFLATE,
        HTTP_COMPRESS_COMPRESS,
        HTTP_COMPRESS_IDENTITY
    }   CompressMode;

class nsHTTPCompressConv	: public nsIStreamConverter	{
public:
    // nsISupports methods
    NS_DECL_ISUPPORTS

	NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    // nsIStreamConverter methods
    NS_DECL_NSISTREAMCONVERTER


    nsHTTPCompressConv ();
    virtual ~nsHTTPCompressConv ();

private:

    nsIStreamListener   *mListener; // this guy gets the converted data via his OnDataAvailable ()
	CompressMode        mMode;

    unsigned char *mOutBuffer;
    unsigned char *mInpBuffer;

    PRUint32	mOutBufferLen;
    PRUint32	mInpBufferLen;
	
    nsCOMPtr<nsISupports>   mAsyncConvContext;
    nsCOMPtr<nsIStringInputStream>  mStream;

    nsresult do_OnDataAvailable (nsIRequest *request, nsISupports *aContext,
                                 PRUint32 aSourceOffset, const char *buffer,
                                 PRUint32 aCount);

    bool        mCheckHeaderDone;
    bool        mStreamEnded;
    bool        mStreamInitialized;
    bool        mDummyStreamInitialised;

    z_stream d_stream;
    unsigned mLen, hMode, mSkipCount, mFlags;

    PRUint32 check_header (nsIInputStream *iStr, PRUint32 streamLen, nsresult *rv);
};


#endif
