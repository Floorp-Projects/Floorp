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

#if !defined (__nsHTTPChunkConv__h__)
#define	__nsHTTPChunkConv__h__	1

#include "nsIStreamConverter.h"
#include "nsIFactory.h"
#include "nsCOMPtr.h"

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
			CHUNK_STATE_FINAL
		}	ChunkState;

#define	HTTP_CHUNK_TYPE		"chunked"
#define	HTTP_UNCHUNK_TYPE	"unchunked"

class nsHTTPChunkConv	: public nsIStreamConverter	{
public:
    // nsISupports methods
    NS_DECL_ISUPPORTS

	NS_DECL_NSISTREAMOBSERVER
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
    
    nsCOMPtr<nsISupports>   mAsyncConvContext;
};


#endif
