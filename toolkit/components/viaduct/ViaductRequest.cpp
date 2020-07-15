/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/ViaductRequest.h"

#include "mozilla/ErrorNames.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"

#include "nsContentUtils.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIHttpChannel.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIInputStream.h"
#include "nsIUploadChannel2.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsStringStream.h"

namespace mozilla {

namespace {

extern "C" {
ViaductByteBuffer viaduct_alloc_bytebuffer(int32_t);
void viaduct_destroy_bytebuffer(ViaductByteBuffer);
}

}  // namespace

class HeaderVisitor final : public nsIHttpHeaderVisitor {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHTTPHEADERVISITOR

  explicit HeaderVisitor(
      google::protobuf::Map<std::string, std::string>* aHeaders)
      : mHeaders(aHeaders) {}

 private:
  google::protobuf::Map<std::string, std::string>* mHeaders;
  ~HeaderVisitor() = default;
};

NS_IMETHODIMP
HeaderVisitor::VisitHeader(const nsACString& aHeader,
                           const nsACString& aValue) {
  (*mHeaders)[aHeader.BeginReading()] = aValue.BeginReading();
  return NS_OK;
}
NS_IMPL_ISUPPORTS(HeaderVisitor, nsIHttpHeaderVisitor)

nsCString ConvertMethod(
    appservices::httpconfig::protobuf::Request_Method method);

///////////////////////////////////////////////////////////////////////////////
// ViaductRequest implementation

ViaductByteBuffer ViaductRequest::MakeRequest(ViaductByteBuffer reqBuf) {
  MOZ_ASSERT(!NS_IsMainThread(), "Background thread only!");
  auto clearBuf = MakeScopeExit([&] { viaduct_destroy_bytebuffer(reqBuf); });
  // We keep the protobuf parsing/serializing in the background thread.
  appservices::httpconfig::protobuf::Request request;
  if (!request.ParseFromArray(static_cast<const void*>(reqBuf.data),
                              reqBuf.len)) {
    // We still need to return something!
    return ViaductByteBuffer{.len = 0, .data = nullptr};
  }
  MonitorAutoLock lock(mMonitor);
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "ViaductRequest::LaunchRequest", [this, &request]() {
        nsresult rv = LaunchRequest(request);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          // Something went very very wrong, but we still have to unblock
          // the calling thread.
          NotifyMonitor();
        }
      }));
  while (!mDone) {
    mMonitor.Wait();
  }
  ViaductByteBuffer respBuf =
      viaduct_alloc_bytebuffer(mResponse.ByteSizeLong());
  if (!mResponse.SerializeToArray(respBuf.data, respBuf.len)) {
    viaduct_destroy_bytebuffer(respBuf);
    return ViaductByteBuffer{.len = 0, .data = nullptr};
  }
  return respBuf;
}

nsresult ViaductRequest::LaunchRequest(
    appservices::httpconfig::protobuf::Request& request) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), request.url().c_str());
  NS_ENSURE_SUCCESS(rv, rv);

  nsSecurityFlags secFlags =
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL |
      nsILoadInfo::SEC_COOKIES_OMIT;
  uint32_t loadFlags = 0;

  if (!request.use_caches()) {
    loadFlags |= nsIRequest::LOAD_BYPASS_CACHE;
  }

  if (!request.follow_redirects()) {
    secFlags |= nsILoadInfo::SEC_DONT_FOLLOW_REDIRECTS;
  }

  rv = NS_NewChannel(getter_AddRefs(mChannel), uri,
                     nsContentUtils::GetSystemPrincipal(), secFlags,
                     nsIContentPolicy::TYPE_OTHER,
                     nullptr,  // nsICookieJarSettings
                     nullptr,  // aPerformanceStorage
                     nullptr,  // aLoadGroup
                     nullptr, loadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  nsCString method = ConvertMethod(request.method());
  rv = httpChannel->SetRequestMethod(method);
  NS_ENSURE_SUCCESS(rv, rv);

  for (auto& header : request.headers()) {
    rv = httpChannel->SetRequestHeader(
        nsDependentCString(header.first.c_str(), header.first.size()),
        nsDependentCString(header.second.c_str(), header.second.size()),
        false /* merge */);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Body
  if (request.has_body()) {
    const std::string& body = request.body();
    nsCOMPtr<nsIStringInputStream> stream(
        do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID));
    rv = stream->SetData(body.data(), body.size());
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(mChannel);
    uploadChannel->ExplicitSetUploadStream(stream, VoidCString(), -1, method,
                                           false /* aStreamHasHeaders */);
  }

  MOZ_TRY_VAR(
      mConnectTimeoutTimer,
      NS_NewTimerWithCallback(this, request.connect_timeout_secs() * 1000,
                              nsITimer::TYPE_ONE_SHOT));
  MOZ_TRY_VAR(mReadTimeoutTimer,
              NS_NewTimerWithCallback(this, request.read_timeout_secs() * 1000,
                                      nsITimer::TYPE_ONE_SHOT));

  rv = httpChannel->AsyncOpen(this);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsCString ConvertMethod(
    appservices::httpconfig::protobuf::Request_Method method) {
  using appservices::httpconfig::protobuf::Request_Method;
  switch (method) {
    case Request_Method::Request_Method_GET:
      return "GET"_ns;
    case Request_Method::Request_Method_HEAD:
      return "HEAD"_ns;
    case Request_Method::Request_Method_POST:
      return "POST"_ns;
    case Request_Method::Request_Method_PUT:
      return "PUT"_ns;
    case Request_Method::Request_Method_DELETE:
      return "DELETE"_ns;
    case Request_Method::Request_Method_CONNECT:
      return "CONNECT"_ns;
    case Request_Method::Request_Method_OPTIONS:
      return "OPTIONS"_ns;
    case Request_Method::Request_Method_TRACE:
      return "TRACE"_ns;
  }
  return "UNKNOWN"_ns;
}

void ViaductRequest::ClearTimers() {
  if (mConnectTimeoutTimer) {
    mConnectTimeoutTimer->Cancel();
    mConnectTimeoutTimer = nullptr;
  }
  if (mReadTimeoutTimer) {
    mReadTimeoutTimer->Cancel();
    mReadTimeoutTimer = nullptr;
  }
}

void ViaductRequest::NotifyMonitor() {
  MonitorAutoLock lock(mMonitor);
  mDone = true;
  mMonitor.Notify();
}

ViaductRequest::~ViaductRequest() {
  ClearTimers();
  if (mChannel) {
    mChannel->Cancel(NS_ERROR_ABORT);
    mChannel = nullptr;
  }
  NotifyMonitor();
}

NS_IMPL_ISUPPORTS(ViaductRequest, nsIStreamListener, nsITimerCallback,
                  nsIChannelEventSink)

///////////////////////////////////////////////////////////////////////////////
// nsIStreamListener implementation

NS_IMETHODIMP
ViaductRequest::OnStartRequest(nsIRequest* aRequest) {
  if (mConnectTimeoutTimer) {
    mConnectTimeoutTimer->Cancel();
    mConnectTimeoutTimer = nullptr;
  }
  return NS_OK;
}

static nsresult AssignResponseToBuffer(nsIInputStream* aIn, void* aClosure,
                                       const char* aFromRawSegment,
                                       uint32_t aToOffset, uint32_t aCount,
                                       uint32_t* aWriteCount) {
  nsCString* buf = static_cast<nsCString*>(aClosure);
  buf->Append(aFromRawSegment, aCount);
  *aWriteCount = aCount;
  return NS_OK;
}

NS_IMETHODIMP
ViaductRequest::OnDataAvailable(nsIRequest* aRequest,
                                nsIInputStream* aInputStream, uint64_t aOffset,
                                uint32_t aCount) {
  nsresult rv;
  uint32_t readCount;
  rv = aInputStream->ReadSegments(AssignResponseToBuffer, &mBodyBuffer, aCount,
                                  &readCount);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
ViaductRequest::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  ClearTimers();
  auto defer = MakeScopeExit([&] {
    mChannel = nullptr;
    NotifyMonitor();
  });

  if (NS_FAILED(aStatusCode)) {
    nsCString errorName;
    GetErrorName(aStatusCode, errorName);
    nsPrintfCString msg("Request error: %s", errorName.get());
    mResponse.set_exception_message(msg.BeginReading());
  } else {
    nsresult rv;
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest, &rv);
    // Early return is OK because MakeScopeExit will call Notify()
    // and unblock the original calling thread.
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t httpStatus;
    rv = httpChannel->GetResponseStatus(&httpStatus);
    NS_ENSURE_SUCCESS(rv, rv);
    mResponse.set_status(httpStatus);

    nsCOMPtr<nsIURI> uri;
    httpChannel->GetURI(getter_AddRefs(uri));
    nsAutoCString uriStr;
    uri->GetSpec(uriStr);
    mResponse.set_url(uriStr.BeginReading());

    auto* headers = mResponse.mutable_headers();
    nsCOMPtr<nsIHttpHeaderVisitor> visitor = new HeaderVisitor(headers);
    rv = httpChannel->VisitResponseHeaders(visitor);
    NS_ENSURE_SUCCESS(rv, rv);

    mResponse.set_body(mBodyBuffer.BeginReading());
  }

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIChannelEventSink implementation

NS_IMETHODIMP
ViaductRequest::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t flags,
    nsIAsyncVerifyRedirectCallback* callback) {
  mChannel = aNewChannel;
  callback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsITimerCallback implementation

NS_IMETHODIMP
ViaductRequest::Notify(nsITimer* timer) {
  ClearTimers();
  // Cancelling the channel will trigger OnStopRequest.
  if (mChannel) {
    mChannel->Cancel(NS_ERROR_ABORT);
    mChannel = nullptr;
  }
  return NS_OK;
}

};  // namespace mozilla
