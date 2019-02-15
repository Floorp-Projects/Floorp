/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "WebExecutorSupport.h"

#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIInputStream.h"
#include "nsIStreamLoader.h"
#include "nsINSSErrorsService.h"
#include "nsIUploadChannel2.h"

#include "nsIDNSService.h"
#include "nsIDNSListener.h"
#include "nsIDNSRecord.h"

#include "mozilla/net/DNS.h"  // for NetAddr

#include "nsNetUtil.h"  // for NS_NewURI, NS_NewChannel, NS_NewStreamLoader

#include "InetAddress.h"  // for java::sdk::InetAddress

namespace mozilla {
using namespace net;

namespace widget {

static void CompleteWithError(java::GeckoResult::Param aResult,
                              nsresult aStatus) {
  nsCOMPtr<nsINSSErrorsService> errSvc =
      do_GetService("@mozilla.org/nss_errors_service;1");
  MOZ_ASSERT(errSvc);

  uint32_t errorClass;
  nsresult rv = errSvc->GetErrorClass(aStatus, &errorClass);
  if (NS_FAILED(rv)) {
    errorClass = 0;
  }

  java::WebRequestError::LocalRef error = java::WebRequestError::FromGeckoError(
      int64_t(aStatus), NS_ERROR_GET_MODULE(aStatus), errorClass);

  aResult->CompleteExceptionally(error.Cast<jni::Throwable>());
}

class ByteBufferStream final : public nsIInputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  ByteBufferStream(jni::ByteBuffer::Param buffer)
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
      memcpy(aBuf, (char*)mBuffer->Address(), *aCountRead);
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

  HeaderVisitor(java::WebResponse::Builder::Param aBuilder)
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

class LoaderListener final : public nsIStreamLoaderObserver,
                             public nsIRequestObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  LoaderListener(java::GeckoResult::Param aResult) : mResult(aResult) {
    MOZ_ASSERT(mResult);
  }

  NS_IMETHOD
  OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                   nsresult aStatus, uint32_t aResultLength,
                   const uint8_t* aResult) override {
    nsresult rv = HandleWebResponse(aLoader, aStatus, aResultLength, aResult);
    if (NS_FAILED(rv)) {
      CompleteWithError(mResult, rv);
    }

    return NS_OK;
  }

  NS_IMETHOD
  OnStartRequest(nsIRequest* aRequest, nsISupports* aContext) override {
    return NS_OK;
  }

  NS_IMETHOD
  OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                nsresult aStatusCode) override {
    return NS_OK;
  }

 private:
  NS_IMETHOD
  HandleWebResponse(nsIStreamLoader* aLoader, nsresult aStatus,
                    uint32_t aBodyLength, const uint8_t* aBody) {
    NS_ENSURE_SUCCESS(aStatus, aStatus);

    nsCOMPtr<nsIRequest> request;
    nsresult rv = aLoader->GetRequest(getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(request, &rv);
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

    // Status code
    uint32_t statusCode;
    rv = channel->GetResponseStatus(&statusCode);
    NS_ENSURE_SUCCESS(rv, rv);

    builder->StatusCode(statusCode);

    // Headers
    RefPtr<HeaderVisitor> visitor = new HeaderVisitor(builder);
    rv = channel->VisitResponseHeaders(visitor);
    NS_ENSURE_SUCCESS(rv, rv);

    // Redirected
    nsCOMPtr<nsILoadInfo> loadInfo;
    rv = channel->GetLoadInfo(getter_AddRefs(loadInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    builder->Redirected(!loadInfo->RedirectChain().IsEmpty());

    // Body
    if (aBodyLength) {
      jni::ByteBuffer::LocalRef buffer;

      rv = java::GeckoWebExecutor::CreateByteBuffer(aBodyLength, &buffer);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_OUT_OF_MEMORY);

      MOZ_ASSERT(buffer->Address());
      MOZ_ASSERT(buffer->Capacity() == aBodyLength);

      memcpy(buffer->Address(), aBody, aBodyLength);
      builder->Body(buffer);
    }

    mResult->Complete(builder->Build());
    return NS_OK;
  }

  virtual ~LoaderListener() {}

  const java::GeckoResult::GlobalRef mResult;
};

NS_IMPL_ISUPPORTS(LoaderListener, nsIStreamLoaderObserver, nsIRequestObserver)

class DNSListener final : public nsIDNSListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  DNSListener(const nsCString& aHost, java::GeckoResult::Param aResult)
      : mHost(aHost), mResult(aResult) {}

  NS_IMETHOD
  OnLookupComplete(nsICancelable* aRequest, nsIDNSRecord* aRecord,
                   nsresult aStatus) override {
    if (NS_FAILED(aStatus)) {
      CompleteWithError(mResult, aStatus);
      return NS_OK;
    }

    nsresult rv = CompleteWithRecord(aRecord);
    if (NS_FAILED(rv)) {
      CompleteWithError(mResult, rv);
      return NS_OK;
    }

    return NS_OK;
  }

  NS_IMETHOD
  OnLookupByTypeComplete(nsICancelable* aRequest, nsIDNSByTypeRecord* aRecord,
                         nsresult aStatus) override {
    MOZ_ASSERT_UNREACHABLE("unxpected nsIDNSListener callback");
    return NS_ERROR_UNEXPECTED;
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

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Method
  rv = httpChannel->SetRequestMethod(aRequest->Method()->ToCString());
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
    rv = httpChannel->SetRequestHeader(key, value, false /* merge */);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Body
  const auto body = reqBase->Body();
  if (body) {
    nsCOMPtr<nsIInputStream> stream = new ByteBufferStream(body);

    nsCOMPtr<nsIUploadChannel2> uploadChannel(do_QueryInterface(channel, &rv));
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

  rv = httpChannel->SetReferrer(referrerUri);
  NS_ENSURE_SUCCESS(rv, rv);

  // Cache mode
  nsCOMPtr<nsIHttpChannelInternal> internalChannel(
      do_QueryInterface(channel, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t cacheMode;
  rv = ConvertCacheMode(req->CacheMode(), cacheMode);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = internalChannel->SetFetchCacheMode(cacheMode);
  NS_ENSURE_SUCCESS(rv, rv);

  // We don't have any UI
  rv = internalChannel->SetBlockAuthPrompt(true);
  NS_ENSURE_SUCCESS(rv, rv);

  // All done, set up the stream loader
  RefPtr<LoaderListener> listener = new LoaderListener(aResult);

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), listener);
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally, open the channel
  rv = httpChannel->AsyncOpen(loader);
  NS_ENSURE_SUCCESS(rv, rv);

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
    CompleteWithError(result, rv);
  }
}

}  // namespace widget
}  // namespace mozilla
