/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIOService.h"
#include "nsNetModuleMgr.h"
#include "nsFileTransportService.h"
#include "nsSocketTransportService.h"
#include "nsSocketProviderService.h"
#include "nscore.h"
#include "nsStdURLParser.h"
#include "nsAuthURLParser.h"
#include "nsNoAuthURLParser.h"
#include "nsStdURL.h"
#include "nsSimpleURI.h"
#include "nsDnsService.h"
#include "nsLoadGroup.h"
#include "nsInputStreamChannel.h"
#include "nsStreamLoader.h"
#include "nsAsyncStreamListener.h"
#include "nsSyncStreamListener.h"
#include "nsFileStreams.h"
#include "nsBufferedStreams.h"
#include "nsProtocolProxyService.h"

///////////////////////////////////////////////////////////////////////////////
// Module implementation for the net library

static nsModuleComponentInfo gNetModuleInfo[] = {
    { "I/O Service", 
      NS_IOSERVICE_CID,
      "component://netscape/network/io-service",
      nsIOService::Create },
    { "File Transport Service", 
      NS_FILETRANSPORTSERVICE_CID,
      "component://netscape/network/file-transport-service",
      nsFileTransportService::Create },
    { "Socket Transport Service", 
      NS_SOCKETTRANSPORTSERVICE_CID,
      "component://netscape/network/socket-transport-service", 
      nsSocketTransportService::Create },
    { "Socket Provider Service", 
      NS_SOCKETPROVIDERSERVICE_CID,
      "component://netscape/network/socket-provider-service",
      nsSocketProviderService::Create },
    { "DNS Service", 
      NS_DNSSERVICE_CID,
      "component://netscape/network/dns-service",
      nsDNSService::Create },
    { "Standard URL Implementation",
      NS_STANDARDURL_CID,
      "component://netscape/network/standard-url",
      nsStdURL::Create },
    { "Simple URI Implementation",
      NS_SIMPLEURI_CID,
      "component://netscape/network/simple-uri",
      nsSimpleURI::Create },
    { "External Module Manager", 
      NS_NETMODULEMGR_CID,
      "component://netscape/network/net-extern-mod",
      nsNetModuleMgr::Create },
    { "Input Stream Channel", 
      NS_INPUTSTREAMCHANNEL_CID,
      "component://netscape/network/input-stream-channel", 
      nsInputStreamChannel::Create },
    { "Unichar Stream Loader", 
      NS_STREAMLOADER_CID,
      "component://netscape/network/stream-loader",
      nsStreamLoader::Create },
    { "Async Stream Observer",
      NS_ASYNCSTREAMOBSERVER_CID,
      "component://netscape/network/async-stream-observer",
      nsAsyncStreamObserver::Create },
    { "Async Stream Listener",
      NS_ASYNCSTREAMLISTENER_CID,
      "component://netscape/network/async-stream-listener",
      nsAsyncStreamListener::Create },
    { "Sync Stream Listener", 
      NS_SYNCSTREAMLISTENER_CID,
      "component://netscape/network/sync-stream-listener",
      nsSyncStreamListener::Create },
    { "Load Group", 
      NS_LOADGROUP_CID,
      "component://netscape/network/load-group",
      nsLoadGroup::Create },
    { NS_LOCALFILEINPUTSTREAM_CLASSNAME, 
      NS_LOCALFILEINPUTSTREAM_CID,
      NS_LOCALFILEINPUTSTREAM_PROGID,
      nsFileInputStream::Create },
    { NS_LOCALFILEOUTPUTSTREAM_CLASSNAME, 
      NS_LOCALFILEOUTPUTSTREAM_CID,
      NS_LOCALFILEOUTPUTSTREAM_PROGID,
      nsFileOutputStream::Create },
    { "StdURLParser", 
      NS_STANDARDURLPARSER_CID,
      "component://netscape/network/standard-urlparser",
      nsStdURLParser::Create },
    { "AuthURLParser", 
      NS_AUTHORITYURLPARSER_CID,
      "component://netscape/network/authority-urlparser",
      nsAuthURLParser::Create },
    { "NoAuthURLParser", 
      NS_NOAUTHORITYURLPARSER_CID,
      "component://netscape/network/no-authority-urlparser",
      nsNoAuthURLParser::Create },
    { NS_BUFFEREDINPUTSTREAM_CLASSNAME, 
      NS_BUFFEREDINPUTSTREAM_CID,
      NS_BUFFEREDINPUTSTREAM_PROGID,
      nsBufferedInputStream::Create },
    { NS_BUFFEREDOUTPUTSTREAM_CLASSNAME, 
      NS_BUFFEREDOUTPUTSTREAM_CID,
      NS_BUFFEREDOUTPUTSTREAM_PROGID,
      nsBufferedOutputStream::Create },
    { "Protocol Proxy Service",
      NS_PROTOCOLPROXYSERVICE_CID,
      "component::/netscape/network/protocol-proxy-service",
      nsProtocolProxyService::Create }
};

NS_IMPL_NSGETMODULE("net", gNetModuleInfo)
