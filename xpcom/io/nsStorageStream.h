/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

/*
 * The storage stream provides an internal buffer that can be filled by a
 * client using a single output stream.  One or more independent input streams
 * can be created to read the data out non-destructively.  The implementation
 * uses a segmented buffer internally to avoid realloc'ing of large buffers,
 * with the attendant performance loss and heap fragmentation.
 */

#ifndef _nsStorageStream_h_
#define _nsStorageStream_h_

#include "nsIStorageStream.h"
#include "nsIOutputStream.h"
#include "nsMemory.h"

class nsSegmentedBuffer;

class NS_COM nsStorageStream : public nsIStorageStream,
                               public nsIOutputStream
{
public:
    nsStorageStream();
    virtual ~nsStorageStream();
    
    NS_METHOD Init(PRUint32 segmentSize, PRUint32 maxSize, nsIMemory *segmentAllocator = 0);

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTORAGESTREAM
    NS_DECL_NSIOUTPUTSTREAM

    friend class nsStorageInputStream;

private:
    nsSegmentedBuffer* mSegmentedBuffer;
    PRUint32           mSegmentSize;       // All segments, except possibly the last, are of this size
                                           //   Must be power-of-2
    PRUint32           mSegmentSizeLog2;   // log2(mSegmentSize)
    PRBool             mWriteInProgress;   // true, if an un-Close'ed output stream exists
    PRInt32            mLastSegmentNum;    // Last segment # in use, -1 initially
    char*              mWriteCursor;       // Pointer to next byte to be written
    char*              mSegmentEnd;        // Pointer to one byte after end of segment
                                           //   containing the write cursor
    PRUint32           mLogicalLength;     // Number of bytes written to stream

    NS_METHOD Seek(PRInt32 aPosition);
    PRUint32 SegNum(PRUint32 aPosition)    {return aPosition >> mSegmentSizeLog2;}
    PRUint32 SegOffset(PRUint32 aPosition) {return aPosition & (mSegmentSize - 1);}
};

#endif //  _nsStorageStream_h_
