/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsViewSourceChannel_h___
#define nsViewSourceChannel_h___

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIViewSourceChannel.h"
#include "nsIURI.h"
#include "nsIStreamListener.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsICachingChannel.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIUploadChannel.h"
#include "mozilla/Attributes.h"

class nsViewSourceChannel MOZ_FINAL : public nsIViewSourceChannel,
                                      public nsIStreamListener,
                                      public nsIHttpChannel,
                                      public nsIHttpChannelInternal,
                                      public nsICachingChannel,
                                      public nsIApplicationCacheChannel,
                                      public nsIUploadChannel
{

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIVIEWSOURCECHANNEL
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIHTTPCHANNEL
    NS_FORWARD_SAFE_NSICACHEINFOCHANNEL(mCachingChannel)
    NS_FORWARD_SAFE_NSICACHINGCHANNEL(mCachingChannel)
    NS_FORWARD_SAFE_NSIAPPLICATIONCACHECHANNEL(mApplicationCacheChannel)
    NS_FORWARD_SAFE_NSIAPPLICATIONCACHECONTAINER(mApplicationCacheChannel)
    NS_FORWARD_SAFE_NSIUPLOADCHANNEL(mUploadChannel)
    NS_FORWARD_SAFE_NSIHTTPCHANNELINTERNAL(mHttpChannelInternal)

    // nsViewSourceChannel methods:
    nsViewSourceChannel()
        : mIsDocument(false)
        , mOpened(false) {}

    nsresult Init(nsIURI* uri);

    nsresult InitSrcdoc(nsIURI* aURI, const nsAString &aSrcdoc,
                                    nsIURI* aBaseURI);

protected:
    ~nsViewSourceChannel() {}

    nsCOMPtr<nsIChannel>        mChannel;
    nsCOMPtr<nsIHttpChannel>    mHttpChannel;
    nsCOMPtr<nsIHttpChannelInternal>    mHttpChannelInternal;
    nsCOMPtr<nsICachingChannel> mCachingChannel;
    nsCOMPtr<nsIApplicationCacheChannel> mApplicationCacheChannel;
    nsCOMPtr<nsIUploadChannel>  mUploadChannel;
    nsCOMPtr<nsIStreamListener> mListener;
    nsCOMPtr<nsIURI>            mOriginalURI;
    nsCOMPtr<nsIURI>            mBaseURI;
    nsCString                   mContentType;
    bool                        mIsDocument; // keeps track of the LOAD_DOCUMENT_URI flag
    bool                        mOpened;
    bool                        mIsSrcdocChannel;
};

#endif /* nsViewSourceChannel_h___ */
