/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_HttpChannelChild_h
#define mozilla_net_HttpChannelChild_h

#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/net/PHttpChannelChild.h"
#include "mozilla/net/ChannelEventQueue.h"

#include "nsIStreamListener.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsICacheInfoChannel.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIUploadChannel2.h"
#include "nsIResumableChannel.h"
#include "nsIProxiedChannel.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIAssociatedContentSecurity.h"
#include "nsIChildChannel.h"
#include "nsIHttpChannelChild.h"
#include "nsIDivertableChannel.h"
#include "mozilla/net/DNS.h"

class nsInputStreamPump;

namespace mozilla {
namespace net {

class InterceptedChannelContent;
class InterceptStreamListener;

class HttpChannelChild final : public PHttpChannelChild
                             , public HttpBaseChannel
                             , public HttpAsyncAborter<HttpChannelChild>
                             , public nsICacheInfoChannel
                             , public nsIProxiedChannel
                             , public nsIApplicationCacheChannel
                             , public nsIAsyncVerifyRedirectCallback
                             , public nsIAssociatedContentSecurity
                             , public nsIChildChannel
                             , public nsIHttpChannelChild
                             , public nsIDivertableChannel
{
  virtual ~HttpChannelChild();
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICACHEINFOCHANNEL
  NS_DECL_NSIPROXIEDCHANNEL
  NS_DECL_NSIAPPLICATIONCACHECONTAINER
  NS_DECL_NSIAPPLICATIONCACHECHANNEL
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK
  NS_DECL_NSIASSOCIATEDCONTENTSECURITY
  NS_DECL_NSICHILDCHANNEL
  NS_DECL_NSIHTTPCHANNELCHILD
  NS_DECL_NSIDIVERTABLECHANNEL

  HttpChannelChild();

  // Methods HttpBaseChannel didn't implement for us or that we override.
  //
  // nsIRequest
  NS_IMETHOD Cancel(nsresult status) override;
  NS_IMETHOD Suspend() override;
  NS_IMETHOD Resume() override;
  // nsIChannel
  NS_IMETHOD GetSecurityInfo(nsISupports **aSecurityInfo) override;
  NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *aContext) override;
  // HttpBaseChannel::nsIHttpChannel
  NS_IMETHOD SetRequestHeader(const nsACString& aHeader,
                              const nsACString& aValue,
                              bool aMerge) override;
  NS_IMETHOD RedirectTo(nsIURI *newURI) override;
  // nsIHttpChannelInternal
  NS_IMETHOD SetupFallbackChannel(const char *aFallbackKey) override;
  NS_IMETHOD GetLocalAddress(nsACString& addr) override;
  NS_IMETHOD GetLocalPort(int32_t* port) override;
  NS_IMETHOD GetRemoteAddress(nsACString& addr) override;
  NS_IMETHOD GetRemotePort(int32_t* port) override;
  // nsISupportsPriority
  NS_IMETHOD SetPriority(int32_t value) override;
  // nsIClassOfService
  NS_IMETHOD SetClassFlags(uint32_t inFlags) override;
  NS_IMETHOD AddClassFlags(uint32_t inFlags) override;
  NS_IMETHOD ClearClassFlags(uint32_t inFlags) override;
  // nsIResumableChannel
  NS_IMETHOD ResumeAt(uint64_t startPos, const nsACString& entityID) override;

  // IPDL holds a reference while the PHttpChannel protocol is live (starting at
  // AsyncOpen, and ending at either OnStopRequest or any IPDL error, either of
  // which call NeckoChild::DeallocPHttpChannelChild()).
  void AddIPDLReference();
  void ReleaseIPDLReference();

  bool IsSuspended();

  bool RecvNotifyTrackingProtectionDisabled() override;
  void FlushedForDiversion();

protected:
  bool RecvOnStartRequest(const nsresult& channelStatus,
                          const nsHttpResponseHead& responseHead,
                          const bool& useResponseHead,
                          const nsHttpHeaderArray& requestHeaders,
                          const bool& isFromCache,
                          const bool& cacheEntryAvailable,
                          const uint32_t& cacheExpirationTime,
                          const nsCString& cachedCharset,
                          const nsCString& securityInfoSerialization,
                          const NetAddr& selfAddr,
                          const NetAddr& peerAddr,
                          const int16_t& redirectCount) override;
  bool RecvOnTransportAndData(const nsresult& channelStatus,
                              const nsresult& status,
                              const uint64_t& progress,
                              const uint64_t& progressMax,
                              const nsCString& data,
                              const uint64_t& offset,
                              const uint32_t& count) override;
  bool RecvOnStopRequest(const nsresult& statusCode, const ResourceTimingStruct& timing) override;
  bool RecvOnProgress(const int64_t& progress, const int64_t& progressMax) override;
  bool RecvOnStatus(const nsresult& status) override;
  bool RecvFailedAsyncOpen(const nsresult& status) override;
  bool RecvRedirect1Begin(const uint32_t& newChannel,
                          const URIParams& newURI,
                          const uint32_t& redirectFlags,
                          const nsHttpResponseHead& responseHead) override;
  bool RecvRedirect3Complete() override;
  bool RecvAssociateApplicationCache(const nsCString& groupID,
                                     const nsCString& clientID) override;
  bool RecvFlushedForDiversion() override;
  bool RecvDivertMessages() override;
  bool RecvDeleteSelf() override;

  bool GetAssociatedContentSecurity(nsIAssociatedContentSecurity** res = nullptr);
  virtual void DoNotifyListenerCleanup() override;

private:
  nsresult ContinueAsyncOpen();

  void DoOnStartRequest(nsIRequest* aRequest, nsISupports* aContext);
  void DoOnStatus(nsIRequest* aRequest, nsresult status);
  void DoOnProgress(nsIRequest* aRequest, int64_t progress, int64_t progressMax);
  void DoOnDataAvailable(nsIRequest* aRequest, nsISupports* aContext, nsIInputStream* aStream,
                         uint64_t offset, uint32_t count);
  void DoPreOnStopRequest(nsresult aStatus);
  void DoOnStopRequest(nsIRequest* aRequest, nsISupports* aContext);

  // Discard the prior interception and continue with the original network request.
  void ResetInterception();

  // Override this channel's pending response with a synthesized one. The content will be
  // asynchronously read from the pump.
  void OverrideWithSynthesizedResponse(nsAutoPtr<nsHttpResponseHead>& aResponseHead,
                                       nsIInputStream* aSynthesizedInput,
                                       nsIStreamListener* aStreamListener);

  RequestHeaderTuples mClientSetRequestHeaders;
  nsCOMPtr<nsIChildChannel> mRedirectChannelChild;
  nsRefPtr<InterceptStreamListener> mInterceptListener;
  nsRefPtr<nsInputStreamPump> mSynthesizedResponsePump;
  int64_t mSynthesizedStreamLength;

  bool mIsFromCache;
  bool mCacheEntryAvailable;
  uint32_t     mCacheExpirationTime;
  nsCString    mCachedCharset;

  // If ResumeAt is called before AsyncOpen, we need to send extra data upstream
  bool mSendResumeAt;

  bool mIPCOpen;
  bool mKeptAlive;            // IPC kept open, but only for security info
  nsRefPtr<ChannelEventQueue> mEventQ;

  // Once set, OnData and possibly OnStop will be diverted to the parent.
  bool mDivertingToParent;
  // Once set, no OnStart/OnData/OnStop callbacks should be received from the
  // parent channel, nor dequeued from the ChannelEventQueue.
  bool mFlushedForDiversion;
  // Set if SendSuspend is called. Determines if SendResume is needed when
  // diverting callbacks to parent.
  bool mSuspendSent;

  // true after successful AsyncOpen until OnStopRequest completes.
  bool RemoteChannelExists() { return mIPCOpen && !mKeptAlive; }

  void AssociateApplicationCache(const nsCString &groupID,
                                 const nsCString &clientID);
  void OnStartRequest(const nsresult& channelStatus,
                      const nsHttpResponseHead& responseHead,
                      const bool& useResponseHead,
                      const nsHttpHeaderArray& requestHeaders,
                      const bool& isFromCache,
                      const bool& cacheEntryAvailable,
                      const uint32_t& cacheExpirationTime,
                      const nsCString& cachedCharset,
                      const nsCString& securityInfoSerialization,
                      const NetAddr& selfAddr,
                      const NetAddr& peerAddr);
  void OnTransportAndData(const nsresult& channelStatus,
                          const nsresult& status,
                          const uint64_t progress,
                          const uint64_t& progressMax,
                          const nsCString& data,
                          const uint64_t& offset,
                          const uint32_t& count);
  void OnStopRequest(const nsresult& channelStatus, const ResourceTimingStruct& timing);
  void OnProgress(const int64_t& progress, const int64_t& progressMax);
  void OnStatus(const nsresult& status);
  void FailedAsyncOpen(const nsresult& status);
  void HandleAsyncAbort();
  void Redirect1Begin(const uint32_t& newChannelId,
                      const URIParams& newUri,
                      const uint32_t& redirectFlags,
                      const nsHttpResponseHead& responseHead);
  void Redirect3Complete();
  void DeleteSelf();

  friend class AssociateApplicationCacheEvent;
  friend class StartRequestEvent;
  friend class StopRequestEvent;
  friend class TransportAndDataEvent;
  friend class ProgressEvent;
  friend class StatusEvent;
  friend class FailedAsyncOpenEvent;
  friend class Redirect1Event;
  friend class Redirect3Event;
  friend class DeleteSelfEvent;
  friend class HttpAsyncAborter<HttpChannelChild>;
  friend class InterceptStreamListener;
  friend class InterceptedChannelContent;
};

//-----------------------------------------------------------------------------
// inline functions
//-----------------------------------------------------------------------------

inline bool
HttpChannelChild::IsSuspended()
{
  return mSuspendCount != 0;
}

} // namespace net
} // namespace mozilla

#endif // mozilla_net_HttpChannelChild_h
