/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNetCID_h__
#define nsNetCID_h__


/******************************************************************************
 * netwerk/base/ classes
 */

// service implementing nsIIOService and nsIIOService2.
#define NS_IOSERVICE_CONTRACTID \
    "@mozilla.org/network/io-service;1"
#define NS_IOSERVICE_CID                             \
{ /* 9ac9e770-18bc-11d3-9337-00104ba0fd40 */         \
    0x9ac9e770,                                      \
    0x18bc,                                          \
    0x11d3,                                          \
    {0x93, 0x37, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

// service implementing nsINetUtil
#define NS_NETUTIL_CONTRACTID \
    "@mozilla.org/network/util;1"

// serialization scriptable helper
#define NS_SERIALIZATION_HELPER_CONTRACTID \
  "@mozilla.org/network/serialization-helper;1"
#define NS_SERIALIZATION_HELPER_CID                  \
{ /* D6EF593D-A429-4b14-A887-D9E2F765D9ED */         \
  0xd6ef593d,                                        \
  0xa429,                                            \
  0x4b14,                                           \
  { 0xa8, 0x87, 0xd9, 0xe2, 0xf7, 0x65, 0xd9, 0xed } \
}

// service implementing nsIProtocolProxyService and nsPIProtocolProxyService.
#define NS_PROTOCOLPROXYSERVICE_CONTRACTID \
    "@mozilla.org/network/protocol-proxy-service;1"
#define NS_PROTOCOLPROXYSERVICE_CID                  \
{ /* E9B301C0-E0E4-11d3-A1A8-0050041CAF44 */         \
    0xe9b301c0,                                      \
    0xe0e4,                                          \
    0x11d3,                                          \
    {0xa1, 0xa8, 0x0, 0x50, 0x4, 0x1c, 0xaf, 0x44}   \
}

// service implementing nsIProxyAutoConfig.
#define NS_PROXYAUTOCONFIG_CONTRACTID \
    "@mozilla.org/network/proxy-auto-config;1" 
#define NS_PROXYAUTOCONFIG_CID                       \
{ /* 63ac8c66-1dd2-11b2-b070-84d00d3eaece */         \
    0x63ac8c66,                                      \
    0x1dd2,                                          \
    0x11b2,                                          \
    {0xb0, 0x70, 0x84, 0xd0, 0x0d, 0x3e, 0xae, 0xce} \
}

// component implementing nsILoadGroup.
#define NS_LOADGROUP_CONTRACTID \
    "@mozilla.org/network/load-group;1"
#define NS_LOADGROUP_CID                             \
{ /* e1c61582-2a84-11d3-8cce-0060b0fc14a3 */         \
    0xe1c61582,                                      \
    0x2a84,                                          \
    0x11d3,                                          \
    {0x8c, 0xce, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

// component implementing nsIURI, nsISerializable, and nsIClassInfo.
#define NS_SIMPLEURI_CONTRACTID \
    "@mozilla.org/network/simple-uri;1"
#define NS_SIMPLEURI_CID                              \
{ /* e0da1d70-2f7b-11d3-8cd0-0060b0fc14a3 */          \
     0xe0da1d70,                                      \
     0x2f7b,                                          \
     0x11d3,                                          \
     {0x8c, 0xd0, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

// component inheriting from the simple URI component and also
// implementing nsINestedURI.
#define NS_SIMPLENESTEDURI_CID                           \
{ /* 56388dad-287b-4240-a785-85c394012503 */             \
     0x56388dad,                                         \
     0x287b,                                             \
     0x4240,                                             \
     { 0xa7, 0x85, 0x85, 0xc3, 0x94, 0x01, 0x25, 0x03 }  \
}

// component inheriting from the nested simple URI component and also
// carrying along its base URI
#define NS_NESTEDABOUTURI_CID                            \
{ /* 2f277c00-0eaf-4ddb-b936-41326ba48aae */             \
     0x2f277c00,                                         \
     0x0eaf,                                             \
     0x4ddb,                                             \
     { 0xb9, 0x36, 0x41, 0x32, 0x6b, 0xa4, 0x8a, 0xae }  \
}

// component implementing nsIStandardURL, nsIURI, nsIURL, nsISerializable,
// and nsIClassInfo.
#define NS_STANDARDURL_CONTRACTID \
    "@mozilla.org/network/standard-url;1"
#define NS_STANDARDURL_CID                           \
{ /* de9472d0-8034-11d3-9399-00104ba0fd40 */         \
    0xde9472d0,                                      \
    0x8034,                                          \
    0x11d3,                                          \
    {0x93, 0x99, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

// service implementing nsIURLParser that assumes the URL will NOT contain an
// authority section.
#define NS_NOAUTHURLPARSER_CONTRACTID \
    "@mozilla.org/network/url-parser;1?auth=no"
#define NS_NOAUTHURLPARSER_CID                       \
{ /* 78804a84-8173-42b6-bb94-789f0816a810 */         \
    0x78804a84,                                      \
    0x8173,                                          \
    0x42b6,                                          \
    {0xbb, 0x94, 0x78, 0x9f, 0x08, 0x16, 0xa8, 0x10} \
}

// service implementing nsIURLParser that assumes the URL will contain an
// authority section.
#define NS_AUTHURLPARSER_CONTRACTID \
    "@mozilla.org/network/url-parser;1?auth=yes"
#define NS_AUTHURLPARSER_CID                         \
{ /* 275d800e-3f60-4896-adb7-d7f390ce0e42 */         \
    0x275d800e,                                      \
    0x3f60,                                          \
    0x4896,                                          \
    {0xad, 0xb7, 0xd7, 0xf3, 0x90, 0xce, 0x0e, 0x42} \
}

// service implementing nsIURLParser that does not make any assumptions about
// whether or not the URL contains an authority section.
#define NS_STDURLPARSER_CONTRACTID \
    "@mozilla.org/network/url-parser;1?auth=maybe"
#define NS_STDURLPARSER_CID                          \
{ /* ff41913b-546a-4bff-9201-dc9b2c032eba */         \
    0xff41913b,                                      \
    0x546a,                                          \
    0x4bff,                                          \
    {0x92, 0x01, 0xdc, 0x9b, 0x2c, 0x03, 0x2e, 0xba} \
}

// component implementing nsIRequestObserverProxy.
#define NS_REQUESTOBSERVERPROXY_CONTRACTID \
    "@mozilla.org/network/request-observer-proxy;1"
#define NS_REQUESTOBSERVERPROXY_CID                  \
{ /* 51fa28c7-74c0-4b85-9c46-d03faa7b696b */         \
    0x51fa28c7,                                      \
    0x74c0,                                          \
    0x4b85,                                          \
    {0x9c, 0x46, 0xd0, 0x3f, 0xaa, 0x7b, 0x69, 0x6b} \
}

// component implementing nsISimpleStreamListener.
#define NS_SIMPLESTREAMLISTENER_CONTRACTID \
    "@mozilla.org/network/simple-stream-listener;1"
#define NS_SIMPLESTREAMLISTENER_CID                  \
{ /* fb8cbf4e-4701-4ba1-b1d6-5388e041fb67 */         \
    0xfb8cbf4e,                                      \
    0x4701,                                          \
    0x4ba1,                                          \
    {0xb1, 0xd6, 0x53, 0x88, 0xe0, 0x41, 0xfb, 0x67} \
}

// component implementing nsIStreamListenerTee.
#define NS_STREAMLISTENERTEE_CONTRACTID \
    "@mozilla.org/network/stream-listener-tee;1"
#define NS_STREAMLISTENERTEE_CID                     \
{ /* 831f8f13-7aa8-485f-b02e-77c881cc5773 */         \
    0x831f8f13,                                      \
    0x7aa8,                                          \
    0x485f,                                          \
    {0xb0, 0x2e, 0x77, 0xc8, 0x81, 0xcc, 0x57, 0x73} \
}

// component implementing nsIAsyncStreamCopier.
#define NS_ASYNCSTREAMCOPIER_CONTRACTID \
    "@mozilla.org/network/async-stream-copier;1"
#define NS_ASYNCSTREAMCOPIER_CID                     \
{ /* e746a8b1-c97a-4fc5-baa4-66607521bd08 */         \
    0xe746a8b1,                                      \
    0xc97a,                                          \
    0x4fc5,                                          \
    {0xba, 0xa4, 0x66, 0x60, 0x75, 0x21, 0xbd, 0x08} \
}

// component implementing nsIInputStreamPump.
#define NS_INPUTSTREAMPUMP_CONTRACTID \
    "@mozilla.org/network/input-stream-pump;1"
#define NS_INPUTSTREAMPUMP_CID                       \
{ /* ccd0e960-7947-4635-b70e-4c661b63d675 */         \
    0xccd0e960,                                      \
    0x7947,                                          \
    0x4635,                                          \
    {0xb7, 0x0e, 0x4c, 0x66, 0x1b, 0x63, 0xd6, 0x75} \
}

// component implementing nsIInputStreamChannel.
#define NS_INPUTSTREAMCHANNEL_CONTRACTID \
    "@mozilla.org/network/input-stream-channel;1"
#define NS_INPUTSTREAMCHANNEL_CID                    \
{ /* 6ddb050c-0d04-11d4-986e-00c04fa0cf4a */         \
    0x6ddb050c,                                      \
    0x0d04,                                          \
    0x11d4,                                          \
    {0x98, 0x6e, 0x00, 0xc0, 0x4f, 0xa0, 0xcf, 0x4a} \
}

// component implementing nsIStreamLoader.
#define NS_STREAMLOADER_CONTRACTID \
    "@mozilla.org/network/stream-loader;1"
#define NS_STREAMLOADER_CID \
{ /* 5BA6D920-D4E9-11d3-A1A5-0050041CAF44 */         \
    0x5ba6d920,                                      \
    0xd4e9,                                          \
    0x11d3,                                          \
    { 0xa1, 0xa5, 0x0, 0x50, 0x4, 0x1c, 0xaf, 0x44 } \
}

// component implementing nsIStreamLoader.
#define NS_INCREMENTALSTREAMLOADER_CONTRACTID \
    "@mozilla.org/network/incremental-stream-loader;1"
#define NS_INCREMENTALSTREAMLOADER_CID \
{ /* 5d6352a3-b9c3-4fa3-87aa-b2a3c6e5a501 */         \
    0x5d6352a3,                                      \
    0xb9c3,                                          \
    0x4fa3,                                          \
    {0x87, 0xaa, 0xb2, 0xa3, 0xc6, 0xe5, 0xa5, 0x01} \
}


// component implementing nsIUnicharStreamLoader.
#define NS_UNICHARSTREAMLOADER_CONTRACTID \
    "@mozilla.org/network/unichar-stream-loader;1"
#define NS_UNICHARSTREAMLOADER_CID \
{ /* 9445791f-fa4c-4669-b174-df5032bb67b3 */           \
    0x9445791f,                                        \
    0xfa4c,                                            \
    0x4669,                                            \
    { 0xb1, 0x74, 0xdf, 0x50, 0x32, 0xbb, 0x67, 0xb3 } \
}

// component implementing nsIDownloader.
#define NS_DOWNLOADER_CONTRACTID \
    "@mozilla.org/network/downloader;1"
#define NS_DOWNLOADER_CID \
{ /* 510a86bb-6019-4ed1-bb4f-965cffd23ece */         \
    0x510a86bb,                                      \
    0x6019,                                          \
    0x4ed1,                                          \
    {0xbb, 0x4f, 0x96, 0x5c, 0xff, 0xd2, 0x3e, 0xce} \
}

// component implementing nsIBackgroundFileSaver and
// nsIOutputStream.
#define NS_BACKGROUNDFILESAVEROUTPUTSTREAM_CONTRACTID \
    "@mozilla.org/network/background-file-saver;1?mode=outputstream"
#define NS_BACKGROUNDFILESAVEROUTPUTSTREAM_CID \
{ /* 62147d1e-ef6a-40e8-aaf8-d039f5caaa81 */         \
    0x62147d1e,                                      \
    0xef6a,                                          \
    0x40e8,                                          \
    {0xaa, 0xf8, 0xd0, 0x39, 0xf5, 0xca, 0xaa, 0x81} \
}

// component implementing nsIBackgroundFileSaver and
// nsIStreamListener.
#define NS_BACKGROUNDFILESAVERSTREAMLISTENER_CONTRACTID \
    "@mozilla.org/network/background-file-saver;1?mode=streamlistener"
#define NS_BACKGROUNDFILESAVERSTREAMLISTENER_CID \
{ /* 208de7fc-a781-4031-bbae-cc0de539f61a */         \
    0x208de7fc,                                      \
    0xa781,                                          \
    0x4031,                                          \
    {0xbb, 0xae, 0xcc, 0x0d, 0xe5, 0x39, 0xf6, 0x1a} \
}

// component implementing nsISyncStreamListener.
#define NS_SYNCSTREAMLISTENER_CONTRACTID \
    "@mozilla.org/network/sync-stream-listener;1"
#define NS_SYNCSTREAMLISTENER_CID \
{ /* 439400d3-6f23-43db-8b06-8aafe1869bd8 */         \
    0x439400d3,                                      \
    0x6f23,                                          \
    0x43db,                                          \
    {0x8b, 0x06, 0x8a, 0xaf, 0xe1, 0x86, 0x9b, 0xd8} \
}

// component implementing nsIIncrementalDownload.
#define NS_INCREMENTALDOWNLOAD_CONTRACTID \
    "@mozilla.org/network/incremental-download;1"

// component implementing nsISystemProxySettings.
#define NS_SYSTEMPROXYSETTINGS_CONTRACTID \
    "@mozilla.org/system-proxy-settings;1"

// service implementing nsIStreamTransportService
#define NS_STREAMTRANSPORTSERVICE_CONTRACTID \
    "@mozilla.org/network/stream-transport-service;1"
#define NS_STREAMTRANSPORTSERVICE_CID \
{ /* 0885d4f8-f7b8-4cda-902e-94ba38bc256e */         \
    0x0885d4f8,                                      \
    0xf7b8,                                          \
    0x4cda,                                          \
    {0x90, 0x2e, 0x94, 0xba, 0x38, 0xbc, 0x25, 0x6e} \
}

// service implementing nsISocketTransportService
#define NS_SOCKETTRANSPORTSERVICE_CONTRACTID \
    "@mozilla.org/network/socket-transport-service;1"
#define NS_SOCKETTRANSPORTSERVICE_CID                \
{ /* ad56b25f-e6bb-4db3-9f7b-5b7db33fd2b1 */         \
    0xad56b25f,                                      \
    0xe6bb,                                          \
    0x4db3,                                          \
    {0x9f, 0x7b, 0x5b, 0x7d, 0xb3, 0x3f, 0xd2, 0xb1} \
}


// component implementing nsIServerSocket
#define NS_SERVERSOCKET_CONTRACTID \
    "@mozilla.org/network/server-socket;1"
#define NS_SERVERSOCKET_CID                          \
{ /* 2ec62893-3b35-48fa-ab1d-5e68a9f45f08 */         \
    0x2ec62893,                                      \
    0x3b35,                                          \
    0x48fa,                                          \
    {0xab, 0x1d, 0x5e, 0x68, 0xa9, 0xf4, 0x5f, 0x08} \
}

// component implementing nsITLSServerSocket
#define NS_TLSSERVERSOCKET_CONTRACTID \
    "@mozilla.org/network/tls-server-socket;1"
#define NS_TLSSERVERSOCKET_CID                       \
{ /* 1813cbb4-c98e-4622-8c7d-839167f3f272 */         \
    0x1813cbb4,                                      \
    0xc98e,                                          \
    0x4622,                                          \
    {0x8c, 0x7d, 0x83, 0x91, 0x67, 0xf3, 0xf2, 0x72} \
}

// component implementing nsIUDPSocket
#define NS_UDPSOCKET_CONTRACTID \
    "@mozilla.org/network/udp-socket;1"
#define NS_UDPSOCKET_CID                             \
{ /* c9f74572-7b8e-4fec-bb4a-03c0d3021bd6 */         \
    0xc9f74572,                                      \
    0x7b8e,                                          \
    0x4fec,                                          \
    {0xbb, 0x4a, 0x03, 0xc0, 0xd3, 0x02, 0x1b, 0xd6} \
}

#define NS_LOCALFILEINPUTSTREAM_CONTRACTID \
    "@mozilla.org/network/file-input-stream;1"
#define NS_LOCALFILEINPUTSTREAM_CID                  \
{ /* be9a53ae-c7e9-11d3-8cda-0060b0fc14a3 */         \
    0xbe9a53ae,                                      \
    0xc7e9,                                          \
    0x11d3,                                          \
    {0x8c, 0xda, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

#define NS_LOCALFILEOUTPUTSTREAM_CONTRACTID \
    "@mozilla.org/network/file-output-stream;1"
#define NS_LOCALFILEOUTPUTSTREAM_CID                 \
{ /* c272fee0-c7e9-11d3-8cda-0060b0fc14a3 */         \
    0xc272fee0,                                      \
    0xc7e9,                                          \
    0x11d3,                                          \
    {0x8c, 0xda, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

#define NS_PARTIALLOCALFILEINPUTSTREAM_CONTRACTID \
    "@mozilla.org/network/partial-file-input-stream;1"
#define NS_PARTIALLOCALFILEINPUTSTREAM_CID           \
{ /* 8738afd6-162a-418d-a99b-75b3a6b10a56 */         \
    0x8738afd6,                                      \
    0x162a,                                          \
    0x418d,                                          \
    {0xa9, 0x9b, 0x75, 0xb3, 0xa6, 0xb1, 0x0a, 0x56} \
}

#define NS_BUFFEREDINPUTSTREAM_CONTRACTID \
    "@mozilla.org/network/buffered-input-stream;1"
#define NS_BUFFEREDINPUTSTREAM_CID                   \
{ /* 9226888e-da08-11d3-8cda-0060b0fc14a3 */         \
    0x9226888e,                                      \
    0xda08,                                          \
    0x11d3,                                          \
    {0x8c, 0xda, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

#define NS_BUFFEREDOUTPUTSTREAM_CONTRACTID \
    "@mozilla.org/network/buffered-output-stream;1"
#define NS_BUFFEREDOUTPUTSTREAM_CID                  \
{ /* 9868b4ce-da08-11d3-8cda-0060b0fc14a3 */         \
    0x9868b4ce,                                      \
    0xda08,                                          \
    0x11d3,                                          \
    {0x8c, 0xda, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

// components implementing nsISafeOutputStream
#define NS_ATOMICLOCALFILEOUTPUTSTREAM_CONTRACTID \
    "@mozilla.org/network/atomic-file-output-stream;1"
#define NS_ATOMICLOCALFILEOUTPUTSTREAM_CID           \
{ /* 6EAE857E-4BA9-11E3-9B39-B4036188709B */         \
    0x6EAE857E,                                      \
    0x4BA9,                                          \
    0x11E3,                                          \
    {0x9b, 0x39, 0xb4, 0x03, 0x61, 0x88, 0x70, 0x9b} \
}

#define NS_SAFELOCALFILEOUTPUTSTREAM_CONTRACTID \
    "@mozilla.org/network/safe-file-output-stream;1"
#define NS_SAFELOCALFILEOUTPUTSTREAM_CID             \
{ /* a181af0d-68b8-4308-94db-d4f859058215 */         \
    0xa181af0d,                                      \
    0x68b8,                                          \
    0x4308,                                          \
    {0x94, 0xdb, 0xd4, 0xf8, 0x59, 0x05, 0x82, 0x15} \
}

// component implementing nsIFileStream
#define NS_LOCALFILESTREAM_CONTRACTID \
    "@mozilla.org/network/file-stream;1"
#define NS_LOCALFILESTREAM_CID                       \
{ /* f8a69bd7-176c-4a60-9a05-b6d92f8f229a */         \
    0xf8a69bd7,                                      \
    0x176c,                                          \
    0x4a60,                                          \
    {0x9a, 0x05, 0xb6, 0xd9, 0x2f, 0x8f, 0x22, 0x9a} \
}

/**
 * Contract ID for a service implementing nsIURIClassifier that identifies
 * phishing and malware sites.
 */
#define NS_URICLASSIFIERSERVICE_CONTRACTID \
    "@mozilla.org/uriclassifierservice"

// Redirect channel registrar used for redirect to various protocols
#define NS_REDIRECTCHANNELREGISTRAR_CONTRACTID \
    "@mozilla.org/redirectchannelregistrar;1"
#define NS_REDIRECTCHANNELREGISTRAR_CID \
{ /* {b69043a6-8929-4d60-8d17-a27e44a8393e} */ \
    0xb69043a6, \
    0x8929, \
    0x4d60, \
    { 0x8d, 0x17, 0xa2, 0x7e, 0x44, 0xa8, 0x39, 0x3e } \
}

// service implementing nsINetworkPredictor
#define NS_NETWORKPREDICTOR_CONTRACTID \
    "@mozilla.org/network/predictor;1"
#define NS_NETWORKPREDICTOR_CID \
{ /* {969adfdf-7221-4419-aecf-05f8faf00c9b} */ \
    0x969adfdf, \
    0x7221, \
    0x4419, \
    { 0xae, 0xcf, 0x05, 0xf8, 0xfa, 0xf0, 0x0c, 0x9b } \
}

// captive portal service implementing nsICaptivePortalService
#define NS_CAPTIVEPORTAL_CONTRACTID \
    "@mozilla.org/network/captive-portal-service;1"
#define NS_CAPTIVEPORTAL_CID \
{ /* bdbe0555-fc3d-4f7b-9205-c309ceb2d641 */ \
    0xbdbe0555, \
    0xfc3d, \
    0x4f7b, \
  { 0x92, 0x05, 0xc3, 0x09, 0xce, 0xb2, 0xd6, 0x41 } \
}

// service implementing nsIRequestContextService
#define NS_REQUESTCONTEXTSERVICE_CONTRACTID \
    "@mozilla.org/network/request-context-service;1"
#define NS_REQUESTCONTEXTSERVICE_CID \
{ /* d5499fa7-7ba8-49ff-9e30-1858b99ace69 */ \
    0xd5499fa7, \
    0x7ba8, \
    0x49ff, \
    {0x93, 0x30, 0x18, 0x58, 0xb9, 0x9a, 0xce, 0x69} \
}

/******************************************************************************
 * netwerk/cache/ classes
 */

// service implementing nsICacheService.
#define NS_CACHESERVICE_CONTRACTID \
    "@mozilla.org/network/cache-service;1"
#define NS_CACHESERVICE_CID                          \
{ /* 6c84aec9-29a5-4264-8fbc-bee8f922ea67 */         \
    0x6c84aec9,                                      \
    0x29a5,                                          \
    0x4264,                                          \
    {0x8f, 0xbc, 0xbe, 0xe8, 0xf9, 0x22, 0xea, 0x67} \
}

// service implementing nsIApplicationCacheService.
#define NS_APPLICATIONCACHESERVICE_CONTRACTID \
    "@mozilla.org/network/application-cache-service;1"
#define NS_APPLICATIONCACHESERVICE_CID               \
{ /* 02bf7a2a-39d8-4a23-a50c-2cbb085ab7a5 */         \
    0x02bf7a2a,                                      \
    0x39d8,                                          \
    0x4a23,                                          \
    {0xa5, 0x0c, 0x2c, 0xbb, 0x08, 0x5a, 0xb7, 0xa5} \
}

#define NS_APPLICATIONCACHENAMESPACE_CONTRACTID \
    "@mozilla.org/network/application-cache-namespace;1"
#define NS_APPLICATIONCACHENAMESPACE_CID             \
{ /* b00ed78a-04e2-4f74-8e1c-d1af79dfd12f */         \
    0xb00ed78a,                                      \
    0x04e2,                                          \
    0x4f74,                                          \
   {0x8e, 0x1c, 0xd1, 0xaf, 0x79, 0xdf, 0xd1, 0x2f}  \
}

#define NS_APPLICATIONCACHE_CONTRACTID \
    "@mozilla.org/network/application-cache;1"

#define NS_APPLICATIONCACHE_CID             \
{ /* 463440c5-baad-4f3c-9e50-0b107abe7183 */ \
    0x463440c5, \
    0xbaad, \
    0x4f3c, \
   {0x9e, 0x50, 0xb, 0x10, 0x7a, 0xbe, 0x71, 0x83 } \
}

/******************************************************************************
 * netwerk/protocol/http/ classes
 */

#define NS_HTTPPROTOCOLHANDLER_CID \
{ /* 4f47e42e-4d23-4dd3-bfda-eb29255e9ea3 */         \
    0x4f47e42e,                                      \
    0x4d23,                                          \
    0x4dd3,                                          \
    {0xbf, 0xda, 0xeb, 0x29, 0x25, 0x5e, 0x9e, 0xa3} \
}

#define NS_HTTPSPROTOCOLHANDLER_CID \
{ /* dccbe7e4-7750-466b-a557-5ea36c8ff24e */         \
    0xdccbe7e4,                                      \
    0x7750,                                          \
    0x466b,                                          \
    {0xa5, 0x57, 0x5e, 0xa3, 0x6c, 0x8f, 0xf2, 0x4e} \
}

#define NS_HTTPBASICAUTH_CID \
{ /* fca3766a-434a-4ae7-83cf-0909e18a093a */         \
    0xfca3766a,                                      \
    0x434a,                                          \
    0x4ae7,                                          \
    {0x83, 0xcf, 0x09, 0x09, 0xe1, 0x8a, 0x09, 0x3a} \
}

#define NS_HTTPDIGESTAUTH_CID \
{ /* 17491ba4-1dd2-11b2-aae3-de6b92dab620 */         \
    0x17491ba4,                                      \
    0x1dd2,                                          \
    0x11b2,                                          \
    {0xaa, 0xe3, 0xde, 0x6b, 0x92, 0xda, 0xb6, 0x20} \
}

#define NS_HTTPNTLMAUTH_CID \
{ /* bbef8185-c628-4cc1-b53e-e61e74c2451a */         \
    0xbbef8185,                                      \
    0xc628,                                          \
    0x4cc1,                                          \
    {0xb5, 0x3e, 0xe6, 0x1e, 0x74, 0xc2, 0x45, 0x1a} \
}

#define NS_HTTPAUTHMANAGER_CONTRACTID \
    "@mozilla.org/network/http-auth-manager;1"
#define NS_HTTPAUTHMANAGER_CID \
{ /* 36b63ef3-e0fa-4c49-9fd4-e065e85568f4 */         \
    0x36b63ef3,                                      \
    0xe0fa,                                          \
    0x4c49,                                          \
    {0x9f, 0xd4, 0xe0, 0x65, 0xe8, 0x55, 0x68, 0xf4} \
}

#define NS_HTTPCHANNELAUTHPROVIDER_CONTRACTID \
    "@mozilla.org/network/http-channel-auth-provider;1"
#define NS_HTTPCHANNELAUTHPROVIDER_CID \
{ /* 02f5a8d8-4ef3-48b1-b527-8a643056abbd */         \
    0x02f5a8d8,                                      \
    0x4ef3,                                          \
    0x48b1,                                          \
    {0xb5, 0x27, 0x8a, 0x64, 0x30, 0x56, 0xab, 0xbd} \
}

// component implementing nsIHttpPushListener.
#define NS_HTTPPUSHLISTENER_CONTRACTID \
    "@mozilla.org/network/push-listener;1"
#define NS_HTTPPUSHLISTENER_CID                      \
{                                                    \
    0x73cf4430,                                      \
    0x5877,                                          \
    0x4c6b,                                          \
    {0xb8, 0x78, 0x3e, 0xde, 0x5b, 0xc8, 0xef, 0xf1} \
}


#define NS_HTTPACTIVITYDISTRIBUTOR_CONTRACTID \
    "@mozilla.org/network/http-activity-distributor;1"
#define NS_HTTPACTIVITYDISTRIBUTOR_CID \
{ /* 15629ada-a41c-4a09-961f-6553cd60b1a2 */         \
    0x15629ada,                                      \
    0xa41c,                                          \
    0x4a09,                                          \
    {0x96, 0x1f, 0x65, 0x53, 0xcd, 0x60, 0xb1, 0xa2} \
}

#define NS_THROTTLEQUEUE_CONTRACTID \
    "@mozilla.org/network/throttlequeue;1"
#define NS_THROTTLEQUEUE_CID                            \
{ /* 4c39159c-cd90-4dd3-97a7-06af5e6d84c4 */            \
    0x4c39159c,                                         \
    0xcd90,                                             \
    0x4dd3,                                             \
    {0x97, 0xa7, 0x06, 0xaf, 0x5e, 0x6d, 0x84, 0xc4}    \
}

/******************************************************************************
 * netwerk/protocol/ftp/ classes
 */

#define NS_FTPPROTOCOLHANDLER_CID \
{ /* 25029490-F132-11d2-9588-00805F369F95 */         \
    0x25029490,                                      \
    0xf132,                                          \
    0x11d2,                                          \
    {0x95, 0x88, 0x0, 0x80, 0x5f, 0x36, 0x9f, 0x95}  \
}

/******************************************************************************
 * netwerk/protocol/res/ classes
 */

#define NS_RESPROTOCOLHANDLER_CID                    \
{ /* e64f152a-9f07-11d3-8cda-0060b0fc14a3 */         \
    0xe64f152a,                                      \
    0x9f07,                                          \
    0x11d3,                                          \
    {0x8c, 0xda, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

#define NS_EXTENSIONPROTOCOLHANDLER_CID              \
{ /* aea16cd0-f020-4138-b068-0716c4a15b5a */         \
    0xaea16cd0,                                      \
    0xf020,                                          \
    0x4138,                                          \
    {0xb0, 0x68, 0x07, 0x16, 0xc4, 0xa1, 0x5b, 0x5a} \
}

#define NS_SUBSTITUTINGURL_CID                       \
{ 0xdea9657c,                                        \
  0x18cf,                                            \
  0x4984,                                            \
  { 0xbd, 0xe9, 0xcc, 0xef, 0x5d, 0x8a, 0xb4, 0x73 } \
}

/******************************************************************************
 * netwerk/protocol/file/ classes
 */

#define NS_FILEPROTOCOLHANDLER_CID                   \
{ /* fbc81170-1f69-11d3-9344-00104ba0fd40 */         \
    0xfbc81170,                                      \
    0x1f69,                                          \
    0x11d3,                                          \
    {0x93, 0x44, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

/******************************************************************************
 * netwerk/protocol/data/ classes
 */

#define NS_DATAPROTOCOLHANDLER_CID                   \
{ /* {B6ED3030-6183-11d3-A178-0050041CAF44} */       \
    0xb6ed3030,                                      \
    0x6183,                                          \
    0x11d3,                                          \
    {0xa1, 0x78, 0x00, 0x50, 0x04, 0x1c, 0xaf, 0x44} \
}

/******************************************************************************
 * netwerk/protocol/device classes
 */
#define NS_DEVICEPROTOCOLHANDLER_CID                 \
{ /* 6b0ffe9e-d114-486b-aeb7-da62e7273ed5 */         \
    0x60ffe9e,                                       \
    0xd114,                                          \
    0x486b,                                          \
    {0xae, 0xb7, 0xda, 0x62, 0xe7, 0x27, 0x3e, 0xd5} \
}

/******************************************************************************
 * netwerk/protocol/viewsource/ classes
 */

// service implementing nsIProtocolHandler
#define NS_VIEWSOURCEHANDLER_CID                     \
{ /* {0x9c7ec5d1-23f9-11d5-aea8-8fcc0793e97f} */     \
    0x9c7ec5d1,                                      \
    0x23f9,                                          \
    0x11d5,                                          \
    {0xae, 0xa8, 0x8f, 0xcc, 0x07, 0x93, 0xe9, 0x7f} \
}

/******************************************************************************
 * netwerk/protocol/wyciwyg/ classes
 */

#define NS_WYCIWYGPROTOCOLHANDLER_CID                \
{ /* {0xe7509b46-2eB2-410a-9d7c-c3ce73284d01} */     \
  0xe7509b46,                                        \
  0x2eb2,                                            \
  0x410a,                                            \
  {0x9d, 0x7c, 0xc3, 0xce, 0x73, 0x28, 0x4d, 0x01}   \
}

/******************************************************************************
 * netwerk/protocol/websocket/ classes
 */

#define NS_WEBSOCKETPROTOCOLHANDLER_CID              \
{ /* {dc01db59-a513-4c90-824b-085cce06c0aa} */       \
  0xdc01db59,                                        \
  0xa513,                                            \
  0x4c90,                                            \
  {0x82, 0x4b, 0x08, 0x5c, 0xce, 0x06, 0xc0, 0xaa}   \
}

#define NS_WEBSOCKETSSLPROTOCOLHANDLER_CID           \
{ /* {dc01dbbb-a5bb-4cbb-82bb-085cce06c0bb} */       \
  0xdc01dbbb,                                        \
  0xa5bb,                                            \
  0x4cbb,                                            \
  {0x82, 0xbb, 0x08, 0x5c, 0xce, 0x06, 0xc0, 0xbb}   \
}

/******************************************************************************
 * netwerk/protocol/rtsp / classes
 */

#define NS_RTSPPROTOCOLHANDLER_CID                   \
{ /* {5bb4b980-7b10-11e2-b92a-0800200c9a66} */       \
  0x5bb4b980,                                        \
  0x7b10,                                            \
  0x11e2,                                            \
  {0xb9, 0x2a, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66}   \
}

/******************************************************************************
 * netwerk/protocol/about/ classes
 */

#define NS_ABOUTPROTOCOLHANDLER_CID                  \
{ /* 9e3b6c90-2f75-11d3-8cd0-0060b0fc14a3 */         \
    0x9e3b6c90,                                      \
    0x2f75,                                          \
    0x11d3,                                          \
    {0x8c, 0xd0, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

#define NS_SAFEABOUTPROTOCOLHANDLER_CID              \
{ /* 1423e739-782c-4081-b5d8-fe6fba68c0ef */         \
    0x1423e739,                                      \
    0x782c,                                          \
    0x4081,                                          \
    {0xb5, 0xd8, 0xfe, 0x6f, 0xba, 0x68, 0xc0, 0xef} \
}

/******************************************************************************
 * netwerk/dns/ classes
 */

#define NS_DNSSERVICE_CONTRACTID \
    "@mozilla.org/network/dns-service;1"
#define NS_DNSSERVICE_CID \
{ /* b0ff4572-dae4-4bef-a092-83c1b88f6be9 */         \
    0xb0ff4572,                                      \
    0xdae4,                                          \
    0x4bef,                                          \
    {0xa0, 0x92, 0x83, 0xc1, 0xb8, 0x8f, 0x6b, 0xe9} \
}

/* ContractID of the XPCOM package that implements nsIIDNService */
#define NS_IDNSERVICE_CONTRACTID \
    "@mozilla.org/network/idn-service;1"
#define NS_IDNSERVICE_CID \
{ /* 62b778a6-bce3-456b-8c31-2865fbb68c91 */         \
    0x62b778a6,                                      \
    0xbce3,                                          \
    0x456b,                                          \
    {0x8c, 0x31, 0x28, 0x65, 0xfb, 0xb6, 0x8c, 0x91} \
}

#define NS_EFFECTIVETLDSERVICE_CONTRACTID \
    "@mozilla.org/network/effective-tld-service;1"
#define NS_EFFECTIVETLDSERVICE_CID \
{ /* cb9abbae-66b6-4609-8594-5c4ff300888e */         \
    0xcb9abbae,                                      \
    0x66b6,                                          \
    0x4609,                                          \
    {0x85, 0x94, 0x5c, 0x4f, 0xf3, 0x00, 0x88, 0x8e} \
}


/******************************************************************************
 * netwerk/mime classes
 */

// {1F4DBCF7-245C-4c8c-943D-8A1DA0495E8A} 
#define NS_MIMEHEADERPARAM_CID                         \
{   0x1f4dbcf7,                                        \
    0x245c,                                            \
    0x4c8c,                                            \
    { 0x94, 0x3d, 0x8a, 0x1d, 0xa0, 0x49, 0x5e, 0x8a } \
}

#define NS_MIMEHEADERPARAM_CONTRACTID "@mozilla.org/network/mime-hdrparam;1"


/******************************************************************************
 * netwerk/socket classes
 */

#define NS_SOCKETPROVIDERSERVICE_CONTRACTID \
    "@mozilla.org/network/socket-provider-service;1"
#define NS_SOCKETPROVIDERSERVICE_CID                   \
{ /* ed394ba0-5472-11d3-bbc8-0000861d1237 */           \
    0xed394ba0,                                        \
    0x5472,                                            \
    0x11d3,                                            \
    { 0xbb, 0xc8, 0x00, 0x00, 0x86, 0x1d, 0x12, 0x37 } \
}

#define NS_SOCKSSOCKETPROVIDER_CID                     \
{ /* 8dbe7246-1dd2-11b2-9b8f-b9a849e4403a */           \
    0x8dbe7246,                                        \
    0x1dd2,                                            \
    0x11b2,                                            \
    { 0x9b, 0x8f, 0xb9, 0xa8, 0x49, 0xe4, 0x40, 0x3a } \
}

#define NS_SOCKS4SOCKETPROVIDER_CID                    \
{ /* F7C9F5F4-4451-41c3-A28A-5BA2447FBACE */           \
    0xf7c9f5f4,                                        \
    0x4451,                                            \
    0x41c3,                                            \
    { 0xa2, 0x8a, 0x5b, 0xa2, 0x44, 0x7f, 0xba, 0xce } \
}

#define NS_UDPSOCKETPROVIDER_CID                       \
{ /* 320706D2-2E81-42c6-89C3-8D83B17D3FB4 */           \
    0x320706d2,                                        \
    0x2e81,                                            \
    0x42c6,                                            \
    { 0x89, 0xc3, 0x8d, 0x83, 0xb1, 0x7d, 0x3f, 0xb4 } \
}

#define NS_SSLSOCKETPROVIDER_CONTRACTID \
    NS_NETWORK_SOCKET_CONTRACTID_PREFIX "ssl"

/* This code produces a normal socket which can be used to initiate the
 * STARTTLS protocol by calling its nsISSLSocketControl->StartTLS()
 */
#define NS_STARTTLSSOCKETPROVIDER_CONTRACTID \
    NS_NETWORK_SOCKET_CONTRACTID_PREFIX "starttls"


#define NS_DASHBOARD_CONTRACTID \
    "@mozilla.org/network/dashboard;1"
#define NS_DASHBOARD_CID                               \
{   /*c79eb3c6-091a-45a6-8544-5a8d1ab79537 */          \
    0xc79eb3c6,                                        \
    0x091a,                                            \
    0x45a6,                                            \
    { 0x85, 0x44, 0x5a, 0x8d, 0x1a, 0xb7, 0x95, 0x37 } \
}

#define NS_PACKAGEDAPPSERVICE_CONTRACTID \
    "@mozilla.org/network/packaged-app-service;1"
#define NS_PACKAGEDAPPSERVICE_CID                      \
{   /* adef6762-41b9-4470-a06a-dc29cf8de381 */         \
    0xadef6762,                                        \
    0x41b9,                                            \
    0x4470,                                            \
  { 0xa0, 0x6a, 0xdc, 0x29, 0xcf, 0x8d, 0xe3, 0x81 }   \
}

#define NS_PACKAGEDAPPVERIFIER_CONTRACTID \
    "@mozilla.org/network/packaged-app-verifier;1"
#define NS_PACKAGEDAPPVERIFIER_CID                      \
{   /* 07242d20-4cae-11e5-b970-0800200c9a66 */         \
    0x07242d20,                                        \
    0x4cae,                                            \
    0x11e5,                                            \
  { 0xb9, 0x70, 0x08, 0x00, 0x20, 0x0c, 0x96, 0x66 }   \
}

/******************************************************************************
 * netwerk/cookie classes
 */

// service implementing nsICookieManager and nsICookieManager2.
#define NS_COOKIEMANAGER_CONTRACTID \
    "@mozilla.org/cookiemanager;1"
#define NS_COOKIEMANAGER_CID                           \
{ /* aaab6710-0f2c-11d5-a53b-0010a401eb10 */           \
    0xaaab6710,                                        \
    0x0f2c,                                            \
    0x11d5,                                            \
    { 0xa5, 0x3b, 0x00, 0x10, 0xa4, 0x01, 0xeb, 0x10 } \
}

// service implementing nsICookieService.
#define NS_COOKIESERVICE_CONTRACTID \
    "@mozilla.org/cookieService;1"
#define NS_COOKIESERVICE_CID                           \
{ /* c375fa80-150f-11d6-a618-0010a401eb10 */           \
    0xc375fa80,                                        \
    0x150f,                                            \
    0x11d6,                                            \
    { 0xa6, 0x18, 0x00, 0x10, 0xa4, 0x01, 0xeb, 0x10 } \
}

/******************************************************************************
 * netwerk/wifi classes
 */
#ifdef NECKO_WIFI
#define NS_WIFI_MONITOR_CONTRACTID "@mozilla.org/wifi/monitor;1"

#define NS_WIFI_MONITOR_COMPONENT_CID                  \
{  0x3FF8FB9F,                                         \
   0xEE63,                                             \
   0x48DF,                                             \
   { 0x89, 0xF0, 0xDA, 0xCE, 0x02, 0x42, 0xFD, 0x82 }  \
}
#endif

/******************************************************************************
 * netwerk/streamconv classes
 */

// service implementing nsIStreamConverterService
#define NS_STREAMCONVERTERSERVICE_CONTRACTID \
    "@mozilla.org/streamConverters;1"
#define NS_STREAMCONVERTERSERVICE_CID                \
{ /* 892FFEB0-3F80-11d3-A16C-0050041CAF44 */         \
    0x892ffeb0,                                      \
    0x3f80,                                          \
    0x11d3,                                          \
    {0xa1, 0x6c, 0x00, 0x50, 0x04, 0x1c, 0xaf, 0x44} \
}

/**
 * General-purpose content sniffer component. Use with CreateInstance.
 *
 * Implements nsIContentSniffer
 */
#define NS_GENERIC_CONTENT_SNIFFER \
    "@mozilla.org/network/content-sniffer;1"

/**
 * Detector that can act as either an nsIStreamConverter or an
 * nsIContentSniffer to decide whether text/plain data is "really" text/plain
 * or APPLICATION_GUESS_FROM_EXT.  Use with CreateInstance.
 */
#define NS_BINARYDETECTOR_CONTRACTID \
    "@mozilla.org/network/binary-detector;1"

/******************************************************************************
 * netwerk/system classes
 */

// service implementing nsINetworkLinkService
#define NS_NETWORK_LINK_SERVICE_CID    \
  { 0x75a500a2,                                        \
    0x0030,                                            \
    0x40f7,                                            \
    { 0x86, 0xf8, 0x63, 0xf2, 0x25, 0xb9, 0x40, 0xae } \
  }

/******************************************************************************
 * Contracts that can be implemented by necko users.
 */

/**
 * This contract ID will be gotten as a service implementing nsINetworkLinkService
 * and monitored by IOService for automatic online/offline management.
 *
 * Must implement nsINetworkLinkService
 */
#define NS_NETWORK_LINK_SERVICE_CONTRACTID \
  "@mozilla.org/network/network-link-service;1"

/**
 * This contract ID is used when Necko needs to wrap an nsIAuthPrompt as
 * nsIAuthPrompt2. Implementing it is required for backwards compatibility
 * with Versions before 1.9.
 *
 * Must implement nsIAuthPromptAdapterFactory
 */
#define NS_AUTHPROMPT_ADAPTER_FACTORY_CONTRACTID \
  "@mozilla.org/network/authprompt-adapter-factory;1"

/**
 * Must implement nsICryptoHash.
 */
#define NS_CRYPTO_HASH_CONTRACTID "@mozilla.org/security/hash;1"

/**
 * Must implement nsICryptoHMAC.
 */
#define NS_CRYPTO_HMAC_CONTRACTID "@mozilla.org/security/hmac;1"

/******************************************************************************
 * Categories
 */
/**
 * Services registered in this category will get notified via
 * nsIChannelEventSink about all redirects that happen and have the opportunity
 * to veto them. The value of the category entries is interpreted as the
 * contract ID of the service.
 */
#define NS_CHANNEL_EVENT_SINK_CATEGORY "net-channel-event-sinks"

/**
 * Services in this category will get told about each load that happens and get
 * the opportunity to override the detected MIME type via nsIContentSniffer.
 * Services should not set the MIME type on the channel directly, but return the
 * new type. If getMIMETypeFromContent throws an exception, the type will remain
 * unchanged.
 *
 * Note that only channels with the LOAD_CALL_CONTENT_SNIFFERS flag will call
 * content sniffers. Also note that there can be security implications about
 * changing the MIME type -- proxies filtering responses based on their MIME
 * type might consider certain types to be safe, which these sniffers can
 * override.
 *
 * Not all channels may implement content sniffing. See also
 * nsIChannel::LOAD_CALL_CONTENT_SNIFFERS.
 */
#define NS_CONTENT_SNIFFER_CATEGORY "net-content-sniffers"

/**
 * Services in this category can sniff content that is not necessarily loaded
 * from the network, and they won't be told about each load.
 */
#define NS_DATA_SNIFFER_CATEGORY "content-sniffing-services"

/**
 * Must implement nsINSSErrorsService.
 */
#define NS_NSS_ERRORS_SERVICE_CONTRACTID "@mozilla.org/nss_errors_service;1"

/**
 * LoadContextInfo factory
 */
#define NS_NSILOADCONTEXTINFOFACTORY_CONTRACTID \
    "@mozilla.org/load-context-info-factory;1"
#define NS_NSILOADCONTEXTINFOFACTORY_CID             \
{ /* 62d4b190-3642-4450-b019-d1c1fba56025 */         \
    0x62d4b190,                                      \
    0x3642,                                          \
    0x4450,                                          \
    {0xb0, 0x19, 0xd1, 0xc1, 0xfb, 0xa5, 0x60, 0x25} \
}

#endif // nsNetCID_h__
