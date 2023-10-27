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

namespace mozilla::net {

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
  TimeStamp mInterceptedChannelCreationTimestamp;

  // For the profiler markers
  TimeStamp mLastStatusReported;

  Atomic<int64_t> mProgress;
  int64_t mProgressReported;
  int64_t mSynthesizedStreamLength;
  uint64_t mResumeStartPos;
  nsCString mResumeEntityId;
  nsString mStatusHost;
  Atomic<bool> mCallingStatusAndProgress;
  bool mInterceptionReset{false};

  /**
   *  InterceptionTimeStamps is used to record the time stamps of the
   *  interception.
   *  The general usage:
   *  Step 1. Initialize the InterceptionTimeStamps;
   *    InterceptionTimeStamps::Init(channel);
   *  Step 2. Record time for each stage
   *    InterceptionTimeStamps::RecordTime(); or
   *    InterceptionTimeStamps::RecordTime(timeStamp);
   *  Step 3. Record time for the last stage with the final status
   *    InterceptionTimeStamps::RecordTime(InterceptionTimeStamps::Synthesized);
   */
  class InterceptionTimeStamps final {
   public:
    // The possible status of the interception.
    enum Status {
      Created,
      Initialized,
      Synthesized,
      Reset,
      Redirected,
      Canceled,
      CanceledAfterSynthesized,
      CanceledAfterReset,
      CanceledAfterRedirected
    };

    InterceptionTimeStamps();
    ~InterceptionTimeStamps() = default;

    /**
     * Initialize with the given channel.
     * This method should be called before any RecordTime().
     */
    void Init(nsIChannel* aChannel);

    /**
     * Record the given time stamp for current stage. If there is no given time
     * stamp, TimeStamp::Now() will be recorded.
     * The current stage is auto moved to the next one.
     */
    void RecordTime(TimeStamp&& aTimeStamp = TimeStamp::Now());

    /**
     * Record the given time stamp for the last stage(InterceptionFinish) and
     * set the final status to the given status.
     * If these is no given time stamp, TimeStamp::Now() will be recorded.
     * Notice that this method is for the last stage, it calls SaveTimeStamps()
     * to write data into telemetries.
     */
    void RecordTime(Status&& aStatus,
                    TimeStamp&& aTimeStamp = TimeStamp::Now());

   private:
    // The time stamp which the intercepted channel is created and async opend.
    TimeStamp mInterceptionStart;

    // The time stamp which the interception finishes.
    TimeStamp mInterceptionFinish;

    // The time stamp which the fetch event starts to be handled by fetch event
    // handler.
    TimeStamp mFetchHandlerStart;

    // The time stamp which the fetch event handling finishes. It would the time
    // which remote worker sends result back.
    TimeStamp mFetchHandlerFinish;

    // The stage of interception.
    enum Stage {
      InterceptionStart,
      FetchHandlerStart,
      FetchHandlerFinish,
      InterceptionFinish
    } mStage;

    // The final status of the interception.
    Status mStatus;

    bool mIsNonSubresourceRequest;
    // The keys used for telemetries.
    nsCString mKey;
    nsCString mSubresourceKey;

    void RecordTimeInternal(TimeStamp&& aTimeStamp);

    // Generate the record keys with final status.
    void GenKeysWithStatus(nsCString& aKey, nsCString& aSubresourceKey);

    // Save the time stamps into telemetries.
    void SaveTimeStamps();
  };

  InterceptionTimeStamps mTimeStamps;

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

  NS_IMETHOD SetCanceledReason(const nsACString& aReason) override;
  NS_IMETHOD GetCanceledReason(nsACString& aReason) override;
  NS_IMETHOD CancelWithReason(nsresult status,
                              const nsACString& reason) override;

  NS_IMETHOD
  Cancel(nsresult aStatus) override;

  NS_IMETHOD
  Suspend(void) override;

  NS_IMETHOD
  Resume(void) override;

  NS_IMETHOD
  GetSecurityInfo(nsITransportSecurityInfo** aSecurityInfo) override;

  NS_IMETHOD
  AsyncOpen(nsIStreamListener* aListener) override;

  NS_IMETHOD
  LogBlockedCORSRequest(const nsAString& aMessage, const nsACString& aCategory,
                        bool aIsWarning) override;

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
  SetClassOfService(ClassOfService cos) override;

  NS_IMETHOD
  SetIncremental(bool incremental) override;

  NS_IMETHOD
  ResumeAt(uint64_t startPos, const nsACString& entityID) override;

  NS_IMETHOD
  SetEarlyHintObserver(nsIEarlyHintObserver* aObserver) override {
    return NS_OK;
  }

  NS_IMETHOD SetWebTransportSessionEventListener(
      WebTransportSessionEventListener* aListener) override {
    return NS_OK;
  }

  void DoNotifyListenerCleanup() override;

  void DoAsyncAbort(nsresult aStatus) override;
};

}  // namespace mozilla::net

#endif  // mozilla_net_InterceptedHttpChannel_h
