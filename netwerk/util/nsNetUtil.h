/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsNetUtil_h__
#define nsNetUtil_h__

#include "nsIRequest.h"
#include "nsIUnicharStreamLoader.h"
#include "nsIServiceManagerUtils.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsNetCID.h"
#include "netCore.h"
#include "prtime.h"

class nsIURI;
class nsIChannel;
class nsIIOService;
class nsIProxyInfo;
class nsIFile;
class nsILoadGroup;
class nsIInterfaceRequestor;
class nsIFileProtocolHandler;
class nsIResumableEntityID;
class nsITransport;
class nsIInputStream;
class nsIOutputStream;
class nsIRequestObserver;
class nsIStreamListener;
class nsIStreamProvider;
class nsIDownloader;
class nsIDownloadObserver;
class nsIStreamLoader;
class nsIStreamLoaderObserver;
class nsIStreamIO;
class nsIStreamIOChannel;
class nsIEventQueue;

/**
 * Helper, to simplify getting the I/O service.
 */
nsIIOService *do_GetIOService(nsresult* error = 0);

//----------------------------------------------------------------------------
// URI utils
//----------------------------------------------------------------------------

/**
 * variants on nsIIOService::newURI
 */
nsresult NS_NewURI(nsIURI **result, 
                   const nsACString &spec, 
                   const char *charset = nsnull,
                   nsIURI *baseURI = nsnull);

inline nsresult NS_NewURI(nsIURI **result, 
                          const nsAString &spec, 
                          const char *charset = nsnull,
                          nsIURI *baseURI = nsnull)
{
    return NS_NewURI(result, NS_ConvertUCS2toUTF8(spec), charset, baseURI);
}

inline nsresult NS_NewURI(nsIURI **result, 
                          const char *spec,
                          nsIURI *baseURI = nsnull)
{
    return NS_NewURI(result, nsDependentCString(spec), nsnull, baseURI);
}

/**
 * equivalent to nsIIOService::newFileURI
 */
nsresult NS_NewFileURI(nsIURI **result,
                       nsIFile *spec);

/**
 * effectively baseURI->Resolve(spec)
 */
nsresult NS_MakeAbsoluteURI(nsACString &result,
                            const nsACString &spec, 
                            nsIURI *baseURI);

nsresult NS_MakeAbsoluteURI(char **result,
                            const char *spec,
                            nsIURI *baseURI);

nsresult NS_MakeAbsoluteURI(nsAString &result,
                            const nsAString &spec,
                            nsIURI *baseURI);

//----------------------------------------------------------------------------
// channel utils
//----------------------------------------------------------------------------

/**
 * calls nsIIOService->newChannelFromURI and optionally sets load group,
 * load flags, and notification callbacks on newly created channel.
 */
nsresult NS_NewChannel(nsIChannel **result, 
                       nsIURI *uri,
                       nsILoadGroup *loadGroup = nsnull,
                       nsIInterfaceRequestor *notificationCallbacks = nsnull,
                       PRUint32 loadFlags = nsIRequest::LOAD_NORMAL);

/**
 * creates channel and calls nsIChannel::open
 *
 * use this function with CAUTION. And do not use it on the UI thread. it
 * creates a stream that blocks when you Read() from it and blocking the UI
 * thread should be avoided. if you don't want to implement a full blown
 * asynchronous consumer (via nsIStreamListener) look at nsIStreamLoader
 * or nsIUnicharStreamLoader instead.
 */
nsresult NS_OpenURI(nsIInputStream **result,
                    nsIURI *uri,
                    nsILoadGroup *loadGroup = nsnull,
                    nsIInterfaceRequestor *notificationCallbacks = nsnull,
                    PRUint32 loadFlags = nsIRequest::LOAD_NORMAL);

/**
 * creates channel and calls nsIChannel::asyncOpen
 */
nsresult NS_OpenURI(nsIStreamListener *aConsumer, 
                    nsISupports *context, 
                    nsIURI *uri,
                    nsILoadGroup *loadGroup = nsnull,
                    nsIInterfaceRequestor *notificationCallbacks = nsnull,
                    PRUint32 loadFlags = nsIRequest::LOAD_NORMAL);

/**
 * creates a channel wrapping the given nsIStreamIO object.
 */
nsresult NS_NewStreamIOChannel(nsIStreamIOChannel **result,
                               nsIURI *uri,
                               nsIStreamIO *io);

/**
 * creates a channel wrapping the given nsIInputStream object.
 */
nsresult NS_NewInputStreamChannel(nsIChannel **result,
                                  nsIURI *uri,
                                  nsIInputStream *inStr,
                                  const nsACString &contentType,
                                  const nsACString &contentCharset,
                                  PRInt32 contentLength);

/**
 * allocates a new load group and sets the group observer
 */
nsresult NS_NewLoadGroup(nsILoadGroup **result,
                         nsIRequestObserver *groupObserver);

/**
 * creates a new downloader for the specified URI.
 */
nsresult NS_NewDownloader(nsIDownloader **result,
                          nsIURI *uri,
                          nsIDownloadObserver *observer,
                          nsISupports *context = nsnull,
                          PRBool synchronous = PR_FALSE,
                          nsILoadGroup *loadGroup = nsnull,
                          nsIInterfaceRequestor *notificationCallbacks = nsnull,
                          PRUint32 loadFlags = nsIRequest::LOAD_NORMAL);

/**
 * creates a new stream loader for the specified URI. NOTE: the stream
 * loader interface requires the downloaded data to be stored in a single
 * contiguous buffer.  this can be very bad if the data is large.  be
 * sure to take this fact into consideration before using a stream loader
 * to fetch an URI.
 */
nsresult NS_NewStreamLoader(nsIStreamLoader **result,
                            nsIURI *uri,
                            nsIStreamLoaderObserver *observer,
                            nsISupports *context = nsnull,
                            nsILoadGroup *loadGroup = nsnull,
                            nsIInterfaceRequestor *notificationCallbacks = nsnull,
                            PRUint32 loadFlags = nsIRequest::LOAD_NORMAL,
                            nsIURI *referrer = nsnull);

/**
 * creates a unichar stream loader.  see nsIUnicharStreamLoader for details.
 */
nsresult NS_NewUnicharStreamLoader(nsIUnicharStreamLoader **aResult,
                                   nsIChannel *aChannel,
                                   nsIUnicharStreamLoaderObserver *aObserver,
                                   nsISupports *aContext = nsnull,
                                   PRUint32 aSegmentSize = nsIUnicharStreamLoader::DEFAULT_SEGMENT_SIZE);

//----------------------------------------------------------------------------
// stream utils
//----------------------------------------------------------------------------

/**
 * creates a data stream from either a file or a character buffer.  the
 * resulting nsIInputStream support nsIInputStream and nsISeekableStream.
 * nsIInputStream::ReadSegments is supported.
 */
nsresult NS_NewDataStream(nsIInputStream **result,
                          PRBool isFile,
                          const nsACString &data);

nsresult NS_NewRequestObserverProxy(nsIRequestObserver **aResult,
                                    nsIRequestObserver *aObserver,
                                    nsIEventQueue *aEventQ=nsnull);

nsresult NS_NewStreamListenerProxy(nsIStreamListener **aResult,
                                   nsIStreamListener *aListener,
                                   nsIEventQueue *aEventQ=nsnull,
                                   PRUint32 aBufferSegmentSize=0,
                                   PRUint32 aBufferMaxSize=0);

nsresult NS_NewStreamProviderProxy(nsIStreamProvider **aResult,
                                   nsIStreamProvider *aProvider,
                                   nsIEventQueue *aEventQ=nsnull,
                                   PRUint32 aBufferSegmentSize=0,
                                   PRUint32 aBufferMaxSize=0);

nsresult NS_NewSimpleStreamListener(nsIStreamListener **aResult,
                                    nsIOutputStream *aSink,
                                    nsIRequestObserver *aObserver=nsnull);

nsresult NS_NewSimpleStreamProvider(nsIStreamProvider **aResult,
                                    nsIInputStream *aSource,
                                    nsIRequestObserver *aObserver=nsnull);

/**
 * Depracated, prefer NS_NewStreamListenerProxy
 */
nsresult NS_NewAsyncStreamListener(nsIStreamListener **result,
                                   nsIStreamListener *receiver,
                                   nsIEventQueue *eventQueue);

/**
 * Depracated, prefer a true synchonous implementation
 */
nsresult NS_NewSyncStreamListener(nsIInputStream **aInStream, 
                                  nsIOutputStream **aOutStream,
                                  nsIStreamListener **aResult);

/**
 * Calls AsyncWrite on the specified transport, with a stream provider that
 * reads data from the specified input stream.
 */
nsresult NS_AsyncWriteFromStream(nsIRequest **aRequest,
                                 nsITransport *aTransport,
                                 nsIInputStream *aSource,
                                 PRUint32 aOffset=0,
                                 PRUint32 aCount=0,
                                 PRUint32 aFlags=0,
                                 nsIRequestObserver *aObserver=NULL,
                                 nsISupports *aContext=NULL);

/**
 * Calls AsyncRead on the specified transport, with a stream listener that
 * writes data to the specified output stream.
 */
nsresult NS_AsyncReadToStream(nsIRequest **aRequest,
                              nsITransport *aTransport,
                              nsIOutputStream *aSink,
                              PRUint32 aOffset=0,
                              PRUint32 aCount=0,
                              PRUint32 aFlags=0,
                              nsIRequestObserver *aObserver=NULL,
                              nsISupports *aContext=NULL);

//----------------------------------------------------------------------------
// proxy utils
//----------------------------------------------------------------------------

nsresult NS_NewProxyInfo(const char *type,
                         const char *host,
                         PRInt32 port,
                         nsIProxyInfo **result);

nsresult NS_ExamineForProxy(const char* scheme,
                            const char* host,
                            PRInt32 port,
                            nsIProxyInfo **proxyInfo);

//----------------------------------------------------------------------------
// file utils
//----------------------------------------------------------------------------

nsresult NS_GetFileProtocolHandler(nsIFileProtocolHandler **result);

/**
 * equivalent to nsIFileProtocolHandler::GetFileFromURLSpec
 */
nsresult NS_GetFileFromURLSpec(const nsACString &inURL,
                               nsIFile **result);

/**
 * equivalent to nsIFileProtocolHandler::GetURLSpecFromFile
 */
nsresult NS_GetURLSpecFromFile(nsIFile* aFile,
                               nsACString &aUrl);

//----------------------------------------------------------------------------
// miscellaneous utils
//----------------------------------------------------------------------------

/**
 * calls nsIIOService::allowPort
 */
nsresult NS_CheckPortSafety(PRInt32 port,
                            const char *scheme);

/**
 * uniform helper function for content-type header parsing
 */
nsresult NS_ParseContentType(const nsACString &rawContentType,
                             nsCString &contentType,
                             nsCString &contentCharset);

/**
 * allocates a new nsIResumableEntityID instance.
 */
nsresult NS_NewResumableEntityID(nsIResumableEntityID** aRes,
                                 PRUint32 size,
                                 PRTime lastModified);

#endif // !nsNetUtil_h__
