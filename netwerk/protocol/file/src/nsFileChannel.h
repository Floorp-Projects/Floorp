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

#ifndef nsFileChannel_h__
#define nsFileChannel_h__

#include "nsIChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadGroup.h"
#include "nsIStreamListener.h"
#include "nsFileSpec.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "prlock.h"
#include "nsIEventQueueService.h"
#include "nsIPipe.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"        /* Solaris/gcc needed this here. */
#include "nsIProgressEventSink.h"
#include "nsITransport.h"

class nsFileChannel : public nsIFileChannel,
                      public nsIInterfaceRequestor,
                      public nsIStreamListener,
                      public nsIProgressEventSink
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIFILECHANNEL
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIPROGRESSEVENTSINK

    nsFileChannel();
    // Always make the destructor virtual: 
    virtual ~nsFileChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    nsresult Init(PRInt32 ioFlags, PRInt32 perm, nsIURI* uri);
    nsresult EnsureTransport();

protected:
    nsCOMPtr<nsIFile>                   mFile;
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIURI>                    mURI;
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    PRInt32                             mIOFlags;
    PRInt32                             mPerm;
    nsCOMPtr<nsITransport>              mFileTransport;
    nsCString                           mContentType;
    PRUint32                            mLoadAttributes;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCOMPtr<nsISupports>               mOwner;
    nsCOMPtr<nsIStreamListener>         mRealListener;
    PRUint32                            mTransferOffset;
    PRInt32                             mTransferCount;
    PRUint32                            mBufferSegmentSize;
    PRUint32                            mBufferMaxSize;
    nsresult                            mStatus;
    nsCOMPtr<nsIProgressEventSink>      mProgress;
    nsCOMPtr<nsIRequest>                mCurrentRequest;
#ifdef DEBUG
    PRThread*                           mInitiator;
#endif
};

#endif // nsFileChannel_h__
