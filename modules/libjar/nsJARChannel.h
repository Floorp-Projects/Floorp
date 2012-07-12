/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJARChannel_h__
#define nsJARChannel_h__

#include "nsIJARChannel.h"
#include "nsIJARURI.h"
#include "nsIInputStreamPump.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIStreamListener.h"
#include "nsIZipReader.h"
#include "nsIDownloader.h"
#include "nsILoadGroup.h"
#include "nsHashPropertyBag.h"
#include "nsIFile.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "prlog.h"

class nsJARInputThunk;

//-----------------------------------------------------------------------------

class nsJARChannel : public nsIJARChannel
                   , public nsIDownloadObserver
                   , public nsIStreamListener
                   , public nsHashPropertyBag
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIJARCHANNEL
    NS_DECL_NSIDOWNLOADOBSERVER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    nsJARChannel();
    virtual ~nsJARChannel();

    nsresult Init(nsIURI *uri);

private:
    nsresult CreateJarInput(nsIZipReaderCache *);
    nsresult EnsureJarInput(bool blocking);

#if defined(PR_LOGGING)
    nsCString                       mSpec;
#endif

    nsCOMPtr<nsIJARURI>             mJarURI;
    nsCOMPtr<nsIURI>                mOriginalURI;
    nsCOMPtr<nsIURI>                mAppURI;
    nsCOMPtr<nsISupports>           mOwner;
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    nsCOMPtr<nsISupports>           mSecurityInfo;
    nsCOMPtr<nsIProgressEventSink>  mProgressSink;
    nsCOMPtr<nsILoadGroup>          mLoadGroup;
    nsCOMPtr<nsIStreamListener>     mListener;
    nsCOMPtr<nsISupports>           mListenerContext;
    nsCString                       mContentType;
    nsCString                       mContentCharset;
    nsCString                       mContentDispositionHeader;
    /* mContentDisposition is uninitialized if mContentDispositionHeader is
     * empty */
    PRUint32                        mContentDisposition;
    PRInt32                         mContentLength;
    PRUint32                        mLoadFlags;
    nsresult                        mStatus;
    bool                            mIsPending;
    bool                            mIsUnsafe;

    nsJARInputThunk                *mJarInput;
    nsCOMPtr<nsIStreamListener>     mDownloader;
    nsCOMPtr<nsIInputStreamPump>    mPump;
    nsCOMPtr<nsIFile>               mJarFile;
    nsCOMPtr<nsIURI>                mJarBaseURI;
    nsCString                       mJarEntry;
    nsCString                       mInnerJarEntry;
};

#endif // nsJARChannel_h__
