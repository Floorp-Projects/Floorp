/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is nsStorageTransport.h, released February 26, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#ifndef nsStorageTransport_h__
#define nsStorageTransport_h__

#include "nsITransport.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStreamListener.h"
#include "prclist.h"
#include "nsCOMPtr.h"

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"

/**
 * Each "stream-based" memory cache entry has one transport
 * associated with it.  The transport supports multiple 
 * simultaneous read requests and a single write request.
 */
class nsStorageTransport : public nsITransport
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITRANSPORT

    enum { DEFAULT_SEGMENT_SIZE = 1024 };
    enum { DEFAULT_BUFFER_SIZE = 1024 * 1024 };

    nsStorageTransport();
    virtual ~nsStorageTransport();

    nsresult Init(PRUint32 aSegmentSize = DEFAULT_SEGMENT_SIZE,
                  PRUint32 aBufferSize = DEFAULT_BUFFER_SIZE);

public: /* internal */

    /**
     * The transport request object returned by AsyncRead
     */
    class nsReadRequest : public PRCList
                        , public nsITransportRequest
                        , public nsIStreamListener
                        , public nsIInputStream
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSITRANSPORTREQUEST
        NS_DECL_NSIREQUEST
        NS_DECL_NSISTREAMLISTENER
        NS_DECL_NSIREQUESTOBSERVER
        NS_DECL_NSIINPUTSTREAM

        nsReadRequest();
        virtual ~nsReadRequest();

        void SetTransport(nsStorageTransport *t) { mTransport = t; }
        void SetTransferOffset(PRUint32 o) { mTransferOffset = o; }
        void SetTransferCount(PRUint32 c) { mTransferCount = c; }

        nsresult SetListener(nsIStreamListener *, nsISupports *);
        nsresult Process();

        PRBool IsWaitingForWrite() { return mWaitingForWrite; }

    private:
        nsStorageTransport         *mTransport;   // weak ref
        PRUint32                    mTransferOffset;
        PRUint32                    mTransferCount;
        nsresult                    mStatus;
        nsCOMPtr<nsIStreamListener> mListenerProxy;
        nsCOMPtr<nsIStreamListener> mListener;
        nsCOMPtr<nsISupports>       mListenerContext;
        PRPackedBool                mCanceled;
        PRPackedBool                mOnStartFired;
        PRPackedBool                mWaitingForWrite;
    };

    /**
     * A base class for the blocking streams
     */
    class nsBlockingStream
    {
    public:
        nsBlockingStream();
        virtual ~nsBlockingStream();

        void SetTransport(nsStorageTransport *t) { mTransport = t; }
        void SetTransferCount(PRUint32 c) { mTransferCount = c; }

    protected:
        nsStorageTransport *mTransport; // weak ref
        PRUint32            mTransferCount;
    };

    /**
     * The blocking input stream object returned by OpenInputStream
     */
    class nsInputStream : public PRCList
                        , public nsBlockingStream
                        , public nsIInputStream
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIINPUTSTREAM

        nsInputStream();
        virtual ~nsInputStream();

    private:
        PRUint32 mOffset;
    };

    /**
     * The blocking output stream object returned by OpenOutputStream
     */
    class nsOutputStream : public nsBlockingStream
                         , public nsIOutputStream
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIOUTPUTSTREAM

        nsOutputStream();
        virtual ~nsOutputStream();
    };

    /**
     * Methods called from the friend classes
     */
    nsresult GetReadSegment(PRUint32 aOffset, char **aPtr, PRUint32 *aCount); 
    nsresult GetWriteSegment(char **aPtr, PRUint32 *aCount);
    nsresult AddToBytesWritten(PRUint32 aCount);
    nsresult CloseOutputStream();
    nsresult ReadRequestCompleted(nsReadRequest *);
    nsresult Available(PRUint32 aStartingFrom, PRUint32 *aCount);

    PRBool HasWriter() { return (mOutputStream != nsnull); }

    void FireOnProgress(nsIRequest *aRequest,
                        nsISupports *aContext,
                        PRUint32 aOffset);

private:

    /**
     * Data is stored in a singly-linked list of segments
     */
    struct nsSegment
    {
        struct nsSegment *next;
        
        char *Data() { return NS_REINTERPRET_CAST(char *, this) + sizeof(*this); }
          // the data follows this structure (the two are allocated together)
    };

    /**
     * Internal methods
     */
    nsresult AddWriteSegment();

    void AppendSegment(nsSegment *);
    void DeleteSegments(nsSegment *);
    void DeleteAllSegments() { TruncateTo(0); }
    void TruncateTo(PRUint32 aOffset);

    nsSegment *GetNthSegment(PRUint32 aIndex);

private:
    nsOutputStream *mOutputStream; // weak ref
    PRCList         mInputStreams; // weak ref to objects
    PRCList         mReadRequests; // weak ref to objects

    PRUint32        mSegmentSize;
    PRUint32        mMaxSize;

    nsSegment      *mSegments;

    nsSegment      *mWriteSegment;
    PRUint32        mWriteCursor;

    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    nsCOMPtr<nsIProgressEventSink>  mProgressSink;
};

#endif
