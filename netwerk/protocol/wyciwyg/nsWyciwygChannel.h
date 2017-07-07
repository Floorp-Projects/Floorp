/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWyciwygChannel_h___
#define nsWyciwygChannel_h___

#include "nsString.h"
#include "nsCOMPtr.h"

#include "nsILoadInfo.h"
#include "nsIWyciwygChannel.h"
#include "nsIStreamListener.h"
#include "nsICacheEntryOpenCallback.h"
#include "PrivateBrowsingChannel.h"
#include "mozilla/BasePrincipal.h"

class nsICacheEntry;
class nsICacheStorage;
class nsIInputStream;
class nsIInputStreamPump;
class nsILoadGroup;
class nsIOutputStream;
class nsIProgressEventSink;
class nsIURI;

extern mozilla::LazyLogModule gWyciwygLog;

//-----------------------------------------------------------------------------

class nsWyciwygChannel final: public nsIWyciwygChannel,
                              public nsIStreamListener,
                              public nsICacheEntryOpenCallback,
                              public mozilla::net::PrivateBrowsingChannel<nsWyciwygChannel>
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIWYCIWYGCHANNEL
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSICACHEENTRYOPENCALLBACK

    // nsWyciwygChannel methods:
    nsWyciwygChannel();

    nsresult Init(nsIURI *uri);

protected:
    virtual ~nsWyciwygChannel();

    nsresult ReadFromCache();
    nsresult EnsureWriteCacheEntry();
    nsresult GetCacheStorage(nsICacheStorage **_retval);
    nsresult OpenCacheEntryForReading(nsIURI *aURI);
    nsresult OpenCacheEntryForWriting(nsIURI *aURI);

    void WriteCharsetAndSourceToCache(int32_t aSource,
                                      const nsCString& aCharset);

    void NotifyListener();

    friend class mozilla::net::PrivateBrowsingChannel<nsWyciwygChannel>;

    enum EMode {
      NONE,
      WRITING,
      READING
    };

    EMode                               mMode;
    nsresult                            mStatus;
    bool                                mIsPending;
    bool                                mNeedToWriteCharset;
    int32_t                             mCharsetSource;
    nsCString                           mCharset;
    int64_t                             mContentLength;
    uint32_t                            mLoadFlags;
    mozilla::OriginAttributes           mOriginAttributes;
    nsCOMPtr<nsIURI>                    mURI;
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsISupports>               mOwner;
    nsCOMPtr<nsILoadInfo>               mLoadInfo;
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIProgressEventSink>      mProgressSink;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCOMPtr<nsIStreamListener>         mListener;
    nsCOMPtr<nsISupports>               mListenerContext;

    // reuse as much of this channel implementation as we can
    nsCOMPtr<nsIInputStreamPump>        mPump;

    // Cache related stuff
    nsCOMPtr<nsICacheEntry>             mCacheEntry;
    nsCOMPtr<nsIOutputStream>           mCacheOutputStream;
    nsCOMPtr<nsIInputStream>            mCacheInputStream;

    bool                                mNeedToSetSecurityInfo;
    nsCOMPtr<nsISupports>               mSecurityInfo;
};

/**
 * Casting nsWyciwygChannel to nsISupports is ambiguous.
 * This method handles that.
 */
inline nsISupports*
ToSupports(nsWyciwygChannel* p)
{
  return NS_ISUPPORTS_CAST(nsIStreamListener*, p);
}

#endif /* nsWyciwygChannel_h___ */
