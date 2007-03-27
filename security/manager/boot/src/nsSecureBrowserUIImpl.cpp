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
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIFileChannel.h"
#include "nsIWyciwygChannel.h"
#include "nsIFTPChannel.h"
#include "nsITransportSecurityInfo.h"
#include "nsIURI.h"
#include "nsISecurityEventSink.h"
#include "nsIPrompt.h"
#include "nsIFormSubmitObserver.h"
#include "nsISecurityWarningDialogs.h"
#include "nsIProxyObjectManager.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsCRT.h"

#define SECURITY_STRING_BUNDLE_URL "chrome://pipnss/locale/security.properties"

#define IS_SECURE(state) ((state & 0xFFFF) == STATE_IS_SECURE)

#if defined(PR_LOGGING)
//
// Log module for nsSecureBroswerUI logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsSecureBroswerUI:5
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
  const RequestHashEntry *entry = NS_STATIC_CAST(const RequestHashEntry*, hdr);
  return entry->r == key;
}

PR_STATIC_CALLBACK(PRBool)
RequestMapInitEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                     const void *key)
{
  RequestHashEntry *entry = NS_STATIC_CAST(RequestHashEntry*, hdr);
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


nsSecureBrowserUIImpl::nsSecureBrowserUIImpl()
  : mPreviousSecurityState(lis_no_security),
    mIsViewSource(PR_FALSE)
{
  mTransferringRequests.ops = nsnull;
  mNewToplevelSecurityState = STATE_IS_INSECURE;
  mNewToplevelSecurityStateKnown = PR_TRUE;
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
}

NS_IMPL_ISUPPORTS6(nsSecureBrowserUIImpl,
                   nsISecureBrowserUI,
                   nsIWebProgressListener,
                   nsIFormSubmitObserver,
                   nsIObserver,
                   nsISupportsWeakReference,
                   nsISSLStatusProvider)


NS_IMETHODIMP
nsSecureBrowserUIImpl::Init(nsIDOMWindow *window)
{
  PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
         ("SecureUI:%p: Init: mWindow: %p, window: %p\n", this, mWindow.get(),
          window));

  if (!window) {
    NS_WARNING("Null window passed to nsSecureBrowserUIImpl::Init()");
    return NS_ERROR_INVALID_ARG;
  }

  if (mWindow) {
    NS_WARNING("Trying to init an nsSecureBrowserUIImpl twice");
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  
  nsresult rv = NS_OK;
  mWindow = window;

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
  
  nsCOMPtr<nsPIDOMWindow> piwindow(do_QueryInterface(mWindow));
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
  
  wp->AddProgressListener(NS_STATIC_CAST(nsIWebProgressListener*,this),
                          nsIWebProgress::NOTIFY_STATE_ALL | 
                          nsIWebProgress::NOTIFY_LOCATION  |
                          nsIWebProgress::NOTIFY_SECURITY);


  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::GetState(PRUint32* aState)
{
  NS_ENSURE_ARG(aState);
  
  switch (mPreviousSecurityState)
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
  
  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::GetTooltipText(nsAString& aText)
{
  if (mPreviousSecurityState == lis_mixed_security)
  {
    GetBundleString(NS_LITERAL_STRING("SecurityButtonMixedContentTooltipText").get(),
                    aText);
  }
  else if (!mInfoTooltip.IsEmpty())
  {
    aText = mInfoTooltip;
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

static PRUint32 GetSecurityStateFromChannel(nsIChannel* aChannel)
{
  nsresult res;
  PRUint32 securityState;

  // qi for the psm information about this channel load.
  nsCOMPtr<nsISupports> info;
  aChannel->GetSecurityInfo(getter_AddRefs(info));
  nsCOMPtr<nsITransportSecurityInfo> psmInfo(do_QueryInterface(info));
  if (!psmInfo) {
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI: GetSecurityState:%p - no nsITransportSecurityInfo for %p\n",
                                         aChannel, (nsISupports *)info));
    return nsIWebProgressListener::STATE_IS_INSECURE;
  }
  PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI: GetSecurityState:%p - info is %p\n", aChannel,
                                       (nsISupports *)info));
  
  res = psmInfo->GetSecurityState(&securityState);
  if (NS_FAILED(res)) {
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI: GetSecurityState:%p - GetSecurityState failed: %d\n",
                                         aChannel, res));
    securityState = nsIWebProgressListener::STATE_IS_BROKEN;
  }
  
  PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI: GetSecurityState:%p - Returning %d\n", aChannel,
                                       securityState));
  return securityState;
}


NS_IMETHODIMP
nsSecureBrowserUIImpl::Notify(nsIContent* formNode,
                              nsIDOMWindowInternal* window, nsIURI* actionURL,
                              PRBool* cancelSubmit)
{
  // Return NS_OK unless we want to prevent this form from submitting.
  *cancelSubmit = PR_FALSE;
  if (!window || !actionURL || !formNode)
    return NS_OK;
  
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

  PRBool isChild;
  IsChildOfDomWindow(mWindow, postingWindow, &isChild);
  
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
  mInfoTooltip.Truncate();
  mDocumentRequestsInProgress = 0;
  mSubRequestsHighSecurity = 0;
  mSubRequestsLowSecurity = 0;
  mSubRequestsBrokenSecurity = 0;
  mSubRequestsNoSecurity = 0;
  if (mTransferringRequests.ops) {
    PL_DHashTableFinish(&mTransferringRequests);
    mTransferringRequests.ops = nsnull;
  }
  PL_DHashTableInit(&mTransferringRequests, &gMapOps, nsnull,
                    sizeof(RequestHashEntry), 16);
}

nsresult
nsSecureBrowserUIImpl::EvaluateAndUpdateSecurityState(nsIRequest *aRequest)
{
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));

  if (!channel) {
    mNewToplevelSecurityState = nsIWebProgressListener::STATE_IS_INSECURE;
  } else {
    mNewToplevelSecurityState = GetSecurityStateFromChannel(channel);

    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: OnStateChange: remember mNewToplevelSecurityState => %x\n", this,
            mNewToplevelSecurityState));

    // Get SSL Status information if possible
    nsCOMPtr<nsISupports> info;
    channel->GetSecurityInfo(getter_AddRefs(info));
    nsCOMPtr<nsISSLStatusProvider> sp = do_QueryInterface(info);
    if (sp) {
      // Ignore result
      sp->GetSSLStatus(getter_AddRefs(mSSLStatus));
    }

    if (info) {
      nsCOMPtr<nsITransportSecurityInfo> secInfo(do_QueryInterface(info));
      if (secInfo) {
        secInfo->GetShortSecurityDescription(getter_Copies(mInfoTooltip));
      }
    }
  }

  // assume mNewToplevelSecurityState was set in this scope!
  // see code that is directly above

  mNewToplevelSecurityStateKnown = PR_TRUE;
  return UpdateSecurityState(aRequest);
}

void
nsSecureBrowserUIImpl::UpdateSubrequestMembers(nsIRequest *aRequest)
{
  // For wyciwyg channels in subdocuments we only update our
  // subrequest state members.
  PRUint32 reqState = nsIWebProgressListener::STATE_IS_INSECURE;
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));

  if (channel) {
    reqState = GetSecurityStateFromChannel(channel);
  }

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
  /*
    All discussion, unless otherwise mentioned, only refers to
    http, https, file or wyciwig requests.


    Redirects are evil, well, some of them.
    There are mutliple forms of redirects.

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

  const PRBool isToplevelProgress = (windowForProgress.get() == mWindow.get());
  
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

  if (mIsViewSource)
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

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));

  if (channel)
  {
    nsCOMPtr<nsIURI> uri;
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
            GetSecurityStateFromChannel(channel)
            ));
  }
#endif

  if (aProgressStateFlags & STATE_TRANSFERRING
      &&
      aProgressStateFlags & STATE_IS_REQUEST)
  {
    // The listing of a request in mTransferringRequests
    // means, there has already been data transfered.

    PL_DHashTableOperate(&mTransferringRequests, aRequest, PL_DHASH_ADD);
    
    return NS_OK;
  }

  PRBool requestHasTransferedData = PR_FALSE;

  if (aProgressStateFlags & STATE_STOP
      &&
      aProgressStateFlags & STATE_IS_REQUEST)
  {
    PLDHashEntryHdr *entry = PL_DHashTableOperate(&mTransferringRequests, aRequest, PL_DHASH_LOOKUP);
    if (PL_DHASH_ENTRY_IS_BUSY(entry))
    {
      PL_DHashTableOperate(&mTransferringRequests, aRequest, PL_DHASH_REMOVE);

      requestHasTransferedData = PR_TRUE;
    }
  }

  if (loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI)
  {
    // The original consumer (this) is no longer the target of the load.
    // Ignore any events with this flag, do not allow them to update
    // our secure UI state.
    return NS_OK;
  }

  if (aProgressStateFlags & STATE_START
      &&
      aProgressStateFlags & STATE_IS_REQUEST
      &&
      isToplevelProgress
      &&
      loadFlags & nsIChannel::LOAD_DOCUMENT_URI)
  {
    if (!mDocumentRequestsInProgress)
    {
      PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
             ("SecureUI:%p: OnStateChange: start for toplevel document\n", this
              ));

      ResetStateTracking();
      mNewToplevelSecurityStateKnown = PR_FALSE;
    }

    // By using a counter, this code also works when the toplevel
    // document get's redirected, but the STOP request for the 
    // previous toplevel document has not yet have been received.

    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
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
    if (mDocumentRequestsInProgress <= 0)
    {
      // Ignore stop requests unless a document load is in progress
      // Unfortunately on application start, see some stops without having seen any starts...
      return NS_OK;
    }

    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: OnStateChange: --mDocumentRequestsInProgress\n", this
            ));

    if (!mToplevelEventSink && channel)
    {
      ObtainEventSink(channel);
    }

    --mDocumentRequestsInProgress;

    if (requestHasTransferedData) {
      // Data has been transferred for the single toplevel
      // request. Evaluate the security state.

      return EvaluateAndUpdateSecurityState(aRequest);
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

    if (requestHasTransferedData)
    {  
      UpdateSubrequestMembers(aRequest);
      
      // Care for the following scenario:
      // A new top level document load might have already started,
      // but the security state of the new top level document might not yet been known.
      // 
      // At this point, we are learning about the security state of a sub-document.
      // We must not update the security state based on the sub content,
      // if the new top level state is not yet known.
      //
      // We skip updating the security state in this case.

      if (mNewToplevelSecurityStateKnown)
        return UpdateSecurityState(aRequest);
    }

    return NS_OK;
  }

  return NS_OK;
}

void nsSecureBrowserUIImpl::ObtainEventSink(nsIChannel *channel)
{
  if (!mToplevelEventSink)
    NS_QueryNotificationCallbacks(channel, mToplevelEventSink);
}

nsresult nsSecureBrowserUIImpl::UpdateSecurityState(nsIRequest* aRequest)
{
  lockIconState newSecurityState;

  PRBool showWarning = PR_FALSE;
  lockIconState warnSecurityState;

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
         mPreviousSecurityState, newSecurityState
          ));

  if (mPreviousSecurityState != newSecurityState)
  {
    // must show alert
    
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
    
    switch (mPreviousSecurityState)
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
    
    mPreviousSecurityState = newSecurityState;

    if (lis_no_security == newSecurityState)
    {
      mSSLStatus = nsnull;
      mInfoTooltip.Truncate();
    }
  }

  if (mToplevelEventSink)
  {
    PRUint32 newState = STATE_IS_INSECURE;

    switch (newSecurityState)
    {
      case lis_broken_security:
        newState = STATE_IS_BROKEN;
        break;

      case lis_mixed_security:
        newState = STATE_IS_BROKEN;
        break;

      case lis_low_security:
        newState = STATE_IS_SECURE | STATE_SECURE_LOW;
        break;

      case lis_high_security:
        newState = STATE_IS_SECURE | STATE_SECURE_HIGH;
        break;

      default:
      case lis_no_security:
        newState = STATE_IS_INSECURE;
        break;

    }

    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: UpdateSecurityState: calling OnSecurityChange\n", this
            ));

    mToplevelEventSink->OnSecurityChange(aRequest, newState);
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
  if (aLocation)
  {
    PRBool vs;

    nsresult rv = aLocation->SchemeIs("view-source", &vs);
    NS_ENSURE_SUCCESS(rv, rv);

    if (vs) {
      PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
             ("SecureUI:%p: OnLocationChange: view-source\n", this));
    }

    mIsViewSource = vs;
  }

  mCurrentURI = aLocation;

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

  if (windowForProgress.get() == mWindow.get()) {
    // For toplevel channels, update the security state right away.
    return EvaluateAndUpdateSecurityState(aRequest);
  }

  // For channels in subdocuments we only update our subrequest state members.
  UpdateSubrequestMembers(aRequest);

  // Care for the following scenario:

  // A new toplevel document load might have already started, but the security
  // state of the new toplevel document might not yet be known.
  // 
  // At this point, we are learning about the security state of a sub-document.
  // We must not update the security state based on the sub content, if the new
  // top level state is not yet known.
  //
  // We skip updating the security state in this case.

  if (mNewToplevelSecurityStateKnown)
    return UpdateSecurityState(aRequest);

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
  if (mStringBundle && name) {
    PRUnichar *ptrv = nsnull;
    if (NS_SUCCEEDED(mStringBundle->GetStringFromName(name,
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

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(mWindow);

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

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(mWindow);

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

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(mWindow);

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

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(mWindow);

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

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(mWindow);

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

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(mWindow);

  PRBool result;

  rv = dialogs->ConfirmPostToInsecureFromSecure(ctx, &result);
  if (NS_FAILED(rv)) return PR_FALSE;

  return result;
}
