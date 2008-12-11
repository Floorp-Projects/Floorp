/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
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

#include "imgLoader.h"

#include "nsCOMPtr.h"

#include "nsNetUtil.h"
#include "nsIHttpChannel.h"
#include "nsICachingChannel.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsIFileURL.h"
#include "nsThreadUtils.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"

#include "netCore.h"

#include "imgRequest.h"
#include "imgRequestProxy.h"

#include "ImageErrors.h"
#include "ImageLogging.h"

#include "nsIComponentRegistrar.h"

// we want to explore making the document own the load group
// so we can associate the document URI with the load group.
// until this point, we have an evil hack:
#include "nsIHttpChannelInternal.h"  

#if defined(DEBUG_pavlov) || defined(DEBUG_timeless)
#include "nsISimpleEnumerator.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsComponentManagerUtils.h"

static void PrintImageDecoders()
{
  nsCOMPtr<nsIComponentRegistrar> compMgr;
  if (NS_FAILED(NS_GetComponentRegistrar(getter_AddRefs(compMgr))) || !compMgr)
    return;
  nsCOMPtr<nsISimpleEnumerator> enumer;
  if (NS_FAILED(compMgr->EnumerateContractIDs(getter_AddRefs(enumer))) || !enumer)
    return;
  
  nsCString str;
  nsCOMPtr<nsISupports> s;
  PRBool more = PR_FALSE;
  while (NS_SUCCEEDED(enumer->HasMoreElements(&more)) && more) {
    enumer->GetNext(getter_AddRefs(s));
    if (s) {
      nsCOMPtr<nsISupportsCString> ss(do_QueryInterface(s));

      nsCAutoString xcs;
      ss->GetData(xcs);

      NS_NAMED_LITERAL_CSTRING(decoderContract, "@mozilla.org/image/decoder;2?type=");

      if (StringBeginsWith(xcs, decoderContract)) {
        printf("Have decoder for mime type: %s\n", xcs.get()+decoderContract.Length());
      }
    }
  }
}
#endif

static PRBool NewRequestAndEntry(nsIURI *uri, imgRequest **request, imgCacheEntry **entry)
{
  // If file, force revalidation on expiration
  PRBool isFile;
  uri->SchemeIs("file", &isFile);

  *request = new imgRequest();
  if (!*request)
    return PR_FALSE;

  *entry = new imgCacheEntry(*request, /* mustValidateIfExpired = */ isFile);
  if (!*entry) {
    delete *request;
    return PR_FALSE;
  }

  NS_ADDREF(*request);
  NS_ADDREF(*entry);

  return PR_TRUE;
}

static PRBool ShouldRevalidateEntry(imgCacheEntry *aEntry,
                              nsLoadFlags aFlags,
                              PRBool aHasExpired)
{
  PRBool bValidateEntry = PR_FALSE;

  if (aFlags & nsIRequest::LOAD_BYPASS_CACHE)
    return PR_FALSE;

  if (aFlags & nsIRequest::VALIDATE_ALWAYS) {
    bValidateEntry = PR_TRUE;
  }
  //
  // The cache entry has expired...  Determine whether the stale cache
  // entry can be used without validation...
  //
  else if (aHasExpired) {
    //
    // VALIDATE_NEVER and VALIDATE_ONCE_PER_SESSION allow stale cache
    // entries to be used unless they have been explicitly marked to
    // indicate that revalidation is necessary.
    //
    if (aFlags & (nsIRequest::VALIDATE_NEVER | 
                  nsIRequest::VALIDATE_ONCE_PER_SESSION)) 
    {
      bValidateEntry = aEntry->GetMustValidateIfExpired();
    }
    //
    // LOAD_FROM_CACHE allows a stale cache entry to be used... Otherwise,
    // the entry must be revalidated.
    //
    else if (!(aFlags & nsIRequest::LOAD_FROM_CACHE)) {
      bValidateEntry = PR_TRUE;
    }
  }

  return bValidateEntry;
}

static nsresult NewImageChannel(nsIChannel **aResult,
                                nsIURI *aURI,
                                nsIURI *aInitialDocumentURI,
                                nsIURI *aReferringURI,
                                nsILoadGroup *aLoadGroup,
                                nsLoadFlags aLoadFlags)
{
  nsresult rv;
  nsCOMPtr<nsIChannel> newChannel;
  nsCOMPtr<nsIHttpChannel> newHttpChannel;
 
  nsCOMPtr<nsIInterfaceRequestor> callbacks;

  if (aLoadGroup) {
    // Get the notification callbacks from the load group for the new channel.
    //
    // XXX: This is not exactly correct, because the network request could be
    //      referenced by multiple windows...  However, the new channel needs
    //      something.  So, using the 'first' notification callbacks is better
    //      than nothing...
    //
    aLoadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
  }

  // Pass in a NULL loadgroup because this is the underlying network request.
  // This request may be referenced by several proxy image requests (psossibly
  // in different documents).
  // If all of the proxy requests are canceled then this request should be
  // canceled too.
  //
  rv = NS_NewChannel(aResult,
                     aURI,        // URI 
                     nsnull,      // Cached IOService
                     nsnull,      // LoadGroup
                     callbacks,   // Notification Callbacks
                     aLoadFlags);
  if (NS_FAILED(rv))
    return rv;

  // Initialize HTTP-specific attributes
  newHttpChannel = do_QueryInterface(*aResult);
  if (newHttpChannel) {
    newHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                     NS_LITERAL_CSTRING("image/png,image/*;q=0.8,*/*;q=0.5"),
                                     PR_FALSE);

    nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal = do_QueryInterface(newHttpChannel);
    NS_ENSURE_TRUE(httpChannelInternal, NS_ERROR_UNEXPECTED);
    httpChannelInternal->SetDocumentURI(aInitialDocumentURI);
    newHttpChannel->SetReferrer(aReferringURI);
  }

  // Image channels are loaded by default with reduced priority.
  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(*aResult);
  if (p) {
    PRUint32 priority = nsISupportsPriority::PRIORITY_LOW;

    if (aLoadFlags & nsIRequest::LOAD_BACKGROUND)
      ++priority; // further reduce priority for background loads

    p->AdjustPriority(priority);
  }

  return NS_OK;
}

static PRUint32 SecondsFromPRTime(PRTime prTime)
{
  return PRUint32(PRInt64(prTime) / PRInt64(PR_USEC_PER_SEC));
}

imgCacheEntry::imgCacheEntry(imgRequest *request, PRBool mustValidateIfExpired /* = PR_FALSE */)
 : mRequest(request),
   mDataSize(0),
   mTouchedTime(SecondsFromPRTime(PR_Now())),
   mExpiryTime(0),
   mMustValidateIfExpired(mustValidateIfExpired),
   mEvicted(PR_FALSE)
{}

void imgCacheEntry::TouchWithSize(PRInt32 diff)
{
  LOG_SCOPE(gImgLog, "imgCacheEntry::TouchWithSize");

  mTouchedTime = SecondsFromPRTime(PR_Now());

  if (!Evicted()) {
    nsCOMPtr<nsIURI> uri;
    mRequest->GetURI(getter_AddRefs(uri));
    imgLoader::CacheEntriesChanged(uri, diff);
  }
}

void imgCacheEntry::Touch(PRBool updateTime /* = PR_TRUE */)
{
  LOG_SCOPE(gImgLog, "imgCacheEntry::Touch");

  if (updateTime)
    mTouchedTime = SecondsFromPRTime(PR_Now());

  if (!Evicted()) {
    nsCOMPtr<nsIURI> uri;
    mRequest->GetURI(getter_AddRefs(uri));
    imgLoader::CacheEntriesChanged(uri);
  }
}

imgCacheQueue::imgCacheQueue()
 : mDirty(PR_FALSE),
   mSize(0)
{}

void imgCacheQueue::UpdateSize(PRInt32 diff)
{
  mSize += diff;
}

PRUint32 imgCacheQueue::GetSize() const
{
  return mSize;
}

#include <algorithm>
using namespace std;

void imgCacheQueue::Remove(imgCacheEntry *entry)
{
  queueContainer::iterator it = find(mQueue.begin(), mQueue.end(), entry);
  if (it != mQueue.end()) {
    mSize -= (*it)->GetDataSize();
    mQueue.erase(it);
    MarkDirty();
  }
}

void imgCacheQueue::Push(imgCacheEntry *entry)
{
  mSize += entry->GetDataSize();

  nsRefPtr<imgCacheEntry> refptr(entry);
  mQueue.push_back(refptr);
  MarkDirty();
}

already_AddRefed<imgCacheEntry> imgCacheQueue::Pop()
{
  if (mQueue.empty())
    return nsnull;
  if (IsDirty())
    Refresh();

  nsRefPtr<imgCacheEntry> entry = mQueue[0];
  std::pop_heap(mQueue.begin(), mQueue.end(), imgLoader::CompareCacheEntries);
  mQueue.pop_back();

  mSize -= entry->GetDataSize();
  imgCacheEntry *ret = entry;
  NS_ADDREF(ret);
  return ret;
}

void imgCacheQueue::Refresh()
{
  std::make_heap(mQueue.begin(), mQueue.end(), imgLoader::CompareCacheEntries);
  mDirty = PR_FALSE;
}

void imgCacheQueue::MarkDirty()
{
  mDirty = PR_TRUE;
}

PRBool imgCacheQueue::IsDirty()
{
  return mDirty;
}

PRUint32 imgCacheQueue::GetNumElements() const
{
  return mQueue.size();
}

imgCacheQueue::iterator imgCacheQueue::begin()
{
  return mQueue.begin();
}
imgCacheQueue::const_iterator imgCacheQueue::begin() const
{
  return mQueue.begin();
}

imgCacheQueue::iterator imgCacheQueue::end()
{
  return mQueue.end();
}
imgCacheQueue::const_iterator imgCacheQueue::end() const
{
  return mQueue.end();
}

nsresult imgLoader::CreateNewProxyForRequest(imgRequest *aRequest, nsILoadGroup *aLoadGroup,
                                             imgIDecoderObserver *aObserver,
                                             nsLoadFlags aLoadFlags, imgIRequest *aProxyRequest,
                                             imgIRequest **_retval)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgLoader::CreateNewProxyForRequest", "imgRequest", aRequest);

  /* XXX If we move decoding onto separate threads, we should save off the
     calling thread here and pass it off to |proxyRequest| so that it call
     proxy calls to |aObserver|.
   */

  imgRequestProxy *proxyRequest;
  if (aProxyRequest) {
    proxyRequest = static_cast<imgRequestProxy *>(aProxyRequest);
  } else {
    NS_NEWXPCOM(proxyRequest, imgRequestProxy);
    if (!proxyRequest) return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(proxyRequest);

  /* It is important to call |SetLoadFlags()| before calling |Init()| because
     |Init()| adds the request to the loadgroup.
   */
  proxyRequest->SetLoadFlags(aLoadFlags);

  // init adds itself to imgRequest's list of observers
  nsresult rv = proxyRequest->Init(aRequest, aLoadGroup, aObserver);
  if (NS_FAILED(rv)) {
    NS_RELEASE(proxyRequest);
    return rv;
  }

  // transfer reference to caller
  *_retval = static_cast<imgIRequest*>(proxyRequest);

  return NS_OK;
}

class imgCacheObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
private:
  imgLoader mLoader;
};

NS_IMPL_ISUPPORTS1(imgCacheObserver, nsIObserver)

NS_IMETHODIMP
imgCacheObserver::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aSomeData)
{
  if (strcmp(aTopic, "memory-pressure") == 0) {
    mLoader.ClearCache(PR_FALSE);
    mLoader.ClearCache(PR_TRUE);
  } else if (strcmp(aTopic, "chrome-flush-skin-caches") == 0 ||
             strcmp(aTopic, "chrome-flush-caches") == 0) {
    mLoader.ClearCache(PR_TRUE);
  }
  return NS_OK;
}

class imgCacheExpirationTracker : public nsExpirationTracker<imgCacheEntry, 3>
{
  enum { TIMEOUT_SECONDS = 10 };
public:
  imgCacheExpirationTracker();

protected:
  void NotifyExpired(imgCacheEntry *entry);
};

imgCacheExpirationTracker::imgCacheExpirationTracker()
 : nsExpirationTracker<imgCacheEntry, 3>(TIMEOUT_SECONDS * 1000)
{}

void imgCacheExpirationTracker::NotifyExpired(imgCacheEntry *entry)
{
  // We can be called multiple times on the same entry. Don't do work multiple
  // times.
  if (!entry->Evicted())
    imgLoader::RemoveFromCache(entry);

  imgLoader::VerifyCacheSizes();
}

imgCacheObserver *gCacheObserver;
imgCacheExpirationTracker *gCacheTracker;

imgLoader::imgCacheTable imgLoader::sCache;
imgCacheQueue imgLoader::sCacheQueue;

imgLoader::imgCacheTable imgLoader::sChromeCache;
imgCacheQueue imgLoader::sChromeCacheQueue;

PRFloat64 imgLoader::sCacheTimeWeight;
PRUint32 imgLoader::sCacheMaxSize;

NS_IMPL_ISUPPORTS4(imgLoader, imgILoader, nsIContentSniffer, imgICache, nsISupportsWeakReference)

imgLoader::imgLoader()
{
  /* member initializers and constructor code */
#ifdef DEBUG_pavlov
  PrintImageDecoders();
#endif
}

imgLoader::~imgLoader()
{
  /* destructor code */
}

void imgLoader::VerifyCacheSizes()
{
  if (!gCacheTracker)
    return;

  PRUint32 queuesize = sCacheQueue.GetNumElements() + sChromeCacheQueue.GetNumElements();
  PRUint32 cachesize = sCache.Count() + sChromeCache.Count();
  PRUint32 trackersize = 0;
  for (nsExpirationTracker<imgCacheEntry, 3>::Iterator it(gCacheTracker); it.Next(); )
    trackersize++;
  NS_ASSERTION(queuesize == cachesize, "Queue and cache sizes out of sync!");
  NS_ASSERTION(queuesize == trackersize, "Queue and tracker sizes out of sync!");
}

imgLoader::imgCacheTable & imgLoader::GetCache(nsIURI *aURI)
{
  PRBool chrome = PR_FALSE;
  aURI->SchemeIs("chrome", &chrome);
  if (chrome)
    return sChromeCache;
  else
    return sCache;
}

imgCacheQueue & imgLoader::GetCacheQueue(nsIURI *aURI)
{
  PRBool chrome = PR_FALSE;
  aURI->SchemeIs("chrome", &chrome);
  if (chrome)
    return sChromeCacheQueue;
  else
    return sCacheQueue;
}

nsresult imgLoader::InitCache()
{
  nsresult rv;
  nsCOMPtr<nsIObserverService> os = do_GetService("@mozilla.org/observer-service;1", &rv);
  if (NS_FAILED(rv))
    return rv;
  
  gCacheObserver = new imgCacheObserver();
  if (!gCacheObserver) 
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(gCacheObserver);

  os->AddObserver(gCacheObserver, "memory-pressure", PR_FALSE);
  os->AddObserver(gCacheObserver, "chrome-flush-skin-caches", PR_FALSE);
  os->AddObserver(gCacheObserver, "chrome-flush-caches", PR_FALSE);

  gCacheTracker = new imgCacheExpirationTracker();
  if (!gCacheTracker)
    return NS_ERROR_OUT_OF_MEMORY;

  if (!sCache.Init())
      return NS_ERROR_OUT_OF_MEMORY;
  if (!sChromeCache.Init())
      return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv); 
  if (NS_FAILED(rv))
    return rv;

  PRInt32 timeweight;
  rv = prefs->GetIntPref("image.cache.timeweight", &timeweight);
  if (NS_SUCCEEDED(rv))
    sCacheTimeWeight = timeweight / 1000.0;
  else
    sCacheTimeWeight = 0.5;

  PRInt32 cachesize;
  rv = prefs->GetIntPref("image.cache.size", &cachesize);
  if (NS_SUCCEEDED(rv))
    sCacheMaxSize = cachesize;
  else
    sCacheMaxSize = 5 * 1024 * 1024;
 
  return NS_OK;
}

/* void clearCache (in boolean chrome); */
NS_IMETHODIMP imgLoader::ClearCache(PRBool chrome)
{
  if (chrome)
    return ClearChromeImageCache();
  else
    return ClearImageCache();
}

/* void removeEntry(in nsIURI uri); */
NS_IMETHODIMP imgLoader::RemoveEntry(nsIURI *uri)
{
  if (RemoveFromCache(uri))
    return NS_OK;

  return NS_ERROR_NOT_AVAILABLE;
}

/* imgIRequest findEntry(in nsIURI uri); */
NS_IMETHODIMP imgLoader::FindEntryProperties(nsIURI *uri, nsIProperties **_retval)
{
  nsRefPtr<imgCacheEntry> entry;
  nsCAutoString spec;
  imgCacheTable &cache = GetCache(uri);

  uri->GetSpec(spec);
  *_retval = nsnull;

  if (cache.Get(spec, getter_AddRefs(entry)) && entry) {
    if (gCacheTracker)
      gCacheTracker->MarkUsed(entry);
    nsRefPtr<imgRequest> request = getter_AddRefs(entry->GetRequest());
    if (request) {
      *_retval = request->Properties();
      NS_ADDREF(*_retval);
    }
  }

  return NS_OK;
}

void imgLoader::Shutdown()
{
  ClearChromeImageCache();
  ClearImageCache();
  NS_IF_RELEASE(gCacheObserver);
  delete gCacheTracker;
  gCacheTracker = nsnull;
}

nsresult imgLoader::ClearChromeImageCache()
{
  return EvictEntries(sChromeCache, sChromeCacheQueue);
}

nsresult imgLoader::ClearImageCache()
{
  return EvictEntries(sCache, sCacheQueue);
}

PRBool imgLoader::PutIntoCache(nsIURI *key, imgCacheEntry *entry)
{
  LOG_STATIC_FUNC(gImgLog, "imgLoader::PutIntoCache");

  imgCacheTable &cache = GetCache(key);

  nsCAutoString spec;
  key->GetSpec(spec);

  // Check to see if this request already exists in the cache and is being
  // loaded on a different thread. If so, don't allow this entry to be added to
  // the cache.
  nsRefPtr<imgCacheEntry> tmpCacheEntry;
  if (cache.Get(spec, getter_AddRefs(tmpCacheEntry)) && tmpCacheEntry) {
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::PutIntoCache -- Element already in the cache", nsnull));
    nsRefPtr<imgRequest> tmpRequest = getter_AddRefs(tmpCacheEntry->GetRequest());
    void *cacheId = NS_GetCurrentThread();

    if (!tmpRequest->IsReusable(cacheId))
      return PR_FALSE;

    // If it already exists, and we're putting the same key into the cache, we
    // should remove the old version.
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::PutIntoCache -- Replacing cached element", nsnull));

    RemoveFromCache(key);
  } else {
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::PutIntoCache -- Element NOT already in the cache", nsnull));
  }

  if (!cache.Put(spec, entry))
    return PR_FALSE;

  imgCacheQueue &queue = GetCacheQueue(key);
  queue.Push(entry);

  if (gCacheTracker)
    gCacheTracker->AddObject(entry);

  CheckCacheLimits(cache, queue);

  return PR_TRUE;
}

void imgLoader::CacheEntriesChanged(nsIURI *uri, PRInt32 sizediff /* = 0 */)
{
  imgCacheQueue &queue = GetCacheQueue(uri);
  queue.MarkDirty();
  queue.UpdateSize(sizediff);
}

void imgLoader::CheckCacheLimits(imgCacheTable &cache, imgCacheQueue &queue)
{
  if (queue.GetNumElements() == 0)
    NS_ASSERTION(queue.GetSize() == 0, 
                 "imgLoader::CheckCacheLimits -- incorrect cache size");

  // Remove entries from the cache until we're back under our desired size.
  while (queue.GetSize() >= sCacheMaxSize) {
    // Remove the first entry in the queue.
    nsRefPtr<imgCacheEntry> entry(queue.Pop());

    NS_ASSERTION(entry, "imgLoader::CheckCacheLimits -- NULL entry pointer");

    if (entry)
      RemoveFromCache(entry);
  }
}

PRBool imgLoader::ValidateRequestWithNewChannel(imgRequest *request,
                                                nsIURI *aURI,
                                                nsIURI *aInitialDocumentURI,
                                                nsIURI *aReferrerURI,
                                                nsILoadGroup *aLoadGroup,
                                                imgIDecoderObserver *aObserver,
                                                nsISupports *aCX,
                                                nsLoadFlags aLoadFlags,
                                                imgIRequest *aExistingRequest,
                                                imgIRequest **aProxyRequest)
{
  // now we need to insert a new channel request object inbetween the real
  // request and the proxy that basically delays loading the image until it
  // gets a 304 or figures out that this needs to be a new request

  nsresult rv;

  if (request->mValidator) {
    rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver,
                                  aLoadFlags, aExistingRequest, 
                                  reinterpret_cast<imgIRequest **>(aProxyRequest));

    if (*aProxyRequest)
      request->mValidator->AddProxy(static_cast<imgRequestProxy*>(*aProxyRequest));

    return NS_SUCCEEDED(rv);

  } else {
    nsCOMPtr<nsIChannel> newChannel;
    rv = NewImageChannel(getter_AddRefs(newChannel),
                         aURI,
                         aInitialDocumentURI,
                         aReferrerURI,
                         aLoadGroup,
                         aLoadFlags);
    if (NS_FAILED(rv)) {
      return PR_FALSE;
    }

    nsCOMPtr<nsICachingChannel> cacheChan(do_QueryInterface(newChannel));

    if (cacheChan) {
      // since this channel supports nsICachingChannel, we can ask it
      // to only stream us data if the data comes off the net.
      PRUint32 loadFlags;
      if (NS_SUCCEEDED(newChannel->GetLoadFlags(&loadFlags)))
        newChannel->SetLoadFlags(loadFlags | nsICachingChannel::LOAD_ONLY_IF_MODIFIED);
    }

    nsCOMPtr<imgIRequest> req;
    rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver,
                                  aLoadFlags, aExistingRequest, getter_AddRefs(req));
    if (NS_FAILED(rv)) {
      return PR_FALSE;
    }

    imgCacheValidator *hvc = new imgCacheValidator(request, aCX);
    if (!hvc) {
      return PR_FALSE;
    }

    NS_ADDREF(hvc);
    request->mValidator = hvc;

    hvc->AddProxy(static_cast<imgRequestProxy*>
                             (static_cast<imgIRequest*>(req.get())));

    rv = newChannel->AsyncOpen(static_cast<nsIStreamListener *>(hvc), nsnull);
    if (NS_SUCCEEDED(rv))
      NS_ADDREF(*aProxyRequest = req.get());

    NS_RELEASE(hvc);

    return NS_SUCCEEDED(rv);
  }
}

PRBool imgLoader::ValidateEntry(imgCacheEntry *aEntry,
                                nsIURI *aURI,
                                nsIURI *aInitialDocumentURI,
                                nsIURI *aReferrerURI,
                                nsILoadGroup *aLoadGroup,
                                imgIDecoderObserver *aObserver,
                                nsISupports *aCX,
                                nsLoadFlags aLoadFlags,
                                PRBool aCanMakeNewChannel,
                                imgIRequest *aExistingRequest,
                                imgIRequest **aProxyRequest)
{
  LOG_SCOPE(gImgLog, "imgLoader::ValidateEntry");

  PRBool hasExpired;
  PRUint32 expirationTime = aEntry->GetExpiryTime();
  if (expirationTime <= SecondsFromPRTime(PR_Now())) {
    hasExpired = PR_TRUE;
  } else {
    hasExpired = PR_FALSE;
  }

  nsresult rv;

  // Special treatment for file URLs - aEntry has expired if file has changed
  nsCOMPtr<nsIFileURL> fileUrl(do_QueryInterface(aURI));
  if (fileUrl) {
    PRUint32 lastModTime = aEntry->GetTouchedTime();

    nsCOMPtr<nsIFile> theFile;
    rv = fileUrl->GetFile(getter_AddRefs(theFile));
    if (NS_SUCCEEDED(rv)) {
      PRInt64 fileLastMod;
      rv = theFile->GetLastModifiedTime(&fileLastMod);
      if (NS_SUCCEEDED(rv)) {
        // nsIFile uses millisec, NSPR usec
        fileLastMod *= 1000;
        hasExpired = SecondsFromPRTime((PRTime)fileLastMod) > lastModTime;
      }
    }
  }

  nsRefPtr<imgRequest> request(aEntry->GetRequest());

  if (!request)
    return PR_FALSE;

  PRBool validateRequest = PR_FALSE;

  // If the request's loadId is the same as the aCX, then it is ok to use
  // this one because it has already been validated for this context.
  //
  // XXX: nsnull seems to be a 'special' key value that indicates that NO
  //      validation is required.
  //
  void *key = (void *)aCX;
  if (request->mLoadId != key) {
    // Determine whether the cache aEntry must be revalidated...
    validateRequest = ShouldRevalidateEntry(aEntry, aLoadFlags, hasExpired);

    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("imgLoader::ValidateEntry validating cache entry. " 
            "validateRequest = %d", validateRequest));
  }
#if defined(PR_LOGGING)
  else if (!key) {
    nsCAutoString spec;
    aURI->GetSpec(spec);

    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("imgLoader::ValidateEntry BYPASSING cache validation for %s " 
            "because of NULL LoadID", spec.get()));
  }
#endif

  //
  // Get the current thread...  This is used as a cacheId to prevent
  // sharing requests which are being loaded across multiple threads...
  //
  void *cacheId = NS_GetCurrentThread();
  if (!request->IsReusable(cacheId)) {
    //
    // The current request is still being loaded and lives on a different
    // event queue.
    //
    // Since its event queue is NOT active, do not reuse this imgRequest.
    // PutIntoCache() will also ensure that we don't cache it.
    //
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("imgLoader::ValidateEntry -- DANGER!! Unable to use cached "
            "imgRequest [request=%p]\n", address_of(request)));

    return PR_FALSE;
  }

  if (validateRequest && aCanMakeNewChannel) {
    LOG_SCOPE(gImgLog, "imgLoader::ValidateRequest |cache hit| must validate");

    return ValidateRequestWithNewChannel(request, aURI, aInitialDocumentURI,
                                         aReferrerURI, aLoadGroup, aObserver,
                                         aCX, aLoadFlags, aExistingRequest,
                                         aProxyRequest);
  } 

  return !validateRequest;
}


PRBool imgLoader::RemoveFromCache(nsIURI *aKey)
{
  LOG_STATIC_FUNC(gImgLog, "imgLoader::RemoveFromCache uri");
  if (!aKey) return PR_FALSE;

  imgCacheTable &cache = GetCache(aKey);
  imgCacheQueue &queue = GetCacheQueue(aKey);

  nsCAutoString spec;
  aKey->GetSpec(spec);

  nsRefPtr<imgCacheEntry> entry;
  if (cache.Get(spec, getter_AddRefs(entry)) && entry) {
    if (gCacheTracker)
      gCacheTracker->RemoveObject(entry);
    cache.Remove(spec);
    queue.Remove(entry);
    entry->SetEvicted(PR_TRUE);
    return PR_TRUE;
  }
  else
    return PR_FALSE;
}

PRBool imgLoader::RemoveFromCache(imgCacheEntry *entry)
{
  LOG_STATIC_FUNC(gImgLog, "imgLoader::RemoveFromCache entry");
  PRBool ret = PR_FALSE;
  nsRefPtr<imgRequest> request(getter_AddRefs(entry->GetRequest()));
  if (request) {
    nsCOMPtr<nsIURI> key;
    if (NS_SUCCEEDED(request->GetURI(getter_AddRefs(key))) && key)
      ret = RemoveFromCache(key);
  }

  return ret;
}

nsresult imgLoader::EvictEntries(imgCacheTable &aCacheToClear, imgCacheQueue &aQueueToClear)
{
  LOG_STATIC_FUNC(gImgLog, "imgLoader::EvictEntries");

  // We have to make a temporary, since RemoveFromCache removes the element
  // from the queue, invalidating iterators.
  nsTArray<nsRefPtr<imgCacheEntry> > entries;
  for (imgCacheQueue::iterator it = aQueueToClear.begin(); it != aQueueToClear.end(); ++it)
    entries.AppendElement(*it);
  
  for (PRUint32  i = 0; i < entries.Length(); ++i)
    if (!RemoveFromCache(entries[i]))
      return NS_ERROR_FAILURE;

  return NS_OK;
}

#define LOAD_FLAGS_CACHE_MASK    (nsIRequest::LOAD_BYPASS_CACHE | \
                                  nsIRequest::LOAD_FROM_CACHE)

#define LOAD_FLAGS_VALIDATE_MASK (nsIRequest::VALIDATE_ALWAYS |   \
                                  nsIRequest::VALIDATE_NEVER |    \
                                  nsIRequest::VALIDATE_ONCE_PER_SESSION)


/* imgIRequest loadImage (in nsIURI aURI, in nsIURI initialDocumentURI, in nsILoadGroup aLoadGroup, in imgIDecoderObserver aObserver, in nsISupports aCX, in nsLoadFlags aLoadFlags, in nsISupports cacheKey, in imgIRequest aRequest); */

NS_IMETHODIMP imgLoader::LoadImage(nsIURI *aURI, 
                                   nsIURI *aInitialDocumentURI,
                                   nsIURI *aReferrerURI,
                                   nsILoadGroup *aLoadGroup,
                                   imgIDecoderObserver *aObserver,
                                   nsISupports *aCX,
                                   nsLoadFlags aLoadFlags,
                                   nsISupports *aCacheKey,
                                   imgIRequest *aRequest,
                                   imgIRequest **_retval)
{
  VerifyCacheSizes();

  NS_ASSERTION(aURI, "imgLoader::LoadImage -- NULL URI pointer");

  if (!aURI)
    return NS_ERROR_NULL_POINTER;

#if defined(PR_LOGGING)
  nsCAutoString spec;
  aURI->GetSpec(spec);
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgLoader::LoadImage", "aURI", spec.get());
#endif

  *_retval = nsnull;

  nsRefPtr<imgRequest> request;

  nsresult rv;
  nsLoadFlags requestFlags = nsIRequest::LOAD_NORMAL;

  // Get the default load flags from the loadgroup (if possible)...
  if (aLoadGroup) {
    aLoadGroup->GetLoadFlags(&requestFlags);
  }
  //
  // Merge the default load flags with those passed in via aLoadFlags.
  // Currently, *only* the caching, validation and background load flags
  // are merged...
  //
  // The flags in aLoadFlags take precedence over the default flags!
  //
  if (aLoadFlags & LOAD_FLAGS_CACHE_MASK) {
    // Override the default caching flags...
    requestFlags = (requestFlags & ~LOAD_FLAGS_CACHE_MASK) |
                   (aLoadFlags & LOAD_FLAGS_CACHE_MASK);
  }
  if (aLoadFlags & LOAD_FLAGS_VALIDATE_MASK) {
    // Override the default validation flags...
    requestFlags = (requestFlags & ~LOAD_FLAGS_VALIDATE_MASK) |
                   (aLoadFlags & LOAD_FLAGS_VALIDATE_MASK);
  }
  if (aLoadFlags & nsIRequest::LOAD_BACKGROUND) {
    // Propagate background loading...
    requestFlags |= nsIRequest::LOAD_BACKGROUND;
  }

  nsRefPtr<imgCacheEntry> entry;

  // If we're bypassing the cache, we are guaranteed to want a new cache entry,
  // since we're going to do a new load.
  if (aLoadFlags & nsIRequest::LOAD_BYPASS_CACHE) {
    RemoveFromCache(aURI);
  } else {
    // Look in the cache for our URI, and then validate it.
    // XXX For now ignore aCacheKey. We will need it in the future
    // for correctly dealing with image load requests that are a result
    // of post data.
    imgCacheTable &cache = GetCache(aURI);
    nsCAutoString spec;

    aURI->GetSpec(spec);

    if (cache.Get(spec, getter_AddRefs(entry)) && entry) {
      if (gCacheTracker)
        gCacheTracker->MarkUsed(entry);

      if (ValidateEntry(entry, aURI, aInitialDocumentURI, aReferrerURI, aLoadGroup, aObserver, aCX,
                        requestFlags, PR_TRUE, aRequest, _retval)) {
        request = getter_AddRefs(entry->GetRequest());

        entry->Touch();
#ifdef DEBUG_joe
        printf("CACHEGET: %d %s %d\n", time(NULL), spec.get(), entry->GetDataSize());
#endif
      }
      else
        entry = nsnull;
    }
  }

  // If we didn't get a cache hit, we need to load from the network.
  if (!request) {
    LOG_SCOPE(gImgLog, "imgLoader::LoadImage |cache miss|");

    nsCOMPtr<nsIChannel> newChannel;
    rv = NewImageChannel(getter_AddRefs(newChannel),
                         aURI,
                         aInitialDocumentURI,
                         aReferrerURI,
                         aLoadGroup,
                         requestFlags);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;

    if (!NewRequestAndEntry(aURI, getter_AddRefs(request), getter_AddRefs(entry)))
      return NS_ERROR_OUT_OF_MEMORY;

    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::LoadImage -- Created new imgRequest [request=%p]\n", this, request.get()));

    // Create a loadgroup for this new channel.  This way if the channel
    // is redirected, we'll have a way to cancel the resulting channel.
    nsCOMPtr<nsILoadGroup> loadGroup =
        do_CreateInstance(NS_LOADGROUP_CONTRACTID);
    newChannel->SetLoadGroup(loadGroup);

    void *cacheId = NS_GetCurrentThread();
    request->Init(aURI, loadGroup, entry, cacheId, aCX);

    // create the proxy listener
    ProxyListener *pl = new ProxyListener(static_cast<nsIStreamListener *>(request.get()));
    if (!pl) {
      request->Cancel(NS_ERROR_OUT_OF_MEMORY);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(pl);

    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::LoadImage -- Calling channel->AsyncOpen()\n", this));

    nsresult openRes = newChannel->AsyncOpen(static_cast<nsIStreamListener *>(pl), nsnull);

    NS_RELEASE(pl);

    if (NS_FAILED(openRes)) {
      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("[this=%p] imgLoader::LoadImage -- AsyncOpen() failed: 0x%x\n",
              this, openRes));
      request->Cancel(openRes);
      return openRes;
    }

    // Try to add the new request into the cache.
    PutIntoCache(aURI, entry);

  // If we did get a cache hit, use it.
  } else {
    // XXX: Should this be executed if an expired cache entry does not have a caching channel??
    LOG_MSG_WITH_PARAM(gImgLog, 
                       "imgLoader::LoadImage |cache hit|", "request", request);

    // Update the request's LoadId
    request->SetLoadId(aCX);
  }

  // If we didn't get a proxy when validating the cache entry, we need to create one.
  if (!*_retval) {
    LOG_MSG(gImgLog, "imgLoader::LoadImage", "creating proxy request.");

    rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver,
                                  requestFlags, aRequest, _retval);
    imgRequestProxy *proxy = static_cast<imgRequestProxy *>(*_retval);

    // Note that it's OK to add here even if the request is done.  If it is,
    // it'll send a OnStopRequest() to the proxy in NotifyProxyListener and the
    // proxy will be removed from the loadgroup.
    proxy->AddToLoadGroup();

    request->NotifyProxyListener(proxy);

    return rv;
  }

  NS_ASSERTION(*_retval, "imgLoader::LoadImage -- no return value");

  return NS_OK;
}

/* imgIRequest loadImageWithChannel(in nsIChannel channel, in imgIDecoderObserver aObserver, in nsISupports cx, out nsIStreamListener); */
NS_IMETHODIMP imgLoader::LoadImageWithChannel(nsIChannel *channel, imgIDecoderObserver *aObserver, nsISupports *aCX, nsIStreamListener **listener, imgIRequest **_retval)
{
  NS_ASSERTION(channel, "imgLoader::LoadImageWithChannel -- NULL channel pointer");

  nsresult rv;
  nsRefPtr<imgRequest> request;

  nsCOMPtr<nsIURI> uri;
  channel->GetURI(getter_AddRefs(uri));

  nsLoadFlags requestFlags = nsIRequest::LOAD_NORMAL;
  channel->GetLoadFlags(&requestFlags);

  nsRefPtr<imgCacheEntry> entry;

  if (requestFlags & nsIRequest::LOAD_BYPASS_CACHE) {
    RemoveFromCache(uri);
  } else {
    // Look in the cache for our URI, and then validate it.
    // XXX For now ignore aCacheKey. We will need it in the future
    // for correctly dealing with image load requests that are a result
    // of post data.
    imgCacheTable &cache = GetCache(uri);
    nsCAutoString spec;

    uri->GetSpec(spec);

    if (cache.Get(spec, getter_AddRefs(entry)) && entry) {
      if (gCacheTracker)
        gCacheTracker->MarkUsed(entry);

      // We don't want to kick off another network load. So we ask
      // ValidateEntry to only do validation without creating a new proxy. If
      // it says that the entry isn't valid any more, we'll only use the entry
      // we're getting if the channel is loading from the cache anyways.
      //
      // XXX -- should this be changed? it's pretty much verbatim from the old
      // code, but seems nonsensical.
      if (ValidateEntry(entry, uri, nsnull, nsnull, nsnull, aObserver, aCX,
                        requestFlags, PR_FALSE, nsnull, nsnull)) {
        request = getter_AddRefs(entry->GetRequest());
      } else {
        nsCOMPtr<nsICachingChannel> cacheChan(do_QueryInterface(channel));
        PRBool bUseCacheCopy;

        if (cacheChan)
          cacheChan->IsFromCache(&bUseCacheCopy);
        else
          bUseCacheCopy = PR_FALSE;

        if (!bUseCacheCopy)
          entry = nsnull;
        else {
          request = getter_AddRefs(entry->GetRequest());
        }
      }
    }
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  channel->GetLoadGroup(getter_AddRefs(loadGroup));

  if (request) {
    // we have this in our cache already.. cancel the current (document) load

    channel->Cancel(NS_IMAGELIB_ERROR_LOAD_ABORTED); // this should fire an OnStopRequest

    *listener = nsnull; // give them back a null nsIStreamListener
  } else {

    // Get the current Thread...  This is used as a cacheId to prevent
    // sharing requests which are being loaded across multiple threads...
    nsIThread *thread = NS_GetCurrentThread();

    NewRequestAndEntry(uri, getter_AddRefs(request), getter_AddRefs(entry));

    // XXX(darin):  I'm not sure that using the original URI is correct here.
    // Perhaps we should use the same URI that indexes the cache?  Or, perhaps
    // the cache should use the original URI?  See bug 89419.
    nsCOMPtr<nsIURI> originalURI;
    channel->GetOriginalURI(getter_AddRefs(originalURI));
    request->Init(originalURI, channel, entry, thread, aCX);

    ProxyListener *pl = new ProxyListener(static_cast<nsIStreamListener *>(request.get()));
    if (!pl)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(pl);

    *listener = static_cast<nsIStreamListener*>(pl);
    NS_ADDREF(*listener);

    NS_RELEASE(pl);

    // Try to add the new request into the cache.
    PutIntoCache(uri, entry);
  }

  // XXX: It looks like the wrong load flags are being passed in...
  requestFlags &= 0xFFFF;

  rv = CreateNewProxyForRequest(request, loadGroup, aObserver,
                                requestFlags, nsnull, _retval);
  request->NotifyProxyListener(static_cast<imgRequestProxy*>(*_retval));

  return rv;
}


NS_IMETHODIMP imgLoader::SupportImageWithMimeType(const char* aMimeType, PRBool *_retval)
{
  *_retval = PR_FALSE;
  nsCOMPtr<nsIComponentRegistrar> reg;
  nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(reg));
  if (NS_FAILED(rv))
    return rv;
  nsCAutoString mimeType(aMimeType);
  ToLowerCase(mimeType);
  nsCAutoString decoderId(NS_LITERAL_CSTRING("@mozilla.org/image/decoder;2?type=") + mimeType);
  return reg->IsContractIDRegistered(decoderId.get(),  _retval);
}

NS_IMETHODIMP imgLoader::GetMIMETypeFromContent(nsIRequest* aRequest,
                                                const PRUint8* aContents,
                                                PRUint32 aLength,
                                                nsACString& aContentType)
{
  return GetMimeTypeFromContent((const char*)aContents, aLength, aContentType);
}

/* static */
nsresult imgLoader::GetMimeTypeFromContent(const char* aContents, PRUint32 aLength, nsACString& aContentType)
{
  /* Is it a GIF? */
  if (aLength >= 4 && !nsCRT::strncmp(aContents, "GIF8", 4))  {
    aContentType.AssignLiteral("image/gif");
  }

  /* or a PNG? */
  else if (aLength >= 4 && ((unsigned char)aContents[0]==0x89 &&
                   (unsigned char)aContents[1]==0x50 &&
                   (unsigned char)aContents[2]==0x4E &&
                   (unsigned char)aContents[3]==0x47))
  { 
    aContentType.AssignLiteral("image/png");
  }

  /* maybe a JPEG (JFIF)? */
  /* JFIF files start with SOI APP0 but older files can start with SOI DQT
   * so we test for SOI followed by any marker, i.e. FF D8 FF
   * this will also work for SPIFF JPEG files if they appear in the future.
   *
   * (JFIF is 0XFF 0XD8 0XFF 0XE0 <skip 2> 0X4A 0X46 0X49 0X46 0X00)
   */
  else if (aLength >= 3 &&
     ((unsigned char)aContents[0])==0xFF &&
     ((unsigned char)aContents[1])==0xD8 &&
     ((unsigned char)aContents[2])==0xFF)
  {
    aContentType.AssignLiteral("image/jpeg");
  }

  /* or how about ART? */
  /* ART begins with JG (4A 47). Major version offset 2.
   * Minor version offset 3. Offset 4 must be NULL.
   */
  else if (aLength >= 5 &&
   ((unsigned char) aContents[0])==0x4a &&
   ((unsigned char) aContents[1])==0x47 &&
   ((unsigned char) aContents[4])==0x00 )
  {
    aContentType.AssignLiteral("image/x-jg");
  }

  else if (aLength >= 2 && !nsCRT::strncmp(aContents, "BM", 2)) {
    aContentType.AssignLiteral("image/bmp");
  }

  // ICOs always begin with a 2-byte 0 followed by a 2-byte 1.
  // CURs begin with 2-byte 0 followed by 2-byte 2.
  else if (aLength >= 4 && (!memcmp(aContents, "\000\000\001\000", 4) ||
                            !memcmp(aContents, "\000\000\002\000", 4))) {
    aContentType.AssignLiteral("image/x-icon");
  }

  else if (aLength >= 8 && !nsCRT::strncmp(aContents, "#define ", 8)) {
    aContentType.AssignLiteral("image/x-xbitmap");
  }
  else {
    /* none of the above?  I give up */
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

/**
 * proxy stream listener class used to handle multipart/x-mixed-replace
 */

#include "nsIRequest.h"
#include "nsIStreamConverterService.h"
#include "nsXPIDLString.h"

NS_IMPL_ISUPPORTS2(ProxyListener, nsIStreamListener, nsIRequestObserver)

ProxyListener::ProxyListener(nsIStreamListener *dest) :
  mDestListener(dest)
{
  /* member initializers and constructor code */
}

ProxyListener::~ProxyListener()
{
  /* destructor code */
}


/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP ProxyListener::OnStartRequest(nsIRequest *aRequest, nsISupports *ctxt)
{
  if (!mDestListener)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel) {
    nsCAutoString contentType;
    nsresult rv = channel->GetContentType(contentType);

    if (!contentType.IsEmpty()) {
     /* If multipart/x-mixed-replace content, we'll insert a MIME decoder
        in the pipeline to handle the content and pass it along to our
        original listener.
      */
      if (NS_LITERAL_CSTRING("multipart/x-mixed-replace").Equals(contentType)) {

        nsCOMPtr<nsIStreamConverterService> convServ(do_GetService("@mozilla.org/streamConverters;1", &rv));
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIStreamListener> toListener(mDestListener);
          nsCOMPtr<nsIStreamListener> fromListener;

          rv = convServ->AsyncConvertData("multipart/x-mixed-replace",
                                          "*/*",
                                          toListener,
                                          nsnull,
                                          getter_AddRefs(fromListener));
          if (NS_SUCCEEDED(rv))
            mDestListener = fromListener;
        }
      }
    }
  }

  return mDestListener->OnStartRequest(aRequest, ctxt);
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status); */
NS_IMETHODIMP ProxyListener::OnStopRequest(nsIRequest *aRequest, nsISupports *ctxt, nsresult status)
{
  if (!mDestListener)
    return NS_ERROR_FAILURE;

  return mDestListener->OnStopRequest(aRequest, ctxt, status);
}

/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP ProxyListener::OnDataAvailable(nsIRequest *aRequest, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  if (!mDestListener)
    return NS_ERROR_FAILURE;

  return mDestListener->OnDataAvailable(aRequest, ctxt, inStr, sourceOffset, count);
}

/**
 * http validate class.  check a channel for a 304
 */

NS_IMPL_ISUPPORTS2(imgCacheValidator, nsIStreamListener, nsIRequestObserver)

imgLoader imgCacheValidator::sImgLoader;

imgCacheValidator::imgCacheValidator(imgRequest *request, void *aContext) :
  mRequest(request),
  mContext(aContext)
{}

imgCacheValidator::~imgCacheValidator()
{
  /* destructor code */
  if (mRequest) {
    mRequest->mValidator = nsnull;
  }
}

void imgCacheValidator::AddProxy(imgRequestProxy *aProxy)
{
  // aProxy needs to be in the loadgroup since we're validating from
  // the network.
  aProxy->AddToLoadGroup();

  mProxies.AppendObject(aProxy);
}

/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP imgCacheValidator::OnStartRequest(nsIRequest *aRequest, nsISupports *ctxt)
{
  nsCOMPtr<nsICachingChannel> cacheChan(do_QueryInterface(aRequest));
  if (cacheChan) {
    PRBool isFromCache;
    if (NS_SUCCEEDED(cacheChan->IsFromCache(&isFromCache)) && isFromCache) {

      PRUint32 count = mProxies.Count();
      for (PRInt32 i = count-1; i>=0; i--) {
        imgRequestProxy *proxy = static_cast<imgRequestProxy *>(mProxies[i]);
        mRequest->NotifyProxyListener(proxy);
      }

      mRequest->SetLoadId(mContext);
      mRequest->mValidator = nsnull;

      mRequest = nsnull;

      return NS_OK;
    }
  }

  // fun stuff.
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  nsRefPtr<imgCacheEntry> entry;
  nsCOMPtr<nsIURI> uri;

  // Doom the old request's cache entry
  mRequest->RemoveFromCache();

  mRequest->GetURI(getter_AddRefs(uri));

  mRequest->mValidator = nsnull;
  mRequest = nsnull;

  imgRequest *request;

  if (!NewRequestAndEntry(uri, &request, getter_AddRefs(entry)))
      return NS_ERROR_OUT_OF_MEMORY;

  // XXX(darin):  I'm not sure that using the original URI is correct here.
  // Perhaps we should use the same URI that indexes the cache?  Or, perhaps
  // the cache should use the original URI?  See bug 89419.
  nsCOMPtr<nsIURI> originalURI;
  channel->GetOriginalURI(getter_AddRefs(originalURI));
  request->Init(originalURI, channel, entry, NS_GetCurrentThread(), mContext);

  ProxyListener *pl = new ProxyListener(static_cast<nsIStreamListener *>(request));
  if (!pl) {
    NS_RELEASE(request);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mDestListener = static_cast<nsIStreamListener*>(pl);

  PRUint32 count = mProxies.Count();
  for (PRInt32 i = count-1; i>=0; i--) {
    imgRequestProxy *proxy = static_cast<imgRequestProxy *>(mProxies[i]);
    proxy->ChangeOwner(request);
    request->NotifyProxyListener(proxy);
  }

  // Try to add the new request into the cache.
  sImgLoader.PutIntoCache(uri, entry);

  NS_RELEASE(request);

  if (!mDestListener)
    return NS_OK;

  return mDestListener->OnStartRequest(aRequest, ctxt);
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status); */
NS_IMETHODIMP imgCacheValidator::OnStopRequest(nsIRequest *aRequest, nsISupports *ctxt, nsresult status)
{
  if (!mDestListener)
    return NS_OK;

  return mDestListener->OnStopRequest(aRequest, ctxt, status);
}

/** nsIStreamListener methods **/


// XXX see bug 113959
static NS_METHOD dispose_of_data(nsIInputStream* in, void* closure,
                                 const char* fromRawSegment, PRUint32 toOffset,
                                 PRUint32 count, PRUint32 *writeCount)
{
  *writeCount = count;
  return NS_OK;
}

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP imgCacheValidator::OnDataAvailable(nsIRequest *aRequest, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
#ifdef DEBUG
  nsCOMPtr<nsICachingChannel> cacheChan(do_QueryInterface(aRequest));
  if (cacheChan) {
    PRBool isFromCache;
    if (NS_SUCCEEDED(cacheChan->IsFromCache(&isFromCache)) && isFromCache)
      NS_ERROR("OnDataAvailable not suppressed by LOAD_ONLY_IF_MODIFIED load flag");
  }
#endif

  if (!mDestListener) {
    // XXX see bug 113959
    PRUint32 _retval;
    inStr->ReadSegments(dispose_of_data, nsnull, count, &_retval);
    return NS_OK;
  }

  return mDestListener->OnDataAvailable(aRequest, ctxt, inStr, sourceOffset, count);
}
