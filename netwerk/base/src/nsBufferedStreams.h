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
#include "nsIIPCSerializableInputStream.h"

////////////////////////////////////////////////////////////////////////////////

class nsBufferedStream : public nsISeekableStream
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSISEEKABLESTREAM

    nsBufferedStream();

    nsresult Close();

protected:
    virtual ~nsBufferedStream();

    nsresult Init(nsISupports* stream, uint32_t bufferSize);
    NS_IMETHOD Fill() = 0;
    NS_IMETHOD Flush() = 0;

    uint32_t                    mBufferSize;
    char*                       mBuffer;

    // mBufferStartOffset is the offset relative to the start of mStream.
    int64_t                     mBufferStartOffset;

    // mCursor is the read cursor for input streams, or write cursor for
    // output streams, and is relative to mBufferStartOffset.
    uint32_t                    mCursor;

    // mFillPoint is the amount available in the buffer for input streams,
    // or the high watermark of bytes written into the buffer, and therefore
    // is relative to mBufferStartOffset.
    uint32_t                    mFillPoint;

    nsISupports*                mStream;        // cast to appropriate subclass

    bool                        mBufferDisabled;
    bool                        mEOF;  // True if mStream is at EOF
    uint8_t                     mGetBufferCount;
};

////////////////////////////////////////////////////////////////////////////////

class nsBufferedInputStream : public nsBufferedStream,
                              public nsIBufferedInputStream,
                              public nsIStreamBufferAccess,
                              public nsIIPCSerializableInputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIBUFFEREDINPUTSTREAM
    NS_DECL_NSISTREAMBUFFERACCESS
    NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM

    nsBufferedInputStream() : nsBufferedStream() {}

    static nsresult
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsIInputStream* Source() { 
        return (nsIInputStream*)mStream;
    }

protected:
    virtual ~nsBufferedInputStream() {}

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

    static nsresult
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsIOutputStream* Sink() { 
        return (nsIOutputStream*)mStream;
    }

protected:
    virtual ~nsBufferedOutputStream() { nsBufferedOutputStream::Close(); }

    NS_IMETHOD Fill() { return NS_OK; } // no-op for output streams

    nsCOMPtr<nsISafeOutputStream> mSafeStream; // QI'd from mStream
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsBufferedStreams_h__
