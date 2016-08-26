/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSecureBrowserUIImpl.h"

#include "imgIRequest.h"
#include "mozilla/Logging.h"
#include "nsCURILoader.h"
#include "nsIAssociatedContentSecurity.h"
#include "nsIChannel.h"
#include "nsIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocument.h"
#include "nsIFTPChannel.h"
#include "nsIFileChannel.h"
#include "nsIHttpChannel.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProtocolHandler.h"
#include "nsISSLStatus.h"
#include "nsISecurityInfoProvider.h"
#include "nsIServiceManager.h"
#include "nsITransportSecurityInfo.h"
#include "nsIWebProgress.h"
#include "nsIWyciwygChannel.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsThreadUtils.h"
#include "nspr.h"
#include "nsString.h"

using namespace mozilla;

LazyLogModule gSecureDocLog("nsSecureBrowserUI");

struct RequestHashEntry : PLDHashEntryHdr {
    void *r;
};

static bool
RequestMapMatchEntry(const PLDHashEntryHdr *hdr, const void *key)
{
  const RequestHashEntry *entry = static_cast<const RequestHashEntry*>(hdr);
  return entry->r == key;
}

static void
RequestMapInitEntry(PLDHashEntryHdr *hdr, const void *key)
{
  RequestHashEntry *entry = static_cast<RequestHashEntry*>(hdr);
  entry->r = (void*)key;
}

static const PLDHashTableOps gMapOps = {
  PLDHashTable::HashVoidPtrKeyStub,
  RequestMapMatchEntry,
  PLDHashTable::MoveEntryStub,
  PLDHashTable::ClearEntryStub,
  RequestMapInitEntry
};

nsSecureBrowserUIImpl::nsSecureBrowserUIImpl()
  : mNotifiedSecurityState(lis_no_security)
  , mNotifiedToplevelIsEV(false)
  , mNewToplevelSecurityState(STATE_IS_INSECURE)
  , mNewToplevelIsEV(false)
  , mNewToplevelSecurityStateKnown(true)
  , mIsViewSource(false)
  , mSubRequestsBrokenSecurity(0)
  , mSubRequestsNoSecurity(0)
  , mCertUserOverridden(false)
  , mRestoreSubrequests(false)
  , mOnLocationChangeSeen(false)
#ifdef DEBUG
  , mEntered(false)
#endif
  , mTransferringRequests(&gMapOps, sizeof(RequestHashEntry))
{
  MOZ_ASSERT(NS_IsMainThread());

  ResetStateTracking();
}

NS_IMPL_ISUPPORTS(nsSecureBrowserUIImpl,
                  nsISecureBrowserUI,
                  nsIWebProgressListener,
                  nsISupportsWeakReference,
                  nsISSLStatusProvider)

NS_IMETHODIMP
nsSecureBrowserUIImpl::Init(mozIDOMWindowProxy* aWindow)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (MOZ_LOG_TEST(gSecureDocLog, LogLevel::Debug)) {
    nsCOMPtr<nsIDOMWindow> window(do_QueryReferent(mWindow));

    MOZ_LOG(gSecureDocLog, LogLevel::Debug,
           ("SecureUI:%p: Init: mWindow: %p, aWindow: %p\n", this,
            window.get(), aWindow));
  }

  if (!aWindow) {
    NS_WARNING("Null window passed to nsSecureBrowserUIImpl::Init()");
    return NS_ERROR_INVALID_ARG;
  }

  if (mWindow) {
    NS_WARNING("Trying to init an nsSecureBrowserUIImpl twice");
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  nsresult rv;
  mWindow = do_GetWeakReference(aWindow, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  auto* piwindow = nsPIDOMWindowOuter::From(aWindow);
  nsIDocShell *docShell = piwindow->GetDocShell();

  // The Docshell will own the SecureBrowserUI object
  if (!docShell)
    return NS_ERROR_FAILURE;

  docShell->SetSecurityUI(this);

  /* GetWebProgress(mWindow) */
  // hook up to the webprogress notifications.
  nsCOMPtr<nsIWebProgress> wp(do_GetInterface(docShell));
  if (!wp) return NS_ERROR_FAILURE;
  /* end GetWebProgress */
  
  wp->AddProgressListener(static_cast<nsIWebProgressListener*>(this),
                          nsIWebProgress::NOTIFY_STATE_ALL | 
                          nsIWebProgress::NOTIFY_LOCATION  |
                          nsIWebProgress::NOTIFY_SECURITY);


  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::GetState(uint32_t* aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  return MapInternalToExternalState(aState, mNotifiedSecurityState,
                                    mNotifiedToplevelIsEV);
}

// static
already_AddRefed<nsISupports> 
nsSecureBrowserUIImpl::ExtractSecurityInfo(nsIRequest* aRequest)
{
  nsCOMPtr<nsISupports> retval;
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel)
    channel->GetSecurityInfo(getter_AddRefs(retval));
  
  if (!retval) {
    nsCOMPtr<nsISecurityInfoProvider> provider(do_QueryInterface(aRequest));
    if (provider)
      provider->GetSecurityInfo(getter_AddRefs(retval));
  }

  return retval.forget();
}

nsresult
nsSecureBrowserUIImpl::MapInternalToExternalState(uint32_t* aState, lockIconState lock, bool ev)
{
  NS_ENSURE_ARG(aState);

  switch (lock)
  {
    case lis_broken_security:
      *aState = STATE_IS_BROKEN;
      break;

    case lis_mixed_security:
      *aState = STATE_IS_BROKEN;
      break;

    case lis_high_security:
      *aState = STATE_IS_SECURE | STATE_SECURE_HIGH;
      break;

    default:
    case lis_no_security:
      *aState = STATE_IS_INSECURE;
      break;
  }

  if (ev && (*aState & STATE_IS_SECURE))
    *aState |= nsIWebProgressListener::STATE_IDENTITY_EV_TOPLEVEL;

  if (mCertUserOverridden && (*aState & STATE_IS_SECURE)) {
    *aState |= nsIWebProgressListener::STATE_CERT_USER_OVERRIDDEN;
  }

  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocShell);
  if (!docShell)
    return NS_OK;

  // For content docShell's, the mixed content security state is set on the root docShell.
  if (docShell->ItemType() == nsIDocShellTreeItem::typeContent) {
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem(do_QueryInterface(docShell));
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    docShellTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    NS_ASSERTION(sameTypeRoot, "No document shell root tree item from document shell tree item!");
    docShell = do_QueryInterface(sameTypeRoot);
    if (!docShell)
      return NS_OK;
  }

  // Has a Mixed Content Load initiated in nsMixedContentBlocker?
  // * If not, the state should not be broken because of mixed content;
  // overriding the previous state if it is inaccurately flagged as mixed.
  if (lock == lis_mixed_security &&
      !docShell->GetHasMixedActiveContentLoaded() &&
      !docShell->GetHasMixedDisplayContentLoaded() &&
      !docShell->GetHasMixedActiveContentBlocked() &&
      !docShell->GetHasMixedDisplayContentBlocked()) {
    *aState = STATE_IS_SECURE;
    if (ev) {
      *aState |= nsIWebProgressListener::STATE_IDENTITY_EV_TOPLEVEL;
    }
  }
  // * If so, the state should be broken or insecure; overriding the previous
  // state set by the lock parameter.
  uint32_t tempState = STATE_IS_BROKEN;
  if (lock == lis_no_security) {
      // this is to ensure that http: pages with mixed content in nested
      // iframes don't get marked as broken instead of insecure
      tempState = STATE_IS_INSECURE;
  }
  if (docShell->GetHasMixedActiveContentLoaded() &&
      docShell->GetHasMixedDisplayContentLoaded()) {
      *aState = tempState |
                nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT |
                nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT;
  } else if (docShell->GetHasMixedActiveContentLoaded()) {
      *aState = tempState |
                nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT;
  } else if (docShell->GetHasMixedDisplayContentLoaded()) {
      *aState = tempState |
                nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT;
  }

  if (mCertUserOverridden) {
    *aState |= nsIWebProgressListener::STATE_CERT_USER_OVERRIDDEN;
  }

  // Has Mixed Content Been Blocked in nsMixedContentBlocker?
  if (docShell->GetHasMixedActiveContentBlocked())
    *aState |= nsIWebProgressListener::STATE_BLOCKED_MIXED_ACTIVE_CONTENT;

  if (docShell->GetHasMixedDisplayContentBlocked())
    *aState |= nsIWebProgressListener::STATE_BLOCKED_MIXED_DISPLAY_CONTENT;

  // Has Tracking Content been Blocked?
  if (docShell->GetHasTrackingContentBlocked())
    *aState |= nsIWebProgressListener::STATE_BLOCKED_TRACKING_CONTENT;

  if (docShell->GetHasTrackingContentLoaded())
    *aState |= nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT;

  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::SetDocShell(nsIDocShell* aDocShell)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv;
  mDocShell = do_GetWeakReference(aDocShell, &rv);
  return rv;
}

static uint32_t GetSecurityStateFromSecurityInfoAndRequest(nsISupports* info,
                                                           nsIRequest* request)
{
  nsresult res;
  uint32_t securityState;

  nsCOMPtr<nsITransportSecurityInfo> psmInfo(do_QueryInterface(info));
  if (!psmInfo) {
    MOZ_LOG(gSecureDocLog, LogLevel::Debug, ("SecureUI: GetSecurityState: - no nsITransportSecurityInfo for %p\n",
                                         (nsISupports *)info));
    return nsIWebProgressListener::STATE_IS_INSECURE;
  }
  MOZ_LOG(gSecureDocLog, LogLevel::Debug, ("SecureUI: GetSecurityState: - info is %p\n", 
                                       (nsISupports *)info));
  
  res = psmInfo->GetSecurityState(&securityState);
  if (NS_FAILED(res)) {
    MOZ_LOG(gSecureDocLog, LogLevel::Debug, ("SecureUI: GetSecurityState: - GetSecurityState failed: %d\n",
                                         res));
    securityState = nsIWebProgressListener::STATE_IS_BROKEN;
  }

  if (securityState != nsIWebProgressListener::STATE_IS_INSECURE) {
    // A secure connection does not yield a secure per-uri channel if the
    // scheme is plain http.

    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIChannel> channel(do_QueryInterface(request));
    if (channel) {
      channel->GetURI(getter_AddRefs(uri));
    } else {
      nsCOMPtr<imgIRequest> imgRequest(do_QueryInterface(request));
      if (imgRequest) {
        imgRequest->GetURI(getter_AddRefs(uri));
      }
    }
    if (uri) {
      bool isHttp, isFtp;
      if ((NS_SUCCEEDED(uri->SchemeIs("http", &isHttp)) && isHttp) ||
          (NS_SUCCEEDED(uri->SchemeIs("ftp", &isFtp)) && isFtp)) {
        MOZ_LOG(gSecureDocLog, LogLevel::Debug, ("SecureUI: GetSecurityState: - "
                                             "channel scheme is insecure.\n"));
        securityState = nsIWebProgressListener::STATE_IS_INSECURE;
      }
    }
  }

  MOZ_LOG(gSecureDocLog, LogLevel::Debug, ("SecureUI: GetSecurityState: - Returning %d\n", 
                                       securityState));
  return securityState;
}


//  nsIWebProgressListener
NS_IMETHODIMP 
nsSecureBrowserUIImpl::OnProgressChange(nsIWebProgress* aWebProgress,
                                        nsIRequest* aRequest,
                                        int32_t aCurSelfProgress,
                                        int32_t aMaxSelfProgress,
                                        int32_t aCurTotalProgress,
                                        int32_t aMaxTotalProgress)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

void
nsSecureBrowserUIImpl::ResetStateTracking()
{
  mDocumentRequestsInProgress = 0;
  mTransferringRequests.Clear();
}

void
nsSecureBrowserUIImpl::EvaluateAndUpdateSecurityState(nsIRequest* aRequest,
                                                      nsISupports* info,
                                                      bool withNewLocation,
                                                      bool withNewSink)
{
  mNewToplevelIsEV = false;

  bool updateStatus = false;
  nsCOMPtr<nsISSLStatus> temp_SSLStatus;

  mNewToplevelSecurityState =
    GetSecurityStateFromSecurityInfoAndRequest(info, aRequest);

  MOZ_LOG(gSecureDocLog, LogLevel::Debug,
          ("SecureUI:%p: OnStateChange: remember mNewToplevelSecurityState => %x\n",
           this, mNewToplevelSecurityState));

  nsCOMPtr<nsISSLStatusProvider> sp(do_QueryInterface(info));
  if (sp) {
    // Ignore result
    updateStatus = true;
    (void) sp->GetSSLStatus(getter_AddRefs(temp_SSLStatus));
    if (temp_SSLStatus) {
      bool aTemp;
      if (NS_SUCCEEDED(temp_SSLStatus->GetIsExtendedValidation(&aTemp))) {
        mNewToplevelIsEV = aTemp;
      }
    }
  }

  mNewToplevelSecurityStateKnown = true;
  if (updateStatus) {
    mSSLStatus = temp_SSLStatus;
  }
  MOZ_LOG(gSecureDocLog, LogLevel::Debug,
         ("SecureUI:%p: remember securityInfo %p\n", this,
          info));
  nsCOMPtr<nsIAssociatedContentSecurity> associatedContentSecurityFromRequest(
    do_QueryInterface(aRequest));
  if (associatedContentSecurityFromRequest) {
    mCurrentToplevelSecurityInfo = aRequest;
  } else {
    mCurrentToplevelSecurityInfo = info;
  }

  // The subrequest counters are now in sync with mCurrentToplevelSecurityInfo,
  // don't restore after top level document load finishes.
  mRestoreSubrequests = false;

  UpdateSecurityState(aRequest, withNewLocation, withNewSink || updateStatus);
}

void
nsSecureBrowserUIImpl::UpdateSubrequestMembers(nsISupports* securityInfo,
                                               nsIRequest* request)
{
  // For wyciwyg channels in subdocuments we only update our
  // subrequest state members.
  uint32_t reqState = GetSecurityStateFromSecurityInfoAndRequest(securityInfo,
                                                                 request);

  if (reqState & STATE_IS_SECURE) {
    // do nothing
  } else if (reqState & STATE_IS_BROKEN) {
    MOZ_LOG(gSecureDocLog, LogLevel::Debug,
           ("SecureUI:%p: OnStateChange: subreq BROKEN\n", this));
    ++mSubRequestsBrokenSecurity;
  } else {
    MOZ_LOG(gSecureDocLog, LogLevel::Debug,
           ("SecureUI:%p: OnStateChange: subreq INSECURE\n", this));
    ++mSubRequestsNoSecurity;
  }
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnStateChange(nsIWebProgress* aWebProgress,
                                     nsIRequest* aRequest,
                                     uint32_t aProgressStateFlags,
                                     nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrancyGuard guard(*this);
  /*
    All discussion, unless otherwise mentioned, only refers to
    http, https, file or wyciwig requests.


    Redirects are evil, well, some of them.
    There are multiple forms of redirects.

    Redirects caused by http refresh content are ok, because experiments show,
    with those redirects, the old page contents and their requests will come to STOP
    completely, before any progress from new refreshed page content is reported.
    So we can safely treat them as separate page loading transactions.

    Evil are redirects at the http protocol level, like code 302.

    If the toplevel documents gets replaced, i.e. redirected with 302, we do not care for the 
    security state of the initial transaction, which has now been redirected, 
    we only care for the new page load.
    
    For the implementation of the security UI, we make an assumption, that is hopefully true.
    
    Imagine, the received page that was delivered with the 302 redirection answer,
    also delivered html content.

    What happens if the parser starts to analyze the content and tries to load contained sub objects?
    
    In that case we would see start and stop requests for subdocuments, some for the previous document,
    some for the new target document. And only those for the new toplevel document may be
    taken into consideration, when deciding about the security state of the next toplevel document.
    
    Because security state is being looked at, when loading stops for (sub)documents, this 
    could cause real confusion, because we have to decide, whether an incoming progress 
    belongs to the new toplevel page, or the previous, already redirected page.
    
    Can we simplify here?
    
    If a redirect at the http protocol level is seen, can we safely assume, its html content
    will not be parsed, anylzed, and no embedded objects will get loaded (css, js, images),
    because the redirect is already happening?
    
    If we can assume that, this really simplify things. Because we will never see notification
    for sub requests that need to get ignored.
    
    I would like to make this assumption for now, but please let me (kaie) know if I'm wrong.
    
    Excurse:
      If my assumption is wrong, then we would require more tracking information.
      We need to keep lists of all pointers to request object that had been seen since the
      last toplevel start event.
      If the start for a redirected page is seen, the list of releveant object must be cleared,
      and only progress for requests which start after it must be analyzed.
      All other events must be ignored, as they belong to now irrelevant previous top level documents.


    Frames are also evil.

    First we need a decision.
    kaie thinks: 
      Only if the toplevel frame is secure, we should try to display secure lock icons.
      If some of the inner contents are insecure, we display mixed mode.
      
      But if the top level frame is not secure, why indicate a mixed lock icon at all?
      I think we should always display an open lock icon, if the top level frameset is insecure.
      
      That's the way Netscape Communicator behaves, and I think we should do the same.
      
      The user will not know which parts are secure and which are not,
      and any certificate information, displayed in the tooltip or in the "page info"
      will only be relevant for some subframe(s), and the user will not know which ones,
      so we shouldn't display it as a general attribute of the displayed page.

    Why are frames evil?
    
    Because the progress for the toplevel frame document is not easily distinguishable
    from subframes. The same STATE bits are reported.

    While at first sight, when a new page load happens,
    the toplevel frameset document has also the STATE_IS_NETWORK bit in it.
    But this can't really be used. Because in case that document causes a http 302 redirect, 
    the real top level frameset will no longer have that bit.
    
    But we need some way to distinguish top level frames from inner frames.
    
    I saw that the web progress we get delivered has a reference to the toplevel DOM window.
    
    I suggest, we look at all incoming requests.
    If a request is NOT for the toplevel DOM window, we will always treat it as a subdocument request,
    regardless of whether the load flags indicate a top level document.
  */

  nsCOMPtr<mozIDOMWindowProxy> windowForProgress;
  aWebProgress->GetDOMWindow(getter_AddRefs(windowForProgress));

  nsCOMPtr<mozIDOMWindowProxy> window(do_QueryReferent(mWindow));
  NS_ASSERTION(window, "Window has gone away?!");

  if (!mIOService) {
    mIOService = do_GetService(NS_IOSERVICE_CONTRACTID);
  }

  bool isNoContentResponse = false;
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
  if (httpChannel) 
  {
    uint32_t response;
    isNoContentResponse = NS_SUCCEEDED(httpChannel->GetResponseStatus(&response)) &&
        (response == 204 || response == 205);
  }
  const bool isToplevelProgress = (windowForProgress.get() == window.get()) && !isNoContentResponse;
  
  if (windowForProgress)
  {
    if (isToplevelProgress)
    {
      MOZ_LOG(gSecureDocLog, LogLevel::Debug,
             ("SecureUI:%p: OnStateChange: progress: for toplevel\n", this));
    }
    else
    {
      MOZ_LOG(gSecureDocLog, LogLevel::Debug,
             ("SecureUI:%p: OnStateChange: progress: for something else\n", this));
    }
  }
  else
  {
      MOZ_LOG(gSecureDocLog, LogLevel::Debug,
             ("SecureUI:%p: OnStateChange: progress: no window known\n", this));
  }

  MOZ_LOG(gSecureDocLog, LogLevel::Debug,
         ("SecureUI:%p: OnStateChange\n", this));

  if (mIsViewSource) {
    return NS_OK;
  }

  if (!aRequest)
  {
    MOZ_LOG(gSecureDocLog, LogLevel::Debug,
           ("SecureUI:%p: OnStateChange with null request\n", this));
    return NS_ERROR_NULL_POINTER;
  }

  if (MOZ_LOG_TEST(gSecureDocLog, LogLevel::Debug)) {
    nsXPIDLCString reqname;
    aRequest->GetName(reqname);
    MOZ_LOG(gSecureDocLog, LogLevel::Debug,
           ("SecureUI:%p: %p %p OnStateChange %x %s\n", this, aWebProgress,
            aRequest, aProgressStateFlags, reqname.get()));
  }

  nsCOMPtr<nsISupports> securityInfo(ExtractSecurityInfo(aRequest));

  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel) {
    channel->GetURI(getter_AddRefs(uri));
  }

  nsCOMPtr<imgIRequest> imgRequest(do_QueryInterface(aRequest));
  if (imgRequest) {
    NS_ASSERTION(!channel, "How did that happen, exactly?");
    // for image requests, we get the URI from here
    imgRequest->GetURI(getter_AddRefs(uri));
  }
  
  if (uri) {
    bool vs;
    if (NS_SUCCEEDED(uri->SchemeIs("javascript", &vs)) && vs) {
      // We ignore the progress events for javascript URLs.
      // If a document loading gets triggered, we will see more events.
      return NS_OK;
    }
  }

  uint32_t loadFlags = 0;
  aRequest->GetLoadFlags(&loadFlags);

  if (aProgressStateFlags & STATE_START
      &&
      aProgressStateFlags & STATE_IS_REQUEST
      &&
      isToplevelProgress
      &&
      loadFlags & nsIChannel::LOAD_DOCUMENT_URI)
  {
    MOZ_LOG(gSecureDocLog, LogLevel::Debug,
           ("SecureUI:%p: OnStateChange: SOMETHING STARTS FOR TOPMOST DOCUMENT\n", this));
  }

  if (aProgressStateFlags & STATE_STOP
      &&
      aProgressStateFlags & STATE_IS_REQUEST
      &&
      isToplevelProgress
      &&
      loadFlags & nsIChannel::LOAD_DOCUMENT_URI)
  {
    MOZ_LOG(gSecureDocLog, LogLevel::Debug,
           ("SecureUI:%p: OnStateChange: SOMETHING STOPS FOR TOPMOST DOCUMENT\n", this));
  }

  bool isSubDocumentRelevant = true;

  // We are only interested in requests that load in the browser window...
  if (!imgRequest) { // is not imgRequest
    nsCOMPtr<nsIHttpChannel> httpRequest(do_QueryInterface(aRequest));
    if (!httpRequest) {
      nsCOMPtr<nsIFileChannel> fileRequest(do_QueryInterface(aRequest));
      if (!fileRequest) {
        nsCOMPtr<nsIWyciwygChannel> wyciwygRequest(do_QueryInterface(aRequest));
        if (!wyciwygRequest) {
          nsCOMPtr<nsIFTPChannel> ftpRequest(do_QueryInterface(aRequest));
          if (!ftpRequest) {
            MOZ_LOG(gSecureDocLog, LogLevel::Debug,
                   ("SecureUI:%p: OnStateChange: not relevant for sub content\n", this));
            isSubDocumentRelevant = false;
          }
        }
      }
    }
  }

  // This will ignore all resource, chrome, data, file, moz-icon, and anno
  // protocols. Local resources are treated as trusted.
  if (uri && mIOService) {
    bool hasFlag;
    nsresult rv =
      mIOService->URIChainHasFlags(uri,
                                   nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                                   &hasFlag);
    if (NS_SUCCEEDED(rv) && hasFlag) {
      isSubDocumentRelevant = false;
    }
  }

#if defined(DEBUG)
  if (aProgressStateFlags & STATE_STOP
      &&
      channel)
  {
    MOZ_LOG(gSecureDocLog, LogLevel::Debug,
           ("SecureUI:%p: OnStateChange: seeing STOP with security state: %d\n", this,
            GetSecurityStateFromSecurityInfoAndRequest(securityInfo, aRequest)
            ));
  }
#endif

  if (aProgressStateFlags & STATE_TRANSFERRING
      &&
      aProgressStateFlags & STATE_IS_REQUEST)
  {
    // The listing of a request in mTransferringRequests
    // means, there has already been data transfered.
    mTransferringRequests.Add(aRequest, fallible);

    return NS_OK;
  }

  bool requestHasTransferedData = false;

  if (aProgressStateFlags & STATE_STOP
      &&
      aProgressStateFlags & STATE_IS_REQUEST)
  {
    PLDHashEntryHdr* entry = mTransferringRequests.Search(aRequest);
    if (entry) {
      mTransferringRequests.RemoveEntry(entry);
      requestHasTransferedData = true;
    }

    if (!requestHasTransferedData) {
      // Because image loads doesn't support any TRANSFERRING notifications but
      // only START and STOP we must ask them directly whether content was
      // transferred.  See bug 432685 for details.
      nsCOMPtr<nsISecurityInfoProvider> securityInfoProvider =
        do_QueryInterface(aRequest);
      // Guess true in all failure cases to be safe.  But if we're not
      // an nsISecurityInfoProvider, then we just haven't transferred
      // any data.
      bool hasTransferred;
      requestHasTransferedData =
        securityInfoProvider &&
        (NS_FAILED(securityInfoProvider->GetHasTransferredData(&hasTransferred)) ||
         hasTransferred);
    }
  }

  bool allowSecurityStateChange = true;
  if (loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI)
  {
    // The original consumer (this) is no longer the target of the load.
    // Ignore any events with this flag, do not allow them to update
    // our secure UI state.
    allowSecurityStateChange = false;
  }

  if (aProgressStateFlags & STATE_START
      &&
      aProgressStateFlags & STATE_IS_REQUEST
      &&
      isToplevelProgress
      &&
      loadFlags & nsIChannel::LOAD_DOCUMENT_URI)
  {
    int32_t saveSubBroken;
    int32_t saveSubNo;
    nsCOMPtr<nsIAssociatedContentSecurity> prevContentSecurity;

    int32_t newSubBroken = 0;
    int32_t newSubNo = 0;

    bool inProgress = (mDocumentRequestsInProgress != 0);

    if (allowSecurityStateChange && !inProgress) {
      saveSubBroken = mSubRequestsBrokenSecurity;
      saveSubNo = mSubRequestsNoSecurity;
      prevContentSecurity = do_QueryInterface(mCurrentToplevelSecurityInfo);
    }

    if (allowSecurityStateChange && !inProgress)
    {
      MOZ_LOG(gSecureDocLog, LogLevel::Debug,
             ("SecureUI:%p: OnStateChange: start for toplevel document\n", this
              ));

      if (prevContentSecurity)
      {
        MOZ_LOG(gSecureDocLog, LogLevel::Debug,
               ("SecureUI:%p: OnStateChange: start, saving current sub state\n", this
                ));
  
        // before resetting our state, let's save information about
        // sub element loads, so we can restore it later
        prevContentSecurity->SetCountSubRequestsBrokenSecurity(saveSubBroken);
        prevContentSecurity->SetCountSubRequestsNoSecurity(saveSubNo);
        prevContentSecurity->Flush();
        MOZ_LOG(gSecureDocLog, LogLevel::Debug, ("SecureUI:%p: Saving subs in START to %p as %d,%d\n", 
          this, prevContentSecurity.get(), saveSubBroken, saveSubNo));      
      }

      bool retrieveAssociatedState = false;

      if (securityInfo &&
          (aProgressStateFlags & nsIWebProgressListener::STATE_RESTORING) != 0) {
        retrieveAssociatedState = true;
      } else {
        nsCOMPtr<nsIWyciwygChannel> wyciwygRequest(do_QueryInterface(aRequest));
        if (wyciwygRequest) {
          retrieveAssociatedState = true;
        }
      }

      if (retrieveAssociatedState)
      {
        // When restoring from bfcache, we will not get events for the 
        // page's sub elements, so let's load the state of sub elements
        // from the cache.
    
        nsCOMPtr<nsIAssociatedContentSecurity> 
          newContentSecurity(do_QueryInterface(securityInfo));
    
        if (newContentSecurity)
        {
          MOZ_LOG(gSecureDocLog, LogLevel::Debug,
                 ("SecureUI:%p: OnStateChange: start, loading old sub state\n", this
                  ));
    
          newContentSecurity->GetCountSubRequestsBrokenSecurity(&newSubBroken);
          newContentSecurity->GetCountSubRequestsNoSecurity(&newSubNo);
          MOZ_LOG(gSecureDocLog, LogLevel::Debug, ("SecureUI:%p: Restoring subs in START from %p to %d,%d\n", 
            this, newContentSecurity.get(), newSubBroken, newSubNo));      
        }
      }
      else
      {
        // If we don't get OnLocationChange for this top level load later,
        // it didn't get rendered.  But we reset the state to unknown and
        // mSubRequests* to zeros.  If we would have left these values after 
        // this top level load stoped, we would override the original top level
        // load with all zeros and break mixed content state on back and forward.
        mRestoreSubrequests = true;
      }
    }

    if (allowSecurityStateChange && !inProgress) {
      ResetStateTracking();
      mSubRequestsBrokenSecurity = newSubBroken;
      mSubRequestsNoSecurity = newSubNo;
      mNewToplevelSecurityStateKnown = false;
    }

    // By using a counter, this code also works when the toplevel
    // document get's redirected, but the STOP request for the 
    // previous toplevel document has not yet have been received.
    MOZ_LOG(gSecureDocLog, LogLevel::Debug,
           ("SecureUI:%p: OnStateChange: ++mDocumentRequestsInProgress\n", this
            ));
    ++mDocumentRequestsInProgress;

    return NS_OK;
  }

  if (aProgressStateFlags & STATE_STOP
      &&
      aProgressStateFlags & STATE_IS_REQUEST
      &&
      isToplevelProgress
      &&
      loadFlags & nsIChannel::LOAD_DOCUMENT_URI)
  {
    nsCOMPtr<nsISecurityEventSink> temp_ToplevelEventSink;

    if (allowSecurityStateChange) {
      temp_ToplevelEventSink = mToplevelEventSink;
    }

    if (mDocumentRequestsInProgress <= 0) {
      // Ignore stop requests unless a document load is in progress
      // Unfortunately on application start, see some stops without having seen any starts...
      return NS_OK;
    }

    MOZ_LOG(gSecureDocLog, LogLevel::Debug,
           ("SecureUI:%p: OnStateChange: --mDocumentRequestsInProgress\n", this
            ));

    if (!temp_ToplevelEventSink && channel)
    {
      if (allowSecurityStateChange)
      {
        ObtainEventSink(channel, temp_ToplevelEventSink);
      }
    }

    bool sinkChanged = false;
    bool inProgress;
    if (allowSecurityStateChange) {
      sinkChanged = (mToplevelEventSink != temp_ToplevelEventSink);
      mToplevelEventSink = temp_ToplevelEventSink;
    }
    --mDocumentRequestsInProgress;
    inProgress = mDocumentRequestsInProgress > 0;

    if (allowSecurityStateChange && requestHasTransferedData) {
      // Data has been transferred for the single toplevel
      // request. Evaluate the security state.

      // Do this only when the sink has changed.  We update and notify
      // the state from OnLacationChange, this is actually redundant.
      // But when the target sink changes between OnLocationChange and
      // OnStateChange, we have to fire the notification here (again).

      if (sinkChanged || mOnLocationChangeSeen) {
        EvaluateAndUpdateSecurityState(aRequest, securityInfo, false,
                                       sinkChanged);
        return NS_OK;
      }
    }
    mOnLocationChangeSeen = false;

    if (mRestoreSubrequests && !inProgress)
    {
      // We get here when there were no OnLocationChange between 
      // OnStateChange(START) and OnStateChange(STOP).  Then the load has not
      // been rendered but has been retargeted in some other way then by external
      // app handler.  Restore mSubRequests* members to what the current security 
      // state info holds (it was reset to all zero in OnStateChange(START) 
      // before).
      nsCOMPtr<nsIAssociatedContentSecurity> currentContentSecurity(
        do_QueryInterface(mCurrentToplevelSecurityInfo));

      // Drop this indication flag, the restore operation is just being done.
      mRestoreSubrequests = false;

      // We can do this since the state didn't actually change.
      mNewToplevelSecurityStateKnown = true;

      int32_t subBroken = 0;
      int32_t subNo = 0;

      if (currentContentSecurity)
      {
        currentContentSecurity->GetCountSubRequestsBrokenSecurity(&subBroken);
        currentContentSecurity->GetCountSubRequestsNoSecurity(&subNo);
        MOZ_LOG(gSecureDocLog, LogLevel::Debug, ("SecureUI:%p: Restoring subs in STOP from %p to %d,%d\n", 
          this, currentContentSecurity.get(), subBroken, subNo));      
      }

      mSubRequestsBrokenSecurity = subBroken;
      mSubRequestsNoSecurity = subNo;
    }

    return NS_OK;
  }

  if (aProgressStateFlags & STATE_STOP
      &&
      aProgressStateFlags & STATE_IS_REQUEST)
  {
    if (!isSubDocumentRelevant)
      return NS_OK;

    // if we arrive here, LOAD_DOCUMENT_URI is not set

    // We only care for the security state of sub requests which have actually transfered data.

    if (allowSecurityStateChange && requestHasTransferedData)
    {  
      UpdateSubrequestMembers(securityInfo, aRequest);

      // Care for the following scenario:
      // A new top level document load might have already started,
      // but the security state of the new top level document might not yet been known.
      // 
      // At this point, we are learning about the security state of a sub-document.
      // We must not update the security state based on the sub content,
      // if the new top level state is not yet known.
      //
      // We skip updating the security state in this case.

      if (mNewToplevelSecurityStateKnown) {
        UpdateSecurityState(aRequest, false, false);
      }
    }

    return NS_OK;
  }

  return NS_OK;
}

// I'm keeping this as a separate function, in order to simplify the review
// for bug 412456. We should inline this in a follow up patch.
void nsSecureBrowserUIImpl::ObtainEventSink(nsIChannel *channel, 
                                            nsCOMPtr<nsISecurityEventSink> &sink)
{
  if (!sink)
    NS_QueryNotificationCallbacks(channel, sink);
}

void
nsSecureBrowserUIImpl::UpdateSecurityState(nsIRequest* aRequest,
                                           bool withNewLocation,
                                           bool withUpdateStatus)
{
  lockIconState newSecurityState = lis_no_security;
  if (mNewToplevelSecurityState & STATE_IS_SECURE) {
    // If a subresoure/request was insecure, then we have mixed security.
    if (mSubRequestsBrokenSecurity || mSubRequestsNoSecurity) {
      newSecurityState = lis_mixed_security;
    } else {
      newSecurityState = lis_high_security;
    }
  }

  if (mNewToplevelSecurityState & STATE_IS_BROKEN) {
    newSecurityState = lis_broken_security;
  }

  mCertUserOverridden =
    mNewToplevelSecurityState & STATE_CERT_USER_OVERRIDDEN;

  MOZ_LOG(gSecureDocLog, LogLevel::Debug,
         ("SecureUI:%p: UpdateSecurityState:  old-new  %d - %d\n", this,
          mNotifiedSecurityState, newSecurityState));

  bool flagsChanged = false;
  if (mNotifiedSecurityState != newSecurityState) {
    // Something changed since the last time.
    flagsChanged = true;
    mNotifiedSecurityState = newSecurityState;

    // If we have no security, we also shouldn't have any SSL status.
    if (newSecurityState == lis_no_security) {
      mSSLStatus = nullptr;
    }
  }

  if (mNotifiedToplevelIsEV != mNewToplevelIsEV) {
    flagsChanged = true;
    mNotifiedToplevelIsEV = mNewToplevelIsEV;
  }

  if (flagsChanged || withNewLocation || withUpdateStatus) {
    TellTheWorld(aRequest);
  }
}

void
nsSecureBrowserUIImpl::TellTheWorld(nsIRequest* aRequest)
{
  uint32_t state = STATE_IS_INSECURE;
  GetState(&state);

  if (mToplevelEventSink) {
    MOZ_LOG(gSecureDocLog, LogLevel::Debug,
           ("SecureUI:%p: UpdateSecurityState: calling OnSecurityChange\n",
            this));

    mToplevelEventSink->OnSecurityChange(aRequest, state);
  } else {
    MOZ_LOG(gSecureDocLog, LogLevel::Debug,
           ("SecureUI:%p: UpdateSecurityState: NO mToplevelEventSink!\n",
            this));

  }
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnLocationChange(nsIWebProgress* aWebProgress,
                                        nsIRequest* aRequest,
                                        nsIURI* aLocation,
                                        uint32_t aFlags)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrancyGuard guard(*this);

  MOZ_LOG(gSecureDocLog, LogLevel::Debug,
         ("SecureUI:%p: OnLocationChange\n", this));

  bool updateIsViewSource = false;
  bool temp_IsViewSource = false;
  nsCOMPtr<mozIDOMWindowProxy> window;

  if (aLocation)
  {
    bool vs;

    nsresult rv = aLocation->SchemeIs("view-source", &vs);
    NS_ENSURE_SUCCESS(rv, rv);

    if (vs) {
      MOZ_LOG(gSecureDocLog, LogLevel::Debug,
             ("SecureUI:%p: OnLocationChange: view-source\n", this));
    }

    updateIsViewSource = true;
    temp_IsViewSource = vs;
  }

  if (updateIsViewSource) {
    mIsViewSource = temp_IsViewSource;
  }
  mCurrentURI = aLocation;
  window = do_QueryReferent(mWindow);
  NS_ASSERTION(window, "Window has gone away?!");

  // When |aRequest| is null, basically we don't trust that document. But if
  // docshell insists that the document has not changed at all, we will reuse
  // the previous security state, no matter what |aRequest| may be.
  if (aFlags & LOCATION_CHANGE_SAME_DOCUMENT)
    return NS_OK;

  // The location bar has changed, so we must update the security state.  The
  // only concern with doing this here is that a page may transition from being
  // reported as completely secure to being reported as partially secure
  // (mixed).  This may be confusing for users, and it may bother users who
  // like seeing security dialogs.  However, it seems prudent given that page
  // loading may never end in some edge cases (perhaps by a site with malicious
  // intent).

  nsCOMPtr<mozIDOMWindowProxy> windowForProgress;
  aWebProgress->GetDOMWindow(getter_AddRefs(windowForProgress));

  nsCOMPtr<nsISupports> securityInfo(ExtractSecurityInfo(aRequest));

  if (windowForProgress.get() == window.get()) {
    // For toplevel channels, update the security state right away.
    mOnLocationChangeSeen = true;
    EvaluateAndUpdateSecurityState(aRequest, securityInfo, true, false);
    return NS_OK;
  }

  // For channels in subdocuments we only update our subrequest state members.
  UpdateSubrequestMembers(securityInfo, aRequest);

  // Care for the following scenario:

  // A new toplevel document load might have already started, but the security
  // state of the new toplevel document might not yet be known.
  // 
  // At this point, we are learning about the security state of a sub-document.
  // We must not update the security state based on the sub content, if the new
  // top level state is not yet known.
  //
  // We skip updating the security state in this case.

  if (mNewToplevelSecurityStateKnown) {
    UpdateSecurityState(aRequest, true, false);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnStatusChange(nsIWebProgress* aWebProgress,
                                      nsIRequest* aRequest,
                                      nsresult aStatus,
                                      const char16_t* aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

nsresult
nsSecureBrowserUIImpl::OnSecurityChange(nsIWebProgress* aWebProgress,
                                        nsIRequest* aRequest,
                                        uint32_t state)
{
  MOZ_ASSERT(NS_IsMainThread());
#if defined(DEBUG)
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (!channel)
    return NS_OK;

  nsCOMPtr<nsIURI> aURI;
  channel->GetURI(getter_AddRefs(aURI));

  if (aURI) {
    MOZ_LOG(gSecureDocLog, LogLevel::Debug,
           ("SecureUI:%p: OnSecurityChange: (%x) %s\n", this,
            state, aURI->GetSpecOrDefault().get()));
  }
#endif

  return NS_OK;
}

// nsISSLStatusProvider methods
NS_IMETHODIMP
nsSecureBrowserUIImpl::GetSSLStatus(nsISSLStatus** _result)
{
  NS_ENSURE_ARG_POINTER(_result);
  MOZ_ASSERT(NS_IsMainThread());

  switch (mNotifiedSecurityState)
  {
    case lis_broken_security:
    case lis_mixed_security:
    case lis_high_security:
      break;

    default:
      MOZ_FALLTHROUGH_ASSERT("if this is reached you must add more entries to the switch");
    case lis_no_security:
      *_result = nullptr;
      return NS_OK;
  }

  *_result = mSSLStatus;
  NS_IF_ADDREF(*_result);

  return NS_OK;
}
