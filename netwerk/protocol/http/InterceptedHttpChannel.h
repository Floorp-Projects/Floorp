/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_InterceptedHttpChannel_h
#define mozilla_net_InterceptedHttpChannel_h

#include "HttpBaseChannel.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsINetworkInterceptController.h"
#include "nsIInputStream.h"
#include "nsICacheInfoChannel.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIThreadRetargetableStreamListener.h"

namespace mozilla {
namespace net {

// This class represents an http channel that is being intercepted by a
// ServiceWorker.  This means that when the channel is opened a FetchEvent
// will be fired on the ServiceWorker thread.  The channel will complete
// depending on what the worker does.  The options are:
//
// 1. If the ServiceWorker does not handle the FetchEvent or does not call
//    FetchEvent.respondWith(), then the channel needs to fall back to a
//    normal request.  When this happens ResetInterception() is called and
//    the channel will perform an internal redirect back to an nsHttpChannel.
//
// 2. If the ServiceWorker provides a Response to FetchEvent.respondWith()
//    then the status, headers, and body must be synthesized.  When
//    FinishSynthesizedResponse() is called the synthesized data must be
//    reported back to the channel listener.  This is handled in a few
//    different ways:
//      a. If a redirect was synthesized, then we perform the redirect to
//         a new nsHttpChannel.  This new channel might trigger yet another
//         interception.
//      b. If a same-origin or CORS Response was synthesized, then we simply
//         crate an nsInputStreamPump to process it and call back to the
//         listener.
//      c. If an opaque Response was synthesized, then we perform an internal
//         redirect to a new InterceptedHttpChannel using the cross-origin URL.
//         When this new channel is opened, it then creates a pump as in case
//         (b).  The extra redirect here is to make sure the various listeners
//         treat the result as unsafe cross-origin data.
//
// 3. If an error occurs, such as the ServiceWorker passing garbage to
//    FetchEvent.respondWith(), then CancelInterception() is called.  This is
//    handled the same as a normal nsIChannel::Cancel() call.  We abort the
//    channel and end up calling OnStopRequest() with an error code.
class InterceptedHttpChannel final
    : public HttpBaseChannel,
      public HttpAsyncAborter<InterceptedHttpChannel>,
      public nsIInterceptedChannel,
      public nsICacheInfoChannel,
      public nsIAsyncVerifyRedirectCallback,
      public nsIStreamListener,
      public nsIThreadRetargetableRequest,
      public nsIThreadRetargetableStreamListener {
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIINTERCEPTEDCHANNEL
  NS_DECL_NSICACHEINFOCHANNEL
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITHREADRETARGETABLEREQUEST
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

 private:
  friend class HttpAsyncAborter<InterceptedHttpChannel>;

  UniquePtr<nsHttpResponseHead> mSynthesizedResponseHead;
  nsCOMPtr<nsIChannel> mRedirectChannel;
  nsCOMPtr<nsIInputStream> mBodyReader;
  nsCOMPtr<nsISupports> mReleaseHandle;
  nsCOMPtr<nsIProgressEventSink> mProgressSink;
  nsCOMPtr<nsIInterceptedBodyCallback> mBodyCallback;
  nsCOMPtr<nsICacheInfoChannel> mSynthesizedCacheInfo;
  RefPtr<nsInputStreamPump> mPump;
  TimeStamp mFinishResponseStart;
  TimeStamp mFinishResponseEnd;
  TimeStamp mInterceptedChannelCreationTimestamp;

  // For the profiler markers
  TimeStamp mLastStatusReported;

  Atomic<int64_t> mProgress;
  int64_t mProgressReported;
  int64_t mSynthesizedStreamLength;
  uint64_t mResumeStartPos;
  nsCString mResumeEntityId;
  nsString mStatusHost;
  // enum { Invalid = 0, Synthesized, Reset } mSynthesizedOrReset;
  Atomic<bool> mCallingStatusAndProgress;

  InterceptedHttpChannel(PRTime aCreationTime,
                         const TimeStamp& aCreationTimestamp,
                         const TimeStamp& aAsyncOpenTimestamp);
  ~InterceptedHttpChannel() = default;

  virtual void ReleaseListeners() override;

  [[nodiscard]] virtual nsresult SetupReplacementChannel(
      nsIURI* aURI, nsIChannel* aChannel, bool aPreserveMethod,
      uint32_t aRedirectFlags) override;

  void AsyncOpenInternal();

  bool ShouldRedirect() const;

  nsresult FollowSyntheticRedirect();

  // If the response's URL is different from the request's then do a service
  // worker redirect. If Response.redirected is false we do an internal
  // redirect. Otherwise, if Response.redirect is true do a non-internal
  // redirect so end consumers detect the redirected state.
  nsresult RedirectForResponseURL(nsIURI* aResponseURI,
                                  bool aResponseRedirected);

  nsresult StartPump();

  nsresult OpenRedirectChannel();

  void MaybeCallStatusAndProgress();

  void MaybeCallBodyCallback();

 public:
  static already_AddRefed<InterceptedHttpChannel> CreateForInterception(
      PRTime aCreationTime, const TimeStamp& aCreationTimestamp,
      const TimeStamp& aAsyncOpenTimestamp);

  static already_AddRefed<InterceptedHttpChannel> CreateForSynthesis(
      const nsHttpResponseHead* aHead, nsIInputStream* aBody,
      nsIInterceptedBodyCallback* aBodyCallback, PRTime aCreationTime,
      const TimeStamp& aCreationTimestamp,
      const TimeStamp& aAsyncOpenTimestamp);

  NS_IMETHOD
  Cancel(nsresult aStatus) override;

  NS_IMETHOD
  Suspend(void) override;

  NS_IMETHOD
  Resume(void) override;

  NS_IMETHOD
  GetSecurityInfo(nsISupports** aSecurityInfo) override;

  NS_IMETHOD
  AsyncOpen(nsIStreamListener* aListener) override;

  NS_IMETHOD
  LogBlockedCORSRequest(const nsAString& aMessage,
                        const nsACString& aCategory) override;

  NS_IMETHOD
  LogMimeTypeMismatch(const nsACString& aMessageName, bool aWarning,
                      const nsAString& aURL,
                      const nsAString& aContentType) override;

  NS_IMETHOD
  GetIsAuthChannel(bool* aIsAuthChannel) override;

  NS_IMETHOD
  SetPriority(int32_t aPriority) override;

  NS_IMETHOD
  SetClassFlags(uint32_t aClassFlags) override;

  NS_IMETHOD
  ClearClassFlags(uint32_t flags) override;

  NS_IMETHOD
  AddClassFlags(uint32_t flags) override;

  NS_IMETHOD
  ResumeAt(uint64_t startPos, const nsACString& entityID) override;

  void DoNotifyListenerCleanup() override;

  void DoAsyncAbort(nsresult aStatus) override;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_InterceptedHttpChannel_h
