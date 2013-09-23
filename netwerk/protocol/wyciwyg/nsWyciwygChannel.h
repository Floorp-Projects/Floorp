/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWyciwygChannel_h___
#define nsWyciwygChannel_h___

#include "nsString.h"
#include "nsCOMPtr.h"

#include "nsIWyciwygChannel.h"
#include "nsIStreamListener.h"
#include "nsICacheListener.h"
#include "PrivateBrowsingChannel.h"

class nsICacheEntryDescriptor;
class nsIEventTarget;
class nsIInputStream;
class nsIInputStreamPump;
class nsILoadGroup;
class nsIOutputStream;
class nsIProgressEventSink;
class nsIURI;

extern PRLogModuleInfo * gWyciwygLog;

//-----------------------------------------------------------------------------

class nsWyciwygChannel: public nsIWyciwygChannel,
                        public nsIStreamListener,
                        public nsICacheListener,
                        public mozilla::net::PrivateBrowsingChannel<nsWyciwygChannel>
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
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

    void WriteCharsetAndSourceToCache(int32_t aSource,
                                      const nsCString& aCharset);

    void NotifyListener();
    bool IsOnCacheIOThread();

    friend class mozilla::net::PrivateBrowsingChannel<nsWyciwygChannel>;

    nsresult                            mStatus;
    bool                                mIsPending;
    bool                                mCharsetAndSourceSet;
    bool                                mNeedToWriteCharset;
    int32_t                             mCharsetSource;
    nsCString                           mCharset;
    int64_t                             mContentLength;
    uint32_t                            mLoadFlags;
    uint32_t                            mAppId;
    bool                                mInBrowser;
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
