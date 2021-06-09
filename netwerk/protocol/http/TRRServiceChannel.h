/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_TRRServiceChannel_h
#define mozilla_net_TRRServiceChannel_h

#include "HttpBaseChannel.h"
#include "mozilla/DataMutex.h"
#include "nsIDNSListener.h"
#include "nsIProtocolProxyCallback.h"
#include "nsIProxiedChannel.h"
#include "nsIStreamListener.h"
#include "nsWeakReference.h"

class nsDNSPrefetch;

namespace mozilla {
namespace net {

class HttpTransactionShell;
class nsHttpHandler;

// Use to support QI nsIChannel to TRRServiceChannel
#define NS_TRRSERVICECHANNEL_IID                     \
  {                                                  \
    0x361c4bb1, 0xd6b2, 0x493b, {                    \
      0x86, 0xbc, 0x88, 0xd3, 0x5d, 0x16, 0x38, 0xfa \
    }                                                \
  }

// TRRServiceChannel is designed to fetch DNS data from DoH server. This channel
// MUST only be used by TRR.
class TRRServiceChannel : public HttpBaseChannel,
                          public HttpAsyncAborter<TRRServiceChannel>,
                          public nsIDNSListener,
                          public nsIStreamListener,
                          public nsITransportEventSink,
                          public nsIProxiedChannel,
                          public nsIProtocolProxyCallback,
                          public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDNSLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITRANSPORTEVENTSINK
  NS_DECL_NSIPROXIEDCHANNEL
  NS_DECL_NSIPROTOCOLPROXYCALLBACK
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_TRRSERVICECHANNEL_IID)

  // nsIRequest
  NS_IMETHOD Cancel(nsresult status) override;
  NS_IMETHOD Suspend() override;
  NS_IMETHOD Resume() override;
  NS_IMETHOD GetLoadFlags(nsLoadFlags* aLoadFlags) override;
  NS_IMETHOD SetLoadFlags(nsLoadFlags aLoadFlags) override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetNotificationCallbacks(
      nsIInterfaceRequestor** aCallbacks) override;
  NS_IMETHOD GetLoadGroup(nsILoadGroup** aLoadGroup) override;
  NS_IMETHOD GetRequestMethod(nsACString& aMethod) override;
  // nsIChannel
  NS_IMETHOD GetSecurityInfo(nsISupports** aSecurityInfo) override;
  NS_IMETHOD AsyncOpen(nsIStreamListener* aListener) override;

  NS_IMETHOD LogBlockedCORSRequest(const nsAString& aMessage,
                                   const nsACString& aCategory) override;
  NS_IMETHOD LogMimeTypeMismatch(const nsACString& aMessageName, bool aWarning,
                                 const nsAString& aURL,
                                 const nsAString& aContentType) override;
  NS_IMETHOD GetIsAuthChannel(bool* aIsAuthChannel) override;

  NS_IMETHOD SetNotificationCallbacks(
      nsIInterfaceRequestor* aCallbacks) override;
  // nsISupportsPriority
  NS_IMETHOD SetPriority(int32_t value) override;
  // nsIClassOfService
  NS_IMETHOD SetClassFlags(uint32_t inFlags) override;
  NS_IMETHOD AddClassFlags(uint32_t inFlags) override;
  NS_IMETHOD ClearClassFlags(uint32_t inFlags) override;
  // nsIResumableChannel
  NS_IMETHOD ResumeAt(uint64_t startPos, const nsACString& entityID) override;

  [[nodiscard]] nsresult OnPush(uint32_t aPushedStreamId,
                                const nsACString& aUrl,
                                const nsACString& aRequestString,
                                HttpTransactionShell* aTransaction);
  void SetPushedStreamTransactionAndId(
      HttpTransactionShell* aTransWithPushedStream, uint32_t aPushedStreamId);

  // nsITimedChannel
  NS_IMETHOD GetDomainLookupStart(
      mozilla::TimeStamp* aDomainLookupStart) override;
  NS_IMETHOD GetDomainLookupEnd(mozilla::TimeStamp* aDomainLookupEnd) override;
  NS_IMETHOD GetConnectStart(mozilla::TimeStamp* aConnectStart) override;
  NS_IMETHOD GetTcpConnectEnd(mozilla::TimeStamp* aTcpConnectEnd) override;
  NS_IMETHOD GetSecureConnectionStart(
      mozilla::TimeStamp* aSecureConnectionStart) override;
  NS_IMETHOD GetConnectEnd(mozilla::TimeStamp* aConnectEnd) override;
  NS_IMETHOD GetRequestStart(mozilla::TimeStamp* aRequestStart) override;
  NS_IMETHOD GetResponseStart(mozilla::TimeStamp* aResponseStart) override;
  NS_IMETHOD GetResponseEnd(mozilla::TimeStamp* aResponseEnd) override;
  NS_IMETHOD SetLoadGroup(nsILoadGroup* aLoadGroup) override;
  NS_IMETHOD TimingAllowCheck(nsIPrincipal* aOrigin, bool* aResult) override;

 protected:
  TRRServiceChannel();
  virtual ~TRRServiceChannel();

  void CancelNetworkRequest(nsresult aStatus);
  nsresult BeginConnect();
  nsresult ContinueOnBeforeConnect();
  nsresult Connect();
  nsresult SetupTransaction();
  void OnClassOfServiceUpdated();
  virtual void DoNotifyListenerCleanup() override;
  virtual void DoAsyncAbort(nsresult aStatus) override;
  bool IsIsolated() { return false; };
  void ProcessAltService();
  nsresult CallOnStartRequest();

  void MaybeStartDNSPrefetch();
  void DoNotifyListener();
  nsresult MaybeResolveProxyAndBeginConnect();
  nsresult ResolveProxy();
  void AfterApplyContentConversions(nsresult aResult,
                                    nsIStreamListener* aListener);
  nsresult SyncProcessRedirection(uint32_t aHttpStatus);
  [[nodiscard]] virtual nsresult SetupReplacementChannel(
      nsIURI* aNewURI, nsIChannel* aNewChannel, bool aPreserveMethod,
      uint32_t aRedirectFlags) override;

  virtual bool SameOriginWithOriginalUri(nsIURI* aURI) override;
  bool DispatchRelease();

  nsCString mUsername;

  // Needed for accurate DNS timing
  RefPtr<nsDNSPrefetch> mDNSPrefetch;

  nsCOMPtr<nsIRequest> mTransactionPump;
  RefPtr<HttpTransactionShell> mTransaction;
  uint32_t mPushedStreamId;
  RefPtr<HttpTransactionShell> mTransWithPushedStream;
  DataMutex<nsCOMPtr<nsICancelable>> mProxyRequest;
  nsCOMPtr<nsIEventTarget> mCurrentEventTarget;

  friend class HttpAsyncAborter<TRRServiceChannel>;
  friend class nsHttpHandler;
};

NS_DEFINE_STATIC_IID_ACCESSOR(TRRServiceChannel, NS_TRRSERVICECHANNEL_IID)

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_TRRServiceChannel_h
