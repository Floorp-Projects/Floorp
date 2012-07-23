/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWyciwygChannel_h___
#define nsWyciwygChannel_h___

#include "nsWyciwygProtocolHandler.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsCOMPtr.h"

#include "nsIWyciwygChannel.h"
#include "nsILoadGroup.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsIInputStreamPump.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIStreamListener.h"
#include "nsICacheListener.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIURI.h"
#include "nsIEventTarget.h"
#include "nsILoadContext.h"
#include "nsNetUtil.h"

extern PRLogModuleInfo * gWyciwygLog;

//-----------------------------------------------------------------------------

class nsWyciwygChannel: public nsIWyciwygChannel,
                        public nsIStreamListener,
                        public nsICacheListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIWYCIWYGCHANNEL
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSICACHELISTENER

    friend class nsWyciwygSetCharsetandSourceEvent;
    friend class nsWyciwygWriteEvent;
    friend class nsWyciwygCloseEvent;

    // nsWyciwygChannel methods:
    nsWyciwygChannel();
    virtual ~nsWyciwygChannel();

    nsresult Init(nsIURI *uri);

protected:
    nsresult WriteToCacheEntryInternal(const nsAString& aData, const nsACString& spec);
    void SetCharsetAndSourceInternal();
    nsresult CloseCacheEntryInternal(nsresult reason);

    nsresult ReadFromCache();
    nsresult OpenCacheEntry(const nsACString & aCacheKey, nsCacheAccessMode aWriteAccess);

    void WriteCharsetAndSourceToCache(PRInt32 aSource,
                                      const nsCString& aCharset);

    void NotifyListener();
    bool IsOnCacheIOThread();

    nsresult                            mStatus;
    bool                                mIsPending;
    bool                                mCharsetAndSourceSet;
    bool                                mNeedToWriteCharset;
    bool                                mPrivateBrowsing;
    PRInt32                             mCharsetSource;
    nsCString                           mCharset;
    PRInt32                             mContentLength;
    PRUint32                            mLoadFlags;
    nsCOMPtr<nsIURI>                    mURI;
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsISupports>               mOwner;
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIProgressEventSink>      mProgressSink;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCOMPtr<nsIStreamListener>         mListener;
    nsCOMPtr<nsISupports>               mListenerContext;

    // reuse as much of this channel implementation as we can
    nsCOMPtr<nsIInputStreamPump>        mPump;
    
    // Cache related stuff    
    nsCOMPtr<nsICacheEntryDescriptor>   mCacheEntry;
    nsCOMPtr<nsIOutputStream>           mCacheOutputStream;
    nsCOMPtr<nsIInputStream>            mCacheInputStream;
    nsCOMPtr<nsIEventTarget>            mCacheIOTarget;

    nsCOMPtr<nsISupports>               mSecurityInfo;
};

#endif /* nsWyciwygChannel_h___ */
