/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  A protocol handler for ``chrome:''

*/

#include "nsChromeProtocolHandler.h"
#include "nsChromeRegistry.h"
#include "nsCOMPtr.h"
#include "nsContentCID.h"
#include "nsThreadUtils.h"
#include "nsCRT.h"
#include "nsIChannel.h"
#include "nsIChromeRegistry.h"
#include "nsIComponentManager.h"
#include "nsIFastLoadService.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIFileChannel.h"
#include "nsIIOService.h"
#include "nsIJARChannel.h"
#include "nsIJARURI.h"
#include "nsILoadGroup.h"
#include "nsIObjectOutputStream.h"
#include "nsIScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIStandardURL.h"
#include "nsIStreamListener.h"
#ifdef MOZ_XUL
#include "nsIXULPrototypeCache.h"
#include "nsIXULPrototypeDocument.h"
#endif
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsAutoPtr.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "prlog.h"

//----------------------------------------------------------------------

#ifdef MOZ_XUL
static NS_DEFINE_CID(kXULPrototypeCacheCID,      NS_XULPROTOTYPECACHE_CID);
#endif

// This comes from nsChromeRegistry.cpp
extern nsIChromeRegistry* gChromeRegistry;

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
    nsCOMPtr<nsIURI>            mOriginalURI;
    nsCOMPtr<nsILoadGroup>      mLoadGroup;
    nsCOMPtr<nsIStreamListener> mListener;
    nsCOMPtr<nsISupports>       mContext;
    nsLoadFlags                 mLoadFlags;
    nsCOMPtr<nsISupports>       mOwner;
    nsresult                    mStatus;

    typedef void (*LoadEventCallback)(nsCachedChromeChannel*);

    struct LoadEvent : nsRunnable {
        LoadEvent(nsCachedChromeChannel *chan, LoadEventCallback callback)
            : mChannel(chan), mCallback(callback) {}

        nsRefPtr<nsCachedChromeChannel> mChannel;
        LoadEventCallback mCallback;

        NS_IMETHOD Run() {
            mCallback(mChannel);
            return NS_OK;
        }
    };

    static nsresult
    PostLoadEvent(nsCachedChromeChannel* aChannel, LoadEventCallback aCallback);

    static void HandleStartLoadEvent(nsCachedChromeChannel* channel);
    static void HandleStopLoadEvent(nsCachedChromeChannel* channel);

#ifdef PR_LOGGING
    static PRLogModuleInfo* gLog;
#endif

public:
    static nsresult
    Create(nsIURI* aURI, nsIChannel** aResult);
	
    NS_DECL_ISUPPORTS

    // nsIRequest
    NS_IMETHOD GetName(nsACString &result) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD IsPending(PRBool *_retval) { *_retval = (mListener != nsnull); return NS_OK; }
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
                   nsIRequest)

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
    : mURI(aURI), 
      mLoadFlags(nsIRequest::LOAD_NORMAL), 
      mStatus(NS_OK)
{
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
    *aOriginalURI = mOriginalURI ? mOriginalURI : mURI;
    NS_ADDREF(*aOriginalURI);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetOriginalURI(nsIURI* aOriginalURI)
{
    mOriginalURI = aOriginalURI;
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
nsCachedChromeChannel::GetContentType(nsACString &aContentType)
{
    aContentType.AssignLiteral("mozilla.application/cached-xul");
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetContentType(const nsACString &aContentType)
{
    // Do not allow the content-type to be changed.
    NS_NOTREACHED("don't do that");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetContentCharset(nsACString &aContentCharset)
{
    aContentCharset.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetContentCharset(const nsACString &aContentCharset)
{
    // Do not allow the content charset to be changed.
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
                                     LoadEventCallback aCallback)
{
    nsCOMPtr<nsIRunnable> event = new LoadEvent(aChannel, aCallback);
    if (! event)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_DispatchToCurrentThread(event);
}

/*static*/ void
nsCachedChromeChannel::HandleStartLoadEvent(nsCachedChromeChannel *channel)
{
    // Fire the OnStartRequest() for the cached chrome channel, then
    // queue another event to trigger the OnStopRequest()...

    // If the load has been cancelled, then just bail now. We won't
    // send On[Start|Stop]Request().
    // XXX(darin): this violates the nsIChannel API
    if (NS_FAILED(channel->mStatus))
      return;

    PR_LOG(gLog, PR_LOG_DEBUG,
              ("nsCachedChromeChannel[%p]: firing OnStartRequest for %p",
               channel, channel->mListener.get()));

    (void) channel->mListener->OnStartRequest(channel, channel->mContext);
    (void) PostLoadEvent(channel, HandleStopLoadEvent);
}


/*static*/ void
nsCachedChromeChannel::HandleStopLoadEvent(nsCachedChromeChannel* channel)
{
    // Fire the OnStopRequest() for the cached chrome channel, and
    // remove it from the load group.

    PR_LOG(gLog, PR_LOG_DEBUG,
           ("nsCachedChromeChannel[%p]: firing OnStopRequest for %p",
            channel, channel->mListener.get()));

    (void) channel->mListener->OnStopRequest(channel, channel->mContext,
                                             channel->mStatus);

    if (channel->mLoadGroup) {
        PR_LOG(gLog, PR_LOG_DEBUG,
               ("nsCachedChromeChannel[%p]: removing self from load group %p",
                channel, channel->mLoadGroup.get()));

        (void) channel->mLoadGroup->RemoveRequest(channel, nsnull, NS_OK);
    }

    channel->mListener = nsnull;
    channel->mContext  = nsnull;
}

////////////////////////////////////////////////////////////////////////////////

nsChromeProtocolHandler::nsChromeProtocolHandler()
{
}

nsresult
nsChromeProtocolHandler::Init()
{
    return NS_OK;
}

nsChromeProtocolHandler::~nsChromeProtocolHandler()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsChromeProtocolHandler, nsIProtocolHandler, nsISupportsWeakReference)

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
nsChromeProtocolHandler::GetScheme(nsACString &result)
{
    result = "chrome";
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
nsChromeProtocolHandler::NewURI(const nsACString &aSpec,
                                const char *aCharset,
                                nsIURI *aBaseURI,
                                nsIURI **result)
{
    NS_PRECONDITION(result, "Null out param");
    
    nsresult rv;

    *result = nsnull;

    // Chrome: URLs (currently) have no additional structure beyond that provided
    // by standard URLs, so there is no "outer" given to CreateInstance

    nsCOMPtr<nsIStandardURL> url(do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;

    rv = url->Init(nsIStandardURL::URLTYPE_STANDARD, -1, aSpec, aCharset, aBaseURI);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIURI> uri(do_QueryInterface(url, &rv));
    if (NS_FAILED(rv))
        return rv;
    
    // Canonify the "chrome:" URL; e.g., so that we collapse
    // "chrome://navigator/content/" and "chrome://navigator/content"
    // and "chrome://navigator/content/navigator.xul".
    rv = nsChromeRegistry::Canonify(uri);
    if (NS_FAILED(rv))
        return rv;

    *result = uri;
    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::NewChannel(nsIURI* aURI,
                                    nsIChannel* *aResult)
{
    NS_ENSURE_ARG_POINTER(aURI);
    NS_PRECONDITION(aResult, "Null out param");
    
#ifdef DEBUG
    // Check that the uri we got is already canonified
    nsresult debug_rv;
    nsCOMPtr<nsIChromeRegistry> debugReg(do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &debug_rv));
    if (NS_SUCCEEDED(debug_rv)) {
        nsCOMPtr<nsIURI> debugClone;
        debug_rv = aURI->Clone(getter_AddRefs(debugClone));
        if (NS_SUCCEEDED(debug_rv)) {
            debug_rv = nsChromeRegistry::Canonify(debugClone);
            if (NS_SUCCEEDED(debug_rv)) {
                PRBool same;
                debug_rv = aURI->Equals(debugClone, &same);
                if (NS_SUCCEEDED(debug_rv)) {
                    NS_ASSERTION(same, "Non-canonified chrome uri passed to nsChromeProtocolHandler::NewChannel!");
                }
            }
                
        }
    }
#endif

    nsresult rv;
    nsCOMPtr<nsIChannel> result;

#ifdef MOZ_XUL
    // Check the prototype cache to see if we've already got the
    // document in the cache.
    nsCOMPtr<nsIXULPrototypeCache> cache =
             do_GetService(kXULPrototypeCacheCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIXULPrototypeDocument> proto;
    cache->GetPrototype(aURI, getter_AddRefs(proto));

    // Same comment as nsXULDocument::StartDocumentLoad and
    // nsXULDocument::ResumeWalk
    // - Ben Goodger
    //
    // We don't abort on failure here because there are too many valid
    // cases that can return failure, and the null-ness of |proto| is enough
    // to trigger the fail-safe parse-from-disk solution. Example failure cases
    // (for reference) include:
    //
    // NS_ERROR_NOT_AVAILABLE: the URI cannot be found in the FastLoad cache, 
    //                         parse from disk
    // other: the FastLoad cache file, XUL.mfl, could not be found, probably
    //        due to being accessed before a profile has been selected (e.g.
    //        loading chrome for the profile manager itself). This must be 
    //        parsed from disk. 

    if (proto) {
        // ...in which case, we'll create a dummy stream that'll just
        // load the thing.
        rv = nsCachedChromeChannel::Create(aURI, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }
    else
#endif
        {
        // Miss. Resolve the chrome URL using the registry and do a
        // normal necko load.
        //nsXPIDLCString oldSpec;
        //aURI->GetSpec(getter_Copies(oldSpec));
        //printf("*************************** %s\n", (const char*)oldSpec);

        nsCOMPtr<nsIChromeRegistry> reg = gChromeRegistry;
        if (!reg) {
            reg = do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
            if (NS_FAILED(rv)) return rv;
        }

        nsCOMPtr<nsIURI> chromeURI;
        rv = reg->ConvertChromeURL(aURI, getter_AddRefs(chromeURI));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIIOService> ioServ (do_GetIOService());
        if (!ioServ) return NS_ERROR_FAILURE;

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

        // Get a system principal for content files and set the owner
        // property of the result
        nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
        nsCAutoString path;
        rv = url->GetPath(path);
        if (StringBeginsWith(path, NS_LITERAL_CSTRING("/content/")))
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

                if (fileChan) {
                    fileChan->GetFile(getter_AddRefs(file));
                } else {
                    nsCOMPtr<nsIURI> uri;
                    result->GetURI(getter_AddRefs(uri));

                    // Loop, jar URIs can nest (e.g. jar:jar:A.jar!B.jar!C.xml).
                    // Often, however, we have jar:resource:/chrome/A.jar!C.xml.
                    nsCOMPtr<nsIJARURI> jarURI;
                    while ((jarURI = do_QueryInterface(uri)) != nsnull)
                        jarURI->GetJARFile(getter_AddRefs(uri));

                    // Here we have a URL of the form resource:/chrome/A.jar
                    // or file:/some/path/to/A.jar.
                    nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(uri));
                    if (fileURL)
                        fileURL->GetFile(getter_AddRefs(file));
                }

                if (file) {
                    rv = fastLoadServ->AddDependency(file);
#ifdef MOZ_XUL
                    if (NS_FAILED(rv))
                        cache->AbortFastLoads();
#endif
                }
            }
        }
    }

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
