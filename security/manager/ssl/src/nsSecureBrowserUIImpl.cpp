/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Brian Ryner <bryner@netscape.com>
 *   Terry Hayes <thayes@netscape.com>
 */
#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "nspr.h"
#include "prlog.h"
#include "prmem.h"

#include "nsISecureBrowserUI.h"
#include "nsSecureBrowserUIImpl.h"
#include "nsNSSComponent.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIObserverService.h"
#include "nsObserverService.h"
#include "nsIDocumentLoader.h"
#include "nsCURILoader.h"
#include "nsIDocShell.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindowInternal.h"
#include "nsIContent.h"
#include "nsIWebProgress.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIFileChannel.h"
#include "nsITransportSecurityInfo.h"
#include "nsIURI.h"
#include "nsISecurityEventSink.h"
#include "nsIPrompt.h"
#include "nsIFormSubmitObserver.h"
#include "nsNSSHelper.h"

#include "nsINSSDialogs.h"

static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);

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


nsSecureBrowserUIImpl::nsSecureBrowserUIImpl()
  : mMixContentAlertShown(PR_FALSE),
    mSecurityState(STATE_IS_INSECURE)
{
  NS_INIT_ISUPPORTS();
  
#if defined(PR_LOGGING)
  if (!gSecureDocLog)
    gSecureDocLog = PR_NewLogModule("nsSecureBrowserUI");
#endif /* PR_LOGGING */
}

nsSecureBrowserUIImpl::~nsSecureBrowserUIImpl()
{
  nsresult rv;
  // remove self from form post notifications:
  nsCOMPtr<nsIObserverService> svc(do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) {
    svc->RemoveObserver(this, NS_FORMSUBMIT_SUBJECT);
  }
}

NS_IMPL_ISUPPORTS6(nsSecureBrowserUIImpl,
                   nsISecureBrowserUI,
                   nsIWebProgressListener,
                   nsIFormSubmitObserver,
                   nsIObserver,
                   nsISupportsWeakReference,
                   nsISSLStatusProvider);


NS_IMETHODIMP
nsSecureBrowserUIImpl::Init(nsIDOMWindow *window,
                            nsIDOMElement *button)
{
  nsresult rv = NS_OK;
  mSecurityButton = button;  /* may be null */
  mWindow = window;

  nsCOMPtr<nsIStringBundleService> service(do_GetService(kCStringBundleServiceCID, &rv));
  if (NS_FAILED(rv)) return rv;
  
  rv = service->CreateBundle(SECURITY_STRING_BUNDLE_URL,
                             getter_AddRefs(mStringBundle));
  if (NS_FAILED(rv)) return rv;
  
  // hook up to the form post notifications:
  nsCOMPtr<nsIObserverService> svc(do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = svc->AddObserver(this, NS_FORMSUBMIT_SUBJECT, PR_TRUE);
  }
  
  /* GetWebProgress(mWindow) */
  // hook up to the webprogress notifications.
  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(mWindow));
  if (!sgo) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDocShell> docShell;
  sgo->GetDocShell(getter_AddRefs(docShell));
  if (!docShell) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIWebProgress> wp(do_GetInterface(docShell));
  if (!wp) return NS_ERROR_FAILURE;
  /* end GetWebProgress */
  
  wp->AddProgressListener(NS_STATIC_CAST(nsIWebProgressListener*,this));

  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::DisplayPageInfoUI()
{
#if 0
  nsresult res = NS_OK;
  nsCOMPtr<nsISecurityManagerComponent> psm(do_GetService(PSM_COMPONENT_CONTRACTID,
                                                          &res));
  if (NS_FAILED(res))
    return res;
  
  nsXPIDLCString host;
  if (mCurrentURI)
    mCurrentURI->GetHost(getter_Copies(host));
  
  //    return psm->DisplayPSMAdvisor(mLastPSMStatus, host);
#endif
  return NS_ERROR_NOT_IMPLEMENTED;
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

static PRInt32 GetSecurityStateFromChannel(nsIChannel* aChannel)
{
  nsresult res;
  PRInt32 securityState;

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
  if (!NS_SUCCEEDED(res)) {
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
  
  nsCOMPtr<nsIDocument> document;
  formNode->GetDocument(*getter_AddRefs(document));
  if (!document) return NS_OK;

  nsCOMPtr<nsIURI> formURL;
  document->GetBaseURL(*getter_AddRefs(formURL));
  
  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  document->GetScriptGlobalObject(getter_AddRefs(globalObject));
  nsCOMPtr<nsIDOMWindow> postingWindow(do_QueryInterface(globalObject));
  
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
  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnStateChange(nsIWebProgress* aWebProgress,
                                     nsIRequest* aRequest,
                                     PRInt32 aProgressStateFlags,
                                     nsresult aStatus)
{
  nsresult res = NS_OK;

  if (!aRequest)
    return NS_ERROR_NULL_POINTER;
  
  // Get the channel from the request...
  // If the request is not network based, then ignore it.
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest, &res));
  if (NS_FAILED(res))
    return NS_OK;

  // We are only interested in HTTP and file requests.
  nsCOMPtr<nsIHttpChannel> httpRequest(do_QueryInterface(aRequest));
  nsCOMPtr<nsIFileChannel> fileRequest(do_QueryInterface(aRequest));
  if (!httpRequest && !fileRequest) {
    return NS_OK;
  }
  
  nsCOMPtr<nsIInterfaceRequestor> requestor;
  nsCOMPtr<nsISecurityEventSink> eventSink;
  channel->GetNotificationCallbacks(getter_AddRefs(requestor));
  if (requestor)
    eventSink = do_GetInterface(requestor);
  
#if defined(DEBUG)
  nsCOMPtr<nsIURI> loadingURI;
  res = channel->GetURI(getter_AddRefs(loadingURI));
  NS_ASSERTION(NS_SUCCEEDED(res), "GetURI failed");
  if (loadingURI) {
    nsXPIDLCString temp;
    loadingURI->GetSpec(getter_Copies(temp));
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG,
           ("SecureUI:%p: OnStateChange: %x :%s\n", this,
            aProgressStateFlags,(const char*)temp));
  }
#endif

  // First event when loading doc
  if (aProgressStateFlags & STATE_START) {
    if (aProgressStateFlags & STATE_IS_NETWORK) {
      // Reset state variables used per doc loading
      mMixContentAlertShown = PR_FALSE;
      mFirstRequest = PR_TRUE;
      mSSLStatus = nsnull;
    }
  }
  
  // A Document is starting to load...
  if ((aProgressStateFlags & (STATE_STOP)) &&
      (aProgressStateFlags & STATE_IS_REQUEST)) {

    // work-around for bug 48515.
    nsCOMPtr<nsIURI> aURI;
    channel->GetURI(getter_AddRefs(aURI));

    // Sometimes URI is null, so ignore.
    if (aURI == nsnull) {
      return NS_OK;
    }

    // If this is the first request, then do a protocol check
    if (mFirstRequest) {
      mFirstRequest = PR_FALSE;
      return CheckProtocolContextSwitch(eventSink, aRequest, channel);
    }
    // Check that the request does not have mixed content.
    return CheckMixedContext(eventSink, aRequest, channel);
  }

  // A document has finished loading
  if ((aProgressStateFlags & STATE_STOP) &&
    (aProgressStateFlags & STATE_IS_NETWORK)) {

    // Get SSL Status information if possible
    nsCOMPtr<nsISupports> info;
    channel->GetSecurityInfo(getter_AddRefs(info));
    nsCOMPtr<nsISSLStatusProvider> sp = do_QueryInterface(info);
    if (sp) {
      // Ignore result
      sp->GetSSLStatus(getter_AddRefs(mSSLStatus));
    }

    if (eventSink)
      eventSink->OnSecurityChange(aRequest, mSecurityState);
          
    if (!mSecurityButton)
      return res;
          
    /* TNH - need event for changing the tooltip */

    // Do we really need to look at res here? What happens if there's an error?
    // We should still set the certificate authority display.

    nsXPIDLString tooltip;
    if (info) {
      nsCOMPtr<nsITransportSecurityInfo> secInfo(do_QueryInterface(info));
      if (secInfo &&
          NS_SUCCEEDED(secInfo->GetShortSecurityDescription(getter_Copies(tooltip))) &&
          tooltip) {

        res = mSecurityButton->SetAttribute(NS_LITERAL_STRING("tooltiptext"),
                                                nsString(tooltip));
              
      }
    }
  }

  return res;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnLocationChange(nsIWebProgress* aWebProgress,
                                        nsIRequest* aRequest,
                                        nsIURI* aLocation)
{
  mCurrentURI = aLocation;
  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnStatusChange(nsIWebProgress* aWebProgress,
                                      nsIRequest* aRequest,
                                      nsresult aStatus,
                                      const PRUnichar* aMessage)
{
  return NS_OK;
}

nsresult
nsSecureBrowserUIImpl::OnSecurityChange(nsIWebProgress *aWebProgress,
                                        nsIRequest *aRequest,
                                        PRInt32 state)
{
  nsresult res = NS_OK;

#if defined(DEBUG_dougt)
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (!channel)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIURI> aURI;
  channel->GetURI(getter_AddRefs(aURI));
  
  nsXPIDLCString temp;
  aURI->GetSpec(getter_Copies(temp));
  printf("OnSecurityChange: (%x) %s\n", state, (const char*)temp);
#endif
  /* Deprecated support for mSecurityButton */
  if (mSecurityButton) {
    NS_NAMED_LITERAL_STRING(level, "level");

    if (state == (STATE_IS_SECURE|STATE_SECURE_HIGH)) {
      res = mSecurityButton->SetAttribute(level, NS_LITERAL_STRING("high"));
    } else if (state == (STATE_IS_SECURE|STATE_SECURE_LOW)) {
      res = mSecurityButton->SetAttribute(level, NS_LITERAL_STRING("low"));
    } else if (state == STATE_IS_BROKEN) {
      res = mSecurityButton->SetAttribute(level, NS_LITERAL_STRING("broken"));
    } else {
      res = mSecurityButton->RemoveAttribute(level);
    }
  }

  return res;
}

// nsISSLStatusProvider methods
NS_IMETHODIMP
nsSecureBrowserUIImpl::GetSSLStatus(nsISSLStatus** _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");

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
  
  char* scheme;
	aURL->GetScheme(&scheme);

  // If no scheme, it's not an https url - not necessarily an error.
  // See bugs 54845 and 54966  
	if (!scheme)
		return NS_OK;
  
  if (!PL_strncasecmp(scheme, "https",  5))
		*value = PR_TRUE;
	
	nsMemory::Free(scheme);
	return NS_OK;
}

void
nsSecureBrowserUIImpl::GetBundleString(const PRUnichar* name,
                                       nsString &outString)
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
nsSecureBrowserUIImpl::CheckProtocolContextSwitch(nsISecurityEventSink* eventSink,
                                                  nsIRequest* aRequest,
                                                  nsIChannel* aChannel)
{
  PRInt32 newSecurityState, oldSecurityState = mSecurityState;
  
  newSecurityState = GetSecurityStateFromChannel(aChannel);
  mSecurityState = newSecurityState;

  // Check to see if we are going from a secure page to an insecure page
  if (newSecurityState == STATE_IS_INSECURE &&
      (IS_SECURE(oldSecurityState) ||
       oldSecurityState == STATE_IS_BROKEN)) {

    SetBrokenLockIcon(eventSink, aRequest, PR_TRUE);

    AlertLeavingSecure();

  }
  // check to see if we are going from an insecure page to a secure one.
  else if ((newSecurityState == (STATE_IS_SECURE|STATE_SECURE_HIGH) ||
            newSecurityState == STATE_IS_BROKEN) &&
           oldSecurityState == STATE_IS_INSECURE) {
    AlertEnteringSecure();
  }
  // check to see if we are going from a strong or insecure page to a
  // weak one.
  else if ((IS_SECURE(newSecurityState) &&
            newSecurityState != (STATE_IS_SECURE|STATE_SECURE_HIGH)) &&
           (oldSecurityState == STATE_IS_INSECURE ||
            oldSecurityState == (STATE_IS_SECURE|STATE_SECURE_HIGH))) {

    AlertEnteringWeak();
  }
  
  mSecurityState = newSecurityState;
  return NS_OK;
}

nsresult
nsSecureBrowserUIImpl::CheckMixedContext(nsISecurityEventSink *eventSink,
                                         nsIRequest* aRequest, nsIChannel* aChannel)
{
  PRInt32 newSecurityState;

  newSecurityState = GetSecurityStateFromChannel(aChannel);

  // Deal with http redirect to https //
  if (mSecurityState == STATE_IS_INSECURE && newSecurityState != STATE_IS_INSECURE) {
      return CheckProtocolContextSwitch(eventSink, aRequest, aChannel);
  }

  if ((newSecurityState == STATE_IS_INSECURE ||
       newSecurityState == STATE_IS_BROKEN) &&
      IS_SECURE(mSecurityState)) {
    
    // work-around for bug 48515
    nsCOMPtr<nsIURI> aURI;
    aChannel->GetURI(getter_AddRefs(aURI));

    nsXPIDLCString temp;
    aURI->GetSpec(getter_Copies(temp));

    if (!nsCRT::strncmp((const char*) temp, "file:", 5) ||
        !nsCRT::strcmp((const char*) temp, "about:layout-dummy-request")) {
      return NS_OK;
    }

    mSecurityState = STATE_IS_BROKEN;
    SetBrokenLockIcon(eventSink, aRequest);

    // Show alert to user (first time only)
    // NOTE: doesn't mSecurityState provide the correct
    // one-time checking?? Why have mMixContentAlertShown
    // as well?
    if (!mMixContentAlertShown) {
      AlertMixedMode();
      mMixContentAlertShown = PR_TRUE;
    }
  }
  
  return NS_OK;
}

nsresult
nsSecureBrowserUIImpl::CheckPost(nsIURI *formURL, nsIURI *actionURL, PRBool *okayToPost)
{
  PRBool formSecure,actionSecure;
  *okayToPost = PR_TRUE;

  nsresult rv = IsURLHTTPS(formURL, &formSecure);
  if (NS_FAILED(rv))
    return rv;

  rv = IsURLHTTPS(actionURL, &actionSecure);
  if (NS_FAILED(rv))
    return rv;
  
  // if we are posting to a secure link from a secure page, all is okay.
  if (actionSecure && formSecure) {
    return NS_OK;
  }
    
  // posting to insecure webpage from a secure webpage.
  if (!actionSecure && formSecure) {
    *okayToPost = ConfirmPostToInsecureFromSecure();
  } else {
    *okayToPost = ConfirmPostToInsecure();
  }
  
  return NS_OK;
}

nsresult
nsSecureBrowserUIImpl::SetBrokenLockIcon(nsISecurityEventSink *eventSink,
                                         nsIRequest* aRequest,
                                         PRBool removeValue)
{
  nsresult rv = NS_OK;
  if (removeValue) {
    if (eventSink)
      (void) eventSink->OnSecurityChange(aRequest, STATE_IS_INSECURE);
  } else {
    if (eventSink)
      (void) eventSink->OnSecurityChange(aRequest, (STATE_IS_BROKEN));
  }
  
  nsAutoString tooltiptext;
  GetBundleString(NS_LITERAL_STRING("SecurityButtonTooltipText").get(),
                  tooltiptext);

  /* TNH - need tooltip notification here */
  if (mSecurityButton)
    rv = mSecurityButton->SetAttribute(NS_LITERAL_STRING("tooltiptext"),
                                       tooltiptext);
  return rv;
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
  NS_INIT_ISUPPORTS();
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
  } else {
    rv = NS_ERROR_NO_INTERFACE;
  }

  return rv;
}

nsresult nsSecureBrowserUIImpl::
GetNSSDialogs(const nsIID &id, void* *result)
{
  return ::getNSSDialogs(result, id);
#if 0
  nsCOMPtr<nsIProxyObjectManager> manager = do_GetService(NS_XPCOMPROXY_CONTRACTID);
  if (!manager) return NS_ERROR_FAILURE;

  nsCOMPtr<nsINSSDialogs> nssDialogs = do_GetService(NS_NSSDIALOGS_CONTRACTID);
  if (!nssDialogs) return NS_ERROR_FAILURE;

  manager->GetProxyForObject(NS_UI_THREAD_EVENTQ,
                             NS_GET_IID(nsISecurityWarningDialogs),
                             nssDialogs,
                             PROXY_SYNC,
                             result);
  if (!manager) return NS_ERROR_FAILURE;

  return NS_OK;
#endif
}

void nsSecureBrowserUIImpl::
AlertEnteringSecure()
{
  nsCOMPtr<nsISecurityWarningDialogs> dialogs;

  GetNSSDialogs(NS_GET_IID(nsISecurityWarningDialogs), getter_AddRefs(dialogs));
  if (!dialogs) return;

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(mWindow);

  dialogs->AlertEnteringSecure(ctx);

  return;
}

void nsSecureBrowserUIImpl::
AlertEnteringWeak()
{
  nsCOMPtr<nsISecurityWarningDialogs> dialogs;

  GetNSSDialogs(NS_GET_IID(nsISecurityWarningDialogs), getter_AddRefs(dialogs));
  if (!dialogs) return;

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(mWindow);

  dialogs->AlertEnteringWeak(ctx);

  return;
}

void nsSecureBrowserUIImpl::
AlertLeavingSecure()
{
  nsCOMPtr<nsISecurityWarningDialogs> dialogs;

  GetNSSDialogs(NS_GET_IID(nsISecurityWarningDialogs), getter_AddRefs(dialogs));
  if (!dialogs) return;

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(mWindow);

  dialogs->AlertLeavingSecure(ctx);

  return;
}

void nsSecureBrowserUIImpl::
AlertMixedMode()
{
  nsCOMPtr<nsISecurityWarningDialogs> dialogs;

  GetNSSDialogs(NS_GET_IID(nsISecurityWarningDialogs), getter_AddRefs(dialogs));
  if (!dialogs) return;

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(mWindow);

  dialogs->AlertMixedMode(ctx);

  return;
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

  GetNSSDialogs(NS_GET_IID(nsISecurityWarningDialogs), getter_AddRefs(dialogs));
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

  GetNSSDialogs(NS_GET_IID(nsISecurityWarningDialogs), getter_AddRefs(dialogs));
  if (!dialogs) return PR_FALSE;  // Should this allow PR_TRUE for unimplemented?

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsUIContext(mWindow);

  PRBool result;

  rv = dialogs->ConfirmPostToInsecureFromSecure(ctx, &result);
  if (NS_FAILED(rv)) return PR_FALSE;

  return result;
}
