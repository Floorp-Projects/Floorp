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

#ifndef nsBufferedStreams_h__
#define nsBufferedStreams_h__

#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"

////////////////////////////////////////////////////////////////////////////////

class nsBufferedStream : public nsISeekableStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISEEKABLESTREAM

    nsBufferedStream();
    virtual ~nsBufferedStream();

    nsresult Close();

protected:
    nsresult Init(nsISupports* stream, PRUint32 bufferSize);
    NS_IMETHOD Fill() = 0;
    NS_IMETHOD Flush() = 0;

protected:
    PRUint32                    mBufferSize;
    char*                       mBuffer;
    // mBufferStartOffset is the offset relative to the start of mStream:
    PRUint32                    mBufferStartOffset;
    // mCursor is the read cursor for input streams, or write cursor for
    // output streams, and is relative to mBufferStartOffset:
    PRUint32                    mCursor;
    // mFillPoint is the amount available in the buffer for input streams,
    // or the end of the buffer for output streams, and is relative to 
    // mBufferStartOffset:
    PRUint32                    mFillPoint;
    nsISupports*                mStream;        // cast to appropriate subclass
};

////////////////////////////////////////////////////////////////////////////////

class nsBufferedInputStream : public nsBufferedStream,
                              public nsIBufferedInputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIBUFFEREDINPUTSTREAM

    nsBufferedInputStream() : nsBufferedStream() {}
    virtual ~nsBufferedInputStream() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsIInputStream* Source() { 
        return (nsIInputStream*)mStream;
    }

protected:
    NS_IMETHOD Fill();
    NS_IMETHOD Flush() { return NS_OK; } // no-op for input streams
};

////////////////////////////////////////////////////////////////////////////////

class nsBufferedOutputStream : public nsBufferedStream, 
                               public nsIBufferedOutputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOUTPUTSTREAM
    NS_DECL_NSIBUFFEREDOUTPUTSTREAM

    nsBufferedOutputStream() : nsBufferedStream() {}
    virtual ~nsBufferedOutputStream() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsIOutputStream* Sink() { 
        return (nsIOutputStream*)mStream;
    }

protected:
    NS_IMETHOD Fill() { return NS_OK; } // no-op for output streams
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsBufferedStreams_h__
