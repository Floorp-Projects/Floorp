/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
#ifndef __nsStreamXferOp_h
#define __nsStreamXferOp_h

#include "nsIStreamTransferOperation.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIStreamListener.h"
#include "nsIChannel.h"
#include "nsIOutputStream.h"
#include "nsIFileSpec.h"

#include "nsCOMPtr.h"

class nsIDOMWindowInternal;

// Define USE_ASYNC_READ in order to implement stream transfer using AsyncRead
// (and synchronous file writes) versus AsyncWrite.
#define USE_ASYNC_READ

// Implementation of the stream transfer operation interface.
//
// Ownership model:  Objects of this class are created, via operator new, in
// nsStreamTransfer::SelectFileAndTransferLocation.  The resulting object is
// passed to a newly-created downloadProgress.xul dialog.  That dialog "owns"
// the object (and the creator releases it immediately).  The object's dtor
// should get called when the dialog closes.
//
class nsStreamXferOp : public nsIStreamTransferOperation,
                       public nsIInterfaceRequestor,
                       public nsIProgressEventSink,
#ifdef USE_ASYNC_READ
                       public nsIStreamListener {
#else
                       public nsIStreamObserver {
#endif
public:
    // ctor/dtor
    nsStreamXferOp( nsIChannel *source, nsIFileSpec *target );
    virtual ~nsStreamXferOp();

    // Implementation.
    NS_IMETHOD OpenDialog( nsIDOMWindowInternal *parent );
    NS_IMETHOD OnError( int operation, nsresult rv );

    // Declare inherited interfaces.
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMTRANSFEROPERATION

    // nsIInterfaceRequestor methods:
    NS_DECL_NSIINTERFACEREQUESTOR

    // nsIProgressEventSink methods:
    NS_DECL_NSIPROGRESSEVENTSINK

#ifdef USE_ASYNC_READ
    // nsIStreamListener methods:
    NS_DECL_NSISTREAMLISTENER
#endif

    // nsIStreamObserver methods:
    NS_DECL_NSISTREAMOBSERVER

private:
    nsCOMPtr<nsIChannel>      mInputChannel;
    nsCOMPtr<nsIChannel>      mOutputChannel;
    nsCOMPtr<nsIOutputStream> mOutputStream;
    nsCOMPtr<nsIFileSpec>     mOutputSpec;
    nsIObserver              *mObserver; // Not owned; owner should call SetObserver(0) prior
                                         // to this object getting destroyed.
    int                       mContentLength;
    unsigned long             mBytesProcessed;
}; // nsStreamXferOp

#endif
