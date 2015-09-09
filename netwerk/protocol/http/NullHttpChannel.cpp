/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NullHttpChannel.h"
#include "nsContentUtils.h"
#include "nsContentSecurityManager.h"
#include "nsIScriptSecurityManager.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(NullHttpChannel, nsINullChannel,
                  nsIHttpChannel, nsITimedChannel)

NullHttpChannel::NullHttpChannel()
{
  mChannelCreationTime = PR_Now();
  mChannelCreationTimestamp = TimeStamp::Now();
  mAsyncOpenTime = TimeStamp::Now();
}

NullHttpChannel::NullHttpChannel(nsIHttpChannel * chan)
{
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  ssm->GetChannelURIPrincipal(chan, getter_AddRefs(mResourcePrincipal));

  chan->GetResponseHeader(NS_LITERAL_CSTRING("Timing-Allow-Origin"),
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

nsresult
NullHttpChannel::Init(nsIURI *aURI,
                      uint32_t aCaps,
                      nsProxyInfo *aProxyInfo,
                      uint32_t aProxyResolveFlags,
                      nsIURI *aProxyURI)
{
  mURI = aURI;
  mOriginalURI = aURI;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// NullHttpChannel::nsIHttpChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
NullHttpChannel::GetRequestMethod(nsACString & aRequestMethod)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetRequestMethod(const nsACString & aRequestMethod)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetReferrer(nsIURI * *aReferrer)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetReferrer(nsIURI *aReferrer)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetReferrerPolicy(uint32_t *aReferrerPolicy)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetReferrerWithPolicy(nsIURI *referrer, uint32_t referrerPolicy)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetRequestHeader(const nsACString & aHeader, nsACString & _retval)
{
  _retval.Truncate();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetRequestHeader(const nsACString & aHeader, const nsACString & aValue, bool aMerge)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::VisitRequestHeaders(nsIHttpHeaderVisitor *aVisitor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetAllowPipelining(bool *aAllowPipelining)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetAllowPipelining(bool aAllowPipelining)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetAllowSTS(bool *aAllowSTS)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetAllowSTS(bool aAllowSTS)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetRedirectionLimit(uint32_t *aRedirectionLimit)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetRedirectionLimit(uint32_t aRedirectionLimit)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetResponseStatus(uint32_t *aResponseStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetResponseStatusText(nsACString & aResponseStatusText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetRequestSucceeded(bool *aRequestSucceeded)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetResponseHeader(const nsACString & header, nsACString & _retval)
{
  _retval.Truncate();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetResponseHeader(const nsACString & header, const nsACString & value, bool merge)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::VisitResponseHeaders(nsIHttpHeaderVisitor *aVisitor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::IsNoStoreResponse(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::IsNoCacheResponse(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::IsPrivateResponse(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::RedirectTo(nsIURI *aNewURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetSchedulingContextID(nsID *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetSchedulingContextID(const nsID scID)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// NullHttpChannel::nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
NullHttpChannel::GetOriginalURI(nsIURI * *aOriginalURI)
{
  NS_IF_ADDREF(*aOriginalURI = mOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetOriginalURI(nsIURI *aOriginalURI)
{
  mOriginalURI = aOriginalURI;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetURI(nsIURI * *aURI)
{
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetOwner(nsISupports * *aOwner)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetOwner(nsISupports *aOwner)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetNotificationCallbacks(nsIInterfaceRequestor *aNotificationCallbacks)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetContentType(nsACString & aContentType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetContentType(const nsACString & aContentType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetContentCharset(nsACString & aContentCharset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetContentCharset(const nsACString & aContentCharset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetContentLength(int64_t *aContentLength)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetContentLength(int64_t aContentLength)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::Open(nsIInputStream * *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::Open2(nsIInputStream** aStream)
{
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return Open(aStream);
}

NS_IMETHODIMP
NullHttpChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *aContext)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::AsyncOpen2(nsIStreamListener *aListener)
{
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return AsyncOpen(listener, nullptr);
}

NS_IMETHODIMP
NullHttpChannel::GetContentDisposition(uint32_t *aContentDisposition)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetContentDisposition(uint32_t aContentDisposition)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetContentDispositionFilename(nsAString & aContentDispositionFilename)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetContentDispositionFilename(const nsAString & aContentDispositionFilename)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetContentDispositionHeader(nsACString & aContentDispositionHeader)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetLoadInfo(nsILoadInfo * *aLoadInfo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetLoadInfo(nsILoadInfo *aLoadInfo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// NullHttpChannel::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
NullHttpChannel::GetName(nsACString & aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::IsPending(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetStatus(nsresult *aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::Cancel(nsresult aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::Suspend()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::Resume()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// NullHttpChannel::nsITimedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
NullHttpChannel::GetTimingEnabled(bool *aTimingEnabled)
{
  *aTimingEnabled = true;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetTimingEnabled(bool aTimingEnabled)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetRedirectCount(uint16_t *aRedirectCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::SetRedirectCount(uint16_t aRedirectCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetChannelCreation(mozilla::TimeStamp *aChannelCreation)
{
  *aChannelCreation = mChannelCreationTimestamp;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetAsyncOpen(mozilla::TimeStamp *aAsyncOpen)
{
  *aAsyncOpen = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetDomainLookupStart(mozilla::TimeStamp *aDomainLookupStart)
{
  *aDomainLookupStart = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetDomainLookupEnd(mozilla::TimeStamp *aDomainLookupEnd)
{
  *aDomainLookupEnd = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetConnectStart(mozilla::TimeStamp *aConnectStart)
{
  *aConnectStart = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetConnectEnd(mozilla::TimeStamp *aConnectEnd)
{
  *aConnectEnd = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetRequestStart(mozilla::TimeStamp *aRequestStart)
{
  *aRequestStart = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetResponseStart(mozilla::TimeStamp *aResponseStart)
{
  *aResponseStart = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetResponseEnd(mozilla::TimeStamp *aResponseEnd)
{
  *aResponseEnd = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetRedirectStart(mozilla::TimeStamp *aRedirectStart)
{
  *aRedirectStart = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetRedirectStart(mozilla::TimeStamp aRedirectStart)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetRedirectEnd(mozilla::TimeStamp *aRedirectEnd)
{
  *aRedirectEnd = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetRedirectEnd(mozilla::TimeStamp aRedirectEnd)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetInitiatorType(nsAString & aInitiatorType)
{
  aInitiatorType = mInitiatorType;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetInitiatorType(const nsAString & aInitiatorType)
{
  mInitiatorType = aInitiatorType;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetAllRedirectsSameOrigin(bool *aAllRedirectsSameOrigin)
{
  *aAllRedirectsSameOrigin = mAllRedirectsSameOrigin;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetAllRedirectsSameOrigin(bool aAllRedirectsSameOrigin)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::GetAllRedirectsPassTimingAllowCheck(bool *aAllRedirectsPassTimingAllowCheck)
{
  *aAllRedirectsPassTimingAllowCheck = mAllRedirectsPassTimingAllowCheck;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::SetAllRedirectsPassTimingAllowCheck(bool aAllRedirectsPassTimingAllowCheck)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullHttpChannel::TimingAllowCheck(nsIPrincipal *aOrigin, bool *_retval)
{
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
NullHttpChannel::GetCacheReadStart(mozilla::TimeStamp *aCacheReadStart)
{
  *aCacheReadStart = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
NullHttpChannel::GetCacheReadEnd(mozilla::TimeStamp *aCacheReadEnd)
{
  *aCacheReadEnd = mAsyncOpenTime;
  return NS_OK;
}

#define IMPL_TIMING_ATTR(name)                                 \
NS_IMETHODIMP                                                  \
NullHttpChannel::Get##name##Time(PRTime* _retval) {            \
    TimeStamp stamp;                                           \
    Get##name(&stamp);                                         \
    if (stamp.IsNull()) {                                      \
        *_retval = 0;                                          \
        return NS_OK;                                          \
    }                                                          \
    *_retval = mChannelCreationTime +                          \
        (PRTime) ((stamp - mChannelCreationTimestamp).ToSeconds() * 1e6); \
    return NS_OK;                                              \
}

IMPL_TIMING_ATTR(ChannelCreation)
IMPL_TIMING_ATTR(AsyncOpen)
IMPL_TIMING_ATTR(DomainLookupStart)
IMPL_TIMING_ATTR(DomainLookupEnd)
IMPL_TIMING_ATTR(ConnectStart)
IMPL_TIMING_ATTR(ConnectEnd)
IMPL_TIMING_ATTR(RequestStart)
IMPL_TIMING_ATTR(ResponseStart)
IMPL_TIMING_ATTR(ResponseEnd)
IMPL_TIMING_ATTR(CacheReadStart)
IMPL_TIMING_ATTR(CacheReadEnd)
IMPL_TIMING_ATTR(RedirectStart)
IMPL_TIMING_ATTR(RedirectEnd)

#undef IMPL_TIMING_ATTR

} // namespace net
} // namespace mozilla
