/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998-2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Brian Ryner <bryner@brianryner.com>
 *   Terry Hayes <thayes@netscape.com>
 *   Kai Engert <kaie@netscape.com>
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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "nspr.h"
#include "prlog.h"
#include "prmem.h"

#include "nsISecureBrowserUI.h"
#include "nsSecureBrowserUIImpl.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsCURILoader.h"
#include "nsIDocShell.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIPrincipal.h"
#include "nsIDOMElement.h"
#include "nsPIDOMWindow.h"
#include "nsIContent.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIFileChannel.h"
#include "nsIWyciwygChannel.h"
#include "nsIFTPChannel.h"
#include "nsITransportSecurityInfo.h"
#include "nsIIdentityInfo.h"
#include "nsIURI.h"
#include "nsISecurityEventSink.h"
#include "nsIPrompt.h"
#include "nsIFormSubmitObserver.h"
#include "nsISecurityWarningDialogs.h"
#include "nsISecurityInfoProvider.h"
#include "nsIProxyObjectManager.h"
#include "imgIRequest.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsCRT.h"
#include "nsAutoLock.h"

#define SECURITY_STRING_BUNDLE_URL "chrome://pipnss/locale/security.properties"

#define IS_SECURE(state) ((state & 0xFFFF) == STATE_IS_SECURE)

#if defined(PR_LOGGING)
//
// Log module for nsSecureBrowserUI logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsSecureBrowserUI:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gSecureDocLog = nsnull;
#endif /* PR_LOGGING */

struct RequestHashEntry : PLDHashEntryHdr {
    void *r;
};

PR_STATIC_CALLBACK(PRBool)
RequestMapMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                         const void *key)
{
  const RequestHashEntry *entry = static_cast<const RequestHashEntry*>(hdr);
  return entry->r == key;
}

PR_STATIC_CALLBACK(PRBool)
RequestMapInitEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                     const void *key)
{
  RequestHashEntry *entry = static_cast<RequestHashEntry*>(hdr);
  entry->r = (void*)key;
  return PR_TRUE;
}

static PLDHashTableOps gMapOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashVoidPtrKeyStub,
  RequestMapMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  RequestMapInitEntry
};

#ifdef DEBUG
class nsAutoAtomic {
  public:
    nsAutoAtomic(PRInt32 &i)
    :mI(i) {
      PR_AtomicIncrement(&mI);
    }

    ~nsAutoAtomic() {
      PR_AtomicDecrement(&mI);
    }

  protected:
    PRInt32 &mI;

  private:
    nsAutoAtomic(); // not accessible
};
#endif

nsSecureBrowserUIImpl::nsSecureBrowserUIImpl()
  : mNotifiedSecurityState(lis_no_security)
  , mNotifiedToplevelIsEV(PR_FALSE)
  , mNewToplevelSecurityState(STATE_IS_INSECURE)
  , mNewToplevelIsEV(PR_FALSE)
  , mNewToplevelSecurityStateKnown(PR_TRUE)
  , mIsViewSource(PR_FALSE)
  , mSubRequestsHighSecurity(0)
  , mSubRequestsLowSecurity(0)
  , mSubRequestsBrokenSecurity(0)
  , mSubRequestsNoSecurity(0)
#ifdef DEBUG
  , mOnStateLocationChangeReentranceDetection(0)
#endif
{
  mMonitor = nsAutoMonitor::NewMonitor("security.secureBrowserUIImplMonitor");
  mTransferringRequests.ops = nsnull;
  ResetStateTracking();
  
#if defined(PR_LOGGING)
  if (!gSecureDocLog)
    gSecureDocLog = PR_NewLogModule("nsSecureBrowserUI");
#endif /* PR_LOGGING */
}

nsSecureBrowserUIImpl::~nsSecureBrowserUIImpl()
{
  if (mTransferringRequests.ops) {
    PL_DHashTableFinish(&mTransferringRequests);
    mTransferringRequests.ops = nsnull;
  }
  if (mMonitor)
    nsAutoMonitor::DestroyMonitor(mMonitor);
}

NS_IMPL_THREADSAFE_ISUPPORTS6(nsSecureBrowserUIImpl,
                              nsISecureBrowserUI,
                              nsIWebProgressListener,
                              nsIFormSubmitObserver,
                              nsIObserver,
                              nsISupportsWeakReference,
                              nsISSLStatusProvider)

NS_IMETHODIMP
nsSecureBrowserUIImpl::Init(nsIDOMWindow *aWindow)
{

#ifdef PR_LOGGING
  nsCOMPtr<nsIDOMWindow> window(do_QueryReferent(mWindow));

  PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
         ("SecureUI:%p: Init: mWindow: %p, aWindow: %p\n", this,
          window.get(), aWindow));
#endif

  if (!aWindow) {
    NS_WARNING("Null window passed to nsSecureBrowserUIImpl::Init()");
    return NS_ERROR_INVALID_ARG;
  }

  if (mWindow) {
    NS_WARNING("Trying to init an nsSecureBrowserUIImpl twice");
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  nsCOMPtr<nsPIDOMWindow> pwin(do_QueryInterface(aWindow));
  if (pwin->IsInnerWindow()) {
    pwin = pwin->GetOuterWindow();
  }

  nsresult rv;
  mWindow = do_GetWeakReference(pwin, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundleService> service(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;
  
  // We do not need to test for mStringBundle here...
  // Anywhere we use it, we will test before using.  Some
  // embedded users of PSM may want to reuse our
  // nsSecureBrowserUIImpl implementation without the
  // bundle.
  service->CreateBundle(SECURITY_STRING_BUNDLE_URL, getter_AddRefs(mStringBundle));
  
  
  // hook up to the form post notifications:
  nsCOMPtr<nsIObserverService> svc(do_GetService("@mozilla.org/observer-service;1", &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = svc->AddObserver(this, NS_FORMSUBMIT_SUBJECT, PR_TRUE);
  }
  
  nsCOMPtr<nsPIDOMWindow> piwindow(do_QueryInterface(aWindow));
  if (!piwindow) return NS_ERROR_FAILURE;

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
nsSecureBrowserUIImpl::GetState(PRUint32* aState)
{
  nsAutoMonitor lock(mMonitor);
  return MapInternalToExternalState(aState, mNotifiedSecurityState, mNotifiedToplevelIsEV);
}

// static
already_AddRefed<nsISupports> 
nsSecureBrowserUIImpl::ExtractSecurityInfo(nsIRequest* aRequest)
{
  nsISupports *retval = nsnull; 
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel)
    channel->GetSecurityInfo(&retval);
  
  if (!retval) {
    nsCOMPtr<nsISecurityInfoProvider> provider(do_QueryInterface(aRequest));
    if (provider)
      provider->GetSecurityInfo(&retval);
  }

  return retval;
}

nsresult
nsSecureBrowserUIImpl::MapInternalToExternalState(PRUint32* aState, lockIconState lock, PRBool ev)
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

    case lis_low_security:
      *aState = STATE_IS_SECURE | STATE_SECURE_LOW;
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
  
  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::GetTooltipText(nsAString& aText)
{
  lockIconState state;
  nsXPIDLString tooltip;

  {
    nsAutoMonitor lock(mMonitor);
    state = mNotifiedSecurityState;
    tooltip = mInfoTooltip;
  }

  if (state == lis_mixed_security)
  {
    GetBundleString(NS_LITERAL_STRING("SecurityButtonMixedContentTooltipText").get(),
                    aText);
  }
  else if (!tooltip.IsEmpty())
  {
    aText = tooltip;
  }
  else
  {
    GetBundleString(NS_LITERAL_STRING("SecurityButtonTooltipText").get(),
                    aText);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::Observe(nsISupports*, const char*,
                               const PRUnichar*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


static nsresult IsChildOfDomWindow(nsIDOMWindow *parent, nsIDOMWindow *child,
                                   PRBool* value)
{
  *value = PR_FALSE;
  
  if (parent == child) {
    *value = PR_TRUE;
    return NS_OK;
  }
  
  nsCOMPtr<nsIDOMWindow> childsParent;
  child->GetParent(getter_AddRefs(childsParent));
  
  if (childsParent && childsParent.get() != child)
    IsChildOfDomWindow(parent, childsParent, value);
  
  return NS_OK;
}

static PRUint32 GetSecurityStateFromSecurityInfo(nsISupports *info)
{
  nsresult res;
  PRUint32 securityState;

  nsCOMPtr<nsITransportSecurityInfo> psmInfo(do_QueryInterface(info));
  if (!psmInfo) {
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI: GetSecurityState: - no nsITransportSecurityInfo for %p\n",
                                         (nsISupports *)info));
    return nsIWebProgressListener::STATE_IS_INSECURE;
  }
  PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI: GetSecurityState: - info is %p\n", 
                                       (nsISupports *)info));
  
  res = psmInfo->GetSecurityState(&securityState);
  if (NS_FAILED(res)) {
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI: GetSecurityState: - GetSecurityState failed: %d\n",
                                         res));
    securityState = nsIWebProgressListener::STATE_IS_BROKEN;
  }
  
  PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI: GetSecurityState: - Returning %d\n", 
                                       securityState));
  return securityState;
}


NS_IMETHODIMP
nsSecureBrowserUIImpl::Notify(nsIDOMHTMLFormElement* aDOMForm,
                              nsIDOMWindowInternal* aWindow, nsIURI* actionURL,
                              PRBool* cancelSubmit)
{
  // Return NS_OK unless we want to prevent this form from submitting.
  *cancelSubmit = PR_FALSE;
  if (!aWindow || !actionURL || !aDOMForm)
    return NS_OK;
  
  nsCOMPtr<nsIContent> formNode = do_QueryInterface(aDOMForm);

  nsCOMPtr<nsIDocument> document = formNode->GetDocument();
  if (!document) return NS_OK;

  nsIPrincipal *principal = formNode->NodePrincipal();
  
  if (!principal)
  {
    *cancelSubmit = PR_TRUE;
    return NS_OK;
  }

  nsCOMPtr<nsIURI> formURL;
  if (NS_FAILED(principal->GetURI(getter_AddRefs(formURL))) ||
      !formURL)
  {
    formURL = document->GetDocumentURI();
  }

  nsCOMPtr<nsIDOMWindow> postingWindow =
    do_QueryInterface(document->GetWindow());
  // We can't find this document's window, cancel it.
  if (!postingWindow)
  {
    NS_WARNING("If you see this and can explain why it should be allowed, note in Bug 332324");
    *cancelSubmit = PR_TRUE;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMWindow> window;
  {
    nsAutoMonitor lock(mMonitor);
    window = do_QueryReferent(mWindow);
    NS_ASSERTION(window, "Window has gone away?!");
  }

  PRBool isChild;
  IsChildOfDomWindow(window, postingWindow, &isChild);
  
  // This notify call is not for our window, ignore it.
  if (!isChild)
    return NS_OK;
  
  PRBool okayToPost;
  nsresult res = CheckPost(formURL, actionURL, &okayToPost);
  
  if (NS_SUCCEEDED(res) && !okayToPost)
    *cancelSubmit = PR_TRUE;
  
  return res;
}

//  nsIWebProgressListener
NS_IMETHODIMP 
nsSecureBrowserUIImpl::OnProgressChange(nsIWebProgress* aWebProgress,
                                        nsIRequest* aRequest,
                                        PRInt32 aCurSelfProgress,
                                        PRInt32 aMaxSelfProgress,
                                        PRInt32 aCurTotalProgress,
                                        PRInt32 aMaxTotalProgress)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

void nsSecureBrowserUIImpl::ResetStateTracking()
{
  nsAutoMonitor lock(mMonitor);

  mInfoTooltip.Truncate();
  mDocumentRequestsInProgress = 0;
  if (mTransferringRequests.ops) {
    PL_DHashTableFinish(&mTransferringRequests);
    mTransferringRequests.ops = nsnull;
  }
  PL_DHashTableInit(&mTransferringRequests, &gMapOps, nsnull,
                    sizeof(RequestHashEntry), 16);
}

nsresult
nsSecureBrowserUIImpl::EvaluateAndUpdateSecurityState(nsIRequest* aRequest, nsISupports *info,
                                                      PRBool withNewLocation)
{
  /* I explicitly ignore the camelCase variable naming style here,
     I want to make it clear these are temp variables that relate to the 
     member variables with the same suffix.*/

  PRUint32 temp_NewToplevelSecurityState = nsIWebProgressListener::STATE_IS_INSECURE;
  PRBool temp_NewToplevelIsEV = PR_FALSE;

  PRBool updateStatus = PR_FALSE;
  nsCOMPtr<nsISupports> temp_SSLStatus;

  PRBool updateTooltip = PR_FALSE;
  nsXPIDLString temp_InfoTooltip;

    temp_NewToplevelSecurityState = GetSecurityStateFromSecurityInfo(info);

    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: OnStateChange: remember mNewToplevelSecurityState => %x\n", this,
            temp_NewToplevelSecurityState));

    nsCOMPtr<nsISSLStatusProvider> sp = do_QueryInterface(info);
    if (sp) {
      // Ignore result
      updateStatus = PR_TRUE;
      sp->GetSSLStatus(getter_AddRefs(temp_SSLStatus));
    }

    if (info) {
      nsCOMPtr<nsITransportSecurityInfo> secInfo(do_QueryInterface(info));
      if (secInfo) {
        updateTooltip = PR_TRUE;
        secInfo->GetShortSecurityDescription(getter_Copies(temp_InfoTooltip));
      }

      nsCOMPtr<nsIIdentityInfo> idinfo = do_QueryInterface(info);
      if (idinfo) {
        PRBool aTemp;
        if (NS_SUCCEEDED(idinfo->GetIsExtendedValidation(&aTemp))) {
          temp_NewToplevelIsEV = aTemp;
        }
      }
    }

  // assume temp_NewToplevelSecurityState was set in this scope!
  // see code that is directly above

  {
    nsAutoMonitor lock(mMonitor);
    mNewToplevelSecurityStateKnown = PR_TRUE;
    mNewToplevelSecurityState = temp_NewToplevelSecurityState;
    mNewToplevelIsEV = temp_NewToplevelIsEV;
    if (updateStatus) {
      mSSLStatus = temp_SSLStatus;
    }
    if (updateTooltip) {
      mInfoTooltip = temp_InfoTooltip;
    }
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: remember securityInfo %p\n", this,
            info));
    nsCOMPtr<nsIAssociatedContentSecurity> associatedContentSecurityFromRequest =
        do_QueryInterface(aRequest);
    if (associatedContentSecurityFromRequest)
        mCurrentToplevelSecurityInfo = aRequest;
    else
        mCurrentToplevelSecurityInfo = info;
  }

  return UpdateSecurityState(aRequest, withNewLocation, 
                             updateStatus, updateTooltip);
}

void
nsSecureBrowserUIImpl::UpdateSubrequestMembers(nsISupports *securityInfo)
{
  // For wyciwyg channels in subdocuments we only update our
  // subrequest state members.
  PRUint32 reqState = GetSecurityStateFromSecurityInfo(securityInfo);

  // the code above this line should run without a lock
  nsAutoMonitor lock(mMonitor);

  if (reqState & STATE_IS_SECURE) {
    if (reqState & STATE_SECURE_LOW || reqState & STATE_SECURE_MED) {
      PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
             ("SecureUI:%p: OnStateChange: subreq LOW\n", this));
      ++mSubRequestsLowSecurity;
    } else {
      PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
             ("SecureUI:%p: OnStateChange: subreq HIGH\n", this));
      ++mSubRequestsHighSecurity;
    }
  } else if (reqState & STATE_IS_BROKEN) {
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: OnStateChange: subreq BROKEN\n", this));
    ++mSubRequestsBrokenSecurity;
  } else {
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: OnStateChange: subreq INSECURE\n", this));
    ++mSubRequestsNoSecurity;
  }
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnStateChange(nsIWebProgress* aWebProgress,
                                     nsIRequest* aRequest,
                                     PRUint32 aProgressStateFlags,
                                     nsresult aStatus)
{
#ifdef DEBUG
  nsAutoAtomic atomic(mOnStateLocationChangeReentranceDetection);
  NS_ASSERTION(mOnStateLocationChangeReentranceDetection == 1,
               "unexpected parallel nsIWebProgress OnStateChange and/or OnLocationChange notification");
#endif
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

  nsCOMPtr<nsIDOMWindow> windowForProgress;
  aWebProgress->GetDOMWindow(getter_AddRefs(windowForProgress));

  nsCOMPtr<nsIDOMWindow> window;
  PRBool isViewSource;

  nsCOMPtr<nsINetUtil> ioService;

  {
    nsAutoMonitor lock(mMonitor);
    window = do_QueryReferent(mWindow);
    NS_ASSERTION(window, "Window has gone away?!");
    isViewSource = mIsViewSource;
    ioService = mIOService;
  }

  if (!ioService)
  {
    ioService = do_GetService(NS_IOSERVICE_CONTRACTID);
    if (ioService)
    {
      nsAutoMonitor lock(mMonitor);
      mIOService = ioService;
    }
  }

  PRBool isNoContentResponse = PR_FALSE;
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
  if (httpChannel) 
  {
    PRUint32 response;
    isNoContentResponse = NS_SUCCEEDED(httpChannel->GetResponseStatus(&response)) &&
        (response == 204 || response == 205);
  }
  const PRBool isToplevelProgress = (windowForProgress.get() == window.get()) && !isNoContentResponse;
  
#ifdef PR_LOGGING
  if (windowForProgress)
  {
    if (isToplevelProgress)
    {
      PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
             ("SecureUI:%p: OnStateChange: progress: for toplevel\n", this));
    }
    else
    {
      PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
             ("SecureUI:%p: OnStateChange: progress: for something else\n", this));
    }
  }
  else
  {
      PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
             ("SecureUI:%p: OnStateChange: progress: no window known\n", this));
  }
#endif

  PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
         ("SecureUI:%p: OnStateChange\n", this));

  if (isViewSource)
    return NS_OK;

  if (!aRequest)
  {
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: OnStateChange with null request\n", this));
    return NS_ERROR_NULL_POINTER;
  }

#ifdef PR_LOGGING
  if (PR_LOG_TEST(gSecureDocLog, PR_LOG_DEBUG)) {
    nsXPIDLCString reqname;
    aRequest->GetName(reqname);
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: %p %p OnStateChange %x %s\n", this, aWebProgress,
            aRequest, aProgressStateFlags, reqname.get()));
  }
#endif

  nsCOMPtr<nsISupports> securityInfo(ExtractSecurityInfo(aRequest));

  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel)
  {
    channel->GetURI(getter_AddRefs(uri));
    if (uri)
    {
      PRBool vs;
      if (NS_SUCCEEDED(uri->SchemeIs("javascript", &vs)) && vs)
      {
        // We ignore the progress events for javascript URLs.
        // If a document loading gets triggered, we will see more events.
        return NS_OK;
      }
    }
  }

  PRUint32 loadFlags = 0;
  aRequest->GetLoadFlags(&loadFlags);

#ifdef PR_LOGGING
  if (aProgressStateFlags & STATE_START
      &&
      aProgressStateFlags & STATE_IS_REQUEST
      &&
      isToplevelProgress
      &&
      loadFlags & nsIChannel::LOAD_DOCUMENT_URI)
  {
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
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
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: OnStateChange: SOMETHING STOPS FOR TOPMOST DOCUMENT\n", this));
  }
#endif

  PRBool isSubDocumentRelevant = PR_TRUE;

  // We are only interested in requests that load in the browser window...
  nsCOMPtr<imgIRequest> imgRequest(do_QueryInterface(aRequest));
  if (imgRequest) {
    // for image requests, we get the URI from here
    imgRequest->GetURI(getter_AddRefs(uri));
  } else { // is not imgRequest
    nsCOMPtr<nsIHttpChannel> httpRequest(do_QueryInterface(aRequest));
    if (!httpRequest) {
      nsCOMPtr<nsIFileChannel> fileRequest(do_QueryInterface(aRequest));
      if (!fileRequest) {
        nsCOMPtr<nsIWyciwygChannel> wyciwygRequest(do_QueryInterface(aRequest));
        if (!wyciwygRequest) {
          nsCOMPtr<nsIFTPChannel> ftpRequest(do_QueryInterface(aRequest));
          if (!ftpRequest) {
            PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
                   ("SecureUI:%p: OnStateChange: not relevant for sub content\n", this));
            isSubDocumentRelevant = PR_FALSE;
          }
        }
      }
    }
  }

  // This will ignore all resource, chrome, data, file, moz-icon, and anno
  // protocols. Local resources are treated as trusted.
  if (uri && ioService) {
    PRBool hasFlag;
    nsresult rv = 
      ioService->URIChainHasFlags(uri, 
                                  nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                                  &hasFlag);
    if (NS_SUCCEEDED(rv) && hasFlag) {
      isSubDocumentRelevant = PR_FALSE;
    }
  }

#if defined(DEBUG)
  nsCString info2;
  PRUint32 testFlags = loadFlags;

  if (testFlags & nsIChannel::LOAD_DOCUMENT_URI)
  {
    testFlags -= nsIChannel::LOAD_DOCUMENT_URI;
    info2.Append("LOAD_DOCUMENT_URI ");
  }
  if (testFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI)
  {
    testFlags -= nsIChannel::LOAD_RETARGETED_DOCUMENT_URI;
    info2.Append("LOAD_RETARGETED_DOCUMENT_URI ");
  }
  if (testFlags & nsIChannel::LOAD_REPLACE)
  {
    testFlags -= nsIChannel::LOAD_REPLACE;
    info2.Append("LOAD_REPLACE ");
  }

  const char *_status = NS_SUCCEEDED(aStatus) ? "1" : "0";

  nsCString info;
  PRUint32 f = aProgressStateFlags;
  if (f & nsIWebProgressListener::STATE_START)
  {
    f -= nsIWebProgressListener::STATE_START;
    info.Append("START ");
  }
  if (f & nsIWebProgressListener::STATE_REDIRECTING)
  {
    f -= nsIWebProgressListener::STATE_REDIRECTING;
    info.Append("REDIRECTING ");
  }
  if (f & nsIWebProgressListener::STATE_TRANSFERRING)
  {
    f -= nsIWebProgressListener::STATE_TRANSFERRING;
    info.Append("TRANSFERRING ");
  }
  if (f & nsIWebProgressListener::STATE_NEGOTIATING)
  {
    f -= nsIWebProgressListener::STATE_NEGOTIATING;
    info.Append("NEGOTIATING ");
  }
  if (f & nsIWebProgressListener::STATE_STOP)
  {
    f -= nsIWebProgressListener::STATE_STOP;
    info.Append("STOP ");
  }
  if (f & nsIWebProgressListener::STATE_IS_REQUEST)
  {
    f -= nsIWebProgressListener::STATE_IS_REQUEST;
    info.Append("IS_REQUEST ");
  }
  if (f & nsIWebProgressListener::STATE_IS_DOCUMENT)
  {
    f -= nsIWebProgressListener::STATE_IS_DOCUMENT;
    info.Append("IS_DOCUMENT ");
  }
  if (f & nsIWebProgressListener::STATE_IS_NETWORK)
  {
    f -= nsIWebProgressListener::STATE_IS_NETWORK;
    info.Append("IS_NETWORK ");
  }
  if (f & nsIWebProgressListener::STATE_IS_WINDOW)
  {
    f -= nsIWebProgressListener::STATE_IS_WINDOW;
    info.Append("IS_WINDOW ");
  }
  if (f & nsIWebProgressListener::STATE_IS_INSECURE)
  {
    f -= nsIWebProgressListener::STATE_IS_INSECURE;
    info.Append("IS_INSECURE ");
  }
  if (f & nsIWebProgressListener::STATE_IS_BROKEN)
  {
    f -= nsIWebProgressListener::STATE_IS_BROKEN;
    info.Append("IS_BROKEN ");
  }
  if (f & nsIWebProgressListener::STATE_IS_SECURE)
  {
    f -= nsIWebProgressListener::STATE_IS_SECURE;
    info.Append("IS_SECURE ");
  }
  if (f & nsIWebProgressListener::STATE_SECURE_HIGH)
  {
    f -= nsIWebProgressListener::STATE_SECURE_HIGH;
    info.Append("SECURE_HIGH ");
  }
  if (f & nsIWebProgressListener::STATE_SECURE_MED)
  {
    f -= nsIWebProgressListener::STATE_SECURE_MED;
    info.Append("SECURE_MED ");
  }
  if (f & nsIWebProgressListener::STATE_SECURE_LOW)
  {
    f -= nsIWebProgressListener::STATE_SECURE_LOW;
    info.Append("SECURE_LOW ");
  }
  if (f & nsIWebProgressListener::STATE_RESTORING)
  {
    f -= nsIWebProgressListener::STATE_RESTORING;
    info.Append("STATE_RESTORING ");
  }

  if (f > 0)
  {
    info.Append("f contains unknown flag!");
  }

  PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
         ("SecureUI:%p: OnStateChange: %s %s -- %s\n", this, _status, 
          info.get(), info2.get()));

  if (aProgressStateFlags & STATE_STOP
      &&
      channel)
  {
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: OnStateChange: seeing STOP with security state: %d\n", this,
            GetSecurityStateFromSecurityInfo(securityInfo)
            ));
  }
#endif

  if (aProgressStateFlags & STATE_TRANSFERRING
      &&
      aProgressStateFlags & STATE_IS_REQUEST)
  {
    // The listing of a request in mTransferringRequests
    // means, there has already been data transfered.

    nsAutoMonitor lock(mMonitor);
    PL_DHashTableOperate(&mTransferringRequests, aRequest, PL_DHASH_ADD);
    
    return NS_OK;
  }

  PRBool requestHasTransferedData = PR_FALSE;

  if (aProgressStateFlags & STATE_STOP
      &&
      aProgressStateFlags & STATE_IS_REQUEST)
  {
    { /* scope for the nsAutoMonitor */
      nsAutoMonitor lock(mMonitor);
      PLDHashEntryHdr *entry = PL_DHashTableOperate(&mTransferringRequests, aRequest, PL_DHASH_LOOKUP);
      if (PL_DHASH_ENTRY_IS_BUSY(entry))
      {
        PL_DHashTableOperate(&mTransferringRequests, aRequest, PL_DHASH_REMOVE);

        requestHasTransferedData = PR_TRUE;
      }
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
      PRBool hasTransferred;
      requestHasTransferedData =
        securityInfoProvider &&
        (NS_FAILED(securityInfoProvider->GetHasTransferredData(&hasTransferred)) ||
         hasTransferred);
    }
  }

  PRBool allowSecurityStateChange = PR_TRUE;
  if (loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI)
  {
    // The original consumer (this) is no longer the target of the load.
    // Ignore any events with this flag, do not allow them to update
    // our secure UI state.
    allowSecurityStateChange = PR_FALSE;
  }

  if (aProgressStateFlags & STATE_START
      &&
      aProgressStateFlags & STATE_IS_REQUEST
      &&
      isToplevelProgress
      &&
      loadFlags & nsIChannel::LOAD_DOCUMENT_URI)
  {
    PRBool inProgress;

    PRInt32 saveSubHigh;
    PRInt32 saveSubLow;
    PRInt32 saveSubBroken;
    PRInt32 saveSubNo;
    nsCOMPtr<nsIAssociatedContentSecurity> prevContentSecurity;

    PRInt32 newSubHigh = 0;
    PRInt32 newSubLow = 0;
    PRInt32 newSubBroken = 0;
    PRInt32 newSubNo = 0;

    {
      nsAutoMonitor lock(mMonitor);
      inProgress = (mDocumentRequestsInProgress!=0);

      if (allowSecurityStateChange && !inProgress)
      {
        saveSubHigh = mSubRequestsHighSecurity;
        saveSubLow = mSubRequestsLowSecurity;
        saveSubBroken = mSubRequestsBrokenSecurity;
        saveSubNo = mSubRequestsNoSecurity;
        prevContentSecurity = do_QueryInterface(mCurrentToplevelSecurityInfo);
      }
    }

    if (allowSecurityStateChange && !inProgress)
    {
      PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
             ("SecureUI:%p: OnStateChange: start for toplevel document\n", this
              ));

      if (prevContentSecurity)
      {
        PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
               ("SecureUI:%p: OnStateChange: start, saving current sub state\n", this
                ));
  
        // before resetting our state, let's save information about
        // sub element loads, so we can restore it later
        prevContentSecurity->SetCountSubRequestsHighSecurity(saveSubHigh);
        prevContentSecurity->SetCountSubRequestsLowSecurity(saveSubLow);
        prevContentSecurity->SetCountSubRequestsBrokenSecurity(saveSubBroken);
        prevContentSecurity->SetCountSubRequestsNoSecurity(saveSubNo);
        prevContentSecurity->Flush();
      }

      PRBool retrieveAssociatedState = PR_FALSE;

      if (securityInfo &&
          (aProgressStateFlags & nsIWebProgressListener::STATE_RESTORING) != 0) {
        retrieveAssociatedState = PR_TRUE;
      } else {
        nsCOMPtr<nsIWyciwygChannel> wyciwygRequest(do_QueryInterface(aRequest));
        if (wyciwygRequest) {
          retrieveAssociatedState = PR_TRUE;
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
          PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
                 ("SecureUI:%p: OnStateChange: start, loading old sub state\n", this
                  ));
    
          newContentSecurity->GetCountSubRequestsHighSecurity(&newSubHigh);
          newContentSecurity->GetCountSubRequestsLowSecurity(&newSubLow);
          newContentSecurity->GetCountSubRequestsBrokenSecurity(&newSubBroken);
          newContentSecurity->GetCountSubRequestsNoSecurity(&newSubNo);
        }
      }
    }

    {
      nsAutoMonitor lock(mMonitor);

      if (allowSecurityStateChange && !inProgress)
      {
        ResetStateTracking();
        mSubRequestsHighSecurity = newSubHigh;
        mSubRequestsLowSecurity = newSubLow;
        mSubRequestsBrokenSecurity = newSubBroken;
        mSubRequestsNoSecurity = newSubNo;
        mNewToplevelSecurityStateKnown = PR_FALSE;
      }

      // By using a counter, this code also works when the toplevel
      // document get's redirected, but the STOP request for the 
      // previous toplevel document has not yet have been received.
      PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
             ("SecureUI:%p: OnStateChange: ++mDocumentRequestsInProgress\n", this
              ));
      ++mDocumentRequestsInProgress;
    }

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
    PRInt32 temp_DocumentRequestsInProgress;
    nsCOMPtr<nsISecurityEventSink> temp_ToplevelEventSink;

    {
      nsAutoMonitor lock(mMonitor);
      temp_DocumentRequestsInProgress = mDocumentRequestsInProgress;
      if (allowSecurityStateChange)
      {
        temp_ToplevelEventSink = mToplevelEventSink;
      }
    }

    if (temp_DocumentRequestsInProgress <= 0)
    {
      // Ignore stop requests unless a document load is in progress
      // Unfortunately on application start, see some stops without having seen any starts...
      return NS_OK;
    }

    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: OnStateChange: --mDocumentRequestsInProgress\n", this
            ));

    if (!temp_ToplevelEventSink && channel)
    {
      if (allowSecurityStateChange)
      {
        ObtainEventSink(channel, temp_ToplevelEventSink);
      }
    }

    {
      nsAutoMonitor lock(mMonitor);
      if (allowSecurityStateChange)
      {
        mToplevelEventSink = temp_ToplevelEventSink;
      }
      --mDocumentRequestsInProgress;
    }

    if (allowSecurityStateChange && requestHasTransferedData) {
      // Data has been transferred for the single toplevel
      // request. Evaluate the security state.

      return EvaluateAndUpdateSecurityState(aRequest, securityInfo, PR_FALSE);
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
      UpdateSubrequestMembers(securityInfo);
      
      // Care for the following scenario:
      // A new top level document load might have already started,
      // but the security state of the new top level document might not yet been known.
      // 
      // At this point, we are learning about the security state of a sub-document.
      // We must not update the security state based on the sub content,
      // if the new top level state is not yet known.
      //
      // We skip updating the security state in this case.

      PRBool temp_NewToplevelSecurityStateKnown;
      {
        nsAutoMonitor lock(mMonitor);
        temp_NewToplevelSecurityStateKnown = mNewToplevelSecurityStateKnown;
      }

      if (temp_NewToplevelSecurityStateKnown)
        return UpdateSecurityState(aRequest, PR_FALSE, PR_FALSE, PR_FALSE);
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

nsresult nsSecureBrowserUIImpl::UpdateSecurityState(nsIRequest* aRequest, 
                                                    PRBool withNewLocation, 
                                                    PRBool withUpdateStatus, 
                                                    PRBool withUpdateTooltip)
{
  lockIconState warnSecurityState = lis_no_security;
  PRBool showWarning = PR_FALSE;
  nsresult rv = NS_OK;

  // both parameters are both input and outout
  PRBool flagsChanged = UpdateMyFlags(showWarning, warnSecurityState);

  if (flagsChanged || withNewLocation || withUpdateStatus || withUpdateTooltip)
    rv = TellTheWorld(showWarning, warnSecurityState, aRequest);

  return rv;
}

// must not fail, by definition, only trivial assignments
// or string operations are allowed
// returns true if our overall state has changed and we must send out notifications
PRBool nsSecureBrowserUIImpl::UpdateMyFlags(PRBool &showWarning, lockIconState &warnSecurityState)
{
  nsAutoMonitor lock(mMonitor);
  PRBool mustTellTheWorld = PR_FALSE;

  lockIconState newSecurityState;

  if (mNewToplevelSecurityState & STATE_IS_SECURE)
  {
    if (mNewToplevelSecurityState & STATE_SECURE_LOW
        ||
        mNewToplevelSecurityState & STATE_SECURE_MED)
    {
      if (mSubRequestsBrokenSecurity
          ||
          mSubRequestsNoSecurity)
      {
        newSecurityState = lis_mixed_security;
      }
      else
      {
        newSecurityState = lis_low_security;
      }
    }
    else
    {
      // toplevel is high security

      if (mSubRequestsBrokenSecurity
          ||
          mSubRequestsNoSecurity)
      {
        newSecurityState = lis_mixed_security;
      }
      else if (mSubRequestsLowSecurity)
      {
        newSecurityState = lis_low_security;
      }
      else
      {
        newSecurityState = lis_high_security;
      }
    }
  }
  else
  if (mNewToplevelSecurityState & STATE_IS_BROKEN)
  {
    // indicating BROKEN is more important than MIXED.
  
    newSecurityState = lis_broken_security;
  }
  else
  {
    newSecurityState = lis_no_security;
  }

  PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
         ("SecureUI:%p: UpdateSecurityState:  old-new  %d - %d\n", this,
         mNotifiedSecurityState, newSecurityState
          ));

  if (mNotifiedSecurityState != newSecurityState)
  {
    mustTellTheWorld = PR_TRUE;

    // we'll treat "broken" exactly like "insecure",
    // i.e. we do not show alerts when switching between broken and insecure

    /*
      from                 to           shows alert
    ------------------------------     ---------------

    no or broken -> no or broken    => <NOTHING SHOWN>

    no or broken -> mixed           => mixed alert
    no or broken -> low             => low alert
    no or broken -> high            => high alert
    
    mixed, high, low -> no, broken  => leaving secure

    mixed        -> low             => low alert
    mixed        -> high            => high alert

    high         -> low             => low alert
    high         -> mixed           => mixed
    
    low          -> high            => high
    low          -> mixed           => mixed


      security    icon
      ----------------
    
      no          open
      mixed       broken
      broken      broken
      low         low
      high        high
    */

    showWarning = PR_TRUE;
    
    switch (mNotifiedSecurityState)
    {
      case lis_no_security:
      case lis_broken_security:
        switch (newSecurityState)
        {
          case lis_no_security:
          case lis_broken_security:
            showWarning = PR_FALSE;
            break;
          
          default:
            break;
        }
      
      default:
        break;
    }

    if (showWarning)
    {
      warnSecurityState = newSecurityState;
    }
    
    mNotifiedSecurityState = newSecurityState;

    if (lis_no_security == newSecurityState)
    {
      mSSLStatus = nsnull;
      mInfoTooltip.Truncate();
    }
  }

  if (mNotifiedToplevelIsEV != mNewToplevelIsEV) {
    mustTellTheWorld = PR_TRUE;
    mNotifiedToplevelIsEV = mNewToplevelIsEV;
  }

  return mustTellTheWorld;
}

nsresult nsSecureBrowserUIImpl::TellTheWorld(PRBool showWarning, 
                                             lockIconState warnSecurityState, 
                                             nsIRequest* aRequest)
{
  nsCOMPtr<nsISecurityEventSink> temp_ToplevelEventSink;
  lockIconState temp_NotifiedSecurityState;
  PRBool temp_NotifiedToplevelIsEV;

  {
    nsAutoMonitor lock(mMonitor);
    temp_ToplevelEventSink = mToplevelEventSink;
    temp_NotifiedSecurityState = mNotifiedSecurityState;
    temp_NotifiedToplevelIsEV = mNotifiedToplevelIsEV;
  }

  if (temp_ToplevelEventSink)
  {
    PRUint32 newState = STATE_IS_INSECURE;
    MapInternalToExternalState(&newState, 
                               temp_NotifiedSecurityState, 
                               temp_NotifiedToplevelIsEV);

    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: UpdateSecurityState: calling OnSecurityChange\n", this
            ));

    temp_ToplevelEventSink->OnSecurityChange(aRequest, newState);
  }
  else
  {
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: UpdateSecurityState: NO mToplevelEventSink!\n", this
            ));

  }

  if (showWarning)
  {
    switch (warnSecurityState)
    {
      case lis_no_security:
      case lis_broken_security:
        ConfirmLeavingSecure();
        break;

      case lis_mixed_security:
        ConfirmMixedMode();
        break;

      case lis_low_security:
        ConfirmEnteringWeak();
        break;

      case lis_high_security:
        ConfirmEnteringSecure();
        break;
    }
  }

  return NS_OK; 
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnLocationChange(nsIWebProgress* aWebProgress,
                                        nsIRequest* aRequest,
                                        nsIURI* aLocation)
{
#ifdef DEBUG
  nsAutoAtomic atomic(mOnStateLocationChangeReentranceDetection);
  NS_ASSERTION(mOnStateLocationChangeReentranceDetection == 1,
               "unexpected parallel nsIWebProgress OnStateChange and/or OnLocationChange notification");
#endif
  PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
         ("SecureUI:%p: OnLocationChange\n", this));

  PRBool updateIsViewSource = PR_FALSE;
  PRBool temp_IsViewSource = PR_FALSE;
  nsCOMPtr<nsIDOMWindow> window;

  if (aLocation)
  {
    PRBool vs;

    nsresult rv = aLocation->SchemeIs("view-source", &vs);
    NS_ENSURE_SUCCESS(rv, rv);

    if (vs) {
      PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
             ("SecureUI:%p: OnLocationChange: view-source\n", this));
    }

    updateIsViewSource = PR_TRUE;
    temp_IsViewSource = vs;
  }

  {
    nsAutoMonitor lock(mMonitor);
    if (updateIsViewSource) {
      mIsViewSource = temp_IsViewSource;
    }
    mCurrentURI = aLocation;
    window = do_QueryReferent(mWindow);
    NS_ASSERTION(window, "Window has gone away?!");
  }

  // If the location change does not have a corresponding request, then we
  // assume that it does not impact the security state.
  if (!aRequest)
    return NS_OK;

  // The location bar has changed, so we must update the security state.  The
  // only concern with doing this here is that a page may transition from being
  // reported as completely secure to being reported as partially secure
  // (mixed).  This may be confusing for users, and it may bother users who
  // like seeing security dialogs.  However, it seems prudent given that page
  // loading may never end in some edge cases (perhaps by a site with malicious
  // intent).

  nsCOMPtr<nsIDOMWindow> windowForProgress;
  aWebProgress->GetDOMWindow(getter_AddRefs(windowForProgress));

  nsCOMPtr<nsISupports> securityInfo(ExtractSecurityInfo(aRequest));

  if (windowForProgress.get() == window.get()) {
    // For toplevel channels, update the security state right away.
    return EvaluateAndUpdateSecurityState(aRequest, securityInfo, PR_TRUE);
  }

  // For channels in subdocuments we only update our subrequest state members.
  UpdateSubrequestMembers(securityInfo);

  // Care for the following scenario:

  // A new toplevel document load might have already started, but the security
  // state of the new toplevel document might not yet be known.
  // 
  // At this point, we are learning about the security state of a sub-document.
  // We must not update the security state based on the sub content, if the new
  // top level state is not yet known.
  //
  // We skip updating the security state in this case.

  PRBool temp_NewToplevelSecurityStateKnown;
  {
    nsAutoMonitor lock(mMonitor);
    temp_NewToplevelSecurityStateKnown = mNewToplevelSecurityStateKnown;
  }

  if (temp_NewToplevelSecurityStateKnown)
    return UpdateSecurityState(aRequest, PR_TRUE, PR_FALSE, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnStatusChange(nsIWebProgress* aWebProgress,
                                      nsIRequest* aRequest,
                                      nsresult aStatus,
                                      const PRUnichar* aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

nsresult
nsSecureBrowserUIImpl::OnSecurityChange(nsIWebProgress *aWebProgress,
                                        nsIRequest *aRequest,
                                        PRUint32 state)
{
#if defined(DEBUG)
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (!channel)
    return NS_OK;

  nsCOMPtr<nsIURI> aURI;
  channel->GetURI(getter_AddRefs(aURI));
  
  if (aURI) {
    nsCAutoString temp;
    aURI->GetSpec(temp);
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: OnSecurityChange: (%x) %s\n", this,
            state, temp.get()));
  }
#endif

  return NS_OK;
}

// nsISSLStatusProvider methods
NS_IMETHODIMP
nsSecureBrowserUIImpl::GetSSLStatus(nsISupports** _result)
{
  NS_ENSURE_ARG_POINTER(_result);

  nsAutoMonitor lock(mMonitor);

  switch (mNotifiedSecurityState)
  {
    case lis_mixed_security:
    case lis_low_security:
    case lis_high_security:
      break;

    default:
      NS_NOTREACHED("if this is reached you must add more entries to the switch");
    case lis_no_security:
    case lis_broken_security:
      *_result = nsnull;
      return NS_OK;
  }
 
  *_result = mSSLStatus;
  NS_IF_ADDREF(*_result);

  return NS_OK;
}

nsresult
nsSecureBrowserUIImpl::IsURLHTTPS(nsIURI* aURL, PRBool* value)
{
  *value = PR_FALSE;

  if (!aURL)
    return NS_OK;

  return aURL->SchemeIs("https", value);
}

nsresult
nsSecureBrowserUIImpl::IsURLJavaScript(nsIURI* aURL, PRBool* value)
{
  *value = PR_FALSE;

  if (!aURL)
    return NS_OK;

  return aURL->SchemeIs("javascript", value);
}

void
nsSecureBrowserUIImpl::GetBundleString(const PRUnichar* name,
                                       nsAString &outString)
{
  nsCOMPtr<nsIStringBundle> temp_StringBundle;

  {
    nsAutoMonitor lock(mMonitor);
    temp_StringBundle = mStringBundle;
  }

  if (temp_StringBundle && name) {
    PRUnichar *ptrv = nsnull;
    if (NS_SUCCEEDED(temp_StringBundle->GetStringFromName(name,
                                                          &ptrv)))
      outString = ptrv;
    else
      outString.SetLength(0);

    nsMemory::Free(ptrv);

  } else {
    outString.SetLength(0);
  }
}

nsresult
nsSecureBrowserUIImpl::CheckPost(nsIURI *formURL, nsIURI *actionURL, PRBool *okayToPost)
{
  PRBool formSecure, actionSecure, actionJavaScript;
  *okayToPost = PR_TRUE;

  nsresult rv = IsURLHTTPS(formURL, &formSecure);
  if (NS_FAILED(rv))
    return rv;

  rv = IsURLHTTPS(actionURL, &actionSecure);
  if (NS_FAILED(rv))
    return rv;

  rv = IsURLJavaScript(actionURL, &actionJavaScript);
  if (NS_FAILED(rv))
    return rv;

  // If we are posting to a secure link, all is okay.
  // It doesn't matter whether the currently viewed page is secure or not,
  // because the data will be sent to a secure URL.
  if (actionSecure) {
    return NS_OK;
  }

  // Action is a JavaScript call, not an actual post. That's okay too.
  if (actionJavaScript) {
    return NS_OK;
  }

  // posting to insecure webpage from a secure webpage.
  if (formSecure) {
    *okayToPost = ConfirmPostToInsecureFromSecure();
  } else {
    *okayToPost = ConfirmPostToInsecure();
  }

  return NS_OK;
}

//
// Implementation of an nsIInterfaceRequestor for use
// as context for NSS calls
//
class nsUIContext : public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR

  nsUIContext(nsIDOMWindow *window);
  virtual ~nsUIContext();

private:
  nsCOMPtr<nsIDOMWindow> mWindow;
};

NS_IMPL_ISUPPORTS1(nsUIContext, nsIInterfaceRequestor)

nsUIContext::nsUIContext(nsIDOMWindow *aWindow)
: mWindow(aWindow)
{
}

nsUIContext::~nsUIContext()
{
}

/* void getInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP nsUIContext::GetInterface(const nsIID & uuid, void * *result)
{
  NS_ENSURE_TRUE(mWindow, NS_ERROR_FAILURE);
  nsresult rv;

  if (uuid.Equals(NS_GET_IID(nsIPrompt))) {
    nsCOMPtr<nsIDOMWindowInternal> internal = do_QueryInterface(mWindow, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIPrompt *prompt;

    rv = internal->GetPrompter(&prompt);
    *result = prompt;
  } else if (uuid.Equals(NS_GET_IID(nsIDOMWindow))) {
    *result = mWindow;
    NS_ADDREF ((nsISupports*) *result);
    rv = NS_OK;
  } else {
    rv = NS_ERROR_NO_INTERFACE;
  }

  return rv;
}

nsresult nsSecureBrowserUIImpl::
GetNSSDialogs(nsISecurityWarningDialogs **result)
{
  nsresult rv;
  nsCOMPtr<nsISecurityWarningDialogs> my_result(do_GetService(NS_SECURITYWARNINGDIALOGS_CONTRACTID, &rv));

  if (NS_FAILED(rv)) 
    return rv;

  nsCOMPtr<nsISupports> proxiedResult;
  NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                       NS_GET_IID(nsISecurityWarningDialogs),
                       my_result, NS_PROXY_SYNC,
                       getter_AddRefs(proxiedResult));

  if (!proxiedResult) {
    return NS_ERROR_FAILURE;
  }

  return CallQueryInterface(proxiedResult, result);
}

PRBool nsSecureBrowserUIImpl::
ConfirmEnteringSecure()
{
  nsCOMPtr<nsISecurityWarningDialogs> dialogs;

  GetNSSDialogs(getter_AddRefs(dialogs));
  if (!dialogs) return PR_FALSE;  // Should this allow PR_TRUE for unimplemented?

  nsCOMPtr<nsIDOMWindow> window;
  {
    nsAutoMonitor lock(mMonitor);
    window = do_QueryReferent(mWindow);
    NS_ASSERTION(window, "Window has gone away?!");
  }

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(window);

  PRBool confirms;
  dialogs->ConfirmEnteringSecure(ctx, &confirms);

  return confirms;
}

PRBool nsSecureBrowserUIImpl::
ConfirmEnteringWeak()
{
  nsCOMPtr<nsISecurityWarningDialogs> dialogs;

  GetNSSDialogs(getter_AddRefs(dialogs));
  if (!dialogs) return PR_FALSE;  // Should this allow PR_TRUE for unimplemented?

  nsCOMPtr<nsIDOMWindow> window;
  {
    nsAutoMonitor lock(mMonitor);
    window = do_QueryReferent(mWindow);
    NS_ASSERTION(window, "Window has gone away?!");
  }

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(window);

  PRBool confirms;
  dialogs->ConfirmEnteringWeak(ctx, &confirms);

  return confirms;
}

PRBool nsSecureBrowserUIImpl::
ConfirmLeavingSecure()
{
  nsCOMPtr<nsISecurityWarningDialogs> dialogs;

  GetNSSDialogs(getter_AddRefs(dialogs));
  if (!dialogs) return PR_FALSE;  // Should this allow PR_TRUE for unimplemented?

  nsCOMPtr<nsIDOMWindow> window;
  {
    nsAutoMonitor lock(mMonitor);
    window = do_QueryReferent(mWindow);
    NS_ASSERTION(window, "Window has gone away?!");
  }

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(window);

  PRBool confirms;
  dialogs->ConfirmLeavingSecure(ctx, &confirms);

  return confirms;
}

PRBool nsSecureBrowserUIImpl::
ConfirmMixedMode()
{
  nsCOMPtr<nsISecurityWarningDialogs> dialogs;

  GetNSSDialogs(getter_AddRefs(dialogs));
  if (!dialogs) return PR_FALSE;  // Should this allow PR_TRUE for unimplemented?

  nsCOMPtr<nsIDOMWindow> window;
  {
    nsAutoMonitor lock(mMonitor);
    window = do_QueryReferent(mWindow);
    NS_ASSERTION(window, "Window has gone away?!");
  }

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(window);

  PRBool confirms;
  dialogs->ConfirmMixedMode(ctx, &confirms);

  return confirms;
}

/**
 * ConfirmPostToInsecure - returns PR_TRUE if
 *   the user approves the submit (or doesn't care).
 *   returns PR_FALSE on errors.
 */
PRBool nsSecureBrowserUIImpl::
ConfirmPostToInsecure()
{
  nsresult rv;

  nsCOMPtr<nsISecurityWarningDialogs> dialogs;

  GetNSSDialogs(getter_AddRefs(dialogs));
  if (!dialogs) return PR_FALSE;  // Should this allow PR_TRUE for unimplemented?

  nsCOMPtr<nsIDOMWindow> window;
  {
    nsAutoMonitor lock(mMonitor);
    window = do_QueryReferent(mWindow);
    NS_ASSERTION(window, "Window has gone away?!");
  }

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(window);

  PRBool result;

  rv = dialogs->ConfirmPostToInsecure(ctx, &result);
  if (NS_FAILED(rv)) return PR_FALSE;

  return result;
}

/**
 * ConfirmPostToInsecureFromSecure - returns PR_TRUE if
 *   the user approves the submit (or doesn't care).
 *   returns PR_FALSE on errors.
 */
PRBool nsSecureBrowserUIImpl::
ConfirmPostToInsecureFromSecure()
{
  nsresult rv;

  nsCOMPtr<nsISecurityWarningDialogs> dialogs;

  GetNSSDialogs(getter_AddRefs(dialogs));
  if (!dialogs) return PR_FALSE;  // Should this allow PR_TRUE for unimplemented?

  nsCOMPtr<nsIDOMWindow> window;
  {
    nsAutoMonitor lock(mMonitor);
    window = do_QueryReferent(mWindow);
    NS_ASSERTION(window, "Window has gone away?!");
  }

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(window);

  PRBool result;

  rv = dialogs->ConfirmPostToInsecureFromSecure(ctx, &result);
  if (NS_FAILED(rv)) return PR_FALSE;

  return result;
}
