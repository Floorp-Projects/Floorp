/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsFileTransport_h__
#define nsFileTransport_h__

#include "nsIChannel.h"
#include "nsIRunnable.h"
#include "nsFileSpec.h"
#include "prlock.h"
#include "nsIEventQueueService.h"
#include "nsIPipe.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"
#include "nsIStreamListener.h"
#include "nsIProgressEventSink.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStreamIO.h"
#include "nsIInterfaceRequestor.h"
#include "nsIFile.h"
#include "prlog.h"
#include "nsFileTransportService.h"

class nsIInterfaceRequestor;

class nsFileTransport : public nsIChannel, 
                        public nsIRunnable,
                        public nsIInputStreamObserver,
                        public nsIOutputStreamObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIINPUTSTREAMOBSERVER
    NS_DECL_NSIOUTPUTSTREAMOBSERVER
    NS_DECL_NSIRUNNABLE

    nsFileTransport();
    // Always make the destructor virtual: 
    virtual ~nsFileTransport();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    nsresult Init(nsFileTransportService *aService, nsIFile* file,
                  PRInt32 ioFlags,
                  PRInt32 perm);
    nsresult Init(nsFileTransportService *aService, const char* name, 
                  nsIInputStream* fromStream,
                  const char* contentType,
                  PRInt32 contentLength);
    nsresult Init(nsFileTransportService *aService, nsIStreamIO* io);

    void Process(void);
    void DoClose(void);

    enum XferState {
        CLOSED,
        OPEN_FOR_READ,
        START_READ,
        READING,
        END_READ,
        OPEN_FOR_WRITE,
        START_WRITE,
        WRITING,
        END_WRITE,
        CLOSING
    };

    enum RunState {
        RUNNING,
        SUSPENDED,
        CANCELED
    };

protected:
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIProgressEventSink>      mProgress;
    nsCOMPtr<nsIStreamIO>               mStreamIO;
    char*                               mContentType;
    PRUint32                            mBufferSegmentSize;
    PRUint32                            mBufferMaxSize;

    nsCOMPtr<nsISupports>               mContext;

    // mXferState is only changed by the file transport thread:
    XferState                           mXferState;
    // mRunState is only changed by the user's thread, but looked at by the
    // file transport thread:
    RunState                            mRunState;
    nsresult                            mCancelStatus;
    PRMonitor*                          mMonitor;

    // state variables:
    nsresult                            mStatus;
    PRUint32                            mOffset;
    PRInt32                             mTotalAmount;
    PRInt32                             mTransferAmount;
    nsLoadFlags                         mLoadAttributes;

    // reading state varialbles:
    nsCOMPtr<nsIStreamListener>         mListener;
    nsCOMPtr<nsIInputStream>            mSource;
    nsCOMPtr<nsIInputStream>            mInputStream;
    nsCOMPtr<nsIOutputStream>           mOutputStream;

    // writing state variables:
    nsCOMPtr<nsIStreamObserver>         mObserver;
    nsCOMPtr<nsIOutputStream>           mSink;
    char*                               mBuffer;
    nsCString                           mStreamName;
    nsFileTransportService*             mService;
};

#define NS_FILE_TRANSPORT_DEFAULT_SEGMENT_SIZE   (2*1024)
#define NS_FILE_TRANSPORT_DEFAULT_BUFFER_SIZE    (8*1024)

#endif // nsFileTransport_h__
