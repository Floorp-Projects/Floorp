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


#include "nsString.h"
#include "nsReadableUtils.h"
#include "netCore.h"
#include "nsIURI.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStreamListener.h"
#include "nsIStreamProvider.h"
#include "nsIRequestObserverProxy.h"
#include "nsIStreamListenerProxy.h"
#include "nsIStreamProviderProxy.h"
#include "nsISimpleStreamListener.h"
#include "nsISimpleStreamProvider.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"
#include "nsIStreamIOChannel.h"
#include "nsITransport.h"
#include "nsIHttpChannel.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "nsIDownloader.h"
#include "nsIResumableEntityID.h"
#include "nsIStreamLoader.h"
#include "nsIUnicharStreamLoader.h"
#include "nsIStreamIO.h"
#include "nsIPipe.h"
#include "nsIProtocolHandler.h"
#include "nsIFileProtocolHandler.h"
#include "nsIStringStream.h"
#include "nsILocalFile.h"
#include "nsIFileStreams.h"
#include "nsXPIDLString.h"
#include "nsIProtocolProxyService.h"
#include "nsIProxyInfo.h"
#include "prio.h"       // for read/write flags, permissions, etc.

#include "nsNetCID.h"

// Helper, to simplify getting the I/O service.
const nsGetServiceByCID
do_GetIOService(nsresult* error = 0);

nsresult
NS_NewURI(nsIURI **result, 
          const nsACString &spec, 
          const char *charset = nsnull,
          nsIURI *baseURI = nsnull,
          nsIIOService *ioService = nsnull);     // pass in nsIIOService to optimize callers

inline nsresult
NS_NewURI(nsIURI* *result, 
          const nsAString& spec, 
          const char *charset = nsnull,
          nsIURI* baseURI = nsnull,
          nsIIOService* ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    return NS_NewURI(result, NS_ConvertUCS2toUTF8(spec), charset, baseURI, ioService);
}

inline nsresult
NS_NewURI(nsIURI* *result, 
          const char *spec,
          nsIURI* baseURI = nsnull,
          nsIIOService* ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    return NS_NewURI(result, nsDependentCString(spec), nsnull, baseURI, ioService);
}

nsresult
NS_NewFileURI(nsIURI* *result, 
              nsIFile* spec, 
              nsIIOService* ioService = nsnull);     // pass in nsIIOService to optimize callers

nsresult
NS_NewChannel(nsIChannel* *result, 
              nsIURI* uri,
              nsIIOService* ioService = nsnull,    // pass in nsIIOService to optimize callers
              nsILoadGroup* loadGroup = nsnull,
              nsIInterfaceRequestor* notificationCallbacks = nsnull,
              nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIRequest::LOAD_NORMAL));

// Use this function with CAUTION. And do not use it on 
// the UI thread. It creates a stream that blocks when
// you Read() from it and blocking the UI thread is
// illegal. If you don't want to implement a full
// blown asyncrhonous consumer (via nsIStreamListener)
// look at nsIStreamLoader instead.
nsresult
NS_OpenURI(nsIInputStream* *result,
           nsIURI* uri,
           nsIIOService* ioService = nsnull,     // pass in nsIIOService to optimize callers
           nsILoadGroup* loadGroup = nsnull,
           nsIInterfaceRequestor* notificationCallbacks = nsnull,
           nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIRequest::LOAD_NORMAL));

nsresult
NS_OpenURI(nsIStreamListener* aConsumer, 
           nsISupports* context, 
           nsIURI* uri,
           nsIIOService* ioService = nsnull,     // pass in nsIIOService to optimize callers
           nsILoadGroup* loadGroup = nsnull,
           nsIInterfaceRequestor* notificationCallbacks = nsnull,
           nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIRequest::LOAD_NORMAL));

nsresult
NS_MakeAbsoluteURI(nsACString &result,
                   const nsACString &spec, 
                   nsIURI *baseURI, 
                   nsIIOService *ioService = nsnull);     // pass in nsIIOService to optimize callers

nsresult
NS_MakeAbsoluteURI(char **result,
                   const char *spec, 
                   nsIURI *baseURI, 
                   nsIIOService *ioService = nsnull);     // pass in nsIIOService to optimize callers

nsresult
NS_MakeAbsoluteURI(nsAString &result,
                   const nsAString &spec, 
                   nsIURI *baseURI,
                   nsIIOService *ioService = nsnull);     // pass in nsIIOService to optimize callers

nsresult
NS_NewPostDataStream(nsIInputStream **result,
                     PRBool isFile,
                     const nsACString &data,
                     PRUint32 encodeFlags,
                     nsIIOService* ioService = nsnull);     // pass in nsIIOService to optimize callers

nsresult
NS_NewStreamIOChannel(nsIStreamIOChannel **result,
                      nsIURI* uri,
                      nsIStreamIO* io);

nsresult
NS_NewInputStreamChannel(nsIChannel **result,
                         nsIURI* uri,
                         nsIInputStream* inStr,
                         const nsACString &contentType,
                         const nsACString &contentCharset,
                         PRInt32 contentLength);

nsresult
NS_NewLoadGroup(nsILoadGroup* *result, nsIRequestObserver* obs);

nsresult
NS_NewDownloader(nsIDownloader* *result,
                 nsIURI* uri,
                 nsIDownloadObserver* observer,
                 nsISupports* context = nsnull,
                 PRBool synchronous = PR_FALSE,
                 nsILoadGroup* loadGroup = nsnull,
                 nsIInterfaceRequestor* notificationCallbacks = nsnull,
                 nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIRequest::LOAD_NORMAL));

nsresult
NS_NewStreamLoader(nsIStreamLoader **aResult,
                   nsIChannel *aChannel,
                   nsIStreamLoaderObserver *aObserver,
                   nsISupports *aContext);

nsresult
NS_NewStreamLoader(nsIStreamLoader* *result,
                   nsIURI* uri,
                   nsIStreamLoaderObserver* observer,
                   nsISupports* context = nsnull,
                   nsILoadGroup* loadGroup = nsnull,
                   nsIInterfaceRequestor* notificationCallbacks = nsnull,
                   nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIRequest::LOAD_NORMAL),
                   nsIURI* referrer = nsnull);

nsresult
NS_NewUnicharStreamLoader(nsIUnicharStreamLoader **aResult,
                          nsIChannel *aChannel,
                          nsIUnicharStreamLoaderObserver *aObserver,
                          nsISupports *aContext = nsnull,
                          PRUint32 aSegmentSize = nsIUnicharStreamLoader::DEFAULT_SEGMENT_SIZE);

nsresult
NS_NewRequestObserverProxy(nsIRequestObserver **aResult,
                           nsIRequestObserver *aObserver,
                           nsIEventQueue *aEventQ=nsnull);

nsresult
NS_NewStreamListenerProxy(nsIStreamListener **aResult,
                          nsIStreamListener *aListener,
                          nsIEventQueue *aEventQ=nsnull,
                          PRUint32 aBufferSegmentSize=0,
                          PRUint32 aBufferMaxSize=0);

nsresult
NS_NewStreamProviderProxy(nsIStreamProvider **aResult,
                          nsIStreamProvider *aProvider,
                          nsIEventQueue *aEventQ=nsnull,
                          PRUint32 aBufferSegmentSize=0,
                          PRUint32 aBufferMaxSize=0);

nsresult
NS_NewSimpleStreamListener(nsIStreamListener **aResult,
                           nsIOutputStream *aSink,
                           nsIRequestObserver *aObserver=nsnull);

nsresult
NS_NewSimpleStreamProvider(nsIStreamProvider **aResult,
                           nsIInputStream *aSource,
                           nsIRequestObserver *aObserver=nsnull);


// Depracated, prefer NS_NewStreamListenerProxy
nsresult
NS_NewAsyncStreamListener(nsIStreamListener **result,
                          nsIStreamListener *receiver,
                          nsIEventQueue *eventQueue);

// Depracated, prefer a true synchonous implementation
nsresult
NS_NewSyncStreamListener(nsIInputStream **aInStream, 
                         nsIOutputStream **aOutStream,
                         nsIStreamListener **aResult);

//
// Calls AsyncWrite on the specified transport, with a stream provider that
// reads data from the specified input stream.
//
nsresult
NS_AsyncWriteFromStream(nsIRequest **aRequest,
                        nsITransport *aTransport,
                        nsIInputStream *aSource,
                        PRUint32 aOffset=0,
                        PRUint32 aCount=0,
                        PRUint32 aFlags=0,
                        nsIRequestObserver *aObserver=NULL,
                        nsISupports *aContext=NULL);

//
// Calls AsyncRead on the specified transport, with a stream listener that
// writes data to the specified output stream.
//
nsresult
NS_AsyncReadToStream(nsIRequest **aRequest,
                     nsITransport *aTransport,
                     nsIOutputStream *aSink,
                     PRUint32 aOffset=0,
                     PRUint32 aCount=0,
                     PRUint32 aFlags=0,
                     nsIRequestObserver *aObserver=NULL,
                     nsISupports *aContext=NULL);

nsresult
NS_CheckPortSafety(PRInt32 port, const char* scheme, nsIIOService* ioService = nsnull);

nsresult
NS_NewProxyInfo(const char* type, const char* host, PRInt32 port, nsIProxyInfo* *result);

nsresult
NS_GetFileProtocolHandler(nsIFileProtocolHandler **result,
                          nsIIOService *ioService=nsnull);

nsresult
NS_GetFileFromURLSpec(const nsACString &inURL, nsIFile **result,
                      nsIIOService *ioService=nsnull);

nsresult
NS_GetURLSpecFromFile(nsIFile* aFile, nsACString &aUrl,
                      nsIIOService *ioService=nsnull);

nsresult
NS_NewResumableEntityID(nsIResumableEntityID** aRes,
                        PRUint32 size,
                        PRTime lastModified);

nsresult
NS_ExamineForProxy(const char* scheme, const char* host, PRInt32 port, 
                   nsIProxyInfo* *proxyInfo);

nsresult
NS_ParseContentType(const nsACString &rawContentType,
                    nsCString &contentType,
                    nsCString &contentCharset);

#endif // !nsNetUtil_h__
