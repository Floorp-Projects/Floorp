/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sts=2 sw=2 et cin:
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Dan Mosedale <dmose@mozilla.org>
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

#include "nsIURI.h"
#include "nsIURL.h"
#include "nsExternalProtocolHandler.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIStringBundle.h"
#include "nsIPrefService.h"
#include "nsIPrompt.h"
#include "nsNetUtil.h"
#include "nsIChannelEventSink.h"
#include "nsThreadUtils.h"
#include "nsEscape.h"
#include "nsExternalHelperAppService.h"

// used to dispatch urls to default protocol handlers
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"

////////////////////////////////////////////////////////////////////////
// a stub channel implemenation which will map calls to AsyncRead and OpenInputStream
// to calls in the OS for loading the url.
////////////////////////////////////////////////////////////////////////

class nsExtProtocolChannel : public nsIChannel
{
    friend class nsProtocolRedirect;

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICHANNEL
    NS_DECL_NSIREQUEST

    nsExtProtocolChannel();
    virtual ~nsExtProtocolChannel();

    nsresult SetURI(nsIURI*);

private:
    nsresult OpenURL();
    void Finish(nsresult aResult);
    
    nsCOMPtr<nsIURI> mUrl;
    nsCOMPtr<nsIURI> mOriginalURI;
    nsresult mStatus;
    nsLoadFlags mLoadFlags;
    PRBool mIsPending;
    PRBool mWasOpened;
    
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    nsCOMPtr<nsILoadGroup> mLoadGroup;
    nsCOMPtr<nsIStreamListener> mListener;
    nsCOMPtr<nsISupports> mContext;
};

NS_IMPL_THREADSAFE_ADDREF(nsExtProtocolChannel)
NS_IMPL_THREADSAFE_RELEASE(nsExtProtocolChannel)

NS_INTERFACE_MAP_BEGIN(nsExtProtocolChannel)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChannel)
   NS_INTERFACE_MAP_ENTRY(nsIChannel)
   NS_INTERFACE_MAP_ENTRY(nsIRequest)
NS_INTERFACE_MAP_END_THREADSAFE

nsExtProtocolChannel::nsExtProtocolChannel() : mStatus(NS_OK), 
                                               mIsPending(PR_FALSE),
                                               mWasOpened(PR_FALSE)
{
}

nsExtProtocolChannel::~nsExtProtocolChannel()
{}

NS_IMETHODIMP nsExtProtocolChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
  NS_IF_ADDREF(*aLoadGroup = mLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aCallbacks)
{
  NS_IF_ADDREF(*aCallbacks = mCallbacks);
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks)
{
  mCallbacks = aCallbacks;
  return NS_OK;
}

NS_IMETHODIMP 
nsExtProtocolChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  *aSecurityInfo = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetOriginalURI(nsIURI* *aURI)
{
  NS_IF_ADDREF(*aURI = mOriginalURI);
  return NS_OK; 
}
 
NS_IMETHODIMP nsExtProtocolChannel::SetOriginalURI(nsIURI* aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  mOriginalURI = aURI;
  return NS_OK;
}
 
NS_IMETHODIMP nsExtProtocolChannel::GetURI(nsIURI* *aURI)
{
  *aURI = mUrl;
  NS_IF_ADDREF(*aURI);
  return NS_OK; 
}
 
nsresult nsExtProtocolChannel::SetURI(nsIURI* aURI)
{
  mUrl = aURI;
  return NS_OK; 
}

nsresult nsExtProtocolChannel::OpenURL()
{
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIExternalProtocolService> extProtService (do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));

  if (extProtService)
  {
#ifdef DEBUG
    nsCAutoString urlScheme;
    mUrl->GetScheme(urlScheme);
    PRBool haveHandler = PR_FALSE;
    extProtService->ExternalProtocolHandlerExists(urlScheme.get(), &haveHandler);
    NS_ASSERTION(haveHandler, "Why do we have a channel for this url if we don't support the protocol?");
#endif

    rv = extProtService->LoadURI(mUrl, nsnull);
  }

  // Drop notification callbacks to prevent cycles.
  mCallbacks = 0;

  return rv;
}

NS_IMETHODIMP nsExtProtocolChannel::Open(nsIInputStream **_retval)
{
  OpenURL();
  return NS_ERROR_NO_CONTENT; // force caller to abort.
}

class nsProtocolRedirect : public nsRunnable {
  public:
    nsProtocolRedirect(nsIURI *aURI, nsIHandlerInfo *aHandlerInfo,
                       nsIStreamListener *aListener, nsISupports *aContext,
                       nsExtProtocolChannel *aOriginalChannel)
      : mURI(aURI), mHandlerInfo(aHandlerInfo), mListener(aListener), 
        mContext(aContext), mOriginalChannel(aOriginalChannel) {}

    NS_IMETHOD Run() 
    {
      // for now, this code path is only take for a web-based protocol handler
      nsCOMPtr<nsIHandlerApp> handlerApp;
      nsresult rv = 
        mHandlerInfo->GetPreferredApplicationHandler(getter_AddRefs(handlerApp));
      if (NS_FAILED(rv)) {
        mOriginalChannel->Finish(rv);
        return NS_OK;
      }

      nsCOMPtr<nsIWebHandlerApp> webHandlerApp = do_QueryInterface(handlerApp,
                                                                   &rv);
      if (NS_FAILED(rv)) {
        mOriginalChannel->Finish(rv);
        return NS_OK; 
      }

      nsCAutoString uriTemplate;
      rv = webHandlerApp->GetUriTemplate(uriTemplate);
      if (NS_FAILED(rv)) {
        mOriginalChannel->Finish(rv);
        return NS_OK; 
      }
            
      // get the URI spec so we can escape it for insertion into the template 
      nsCAutoString uriSpecToHandle;
      rv = mURI->GetSpec(uriSpecToHandle);
      if (NS_FAILED(rv)) {
        mOriginalChannel->Finish(rv);
        return NS_OK; 
      }

      // XXX need to strip passwd & username from URI to handle, as per the
      // WhatWG HTML5 draft.  nsSimpleURL, which is what we're going to get,
      // can't do this directly.  Ideally, we'd fix nsStandardURL to make it
      // possible to turn off all of its quirks handling, and use that...

      // XXX this doesn't exactly match how the HTML5 draft is requesting us to
      // escape; at the very least, it should be escaping @ signs, and there
      // may well be more issues.  However, this code will probably be thrown
      // out when we do the front-end work, as we'll be using a refactored 
      // nsIWebContentConverterInfo to do this work for us
      nsCAutoString escapedUriSpecToHandle;
      NS_EscapeURL(uriSpecToHandle, esc_Minimal | esc_Forced | esc_Colon,
                   escapedUriSpecToHandle);

      // Note that this replace all occurrences of %s with the URL to be
      // handled.  The HTML5 draft doesn't prohibit %s from occurring more than
      // once, and if it does, I can't think of any problems that could
      // cause, (though I don't know why anyone would need or want to do it). 
      uriTemplate.ReplaceSubstring(NS_LITERAL_CSTRING("%s"),
                                   escapedUriSpecToHandle);

      // convert spec to URI; no original charset needed since there's no way
      // to communicate that information to any handler
      nsCOMPtr<nsIURI> uriToSend;
      rv = NS_NewURI(getter_AddRefs(uriToSend), uriTemplate);
      if (NS_FAILED(rv)) {
        mOriginalChannel->Finish(rv);
        return NS_OK; 
      }

      // create a channel
      nsCOMPtr<nsIChannel> newChannel;
      rv = NS_NewChannel(getter_AddRefs(newChannel), uriToSend, nsnull,
                         mOriginalChannel->mLoadGroup,
                         mOriginalChannel->mCallbacks,
                         mOriginalChannel->mLoadFlags 
                         | nsIChannel::LOAD_REPLACE);
      if (NS_FAILED(rv)) {
        mOriginalChannel->Finish(rv);
        return NS_OK; 
      }

      nsCOMPtr<nsIChannelEventSink> eventSink;
      NS_QueryNotificationCallbacks(mOriginalChannel->mCallbacks,
                                    mOriginalChannel->mLoadGroup, eventSink);

      if (eventSink) {
        // XXX decide on and audit for correct session & global hist behavior 
        rv = eventSink->OnChannelRedirect(mOriginalChannel, newChannel, 
                                          nsIChannelEventSink::REDIRECT_TEMPORARY |
                                          nsIChannelEventSink::REDIRECT_INTERNAL);
        if (NS_FAILED(rv)) {
          mOriginalChannel->Finish(rv);
          return NS_OK;
        }
      }

      rv = newChannel->AsyncOpen(mListener, mContext);
      if (NS_FAILED(rv)) {
        mOriginalChannel->Finish(rv);
        return NS_OK; 
      }
      
      mOriginalChannel->Finish(NS_BINDING_REDIRECTED);
      return NS_OK;
    }

  private:
    nsCOMPtr<nsIURI> mURI;
    nsCOMPtr<nsIHandlerInfo> mHandlerInfo;
    nsCOMPtr<nsIStreamListener> mListener;
    nsCOMPtr<nsISupports> mContext;
    nsCOMPtr<nsExtProtocolChannel> mOriginalChannel;
};

NS_IMETHODIMP nsExtProtocolChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  mWasOpened = PR_TRUE;
  mListener = listener;
  mContext = ctxt;

  if (!gExtProtSvc) {
    return NS_ERROR_FAILURE;
  }

  nsCAutoString urlScheme;  
  nsresult rv = mUrl->GetScheme(urlScheme);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // check whether the scheme is one that we have a web handler for
  nsCOMPtr<nsIHandlerInfo> handlerInfo;
  rv = gExtProtSvc->GetProtocolHandlerInfo(urlScheme, 
                                           getter_AddRefs(handlerInfo));
  if (NS_SUCCEEDED(rv)) {
    PRInt32 preferredAction;                                           
    rv = handlerInfo->GetPreferredAction(&preferredAction);

    // for now, anything that triggers a helper app is going to be a web-based
    // protocol handler, so we use that to decide which path to take...
    if (preferredAction == nsIHandlerInfo::useHelperApp) {

      // redirecting to the web handler involves calling OnChannelRedirect
      // (which is supposed to happen after AsyncOpen completes) or possibly
      // opening a dialog, so we do it in an event
      nsCOMPtr<nsIRunnable> event = new nsProtocolRedirect(mUrl, handlerInfo,
                                                           listener, ctxt,
                                                           this);

      // We don't check if |event| was successfully created because
      // |NS_DispatchToCurrentThread| will do that for us.
      rv = NS_DispatchToCurrentThread(event);
      if (NS_SUCCEEDED(rv)) {
        mIsPending = PR_TRUE;

        // add ourselves to the load group, since this isn't going to finish
        // immediately
        if (mLoadGroup)
          (void)mLoadGroup->AddRequest(this, nsnull);

        return rv;
      }
    }
  }
  
  // no protocol info found, just fall back on whatever the OS has to offer
  OpenURL();
  return NS_ERROR_NO_CONTENT; // force caller to abort.
}

/**
 * Finish out what was started in AsyncOpen.  This can be called in either the
 * success or the failure case.  
 *
 * @param aStatus  used to set the channel's status, and, if this set to 
 *                 anything other than NS_BINDING_REDIRECTED, OnStartRequest
 *                 and OnStopRequest will be called, since Necko guarantees
 *                 this will happen unless the redirect took place.
 */
void nsExtProtocolChannel::Finish(nsresult aStatus)
{
  mStatus = aStatus;

  if (aStatus != NS_BINDING_REDIRECTED && mListener) {
    (void)mListener->OnStartRequest(this, mContext);
    (void)mListener->OnStopRequest(this, mContext, aStatus);
  }
  
  mIsPending = PR_FALSE;
  
  if (mLoadGroup) {
    (void)mLoadGroup->RemoveRequest(this, nsnull, aStatus);
  }
  return;
}

NS_IMETHODIMP nsExtProtocolChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentType(nsACString &aContentType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::SetContentType(const nsACString &aContentType)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentCharset(nsACString &aContentCharset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::SetContentCharset(const nsACString &aContentCharset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentLength(PRInt32 * aContentLength)
{
  *aContentLength = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsExtProtocolChannel::SetContentLength(PRInt32 aContentLength)
{
  NS_NOTREACHED("SetContentLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::GetOwner(nsISupports * *aPrincipal)
{
  NS_NOTREACHED("GetOwner");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::SetOwner(nsISupports * aPrincipal)
{
  NS_NOTREACHED("SetOwner");
  return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIRequest
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsExtProtocolChannel::GetName(nsACString &result)
{
  return mUrl->GetSpec(result);
}

NS_IMETHODIMP nsExtProtocolChannel::IsPending(PRBool *result)
{
  *result = mIsPending;
  return NS_OK; 
}

NS_IMETHODIMP nsExtProtocolChannel::GetStatus(nsresult *status)
{
  *status = mStatus;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::Cancel(nsresult status)
{
  mStatus = status;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::Suspend()
{
  NS_NOTREACHED("Suspend");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::Resume()
{
  NS_NOTREACHED("Resume");
  return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////////////////////////
// the default protocol handler implementation
//////////////////////////////////////////////////////////////////////

nsExternalProtocolHandler::nsExternalProtocolHandler()
{
  m_schemeName = "default";
}


nsExternalProtocolHandler::~nsExternalProtocolHandler()
{}

NS_IMPL_THREADSAFE_ADDREF(nsExternalProtocolHandler)
NS_IMPL_THREADSAFE_RELEASE(nsExternalProtocolHandler)

NS_INTERFACE_MAP_BEGIN(nsExternalProtocolHandler)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIProtocolHandler)
   NS_INTERFACE_MAP_ENTRY(nsIProtocolHandler)
   NS_INTERFACE_MAP_ENTRY(nsIExternalProtocolHandler)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMETHODIMP nsExternalProtocolHandler::GetScheme(nsACString &aScheme)
{
  aScheme = m_schemeName;
  return NS_OK;
}

NS_IMETHODIMP nsExternalProtocolHandler::GetDefaultPort(PRInt32 *aDefaultPort)
{
  *aDefaultPort = 0;
    return NS_OK;
}

NS_IMETHODIMP 
nsExternalProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}
// returns TRUE if the OS can handle this protocol scheme and false otherwise.
PRBool nsExternalProtocolHandler::HaveExternalProtocolHandler(nsIURI * aURI)
{
  PRBool haveHandler = PR_FALSE;
  if (aURI)
  {
    nsCAutoString scheme;
    aURI->GetScheme(scheme);
    if (gExtProtSvc)
      gExtProtSvc->ExternalProtocolHandlerExists(scheme.get(), &haveHandler);
  }

  return haveHandler;
}

NS_IMETHODIMP nsExternalProtocolHandler::GetProtocolFlags(PRUint32 *aUritype)
{
    // Make it norelative since it is a simple uri
    *aUritype = URI_NORELATIVE | URI_NOAUTH | URI_LOADABLE_BY_ANYONE |
        URI_NON_PERSISTABLE | URI_DOES_NOT_RETURN_DATA;
    return NS_OK;
}

NS_IMETHODIMP nsExternalProtocolHandler::NewURI(const nsACString &aSpec,
                                                const char *aCharset, // ignore charset info
                                                nsIURI *aBaseURI,
                                                nsIURI **_retval)
{
  nsresult rv;
  nsCOMPtr<nsIURI> uri = do_CreateInstance(NS_SIMPLEURI_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = uri->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_retval = uri);
  return NS_OK;
}

NS_IMETHODIMP nsExternalProtocolHandler::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
  // only try to return a channel if we have a protocol handler for the url

  PRBool haveExternalHandler = HaveExternalProtocolHandler(aURI);
  if (haveExternalHandler)
  {
    nsCOMPtr<nsIChannel> channel;
    NS_NEWXPCOM(channel, nsExtProtocolChannel);
    if (!channel) return NS_ERROR_OUT_OF_MEMORY;

    ((nsExtProtocolChannel*) channel.get())->SetURI(aURI);
    channel->SetOriginalURI(aURI);

    if (_retval)
    {
      *_retval = channel;
      NS_IF_ADDREF(*_retval);
      return NS_OK;
    }
  }

  return NS_ERROR_UNKNOWN_PROTOCOL;
}

///////////////////////////////////////////////////////////////////////
// External protocol handler interface implementation
//////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsExternalProtocolHandler::ExternalAppExistsForScheme(const nsACString& aScheme, PRBool *_retval)
{
  if (gExtProtSvc)
    return gExtProtSvc->ExternalProtocolHandlerExists(
      PromiseFlatCString(aScheme).get(), _retval);

  // In case we don't have external protocol service.
  *_retval = PR_FALSE;
  return NS_OK;
}

nsBlockedExternalProtocolHandler::nsBlockedExternalProtocolHandler()
{
    m_schemeName = "default-blocked";
}

NS_IMETHODIMP
nsBlockedExternalProtocolHandler::NewChannel(nsIURI *aURI,
                                             nsIChannel **_retval)
{
    *_retval = nsnull;
    return NS_ERROR_UNKNOWN_PROTOCOL;
}
