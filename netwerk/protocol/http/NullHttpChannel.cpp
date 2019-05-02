/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NullHttpChannel.h"
#include "nsContentUtils.h"
#include "nsContentSecurityManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamListener.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(NullHttpChannel, nsINullChannel, nsIHttpChannel,
                  nsITimedChannel)

NullHttpChannel::NullHttpChannel()
    : mAllRedirectsSameOrigin(false), mAllRedirectsPassTimingAllowCheck(false) {
  mChannelCreationTime = PR_Now();
  mChannelCreationTimestamp = TimeStamp::Now();
  mAsyncOpenTime = TimeStamp::Now();
}

NullHttpChannel::NullHttpChannel(nsIHttpChannel* chan)
    : mAllRedirectsSameOrigin(false), mAllRedirectsPassTimingAllowCheck(false) {
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  ssm->GetChannelURIPrincipal(chan, getter_AddRefs(mResourcePrincipal));

  Unused << chan->GetResponseHeader(NS_LITERAL_CSTRING("Timing-Allow-Origin"),
                                    mTimingAllowOriginHeader);
  chan->GetURI(getter_AddRefs(mURI));
  chan->GetOriginalURI(getter_AddRefs(mOriginalURI));

  mChannelCreationTime = PR_Now();
  mChannelCreationTimestamp = TimeStamp::Now();
  mAsyncOpenTime = TimeStamp::Now();

  nsCOMPtr<nsITimedChannel> timedChanel(do_QueryInterface(chan));
  if (timedChanel) {
    timedChanel->GetInitiatorType(mInitiatorType);
  }
}

nsresult NullHttpChannel::Init(nsIURI* aURI, uint32_t aCaps,
                               nsProxyInfo* aProxyInfo,
                               uint32_t aProxyResolveFlags, nsIURI* aProxyURI) {
  mURI = aURI;
  mOriginalURI = aURI;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// NullHttpChannel::nsIHttpChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
NullHttpChannel::GetChannelId(uint64_t* aChannelId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetChannelId(uint64_t aChannelId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetTopLevelContentWindowId(uint64_t* aWindowId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetTopLevelContentWindowId(uint64_t aWindowId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetTopLevelOuterContentWindowId(uint64_t* aWindowId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetTopLevelOuterContentWindowId(uint64_t aWindowId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::IsTrackingResource(bool* aIsTrackingResource) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::IsThirdPartyTrackingResource(bool* aIsTrackingResource) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetClassificationFlags(uint32_t* aClassificationFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetFlashPluginState(
    nsIHttpChannel::FlashPluginState* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetFirstPartyClassificationFlags(
    uint32_t* aClassificationFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetThirdPartyClassificationFlags(
    uint32_t* aClassificationFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetTransferSize(uint64_t* aTransferSize) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetDecodedBodySize(uint64_t* aDecodedBodySize) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetRequestMethod(nsACString& aRequestMethod) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetRequestMethod(const nsACString& aRequestMethod) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetReferrerInfo(nsIReferrerInfo** aReferrerInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetReferrerInfo(nsIReferrerInfo* aReferrerInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetReferrerInfoWithoutClone(nsIReferrerInfo* aReferrerInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetRequestHeader(const nsACString& aHeader,
                                  nsACString& _retval) {
  _retval.Truncate();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetRequestHeader(const nsACString& aHeader,
                                  const nsACString& aValue, bool aMerge) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetEmptyRequestHeader(const nsACString& aHeader) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::VisitRequestHeaders(nsIHttpHeaderVisitor* aVisitor) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::VisitNonDefaultRequestHeaders(nsIHttpHeaderVisitor* aVisitor) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetAllowPipelining(bool* aAllowPipelining) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetAllowPipelining(bool aAllowPipelining) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetAllowSTS(bool* aAllowSTS) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetAllowSTS(bool aAllowSTS) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetRedirectionLimit(uint32_t* aRedirectionLimit) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetRedirectionLimit(uint32_t aRedirectionLimit) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetResponseStatus(uint32_t* aResponseStatus) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetResponseStatusText(nsACString& aResponseStatusText) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetRequestSucceeded(bool* aRequestSucceeded) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetResponseHeader(const nsACString& header,
                                   nsACString& _retval) {
  _retval.Truncate();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetResponseHeader(const nsACString& header,
                                   const nsACString& value, bool merge) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::VisitResponseHeaders(nsIHttpHeaderVisitor* aVisitor) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetOriginalResponseHeader(const nsACString& header,
                                           nsIHttpHeaderVisitor* aVisitor) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::VisitOriginalResponseHeaders(nsIHttpHeaderVisitor* aVisitor) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::IsNoStoreResponse(bool* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::IsNoCacheResponse(bool* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::IsPrivateResponse(bool* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::RedirectTo(nsIURI* aNewURI) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SwitchProcessTo(mozilla::dom::Promise* aBrowserParent,
                                 uint64_t aIdentifier) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
NullHttpChannel::HasCrossOriginOpenerPolicyMismatch(bool* aMismatch) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
NullHttpChannel::UpgradeToSecure() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
NullHttpChannel::GetRequestContextID(uint64_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetRequestContextID(uint64_t rcID) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetProtocolVersion(nsACString& aProtocolVersion) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetEncodedBodySize(uint64_t* aEncodedBodySize) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// NullHttpChannel::nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
NullHttpChannel::GetOriginalURI(nsIURI** aOriginalURI) {
  NS_IF_ADDREF(*aOriginalURI = mOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetOriginalURI(nsIURI* aOriginalURI) {
  mOriginalURI = aOriginalURI;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetURI(nsIURI** aURI) {
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetOwner(nsISupports** aOwner) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetOwner(nsISupports* aOwner) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetNotificationCallbacks(
    nsIInterfaceRequestor** aNotificationCallbacks) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetNotificationCallbacks(
    nsIInterfaceRequestor* aNotificationCallbacks) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetSecurityInfo(nsISupports** aSecurityInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetContentType(nsACString& aContentType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetContentType(const nsACString& aContentType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetContentCharset(nsACString& aContentCharset) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetContentCharset(const nsACString& aContentCharset) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetContentLength(int64_t* aContentLength) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetContentLength(int64_t aContentLength) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::Open(nsIInputStream** aStream) {
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::AsyncOpen(nsIStreamListener* aListener) {
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetContentDisposition(uint32_t* aContentDisposition) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetContentDisposition(uint32_t aContentDisposition) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// NullHttpChannel::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
NullHttpChannel::GetName(nsACString& aName) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
NullHttpChannel::IsPending(bool* _retval) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
NullHttpChannel::GetStatus(nsresult* aStatus) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::Cancel(nsresult aStatus) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
NullHttpChannel::Suspend() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
NullHttpChannel::Resume() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
NullHttpChannel::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetLoadFlags(nsLoadFlags aLoadFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetIsDocument(bool* aIsDocument) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// NullHttpChannel::nsITimedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
NullHttpChannel::GetTimingEnabled(bool* aTimingEnabled) {
  // We don't want to report timing for null channels.
  *aTimingEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetTimingEnabled(bool aTimingEnabled) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetRedirectCount(uint8_t* aRedirectCount) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetRedirectCount(uint8_t aRedirectCount) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetInternalRedirectCount(uint8_t* aRedirectCount) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetInternalRedirectCount(uint8_t aRedirectCount) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetChannelCreation(mozilla::TimeStamp* aChannelCreation) {
  *aChannelCreation = mChannelCreationTimestamp;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetChannelCreation(TimeStamp aValue) {
  MOZ_DIAGNOSTIC_ASSERT(!aValue.IsNull());
  TimeDuration adjust = aValue - mChannelCreationTimestamp;
  mChannelCreationTimestamp = aValue;
  mChannelCreationTime += (PRTime)adjust.ToMicroseconds();
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetAsyncOpen(mozilla::TimeStamp* aAsyncOpen) {
  *aAsyncOpen = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetAsyncOpen(TimeStamp aValue) {
  MOZ_DIAGNOSTIC_ASSERT(!aValue.IsNull());
  mAsyncOpenTime = aValue;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetLaunchServiceWorkerStart(mozilla::TimeStamp* _retval) {
  MOZ_ASSERT(_retval);
  *_retval = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetLaunchServiceWorkerStart(mozilla::TimeStamp aTimeStamp) {
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetLaunchServiceWorkerEnd(mozilla::TimeStamp* _retval) {
  MOZ_ASSERT(_retval);
  *_retval = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetLaunchServiceWorkerEnd(mozilla::TimeStamp aTimeStamp) {
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetDispatchFetchEventStart(mozilla::TimeStamp* _retval) {
  MOZ_ASSERT(_retval);
  *_retval = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetDispatchFetchEventStart(mozilla::TimeStamp aTimeStamp) {
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetDispatchFetchEventEnd(mozilla::TimeStamp* _retval) {
  MOZ_ASSERT(_retval);
  *_retval = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetDispatchFetchEventEnd(mozilla::TimeStamp aTimeStamp) {
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetHandleFetchEventStart(mozilla::TimeStamp* _retval) {
  MOZ_ASSERT(_retval);
  *_retval = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetHandleFetchEventStart(mozilla::TimeStamp aTimeStamp) {
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetHandleFetchEventEnd(mozilla::TimeStamp* _retval) {
  MOZ_ASSERT(_retval);
  *_retval = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetHandleFetchEventEnd(mozilla::TimeStamp aTimeStamp) {
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetDomainLookupStart(mozilla::TimeStamp* aDomainLookupStart) {
  *aDomainLookupStart = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetDomainLookupEnd(mozilla::TimeStamp* aDomainLookupEnd) {
  *aDomainLookupEnd = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetConnectStart(mozilla::TimeStamp* aConnectStart) {
  *aConnectStart = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetTcpConnectEnd(mozilla::TimeStamp* aTcpConnectEnd) {
  *aTcpConnectEnd = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetSecureConnectionStart(
    mozilla::TimeStamp* aSecureConnectionStart) {
  *aSecureConnectionStart = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetConnectEnd(mozilla::TimeStamp* aConnectEnd) {
  *aConnectEnd = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetRequestStart(mozilla::TimeStamp* aRequestStart) {
  *aRequestStart = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetResponseStart(mozilla::TimeStamp* aResponseStart) {
  *aResponseStart = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetResponseEnd(mozilla::TimeStamp* aResponseEnd) {
  *aResponseEnd = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetRedirectStart(mozilla::TimeStamp* aRedirectStart) {
  *aRedirectStart = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetRedirectStart(mozilla::TimeStamp aRedirectStart) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetRedirectEnd(mozilla::TimeStamp* aRedirectEnd) {
  *aRedirectEnd = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetRedirectEnd(mozilla::TimeStamp aRedirectEnd) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetInitiatorType(nsAString& aInitiatorType) {
  aInitiatorType = mInitiatorType;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetInitiatorType(const nsAString& aInitiatorType) {
  mInitiatorType = aInitiatorType;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetAllRedirectsSameOrigin(bool* aAllRedirectsSameOrigin) {
  *aAllRedirectsSameOrigin = mAllRedirectsSameOrigin;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetAllRedirectsSameOrigin(bool aAllRedirectsSameOrigin) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetAllRedirectsPassTimingAllowCheck(
    bool* aAllRedirectsPassTimingAllowCheck) {
  *aAllRedirectsPassTimingAllowCheck = mAllRedirectsPassTimingAllowCheck;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetAllRedirectsPassTimingAllowCheck(
    bool aAllRedirectsPassTimingAllowCheck) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::TimingAllowCheck(nsIPrincipal* aOrigin, bool* _retval) {
  if (!mResourcePrincipal || !aOrigin) {
    *_retval = false;
    return NS_OK;
  }

  bool sameOrigin = false;
  nsresult rv = mResourcePrincipal->Equals(aOrigin, &sameOrigin);
  if (NS_SUCCEEDED(rv) && sameOrigin) {
    *_retval = true;
    return NS_OK;
  }

  if (mTimingAllowOriginHeader == "*") {
    *_retval = true;
    return NS_OK;
  }

  nsAutoCString origin;
  nsContentUtils::GetASCIIOrigin(aOrigin, origin);

  if (mTimingAllowOriginHeader == origin) {
    *_retval = true;
    return NS_OK;
  }

  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetCacheReadStart(mozilla::TimeStamp* aCacheReadStart) {
  *aCacheReadStart = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetCacheReadEnd(mozilla::TimeStamp* aCacheReadEnd) {
  *aCacheReadEnd = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetIsMainDocumentChannel(bool* aValue) {
  *aValue = false;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetIsMainDocumentChannel(bool aValue) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::LogBlockedCORSRequest(const nsAString& aMessage,
                                       const nsACString& aCategory) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::LogMimeTypeMismatch(const nsACString& aMessageName,
                                     bool aWarning, const nsAString& aURL,
                                     const nsAString& aContentType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetReportResourceTiming(bool enabled) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetReportResourceTiming(bool* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetServerTiming(nsIArray** aServerTiming) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetNativeServerTiming(
    nsTArray<nsCOMPtr<nsIServerTiming>>& aServerTiming) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

#define IMPL_TIMING_ATTR(name)                                           \
  NS_IMETHODIMP                                                          \
  NullHttpChannel::Get##name##Time(PRTime* _retval) {                    \
    TimeStamp stamp;                                                     \
    Get##name(&stamp);                                                   \
    if (stamp.IsNull()) {                                                \
      *_retval = 0;                                                      \
      return NS_OK;                                                      \
    }                                                                    \
    *_retval =                                                           \
        mChannelCreationTime +                                           \
        (PRTime)((stamp - mChannelCreationTimestamp).ToSeconds() * 1e6); \
    return NS_OK;                                                        \
  }

IMPL_TIMING_ATTR(ChannelCreation)
IMPL_TIMING_ATTR(AsyncOpen)
IMPL_TIMING_ATTR(LaunchServiceWorkerStart)
IMPL_TIMING_ATTR(LaunchServiceWorkerEnd)
IMPL_TIMING_ATTR(DispatchFetchEventStart)
IMPL_TIMING_ATTR(DispatchFetchEventEnd)
IMPL_TIMING_ATTR(HandleFetchEventStart)
IMPL_TIMING_ATTR(HandleFetchEventEnd)
IMPL_TIMING_ATTR(DomainLookupStart)
IMPL_TIMING_ATTR(DomainLookupEnd)
IMPL_TIMING_ATTR(ConnectStart)
IMPL_TIMING_ATTR(TcpConnectEnd)
IMPL_TIMING_ATTR(SecureConnectionStart)
IMPL_TIMING_ATTR(ConnectEnd)
IMPL_TIMING_ATTR(RequestStart)
IMPL_TIMING_ATTR(ResponseStart)
IMPL_TIMING_ATTR(ResponseEnd)
IMPL_TIMING_ATTR(CacheReadStart)
IMPL_TIMING_ATTR(CacheReadEnd)
IMPL_TIMING_ATTR(RedirectStart)
IMPL_TIMING_ATTR(RedirectEnd)

#undef IMPL_TIMING_ATTR

}  // namespace net
}  // namespace mozilla
