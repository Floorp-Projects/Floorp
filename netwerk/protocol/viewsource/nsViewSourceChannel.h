/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsViewSourceChannel_h___
#define nsViewSourceChannel_h___

#include "mozilla/Attributes.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "nsCOMPtr.h"
#include "nsICachingChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIFormPOSTActionChannel.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsIViewSourceChannel.h"
#include "nsIChildChannel.h"
#include "nsString.h"

class nsViewSourceChannel final : public nsIViewSourceChannel,
                                  public nsIStreamListener,
                                  public nsIHttpChannel,
                                  public nsIHttpChannelInternal,
                                  public nsICachingChannel,
                                  public nsIFormPOSTActionChannel,
                                  public nsIChildChannel,
                                  public nsIInterfaceRequestor,
                                  public nsIChannelEventSink {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSIIDENTCHANNEL
  NS_DECL_NSIVIEWSOURCECHANNEL
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIHTTPCHANNEL
  NS_DECL_NSICHILDCHANNEL
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK
  NS_FORWARD_SAFE_NSICACHEINFOCHANNEL(mCacheInfoChannel)
  NS_FORWARD_SAFE_NSICACHINGCHANNEL(mCachingChannel)
  NS_FORWARD_SAFE_NSIUPLOADCHANNEL(mUploadChannel)
  NS_FORWARD_SAFE_NSIFORMPOSTACTIONCHANNEL(mPostChannel)
  NS_FORWARD_SAFE_NSIHTTPCHANNELINTERNAL(mHttpChannelInternal)

  // nsViewSourceChannel methods:
  nsViewSourceChannel() = default;

  [[nodiscard]] nsresult Init(nsIURI* uri, nsILoadInfo* aLoadInfo);

  [[nodiscard]] nsresult InitSrcdoc(nsIURI* aURI, nsIURI* aBaseURI,
                                    const nsAString& aSrcdoc,
                                    nsILoadInfo* aLoadInfo);

  // Updates or sets the result principal URI of the underlying channel's
  // loadinfo to be prefixed with the "view-source:" schema as:
  //
  // mChannel.loadInfo.resultPrincipalURI = "view-source:" +
  //    (mChannel.loadInfo.resultPrincipalURI | mChannel.orignalURI);
  nsresult UpdateLoadInfoResultPrincipalURI();

 protected:
  ~nsViewSourceChannel() = default;
  void ReleaseListeners();

  nsTArray<mozilla::net::PreferredAlternativeDataTypeParams> mEmptyArray;

  // Clones aURI and prefixes it with "view-source:" schema,
  nsresult BuildViewSourceURI(nsIURI* aURI, nsIURI** aResult);

  // Called to update the forwarding channel members after the `mChannel` field
  // has been changed to reflect the new inner channel.
  void UpdateChannelInterfaces();

  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIHttpChannel> mHttpChannel;
  nsCOMPtr<nsIHttpChannelInternal> mHttpChannelInternal;
  nsCOMPtr<nsICachingChannel> mCachingChannel;
  nsCOMPtr<nsICacheInfoChannel> mCacheInfoChannel;
  nsCOMPtr<nsIUploadChannel> mUploadChannel;
  nsCOMPtr<nsIFormPOSTActionChannel> mPostChannel;
  nsCOMPtr<nsIChildChannel> mChildChannel;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsIURI> mBaseURI;
  nsCString mContentType;
  bool mIsDocument{false};  // keeps track of the LOAD_DOCUMENT_URI flag
  bool mOpened{false};
  bool mIsSrcdocChannel{false};
};

#endif /* nsViewSourceChannel_h___ */
