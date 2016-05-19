/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "necko-config.h"

#define ALLOW_LATE_HTTPLOG_H_INCLUDE 1
#include "base/basictypes.h"

#include "nsCOMPtr.h"
#include "nsIClassInfoImpl.h"
#include "mozilla/ModuleUtils.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsSocketProviderService.h"
#include "nscore.h"
#include "nsSimpleURI.h"
#include "nsSimpleNestedURI.h"
#include "nsLoadGroup.h"
#include "nsStreamLoader.h"
#include "nsIncrementalStreamLoader.h"
#include "nsUnicharStreamLoader.h"
#include "nsFileStreams.h"
#include "nsBufferedStreams.h"
#include "nsMIMEInputStream.h"
#include "nsSOCKSSocketProvider.h"
#include "nsCacheService.h"
#include "nsDiskCacheDeviceSQL.h"
#include "nsApplicationCache.h"
#include "nsApplicationCacheService.h"
#include "nsMimeTypes.h"
#include "nsDNSPrefetch.h"
#include "nsAboutProtocolHandler.h"
#include "nsXULAppAPI.h"
#include "nsCategoryCache.h"
#include "nsIContentSniffer.h"
#include "Predictor.h"
#include "nsIThreadPool.h"
#include "mozilla/net/NeckoChild.h"

#include "nsNetCID.h"

#ifndef XP_MACOSX
#define BUILD_BINHEX_DECODER 1
#endif

typedef nsCategoryCache<nsIContentSniffer> ContentSnifferCache;
ContentSnifferCache* gNetSniffers = nullptr;
ContentSnifferCache* gDataSniffers = nullptr;

///////////////////////////////////////////////////////////////////////////////

#include "nsIOService.h"
typedef mozilla::net::nsIOService nsIOService;
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIOService, nsIOService::GetInstance)

#include "nsDNSService2.h"
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIDNSService,
  nsDNSService::GetXPCOMSingleton)

#include "nsProtocolProxyService.h"
typedef mozilla::net::nsProtocolProxyService nsProtocolProxyService;
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsProtocolProxyService, Init)

#include "nsStreamTransportService.h"
typedef mozilla::net::nsStreamTransportService nsStreamTransportService;
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsStreamTransportService, Init)

#include "nsSocketTransportService2.h"
typedef mozilla::net::nsSocketTransportService nsSocketTransportService;
#undef LOG
#undef LOG_ENABLED
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSocketTransportService, Init)

#include "nsServerSocket.h"
typedef mozilla::net::nsServerSocket nsServerSocket;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsServerSocket)

#include "TLSServerSocket.h"
typedef mozilla::net::TLSServerSocket TLSServerSocket;
NS_GENERIC_FACTORY_CONSTRUCTOR(TLSServerSocket)

#include "nsUDPSocket.h"
typedef mozilla::net::nsUDPSocket nsUDPSocket;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUDPSocket)

#include "nsUDPSocketProvider.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUDPSocketProvider)

#include "nsAsyncStreamCopier.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAsyncStreamCopier)

#include "nsInputStreamPump.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsInputStreamPump)

#include "nsInputStreamChannel.h"
typedef mozilla::net::nsInputStreamChannel nsInputStreamChannel;
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsInputStreamChannel, Init)

#include "nsDownloader.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDownloader)

#include "BackgroundFileSaver.h"
namespace mozilla {
namespace net {
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(BackgroundFileSaverOutputStream, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(BackgroundFileSaverStreamListener, Init)
} // namespace net
} // namespace mozilla

#include "nsSyncStreamListener.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSyncStreamListener, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAtomicFileOutputStream)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSafeFileOutputStream)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsFileStream)

typedef mozilla::net::nsLoadGroup nsLoadGroup;
NS_GENERIC_AGGREGATED_CONSTRUCTOR_INIT(nsLoadGroup, Init)

#include "ArrayBufferInputStream.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(ArrayBufferInputStream)

#include "nsEffectiveTLDService.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsEffectiveTLDService, Init)

#include "nsSerializationHelper.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSerializationHelper)

#include "RedirectChannelRegistrar.h"
typedef mozilla::net::RedirectChannelRegistrar RedirectChannelRegistrar;
NS_GENERIC_FACTORY_CONSTRUCTOR(RedirectChannelRegistrar)

#include "CacheStorageService.h"
typedef mozilla::net::CacheStorageService CacheStorageService;
NS_GENERIC_FACTORY_CONSTRUCTOR(CacheStorageService)

#include "LoadContextInfo.h"
typedef mozilla::net::LoadContextInfoFactory LoadContextInfoFactory;
NS_GENERIC_FACTORY_CONSTRUCTOR(LoadContextInfoFactory)

///////////////////////////////////////////////////////////////////////////////

#include "mozilla/net/CaptivePortalService.h"
namespace mozilla {
namespace net {
  NS_GENERIC_FACTORY_CONSTRUCTOR(CaptivePortalService)
} // namespace net
} // namespace mozilla

#include "RequestContextService.h"
typedef mozilla::net::RequestContextService RequestContextService;
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(RequestContextService, Init)

///////////////////////////////////////////////////////////////////////////////

extern nsresult
net_NewIncrementalDownload(nsISupports *, const nsIID &, void **);

#define NS_INCREMENTALDOWNLOAD_CID \
{ /* a62af1ba-79b3-4896-8aaf-b148bfce4280 */         \
    0xa62af1ba,                                      \
    0x79b3,                                          \
    0x4896,                                          \
    {0x8a, 0xaf, 0xb1, 0x48, 0xbf, 0xce, 0x42, 0x80} \
}

///////////////////////////////////////////////////////////////////////////////

#include "nsMIMEHeaderParamImpl.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMIMEHeaderParamImpl)
///////////////////////////////////////////////////////////////////////////////

#include "nsRequestObserverProxy.h"
#include "nsSimpleStreamListener.h"
#include "nsDirIndexParser.h"
#include "nsDirIndex.h"

typedef mozilla::net::nsRequestObserverProxy nsRequestObserverProxy;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRequestObserverProxy)
typedef mozilla::net::nsSimpleStreamListener nsSimpleStreamListener;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSimpleStreamListener)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsDirIndexParser, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDirIndex)

///////////////////////////////////////////////////////////////////////////////

#include "nsStreamListenerTee.h"
typedef mozilla::net::nsStreamListenerTee nsStreamListenerTee;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStreamListenerTee)

///////////////////////////////////////////////////////////////////////////////

#ifdef NECKO_COOKIES
#include "nsCookieService.h"
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsICookieService,
  nsCookieService::GetXPCOMSingleton)
#endif

///////////////////////////////////////////////////////////////////////////////
#ifdef NECKO_WIFI

#include "nsWifiMonitor.h"
#undef LOG
#undef LOG_ENABLED
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWifiMonitor)

#endif

///////////////////////////////////////////////////////////////////////////////
// protocols
///////////////////////////////////////////////////////////////////////////////

// about:blank is mandatory
#include "nsAboutProtocolHandler.h"
#include "nsAboutBlank.h"
typedef mozilla::net::nsAboutProtocolHandler nsAboutProtocolHandler;
typedef mozilla::net::nsSafeAboutProtocolHandler nsSafeAboutProtocolHandler;
typedef mozilla::net::nsNestedAboutURI nsNestedAboutURI;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAboutProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSafeAboutProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNestedAboutURI)

#ifdef NECKO_PROTOCOL_about
// about
#ifdef NS_BUILD_REFCNT_LOGGING
#include "nsAboutBloat.h"
#endif
#include "nsAboutCache.h"
#include "nsAboutCacheEntry.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAboutCacheEntry)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsApplicationCacheService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsApplicationCacheNamespace)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsApplicationCache)

#ifdef NECKO_PROTOCOL_file
// file
#include "nsFileProtocolHandler.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsFileProtocolHandler, Init)
#endif

#ifdef NECKO_PROTOCOL_ftp
// ftp
#include "nsFtpProtocolHandler.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsFtpProtocolHandler, Init)
#endif

#ifdef NECKO_PROTOCOL_http
// http/https
#include "nsHttpHandler.h"
#include "Http2Compression.h"
#undef LOG
#undef LOG_ENABLED
#include "nsHttpAuthManager.h"
#include "nsHttpChannelAuthProvider.h"
#include "nsHttpBasicAuth.h"
#include "nsHttpDigestAuth.h"
#include "nsHttpNTLMAuth.h"
#include "nsHttpActivityDistributor.h"
#undef LOG
#undef LOG_ENABLED
namespace mozilla {
namespace net {
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHttpNTLMAuth)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsHttpHandler, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsHttpsHandler, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsHttpAuthManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHttpChannelAuthProvider)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHttpActivityDistributor)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHttpBasicAuth)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHttpDigestAuth)
} // namespace net
} // namespace mozilla
#endif // !NECKO_PROTOCOL_http

#include "mozilla/net/Dashboard.h"
#include "mozilla/net/PackagedAppService.h"
#include "mozilla/net/PackagedAppVerifier.h"
namespace mozilla {
namespace net {
  NS_GENERIC_FACTORY_CONSTRUCTOR(Dashboard)
  NS_GENERIC_FACTORY_CONSTRUCTOR(PackagedAppService)
  NS_GENERIC_FACTORY_CONSTRUCTOR(PackagedAppVerifier)
} // namespace net
} // namespace mozilla
#include "AppProtocolHandler.h"

#ifdef NECKO_PROTOCOL_res
// resource
#include "nsResProtocolHandler.h"
#include "ExtensionProtocolHandler.h"
#include "SubstitutingProtocolHandler.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsResProtocolHandler, Init)

namespace mozilla {
NS_GENERIC_FACTORY_CONSTRUCTOR(ExtensionProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(SubstitutingURL)
} // namespace mozilla
#endif

#ifdef NECKO_PROTOCOL_device
#include "nsDeviceProtocolHandler.h"
typedef mozilla::net::nsDeviceProtocolHandler nsDeviceProtocolHandler;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceProtocolHandler)
#endif

#ifdef NECKO_PROTOCOL_viewsource
#include "nsViewSourceHandler.h"
typedef mozilla::net::nsViewSourceHandler nsViewSourceHandler;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsViewSourceHandler)
#endif

#ifdef NECKO_PROTOCOL_data
#include "nsDataHandler.h"
#endif

#ifdef NECKO_PROTOCOL_wyciwyg
#include "nsWyciwygProtocolHandler.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWyciwygProtocolHandler)
#endif

#ifdef NECKO_PROTOCOL_websocket
#include "WebSocketChannel.h"
#include "WebSocketChannelChild.h"
namespace mozilla {
namespace net {
static BaseWebSocketChannel*
WebSocketChannelConstructor(bool aSecure)
{
  if (IsNeckoChild()) {
    return new WebSocketChannelChild(aSecure);
  }

  if (aSecure) {
    return new WebSocketSSLChannel;
  } else {
    return new WebSocketChannel;
  }
}

#define WEB_SOCKET_HANDLER_CONSTRUCTOR(type, secure)  \
static nsresult                                       \
type##Constructor(nsISupports *aOuter, REFNSIID aIID, \
                  void **aResult)                     \
{                                                     \
  nsresult rv;                                        \
                                                      \
  BaseWebSocketChannel * inst;                        \
                                                      \
  *aResult = nullptr;                                 \
  if (nullptr != aOuter) {                            \
    rv = NS_ERROR_NO_AGGREGATION;                     \
    return rv;                                        \
  }                                                   \
  inst = WebSocketChannelConstructor(secure);         \
  NS_ADDREF(inst);                                    \
  rv = inst->QueryInterface(aIID, aResult);           \
  NS_RELEASE(inst);                                   \
  return rv;                                          \
}

WEB_SOCKET_HANDLER_CONSTRUCTOR(WebSocketChannel, false)
WEB_SOCKET_HANDLER_CONSTRUCTOR(WebSocketSSLChannel, true)
#undef WEB_SOCKET_HANDLER_CONSTRUCTOR
} // namespace net
} // namespace mozilla
#endif

#ifdef NECKO_PROTOCOL_rtsp
#include "RtspHandler.h"
namespace mozilla {
namespace net {
NS_GENERIC_FACTORY_CONSTRUCTOR(RtspHandler)
} // namespace mozilla::net
} // namespace mozilla
#endif

///////////////////////////////////////////////////////////////////////////////

#include "nsURLParsers.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNoAuthURLParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAuthURLParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStdURLParser)

#include "nsStandardURL.h"
typedef mozilla::net::nsStandardURL nsStandardURL;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStandardURL)
typedef mozilla::net::nsSimpleURI nsSimpleURI;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSimpleURI)

typedef mozilla::net::nsSimpleNestedURI nsSimpleNestedURI;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSimpleNestedURI)

///////////////////////////////////////////////////////////////////////////////

#include "nsIDNService.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsIDNService, Init)

///////////////////////////////////////////////////////////////////////////////
#if defined(XP_WIN)
#include "nsNotifyAddrListener.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNotifyAddrListener, Init)
#elif defined(MOZ_WIDGET_COCOA)
#include "nsNetworkLinkService.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNetworkLinkService, Init)
#elif defined(MOZ_ENABLE_QTNETWORK)
#include "nsQtNetworkLinkService.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsQtNetworkLinkService, Init)
#elif defined(MOZ_WIDGET_ANDROID)
#include "nsAndroidNetworkLinkService.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAndroidNetworkLinkService)
#elif defined(XP_LINUX)
#include "nsNotifyAddrListener_Linux.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNotifyAddrListener, Init)
#endif

///////////////////////////////////////////////////////////////////////////////

#ifdef NECKO_PROTOCOL_ftp
#include "nsFTPDirListingConv.h"
nsresult NS_NewFTPDirListingConv(nsFTPDirListingConv** result);
#endif

#include "nsStreamConverterService.h"
#include "nsMultiMixedConv.h"
#include "nsHTTPCompressConv.h"
#include "mozTXTToHTMLConv.h"
#include "nsUnknownDecoder.h"

#include "nsTXTToHTMLConv.h"
namespace mozilla {
namespace net {
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsTXTToHTMLConv, Init)
} // namespace net
} // namespace mozilla

#include "nsIndexedToHTML.h"
#ifdef BUILD_BINHEX_DECODER
#include "nsBinHexDecoder.h"
#endif

nsresult NS_NewMultiMixedConv (nsMultiMixedConv** result);
nsresult MOZ_NewTXTToHTMLConv (mozTXTToHTMLConv** result);
nsresult NS_NewHTTPCompressConv  (mozilla::net::nsHTTPCompressConv ** result);
nsresult NS_NewStreamConv(nsStreamConverterService **aStreamConv);

#define FTP_TO_INDEX                 "?from=text/ftp-dir&to=application/http-index-format"
#define INDEX_TO_HTML                "?from=application/http-index-format&to=text/html"
#define MULTI_MIXED_X                "?from=multipart/x-mixed-replace&to=*/*"
#define MULTI_MIXED                  "?from=multipart/mixed&to=*/*"
#define APPLICATION_PACKAGE_CONV     "?from=" APPLICATION_PACKAGE "&to=*/*"
#define MULTI_BYTERANGES             "?from=multipart/byteranges&to=*/*"
#define UNKNOWN_CONTENT              "?from=" UNKNOWN_CONTENT_TYPE "&to=*/*"
#define GZIP_TO_UNCOMPRESSED         "?from=gzip&to=uncompressed"
#define XGZIP_TO_UNCOMPRESSED        "?from=x-gzip&to=uncompressed"
#define BROTLI_TO_UNCOMPRESSED       "?from=br&to=uncompressed"
#define COMPRESS_TO_UNCOMPRESSED     "?from=compress&to=uncompressed"
#define XCOMPRESS_TO_UNCOMPRESSED    "?from=x-compress&to=uncompressed"
#define DEFLATE_TO_UNCOMPRESSED      "?from=deflate&to=uncompressed"
#define PLAIN_TO_HTML                "?from=text/plain&to=text/html"

#ifdef BUILD_BINHEX_DECODER
#define BINHEX_TO_WILD               "?from=application/mac-binhex40&to=*/*"
#endif

static const mozilla::Module::CategoryEntry kNeckoCategories[] = {
    { NS_ISTREAMCONVERTER_KEY, FTP_TO_INDEX, "" },
    { NS_ISTREAMCONVERTER_KEY, INDEX_TO_HTML, "" },
    { NS_ISTREAMCONVERTER_KEY, MULTI_MIXED_X, "" },
    { NS_ISTREAMCONVERTER_KEY, MULTI_MIXED, "" },
    { NS_ISTREAMCONVERTER_KEY, APPLICATION_PACKAGE_CONV, "" },
    { NS_ISTREAMCONVERTER_KEY, MULTI_BYTERANGES, "" },
    { NS_ISTREAMCONVERTER_KEY, UNKNOWN_CONTENT, "" },
    { NS_ISTREAMCONVERTER_KEY, GZIP_TO_UNCOMPRESSED, "" },
    { NS_ISTREAMCONVERTER_KEY, XGZIP_TO_UNCOMPRESSED, "" },
    { NS_ISTREAMCONVERTER_KEY, BROTLI_TO_UNCOMPRESSED, "" },
    { NS_ISTREAMCONVERTER_KEY, COMPRESS_TO_UNCOMPRESSED, "" },
    { NS_ISTREAMCONVERTER_KEY, XCOMPRESS_TO_UNCOMPRESSED, "" },
    { NS_ISTREAMCONVERTER_KEY, DEFLATE_TO_UNCOMPRESSED, "" },
#ifdef BUILD_BINHEX_DECODER
    { NS_ISTREAMCONVERTER_KEY, BINHEX_TO_WILD, "" },
#endif
    { NS_ISTREAMCONVERTER_KEY, PLAIN_TO_HTML, "" },
    NS_BINARYDETECTOR_CATEGORYENTRY,
    { nullptr }
};

#ifdef BUILD_BINHEX_DECODER
typedef mozilla::net::nsBinHexDecoder nsBinHexDecoder;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBinHexDecoder)
#endif

static nsresult
CreateNewStreamConvServiceFactory(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nullptr;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    nsStreamConverterService* inst = nullptr;
    nsresult rv = NS_NewStreamConv(&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nullptr;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nullptr;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}

#ifdef NECKO_PROTOCOL_ftp
static nsresult
CreateNewFTPDirListingConv(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nullptr;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    nsFTPDirListingConv* inst = nullptr;
    nsresult rv = NS_NewFTPDirListingConv(&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nullptr;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nullptr;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}
#endif

static nsresult
CreateNewMultiMixedConvFactory(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nullptr;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    nsMultiMixedConv* inst = nullptr;
    nsresult rv = NS_NewMultiMixedConv(&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nullptr;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nullptr;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}

static nsresult
CreateNewTXTToHTMLConvFactory(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nullptr;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    mozTXTToHTMLConv* inst = nullptr;
    nsresult rv = MOZ_NewTXTToHTMLConv(&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nullptr;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nullptr;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}

static nsresult
CreateNewHTTPCompressConvFactory (nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nullptr;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    mozilla::net::nsHTTPCompressConv* inst = nullptr;
    nsresult rv = NS_NewHTTPCompressConv (&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nullptr;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nullptr;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}

static nsresult
CreateNewUnknownDecoderFactory(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;

  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nullptr;

  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsUnknownDecoder *inst;
  
  inst = new nsUnknownDecoder();
  if (!inst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

static nsresult
CreateNewBinaryDetectorFactory(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;

  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nullptr;

  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsBinaryDetector* inst = new nsBinaryDetector();
  if (!inst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// Module implementation for the net library

// Net module startup hook
static nsresult nsNetStartup()
{
    return NS_OK;
}

// Net module shutdown hook
static void nsNetShutdown()
{
    // Release the url parser that the stdurl is holding.
    nsStandardURL::ShutdownGlobalObjects();

    // Release global state used by the URL helper module.
    net_ShutdownURLHelper();
#ifdef XP_MACOSX
    net_ShutdownURLHelperOSX();
#endif
    
    // Release DNS service reference.
    nsDNSPrefetch::Shutdown();

#ifdef NECKO_PROTOCOL_websocket
    // Release the Websocket Admission Manager
    mozilla::net::WebSocketChannel::Shutdown();
#endif // NECKO_PROTOCOL_websocket

#ifdef NECKO_PROTOCOL_http
    mozilla::net::Http2CompressionCleanup();
#endif // NECKO_PROTOCOL_http

    delete gNetSniffers;
    gNetSniffers = nullptr;
    delete gDataSniffers;
    gDataSniffers = nullptr;
}

NS_DEFINE_NAMED_CID(NS_IOSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_STREAMTRANSPORTSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_SOCKETTRANSPORTSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_SERVERSOCKET_CID);
NS_DEFINE_NAMED_CID(NS_TLSSERVERSOCKET_CID);
NS_DEFINE_NAMED_CID(NS_UDPSOCKET_CID);
NS_DEFINE_NAMED_CID(NS_SOCKETPROVIDERSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_DNSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_IDNSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_EFFECTIVETLDSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_SIMPLEURI_CID);
NS_DEFINE_NAMED_CID(NS_SIMPLENESTEDURI_CID);
NS_DEFINE_NAMED_CID(NS_ASYNCSTREAMCOPIER_CID);
NS_DEFINE_NAMED_CID(NS_INPUTSTREAMPUMP_CID);
NS_DEFINE_NAMED_CID(NS_INPUTSTREAMCHANNEL_CID);
NS_DEFINE_NAMED_CID(NS_STREAMLOADER_CID);
NS_DEFINE_NAMED_CID(NS_INCREMENTALSTREAMLOADER_CID);
NS_DEFINE_NAMED_CID(NS_UNICHARSTREAMLOADER_CID);
NS_DEFINE_NAMED_CID(NS_DOWNLOADER_CID);
NS_DEFINE_NAMED_CID(NS_BACKGROUNDFILESAVEROUTPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_BACKGROUNDFILESAVERSTREAMLISTENER_CID);
NS_DEFINE_NAMED_CID(NS_SYNCSTREAMLISTENER_CID);
NS_DEFINE_NAMED_CID(NS_REQUESTOBSERVERPROXY_CID);
NS_DEFINE_NAMED_CID(NS_SIMPLESTREAMLISTENER_CID);
NS_DEFINE_NAMED_CID(NS_STREAMLISTENERTEE_CID);
NS_DEFINE_NAMED_CID(NS_LOADGROUP_CID);
NS_DEFINE_NAMED_CID(NS_LOCALFILEINPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_LOCALFILEOUTPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_PARTIALLOCALFILEINPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_ATOMICLOCALFILEOUTPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_SAFELOCALFILEOUTPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_LOCALFILESTREAM_CID);
NS_DEFINE_NAMED_CID(NS_INCREMENTALDOWNLOAD_CID);
NS_DEFINE_NAMED_CID(NS_STDURLPARSER_CID);
NS_DEFINE_NAMED_CID(NS_NOAUTHURLPARSER_CID);
NS_DEFINE_NAMED_CID(NS_AUTHURLPARSER_CID);
NS_DEFINE_NAMED_CID(NS_STANDARDURL_CID);
NS_DEFINE_NAMED_CID(NS_ARRAYBUFFERINPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_BUFFEREDINPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_BUFFEREDOUTPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_MIMEINPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_PROTOCOLPROXYSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_STREAMCONVERTERSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_PACKAGEDAPPSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_PACKAGEDAPPVERIFIER_CID);
NS_DEFINE_NAMED_CID(NS_DASHBOARD_CID);
#ifdef NECKO_PROTOCOL_ftp
NS_DEFINE_NAMED_CID(NS_FTPDIRLISTINGCONVERTER_CID);
#endif
NS_DEFINE_NAMED_CID(NS_NSINDEXEDTOHTMLCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_DIRINDEXPARSER_CID);
NS_DEFINE_NAMED_CID(NS_MULTIMIXEDCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_UNKNOWNDECODER_CID);
NS_DEFINE_NAMED_CID(NS_BINARYDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_HTTPCOMPRESSCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_NSTXTTOHTMLCONVERTER_CID);
#ifdef BUILD_BINHEX_DECODER
NS_DEFINE_NAMED_CID(NS_BINHEXDECODER_CID);
#endif
NS_DEFINE_NAMED_CID(MOZITXTTOHTMLCONV_CID);
NS_DEFINE_NAMED_CID(NS_DIRINDEX_CID);
NS_DEFINE_NAMED_CID(NS_MIMEHEADERPARAM_CID);
#ifdef NECKO_PROTOCOL_file
NS_DEFINE_NAMED_CID(NS_FILEPROTOCOLHANDLER_CID);
#endif
#ifdef NECKO_PROTOCOL_http
NS_DEFINE_NAMED_CID(NS_HTTPPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_HTTPSPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_HTTPBASICAUTH_CID);
NS_DEFINE_NAMED_CID(NS_HTTPDIGESTAUTH_CID);
NS_DEFINE_NAMED_CID(NS_HTTPNTLMAUTH_CID);
NS_DEFINE_NAMED_CID(NS_HTTPAUTHMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_HTTPCHANNELAUTHPROVIDER_CID);
NS_DEFINE_NAMED_CID(NS_HTTPACTIVITYDISTRIBUTOR_CID);
#endif // !NECKO_PROTOCOL_http
#ifdef NECKO_PROTOCOL_ftp
NS_DEFINE_NAMED_CID(NS_FTPPROTOCOLHANDLER_CID);
#endif
#ifdef NECKO_PROTOCOL_res
NS_DEFINE_NAMED_CID(NS_RESPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_EXTENSIONPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_SUBSTITUTINGURL_CID);
#endif
NS_DEFINE_NAMED_CID(NS_ABOUTPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_SAFEABOUTPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_ABOUT_BLANK_MODULE_CID);
NS_DEFINE_NAMED_CID(NS_NESTEDABOUTURI_CID);
#ifdef NECKO_PROTOCOL_about
#ifdef NS_BUILD_REFCNT_LOGGING
NS_DEFINE_NAMED_CID(NS_ABOUT_BLOAT_MODULE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_ABOUT_CACHE_MODULE_CID);
NS_DEFINE_NAMED_CID(NS_ABOUT_CACHE_ENTRY_MODULE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_SOCKSSOCKETPROVIDER_CID);
NS_DEFINE_NAMED_CID(NS_SOCKS4SOCKETPROVIDER_CID);
NS_DEFINE_NAMED_CID(NS_UDPSOCKETPROVIDER_CID);
NS_DEFINE_NAMED_CID(NS_CACHESERVICE_CID);
NS_DEFINE_NAMED_CID(NS_APPLICATIONCACHESERVICE_CID);
NS_DEFINE_NAMED_CID(NS_APPLICATIONCACHENAMESPACE_CID);
NS_DEFINE_NAMED_CID(NS_APPLICATIONCACHE_CID);
NS_DEFINE_NAMED_CID(NS_APPPROTOCOLHANDLER_CID);
#ifdef NECKO_COOKIES
NS_DEFINE_NAMED_CID(NS_COOKIEMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_COOKIESERVICE_CID);
#endif
#ifdef NECKO_WIFI
NS_DEFINE_NAMED_CID(NS_WIFI_MONITOR_COMPONENT_CID);
#endif
#ifdef NECKO_PROTOCOL_data
NS_DEFINE_NAMED_CID(NS_DATAPROTOCOLHANDLER_CID);
#endif
#ifdef NECKO_PROTOCOL_device
NS_DEFINE_NAMED_CID(NS_DEVICEPROTOCOLHANDLER_CID);
#endif
#ifdef NECKO_PROTOCOL_viewsource
NS_DEFINE_NAMED_CID(NS_VIEWSOURCEHANDLER_CID);
#endif
#ifdef NECKO_PROTOCOL_wyciwyg
NS_DEFINE_NAMED_CID(NS_WYCIWYGPROTOCOLHANDLER_CID);
#endif
#ifdef NECKO_PROTOCOL_websocket
NS_DEFINE_NAMED_CID(NS_WEBSOCKETPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_WEBSOCKETSSLPROTOCOLHANDLER_CID);
#endif
#ifdef NECKO_PROTOCOL_rtsp
NS_DEFINE_NAMED_CID(NS_RTSPPROTOCOLHANDLER_CID);
#endif
#if defined(XP_WIN)
NS_DEFINE_NAMED_CID(NS_NETWORK_LINK_SERVICE_CID);
#elif defined(MOZ_WIDGET_COCOA)
NS_DEFINE_NAMED_CID(NS_NETWORK_LINK_SERVICE_CID);
#elif defined(MOZ_ENABLE_QTNETWORK)
NS_DEFINE_NAMED_CID(NS_NETWORK_LINK_SERVICE_CID);
#elif defined(MOZ_WIDGET_ANDROID)
NS_DEFINE_NAMED_CID(NS_NETWORK_LINK_SERVICE_CID);
#elif defined(XP_LINUX)
NS_DEFINE_NAMED_CID(NS_NETWORK_LINK_SERVICE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_SERIALIZATION_HELPER_CID);
NS_DEFINE_NAMED_CID(NS_REDIRECTCHANNELREGISTRAR_CID);
NS_DEFINE_NAMED_CID(NS_CACHE_STORAGE_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_NSILOADCONTEXTINFOFACTORY_CID);
NS_DEFINE_NAMED_CID(NS_NETWORKPREDICTOR_CID);
NS_DEFINE_NAMED_CID(NS_CAPTIVEPORTAL_CID);
NS_DEFINE_NAMED_CID(NS_REQUESTCONTEXTSERVICE_CID);

static const mozilla::Module::CIDEntry kNeckoCIDs[] = {
    { &kNS_IOSERVICE_CID, false, nullptr, nsIOServiceConstructor },
    { &kNS_STREAMTRANSPORTSERVICE_CID, false, nullptr, nsStreamTransportServiceConstructor },
    { &kNS_SOCKETTRANSPORTSERVICE_CID, false, nullptr, nsSocketTransportServiceConstructor },
    { &kNS_SERVERSOCKET_CID, false, nullptr, nsServerSocketConstructor },
    { &kNS_TLSSERVERSOCKET_CID, false, nullptr, TLSServerSocketConstructor },
    { &kNS_UDPSOCKET_CID, false, nullptr, nsUDPSocketConstructor },
    { &kNS_SOCKETPROVIDERSERVICE_CID, false, nullptr, nsSocketProviderService::Create },
    { &kNS_DNSSERVICE_CID, false, nullptr, nsIDNSServiceConstructor },
    { &kNS_IDNSERVICE_CID, false, nullptr, nsIDNServiceConstructor },
    { &kNS_EFFECTIVETLDSERVICE_CID, false, nullptr, nsEffectiveTLDServiceConstructor },
    { &kNS_SIMPLEURI_CID, false, nullptr, nsSimpleURIConstructor },
    { &kNS_SIMPLENESTEDURI_CID, false, nullptr, nsSimpleNestedURIConstructor },
    { &kNS_ASYNCSTREAMCOPIER_CID, false, nullptr, nsAsyncStreamCopierConstructor },
    { &kNS_INPUTSTREAMPUMP_CID, false, nullptr, nsInputStreamPumpConstructor },
    { &kNS_INPUTSTREAMCHANNEL_CID, false, nullptr, nsInputStreamChannelConstructor },
    { &kNS_STREAMLOADER_CID, false, nullptr, mozilla::net::nsStreamLoader::Create },
    { &kNS_INCREMENTALSTREAMLOADER_CID, false, nullptr, nsIncrementalStreamLoader::Create },
    { &kNS_UNICHARSTREAMLOADER_CID, false, nullptr, nsUnicharStreamLoader::Create },
    { &kNS_DOWNLOADER_CID, false, nullptr, nsDownloaderConstructor },
    { &kNS_BACKGROUNDFILESAVEROUTPUTSTREAM_CID, false, nullptr,
      mozilla::net::BackgroundFileSaverOutputStreamConstructor },
    { &kNS_BACKGROUNDFILESAVERSTREAMLISTENER_CID, false, nullptr,
      mozilla::net::BackgroundFileSaverStreamListenerConstructor },
    { &kNS_SYNCSTREAMLISTENER_CID, false, nullptr, nsSyncStreamListenerConstructor },
    { &kNS_REQUESTOBSERVERPROXY_CID, false, nullptr, nsRequestObserverProxyConstructor },
    { &kNS_SIMPLESTREAMLISTENER_CID, false, nullptr, nsSimpleStreamListenerConstructor },
    { &kNS_STREAMLISTENERTEE_CID, false, nullptr, nsStreamListenerTeeConstructor },
    { &kNS_LOADGROUP_CID, false, nullptr, nsLoadGroupConstructor },
    { &kNS_LOCALFILEINPUTSTREAM_CID, false, nullptr, nsFileInputStream::Create },
    { &kNS_LOCALFILEOUTPUTSTREAM_CID, false, nullptr, nsFileOutputStream::Create },
    { &kNS_PARTIALLOCALFILEINPUTSTREAM_CID, false, nullptr, nsPartialFileInputStream::Create },
    { &kNS_ATOMICLOCALFILEOUTPUTSTREAM_CID, false, nullptr, nsAtomicFileOutputStreamConstructor },
    { &kNS_SAFELOCALFILEOUTPUTSTREAM_CID, false, nullptr, nsSafeFileOutputStreamConstructor },
    { &kNS_LOCALFILESTREAM_CID, false, nullptr, nsFileStreamConstructor },
    { &kNS_INCREMENTALDOWNLOAD_CID, false, nullptr, net_NewIncrementalDownload },
    { &kNS_STDURLPARSER_CID, false, nullptr, nsStdURLParserConstructor },
    { &kNS_NOAUTHURLPARSER_CID, false, nullptr, nsNoAuthURLParserConstructor },
    { &kNS_AUTHURLPARSER_CID, false, nullptr, nsAuthURLParserConstructor },
    { &kNS_STANDARDURL_CID, false, nullptr, nsStandardURLConstructor },
    { &kNS_ARRAYBUFFERINPUTSTREAM_CID, false, nullptr, ArrayBufferInputStreamConstructor },
    { &kNS_BUFFEREDINPUTSTREAM_CID, false, nullptr, nsBufferedInputStream::Create },
    { &kNS_BUFFEREDOUTPUTSTREAM_CID, false, nullptr, nsBufferedOutputStream::Create },
    { &kNS_MIMEINPUTSTREAM_CID, false, nullptr, nsMIMEInputStreamConstructor },
    { &kNS_PROTOCOLPROXYSERVICE_CID, true, nullptr, nsProtocolProxyServiceConstructor },
    { &kNS_STREAMCONVERTERSERVICE_CID, false, nullptr, CreateNewStreamConvServiceFactory },
    { &kNS_PACKAGEDAPPSERVICE_CID, false, NULL, mozilla::net::PackagedAppServiceConstructor },
    { &kNS_PACKAGEDAPPVERIFIER_CID, false, NULL, mozilla::net::PackagedAppVerifierConstructor },
    { &kNS_DASHBOARD_CID, false, nullptr, mozilla::net::DashboardConstructor },
#ifdef NECKO_PROTOCOL_ftp
    { &kNS_FTPDIRLISTINGCONVERTER_CID, false, nullptr, CreateNewFTPDirListingConv },
#endif
    { &kNS_NSINDEXEDTOHTMLCONVERTER_CID, false, nullptr, nsIndexedToHTML::Create },
    { &kNS_DIRINDEXPARSER_CID, false, nullptr, nsDirIndexParserConstructor },
    { &kNS_MULTIMIXEDCONVERTER_CID, false, nullptr, CreateNewMultiMixedConvFactory },
    { &kNS_UNKNOWNDECODER_CID, false, nullptr, CreateNewUnknownDecoderFactory },
    { &kNS_BINARYDETECTOR_CID, false, nullptr, CreateNewBinaryDetectorFactory },
    { &kNS_HTTPCOMPRESSCONVERTER_CID, false, nullptr, CreateNewHTTPCompressConvFactory },
    { &kNS_NSTXTTOHTMLCONVERTER_CID, false, nullptr, mozilla::net::nsTXTToHTMLConvConstructor },
#ifdef BUILD_BINHEX_DECODER
    { &kNS_BINHEXDECODER_CID, false, nullptr, nsBinHexDecoderConstructor },
#endif
    { &kMOZITXTTOHTMLCONV_CID, false, nullptr, CreateNewTXTToHTMLConvFactory },
    { &kNS_DIRINDEX_CID, false, nullptr, nsDirIndexConstructor },
    { &kNS_MIMEHEADERPARAM_CID, false, nullptr, nsMIMEHeaderParamImplConstructor },
#ifdef NECKO_PROTOCOL_file
    { &kNS_FILEPROTOCOLHANDLER_CID, false, nullptr, nsFileProtocolHandlerConstructor },
#endif
#ifdef NECKO_PROTOCOL_http
    { &kNS_HTTPPROTOCOLHANDLER_CID, false, nullptr, mozilla::net::nsHttpHandlerConstructor },
    { &kNS_HTTPSPROTOCOLHANDLER_CID, false, nullptr, mozilla::net::nsHttpsHandlerConstructor },
    { &kNS_HTTPBASICAUTH_CID, false, nullptr, mozilla::net::nsHttpBasicAuthConstructor },
    { &kNS_HTTPDIGESTAUTH_CID, false, nullptr, mozilla::net::nsHttpDigestAuthConstructor },
    { &kNS_HTTPNTLMAUTH_CID, false, nullptr, mozilla::net::nsHttpNTLMAuthConstructor },
    { &kNS_HTTPAUTHMANAGER_CID, false, nullptr, mozilla::net::nsHttpAuthManagerConstructor },
    { &kNS_HTTPCHANNELAUTHPROVIDER_CID, false, nullptr, mozilla::net::nsHttpChannelAuthProviderConstructor },
    { &kNS_HTTPACTIVITYDISTRIBUTOR_CID, false, nullptr, mozilla::net::nsHttpActivityDistributorConstructor },
#endif // !NECKO_PROTOCOL_http
#ifdef NECKO_PROTOCOL_ftp
    { &kNS_FTPPROTOCOLHANDLER_CID, false, nullptr, nsFtpProtocolHandlerConstructor },
#endif
#ifdef NECKO_PROTOCOL_res
    { &kNS_RESPROTOCOLHANDLER_CID, false, nullptr, nsResProtocolHandlerConstructor },
    { &kNS_EXTENSIONPROTOCOLHANDLER_CID, false, nullptr, mozilla::ExtensionProtocolHandlerConstructor },
    { &kNS_SUBSTITUTINGURL_CID, false, nullptr, mozilla::SubstitutingURLConstructor },
#endif
    { &kNS_ABOUTPROTOCOLHANDLER_CID, false, nullptr, nsAboutProtocolHandlerConstructor },
    { &kNS_SAFEABOUTPROTOCOLHANDLER_CID, false, nullptr, nsSafeAboutProtocolHandlerConstructor },
    { &kNS_ABOUT_BLANK_MODULE_CID, false, nullptr, nsAboutBlank::Create },
    { &kNS_NESTEDABOUTURI_CID, false, nullptr, nsNestedAboutURIConstructor },
#ifdef NECKO_PROTOCOL_about
#ifdef NS_BUILD_REFCNT_LOGGING
    { &kNS_ABOUT_BLOAT_MODULE_CID, false, nullptr, nsAboutBloat::Create },
#endif
    { &kNS_ABOUT_CACHE_MODULE_CID, false, nullptr, nsAboutCache::Create },
    { &kNS_ABOUT_CACHE_ENTRY_MODULE_CID, false, nullptr, nsAboutCacheEntryConstructor },
#endif
    { &kNS_SOCKSSOCKETPROVIDER_CID, false, nullptr, nsSOCKSSocketProvider::CreateV5 },
    { &kNS_SOCKS4SOCKETPROVIDER_CID, false, nullptr, nsSOCKSSocketProvider::CreateV4 },
    { &kNS_UDPSOCKETPROVIDER_CID, false, nullptr, nsUDPSocketProviderConstructor },
    { &kNS_CACHESERVICE_CID, false, nullptr, nsCacheService::Create },
    { &kNS_APPLICATIONCACHESERVICE_CID, false, nullptr, nsApplicationCacheServiceConstructor },
    { &kNS_APPLICATIONCACHENAMESPACE_CID, false, nullptr, nsApplicationCacheNamespaceConstructor },
    { &kNS_APPLICATIONCACHE_CID, false, nullptr, nsApplicationCacheConstructor },
    { &kNS_APPPROTOCOLHANDLER_CID, false, nullptr, AppProtocolHandler::Create },
#ifdef NECKO_COOKIES
    { &kNS_COOKIEMANAGER_CID, false, nullptr, nsICookieServiceConstructor },
    { &kNS_COOKIESERVICE_CID, false, nullptr, nsICookieServiceConstructor },
#endif
#ifdef NECKO_WIFI
    { &kNS_WIFI_MONITOR_COMPONENT_CID, false, nullptr, nsWifiMonitorConstructor },
#endif
#ifdef NECKO_PROTOCOL_data
    { &kNS_DATAPROTOCOLHANDLER_CID, false, nullptr, nsDataHandler::Create },
#endif
#ifdef NECKO_PROTOCOL_device
    { &kNS_DEVICEPROTOCOLHANDLER_CID, false, nullptr, nsDeviceProtocolHandlerConstructor},
#endif
#ifdef NECKO_PROTOCOL_viewsource
    { &kNS_VIEWSOURCEHANDLER_CID, false, nullptr, nsViewSourceHandlerConstructor },
#endif
#ifdef NECKO_PROTOCOL_wyciwyg
    { &kNS_WYCIWYGPROTOCOLHANDLER_CID, false, nullptr, nsWyciwygProtocolHandlerConstructor },
#endif
#ifdef NECKO_PROTOCOL_websocket
    { &kNS_WEBSOCKETPROTOCOLHANDLER_CID, false, nullptr,
      mozilla::net::WebSocketChannelConstructor },
    { &kNS_WEBSOCKETSSLPROTOCOLHANDLER_CID, false, nullptr,
      mozilla::net::WebSocketSSLChannelConstructor },
#endif
#ifdef NECKO_PROTOCOL_rtsp
    { &kNS_RTSPPROTOCOLHANDLER_CID, false, nullptr, mozilla::net::RtspHandlerConstructor },
#endif
#if defined(XP_WIN)
    { &kNS_NETWORK_LINK_SERVICE_CID, false, nullptr, nsNotifyAddrListenerConstructor },
#elif defined(MOZ_WIDGET_COCOA)
    { &kNS_NETWORK_LINK_SERVICE_CID, false, nullptr, nsNetworkLinkServiceConstructor },
#elif defined(MOZ_ENABLE_QTNETWORK)
    { &kNS_NETWORK_LINK_SERVICE_CID, false, nullptr, nsQtNetworkLinkServiceConstructor },
#elif defined(MOZ_WIDGET_ANDROID)
    { &kNS_NETWORK_LINK_SERVICE_CID, false, nullptr, nsAndroidNetworkLinkServiceConstructor },
#elif defined(XP_LINUX)
    { &kNS_NETWORK_LINK_SERVICE_CID, false, nullptr, nsNotifyAddrListenerConstructor },
#endif
    { &kNS_SERIALIZATION_HELPER_CID, false, nullptr, nsSerializationHelperConstructor },
    { &kNS_REDIRECTCHANNELREGISTRAR_CID, false, nullptr, RedirectChannelRegistrarConstructor },
    { &kNS_CACHE_STORAGE_SERVICE_CID, false, nullptr, CacheStorageServiceConstructor },
    { &kNS_NSILOADCONTEXTINFOFACTORY_CID, false, nullptr, LoadContextInfoFactoryConstructor },
    { &kNS_NETWORKPREDICTOR_CID, false, nullptr, mozilla::net::Predictor::Create },
    { &kNS_CAPTIVEPORTAL_CID, false, nullptr, mozilla::net::CaptivePortalServiceConstructor },
    { &kNS_REQUESTCONTEXTSERVICE_CID, false, nullptr, RequestContextServiceConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kNeckoContracts[] = {
    { NS_IOSERVICE_CONTRACTID, &kNS_IOSERVICE_CID },
    { NS_NETUTIL_CONTRACTID, &kNS_IOSERVICE_CID },
    { NS_STREAMTRANSPORTSERVICE_CONTRACTID, &kNS_STREAMTRANSPORTSERVICE_CID },
    { NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &kNS_SOCKETTRANSPORTSERVICE_CID },
    { NS_SERVERSOCKET_CONTRACTID, &kNS_SERVERSOCKET_CID },
    { NS_TLSSERVERSOCKET_CONTRACTID, &kNS_TLSSERVERSOCKET_CID },
    { NS_UDPSOCKET_CONTRACTID, &kNS_UDPSOCKET_CID },
    { NS_SOCKETPROVIDERSERVICE_CONTRACTID, &kNS_SOCKETPROVIDERSERVICE_CID },
    { NS_DNSSERVICE_CONTRACTID, &kNS_DNSSERVICE_CID },
    { NS_IDNSERVICE_CONTRACTID, &kNS_IDNSERVICE_CID },
    { NS_EFFECTIVETLDSERVICE_CONTRACTID, &kNS_EFFECTIVETLDSERVICE_CID },
    { NS_SIMPLEURI_CONTRACTID, &kNS_SIMPLEURI_CID },
    { NS_ASYNCSTREAMCOPIER_CONTRACTID, &kNS_ASYNCSTREAMCOPIER_CID },
    { NS_INPUTSTREAMPUMP_CONTRACTID, &kNS_INPUTSTREAMPUMP_CID },
    { NS_INPUTSTREAMCHANNEL_CONTRACTID, &kNS_INPUTSTREAMCHANNEL_CID },
    { NS_STREAMLOADER_CONTRACTID, &kNS_STREAMLOADER_CID },
    { NS_INCREMENTALSTREAMLOADER_CONTRACTID, &kNS_INCREMENTALSTREAMLOADER_CID },
    { NS_UNICHARSTREAMLOADER_CONTRACTID, &kNS_UNICHARSTREAMLOADER_CID },
    { NS_DOWNLOADER_CONTRACTID, &kNS_DOWNLOADER_CID },
    { NS_BACKGROUNDFILESAVEROUTPUTSTREAM_CONTRACTID, &kNS_BACKGROUNDFILESAVEROUTPUTSTREAM_CID },
    { NS_BACKGROUNDFILESAVERSTREAMLISTENER_CONTRACTID, &kNS_BACKGROUNDFILESAVERSTREAMLISTENER_CID },
    { NS_SYNCSTREAMLISTENER_CONTRACTID, &kNS_SYNCSTREAMLISTENER_CID },
    { NS_REQUESTOBSERVERPROXY_CONTRACTID, &kNS_REQUESTOBSERVERPROXY_CID },
    { NS_SIMPLESTREAMLISTENER_CONTRACTID, &kNS_SIMPLESTREAMLISTENER_CID },
    { NS_STREAMLISTENERTEE_CONTRACTID, &kNS_STREAMLISTENERTEE_CID },
    { NS_LOADGROUP_CONTRACTID, &kNS_LOADGROUP_CID },
    { NS_LOCALFILEINPUTSTREAM_CONTRACTID, &kNS_LOCALFILEINPUTSTREAM_CID },
    { NS_LOCALFILEOUTPUTSTREAM_CONTRACTID, &kNS_LOCALFILEOUTPUTSTREAM_CID },
    { NS_PARTIALLOCALFILEINPUTSTREAM_CONTRACTID, &kNS_PARTIALLOCALFILEINPUTSTREAM_CID },
    { NS_ATOMICLOCALFILEOUTPUTSTREAM_CONTRACTID, &kNS_ATOMICLOCALFILEOUTPUTSTREAM_CID },
    { NS_SAFELOCALFILEOUTPUTSTREAM_CONTRACTID, &kNS_SAFELOCALFILEOUTPUTSTREAM_CID },
    { NS_LOCALFILESTREAM_CONTRACTID, &kNS_LOCALFILESTREAM_CID },
    { NS_INCREMENTALDOWNLOAD_CONTRACTID, &kNS_INCREMENTALDOWNLOAD_CID },
    { NS_STDURLPARSER_CONTRACTID, &kNS_STDURLPARSER_CID },
    { NS_NOAUTHURLPARSER_CONTRACTID, &kNS_NOAUTHURLPARSER_CID },
    { NS_AUTHURLPARSER_CONTRACTID, &kNS_AUTHURLPARSER_CID },
    { NS_STANDARDURL_CONTRACTID, &kNS_STANDARDURL_CID },
    { NS_ARRAYBUFFERINPUTSTREAM_CONTRACTID, &kNS_ARRAYBUFFERINPUTSTREAM_CID },
    { NS_BUFFEREDINPUTSTREAM_CONTRACTID, &kNS_BUFFEREDINPUTSTREAM_CID },
    { NS_BUFFEREDOUTPUTSTREAM_CONTRACTID, &kNS_BUFFEREDOUTPUTSTREAM_CID },
    { NS_MIMEINPUTSTREAM_CONTRACTID, &kNS_MIMEINPUTSTREAM_CID },
    { NS_PROTOCOLPROXYSERVICE_CONTRACTID, &kNS_PROTOCOLPROXYSERVICE_CID },
    { NS_STREAMCONVERTERSERVICE_CONTRACTID, &kNS_STREAMCONVERTERSERVICE_CID },
    { NS_PACKAGEDAPPSERVICE_CONTRACTID, &kNS_PACKAGEDAPPSERVICE_CID },
    { NS_PACKAGEDAPPVERIFIER_CONTRACTID, &kNS_PACKAGEDAPPVERIFIER_CID },
    { NS_DASHBOARD_CONTRACTID, &kNS_DASHBOARD_CID },
#ifdef NECKO_PROTOCOL_ftp
    { NS_ISTREAMCONVERTER_KEY FTP_TO_INDEX, &kNS_FTPDIRLISTINGCONVERTER_CID },
#endif
    { NS_ISTREAMCONVERTER_KEY INDEX_TO_HTML, &kNS_NSINDEXEDTOHTMLCONVERTER_CID },
    { NS_DIRINDEXPARSER_CONTRACTID, &kNS_DIRINDEXPARSER_CID },
    { NS_ISTREAMCONVERTER_KEY MULTI_MIXED_X, &kNS_MULTIMIXEDCONVERTER_CID },
    { NS_ISTREAMCONVERTER_KEY MULTI_BYTERANGES, &kNS_MULTIMIXEDCONVERTER_CID },
    { NS_ISTREAMCONVERTER_KEY MULTI_MIXED, &kNS_MULTIMIXEDCONVERTER_CID },
    { NS_ISTREAMCONVERTER_KEY APPLICATION_PACKAGE_CONV, &kNS_MULTIMIXEDCONVERTER_CID },
    { NS_ISTREAMCONVERTER_KEY UNKNOWN_CONTENT, &kNS_UNKNOWNDECODER_CID },
    { NS_GENERIC_CONTENT_SNIFFER, &kNS_UNKNOWNDECODER_CID },
    { NS_BINARYDETECTOR_CONTRACTID, &kNS_BINARYDETECTOR_CID },
    { NS_ISTREAMCONVERTER_KEY GZIP_TO_UNCOMPRESSED, &kNS_HTTPCOMPRESSCONVERTER_CID },
    { NS_ISTREAMCONVERTER_KEY XGZIP_TO_UNCOMPRESSED, &kNS_HTTPCOMPRESSCONVERTER_CID },
    { NS_ISTREAMCONVERTER_KEY BROTLI_TO_UNCOMPRESSED, &kNS_HTTPCOMPRESSCONVERTER_CID },
    { NS_ISTREAMCONVERTER_KEY COMPRESS_TO_UNCOMPRESSED, &kNS_HTTPCOMPRESSCONVERTER_CID },
    { NS_ISTREAMCONVERTER_KEY XCOMPRESS_TO_UNCOMPRESSED, &kNS_HTTPCOMPRESSCONVERTER_CID },
    { NS_ISTREAMCONVERTER_KEY DEFLATE_TO_UNCOMPRESSED, &kNS_HTTPCOMPRESSCONVERTER_CID },
    { NS_ISTREAMCONVERTER_KEY PLAIN_TO_HTML, &kNS_NSTXTTOHTMLCONVERTER_CID },
#ifdef BUILD_BINHEX_DECODER
    { NS_ISTREAMCONVERTER_KEY BINHEX_TO_WILD, &kNS_BINHEXDECODER_CID },
#endif
    { MOZ_TXTTOHTMLCONV_CONTRACTID, &kMOZITXTTOHTMLCONV_CID },
    { "@mozilla.org/dirIndex;1", &kNS_DIRINDEX_CID },
    { NS_MIMEHEADERPARAM_CONTRACTID, &kNS_MIMEHEADERPARAM_CID },
#ifdef NECKO_PROTOCOL_file
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "file", &kNS_FILEPROTOCOLHANDLER_CID },
#endif
#ifdef NECKO_PROTOCOL_http
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &kNS_HTTPPROTOCOLHANDLER_CID },
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "https", &kNS_HTTPSPROTOCOLHANDLER_CID },
    { NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX "basic", &kNS_HTTPBASICAUTH_CID },
    { NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX "digest", &kNS_HTTPDIGESTAUTH_CID },
    { NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX "ntlm", &kNS_HTTPNTLMAUTH_CID },
    { NS_HTTPAUTHMANAGER_CONTRACTID, &kNS_HTTPAUTHMANAGER_CID },
    { NS_HTTPCHANNELAUTHPROVIDER_CONTRACTID, &kNS_HTTPCHANNELAUTHPROVIDER_CID },
    { NS_HTTPACTIVITYDISTRIBUTOR_CONTRACTID, &kNS_HTTPACTIVITYDISTRIBUTOR_CID },
#endif // !NECKO_PROTOCOL_http
#ifdef NECKO_PROTOCOL_ftp
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "ftp", &kNS_FTPPROTOCOLHANDLER_CID },
#endif
#ifdef NECKO_PROTOCOL_res
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "resource", &kNS_RESPROTOCOLHANDLER_CID },
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "moz-extension", &kNS_EXTENSIONPROTOCOLHANDLER_CID },
#endif
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "about", &kNS_ABOUTPROTOCOLHANDLER_CID },
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "moz-safe-about", &kNS_SAFEABOUTPROTOCOLHANDLER_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "blank", &kNS_ABOUT_BLANK_MODULE_CID },
#ifdef NECKO_PROTOCOL_about
#ifdef NS_BUILD_REFCNT_LOGGING
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "bloat", &kNS_ABOUT_BLOAT_MODULE_CID },
#endif
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "cache", &kNS_ABOUT_CACHE_MODULE_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "cache-entry", &kNS_ABOUT_CACHE_ENTRY_MODULE_CID },
#endif
    { NS_NETWORK_SOCKET_CONTRACTID_PREFIX "socks", &kNS_SOCKSSOCKETPROVIDER_CID },
    { NS_NETWORK_SOCKET_CONTRACTID_PREFIX "socks4", &kNS_SOCKS4SOCKETPROVIDER_CID },
    { NS_NETWORK_SOCKET_CONTRACTID_PREFIX "udp", &kNS_UDPSOCKETPROVIDER_CID },
    { NS_CACHESERVICE_CONTRACTID, &kNS_CACHESERVICE_CID },
    { NS_APPLICATIONCACHESERVICE_CONTRACTID, &kNS_APPLICATIONCACHESERVICE_CID },
    { NS_APPLICATIONCACHENAMESPACE_CONTRACTID, &kNS_APPLICATIONCACHENAMESPACE_CID },
    { NS_APPLICATIONCACHE_CONTRACTID, &kNS_APPLICATIONCACHE_CID },
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "app", &kNS_APPPROTOCOLHANDLER_CID },
#ifdef NECKO_COOKIES
    { NS_COOKIEMANAGER_CONTRACTID, &kNS_COOKIEMANAGER_CID },
    { NS_COOKIESERVICE_CONTRACTID, &kNS_COOKIESERVICE_CID },
#endif
#ifdef NECKO_WIFI
    { NS_WIFI_MONITOR_CONTRACTID, &kNS_WIFI_MONITOR_COMPONENT_CID },
#endif
#ifdef NECKO_PROTOCOL_data
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "data", &kNS_DATAPROTOCOLHANDLER_CID },
#endif
#ifdef NECKO_PROTOCOL_device
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "moz-device", &kNS_DEVICEPROTOCOLHANDLER_CID },
#endif
#ifdef NECKO_PROTOCOL_viewsource
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "view-source", &kNS_VIEWSOURCEHANDLER_CID },
#endif
#ifdef NECKO_PROTOCOL_wyciwyg
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "wyciwyg", &kNS_WYCIWYGPROTOCOLHANDLER_CID },
#endif
#ifdef NECKO_PROTOCOL_websocket
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "ws", &kNS_WEBSOCKETPROTOCOLHANDLER_CID },
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "wss", &kNS_WEBSOCKETSSLPROTOCOLHANDLER_CID },
#endif
#ifdef NECKO_PROTOCOL_rtsp
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "rtsp", &kNS_RTSPPROTOCOLHANDLER_CID },
#endif
#if defined(XP_WIN)
    { NS_NETWORK_LINK_SERVICE_CONTRACTID, &kNS_NETWORK_LINK_SERVICE_CID },
#elif defined(MOZ_WIDGET_COCOA)
    { NS_NETWORK_LINK_SERVICE_CONTRACTID, &kNS_NETWORK_LINK_SERVICE_CID },
#elif defined(MOZ_ENABLE_QTNETWORK)
    { NS_NETWORK_LINK_SERVICE_CONTRACTID, &kNS_NETWORK_LINK_SERVICE_CID },
#elif defined(MOZ_WIDGET_ANDROID)
    { NS_NETWORK_LINK_SERVICE_CONTRACTID, &kNS_NETWORK_LINK_SERVICE_CID },
#elif defined(XP_LINUX)
    { NS_NETWORK_LINK_SERVICE_CONTRACTID, &kNS_NETWORK_LINK_SERVICE_CID },
#endif
    { NS_SERIALIZATION_HELPER_CONTRACTID, &kNS_SERIALIZATION_HELPER_CID },
    { NS_REDIRECTCHANNELREGISTRAR_CONTRACTID, &kNS_REDIRECTCHANNELREGISTRAR_CID },
    { NS_CACHE_STORAGE_SERVICE_CONTRACTID, &kNS_CACHE_STORAGE_SERVICE_CID },
    { NS_CACHE_STORAGE_SERVICE_CONTRACTID2, &kNS_CACHE_STORAGE_SERVICE_CID },
    { NS_NSILOADCONTEXTINFOFACTORY_CONTRACTID, &kNS_NSILOADCONTEXTINFOFACTORY_CID },
    { NS_NETWORKPREDICTOR_CONTRACTID, &kNS_NETWORKPREDICTOR_CID },
    { NS_CAPTIVEPORTAL_CONTRACTID, &kNS_CAPTIVEPORTAL_CID },
    { NS_REQUESTCONTEXTSERVICE_CONTRACTID, &kNS_REQUESTCONTEXTSERVICE_CID },
    { nullptr }
};

static const mozilla::Module kNeckoModule = {
    mozilla::Module::kVersion,
    kNeckoCIDs,
    kNeckoContracts,
    kNeckoCategories,
    nullptr,
    nsNetStartup,
    nsNetShutdown
};

NSMODULE_DEFN(necko) = &kNeckoModule;
