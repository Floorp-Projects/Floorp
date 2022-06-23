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
#ifdef MOZ_AUTH_EXTENSION
#  include "nsAuthGSSAPI.h"
#endif

#include "nsNetCID.h"

#if defined(XP_MACOSX) || defined(XP_WIN) || defined(XP_LINUX)
#  define BUILD_NETWORK_INFO_SERVICE 1
#endif

using namespace mozilla;

using ContentSnifferCache = nsCategoryCache<nsIContentSniffer>;
ContentSnifferCache* gNetSniffers = nullptr;
ContentSnifferCache* gDataSniffers = nullptr;
ContentSnifferCache* gORBSniffers = nullptr;
ContentSnifferCache* gNetAndORBSniffers = nullptr;

#define static
using nsLoadGroup = mozilla::net::nsLoadGroup;
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

#define WEB_SOCKET_HANDLER_CONSTRUCTOR(type, secure)          \
  nsresult type##Constructor(REFNSIID aIID, void** aResult) { \
    RefPtr<BaseWebSocketChannel> inst;                        \
                                                              \
    *aResult = nullptr;                                       \
    inst = WebSocketChannelConstructor(secure);               \
    return inst->QueryInterface(aIID, aResult);               \
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

nsresult CreateNewStreamConvServiceFactory(REFNSIID aIID, void** aResult) {
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
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

nsresult CreateNewMultiMixedConvFactory(REFNSIID aIID, void** aResult) {
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
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

nsresult CreateNewTXTToHTMLConvFactory(REFNSIID aIID, void** aResult) {
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
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

nsresult CreateNewHTTPCompressConvFactory(REFNSIID aIID, void** aResult) {
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
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

nsresult CreateNewUnknownDecoderFactory(REFNSIID aIID, void** aResult) {
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nullptr;

  RefPtr<nsUnknownDecoder> inst = new nsUnknownDecoder();
  return inst->QueryInterface(aIID, aResult);
}

nsresult CreateNewBinaryDetectorFactory(REFNSIID aIID, void** aResult) {
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nullptr;

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

#ifdef MOZ_AUTH_EXTENSION
  nsAuthGSSAPI::Shutdown();
#endif

  delete gNetSniffers;
  gNetSniffers = nullptr;
  delete gDataSniffers;
  gDataSniffers = nullptr;
  delete gORBSniffers;
  gORBSniffers = nullptr;
  delete gNetAndORBSniffers;
  gNetAndORBSniffers = nullptr;
}
