/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef __nsStreamXferOp_h
#define __nsStreamXferOp_h

#include "nsIStreamTransferOperation.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsIStreamListener.h"
#include "nsIChannel.h"
#include "nsIOutputStream.h"
#include "nsILocalFile.h"
#include "nsITransport.h"
#include "nsIDOMWindowInternal.h"
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
    nsStreamXferOp( nsIChannel *source, nsILocalFile *target );
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

    // nsIRequestObserver methods:
    NS_DECL_NSIREQUESTOBSERVER

private:
    nsCOMPtr<nsIChannel>      mInputChannel;
    nsCOMPtr<nsITransport>      mOutputTransport;
    nsCOMPtr<nsIOutputStream> mOutputStream;
    nsCOMPtr<nsILocalFile>    mOutputFile;
    nsCOMPtr<nsIDOMWindowInternal> mParentWindow;
    nsIObserver              *mObserver; // Not owned; owner should call SetObserver(0) prior
                                         // to this object getting destroyed.
    int                       mContentLength;
    unsigned long             mBytesProcessed;
    PRBool                    mError;
}; // nsStreamXferOp

#endif
