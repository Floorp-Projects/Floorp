/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define ALLOW_LATE_HTTPLOG_H_INCLUDE 1
#include "base/basictypes.h"

#include "nsCOMPtr.h"
#include "nsIClassInfoImpl.h"
#include "mozilla/Components.h"
#include "mozilla/ModuleUtils.h"
#include "nscore.h"
#include "nsSimpleURI.h"
#include "nsLoadGroup.h"
#include "nsMimeTypes.h"
#include "nsDNSPrefetch.h"
#include "nsXULAppAPI.h"
#include "nsCategoryCache.h"
#include "nsIContentSniffer.h"
#include "nsStandardURL.h"
#include "mozilla/net/BackgroundChannelRegistrar.h"
#include "mozilla/net/NeckoChild.h"
#include "RedirectChannelRegistrar.h"
#include "nsAuthGSSAPI.h"

#include "nsNetCID.h"

#if defined(XP_MACOSX) || defined(XP_WIN) || defined(XP_LINUX)
#  define BUILD_NETWORK_INFO_SERVICE 1
#endif

using namespace mozilla;

typedef nsCategoryCache<nsIContentSniffer> ContentSnifferCache;
ContentSnifferCache* gNetSniffers = nullptr;
ContentSnifferCache* gDataSniffers = nullptr;
ContentSnifferCache* gORBSniffers = nullptr;
ContentSnifferCache* gNetAndORBSniffers = nullptr;

#define static
typedef mozilla::net::nsLoadGroup nsLoadGroup;
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsLoadGroup, Init)
#undef static

///////////////////////////////////////////////////////////////////////////////
// protocols
///////////////////////////////////////////////////////////////////////////////

// http/https
#include "nsHttpHandler.h"
#include "Http2Compression.h"
#undef LOG
#undef LOG_ENABLED
#include "nsHttpAuthManager.h"
#include "nsHttpActivityDistributor.h"
#include "ThrottleQueue.h"
#undef LOG
#undef LOG_ENABLED

NS_IMPL_COMPONENT_FACTORY(net::nsHttpHandler) {
  return net::nsHttpHandler::GetInstance().downcast<nsIHttpProtocolHandler>();
}

NS_IMPL_COMPONENT_FACTORY(net::nsHttpsHandler) {
  auto handler = MakeRefPtr<net::nsHttpsHandler>();

  if (NS_FAILED(handler->Init())) {
    return nullptr;
  }
  return handler.forget().downcast<nsIHttpProtocolHandler>();
}

#include "WebSocketChannel.h"
#include "WebSocketChannelChild.h"
namespace mozilla::net {
static BaseWebSocketChannel* WebSocketChannelConstructor(bool aSecure) {
  if (IsNeckoChild()) {
    return new WebSocketChannelChild(aSecure);
  }

  if (aSecure) {
    return new WebSocketSSLChannel;
  }
  return new WebSocketChannel;
}

#define WEB_SOCKET_HANDLER_CONSTRUCTOR(type, secure)             \
  nsresult type##Constructor(nsISupports* aOuter, REFNSIID aIID, \
                             void** aResult) {                   \
    nsresult rv;                                                 \
                                                                 \
    RefPtr<BaseWebSocketChannel> inst;                           \
                                                                 \
    *aResult = nullptr;                                          \
    if (nullptr != aOuter) {                                     \
      rv = NS_ERROR_NO_AGGREGATION;                              \
      return rv;                                                 \
    }                                                            \
    inst = WebSocketChannelConstructor(secure);                  \
    return inst->QueryInterface(aIID, aResult);                  \
  }

WEB_SOCKET_HANDLER_CONSTRUCTOR(WebSocketChannel, false)
WEB_SOCKET_HANDLER_CONSTRUCTOR(WebSocketSSLChannel, true)
#undef WEB_SOCKET_HANDLER_CONSTRUCTOR
}  // namespace mozilla::net

///////////////////////////////////////////////////////////////////////////////

#include "nsStreamConverterService.h"
#include "nsMultiMixedConv.h"
#include "nsHTTPCompressConv.h"
#include "mozTXTToHTMLConv.h"
#include "nsUnknownDecoder.h"

///////////////////////////////////////////////////////////////////////////////

#include "nsIndexedToHTML.h"

nsresult NS_NewMultiMixedConv(nsMultiMixedConv** result);
nsresult MOZ_NewTXTToHTMLConv(mozTXTToHTMLConv** result);
nsresult NS_NewHTTPCompressConv(mozilla::net::nsHTTPCompressConv** result);
nsresult NS_NewStreamConv(nsStreamConverterService** aStreamConv);

#define INDEX_TO_HTML "?from=application/http-index-format&to=text/html"
#define MULTI_MIXED_X "?from=multipart/x-mixed-replace&to=*/*"
#define MULTI_MIXED "?from=multipart/mixed&to=*/*"
#define MULTI_BYTERANGES "?from=multipart/byteranges&to=*/*"
#define UNKNOWN_CONTENT "?from=" UNKNOWN_CONTENT_TYPE "&to=*/*"
#define GZIP_TO_UNCOMPRESSED "?from=gzip&to=uncompressed"
#define XGZIP_TO_UNCOMPRESSED "?from=x-gzip&to=uncompressed"
#define BROTLI_TO_UNCOMPRESSED "?from=br&to=uncompressed"
#define COMPRESS_TO_UNCOMPRESSED "?from=compress&to=uncompressed"
#define XCOMPRESS_TO_UNCOMPRESSED "?from=x-compress&to=uncompressed"
#define DEFLATE_TO_UNCOMPRESSED "?from=deflate&to=uncompressed"

static const mozilla::Module::CategoryEntry kNeckoCategories[] = {
    {NS_ISTREAMCONVERTER_KEY, INDEX_TO_HTML, ""},
    {NS_ISTREAMCONVERTER_KEY, MULTI_MIXED_X, ""},
    {NS_ISTREAMCONVERTER_KEY, MULTI_MIXED, ""},
    {NS_ISTREAMCONVERTER_KEY, MULTI_BYTERANGES, ""},
    {NS_ISTREAMCONVERTER_KEY, UNKNOWN_CONTENT, ""},
    {NS_ISTREAMCONVERTER_KEY, GZIP_TO_UNCOMPRESSED, ""},
    {NS_ISTREAMCONVERTER_KEY, XGZIP_TO_UNCOMPRESSED, ""},
    {NS_ISTREAMCONVERTER_KEY, BROTLI_TO_UNCOMPRESSED, ""},
    {NS_ISTREAMCONVERTER_KEY, COMPRESS_TO_UNCOMPRESSED, ""},
    {NS_ISTREAMCONVERTER_KEY, XCOMPRESS_TO_UNCOMPRESSED, ""},
    {NS_ISTREAMCONVERTER_KEY, DEFLATE_TO_UNCOMPRESSED, ""},
    NS_BINARYDETECTOR_CATEGORYENTRY,
    {nullptr}};

nsresult CreateNewStreamConvServiceFactory(nsISupports* aOuter, REFNSIID aIID,
                                           void** aResult) {
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }
  if (aOuter) {
    *aResult = nullptr;
    return NS_ERROR_NO_AGGREGATION;
  }
  RefPtr<nsStreamConverterService> inst;
  nsresult rv = NS_NewStreamConv(getter_AddRefs(inst));
  if (NS_FAILED(rv)) {
    *aResult = nullptr;
    return rv;
  }
  rv = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    *aResult = nullptr;
  }
  return rv;
}

nsresult CreateNewMultiMixedConvFactory(nsISupports* aOuter, REFNSIID aIID,
                                        void** aResult) {
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }
  if (aOuter) {
    *aResult = nullptr;
    return NS_ERROR_NO_AGGREGATION;
  }
  RefPtr<nsMultiMixedConv> inst;
  nsresult rv = NS_NewMultiMixedConv(getter_AddRefs(inst));
  if (NS_FAILED(rv)) {
    *aResult = nullptr;
    return rv;
  }
  rv = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    *aResult = nullptr;
  }
  return rv;
}

nsresult CreateNewTXTToHTMLConvFactory(nsISupports* aOuter, REFNSIID aIID,
                                       void** aResult) {
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }
  if (aOuter) {
    *aResult = nullptr;
    return NS_ERROR_NO_AGGREGATION;
  }
  RefPtr<mozTXTToHTMLConv> inst;
  nsresult rv = MOZ_NewTXTToHTMLConv(getter_AddRefs(inst));
  if (NS_FAILED(rv)) {
    *aResult = nullptr;
    return rv;
  }
  rv = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    *aResult = nullptr;
  }
  return rv;
}

nsresult CreateNewHTTPCompressConvFactory(nsISupports* aOuter, REFNSIID aIID,
                                          void** aResult) {
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }
  if (aOuter) {
    *aResult = nullptr;
    return NS_ERROR_NO_AGGREGATION;
  }
  RefPtr<mozilla::net::nsHTTPCompressConv> inst;
  nsresult rv = NS_NewHTTPCompressConv(getter_AddRefs(inst));
  if (NS_FAILED(rv)) {
    *aResult = nullptr;
    return rv;
  }
  rv = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    *aResult = nullptr;
  }
  return rv;
}

nsresult CreateNewUnknownDecoderFactory(nsISupports* aOuter, REFNSIID aIID,
                                        void** aResult) {
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nullptr;

  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  RefPtr<nsUnknownDecoder> inst = new nsUnknownDecoder();
  return inst->QueryInterface(aIID, aResult);
}

nsresult CreateNewBinaryDetectorFactory(nsISupports* aOuter, REFNSIID aIID,
                                        void** aResult) {
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nullptr;

  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  RefPtr<nsBinaryDetector> inst = new nsBinaryDetector();
  return inst->QueryInterface(aIID, aResult);
}

///////////////////////////////////////////////////////////////////////////////
// Module implementation for the net library

// Net module startup hook
nsresult nsNetStartup() {
  mozilla::net::nsStandardURL::InitGlobalObjects();
  return NS_OK;
}

// Net module shutdown hook
void nsNetShutdown() {
  // Release the url parser that the stdurl is holding.
  mozilla::net::nsStandardURL::ShutdownGlobalObjects();

  // Release global state used by the URL helper module.
  net_ShutdownURLHelper();
#ifdef XP_MACOSX
  net_ShutdownURLHelperOSX();
#endif

  // Release DNS service reference.
  nsDNSPrefetch::Shutdown();

  // Release the Websocket Admission Manager
  mozilla::net::WebSocketChannel::Shutdown();

  mozilla::net::Http2CompressionCleanup();

  mozilla::net::RedirectChannelRegistrar::Shutdown();

  mozilla::net::BackgroundChannelRegistrar::Shutdown();

  nsAuthGSSAPI::Shutdown();

  delete gNetSniffers;
  gNetSniffers = nullptr;
  delete gDataSniffers;
  gDataSniffers = nullptr;
  delete gORBSniffers;
  gORBSniffers = nullptr;
  delete gNetAndORBSniffers;
  gNetAndORBSniffers = nullptr;
}

extern const mozilla::Module kNeckoModule = {
    mozilla::Module::kVersion,
    nullptr,
    nullptr,
    kNeckoCategories,
    nullptr,
    nullptr,
    nullptr,
    mozilla::Module::ALLOW_IN_SOCKET_PROCESS};
