/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#if !defined (__nsHTTPChunkConv__h__)
#define	__nsHTTPChunkConv__h__	1

#include "nsIStreamConverter.h"
#include "nsIFactory.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsString.h"

#include "nsISupportsPrimitives.h"

#define NS_HTTPCHUNKCONVERTER_CID					\
{													\
	/* 95ca98d9-2a96-48d6-a014-0dffa84834a1 */		\
    0x95ca98d9,										\
    0x2a96,											\
    0x48d6,											\
    {0xa0, 0x14, 0x0d, 0xff, 0xa8, 0x48, 0x34, 0xa1}\
}

static NS_DEFINE_CID(kHTTPChunkConverterCID, NS_HTTPCHUNKCONVERTER_CID);

typedef	enum emum_ChunkMode
		{
			DO_CHUNKING, DO_UNCHUNKING
		}	ChunkMode;

typedef	enum enum_ChunkState
		{
			CHUNK_STATE_INIT,
			CHUNK_STATE_CR,
			CHUNK_STATE_LF,
			CHUNK_STATE_LENGTH,
			CHUNK_STATE_DATA,
			CHUNK_STATE_CR_FINAL,
			CHUNK_STATE_LF_FINAL,
			CHUNK_STATE_FINAL,
            CHUNK_STATE_TRAILER_HEADER,
            CHUNK_STATE_TRAILER_VALUE,
            CHUNK_STATE_TRAILER,
            CHUNK_STATE_DONE
		}	ChunkState;

#define	HTTP_CHUNK_TYPE		"chunked"
#define	HTTP_UNCHUNK_TYPE	"unchunked"

class nsHTTPChunkConvContext;

class nsHTTPChunkConv	: public nsIStreamConverter	{
public:
    // nsISupports methods
    NS_DECL_ISUPPORTS

	NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    // nsIStreamConverter methods
    NS_DECL_NSISTREAMCONVERTER


    nsHTTPChunkConv ();
    virtual ~nsHTTPChunkConv ();

private:

    nsIStreamListener   *mListener; // this guy gets the converted data via his OnDataAvailable ()
	ChunkState	mState;
	ChunkMode	 mMode;
	char *mChunkBuffer;
	PRUint32	mChunkBufferLength;
	PRUint32	mChunkBufferPos;
	
	char mLenBuf[20];
	PRUint32	mLenBufCnt;
    
    nsCOMPtr<nsISupportsVoid> mAsyncConvContext;

    char        mValueBuf [4096];
    PRUint32    mValueBufLen;

    char        mHeaderBuf [400];
    PRUint32    mHeaderBufLen;

    PRUint32    mHeadersCount;
    PRUint32    mHeadersExpected;

    nsHTTPChunkConvContext  *mChunkContext;
};

class nsHTTPChunkConvHeaderEntry {
public:

    nsHTTPChunkConvHeaderEntry (const char *aName, const char* aValue)
    {
        mName  = aName;
        mValue = aValue;
    }

    ~nsHTTPChunkConvHeaderEntry()
    {
    }

    nsCString   mName;
    nsCString   mValue;
};

class nsHTTPChunkConvContext    {
public:

    nsHTTPChunkConvContext ()
            : mEof (PR_FALSE), mHeadersCount (0)
    {
    }

    ~nsHTTPChunkConvContext ()
    {
        PRInt32 i = mHeaders.Count ();

        while (i > 0)
        {
            nsHTTPChunkConvHeaderEntry *e = (nsHTTPChunkConvHeaderEntry *)mHeaders.RemoveElementAt (i - 1);
            delete e;
        }
    }

    void SetEOF (PRBool eof)
    {
        mEof = eof;
    }

    PRBool  GetEOF ()   {   return mEof;    }

    nsVoidArray * GetAllHeaders ()
    {
        return &mHeaders;
    }

    PRUint32 GetTrailerHeaderCount ()
    {
        return mHeadersCount;
    }
    
    void AddTrailerHeader (const char *header)
    {
        nsCStringKey key (header);
        mTrailer.Put (&key, (void *) 1);

        mHeadersCount++;
    }

    void SetResponseHeader (const char *header, const char *value)
    {
        nsHTTPChunkConvHeaderEntry *e = new nsHTTPChunkConvHeaderEntry (header, value);
        mHeaders.AppendElement (e);
    }

private:

    PRBool  mEof;
    nsVoidArray mHeaders;
    nsHashtable mTrailer;

    PRUint32    mHeadersCount;
};


#endif
