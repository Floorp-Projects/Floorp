/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_HttpChannelParent_h
#define mozilla_net_HttpChannelParent_h

#include "nsHttp.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/net/PHttpChannelParent.h"
#include "mozilla/net/NeckoCommon.h"
#include "nsIParentRedirectingChannel.h"
#include "nsIProgressEventSink.h"
#include "nsHttpChannel.h"

class nsICacheEntryDescriptor;
class nsIAssociatedContentSecurity;

namespace mozilla {

namespace dom{
class TabParent;
}

namespace net {

class HttpChannelParentListener;

class HttpChannelParent : public PHttpChannelParent
                        , public nsIParentRedirectingChannel
                        , public nsIProgressEventSink
                        , public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIPARENTCHANNEL
  NS_DECL_NSIPARENTREDIRECTINGCHANNEL
  NS_DECL_NSIPROGRESSEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR

  HttpChannelParent(mozilla::dom::PBrowserParent* iframeEmbedding,
                    const IPC::SerializedLoadContext& loadContext);
  virtual ~HttpChannelParent();

protected:
  virtual bool RecvAsyncOpen(const URIParams&           uri,
                             const OptionalURIParams&   originalUri,
                             const OptionalURIParams&   docUri,
                             const OptionalURIParams&   referrerUri,
                             const uint32_t&            loadFlags,
                             const RequestHeaderTuples& requestHeaders,
                             const nsHttpAtom&          requestMethod,
                             const OptionalInputStreamParams& uploadStream,
                             const bool&              uploadStreamHasHeaders,
                             const uint16_t&            priority,
                             const uint8_t&             redirectionLimit,
                             const bool&              allowPipelining,
                             const bool&              forceAllowThirdPartyCookie,
                             const bool&                doResumeAt,
                             const uint64_t&            startPos,
                             const nsCString&           entityID,
                             const bool&                chooseApplicationCache,
                             const nsCString&           appCacheClientID,
                             const bool&                allowSpdy) MOZ_OVERRIDE;

  virtual bool RecvConnectChannel(const uint32_t& channelId);
  virtual bool RecvSetPriority(const uint16_t& priority);
  virtual bool RecvSetCacheTokenCachedCharset(const nsCString& charset);
  virtual bool RecvSuspend();
  virtual bool RecvResume();
  virtual bool RecvCancel(const nsresult& status);
  virtual bool RecvRedirect2Verify(const nsresult& result,
                                   const RequestHeaderTuples& changedHeaders);
  virtual bool RecvUpdateAssociatedContentSecurity(const int32_t& broken,
                                                   const int32_t& no);
  virtual bool RecvDocumentChannelCleanup();
  virtual bool RecvMarkOfflineCacheEntryAsForeign();

  virtual void ActorDestroy(ActorDestroyReason why);

protected:
  friend class mozilla::net::HttpChannelParentListener;
  nsRefPtr<mozilla::dom::TabParent> mTabParent;

private:
  nsCOMPtr<nsIChannel>                    mChannel;
  nsCOMPtr<nsICacheEntryDescriptor>       mCacheDescriptor;
  nsCOMPtr<nsIAssociatedContentSecurity>  mAssociatedContentSecurity;
  bool mIPCClosed;                // PHttpChannel actor has been Closed()

  nsCOMPtr<nsIChannel> mRedirectChannel;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;

  nsAutoPtr<class nsHttpChannel::OfflineCacheEntryAsForeignMarker> mOfflineForeignMarker;

  // state for combining OnStatus/OnProgress with OnDataAvailable
  // into one IPDL call to child.
  nsresult mStoredStatus;
  uint64_t mStoredProgress;
  uint64_t mStoredProgressMax;

  bool mSentRedirect1Begin          : 1;
  bool mSentRedirect1BeginFailed    : 1;
  bool mReceivedRedirect2Verify     : 1;

  // Used to override channel Private Browsing status if needed.
  enum PBOverrideStatus {
    kPBOverride_Unset = 0,
    kPBOverride_Private,
    kPBOverride_NotPrivate
  };
  PBOverrideStatus mPBOverride;

  nsCOMPtr<nsILoadContext> mLoadContext;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_HttpChannelParent_h
