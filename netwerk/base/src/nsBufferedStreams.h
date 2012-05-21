/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBufferedStreams_h__
#define nsBufferedStreams_h__

#include "nsIBufferedStreams.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISafeOutputStream.h"
#include "nsISeekableStream.h"
#include "nsIStreamBufferAccess.h"
#include "nsCOMPtr.h"
#include "nsIIPCSerializable.h"

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

    PRUint32                    mBufferSize;
    char*                       mBuffer;

    // mBufferStartOffset is the offset relative to the start of mStream.
    PRInt64                     mBufferStartOffset;

    // mCursor is the read cursor for input streams, or write cursor for
    // output streams, and is relative to mBufferStartOffset.
    PRUint32                    mCursor;

    // mFillPoint is the amount available in the buffer for input streams,
    // or the high watermark of bytes written into the buffer, and therefore
    // is relative to mBufferStartOffset.
    PRUint32                    mFillPoint;

    nsISupports*                mStream;        // cast to appropriate subclass

    bool                        mBufferDisabled;
    bool                        mEOF;  // True if mStream is at EOF
    PRUint8                     mGetBufferCount;
};

////////////////////////////////////////////////////////////////////////////////

class nsBufferedInputStream : public nsBufferedStream,
                              public nsIBufferedInputStream,
                              public nsIStreamBufferAccess,
                              public nsIIPCSerializable
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIBUFFEREDINPUTSTREAM
    NS_DECL_NSISTREAMBUFFERACCESS
    NS_DECL_NSIIPCSERIALIZABLE

    nsBufferedInputStream() : nsBufferedStream() {}
    virtual ~nsBufferedInputStream() {}

    static nsresult
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
                               public nsISafeOutputStream,
                               public nsIBufferedOutputStream,
                               public nsIStreamBufferAccess
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOUTPUTSTREAM
    NS_DECL_NSISAFEOUTPUTSTREAM
    NS_DECL_NSIBUFFEREDOUTPUTSTREAM
    NS_DECL_NSISTREAMBUFFERACCESS

    nsBufferedOutputStream() : nsBufferedStream() {}
    virtual ~nsBufferedOutputStream() { nsBufferedOutputStream::Close(); }

    static nsresult
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsIOutputStream* Sink() { 
        return (nsIOutputStream*)mStream;
    }

protected:
    NS_IMETHOD Fill() { return NS_OK; } // no-op for input streams

    nsCOMPtr<nsISafeOutputStream> mSafeStream; // QI'd from mStream
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsBufferedStreams_h__
