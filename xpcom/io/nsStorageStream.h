/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#define NS_STORAGESTREAM_CID                       \
{ /* 669a9795-6ff7-4ed4-9150-c34ce2971b63 */       \
  0x669a9795,                                      \
  0x6ff7,                                          \
  0x4ed4,                                          \
  {0x91, 0x50, 0xc3, 0x4c, 0xe2, 0x97, 0x1b, 0x63} \
}

#define NS_STORAGESTREAM_CONTRACTID "@mozilla.org/storagestream;1"
#define NS_STORAGESTREAM_CLASSNAME "Storage Stream"

class nsSegmentedBuffer;

class nsStorageStream : public nsIStorageStream,
                        public nsIOutputStream
{
public:
    nsStorageStream();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTORAGESTREAM
    NS_DECL_NSIOUTPUTSTREAM

    friend class nsStorageInputStream;

private:
    ~nsStorageStream();

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
