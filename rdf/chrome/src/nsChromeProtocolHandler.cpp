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

/*

  A protocol handler for ``chrome:''

*/

#include "nsChromeProtocolHandler.h"
#include "nsCOMPtr.h"
#include "nsContentCID.h"
#include "nsCRT.h"
#include "nsIChannel.h"
#include "nsIChromeRegistry.h"
#include "nsIComponentManager.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIFastLoadService.h"
#include "nsIFile.h"
#include "nsIFileChannel.h"
#include "nsIIOService.h"
#include "nsIJARChannel.h"
#include "nsIJARURI.h"
#include "nsILoadGroup.h"
#include "nsIObjectOutputStream.h"
#include "nsIScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIXULPrototypeCache.h"
#include "nsIXULPrototypeDocument.h"
#include "nsNetCID.h"
#include "nsXPIDLString.h"
#include "prlog.h"

//----------------------------------------------------------------------

static NS_DEFINE_CID(kChromeRegistryCID,         NS_CHROMEREGISTRY_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID,            NS_STANDARDURL_CID);
static NS_DEFINE_CID(kXULPrototypeCacheCID,      NS_XULPROTOTYPECACHE_CID);

//----------------------------------------------------------------------
//
//  A channel that's used for loading cached chrome documents. Since a
//  cached chrome document really doesn't have anything to do to load,
//  this is just the puppeteer that pulls the webshell's strings at the
//  right time.
//
//  Specifically, when AsyncOpen() is called, it adds the channel to
//  the load group, and queues an asychronous event to fire the
//  listener's OnStartRequest().
//
//  After triggering OnStartRequest(), it then queues another event
//  which will fire the listener's OnStopRequest() and remove the
//  channel from the load group.
//
//  Each is done asynchronously to allow the stack to unwind back to
//  the main event loop. This avoids any weird re-entrancy that occurs
//  if we try to immediately fire the On[Start|Stop]Request().
//
//  For logging information, NSPR_LOG_MODULES=nsCachedChromeChannel:5
//

class nsCachedChromeChannel : public nsIChannel
{
protected:
    nsCachedChromeChannel(nsIURI* aURI);
    virtual ~nsCachedChromeChannel();

    nsCOMPtr<nsIURI>            mURI;
    nsCOMPtr<nsILoadGroup>      mLoadGroup;
    nsCOMPtr<nsIStreamListener> mListener;
    nsCOMPtr<nsISupports>       mContext;
    nsLoadFlags                 mLoadFlags;
    nsCOMPtr<nsISupports>       mOwner;
    nsresult                    mStatus;

    struct LoadEvent {
        PLEvent                mEvent;
        nsCachedChromeChannel* mChannel;
    };

    static nsresult
    PostLoadEvent(nsCachedChromeChannel* aChannel, PLHandleEventProc aHandler);

    static void* PR_CALLBACK HandleStartLoadEvent(PLEvent* aEvent);
    static void* PR_CALLBACK HandleStopLoadEvent(PLEvent* aEvent);
    static void PR_CALLBACK DestroyLoadEvent(PLEvent* aEvent);

#ifdef PR_LOGGING
    static PRLogModuleInfo* gLog;
#endif

public:
    static nsresult
    Create(nsIURI* aURI, nsIChannel** aResult);
	
    NS_DECL_ISUPPORTS

    // nsIRequest
    NS_IMETHOD GetName(PRUnichar* *result) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD IsPending(PRBool *_retval) { *_retval = PR_TRUE; return NS_OK; }
    NS_IMETHOD GetStatus(nsresult *status) { *status = mStatus; return NS_OK; }
    NS_IMETHOD Cancel(nsresult status)  { mStatus = status; return NS_OK; }
    NS_IMETHOD Suspend(void) { return NS_OK; }
    NS_IMETHOD Resume(void)  { return NS_OK; }
    NS_IMETHOD GetLoadGroup(nsILoadGroup **);
    NS_IMETHOD SetLoadGroup(nsILoadGroup *);
    NS_IMETHOD GetLoadFlags(nsLoadFlags *);
    NS_IMETHOD SetLoadFlags(nsLoadFlags);

// nsIChannel
    NS_DECL_NSICHANNEL

};

#ifdef PR_LOGGING
PRLogModuleInfo* nsCachedChromeChannel::gLog;
#endif

NS_IMPL_ISUPPORTS2(nsCachedChromeChannel,
                   nsIChannel,
                   nsIRequest);

nsresult
nsCachedChromeChannel::Create(nsIURI* aURI, nsIChannel** aResult)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    nsCachedChromeChannel* channel = new nsCachedChromeChannel(aURI);
    if (! channel)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = channel;
    NS_ADDREF(*aResult);
    return NS_OK;
}


nsCachedChromeChannel::nsCachedChromeChannel(nsIURI* aURI)
    : mURI(aURI), mLoadGroup(nsnull), mLoadFlags (nsIRequest::LOAD_NORMAL), mStatus(NS_OK)
{
    NS_INIT_REFCNT();

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsCachedChromeChannel");
#endif

    PR_LOG(gLog, PR_LOG_DEBUG,
           ("nsCachedChromeChannel[%p]: created", this));
}


nsCachedChromeChannel::~nsCachedChromeChannel()
{
    PR_LOG(gLog, PR_LOG_DEBUG,
           ("nsCachedChromeChannel[%p]: destroyed", this));
}


NS_IMETHODIMP
nsCachedChromeChannel::GetOriginalURI(nsIURI* *aOriginalURI)
{
    *aOriginalURI = mURI;
    NS_ADDREF(*aOriginalURI);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetOriginalURI(nsIURI* aOriginalURI)
{
  // don't stp on a uri if we already have one there...this is a work around fix
  // for Bug #34769.
  if (!mURI)
    mURI = aOriginalURI;
  return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetURI(nsIURI* *aURI)
{
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::Open(nsIInputStream **_retval)
{
//    NS_NOTREACHED("don't do that");
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
    if (listener) {
        nsresult rv;

        if (mLoadGroup) {
            PR_LOG(gLog, PR_LOG_DEBUG,
                   ("nsCachedChromeChannel[%p]: adding self to load group %p",
                    this, mLoadGroup.get()));

            rv = mLoadGroup->AddRequest(this, nsnull);
            if (NS_FAILED(rv)) return rv;
        }

        // Fire the OnStartRequest(), which will cause the XUL
        // document to get embedded.
        PR_LOG(gLog, PR_LOG_DEBUG,
               ("nsCachedChromeChannel[%p]: firing OnStartRequest for %p",
                this, listener));

        // Queue an event to ourselves to let the stack unwind before
        // calling OnStartRequest(). This allows embedding to occur
        // before we fire OnStopRequest().
        rv = PostLoadEvent(this, HandleStartLoadEvent);
        if (NS_FAILED(rv)) {
            if (mLoadGroup) {
                PR_LOG(gLog, PR_LOG_DEBUG,
                       ("nsCachedChromeChannel[%p]: removing self from load group %p",
                        this, mLoadGroup.get()));

                (void) mLoadGroup->RemoveRequest(this, nsnull, NS_OK);
            }

            return rv;
        }

        mContext  = ctxt;
        mListener = listener;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetOwner(nsISupports * *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetOwner(nsISupports * aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks)
{
    *aNotificationCallbacks = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks)
{
    return NS_OK;	// ignored
}

NS_IMETHODIMP
nsCachedChromeChannel::GetContentType(char * *aContentType)
{
    *aContentType = nsCRT::strdup("mozilla.application/cached-xul");
    return *aContentType ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetContentType(const char *aContentType)
{
    // Do not allow the content-type to be changed.
    NS_NOTREACHED("don't do that");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetContentLength(PRInt32 *aContentLength)
{
    NS_NOTREACHED("don't do that");
    *aContentLength = 0;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetContentLength(PRInt32 aContentLength)
{
    NS_NOTREACHED("nsCachedChromeChannel::SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsCachedChromeChannel::PostLoadEvent(nsCachedChromeChannel* aChannel,
                                     PLHandleEventProc aHandler)
{
    nsresult rv;

    nsCOMPtr<nsIEventQueueService> svc = do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    if (! svc)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIEventQueue> queue;
    rv = svc->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
    if (NS_FAILED(rv)) return rv;

    if (! queue)
        return NS_ERROR_UNEXPECTED;

    LoadEvent* event = new LoadEvent;
    if (! event)
        return NS_ERROR_OUT_OF_MEMORY;

    PL_InitEvent(NS_REINTERPRET_CAST(PLEvent*, event),
                 nsnull,
                 aHandler,
                 DestroyLoadEvent);

    event->mChannel = aChannel;
    NS_ADDREF(event->mChannel);

    rv = queue->EnterMonitor();
    if (NS_SUCCEEDED(rv)) {
        (void) queue->PostEvent(NS_REINTERPRET_CAST(PLEvent*, event));
        (void) queue->ExitMonitor();
        return NS_OK;
    }

    // If we get here, something bad happened. Clean up.
    NS_RELEASE(event->mChannel);
    delete event;
    return rv;
}

void* PR_CALLBACK
nsCachedChromeChannel::HandleStartLoadEvent(PLEvent* aEvent)
{
    // Fire the OnStartRequest() for the cached chrome channel, then
    // queue another event to trigger the OnStopRequest()...
    LoadEvent* event = NS_REINTERPRET_CAST(LoadEvent*, aEvent);
    nsCachedChromeChannel* channel = event->mChannel;

    // If the load has been cancelled, then just bail now. We won't
    // send On[Start|Stop]Request().
    if (NS_FAILED(channel->mStatus))
      return nsnull;

    PR_LOG(gLog, PR_LOG_DEBUG,
              ("nsCachedChromeChannel[%p]: firing OnStartRequest for %p",
               channel, channel->mListener.get()));

    (void) channel->mListener->OnStartRequest(channel, channel->mContext);
    (void) PostLoadEvent(channel, HandleStopLoadEvent);
    return nsnull;
}


void* PR_CALLBACK
nsCachedChromeChannel::HandleStopLoadEvent(PLEvent* aEvent)
{
    // Fire the OnStopRequest() for the cached chrome channel, and
    // remove it from the load group.
    LoadEvent* event = NS_REINTERPRET_CAST(LoadEvent*, aEvent);
    nsCachedChromeChannel* channel = event->mChannel;
    nsIRequest* request = NS_REINTERPRET_CAST(nsIRequest*, channel);


    PR_LOG(gLog, PR_LOG_DEBUG,
           ("nsCachedChromeChannel[%p]: firing OnStopRequest for %p",
            channel, channel->mListener.get()));

    (void) channel->mListener->OnStopRequest(request, channel->mContext,
                                             channel->mStatus);

    if (channel->mLoadGroup) {
        PR_LOG(gLog, PR_LOG_DEBUG,
               ("nsCachedChromeChannel[%p]: removing self from load group %p",
                channel, channel->mLoadGroup.get()));

        (void) channel->mLoadGroup->RemoveRequest(request, nsnull, NS_OK);
    }

    channel->mListener = nsnull;
    channel->mContext  = nsnull;

    return nsnull;
}

void PR_CALLBACK
nsCachedChromeChannel::DestroyLoadEvent(PLEvent* aEvent)
{
    LoadEvent* event = NS_REINTERPRET_CAST(LoadEvent*, aEvent);
    NS_RELEASE(event->mChannel);
    delete event;
}

////////////////////////////////////////////////////////////////////////////////

nsChromeProtocolHandler::nsChromeProtocolHandler()
{
    NS_INIT_REFCNT();
}

nsresult
nsChromeProtocolHandler::Init()
{
    return NS_OK;
}

nsChromeProtocolHandler::~nsChromeProtocolHandler()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsChromeProtocolHandler, nsIProtocolHandler, nsISupportsWeakReference);

NS_METHOD
nsChromeProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsChromeProtocolHandler* ph = new nsChromeProtocolHandler();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = ph->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(ph);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsChromeProtocolHandler::GetScheme(char* *result)
{
    *result = nsCRT::strdup("chrome");
    if (*result == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for chrome: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.
    *_retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_STD;
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                                nsIURI **result)
{
    nsresult rv;

    // Chrome: URLs (currently) have no additional structure beyond that provided
    // by standard URLs, so there is no "outer" given to CreateInstance

    nsCOMPtr<nsIURI> url(do_CreateInstance(kStandardURLCID, &rv));
    if (NS_FAILED(rv))
        return rv;

    if (aBaseURI) {
        nsXPIDLCString aResolvedURI;
        rv = aBaseURI->Resolve(aSpec, getter_Copies(aResolvedURI));
        if (NS_SUCCEEDED(rv))
            rv = url->SetSpec(aResolvedURI);
    }
    else {
        rv = url->SetSpec((char*)aSpec);
    }

    if (NS_FAILED(rv))
        return rv;

    *result = url;
    NS_ADDREF(*result);
    return rv;
}

NS_IMETHODIMP
nsChromeProtocolHandler::NewChannel(nsIURI* aURI,
                                    nsIChannel* *aResult)
{
    nsresult rv;
    nsCOMPtr<nsIChannel> result;

    // Canonify the "chrome:" URL; e.g., so that we collapse
    // "chrome://navigator/content/navigator.xul" and "chrome://navigator/content"
    // and "chrome://navigator/content/navigator.xul".
    nsCOMPtr<nsIChromeRegistry> reg(do_GetService(kChromeRegistryCID, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = reg->Canonify(aURI);
    if (NS_FAILED(rv)) return rv;

    // Check the prototype cache to see if we've already got the
    // document in the cache.
    nsCOMPtr<nsIXULPrototypeCache> cache =
             do_GetService(kXULPrototypeCacheCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIXULPrototypeDocument> proto;
    rv = cache->GetPrototype(aURI, getter_AddRefs(proto));
    if (NS_FAILED(rv)) return rv;

    if (proto) {
        // ...in which case, we'll create a dummy stream that'll just
        // load the thing.
        rv = nsCachedChromeChannel::Create(aURI, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // Miss. Resolve the chrome URL using the registry and do a
        // normal necko load.
        //nsXPIDLCString oldSpec;
        //aURI->GetSpec(getter_Copies(oldSpec));
        //printf("*************************** %s\n", (const char*)oldSpec);

        nsXPIDLCString spec;
        rv = reg->ConvertChromeURL(aURI, getter_Copies(spec));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIIOService> ioServ(do_GetService(kIOServiceCID, &rv));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIURI> chromeURI;
        rv = ioServ->NewURI(spec, nsnull, getter_AddRefs(chromeURI));
        if (NS_FAILED(rv)) return rv;

        rv = ioServ->NewChannelFromURI(chromeURI, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        // XXX Will be removed someday when we handle remote chrome.
        nsCOMPtr<nsIFileChannel> fileChan;
        nsCOMPtr<nsIJARChannel> jarChan;
        fileChan = do_QueryInterface(result);
        if (!fileChan)
            jarChan = do_QueryInterface(result);
        if (!fileChan && !jarChan) {
            NS_WARNING("Remote chrome not allowed! Only file:, resource:, and jar: are valid.\n");
            result = nsnull;
            return NS_ERROR_FAILURE;
        }

        // Make sure that the channel remembers where it was
        // originally loaded from.
        rv = result->SetOriginalURI(aURI);
        if (NS_FAILED(rv)) return rv;

        // Get a system principal for xul files and set the owner
        // property of the result
        nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
        nsXPIDLCString fileExtension;
        rv = url->GetFileExtension(getter_Copies(fileExtension));
        if (PL_strcasecmp(fileExtension, "xul") == 0 ||
            PL_strcasecmp(fileExtension, "html") == 0 ||
            PL_strcasecmp(fileExtension, "xml") == 0)
        {
            nsCOMPtr<nsIScriptSecurityManager> securityManager =
                     do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIPrincipal> principal;
            rv = securityManager->GetSystemPrincipal(getter_AddRefs(principal));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsISupports> owner = do_QueryInterface(principal);
            result->SetOwner(owner);
        }

        // Track FastLoad file dependencies.
        //
        // This is harder than it ought to be!  While an nsResChannel "is-a"
        // nsIFileChannel, an nsJARChannel is not.  Once you unravel the jar:
        // URI, you may have a resource: URL -- but without a channel for it,
        // you can't get the URI that it yields through substitution!
        //
        // XXXbe fix nsResChannel.cpp to move the substitution code into a new
        //       nsResURL class?
        nsCOMPtr<nsIFastLoadService> fastLoadServ(do_GetFastLoadService());
        if (fastLoadServ) {
            nsCOMPtr<nsIObjectOutputStream> objectOutput;
            fastLoadServ->GetOutputStream(getter_AddRefs(objectOutput));
            if (objectOutput) {
                nsCOMPtr<nsIFile> file;
                nsCOMPtr<nsIChannel> chan = result;
                while (!fileChan) {
                    nsCOMPtr<nsIURI> uri;
                    chan->GetURI(getter_AddRefs(uri));

                    // Loop, jar: URIs can nest (e.g. jar:jar:A.jar!B.jar!C.xml).
                    // Often, however, we have jar:resource:/chrome/A.jar!C.xml.
                    nsCOMPtr<nsIJARURI> jarURI;
                    while ((jarURI = do_QueryInterface(uri)) != nsnull)
                        jarURI->GetJARFile(getter_AddRefs(uri));

                    // Here we must have a URL of the form resource:/chrome/A.jar
                    // or file:/some/path/to/A.jar.  Let's hope for the latter.
                    nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(uri));
                    if (fileURL) {
                        fileURL->GetFile(getter_AddRefs(file));
                        if (file)
                            break;
                    }

                    // Thanks to the way that the resource: URL implementation
                    // hides its substitution code from itself and the rest of
                    // the world, we must make a new channel simply to get the
                    // substituted URI.
                    ioServ->NewChannelFromURI(uri, getter_AddRefs(chan));
                    if (!chan)
                        break;
                    fileChan = do_QueryInterface(chan);
                }

                if (!file && fileChan)
                    fileChan->GetFile(getter_AddRefs(file));
                if (file) {
                    rv = fastLoadServ->AddDependency(file);
                    if (NS_FAILED(rv))
                        cache->AbortFastLoads();
                }
            }
        }
    }

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
