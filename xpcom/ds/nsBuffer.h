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

#ifndef nsBuffer_h___
#define nsBuffer_h___

#include "nsIBuffer.h"
#include "nscore.h"
#include "prclist.h"
#include "nsIAllocator.h"

class nsBuffer : public nsIBuffer {
public:
    NS_DECL_ISUPPORTS

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    // nsIBuffer methods:
    NS_IMETHOD Init(PRUint32 growBySize, PRUint32 maxSize,
                    nsIAllocator* allocator);
    NS_IMETHOD Read(char* toBuf, PRUint32 bufLen, PRUint32 *readCount);
    NS_IMETHOD GetReadBuffer(PRUint32 startPosition, 
                             char* *result,
                             PRUint32 *readBufferLength);

    NS_IMETHOD Write(const char* fromBuf, PRUint32 bufLen, PRUint32 *writeCount);
    NS_IMETHOD WriteFrom(nsIInputStream* fromStream, PRUint32 count, PRUint32 *writeCount);
    NS_IMETHOD GetWriteBuffer(PRUint32 startPosition,
                              char* *result,
                              PRUint32 *writeBufferLength);
    NS_IMETHOD SetEOF();

    // nsBuffer methods:
    nsBuffer();
    virtual ~nsBuffer();

    nsresult PushWriteSegment();
    nsresult PopReadSegment();

protected:
    PRUint32            mGrowBySize;
    PRUint32            mMaxSize;
    nsIAllocator*       mAllocator;

    PRCList             mSegments;
    PRUint32            mBufferSize;
    
    char*               mReadSegment;
    char*               mReadSegmentEnd;
    char*               mReadCursor;

    char*               mWriteSegment;
    char*               mWriteSegmentEnd;
    char*               mWriteCursor;

    PRBool              mEOF;
};

#endif // nsBuffer_h___
