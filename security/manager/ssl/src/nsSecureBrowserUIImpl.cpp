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
 */
#define FORCE_PR_LOG

#include "nspr.h"
#include "prlog.h"
#include "prmem.h"

#include "nsISecureBrowserUI.h"
#include "nsSecureBrowserUIImpl.h"
#include "nsNSSComponent.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsIServiceManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIObserverService.h"
#include "nsIDocumentLoader.h"
#include "nsCURILoader.h"
#include "nsIDocShell.h"
#include "nsIDocumentViewer.h"
#include "nsCURILoader.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindowInternal.h"
#include "nsIContent.h"
#include "nsIWebProgress.h"
#include "nsIChannel.h"
#include "nsIChannelSecurityInfo.h"
#include "nsIURI.h"
#include "nsISecurityEventSink.h"
#include "nsIPrompt.h"
#include "nsIPref.h"
#include "nsIFormSubmitObserver.h"

static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define ENTER_SITE_PREF      "security.warn_entering_secure"
#define LEAVE_SITE_PREF      "security.warn_leaving_secure"
#define MIXEDCONTENT_PREF    "security.warn_viewing_mixed"
#define INSECURE_SUBMIT_PREF "security.warn_submit_insecure"

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
}

NS_IMPL_ISUPPORTS5(nsSecureBrowserUIImpl,
                   nsSecureBrowserUI,
                   nsIWebProgressListener,
                   nsIFormSubmitObserver,
                   nsIObserver,
                   nsISupportsWeakReference);


NS_IMETHODIMP
nsSecureBrowserUIImpl::Init(nsIDOMWindowInternal *window,
                            nsIDOMElement *button)
{
  nsresult rv = NS_OK;
  mSecurityButton = button;
  mWindow = window;

  mPref = do_GetService(kPrefCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStringBundleService> service(do_GetService(kCStringBundleServiceCID, &rv));
  if (NS_FAILED(rv)) return rv;
  
  rv = service->CreateBundle(SECURITY_STRING_BUNDLE_URL, nsnull,
                             getter_AddRefs(mStringBundle));
  if (NS_FAILED(rv)) return rv;
  
  // hook up to the form post notifications:
  nsCOMPtr<nsIObserverService> svc(do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = svc->AddObserver(this, NS_ConvertASCIItoUCS2(NS_FORMSUBMIT_SUBJECT).get());
  }
  
  // hook up to the webprogress notifications.
  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(window));
  if (!sgo) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDocShell> docShell;
  sgo->GetDocShell(getter_AddRefs(docShell));
  if (!docShell) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIWebProgress> wp(do_GetInterface(docShell));
  if (!wp) return NS_ERROR_FAILURE;
  
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
nsSecureBrowserUIImpl::Observe(nsISupports*, const PRUnichar*,
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

static PRInt16 GetSecurityStateFromChannel(nsIChannel* aChannel)
{
  nsresult res;
  PRInt32 securityState;

  // qi for the psm information about this channel load.
  nsCOMPtr<nsISupports> info;
  aChannel->GetSecurityInfo(getter_AddRefs(info));
  nsCOMPtr<nsIChannelSecurityInfo> psmInfo(do_QueryInterface(info));
  if (!psmInfo) {
    PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI: GetSecurityState:%p - no nsIChannelSecurityInfo for %p\n",
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
  
  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  document->GetScriptGlobalObject(getter_AddRefs(globalObject));
  nsCOMPtr<nsIDOMWindowInternal> postingWindow(do_QueryInterface(globalObject));
  
  PRBool isChild;
  IsChildOfDomWindow(mWindow, postingWindow, &isChild);
  
  // This notify call is not for our window, ignore it.
  if (!isChild)
    return NS_OK;
  
  PRBool okayToPost;
  nsresult res = CheckPost(actionURL, &okayToPost);
  
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

  if (!aRequest || !mPref)
    return NS_ERROR_NULL_POINTER;
  
  // Get the channel from the request...
  // If the request is not network based, then ignore it.
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest, &res));
  if (NS_FAILED(res))
    return NS_OK;
  
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
  
  // A Document is starting to load...
  if ((aProgressStateFlags & (STATE_TRANSFERRING|STATE_REDIRECTING)) &&
      (aProgressStateFlags & STATE_IS_DOCUMENT)) {
    // starting to load a webpage
    mMixContentAlertShown = PR_FALSE;

    return CheckProtocolContextSwitch(eventSink, aRequest, channel);
  }

  // A document has finished loading
  if ((aProgressStateFlags & STATE_STOP) &&
      (aProgressStateFlags & STATE_IS_NETWORK) &&
      (mSecurityState == STATE_IS_SECURE ||
       mSecurityState == STATE_IS_BROKEN))
    {
      if (mSecurityState == STATE_IS_SECURE) {
        // XXX Shouldn't we do this even if the state is broken?
        // XXX Shouldn't we grab the pickled status at STATE_NET_TRANSFERRING?
        
        if (GetSecurityStateFromChannel(channel) == STATE_IS_SECURE) {
          // Everything looks okay.
          PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI:%p: Icon set to lock\n", this));
          
          if (mSecurityButton)
            res = mSecurityButton->SetAttribute(NS_LITERAL_STRING("level"),
                                                NS_LITERAL_STRING("high"));
          
          if (eventSink)
            eventSink->OnSecurityChange(aRequest, (STATE_IS_SECURE));
          
          if (!mSecurityButton)
            return res;
          
          // Do we really need to look at res here? What happens if there's an error?
          // We should still set the certificate authority display.

          PRUnichar* tooltip = nsnull;
          nsCOMPtr<nsISupports> info;
          channel->GetSecurityInfo(getter_AddRefs(info));
          if (info) {
            nsCOMPtr<nsIChannelSecurityInfo> secInfo(do_QueryInterface(info));
            if (secInfo &&
                NS_SUCCEEDED(secInfo->GetShortSecurityDescription(&tooltip)) &&
                tooltip) {

              res = mSecurityButton->SetAttribute(NS_LITERAL_STRING("tooltiptext"),
                                                  nsString(tooltip));
              
              PR_Free(tooltip);
            }
          }
          return res;
        }
        mSecurityState = STATE_IS_BROKEN;
      }
      
      PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI:%p: Icon set to broken\n", this));
      SetBrokenLockIcon(eventSink, aRequest);
      
      return res;
    }
  
  // don't need to do anything more if the page is broken or not secure...
  
  if (mSecurityState != STATE_IS_SECURE)
    return NS_OK;
  
  // A URL is starting to load...
  if ((aProgressStateFlags & (STATE_TRANSFERRING | STATE_REDIRECTING)) &&
      (aProgressStateFlags & STATE_IS_REQUEST)) {
    // check to see if we are going to mix content.
    return CheckMixedContext(eventSink, aRequest, channel);
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
  // I am the guy that created this notification - do nothing
  
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
  nsresult res;
  PRInt32 newSecurityState, oldSecurityState = mSecurityState;
  PRBool boolpref;
  
  newSecurityState = GetSecurityStateFromChannel(aChannel);

  // Check to see if we are going from a secure page to an insecure page
  if (newSecurityState == STATE_IS_INSECURE &&
      (oldSecurityState == STATE_IS_SECURE ||
       oldSecurityState == STATE_IS_BROKEN)) {

    SetBrokenLockIcon(eventSink, aRequest, PR_TRUE);

    if ((mPref->GetBoolPref(LEAVE_SITE_PREF, &boolpref) != 0))
      boolpref = PR_TRUE;
    
    if (boolpref) {
      nsCOMPtr<nsIPrompt> dialog;
      mWindow->GetPrompter(getter_AddRefs(dialog));
      if (!dialog)
        return NS_ERROR_FAILURE;
      
      nsAutoString windowTitle, message, dontShowAgain;
      
      GetBundleString(NS_LITERAL_STRING("Title").get(), windowTitle);
      GetBundleString(NS_LITERAL_STRING("LeaveSiteMessage").get(), message);
      GetBundleString(NS_LITERAL_STRING("DontShowAgain").get(), dontShowAgain);
      
      PRBool outCheckValue = PR_TRUE;
      res = dialog->AlertCheck(windowTitle.GetUnicode(),
                               message.GetUnicode(),
                               dontShowAgain.GetUnicode(),
                               &outCheckValue);
      if (NS_FAILED(res))
        return res;
      
      if (!outCheckValue) {
        mPref->SetBoolPref(LEAVE_SITE_PREF, PR_FALSE);
#if 0
        nsCOMPtr<nsISecurityManagerComponent> psm(do_GetService(PSM_COMPONENT_CONTRACTID, &res));
        if (NS_FAILED(res))
          return res;
        //        psm->PassPrefs();
#endif
      }
    }
  }
  // check to see if we are going from an insecure page to a secure one.
  else if ((newSecurityState == STATE_IS_SECURE ||
            newSecurityState == STATE_IS_BROKEN) &&
           oldSecurityState == STATE_IS_INSECURE) {

    if ((mPref->GetBoolPref(ENTER_SITE_PREF, &boolpref) != 0))
      boolpref = PR_TRUE;
    if (boolpref) {
      nsCOMPtr<nsIPrompt> dialog;
      mWindow->GetPrompter(getter_AddRefs(dialog));
      if (!dialog)
        return NS_ERROR_FAILURE;
      
      nsAutoString windowTitle, message, dontShowAgain;
      
      GetBundleString(NS_LITERAL_STRING("Title").get(), windowTitle);
      GetBundleString(NS_LITERAL_STRING("EnterSiteMessage").get(), message);
      GetBundleString(NS_LITERAL_STRING("DontShowAgain").get(), dontShowAgain);
      
      PRBool outCheckValue = PR_TRUE;
      res = dialog->AlertCheck(windowTitle.GetUnicode(),
                               message.GetUnicode(),
                               dontShowAgain.GetUnicode(),
                               &outCheckValue);
      if (NS_FAILED(res))
        return res;
      
      if (!outCheckValue) {
        mPref->SetBoolPref(ENTER_SITE_PREF, PR_FALSE);
#if 0
        nsCOMPtr<nsISecurityManageComponent> psm(do_getService(PSM_COMPONENT_CONTRACTID, &res));
        if (NS_FAILED(res)) 
          return res;
        //        psm->PassPrefs();
#endif
      }
    }
  }
  
  mSecurityState = newSecurityState;
  return NS_OK;
}

nsresult
nsSecureBrowserUIImpl::CheckMixedContext(nsISecurityEventSink *eventSink,
                                         nsIRequest* aRequest, nsIChannel* aChannel)
{
  PRInt16 newSecurityState;
  nsresult rv;
  
  newSecurityState = GetSecurityStateFromChannel(aChannel);

  if ((newSecurityState == STATE_IS_INSECURE ||
       newSecurityState == STATE_IS_BROKEN) &&
      mSecurityState == STATE_IS_SECURE) {
    
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
    
    if (!mPref) return NS_ERROR_NULL_POINTER;
    
    PRBool boolpref;
    if ((mPref->GetBoolPref(MIXEDCONTENT_PREF, &boolpref) != 0))
      boolpref = PR_TRUE;
    
    if (boolpref && !mMixContentAlertShown) {
      nsCOMPtr<nsIPrompt> dialog;
      mWindow->GetPrompter(getter_AddRefs(dialog));
      if (!dialog)
        return NS_ERROR_FAILURE;
      
      nsAutoString windowTitle, message, dontShowAgain;
      
      GetBundleString(NS_LITERAL_STRING("Title").get(), windowTitle);
      GetBundleString(NS_LITERAL_STRING("MixedContentMessage").get(), message);
      GetBundleString(NS_LITERAL_STRING("DontShowAgain").get(), dontShowAgain);
      
      PRBool outCheckValue = PR_TRUE;
      
      rv = dialog->AlertCheck(windowTitle.GetUnicode(),
                              message.GetUnicode(),
                              dontShowAgain.GetUnicode(),
                              &outCheckValue);
      if (NS_FAILED(rv))
        return rv;
      
      if (!outCheckValue) {
        mPref->SetBoolPref(MIXEDCONTENT_PREF, PR_FALSE);
#if 0
        nsCOMptr<nsISecurityManagerComponent> psm(do_GetService(PSM_COMPONENT_CONTRACTID, &rv));
        if (NS_FAILED(rv))
          return rv;
        //        psm->PassPrefs();
#endif
      }
      
      mMixContentAlertShown = PR_TRUE;
    }
  }
  
  return NS_OK;
}

nsresult
nsSecureBrowserUIImpl::CheckPost(nsIURI *actionURL, PRBool *okayToPost)
{
  PRBool secure;
  *okayToPost = PR_TRUE;

  nsresult rv = IsURLHTTPS(actionURL, &secure);
  if (NS_FAILED(rv))
    return rv;
  
  // if we are posting to a secure link from a secure page, all is okay.
  if (secure &&
      (mSecurityState == STATE_IS_SECURE ||
       mSecurityState == STATE_IS_BROKEN)) {
    return NS_OK;
  }

  PRBool boolpref = PR_TRUE;
  
  // posting to a non https URL.
  mPref->GetBoolPref(INSECURE_SUBMIT_PREF, &boolpref);
  
  if (boolpref) {
    nsCOMPtr<nsIPrompt> dialog;
    mWindow->GetPrompter(getter_AddRefs(dialog));
    if (!dialog)
      return NS_ERROR_FAILURE;
    
    nsAutoString windowTitle, message, dontShowAgain;
    
    GetBundleString(NS_LITERAL_STRING("Title").get(), windowTitle);
    GetBundleString(NS_LITERAL_STRING("DontShowAgain").get(), dontShowAgain);
    
    // posting to insecure webpage from a secure webpage.
    if (!secure  && mSecurityState == STATE_IS_SECURE) {
      GetBundleString(NS_LITERAL_STRING("PostToInsecure").get(), message);
    } else { // anything else, post generic warning
      GetBundleString(NS_LITERAL_STRING("PostToInsecureFromInsecure").get(),
                      message);
    }
    
    PRBool outCheckValue = PR_TRUE;
    rv = dialog->ConfirmCheck(windowTitle.GetUnicode(),
                              message.GetUnicode(),
                              dontShowAgain.GetUnicode(),
                              &outCheckValue,
                              okayToPost);
    if (NS_FAILED(rv))
      return rv;
    
    if (!outCheckValue) {
      mPref->SetBoolPref(INSECURE_SUBMIT_PREF, PR_FALSE);
      return NS_OK;
#if 0
      nsCOMPtr<nsISecurityManagerComponent> psm(do_GetService(PSM_COMPONENT_CONTRACTID, &rv));
      if (NS_FAILED(rv))
        return rv;
      //      psm->PassPrefs();
#endif
    }
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
    if (mSecurityButton)
      rv = mSecurityButton->RemoveAttribute(NS_LITERAL_STRING("level"));
    if (eventSink)
      (void) eventSink->OnSecurityChange(aRequest, STATE_IS_INSECURE);
  } else {
    if (mSecurityButton)
      rv = mSecurityButton->SetAttribute(NS_LITERAL_STRING("level"),
                                         NS_LITERAL_STRING("broken"));
    if (eventSink)
      (void) eventSink->OnSecurityChange(aRequest, (STATE_IS_BROKEN));
  }
  
  nsAutoString tooltiptext;
  GetBundleString(NS_LITERAL_STRING("SecurityButtonTooltipText").get(),
                  tooltiptext);
  if (mSecurityButton)
    rv = mSecurityButton->SetAttribute(NS_LITERAL_STRING("tooltiptext"),
                                       tooltiptext);
  return rv;
}

