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
class nsIInputStream;
class nsIOutputStream;

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

class nsIBuffer : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IBUFFER_IID);

    enum { SEGMENT_OVERHEAD = 8 };

    /**
     * Initializes a buffer. The segment size (including overhead) will 
     * start from and increment by the growBySize, until reaching maxSize.
     * The size of the data that can fit in a segment will be the growBySize
     * minus SEGMENT_OVERHEAD bytes.
     */
    NS_IMETHOD Init(PRUint32 growBySize, PRUint32 maxSize,
                    nsIAllocator* allocator) = 0;

    NS_IMETHOD Read(char* toBuf, PRUint32 bufLen, PRUint32 *readCount) = 0;
    NS_IMETHOD GetReadBuffer(PRUint32 startPosition, 
                             char* *result,
                             PRUint32 *readBufferLength) = 0;

    NS_IMETHOD Write(const char* fromBuf, PRUint32 bufLen, PRUint32 *writeCount) = 0;
    NS_IMETHOD WriteFrom(nsIInputStream* fromStream, PRUint32 count, PRUint32 *writeCount) = 0;
    NS_IMETHOD GetWriteBuffer(PRUint32 startPosition,
                              char* *result,
                              PRUint32 *writeBufferLength) = 0;
    NS_IMETHOD SetEOF() = 0;
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
NS_NewBufferInputStream(nsIInputStream* *result,
                        nsIBuffer* buffer, PRBool blocking = PR_FALSE);

extern NS_COM nsresult
NS_NewBufferOutputStream(nsIOutputStream* *result,
                         nsIBuffer* buffer, PRBool blocking = PR_FALSE);

extern NS_COM nsresult
NS_NewPipe2(nsIInputStream* *inStrResult,
           nsIOutputStream* *outStrResult,
           PRUint32 growBySize, PRUint32 maxSize);

#endif // nsIBuffer_h___
