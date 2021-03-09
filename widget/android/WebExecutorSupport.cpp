/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "WebExecutorSupport.h"

#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsINSSErrorsService.h"
#include "nsIUploadChannel2.h"
#include "nsIX509Cert.h"

#include "nsIDNSService.h"
#include "nsIDNSListener.h"
#include "nsIDNSRecord.h"

#include "mozilla/java/GeckoWebExecutorWrappers.h"
#include "mozilla/java/WebMessageWrappers.h"
#include "mozilla/java/WebRequestErrorWrappers.h"
#include "mozilla/java/WebResponseWrappers.h"
#include "mozilla/net/DNS.h"  // for NetAddr
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/Preferences.h"
#include "GeckoViewStreamListener.h"
#include "nsIPrivateBrowsingChannel.h"

#include "nsNetUtil.h"  // for NS_NewURI, NS_NewChannel, NS_NewStreamLoader

#include "InetAddress.h"  // for java::sdk::InetAddress and java::sdk::UnknownHostException
#include "ReferrerInfo.h"

namespace mozilla {
using namespace net;

namespace widget {

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
    std::tie(certBytes, std::ignore) =
        GeckoViewStreamListener::CertificateFromChannel(aChannel);
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

class LoaderListener final : public GeckoViewStreamListener {
 public:
  explicit LoaderListener(java::GeckoResult::Param aResult,
                          bool aAllowRedirects, bool testStreamFailure)
      : GeckoViewStreamListener(),
        mResult(aResult),
        mTestStreamFailure(testStreamFailure),
        mAllowRedirects(aAllowRedirects) {
    MOZ_ASSERT(mResult);
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
    nsresult rv =
        aInputStream->ReadSegments(WriteSegment, this, aCount, &countRead);
    NS_ENSURE_SUCCESS(rv, rv);
    return rv;
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

  void SendWebResponse(java::WebResponse::Param aResponse) override {
    mResult->Complete(aResponse);
  }

  void CompleteWithError(nsresult aStatus, nsIChannel* aChannel) override {
    ::CompleteWithError(mResult, aStatus, aChannel);
  }

  virtual ~LoaderListener() {}

  const java::GeckoResult::GlobalRef mResult;
  const bool mTestStreamFailure;
  bool mAllowRedirects;
};

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
    nsCOMPtr<nsIDNSAddrRecord> rec = do_QueryInterface(aRecord);
    if (!rec) {
      return NS_ERROR_UNEXPECTED;
    }
    nsresult rv = rec->GetAddresses(addrs);
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
  nsCString contentType;
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
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aFlags & java::GeckoWebExecutor::FETCH_FLAGS_ANONYMOUS) {
    channel->SetLoadFlags(nsIRequest::LOAD_ANONYMOUS);
  }

  if (aFlags & java::GeckoWebExecutor::FETCH_FLAGS_PRIVATE) {
    nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(channel);
    NS_ENSURE_TRUE(pbChannel, NS_ERROR_FAILURE);
    pbChannel->SetPrivate(true);
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
  return channel->AsyncOpen(listener);
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
  rv = dns->AsyncResolveNative(host, nsIDNSService::RESOLVE_TYPE_DEFAULT, 0,
                               nullptr, listener, nullptr /* aListenerTarget */,
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
