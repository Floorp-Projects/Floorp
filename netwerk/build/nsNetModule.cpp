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
#include "nsSOCKS4SocketProvider.h"

#include "nsNetCID.h"

///////////////////////////////////////////////////////////////////////////////

#include "nsStreamConverterService.h"
#if defined(XP_MAC)
#include "nsAppleFileDecoder.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAppleFileDecoder)
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

#include "nsHttpHandler.h"
#include "nsHttpBasicAuth.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHttpBasicAuth)

///////////////////////////////////////////////////////////////////////////////

#include "nsResProtocolHandler.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsResProtocolHandler, Init)

///////////////////////////////////////////////////////////////////////////////


static NS_METHOD
RegisterBuiltInURLParsers(nsIComponentManager *aCompMgr, 
                          nsIFile *aPath,
                          const char *registryLocation, 
                          const char *componentType,
                          const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString previous;

    catman->AddCategoryEntry(NS_IURLPARSER_KEY, 
                             "file", 
                             NS_NOAUTHORITYURLPARSER_CONTRACT_ID,
                             PR_TRUE, 
                             PR_TRUE, 
                             getter_Copies(previous));

    catman->AddCategoryEntry(NS_IURLPARSER_KEY, 
                             "ftp", 
                             NS_AUTHORITYURLPARSER_CONTRACT_ID,
                             PR_TRUE, 
                             PR_TRUE, 
                             getter_Copies(previous));

    catman->AddCategoryEntry(NS_IURLPARSER_KEY, 
                             "http", 
                             NS_AUTHORITYURLPARSER_CONTRACT_ID,
                             PR_TRUE, 
                             PR_TRUE, 
                             getter_Copies(previous));
    return NS_OK;
}

static NS_METHOD
UnregisterBuiltInURLParsers(nsIComponentManager *aCompMgr, 
                            nsIFile *aPath,
                            const char *registryLocation,
                            const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    catman->DeleteCategoryEntry(NS_IURLPARSER_KEY, "file", PR_TRUE);
    catman->DeleteCategoryEntry(NS_IURLPARSER_KEY, "ftp", PR_TRUE);
    catman->DeleteCategoryEntry(NS_IURLPARSER_KEY, "http", PR_TRUE);

    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////

#include "nsFileChannel.h"
#include "nsFileProtocolHandler.h"
#include "nsDataHandler.h"
#include "nsJARProtocolHandler.h"

#include "nsAboutProtocolHandler.h"
#include "nsAboutBlank.h"
#include "nsAboutBloat.h"
#include "nsAboutCache.h"
#include "nsAboutConfig.h"
#include "nsAboutRedirector.h"
#include "nsKeywordProtocolHandler.h"

#include "nsAboutCacheEntry.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAboutCacheEntry)

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
#ifndef XP_MAC
#include "nsBinHexDecoder.h"
#endif

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
#define MULTI_BYTERANGES             "?from=multipart/byteranges&to=*/*"
#define UNKNOWN_CONTENT              "?from=application/x-unknown-content-type&to=*/*"
#define CHUNKED_TO_UNCHUNKED         "?from=chunked&to=unchunked"
#define UNCHUNKED_TO_CHUNKED         "?from=unchunked&to=chunked"
#define GZIP_TO_UNCOMPRESSED         "?from=gzip&to=uncompressed"
#define XGZIP_TO_UNCOMPRESSED        "?from=x-gzip&to=uncompressed"
#define COMPRESS_TO_UNCOMPRESSED     "?from=compress&to=uncompressed"
#define XCOMPRESS_TO_UNCOMPRESSED    "?from=x-compress&to=uncompressed"
#define DEFLATE_TO_UNCOMPRESSED      "?from=deflate&to=uncompressed"
#define PLAIN_TO_HTML                "?from=text/plain&to=text/html"

#ifndef XP_MAC
#define BINHEX_TO_WILD               "?from=application/mac-binhex40&to=*/*"
#endif

#ifndef XP_MAC
static PRUint32 g_StreamConverterCount = 16;
#else
static PRUint32 g_StreamConverterCount = 15;
#endif

static char *g_StreamConverterArray[] = {
        FTP_UNIX_TO_INDEX,
        FTP_NT_TO_INDEX,
        GOPHER_TO_INDEX,
        INDEX_TO_HTML,
        MULTI_MIXED_X,
        MULTI_MIXED,
        MULTI_BYTERANGES,
        UNKNOWN_CONTENT,
        CHUNKED_TO_UNCHUNKED,
        UNCHUNKED_TO_CHUNKED,
        GZIP_TO_UNCOMPRESSED,
        XGZIP_TO_UNCOMPRESSED,
        COMPRESS_TO_UNCOMPRESSED,
        XCOMPRESS_TO_UNCOMPRESSED,
        DEFLATE_TO_UNCOMPRESSED,
#ifndef XP_MAC
        BINHEX_TO_WILD,
#endif
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


    PRUint32 count = 0;
    while (count < g_StreamConverterCount) {
        rv = catmgr->DeleteCategoryEntry(NS_ISTREAMCONVERTER_KEY, 
                                         g_StreamConverterArray[count], 
                                         PR_TRUE);
        if (NS_FAILED(rv)) return rv;
        count++;
    }
    return rv;
}
#ifndef XP_MAC
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBinHexDecoder);
#endif

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
    
    // The register functions for the build in 
    // parsers just need to be called once.
    { "StdURLParser", 
      NS_STANDARDURLPARSER_CID,
      NS_STANDARDURLPARSER_CONTRACT_ID,
      nsStdURLParser::Create,
      RegisterBuiltInURLParsers,  
      UnregisterBuiltInURLParsers 
    },
    { "AuthURLParser", 
      NS_AUTHORITYURLPARSER_CID,
      NS_AUTHORITYURLPARSER_CONTRACT_ID,
      nsAuthURLParser::Create },
    { "NoAuthURLParser", 
      NS_NOAUTHORITYURLPARSER_CID,
      NS_NOAUTHORITYURLPARSER_CONTRACT_ID,
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
    
#if defined(XP_MAC)
    { NS_APPLEFILEDECODER_CLASSNAME, 
      NS_APPLEFILEDECODER_CID,
      NS_IAPPLEFILEDECODER_CONTRACTID, 
      nsAppleFileDecoderConstructor
    },
#endif

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

    { "MultiMixedByteRange", 
      NS_MULTIMIXEDCONVERTER_CID,
      NS_ISTREAMCONVERTER_KEY MULTI_BYTERANGES, 
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
#ifndef XP_MAC
    { "nsBinHexConverter", NS_BINHEXDECODER_CID,
      NS_ISTREAMCONVERTER_KEY BINHEX_TO_WILD,
      nsBinHexDecoderConstructor
    },
#endif
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

    { "HTTP Handler",
      NS_HTTPPROTOCOLHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http",
      nsHttpHandler::Create },

    { "HTTPS Handler",
      NS_HTTPPROTOCOLHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "https",
      nsHttpHandler::Create },

    { "HTTP Basic Auth Encoder",
      NS_HTTPBASICAUTH_CID,
      NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX "basic",
      nsHttpBasicAuthConstructor },

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
    { "Resource Protocol Handler", 
      NS_RESPROTOCOLHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "resource",
      nsResProtocolHandlerConstructor
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
    { "about:config",
      NS_ABOUT_CONFIG_MODULE_CID,
      NS_ABOUT_MODULE_CONTRACTID_PREFIX "config",
      nsAboutConfig::Create
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
    { "about:cache-entry",
      NS_ABOUT_CACHE_ENTRY_MODULE_CID,
      NS_ABOUT_MODULE_CONTRACTID_PREFIX "cache-entry",
      nsAboutCacheEntryConstructor
    },
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
    },

    {  NS_ISOCKS4SOCKETPROVIDER_CLASSNAME,
       NS_SOCKS4SOCKETPROVIDER_CID,
       NS_ISOCKS4SOCKETPROVIDER_CONTRACTID,
       nsSOCKS4SocketProvider::Create
    }


};

NS_IMPL_NSGETMODULE_WITH_DTOR(necko_core_and_primary_protocols, gNetModuleInfo,
                              nsNeckoShutdown)
