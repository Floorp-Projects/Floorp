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
#include "nsICategoryManager.h"
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
#include "nsDownloader.h"
#include "nsAsyncStreamListener.h"
//#include "nsSyncStreamListener.h"
#include "nsFileStreams.h"
#include "nsBufferedStreams.h"
#include "nsProtocolProxyService.h"
#include "nsSOCKSSocketProvider.h"

#include "nsNetCID.h"

///////////////////////////////////////////////////////////////////////////////

#include "nsStreamConverterService.h"

///////////////////////////////////////////////////////////////////////////////

#if defined(OLD_CACHE)
#include "nsINetDataCache.h"
#include "nsINetDataCacheManager.h"
#include "nsMemCacheCID.h"
#include "nsMemCache.h"
#include "nsNetDiskCache.h"
#include "nsNetDiskCacheCID.h"
#include "nsCacheManager.h"

// Factory method to create a new nsMemCache instance.  Used
// by nsNetDataCacheModule
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMemCache, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNetDiskCache, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCacheManager, Init)
#endif

///////////////////////////////////////////////////////////////////////////////

#include "nsMIMEService.h"
#include "nsXMLMIMEDataSource.h"
#include "nsMIMEInfoImpl.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMIMEInfoImpl)

///////////////////////////////////////////////////////////////////////////////

#include "nsRequestObserverProxy.h"
#include "nsStreamListenerProxy.h"
#include "nsStreamProviderProxy.h"
#include "nsSimpleStreamListener.h"
#include "nsSimpleStreamProvider.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsRequestObserverProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStreamListenerProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStreamProviderProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSimpleStreamListener)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSimpleStreamProvider)

///////////////////////////////////////////////////////////////////////////////

#include "nsStreamListenerTee.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStreamListenerTee)

///////////////////////////////////////////////////////////////////////////////

#include "nsStorageTransport.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsStorageTransport)

///////////////////////////////////////////////////////////////////////////////

#include "nsIHTTPProtocolHandler.h"
#include "nsHTTPHandler.h"
#include "nsHTTPSHandler.h"
#include "nsBasicAuth.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsHTTPHandler, Init);
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsHTTPSHandler, Init);

#define NS_HTTPS_HANDLER_FACTORY_CID { 0xd2771480, 0xcac4, 0x11d3, { 0x8c, 0xaf, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsBasicAuth);
#define NS_BASICAUTH_CID { 0xd5c9bc48, 0x1dd1, 0x11b2, { 0x9a, 0x0b, 0xf7, 0x3f, 0x59, 0x53, 0x19, 0xae } }
#define NS_BASICAUTH_CONTRACTID "@mozilla.org/network/http-basic-auth;1"

/* XXX this should all be data-driven, via NS_IMPL_GETMODULE_WITH_CATEGORIES */
static NS_METHOD
RegisterBasicAuth(nsIComponentManager *aCompMgr, nsIFile *aPath,
                  const char *registryLocation, const char *componentType,
                  const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString previous;
    return catman->AddCategoryEntry("http-auth", "basic", NS_BASICAUTH_CONTRACTID,
                                    PR_TRUE, PR_TRUE, getter_Copies(previous));
}

static NS_METHOD
UnregisterBasicAuth(nsIComponentManager *aCompMgr, nsIFile *aPath,
                    const char *registryLocation,
                    const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString basicAuth;
    rv = catman->GetCategoryEntry("http-auth", "basic",
                                  getter_Copies(basicAuth));
    if (NS_FAILED(rv)) return rv;
    
    // only unregister if we're the current Basic-auth handler
    if (!strcmp(basicAuth, NS_BASICAUTH_CONTRACTID))
        return catman->DeleteCategoryEntry("http-auth", "basic", PR_TRUE,
                                           getter_Copies(basicAuth));
    return NS_OK;
}
///////////////////////////////////////////////////////////////////////////////

#include "nsFileChannel.h"
#include "nsFileProtocolHandler.h"
#include "nsDataHandler.h"
#include "nsJARProtocolHandler.h"
#include "nsResProtocolHandler.h"

#include "nsAboutProtocolHandler.h"
#include "nsAboutBlank.h"
#include "nsAboutBloat.h"
#include "nsAboutCache.h"
#include "nsAboutRedirector.h"
#include "nsKeywordProtocolHandler.h"

#ifdef MOZ_NEW_CACHE
#include "nsAboutCacheEntry.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAboutCacheEntry)
#endif

///////////////////////////////////////////////////////////////////////////////

#include "nsFTPDirListingConv.h"
#include "nsGopherDirListingConv.h"
#include "nsMultiMixedConv.h"
#include "nsHTTPChunkConv.h"
#include "nsHTTPCompressConv.h"
#include "mozTXTToHTMLConv.h"
#include "nsUnknownDecoder.h"
#include "nsTXTToHTMLConv.h"
#include "nsIndexedToHTML.h"

nsresult NS_NewFTPDirListingConv(nsFTPDirListingConv** result);
nsresult NS_NewGopherDirListingConv(nsGopherDirListingConv** result);
nsresult NS_NewMultiMixedConv (nsMultiMixedConv** result);
nsresult MOZ_NewTXTToHTMLConv (mozTXTToHTMLConv** result);
nsresult NS_NewHTTPChunkConv  (nsHTTPChunkConv ** result);
nsresult NS_NewHTTPCompressConv  (nsHTTPCompressConv ** result);
nsresult NS_NewNSTXTToHTMLConv(nsTXTToHTMLConv** result);
nsresult NS_NewStreamConv(nsStreamConverterService **aStreamConv);

#define FTP_UNIX_TO_INDEX            "?from=text/ftp-dir-unix&to=application/http-index-format"
#define FTP_NT_TO_INDEX              "?from=text/ftp-dir-nt&to=application/http-index-format"
#define GOPHER_TO_INDEX              "?from=text/gopher-dir&to=application/http-index-format"
#define INDEX_TO_HTML                "?from=application/http-index-format&to=text/html"
#define MULTI_MIXED_X                "?from=multipart/x-mixed-replace&to=*/*"
#define MULTI_MIXED                  "?from=multipart/mixed&to=*/*"
#define UNKNOWN_CONTENT              "?from=application/x-unknown-content-type&to=*/*"
#define CHUNKED_TO_UNCHUNKED         "?from=chunked&to=unchunked"
#define UNCHUNKED_TO_CHUNKED         "?from=unchunked&to=chunked"
#define GZIP_TO_UNCOMPRESSED         "?from=gzip&to=uncompressed"
#define XGZIP_TO_UNCOMPRESSED        "?from=x-gzip&to=uncompressed"
#define COMPRESS_TO_UNCOMPRESSED     "?from=compress&to=uncompressed"
#define XCOMPRESS_TO_UNCOMPRESSED    "?from=x-compress&to=uncompressed"
#define DEFLATE_TO_UNCOMPRESSED      "?from=deflate&to=uncompressed"
#define PLAIN_TO_HTML                "?from=text/plain&to=text/html"

static PRUint32 g_StreamConverterCount = 15;

static char *g_StreamConverterArray[] = {
        FTP_UNIX_TO_INDEX,
        FTP_NT_TO_INDEX,
        GOPHER_TO_INDEX,
        INDEX_TO_HTML,
        MULTI_MIXED_X,
        MULTI_MIXED,
        UNKNOWN_CONTENT,
        CHUNKED_TO_UNCHUNKED,
        UNCHUNKED_TO_CHUNKED,
        GZIP_TO_UNCOMPRESSED,
        XGZIP_TO_UNCOMPRESSED,
        COMPRESS_TO_UNCOMPRESSED,
        XCOMPRESS_TO_UNCOMPRESSED,
        DEFLATE_TO_UNCOMPRESSED,
        PLAIN_TO_HTML
    };

// each stream converter must add its from/to key to the category manager
// in RegisterStreamConverters(). This provides a string representation
// of each registered converter, rooted in the NS_ISTREAMCONVERTER_KEY
// category. These keys are then (when the stream converter service
// needs to chain converters together) enumerated and parsed to build
// a graph of converters for potential chaining.
static NS_METHOD
RegisterStreamConverters(nsIComponentManager *aCompMgr, nsIFile *aPath,
                         const char *registryLocation,
                         const char *componentType,
                         const nsModuleComponentInfo *info) {
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catmgr =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString previous;

    PRUint32 count = 0;
    while (count < g_StreamConverterCount) {
        (void) catmgr->AddCategoryEntry(NS_ISTREAMCONVERTER_KEY, g_StreamConverterArray[count],
                                      "", PR_TRUE, PR_TRUE, getter_Copies(previous));
        if (NS_FAILED(rv)) NS_ASSERTION(0, "adding a cat entry failed");
        count++;
    }
    
    return rv;
}

// same as RegisterStreamConverters except the reverse.
static NS_METHOD
UnregisterStreamConverters(nsIComponentManager *aCompMgr, nsIFile *aPath,
                           const char *registryLocation,
                           const nsModuleComponentInfo *info) {
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catmgr =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString entry;

    PRUint32 count = 0;
    while (count < g_StreamConverterCount) {
        rv = catmgr->DeleteCategoryEntry(NS_ISTREAMCONVERTER_KEY, g_StreamConverterArray[count],
                                         PR_TRUE, getter_Copies(entry));
        if (NS_FAILED(rv)) return rv;
        NS_ASSERTION(entry, "deleting an entry that doesn't exist");
        count++;
    }
    return rv;
}

static NS_IMETHODIMP                 
CreateNewStreamConvServiceFactory(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    nsStreamConverterService* inst = nsnull;
    nsresult rv = NS_NewStreamConv(&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}

static NS_IMETHODIMP                 
CreateNewFTPDirListingConv(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    nsFTPDirListingConv* inst = nsnull;
    nsresult rv = NS_NewFTPDirListingConv(&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}

static NS_IMETHODIMP                 
CreateNewGopherDirListingConv(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {
        return NS_ERROR_INVALID_POINTER;
    }
    if (aOuter) {
        *aResult = nsnull;
        return NS_ERROR_NO_AGGREGATION;
    }
    nsGopherDirListingConv* inst = nsnull;
    nsresult rv = NS_NewGopherDirListingConv(&inst);
    if (NS_FAILED(rv)) {
        *aResult = nsnull;
        return rv;
    }
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {
        *aResult = nsnull;
    }
    NS_RELEASE(inst);             /* get rid of extra refcnt */
    return rv;
}

static NS_IMETHODIMP                 
CreateNewMultiMixedConvFactory(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    nsMultiMixedConv* inst = nsnull;
    nsresult rv = NS_NewMultiMixedConv(&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}

static NS_IMETHODIMP                 
CreateNewTXTToHTMLConvFactory(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    mozTXTToHTMLConv* inst = nsnull;
    nsresult rv = MOZ_NewTXTToHTMLConv(&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}

static NS_IMETHODIMP                 
CreateNewHTTPChunkConvFactory (nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    nsHTTPChunkConv* inst = nsnull;
    nsresult rv = NS_NewHTTPChunkConv (&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}

static NS_IMETHODIMP                 
CreateNewHTTPCompressConvFactory (nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    nsHTTPCompressConv* inst = nsnull;
    nsresult rv = NS_NewHTTPCompressConv (&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}

static NS_IMETHODIMP
CreateNewUnknownDecoderFactory(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;

  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nsnull;

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

static NS_IMETHODIMP
CreateNewNSTXTToHTMLConvFactory(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;

  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nsnull;

  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsTXTToHTMLConv *inst;
  
  inst = new nsTXTToHTMLConv();
  if (!inst) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(inst);
  rv = inst->Init();
  if (NS_FAILED(rv)) {
    delete inst;
    return rv;
  }
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// Module implementation for the net library

// Necko module shutdown hook
static void PR_CALLBACK nsNeckoShutdown(nsIModule *neckoModule)
{
    // Release the url parser that the stdurl is holding.
    nsStdURL::ShutdownGlobalObjects();
}

static nsModuleComponentInfo gNetModuleInfo[] = {
    { NS_IOSERVICE_CLASSNAME,
      NS_IOSERVICE_CID,
      NS_IOSERVICE_CONTRACTID,
      nsIOService::Create },
    { "File Transport Service", 
      NS_FILETRANSPORTSERVICE_CID,
      "@mozilla.org/network/file-transport-service;1",
      nsFileTransportService::Create },
    { "Socket Transport Service", 
      NS_SOCKETTRANSPORTSERVICE_CID,
      "@mozilla.org/network/socket-transport-service;1", 
      nsSocketTransportService::Create },
    { "Socket Provider Service", 
      NS_SOCKETPROVIDERSERVICE_CID,
      "@mozilla.org/network/socket-provider-service;1",
      nsSocketProviderService::Create },
    { "DNS Service", 
      NS_DNSSERVICE_CID,
      "@mozilla.org/network/dns-service;1",
      nsDNSService::Create },
    { NS_STANDARDURL_CLASSNAME,
      NS_STANDARDURL_CID,
      NS_STANDARDURL_CONTRACTID,
      nsStdURL::Create },
    { NS_SIMPLEURI_CLASSNAME,
      NS_SIMPLEURI_CID,
      NS_SIMPLEURI_CONTRACTID,
      nsSimpleURI::Create },
    { "External Module Manager", 
      NS_NETMODULEMGR_CID,
      "@mozilla.org/network/net-extern-mod;1",
      nsNetModuleMgr::Create },
    { NS_FILEIO_CLASSNAME,
      NS_FILEIO_CID,
      NS_FILEIO_CONTRACTID,
      nsFileIO::Create },
    { NS_INPUTSTREAMIO_CLASSNAME,
      NS_INPUTSTREAMIO_CID,
      NS_INPUTSTREAMIO_CONTRACTID,
      nsInputStreamIO::Create },
    { NS_STREAMIOCHANNEL_CLASSNAME,
      NS_STREAMIOCHANNEL_CID,
      NS_STREAMIOCHANNEL_CONTRACTID,
      nsStreamIOChannel::Create },
    { "Unichar Stream Loader", 
      NS_STREAMLOADER_CID,
      "@mozilla.org/network/stream-loader;1",
      nsStreamLoader::Create },
    { NS_DOWNLOADER_CLASSNAME,
      NS_DOWNLOADER_CID,
      NS_DOWNLOADER_CONTRACTID,
      nsDownloader::Create },
    { NS_REQUESTOBSERVERPROXY_CLASSNAME,
      NS_REQUESTOBSERVERPROXY_CID,
      NS_REQUESTOBSERVERPROXY_CONTRACTID,
      nsRequestObserverProxyConstructor },
    { NS_STREAMLISTENERPROXY_CLASSNAME,
      NS_STREAMLISTENERPROXY_CID,
      NS_STREAMLISTENERPROXY_CONTRACTID,
      nsStreamListenerProxyConstructor },
    { NS_STREAMPROVIDERPROXY_CLASSNAME,
      NS_STREAMPROVIDERPROXY_CID,
      NS_STREAMPROVIDERPROXY_CONTRACTID,
      nsStreamProviderProxyConstructor },
    { NS_SIMPLESTREAMLISTENER_CLASSNAME,
      NS_SIMPLESTREAMLISTENER_CID,
      NS_SIMPLESTREAMLISTENER_CONTRACTID,
      nsSimpleStreamListenerConstructor },
    { NS_SIMPLESTREAMPROVIDER_CLASSNAME,
      NS_SIMPLESTREAMPROVIDER_CID,
      NS_SIMPLESTREAMPROVIDER_CONTRACTID,
      nsSimpleStreamProviderConstructor },
    { NS_ASYNCSTREAMLISTENER_CLASSNAME,
      NS_ASYNCSTREAMLISTENER_CID,
      NS_ASYNCSTREAMLISTENER_CONTRACTID,
      nsAsyncStreamListener::Create },
    { NS_STREAMLISTENERTEE_CLASSNAME,
      NS_STREAMLISTENERTEE_CID,
      NS_STREAMLISTENERTEE_CONTRACTID,
      nsStreamListenerTeeConstructor },
    { NS_STORAGETRANSPORT_CLASSNAME,
      NS_STORAGETRANSPORT_CID,
      NS_STORAGETRANSPORT_CONTRACTID,
      nsStorageTransportConstructor },
    { NS_LOADGROUP_CLASSNAME,
      NS_LOADGROUP_CID,
      NS_LOADGROUP_CONTRACTID,
      nsLoadGroup::Create },
    { NS_LOCALFILEINPUTSTREAM_CLASSNAME, 
      NS_LOCALFILEINPUTSTREAM_CID,
      NS_LOCALFILEINPUTSTREAM_CONTRACTID,
      nsFileInputStream::Create },
    { NS_LOCALFILEOUTPUTSTREAM_CLASSNAME, 
      NS_LOCALFILEOUTPUTSTREAM_CID,
      NS_LOCALFILEOUTPUTSTREAM_CONTRACTID,
      nsFileOutputStream::Create },
    { "StdURLParser", 
      NS_STANDARDURLPARSER_CID,
      "@mozilla.org/network/standard-urlparser;1",
      nsStdURLParser::Create },
    { "AuthURLParser", 
      NS_AUTHORITYURLPARSER_CID,
      "@mozilla.org/network/authority-urlparser;1",
      nsAuthURLParser::Create },
    { "NoAuthURLParser", 
      NS_NOAUTHORITYURLPARSER_CID,
      "@mozilla.org/network/no-authority-urlparser;1",
      nsNoAuthURLParser::Create },
    { NS_BUFFEREDINPUTSTREAM_CLASSNAME, 
      NS_BUFFEREDINPUTSTREAM_CID,
      NS_BUFFEREDINPUTSTREAM_CONTRACTID,
      nsBufferedInputStream::Create },
    { NS_BUFFEREDOUTPUTSTREAM_CLASSNAME, 
      NS_BUFFEREDOUTPUTSTREAM_CID,
      NS_BUFFEREDOUTPUTSTREAM_CONTRACTID,
      nsBufferedOutputStream::Create },
    { "Protocol Proxy Service",
      NS_PROTOCOLPROXYSERVICE_CID,
      "@mozilla.org/network/protocol-proxy-service;1",
      nsProtocolProxyService::Create },

    // from netwerk/streamconv:

    // this converter is "always" part of a build.
    // HACK-ALERT, register *all* converters
    // in this converters component manager 
    // registration. just piggy backing here until
    // you can add registration functions to
    // the generic module macro.
    { "Stream Converter Service", 
      NS_STREAMCONVERTERSERVICE_CID,
      "@mozilla.org/streamConverters;1", 
      CreateNewStreamConvServiceFactory,
      RegisterStreamConverters,   // registers *all* converters
      UnregisterStreamConverters  // unregisters *all* converters
    },

    // from netwerk/streamconv/converters:
    { "FTPDirListingConverter", 
      NS_FTPDIRLISTINGCONVERTER_CID, 
      NS_ISTREAMCONVERTER_KEY FTP_UNIX_TO_INDEX, 
      CreateNewFTPDirListingConv
    },

    { "FTPDirListingConverter", 
      NS_FTPDIRLISTINGCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY FTP_NT_TO_INDEX, 
      CreateNewFTPDirListingConv
    },

    { "GopherDirListingConverter",
      NS_GOPHERDIRLISTINGCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY GOPHER_TO_INDEX,
      CreateNewGopherDirListingConv
    },    

    { "Indexed to HTML Converter", 
      NS_NSINDEXEDTOHTMLCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY INDEX_TO_HTML, 
      nsIndexedToHTML::Create
    },
    
    { "MultiMixedConverter", 
      NS_MULTIMIXEDCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY MULTI_MIXED_X, 
      CreateNewMultiMixedConvFactory
    },

    { "MultiMixedConverter2",
      NS_MULTIMIXEDCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY MULTI_MIXED,
      CreateNewMultiMixedConvFactory
    },
    { "Unknown Content-Type Decoder",
      NS_UNKNOWNDECODER_CID,
      NS_ISTREAMCONVERTER_KEY UNKNOWN_CONTENT,
      CreateNewUnknownDecoderFactory
    },

    { "HttpChunkConverter", 
      NS_HTTPCHUNKCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY CHUNKED_TO_UNCHUNKED,
      CreateNewHTTPChunkConvFactory
    },

    { "HttpChunkConverter", 
      NS_HTTPCHUNKCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY UNCHUNKED_TO_CHUNKED,
      CreateNewHTTPChunkConvFactory
    },

    { "HttpCompressConverter", 
      NS_HTTPCOMPRESSCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY GZIP_TO_UNCOMPRESSED,
      CreateNewHTTPCompressConvFactory
    },

    { "HttpCompressConverter", 
      NS_HTTPCOMPRESSCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY XGZIP_TO_UNCOMPRESSED,
      CreateNewHTTPCompressConvFactory
    },
    { "HttpCompressConverter", 
      NS_HTTPCOMPRESSCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY COMPRESS_TO_UNCOMPRESSED,
      CreateNewHTTPCompressConvFactory
    },
    { "HttpCompressConverter", 
      NS_HTTPCOMPRESSCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY XCOMPRESS_TO_UNCOMPRESSED,
      CreateNewHTTPCompressConvFactory
    },
    { "HttpCompressConverter", 
      NS_HTTPCOMPRESSCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY DEFLATE_TO_UNCOMPRESSED,
      CreateNewHTTPCompressConvFactory
    },
    { "NSTXTToHTMLConverter",
      NS_NSTXTTOHTMLCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY PLAIN_TO_HTML,
      CreateNewNSTXTToHTMLConvFactory
	},
	// This is not a real stream converter, it's just
	// registering it's cid factory here.
	{ "HACK-TXTToHTMLConverter", 
  	  MOZITXTTOHTMLCONV_CID,
	  NS_ISTREAMCONVERTER_KEY, 
	  CreateNewTXTToHTMLConvFactory
    },
#if defined(OLD_CACHE)
    // from netwerk/cache:
    { "Memory Cache", NS_MEM_CACHE_FACTORY_CID, NS_NETWORK_MEMORY_CACHE_CONTRACTID, nsMemCacheConstructor },
    { "File Cache",   NS_NETDISKCACHE_CID,      NS_NETWORK_FILE_CACHE_CONTRACTID,   nsNetDiskCacheConstructor },
    { "Cache Manager",NS_CACHE_MANAGER_CID,     NS_NETWORK_CACHE_MANAGER_CONTRACTID,nsCacheManagerConstructor },
#endif
    // from netwerk/mime:
    { "The MIME mapping service", 
      NS_MIMESERVICE_CID,
      "@mozilla.org/mimeold;1",
      nsMIMEService::Create
    },
    { "xml mime datasource", 
      NS_XMLMIMEDATASOURCE_CID,
      NS_XMLMIMEDATASOURCE_CONTRACTID,
      nsXMLMIMEDataSource::Create
    },
    { "xml mime INFO", 
      NS_MIMEINFO_CID,
      NS_MIMEINFO_CONTRACTID,
      nsMIMEInfoImplConstructor
    },

    // from netwerk/protocol/file:
    { "File Protocol Handler", 
      NS_FILEPROTOCOLHANDLER_CID,  
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "file", 
      nsFileProtocolHandler::Create
    },
    { NS_LOCALFILECHANNEL_CLASSNAME,
      NS_LOCALFILECHANNEL_CID,  
      NS_LOCALFILECHANNEL_CONTRACTID, 
      nsFileChannel::Create
    },
    
    // from netwerk/protocol/http:
    { "HTTP Handler",
      NS_IHTTPHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http",
      nsHTTPHandlerConstructor },
    { "HTTPS Handler",
      NS_HTTPS_HANDLER_FACTORY_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "https",
      nsHTTPSHandlerConstructor },
    { "Basic Auth Encoder",
      NS_BASICAUTH_CID,
      NS_BASICAUTH_CONTRACTID,
      nsBasicAuthConstructor,
      RegisterBasicAuth,
      UnregisterBasicAuth
    },

    // from netwerk/protocol/data:
    { "Data Protocol Handler", 
      NS_DATAHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "data", 
      nsDataHandler::Create},

    // from netwerk/protocol/jar:
    { "JAR Protocol Handler", 
       NS_JARPROTOCOLHANDLER_CID,
       NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "jar", 
       nsJARProtocolHandler::Create
    },

    // from netwerk/protocol/res:
    { "The Resource Protocol Handler", 
      NS_RESPROTOCOLHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "resource",
      nsResProtocolHandler::Create
    },

    // from netwerk/protocol/about:
    { "About Protocol Handler", 
      NS_ABOUTPROTOCOLHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "about", 
      nsAboutProtocolHandler::Create
    },
    { "about:blank", 
      NS_ABOUT_BLANK_MODULE_CID,
      NS_ABOUT_MODULE_CONTRACTID_PREFIX "blank", 
      nsAboutBlank::Create
    },
    { "about:bloat", 
      NS_ABOUT_BLOAT_MODULE_CID,
      NS_ABOUT_MODULE_CONTRACTID_PREFIX "bloat", 
      nsAboutBloat::Create
    },
    { "about:credits",
      NS_ABOUT_REDIRECTOR_MODULE_CID,
      NS_ABOUT_MODULE_CONTRACTID_PREFIX "credits",
      nsAboutRedirector::Create
    },
    { "about:plugins",
      NS_ABOUT_REDIRECTOR_MODULE_CID,
      NS_ABOUT_MODULE_CONTRACTID_PREFIX "plugins",
      nsAboutRedirector::Create
    },
    { "about:mozilla",
      NS_ABOUT_REDIRECTOR_MODULE_CID,
      NS_ABOUT_MODULE_CONTRACTID_PREFIX "mozilla",
      nsAboutRedirector::Create
    },
    { "about:cache", 
      NS_ABOUT_CACHE_MODULE_CID,
      NS_ABOUT_MODULE_CONTRACTID_PREFIX "cache", 
      nsAboutCache::Create
    },
#ifdef MOZ_NEW_CACHE
    { "about:cache-entry",
      NS_ABOUT_CACHE_ENTRY_MODULE_CID,
      NS_ABOUT_MODULE_CONTRACTID_PREFIX "cache-entry",
      nsAboutCacheEntryConstructor
    },
#endif
    // from netwerk/protocol/keyword:
    { "The Keyword Protocol Handler", 
      NS_KEYWORDPROTOCOLHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "keyword",
      nsKeywordProtocolHandler::Create
    },

    {  NS_ISOCKSSOCKETPROVIDER_CLASSNAME,
       NS_SOCKSSOCKETPROVIDER_CID,
       NS_ISOCKSSOCKETPROVIDER_CONTRACTID,
       nsSOCKSSocketProvider::Create
    }

};

NS_IMPL_NSGETMODULE_WITH_DTOR("necko core and primary protocols", gNetModuleInfo,
                              nsNeckoShutdown)
