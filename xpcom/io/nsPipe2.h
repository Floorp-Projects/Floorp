/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIPipe2_h___
#define nsIPipe2_h___

#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"

#define NS_IPIPEOBSERVER_IID                         \
{ /* 3c87cec0-4e28-11d3-9369-00104ba0fd40 */         \
    0x3c87cec0,                                      \
    0x4e28,                                          \
    0x11d3,                                          \
    {0x93, 0x69, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsIPipeObserver : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPIPEOBSERVER_IID);

    NS_IMETHOD OnFull() = 0;

    NS_IMETHOD OnWrite(PRUint32 amount) = 0;

    NS_IMETHOD OnEmpty() = 0;
};

/**
 * The signature for the reader function passed to WriteSegment. This 
 * specifies where the data should come from that gets written into the buffer.
 * Implementers should return the following:
 * @return NS_OK and readCount - if successfully read something
 * @return NS_BASE_STREAM_EOF - if no more to read
 * @return NS_BASE_STREAM_WOULD_BLOCK - if there is currently no data (in
 *   a non-blocking mode)
 * @return <other-error> - on failure
 */
typedef NS_CALLBACK(nsReadSegmentFun)(void* closure,
                                      char* toRawSegment,
                                      PRUint32 fromOffset,
                                      PRUint32 count,
                                      PRUint32 *readCount);

/**
 * The signature of the writer function passed to ReadSegments. This
 * specifies where the data should go that gets read from the buffer.
 * Implementers should return the following:
 * @return NS_OK and writeCount - if successfully wrote something
 * @return NS_BASE_STREAM_CLOSED - if no more can be written
 * @return NS_BASE_STREAM_WOULD_BLOCK - if there is currently space to write (in
 *   a non-blocking mode)
 * @return <other-error> - on failure
 */
typedef NS_CALLBACK(nsWriteSegmentFun)(void* closure,
                                       const char* fromRawSegment,
                                       PRUint32 toOffset,
                                       PRUint32 count,
                                       PRUint32 *writeCount);

#define NS_PIPE_DEFAULT_SEGMENT_SIZE    4096
#define NS_PIPE_DEFAULT_BUFFER_SIZE     (1024*1024)

extern NS_COM nsresult
NS_NewPipe(nsIBufferInputStream* *inStrResult,
           nsIBufferOutputStream* *outStrResult,
           nsIPipeObserver* observer = nsnull,
           PRUint32 segmentSize = NS_PIPE_DEFAULT_SEGMENT_SIZE,
           PRUint32 maxSize = NS_PIPE_DEFAULT_BUFFER_SIZE);

////////////////////////////////////////////////////////////////////////////////

#endif // nsIPipe2_h___
