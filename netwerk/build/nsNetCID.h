/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#ifndef nsNetCID_h__
#define nsNetCID_h__


/******************************************************************************
 * netwerk/base/ classes
 */

#define NS_IOSERVICE_CLASSNAME \
    "I/O Service"
#define NS_IOSERVICE_CONTRACTID \
    "@mozilla.org/network/io-service;1"
#define NS_IOSERVICE_CID                             \
{ /* 9ac9e770-18bc-11d3-9337-00104ba0fd40 */         \
    0x9ac9e770,                                      \
    0x18bc,                                          \
    0x11d3,                                          \
    {0x93, 0x37, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

#define NS_LOADGROUP_CLASSNAME \
    "Load Group"
#define NS_LOADGROUP_CONTRACTID \
    "@mozilla.org/network/load-group;1"
#define NS_LOADGROUP_CID                             \
{ /* e1c61582-2a84-11d3-8cce-0060b0fc14a3 */         \
    0xe1c61582,                                      \
    0x2a84,                                          \
    0x11d3,                                          \
    {0x8c, 0xce, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

#define NS_SIMPLEURI_CLASSNAME \
    "Simple URI"
#define NS_SIMPLEURI_CONTRACTID \
    "@mozilla.org/network/simple-uri;1"
#define NS_SIMPLEURI_CID                              \
{ /* e0da1d70-2f7b-11d3-8cd0-0060b0fc14a3 */          \
     0xe0da1d70,                                      \
     0x2f7b,                                          \
     0x11d3,                                          \
     {0x8c, 0xd0, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

// DO NOT USE THIS INTERFACE DIRECTLY UNLESS YOU KNOW
// WHAT YOU ARE DOING! - dougt@netscape.com
#define NS_STANDARDURL_CLASSNAME \
    "Standard URL"
#define NS_STANDARDURL_CONTRACTID \
    "@mozilla.org/network/standard-url;1"
#define NS_STANDARDURL_CID                           \
{ /* de9472d0-8034-11d3-9399-00104ba0fd40 */         \
    0xde9472d0,                                      \
    0x8034,                                          \
    0x11d3,                                          \
    {0x93, 0x99, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

#define NS_REQUESTOBSERVERPROXY_CLASSNAME \
    "Request Observer Proxy"
#define NS_REQUESTOBSERVERPROXY_CONTRACTID \
    "@mozilla.org/network/request-observer-proxy;1"
#define NS_REQUESTOBSERVERPROXY_CID                  \
{ /* 51fa28c7-74c0-4b85-9c46-d03faa7b696b */         \
    0x51fa28c7,                                      \
    0x74c0,                                          \
    0x4b85,                                          \
    {0x9c, 0x46, 0xd0, 0x3f, 0xaa, 0x7b, 0x69, 0x6b} \
}

#define NS_STREAMLISTENERPROXY_CLASSNAME \
    "Stream Listener Proxy"
#define NS_STREAMLISTENERPROXY_CONTRACTID \
    "@mozilla.org/network/stream-listener-proxy;1"
#define NS_STREAMLISTENERPROXY_CID                   \
{ /* 96c48f15-aa8a-4da7-a9d5-e842bd76f015 */         \
    0x96c48f15,                                      \
    0xaa8a,                                          \
    0x4da7,                                          \
    {0xa9, 0xd5, 0xe8, 0x42, 0xbd, 0x76, 0xf0, 0x15} \
}

#define NS_STREAMPROVIDERPROXY_CLASSNAME \
    "Stream Provider Proxy"
#define NS_STREAMPROVIDERPROXY_CONTRACTID \
    "@mozilla.org/network/stream-provider-proxy;1"
#define NS_STREAMPROVIDERPROXY_CID                   \
{ /* ae964fcf-9c27-40f7-9bbd-78894bfc1f31 */         \
    0xae964fcf,                                      \
    0x9c27,                                          \
    0x40f7,                                          \
    {0x9b, 0xbd, 0x78, 0x89, 0x4b, 0xfc, 0x1f, 0x31} \
}

#define NS_SIMPLESTREAMLISTENER_CLASSNAME \
    "Simple Stream Listener"
#define NS_SIMPLESTREAMLISTENER_CONTRACTID \
    "@mozilla.org/network/simple-stream-listener;1"
#define NS_SIMPLESTREAMLISTENER_CID                  \
{ /* fb8cbf4e-4701-4ba1-b1d6-5388e041fb67 */         \
    0xfb8cbf4e,                                      \
    0x4701,                                          \
    0x4ba1,                                          \
    {0xb1, 0xd6, 0x53, 0x88, 0xe0, 0x41, 0xfb, 0x67} \
}

#define NS_SIMPLESTREAMPROVIDER_CLASSNAME \
    "Simple Stream Provider"
#define NS_SIMPLESTREAMPROVIDER_CONTRACTID \
    "@mozilla.org/network/simple-stream-provider;1"
#define NS_SIMPLESTREAMPROVIDER_CID                  \
{ /* f9f6a519-4efb-4f36-af40-2a5ec3992710 */         \
    0xf9f6a519,                                      \
    0x4efb,                                          \
    0x4f36,                                          \
    {0xaf, 0x40, 0x2a, 0x5e, 0xc3, 0x99, 0x27, 0x10} \
}

#define NS_ASYNCSTREAMLISTENER_CLASSNAME \
    "Async Stream Listener"
#define NS_ASYNCSTREAMLISTENER_CONTRACTID \
    "@mozilla.org/network/async-stream-listener;1"
#define NS_ASYNCSTREAMLISTENER_CID                   \
{ /* 60047bb2-91c0-11d3-8cd9-0060b0fc14a3 */         \
    0x60047bb2,                                      \
    0x91c0,                                          \
    0x11d3,                                          \
    {0x8c, 0xd9, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

#define NS_STREAMLISTENERTEE_CLASSNAME \
    "Stream Listener Tee"
#define NS_STREAMLISTENERTEE_CONTRACTID \
    "@mozilla.org/network/stream-listener-tee;1"
#define NS_STREAMLISTENERTEE_CID                     \
{ /* 831f8f13-7aa8-485f-b02e-77c881cc5773 */         \
    0x831f8f13,                                      \
    0x7aa8,                                          \
    0x485f,                                          \
    {0xb0, 0x2e, 0x77, 0xc8, 0x81, 0xcc, 0x57, 0x73} \
}

#define NS_STORAGETRANSPORT_CLASSNAME \
    "Storage Transport"
#define NS_STORAGETRANSPORT_CONTRACTID \
    "@mozilla.org/network/storage-transport;1"
#define NS_STORAGETRANSPORT_CID                      \
{ /* 5e955cdb-1334-4b8f-86b5-3b0f4d54b9d2 */         \
    0x5e955cdb,                                      \
    0x1334,                                          \
    0x4b8f,                                          \
    {0x86, 0xb5, 0x3b, 0x0f, 0x4d, 0x54, 0xb9, 0xd2} \
}

#define NS_STREAMIOCHANNEL_CLASSNAME \
    "Stream I/O Channel"
#define NS_STREAMIOCHANNEL_CONTRACTID \
    "@mozilla.org/network/stream-io-channel;1"
#define NS_STREAMIOCHANNEL_CID                       \
{ /* 6ddb050c-0d04-11d4-986e-00c04fa0cf4a */         \
    0x6ddb050c,                                      \
    0x0d04,                                          \
    0x11d4,                                          \
    {0x98, 0x6e, 0x00, 0xc0, 0x4f, 0xa0, 0xcf, 0x4a} \
}

#define NS_DOWNLOADER_CLASSNAME \
    "File Downloader"
#define NS_DOWNLOADER_CONTRACTID \
    "@mozilla.org/network/downloader;1"
#define NS_DOWNLOADER_CID \
{ /* 510a86bb-6019-4ed1-bb4f-965cffd23ece */         \
    0x510a86bb,                                      \
    0x6019,                                          \
    0x4ed1,                                          \
    {0xbb, 0x4f, 0x96, 0x5c, 0xff, 0xd2, 0x3e, 0xce} \
}


/******************************************************************************
 * netwerk/cache/ classes
 */

#define NS_CACHESERVICE_CLASSNAME \
    "Cache Service"
#define NS_CACHESERVICE_CONTRACTID \
    "@mozilla.org/network/cache-service;1"
#define NS_CACHESERVICE_CID                          \
{ /* 6c84aec9-29a5-4264-8fbc-bee8f922ea67 */         \
    0x6c84aec9,                                      \
    0x29a5,                                          \
    0x4264,                                          \
    {0x8f, 0xbc, 0xbe, 0xe8, 0xf9, 0x22, 0xea, 0x67} \
}


/******************************************************************************
 * netwerk/protocol/http/ classes
 */

// This protocol handler implements both HTTP and HTTPS
#define NS_HTTPPROTOCOLHANDLER_CID \
{ /* 4f47e42e-4d23-4dd3-bfda-eb29255e9ea3 */         \
    0x4f47e42e,                                      \
    0x4d23,                                          \
    0x4dd3,                                          \
    {0xbf, 0xda, 0xeb, 0x29, 0x25, 0x5e, 0x9e, 0xa3} \
}

#define NS_HTTPBASICAUTH_CID \
{ /* fca3766a-434a-4ae7-83cf-0909e18a093a */         \
    0xfca3766a,                                      \
    0x434a,                                          \
    0x4ae7,                                          \
    {0x83, 0xcf, 0x09, 0x09, 0xe1, 0x8a, 0x09, 0x3a} \
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


#endif // nsNetCID_h__
