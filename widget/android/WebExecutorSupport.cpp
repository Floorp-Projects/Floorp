/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "WebExecutorSupport.h"

#include "nsIChannelEventSink.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsINSSErrorsService.h"
#include "nsITransportSecurityInfo.h"
#include "nsIUploadChannel2.h"
#include "nsIWebProgressListener.h"
#include "nsIX509Cert.h"

#include "nsIDNSService.h"
#include "nsIDNSListener.h"
#include "nsIDNSRecord.h"

#include "mozilla/net/DNS.h"  // for NetAddr
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/Preferences.h"

#include "nsNetUtil.h"  // for NS_NewURI, NS_NewChannel, NS_NewStreamLoader

#include "InetAddress.h"  // for java::sdk::InetAddress and java::sdk::UnknownHostException
#include "ReferrerInfo.h"

namespace mozilla {
using namespace net;

namespace widget {

static jni::ByteArray::LocalRef CertificateFromChannel(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsISupports> securityInfo;
  aChannel->GetSecurityInfo(getter_AddRefs(securityInfo));
  if (!securityInfo) {
    return nullptr;
  }

  nsresult rv;
  nsCOMPtr<nsITransportSecurityInfo> tsi = do_QueryInterface(securityInfo, &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIX509Cert> cert;
  tsi->GetServerCert(getter_AddRefs(cert));
  if (!cert) {
    return nullptr;
  }

  nsTArray<uint8_t> derBytes;
  rv = cert->GetRawDER(derBytes);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return jni::ByteArray::New(
      reinterpret_cast<const int8_t*>(derBytes.Elements()), derBytes.Length());
}

static void CompleteWithError(java::GeckoResult::Param aResult,
                              nsresult aStatus, nsIChannel* aChannel) {
  nsCOMPtr<nsINSSErrorsService> errSvc =
      do_GetService("@mozilla.org/nss_errors_service;1");
  MOZ_ASSERT(errSvc);

  uint32_t errorClass;
  nsresult rv = errSvc->GetErrorClass(aStatus, &errorClass);
  if (NS_FAILED(rv)) {
    errorClass = 0;
  }

  jni::ByteArray::LocalRef certBytes;
  if (aChannel) {
    certBytes = CertificateFromChannel(aChannel);
  }

  java::WebRequestError::LocalRef error = java::WebRequestError::FromGeckoError(
      int64_t(aStatus), NS_ERROR_GET_MODULE(aStatus), errorClass, certBytes);

  aResult->CompleteExceptionally(error.Cast<jni::Throwable>());
}

static void CompleteWithError(java::GeckoResult::Param aResult,
                              nsresult aStatus) {
  CompleteWithError(aResult, aStatus, nullptr);
}

class ByteBufferStream final : public nsIInputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit ByteBufferStream(jni::ByteBuffer::Param buffer)
      : mBuffer(buffer), mPosition(0), mClosed(false) {
    MOZ_ASSERT(mBuffer);
    MOZ_ASSERT(mBuffer->Address());
  }

  NS_IMETHOD
  Close() override {
    mClosed = true;
    return NS_OK;
  }

  NS_IMETHOD
  Available(uint64_t* aResult) override {
    if (mClosed) {
      return NS_BASE_STREAM_CLOSED;
    }

    *aResult = (mBuffer->Capacity() - mPosition);
    return NS_OK;
  }

  NS_IMETHOD
  Read(char* aBuf, uint32_t aCount, uint32_t* aCountRead) override {
    if (mClosed) {
      return NS_BASE_STREAM_CLOSED;
    }

    *aCountRead = uint32_t(
        std::min(uint64_t(mBuffer->Capacity() - mPosition), uint64_t(aCount)));

    if (*aCountRead > 0) {
      memcpy(aBuf, (char*)mBuffer->Address() + mPosition, *aCountRead);
      mPosition += *aCountRead;
    }

    return NS_OK;
  }

  NS_IMETHOD
  ReadSegments(nsWriteSegmentFun aWriter, void* aClosure, uint32_t aCount,
               uint32_t* aResult) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD
  IsNonBlocking(bool* aResult) override {
    *aResult = false;
    return NS_OK;
  }

 protected:
  virtual ~ByteBufferStream() {}

  const jni::ByteBuffer::GlobalRef mBuffer;
  uint64_t mPosition;
  bool mClosed;
};

NS_IMPL_ISUPPORTS(ByteBufferStream, nsIInputStream)

class HeaderVisitor final : public nsIHttpHeaderVisitor {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit HeaderVisitor(java::WebResponse::Builder::Param aBuilder)
      : mBuilder(aBuilder) {}

  NS_IMETHOD
  VisitHeader(const nsACString& aHeader, const nsACString& aValue) override {
    mBuilder->Header(aHeader, aValue);
    return NS_OK;
  }

 private:
  virtual ~HeaderVisitor() {}

  const java::WebResponse::Builder::GlobalRef mBuilder;
};

NS_IMPL_ISUPPORTS(HeaderVisitor, nsIHttpHeaderVisitor)

class StreamSupport final
    : public java::GeckoInputStream::Support::Natives<StreamSupport> {
 public:
  typedef java::GeckoInputStream::Support::Natives<StreamSupport> Base;
  using Base::AttachNative;
  using Base::GetNative;

  explicit StreamSupport(java::GeckoInputStream::Support::Param aInstance,
                         nsIRequest* aRequest)
      : mInstance(aInstance), mRequest(aRequest) {}

  void Close() {
    mRequest->Cancel(NS_ERROR_ABORT);
    mRequest->Resume();

    // This is basically `delete this`, so don't run anything else!
    Base::DisposeNative(mInstance);
  }

  void Resume() { mRequest->Resume(); }

 private:
  java::GeckoInputStream::Support::GlobalRef mInstance;
  nsCOMPtr<nsIRequest> mRequest;
};

class LoaderListener final : public nsIStreamListener,
                             public nsIInterfaceRequestor,
                             public nsIChannelEventSink {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit LoaderListener(java::GeckoResult::Param aResult,
                          bool aAllowRedirects, bool testStreamFailure)
      : mResult(aResult),
        mTestStreamFailure(testStreamFailure),
        mAllowRedirects(aAllowRedirects) {
    MOZ_ASSERT(mResult);
  }

  NS_IMETHOD
  OnStartRequest(nsIRequest* aRequest) override {
    MOZ_ASSERT(!mStream);

    nsresult status;
    aRequest->GetStatus(&status);
    if (NS_FAILED(status)) {
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
      CompleteWithError(mResult, status, channel);
      return NS_OK;
    }

    StreamSupport::Init();

    // We're expecting data later via OnDataAvailable, so create the stream now.
    mSupport = java::GeckoInputStream::Support::New();
    StreamSupport::AttachNative(
        mSupport, mozilla::MakeUnique<StreamSupport>(mSupport, aRequest));

    mStream = java::GeckoInputStream::New(mSupport);

    // Suspend the request immediately. It will be resumed when (if) someone
    // tries to read the Java stream.
    aRequest->Suspend();

    nsresult rv = HandleWebResponse(aRequest);
    if (NS_FAILED(rv)) {
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
      CompleteWithError(mResult, rv, channel);
      return NS_OK;
    }

    return NS_OK;
  }

  NS_IMETHOD
  OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) override {
    if (mStream) {
      if (NS_FAILED(aStatusCode)) {
        mStream->SendError();
      } else {
        mStream->SendEof();
      }
    }
    return NS_OK;
  }

  NS_IMETHOD
  OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aInputStream,
                  uint64_t aOffset, uint32_t aCount) override {
    MOZ_ASSERT(mStream);

    if (mTestStreamFailure) {
      return NS_ERROR_UNEXPECTED;
    }

    // We only need this for the ReadSegments call, the value is unused.
    uint32_t countRead;
    return aInputStream->ReadSegments(WriteSegment, this, aCount, &countRead);
  }

  NS_IMETHOD
  GetInterface(const nsIID& aIID, void** aResultOut) override {
    if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
      *aResultOut = static_cast<nsIChannelEventSink*>(this);
      NS_ADDREF_THIS();
      return NS_OK;
    }

    return NS_ERROR_NO_INTERFACE;
  }

  NS_IMETHOD
  AsyncOnChannelRedirect(nsIChannel* aOldChannel, nsIChannel* aNewChannel,
                         uint32_t flags,
                         nsIAsyncVerifyRedirectCallback* callback) override {
    if (!mAllowRedirects) {
      return NS_ERROR_ABORT;
    }

    callback->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
  }

 private:
  static nsresult WriteSegment(nsIInputStream* aInputStream, void* aClosure,
                               const char* aFromSegment, uint32_t aToOffset,
                               uint32_t aCount, uint32_t* aWriteCount) {
    LoaderListener* self = static_cast<LoaderListener*>(aClosure);
    MOZ_ASSERT(self);
    MOZ_ASSERT(self->mStream);

    *aWriteCount = aCount;

    jni::ByteArray::LocalRef buffer = jni::ByteArray::New(
        reinterpret_cast<signed char*>(const_cast<char*>(aFromSegment)),
        *aWriteCount);

    if (NS_FAILED(self->mStream->AppendBuffer(buffer))) {
      // The stream was closed or something, abort reading this channel.
      return NS_ERROR_ABORT;
    }

    return NS_OK;
  }

  NS_IMETHOD
  HandleWebResponse(nsIRequest* aRequest) {
    nsresult rv;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // URI
    nsCOMPtr<nsIURI> uri;
    rv = channel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString spec;
    rv = uri->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    java::WebResponse::Builder::LocalRef builder =
        java::WebResponse::Builder::New(spec);

    // Body stream
    if (mStream) {
      builder->Body(mStream);
    }

    // Redirected
    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
    builder->Redirected(!loadInfo->RedirectChain().IsEmpty());

    // Secure status
    nsCOMPtr<nsISupports> securityInfo;
    channel->GetSecurityInfo(getter_AddRefs(securityInfo));
    if (securityInfo) {
      nsCOMPtr<nsITransportSecurityInfo> tsi =
          do_QueryInterface(securityInfo, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      uint32_t securityState = 0;
      tsi->GetSecurityState(&securityState);
      builder->IsSecure(securityState ==
                        nsIWebProgressListener::STATE_IS_SECURE);

      nsCOMPtr<nsIX509Cert> cert;
      tsi->GetServerCert(getter_AddRefs(cert));
      if (cert) {
        nsTArray<uint8_t> derBytes;
        rv = cert->GetRawDER(derBytes);
        NS_ENSURE_SUCCESS(rv, rv);

        auto bytes = jni::ByteArray::New(
            reinterpret_cast<const int8_t*>(derBytes.Elements()),
            derBytes.Length());
        rv = builder->CertificateBytes(bytes);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    // We might need some additional settings for response to http/https request
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel, &rv));
    if (httpChannel) {
      // Status code
      uint32_t statusCode;
      rv = httpChannel->GetResponseStatus(&statusCode);
      NS_ENSURE_SUCCESS(rv, rv);
      builder->StatusCode(statusCode);

      // Headers
      RefPtr<HeaderVisitor> visitor = new HeaderVisitor(builder);
      rv = httpChannel->VisitResponseHeaders(visitor);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    mResult->Complete(builder->Build());
    return NS_OK;
  }

  virtual ~LoaderListener() {}

  const java::GeckoResult::GlobalRef mResult;
  java::GeckoInputStream::GlobalRef mStream;
  java::GeckoInputStream::Support::GlobalRef mSupport;
  const bool mTestStreamFailure;

  bool mAllowRedirects;
};

NS_IMPL_ISUPPORTS(LoaderListener, nsIStreamListener, nsIInterfaceRequestor,
                  nsIChannelEventSink)

class DNSListener final : public nsIDNSListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  DNSListener(const nsCString& aHost, java::GeckoResult::Param aResult)
      : mHost(aHost), mResult(aResult) {}

  NS_IMETHOD
  OnLookupComplete(nsICancelable* aRequest, nsIDNSRecord* aRecord,
                   nsresult aStatus) override {
    if (NS_FAILED(aStatus)) {
      CompleteUnknownHostError();
      return NS_OK;
    }

    nsresult rv = CompleteWithRecord(aRecord);
    if (NS_FAILED(rv)) {
      CompleteUnknownHostError();
      return NS_OK;
    }

    return NS_OK;
  }

  void CompleteUnknownHostError() {
    java::sdk::UnknownHostException::LocalRef error =
        java::sdk::UnknownHostException::New();
    mResult->CompleteExceptionally(error.Cast<jni::Throwable>());
  }

 private:
  nsresult CompleteWithRecord(nsIDNSRecord* aRecord) {
    nsTArray<NetAddr> addrs;
    nsresult rv = aRecord->GetAddresses(addrs);
    NS_ENSURE_SUCCESS(rv, rv);

    jni::ByteArray::LocalRef bytes;
    auto objects =
        jni::ObjectArray::New<java::sdk::InetAddress>(addrs.Length());
    for (size_t i = 0; i < addrs.Length(); i++) {
      const auto& addr = addrs[i];
      if (addr.raw.family == AF_INET) {
        bytes = jni::ByteArray::New(
            reinterpret_cast<const int8_t*>(&addr.inet.ip), 4);
      } else if (addr.raw.family == AF_INET6) {
        bytes = jni::ByteArray::New(
            reinterpret_cast<const int8_t*>(&addr.inet6.ip), 16);
      } else {
        // We don't handle this, skip it.
        continue;
      }

      objects->SetElement(i,
                          java::sdk::InetAddress::GetByAddress(mHost, bytes));
    }

    mResult->Complete(objects);
    return NS_OK;
  }

  virtual ~DNSListener() {}

  const nsCString mHost;
  const java::GeckoResult::GlobalRef mResult;
};

NS_IMPL_ISUPPORTS(DNSListener, nsIDNSListener)

static nsresult ConvertCacheMode(int32_t mode, int32_t& result) {
  switch (mode) {
    case java::WebRequest::CACHE_MODE_DEFAULT:
      result = nsIHttpChannelInternal::FETCH_CACHE_MODE_DEFAULT;
      break;
    case java::WebRequest::CACHE_MODE_NO_STORE:
      result = nsIHttpChannelInternal::FETCH_CACHE_MODE_NO_STORE;
      break;
    case java::WebRequest::CACHE_MODE_RELOAD:
      result = nsIHttpChannelInternal::FETCH_CACHE_MODE_RELOAD;
      break;
    case java::WebRequest::CACHE_MODE_NO_CACHE:
      result = nsIHttpChannelInternal::FETCH_CACHE_MODE_NO_CACHE;
      break;
    case java::WebRequest::CACHE_MODE_FORCE_CACHE:
      result = nsIHttpChannelInternal::FETCH_CACHE_MODE_FORCE_CACHE;
      break;
    case java::WebRequest::CACHE_MODE_ONLY_IF_CACHED:
      result = nsIHttpChannelInternal::FETCH_CACHE_MODE_ONLY_IF_CACHED;
      break;
    default:
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

static nsresult SetupHttpChannel(nsIHttpChannel* aHttpChannel,
                                 nsIChannel* aChannel,
                                 java::WebRequest::Param aRequest) {
  const auto req = java::WebRequest::LocalRef(aRequest);
  const auto reqBase = java::WebMessage::LocalRef(req.Cast<java::WebMessage>());

  // Method
  nsresult rv = aHttpChannel->SetRequestMethod(aRequest->Method()->ToCString());
  NS_ENSURE_SUCCESS(rv, rv);

  // Headers
  const auto keys = reqBase->GetHeaderKeys();
  const auto values = reqBase->GetHeaderValues();
  auto contentType = EmptyCString();
  for (size_t i = 0; i < keys->Length(); i++) {
    const auto key = jni::String::LocalRef(keys->GetElement(i))->ToCString();
    const auto value =
        jni::String::LocalRef(values->GetElement(i))->ToCString();

    if (key.LowerCaseEqualsASCII("content-type")) {
      contentType = value;
    }

    // We clobber any duplicate keys here because we've already merged them
    // in the upstream WebRequest.
    rv = aHttpChannel->SetRequestHeader(key, value, false /* merge */);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Body
  const auto body = req->Body();
  if (body) {
    nsCOMPtr<nsIInputStream> stream = new ByteBufferStream(body);

    nsCOMPtr<nsIUploadChannel2> uploadChannel(do_QueryInterface(aChannel, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = uploadChannel->ExplicitSetUploadStream(
        stream, contentType, -1, aRequest->Method()->ToCString(), false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Referrer
  RefPtr<nsIURI> referrerUri;
  const auto referrer = req->Referrer();
  if (referrer) {
    rv = NS_NewURI(getter_AddRefs(referrerUri), referrer->ToString());
    NS_ENSURE_SUCCESS(rv, NS_ERROR_MALFORMED_URI);
  }

  nsCOMPtr<nsIReferrerInfo> referrerInfo = new dom::ReferrerInfo(referrerUri);
  rv = aHttpChannel->SetReferrerInfoWithoutClone(referrerInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  // Cache mode
  nsCOMPtr<nsIHttpChannelInternal> internalChannel(
      do_QueryInterface(aChannel, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t cacheMode;
  rv = ConvertCacheMode(req->CacheMode(), cacheMode);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = internalChannel->SetFetchCacheMode(cacheMode);
  NS_ENSURE_SUCCESS(rv, rv);

  // We don't have any UI
  rv = internalChannel->SetBlockAuthPrompt(true);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult WebExecutorSupport::CreateStreamLoader(
    java::WebRequest::Param aRequest, int32_t aFlags,
    java::GeckoResult::Param aResult) {
  const auto req = java::WebRequest::LocalRef(aRequest);
  const auto reqBase = java::WebMessage::LocalRef(req.Cast<java::WebMessage>());

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), reqBase->Uri()->ToString());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_MALFORMED_URI);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), uri,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aFlags & java::GeckoWebExecutor::FETCH_FLAGS_ANONYMOUS) {
    channel->SetLoadFlags(nsIRequest::LOAD_ANONYMOUS);
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      CookieJarSettings::Create();
  MOZ_ASSERT(cookieJarSettings);

  nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
  loadInfo->SetCookieJarSettings(cookieJarSettings);

  // setup http/https specific things
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel, &rv));
  if (httpChannel) {
    rv = SetupHttpChannel(httpChannel, channel, aRequest);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // set up the listener
  const bool allowRedirects =
      !(aFlags & java::GeckoWebExecutor::FETCH_FLAGS_NO_REDIRECTS);
  const bool testStreamFailure =
      (aFlags & java::GeckoWebExecutor::FETCH_FLAGS_STREAM_FAILURE_TEST);

  RefPtr<LoaderListener> listener =
      new LoaderListener(aResult, allowRedirects, testStreamFailure);

  rv = channel->SetNotificationCallbacks(listener);
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally, open the channel
  rv = channel->AsyncOpen(listener);

  return NS_OK;
}

void WebExecutorSupport::Fetch(jni::Object::Param aRequest, int32_t aFlags,
                               jni::Object::Param aResult) {
  const auto request = java::WebRequest::LocalRef(aRequest);
  auto result = java::GeckoResult::LocalRef(aResult);

  nsresult rv = CreateStreamLoader(request, aFlags, result);
  if (NS_FAILED(rv)) {
    CompleteWithError(result, rv);
  }
}

static nsresult ResolveHost(nsCString& host, java::GeckoResult::Param result) {
  nsresult rv;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICancelable> cancelable;
  RefPtr<DNSListener> listener = new DNSListener(host, result);
  rv = dns->AsyncResolveNative(host, 0, listener, nullptr /* aListenerTarget */,
                               OriginAttributes(), getter_AddRefs(cancelable));
  return rv;
}

void WebExecutorSupport::Resolve(jni::String::Param aUri,
                                 jni::Object::Param aResult) {
  auto result = java::GeckoResult::LocalRef(aResult);

  nsCString uri = aUri->ToCString();
  nsresult rv = ResolveHost(uri, result);
  if (NS_FAILED(rv)) {
    java::sdk::UnknownHostException::LocalRef error =
        java::sdk::UnknownHostException::New();
    result->CompleteExceptionally(error.Cast<jni::Throwable>());
  }
}

}  // namespace widget
}  // namespace mozilla
