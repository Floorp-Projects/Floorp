/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
#ifndef __nsStreamXferOp_h
#define __nsStreamXferOp_h

#include "nsString.h"
#include "nsIStreamTransferOperation.h"
#include "nsIStreamListener.h"
#include "nsIProgressEventSink.h"

class nsIDOMWindow;
class nsOutputFileStream;

// Implementation of the stream transfer operation interface.
//
// Ownership model:  Objects of this class are created, via operator new, in
// nsStreamTransfer::SelectFileAndTransferLocation.  The resulting object is
// passed to a newly-created downloadProgress.xul dialog.  That dialog "owns"
// the object (and the creator releases it immediately).  The object's dtor
// should get called when the dialog closes.
class nsStreamXferOp : public nsIStreamTransferOperation,
                       public nsIProgressEventSink,
                       public nsIStreamListener {
public:
    // ctor/dtor
    nsStreamXferOp( const nsString &source, const nsString &target );
    virtual ~nsStreamXferOp();

    // Implementation.
    NS_IMETHOD OpenDialog( nsIDOMWindow *parent );

    // Declare inherited interfaces.
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMTRANSFEROPERATION

    // nsIProgressEventSink methods:
    NS_DECL_NSIPROGRESSEVENTSINK

    // nsIStreamObserver methods:
    NS_DECL_NSISTREAMOBSERVER

    // nsIStreamListener methods:
    NS_DECL_NSISTREAMLISTENER

private:
    nsCString           mSource;
    nsCString           mTarget;
    nsIObserver        *mObserver; // Not owned; owner should call SetObserver(0) prior
                                   // to this object getting destroyed.
    PRUint32            mBufLen;
    char               *mBuffer;   // Owned; deleted in dtor.
    PRBool              mStopped;
    nsOutputFileStream *mOutput;   // Owned; deleted in dtor.
}; // nsStreamXferOp


#endif
