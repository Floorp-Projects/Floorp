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

#include "nsISupports.h"
#include "nscore.h"

class nsIInputStream;
class nsIAllocator;
class nsIBufferInputStream;
class nsIBufferOutputStream;

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

typedef NS_CALLBACK(nsReadSegmentFun)(void* closure,
                                      char* toRawSegment, 
                                      PRUint32 fromOffset,
                                      PRUint32 count,
                                      PRUint32 *readCount);
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
                    nsIAllocator* allocator) = 0;

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
     */
    NS_IMETHOD GetReadSegment(PRUint32 segmentLogicalOffset, 
                              const char* *resultSegment,
                              PRUint32 *resultSegmentLen) = 0;

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
     */
    NS_IMETHOD GetWriteSegment(char* *resultSegment,
                               PRUint32 *resultSegmentLen) = 0;

    /**
     * Sets an EOF marker (typcially done by the writer) so that a reader can be informed
     * when all the data in the buffer is consumed. After the EOF marker has been
     * set, all subsequent calls to the above write methods will return NS_BASE_STREAM_EOF.
     */
    NS_IMETHOD SetEOF() = 0;

    /**
     * Tests whether EOF marker is set. Note that this does not necessarily mean that 
     * all the data in the buffer has yet been consumed.
     */
    NS_IMETHOD AtEOF(PRBool *result) = 0;

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
};

extern NS_COM nsresult
NS_NewBuffer(nsIBuffer* *result,
             PRUint32 growBySize, PRUint32 maxSize);

extern NS_COM nsresult
NS_NewPageBuffer(nsIBuffer* *result,
                 PRUint32 growBySize, PRUint32 maxSize);

extern NS_COM nsresult
NS_NewBufferInputStream(nsIBufferInputStream* *result,
                        nsIBuffer* buffer, PRBool blocking = PR_FALSE);

extern NS_COM nsresult
NS_NewBufferOutputStream(nsIBufferOutputStream* *result,
                         nsIBuffer* buffer, PRBool blocking = PR_FALSE);

extern NS_COM nsresult
NS_NewPipe2(nsIBufferInputStream* *inStrResult,
           nsIBufferOutputStream* *outStrResult,
           PRUint32 growBySize, PRUint32 maxSize);

#endif // nsIBuffer_h___
