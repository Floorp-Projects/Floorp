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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsPrefetchService.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIDocCharset.h"
#include "nsIWebProgress.h"
#include "nsCURILoader.h"
#include "nsINetModuleMgr.h"
#include "nsICachingChannel.h"
#include "nsICacheVisitor.h"
#include "nsIHttpChannel.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIParserService.h"
#include "nsParserCIID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "prtime.h"
#include "prlog.h"
#include "plstr.h"

#if defined(PR_LOGGING)
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsPrefetch:5
//    set NSPR_LOG_FILE=prefetch.log
//
// this enables PR_LOG_ALWAYS level information and places all output in
// the file http.log
//
static PRLogModuleInfo *gPrefetchLog;
#endif
#define LOG(args) PR_LOG(gPrefetchLog, 4, args)
#define LOG_ENABLED() PR_LOG_TEST(gPrefetchLog, 4)

static NS_DEFINE_IID(kDocLoaderServiceCID, NS_DOCUMENTLOADER_SERVICE_CID);
static NS_DEFINE_IID(kParserServiceCID, NS_PARSERSERVICE_CID);
static NS_DEFINE_IID(kNetModuleMgrCID, NS_NETMODULEMGR_CID);
static NS_DEFINE_IID(kPrefServiceCID, NS_PREFSERVICE_CID);

//-----------------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------------

static inline PRUint32
PRTimeToSeconds(PRTime t_usec)
{
    PRTime usec_per_sec;
    PRUint32 t_sec;
    LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
    LL_DIV(t_usec, t_usec, usec_per_sec);
    LL_L2I(t_sec, t_usec);
    return t_sec;
}

#define NowInSeconds() PRTimeToSeconds(PR_Now())

//
// parses an attribute value pair (e.g., attr=value).  allows
// double-quotation marks surrounding value.  input must be null
// terminated, and will be updated to point past the attribute
// value pair on success or the next whitespace character or comma
// on failure.
// 
static PRBool
ParseAttrValue(const char *&input,
               const char *&ab,
               const char *&ae,
               const char *&vb,
               const char *&ve)
{
    const char *p = PL_strpbrk(input, " \t=,");
    if (!p) {
        input += strlen(input);
        return PR_FALSE;
    }
    if (*p == ',' || *p == ' ' || *p == '\t') {
        input = p;
        return PR_FALSE;
    }

    // else, we located the equals sign...
    ab = input;
    ae = p;
    vb = p+1;

    if (*vb == '"')
        ++vb;

    ve = PL_strpbrk(p+1, " \t,");
    if (!ve)
        ve = input + strlen(input);
    input = ve;

    // only ignore trailing quote if there was a leading one
    if (ve > vb && *(vb-1) == '"' && *(ve-1) == '"')
        --ve;

    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// nsPrefetchListener <public>
//-----------------------------------------------------------------------------

nsPrefetchListener::nsPrefetchListener(nsPrefetchService *aService)
{
    NS_INIT_ISUPPORTS();

    NS_ADDREF(mService = aService);
}

nsPrefetchListener::~nsPrefetchListener()
{
    NS_RELEASE(mService);
}

//-----------------------------------------------------------------------------
// nsPrefetchListener <private>
//-----------------------------------------------------------------------------

NS_METHOD
nsPrefetchListener::ConsumeSegments(nsIInputStream *aInputStream,
                                    void *aClosure,
                                    const char *aFromSegment,
                                    PRUint32 aOffset,
                                    PRUint32 aCount,
                                    PRUint32 *aBytesConsumed)
{
    *aBytesConsumed = aCount;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchListener::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS2(nsPrefetchListener,
                   nsIRequestObserver,
                   nsIStreamListener)

//-----------------------------------------------------------------------------
// nsPrefetchListener::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchListener::OnStartRequest(nsIRequest *aRequest,
                                   nsISupports *aContext)
{
    nsCOMPtr<nsICachingChannel> cachingChannel(do_QueryInterface(aRequest));
    if (cachingChannel) {
        // no need to prefetch a document that is already in the cache
        PRBool fromCache;
        if (NS_SUCCEEDED(cachingChannel->IsFromCache(&fromCache)) && fromCache) {
            LOG(("document is already in the cache; canceling prefetch\n"));
            return NS_BINDING_ABORTED;
        }
        // no need to prefetch a document that must be requested fresh each
        // and every time.
        nsCOMPtr<nsISupports> cacheToken;
        cachingChannel->GetCacheToken(getter_AddRefs(cacheToken));
        if (cacheToken) {
            nsCOMPtr<nsICacheEntryInfo> entryInfo(do_QueryInterface(cacheToken));
            if (entryInfo) {
                PRUint32 expTime;
                if (NS_SUCCEEDED(entryInfo->GetExpirationTime(&expTime))) {
                    if (NowInSeconds() >= expTime) {
                        LOG(("document cannot be reused from cache; canceling prefetch\n"));
                        return NS_BINDING_ABORTED;
                    }
                }
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchListener::OnDataAvailable(nsIRequest *aRequest,
                                    nsISupports *aContext,
                                    nsIInputStream *aStream,
                                    PRUint32 aOffset,
                                    PRUint32 aCount)
{
    PRUint32 bytesRead = 0;
    aStream->ReadSegments(ConsumeSegments, nsnull, aCount, &bytesRead);
    LOG(("prefetched %u bytes [offset=%u]\n", bytesRead, aOffset));
    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchListener::OnStopRequest(nsIRequest *aRequest,
                                  nsISupports *aContext,
                                  nsresult aStatus)
{
    LOG(("done prefetching [status=%x]\n", aStatus));

    mService->ProcessNextURI();
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService <public>
//-----------------------------------------------------------------------------

nsPrefetchService::nsPrefetchService()
    : mQueueHead(nsnull)
    , mQueueTail(nsnull)
    , mDisabled(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsPrefetchService::~nsPrefetchService()
{
    // cannot reach destructor if prefetch in progress (listener owns reference
    // to this service)
    EmptyQueue();
}

nsresult
nsPrefetchService::Init()
{
#if defined(PR_LOGGING)
    if (!gPrefetchLog)
        gPrefetchLog = PR_NewLogModule("nsPrefetch");
#endif

    static const eHTMLTags watchTags[] =
    { 
        eHTMLTag_link,
        eHTMLTag_unknown
    };

    nsresult rv;

    // Verify that "network.prefetch-next" preference is set to true. Skip
    // this step if we encounter any errors.
    nsCOMPtr<nsIPrefService> prefServ(do_GetService(kPrefServiceCID, &rv));
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIPrefBranch> prefs;
        rv = prefServ->GetBranch(nsnull, getter_AddRefs(prefs));
        if (NS_SUCCEEDED(rv)) {
            PRBool enabled;
            rv = prefs->GetBoolPref("network.prefetch-next", &enabled);
            if (NS_SUCCEEDED(rv) && !enabled) {
                LOG(("nsPrefetchService disabled via preferences\n"));
                return NS_ERROR_ABORT;
            }
        }
    }

    // Observe xpcom-shutdown event
    nsCOMPtr<nsIObserverService> observerServ(
            do_GetService("@mozilla.org/observer-service;1", &rv));
    if (NS_FAILED(rv)) return rv;

    rv = observerServ->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    // Register as an observer for HTTP headers
    nsCOMPtr<nsINetModuleMgr> netModuleMgr(do_GetService(kNetModuleMgrCID, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = netModuleMgr->RegisterModule(
            NS_NETWORK_MODULE_MANAGER_HTTP_RESPONSE_CONTRACTID, this);
    if (NS_FAILED(rv)) return rv;

    // Register as an observer for HTML "link" tags
    nsCOMPtr<nsIParserService> parserServ(do_GetService(kParserServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = parserServ->RegisterObserver(this, NS_LITERAL_STRING("text/html"), watchTags);
    if (NS_FAILED(rv)) return rv;
            
    // Register as an observer for the document loader  
    nsCOMPtr<nsIWebProgress> progress(do_GetService(kDocLoaderServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    return progress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_DOCUMENT);
}

//
// we register ourselves with the parser service category to ensure that
// we'll be initialized before the first page is read.
//
NS_METHOD
nsPrefetchService::RegisterProc(nsIComponentManager *aCompMgr,
                                nsIFile *aPath,
                                const char *registryLocation,
                                const char *componentType,
                                const nsModuleComponentInfo *info)
{
    nsCOMPtr<nsICategoryManager> catman(
            do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
    if (catman) {
        nsXPIDLCString prevEntry;
        catman->AddCategoryEntry("parser-service-category",
                                 NS_PREFETCHSERVICE_CLASSNAME,
                                 NS_PREFETCHSERVICE_CONTRACTID,
                                 PR_TRUE, PR_TRUE,
                                 getter_Copies(prevEntry));
    }
    return NS_OK;

}

NS_METHOD
nsPrefetchService::UnregisterProc(nsIComponentManager *aCompMgr,
                                  nsIFile *aPath,
                                  const char *registryLocation,
                                  const nsModuleComponentInfo *info)
{
    nsCOMPtr<nsICategoryManager> catman(
            do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
    if (catman)
        catman->DeleteCategoryEntry("parser-service-category", 
                                    NS_PREFETCHSERVICE_CONTRACTID,
                                    PR_TRUE);
    return NS_OK;
}

void
nsPrefetchService::ProcessNextURI()
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri;

    mCurrentChannel = nsnull;

    nsCOMPtr<nsIStreamListener> listener(new nsPrefetchListener(this));
    if (!listener) return;

    do {
        rv = DequeueURI(getter_AddRefs(uri));
        if (NS_FAILED(rv)) break;

#if defined(PR_LOGGING)
        if (LOG_ENABLED()) {
            nsCAutoString spec;
            uri->GetSpec(spec);
            LOG(("ProcessNextURI [%s]\n", spec.get()));
        }
#endif

        //
        // if opening the channel fails, then just skip to the next uri
        //
        rv = NS_NewChannel(getter_AddRefs(mCurrentChannel), uri, nsnull, nsnull,
                           nsnull, nsIRequest::LOAD_BACKGROUND);
        if (NS_FAILED(rv)) continue;

        rv = mCurrentChannel->AsyncOpen(listener, nsnull);
    }
    while (NS_FAILED(rv));
}

//-----------------------------------------------------------------------------
// nsPrefetchService <private>
//-----------------------------------------------------------------------------

nsresult
nsPrefetchService::EnqueueURI(nsIURI *aURI)
{
    nsPrefetchNode *node = new nsPrefetchNode(aURI);
    if (!node)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!mQueueTail) {
        mQueueHead = node;
        mQueueTail = node;
    }
    else {
        mQueueTail->mNext = node;
        mQueueTail = node;
    }

    return NS_OK;
}

nsresult
nsPrefetchService::DequeueURI(nsIURI **aURI)
{
    if (!mQueueHead)
        return NS_ERROR_NOT_AVAILABLE;

    // remove from the head
    NS_ADDREF(*aURI = mQueueHead->mURI);

    nsPrefetchNode *node = mQueueHead;
    mQueueHead = mQueueHead->mNext;
    delete node;

    if (!mQueueHead)
        mQueueTail = nsnull;

    return NS_OK;
}

void
nsPrefetchService::EmptyQueue()
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri;

    do {
        rv = DequeueURI(getter_AddRefs(uri));
    }
    while (NS_SUCCEEDED(rv));
}

void
nsPrefetchService::StartPrefetching()
{
    LOG(("StartPrefetching\n"));

    if (!mCurrentChannel)
        ProcessNextURI();
}

void
nsPrefetchService::StopPrefetching()
{
    LOG(("StopPrefetching\n"));

    if (mCurrentChannel) {
        mCurrentChannel->Cancel(NS_BINDING_ABORTED);
        mCurrentChannel = nsnull;
    }

    EmptyQueue();
}

nsresult
nsPrefetchService::GetDocumentCharset(nsISupports *aWebshell, nsACString &aCharset)
{
    nsresult rv;
    nsCOMPtr<nsIDocCharset> docCharset(do_QueryInterface(aWebshell, &rv));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLString uCharset;
    rv = docCharset->GetCharset(getter_Copies(uCharset));
    if (NS_FAILED(rv)) return rv;

    CopyUCS2toASCII(uCharset, aCharset);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS7(nsPrefetchService,
                   nsIPrefetchService,
                   nsIHttpNotify,
                   nsINetNotify,
                   nsIElementObserver,
                   nsIWebProgressListener,
                   nsIObserver,
                   nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsPrefetchService::nsIPretetchService
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchService::PrefetchURI(nsIURI *aURI)
{
    nsresult rv;

#if defined(PR_LOGGING)
    if (LOG_ENABLED()) {
        nsCAutoString spec;
        aURI->GetSpec(spec);
        LOG(("PrefetchURI [%s]\n", spec.get()));
    }
#endif

    if (mDisabled) {
        LOG(("rejected: prefetch service is disabled\n"));
        return NS_ERROR_ABORT;
    }

    //
    // XXX we should really be asking the protocol handler if it supports
    // caching, so we can determine if there is any value to prefetching.
    // for now, we'll only prefetch http links since we know that's the 
    // most common case.  ignore https links since https content only goes
    // into the memory cache.
    //
    // XXX we might want to either leverage nsIProtocolHandler::protocolFlags
    // or possibly nsIRequest::loadFlags to determine if this URI should be
    // prefetched.
    //
    PRBool isHttp;
    rv = aURI->SchemeIs("http", &isHttp); 
    if (NS_FAILED(rv) || !isHttp) {
        LOG(("rejected: URL is not of type http\n"));
        return NS_ERROR_ABORT;
    }

    //
    // skip URLs that contain query strings.  these URLs are likely to result
    // in documents that have zero freshness lifetimes, which we'd stop
    // prefetching anyways (see nsPrefetchListener::OnStartRequest).  this
    // check avoids nearly doubling the load on bugzilla, for example ;-)
    //
    nsCOMPtr<nsIURL> url(do_QueryInterface(aURI, &rv));
    if (NS_FAILED(rv)) return rv;
    nsCAutoString query;
    rv = url->GetQuery(query);
    if (NS_FAILED(rv) || !query.IsEmpty()) {
        LOG(("rejected: URL has a query string\n"));
        return NS_ERROR_ABORT;
    }

    // 
    // cancel if being prefetched
    //
    if (mCurrentChannel) {
        nsCOMPtr<nsIURI> currentURI;
        mCurrentChannel->GetURI(getter_AddRefs(currentURI));
        if (currentURI) {
            PRBool equals;
            if (NS_SUCCEEDED(currentURI->Equals(aURI, &equals)) && equals) {
                LOG(("rejected: URL is already being prefetched\n"));
                return NS_ERROR_ABORT;
            }
        }
    }

    //
    // cancel if already on the prefetch queue
    //
    nsPrefetchNode *node = mQueueHead;
    for (; node; node = node->mNext) {
        PRBool equals;
        if (NS_SUCCEEDED(node->mURI->Equals(aURI, &equals)) && equals) {
            LOG(("rejected: URL is already on prefetch queue\n"));
            return NS_ERROR_ABORT;
        }
    }

    return EnqueueURI(aURI);
}

//-----------------------------------------------------------------------------
// nsPrefetchService::nsIHttpNotify
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchService::OnModifyRequest(nsIHttpChannel *aHttpChannel)
{
    // ignored
    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchService::OnExamineResponse(nsIHttpChannel *aHttpChannel)
{
    // look for Link: rel=next href=http://foo.com/blah
    
    nsCAutoString linkVal;
    aHttpChannel->GetResponseHeader(NS_LITERAL_CSTRING("link"), linkVal);
    if (!linkVal.IsEmpty()) {
        LOG(("nsPrefetchService::OnExamineResponse [Link: %s]\n", linkVal.get()));

        // verify rel=next or rel="next" and extract href value

        const char *it = linkVal.get();
        const char *hrefBeg = nsnull;
        const char *hrefEnd = nsnull;
        PRBool haveRelNext = PR_FALSE;

        for (; *it; ++it) {
            if (*it == ',') {
                haveRelNext = PR_FALSE;
                hrefBeg = nsnull;
                hrefEnd = nsnull;
            }
            // skip over whitespace
            if (*it != ' ' && *it != '\t' && !(haveRelNext && hrefBeg)) {
                // looking for attribute=value
                const char *attrBeg, *attrEnd, *valBeg, *valEnd;
                if (ParseAttrValue(it, attrBeg, attrEnd, valBeg, valEnd)) {
                    if (!haveRelNext && !PL_strncasecmp(attrBeg, "rel", attrEnd - attrBeg))
                        haveRelNext = PR_TRUE;
                    else if (!hrefBeg && !PL_strncasecmp(attrBeg, "href", attrEnd - attrBeg)) {
                        hrefBeg = valBeg;
                        hrefEnd = valEnd;
                    }
                }
                if (haveRelNext && hrefBeg) {
                    // ok, we got something to prefetch...
                    nsCOMPtr<nsIURI> uri, baseURI;
                    aHttpChannel->GetURI(getter_AddRefs(baseURI));
                    NS_NewURI(getter_AddRefs(uri),
                              Substring(hrefBeg, hrefEnd),
                              nsnull, baseURI);
                    if (uri)
                        PrefetchURI(uri);
                }
                continue; // do not increment
            }
        }
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService::nsIElementObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchService::Notify(PRUint32 aDocumentID, eHTMLTags aTag,
                          PRUint32 numOfAttributes, const PRUnichar* nameArray[],
                          const PRUnichar* valueArray[])
{
    // ignored
    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchService::Notify(PRUint32 aDocumentID, const PRUnichar* aTag,
                          PRUint32 numOfAttributes, const PRUnichar* nameArray[],
                          const PRUnichar *valueArray[])
{
    // ignored
    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchService::Notify(nsISupports *aWebShell,
                          nsISupports *aChannel,
                          const PRUnichar *aTag,
                          const nsStringArray *aKeys,
                          const nsStringArray *aValues,
                          const PRUint32 aFlags)
{
    LOG(("nsPrefetchService::Notify\n"));

    PRInt32 count = aKeys->Count();

    nsCOMPtr<nsIURI> uri;
    PRBool relNext = PR_FALSE;

    // check for <link rel=next href="http://blah">
    for (PRInt32 i=0; i<count; ++i) {
        nsString *key = aKeys->StringAt(i);
        if (!relNext && key->EqualsIgnoreCase("rel")) {
            if (aValues->StringAt(i)->EqualsIgnoreCase("next")) {
                relNext = PR_TRUE;
                if (uri)
                    break;
            }
        }
        else if (!uri && key->EqualsIgnoreCase("href")) {
            nsCOMPtr<nsIURI> baseURI;
            nsCOMPtr<nsIChannel> channel(do_QueryInterface(aChannel));
            if (channel) {
                channel->GetURI(getter_AddRefs(baseURI));
                // need to pass document charset to necko...
                nsCAutoString docCharset;
                GetDocumentCharset(aWebShell, docCharset);
                NS_NewURI(getter_AddRefs(uri), *aValues->StringAt(i),
                          docCharset.IsEmpty() ? nsnull : docCharset.get(),
                          baseURI);
                if (uri && relNext)
                    break;
            }
        }
    }

    if (relNext && uri)
        PrefetchURI(uri);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService::nsIWebProgressListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchService::OnProgressChange(nsIWebProgress *aProgress,
                                  nsIRequest *aRequest, 
                                  PRInt32 curSelfProgress, 
                                  PRInt32 maxSelfProgress, 
                                  PRInt32 curTotalProgress, 
                                  PRInt32 maxTotalProgress)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP 
nsPrefetchService::OnStateChange(nsIWebProgress* aWebProgress, 
                                 nsIRequest *aRequest, 
                                 PRUint32 progressStateFlags, 
                                 nsresult aStatus)
{
    if (progressStateFlags & STATE_IS_DOCUMENT) {
        if (progressStateFlags & STATE_STOP)
            StartPrefetching();
        else if (progressStateFlags & STATE_START)
            StopPrefetching();
    }
            
    return NS_OK;
}


NS_IMETHODIMP
nsPrefetchService::OnLocationChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    nsIURI *location)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP 
nsPrefetchService::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  nsresult aStatus,
                                  const PRUnichar* aMessage)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP 
nsPrefetchService::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRUint32 state)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchService::Observe(nsISupports     *aSubject,
                           const char      *aTopic,
                           const PRUnichar *aData)
{
    LOG(("nsPrefetchService::Observe [topic=%s]\n", aTopic));

    if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
        StopPrefetching();
        mDisabled = PR_TRUE;
    }

    return NS_OK;
}

// vim: ts=4 sw=4 expandtab
