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

#ifndef nsIBuffer_h___
#define nsIBuffer_h___

/**
 * nsIBuffer is something that we use to implement pipes (buffered
 * input/output stream pairs). It might be useful to you for other
 * purposes, but if not, oh well.
 *
 * One of the important things to understand about pipes is how
 * they work with respect to EOF and result values. The following
 * table describes:
 *
 *               | empty & not EOF    | full               | reader closed | writer closed                     |
 * -------------------------------------------------------------------------------------------------------------
 * buffer Read   | readCount == 0     | readCount == N     | N/A           | readCount == N, return NS_OK -or- |
 * operations    | return WOULD_BLOCK | return NS_OK       |               | readCount == 0, return EOF        |
 * -------------------------------------------------------------------------------------------------------------
 * buffer Write  | writeCount == N    | writeCount == 0    | N/A           | assertion!                        | 
 * operations    | return NS_OK       | return WOULD_BLOCK |               |                                   |
 * -------------------------------------------------------------------------------------------------------------
 * input stream  | readCount == 0     | readCount == N     | assertion!    | readCount == N, return NS_OK -or- |
 * Read ops      | return WOULD_BLOCK | return NS_OK       |               | readCount == 0, return EOF        |
 * -------------------------------------------------------------------------------------------------------------
 * output stream | writeCount == N    | writeCount == 0    | return        | assertion!                        | 
 * Write ops     | return NS_OK       | return WOULD_BLOCK | STREAM_CLOSED |                                   |
 * -------------------------------------------------------------------------------------------------------------
 */

#include "nsISupports.h"
#include "nscore.h"

class nsIInputStream;
class nsIAllocator;
class nsIBufferInputStream;
class nsIBufferOutputStream;
class nsIBufferObserver;

#define NS_IBUFFER_IID                               \
{ /* 1eebb300-fb8b-11d2-9324-00104ba0fd40 */         \
    0x1eebb300,                                      \
    0xfb8b,                                          \
    0x11d2,                                          \
    {0x93, 0x24, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

#define NS_BUFFER_CID                                \
{ /* 5dbe4de0-fbab-11d2-9324-00104ba0fd40 */         \
    0x5dbe4de0,                                      \
    0xfbab,                                          \
    0x11d2,                                          \
    {0x93, 0x24, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

#define NS_BUFFER_PROGID "component://netscape/buffer"
#define NS_BUFFER_CLASSNAME "Buffer"

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

class nsIBuffer : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IBUFFER_IID);

    /**
     * The segment overhead is the amount of space chopped out of each
     * segment for implementation purposes. The remainder of the segment
     * is available for data, e.g.:
     *     segmentDataSize = growBySize - SEGMENT_OVERHEAD;
     */
    enum { SEGMENT_OVERHEAD = 8 };

    /**
     * Initializes a buffer. The segment size (including overhead) will
     * start from and increment by the growBySize, until reaching maxSize.
     * The size of the data that can fit in a segment will be the growBySize
     * minus SEGMENT_OVERHEAD bytes.
     */
    NS_IMETHOD Init(PRUint32 growBySize, PRUint32 maxSize,
                    nsIBufferObserver* observer, nsIAllocator* allocator) = 0;

    ////////////////////////////////////////////////////////////////////////////
    // Methods for Readers

    /**
     * Reads from the read cursor into a char buffer up to a specified length.
     */
    NS_IMETHOD Read(char* toBuf, PRUint32 bufLen, PRUint32 *readCount) = 0;

    /**
     * This read method allows you to pass a callback function that gets called
     * repeatedly for each buffer segment until the entire amount is read.
     * This avoids the need to copy data to/from and intermediate buffer.
     */
    NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void* closure, PRUint32 count,
                            PRUint32 *readCount) = 0;

    /**
     * Returns the raw char buffer segment and its length available for reading.
     * @param segmentLogicalOffset - The offset from the current read cursor for
     *   the segment to be returned. If this is beyond the available written area,
     *   NULL is returned for the resultSegment.
     * @param resultSegment - The resulting read segment.
     * @param resultSegmentLength - The resulting read segment length.
     *
     * @return NS_OK - if a read segment is successfully returned, or if
     *   the requested offset is at or beyond the write cursor (in which case
     *   the resultSegment will be nsnull and the resultSegmentLen will be 0)
     * @return NS_BASE_STREAM_WOULD_BLOCK - if the buffer size becomes 0
     * @return any error set by SetCondition if the requested offset is at
     *   or beyond the write cursor (in which case the resultSegment will be
     *   nsnull and the resultSegmentLen will be 0). Note that NS_OK will be
     *   returned if SetCondition has not been called. 
     * @return any error returned by OnEmpty
     */
    NS_IMETHOD GetReadSegment(PRUint32 segmentLogicalOffset,
                              const char* *resultSegment,
                              PRUint32 *resultSegmentLen) = 0;

    /**
     * Returns the amount of data currently in the buffer available for reading.
     */
    NS_IMETHOD GetReadableAmount(PRUint32 *amount) = 0;

    /**
     * Searches for a string in the buffer. Since the buffer has a notion
     * of EOF, it is possible that the string may at some time be in the
     * buffer, but is is not currently found up to some offset. Consequently,
     * both the found and not found cases return an offset:
     *    if found, return offset where it was found
     *    if not found, return offset of the first byte not searched
     * In the case the buffer is at EOF and the string is not found, the first
     * byte not searched will correspond to the length of the buffer.
     */
    NS_IMETHOD Search(const char* forString, PRBool ignoreCase,
                      PRBool *found, PRUint32 *offsetSearchedTo) = 0;

    /**
     * Sets that the reader has closed their end of the stream.
     */
    NS_IMETHOD ReaderClosed(void) = 0;

    /**
     * Tests whether EOF marker is set. Note that this does not necessarily mean that
     * all the data in the buffer has yet been consumed.
     */
    NS_IMETHOD GetCondition(nsresult *result) = 0;

    ////////////////////////////////////////////////////////////////////////////
    // Methods for Writers

    /**
     * Writes from a char buffer up to a specified length.
     * @param writeCount - The amount that could be written. If the buffer becomes full,
     *   this could be less then the specified bufLen.
     */
    NS_IMETHOD Write(const char* fromBuf, PRUint32 bufLen, PRUint32 *writeCount) = 0;

    /**
     * Writes from an input stream up to a specified count of bytes.
     * @param writeCount - The amount that could be written. If the buffer becomes full,
     *   this could be less then the specified count.
     */
    NS_IMETHOD WriteFrom(nsIInputStream* fromStream, PRUint32 count, PRUint32 *writeCount) = 0;

    /**
     * This write method allows you to pass a callback function that gets called
     * repeatedly for each buffer segment until the entire amount is written.
     * This avoids the need to copy data to/from and intermediate buffer.
     */
    NS_IMETHOD WriteSegments(nsReadSegmentFun reader, void* closure, PRUint32 count,
                             PRUint32 *writeCount) = 0;

    /**
     * Returns the raw char buffer segment and its length available for writing.
     * @param resultSegment - The resulting write segment.
     * @param resultSegmentLength - The resulting write segment length.
     *
     * @return NS_OK - if there is a segment available to write to
     * @return NS_BASE_STREAM_CLOSED - if ReaderClosed has been called
     * @return NS_BASE_STREAM_WOULD_BLOCK - if the max buffer size is exceeded
     * @return NS_ERROR_OUT_OF_MEMORY - if a new segment could not be allocated
     * @return any error returned by OnFull
     */
    NS_IMETHOD GetWriteSegment(char* *resultSegment,
                               PRUint32 *resultSegmentLen) = 0;

    /**
     * Returns the amount of space currently in the buffer available for writing.
     */
    NS_IMETHOD GetWritableAmount(PRUint32 *amount) = 0;

    /**
     * Returns whether the reader has closed their end of the stream.
     */
    NS_IMETHOD GetReaderClosed(PRBool *result) = 0;

    /**
     * Sets an EOF marker (typcially done by the writer) so that a reader can be informed
     * when all the data in the buffer is consumed. After the EOF marker has been
     * set, all subsequent calls to the above write methods will return NS_BASE_STREAM_EOF.
     */
    NS_IMETHOD SetCondition(nsresult condition) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#define NS_IBUFFEROBSERVER_IID                       \
{ /* 0c18bef0-22a8-11d3-9349-00104ba0fd40 */         \
    0x0c18bef0,                                      \
    0x22a8,                                          \
    0x11d3,                                          \
    {0x93, 0x49, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

/**
 * A buffer observer is used to detect when the buffer becomes completely full
 * or completely empty.
 */
class nsIBufferObserver : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IBUFFEROBSERVER_IID);

    NS_IMETHOD OnFull(nsIBuffer* buffer) = 0;

    NS_IMETHOD OnWrite(nsIBuffer*, PRUint32 amount) = 0;

    NS_IMETHOD OnEmpty(nsIBuffer* buffer) = 0;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Creates a new buffer.
 * @param observer - may be null
 */
extern NS_COM nsresult
NS_NewBuffer(nsIBuffer* *result,
             PRUint32 growBySize, PRUint32 maxSize,
             nsIBufferObserver* observer);

/**
 * Creates a new buffer, allocating segments from virtual memory pages.
 */
extern NS_COM nsresult
NS_NewPageBuffer(nsIBuffer* *result,
                 PRUint32 growBySize, PRUint32 maxSize,
                 nsIBufferObserver* observer);

extern NS_COM nsresult
NS_NewBufferInputStream(nsIBufferInputStream* *result,
                        nsIBuffer* buffer, PRBool blocking = PR_FALSE);

extern NS_COM nsresult
NS_NewBufferOutputStream(nsIBufferOutputStream* *result,
                         nsIBuffer* buffer, PRBool blocking = PR_FALSE);

extern NS_COM nsresult
NS_NewPipe(nsIBufferInputStream* *inStrResult,
           nsIBufferOutputStream* *outStrResult,
           PRUint32 growBySize, PRUint32 maxSize,
           PRBool blocking, nsIBufferObserver* observer);

////////////////////////////////////////////////////////////////////////////////

#endif // nsIBuffer_h___
