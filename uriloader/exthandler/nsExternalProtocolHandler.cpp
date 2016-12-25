/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sts=2 sw=2 et cin:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIURI.h"
#include "nsIURL.h"
#include "nsExternalProtocolHandler.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIServiceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIStringBundle.h"
#include "nsIPrefService.h"
#include "nsIPrompt.h"
#include "nsNetUtil.h"
#include "nsContentSecurityManager.h"
#include "nsExternalHelperAppService.h"

// used to dispatch urls to default protocol handlers
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"
#include "nsIChildChannel.h"
#include "nsIParentChannel.h"

class nsILoadInfo;

////////////////////////////////////////////////////////////////////////
// a stub channel implemenation which will map calls to AsyncRead and OpenInputStream
// to calls in the OS for loading the url.
////////////////////////////////////////////////////////////////////////

class nsExtProtocolChannel : public nsIChannel,
                             public nsIChildChannel,
                             public nsIParentChannel
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSICHANNEL
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHILDCHANNEL
    NS_DECL_NSIPARENTCHANNEL

    nsExtProtocolChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo);

private:
    virtual ~nsExtProtocolChannel();

    nsresult OpenURL();
    void Finish(nsresult aResult);
    
    nsCOMPtr<nsIURI> mUrl;
    nsCOMPtr<nsIURI> mOriginalURI;
    nsresult mStatus;
    nsLoadFlags mLoadFlags;
    bool mWasOpened;
    // Set true (as a result of ConnectParent invoked from child process)
    // when this channel is on the parent process and is being used as
    // a redirect target channel.  It turns AsyncOpen into a no-op since
    // we do it on the child.
    bool mConnectedParent;
    
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    nsCOMPtr<nsILoadGroup> mLoadGroup;
    nsCOMPtr<nsILoadInfo> mLoadInfo;
};

NS_IMPL_ADDREF(nsExtProtocolChannel)
NS_IMPL_RELEASE(nsExtProtocolChannel)

NS_INTERFACE_MAP_BEGIN(nsExtProtocolChannel)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChannel)
   NS_INTERFACE_MAP_ENTRY(nsIChannel)
   NS_INTERFACE_MAP_ENTRY(nsIRequest)
   NS_INTERFACE_MAP_ENTRY(nsIChildChannel)
   NS_INTERFACE_MAP_ENTRY(nsIParentChannel)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
NS_INTERFACE_MAP_END_THREADSAFE

nsExtProtocolChannel::nsExtProtocolChannel(nsIURI* aURI,
                                           nsILoadInfo* aLoadInfo)
  : mUrl(aURI)
  , mOriginalURI(aURI)
  , mStatus(NS_OK)
  , mWasOpened(false)
  , mConnectedParent(false)
  , mLoadInfo(aLoadInfo)
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
  *aSecurityInfo = nullptr;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetOriginalURI(nsIURI* *aURI)
{
  NS_ADDREF(*aURI = mOriginalURI);
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
 
nsresult nsExtProtocolChannel::OpenURL()
{
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIExternalProtocolService> extProtService (do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));

  if (extProtService)
  {
#ifdef DEBUG
    nsAutoCString urlScheme;
    mUrl->GetScheme(urlScheme);
    bool haveHandler = false;
    extProtService->ExternalProtocolHandlerExists(urlScheme.get(), &haveHandler);
    NS_ASSERTION(haveHandler, "Why do we have a channel for this url if we don't support the protocol?");
#endif

    nsCOMPtr<nsIInterfaceRequestor> aggCallbacks;
    rv = NS_NewNotificationCallbacksAggregation(mCallbacks, mLoadGroup,
                                                getter_AddRefs(aggCallbacks));
    if (NS_FAILED(rv)) {
      goto finish;
    }
                                                
    rv = extProtService->LoadURI(mUrl, aggCallbacks);
    if (NS_SUCCEEDED(rv)) {
        // despite success, we need to abort this channel, at the very least 
        // to make it clear to the caller that no on{Start,Stop}Request
        // should be expected.
        rv = NS_ERROR_NO_CONTENT;
    }
  }

finish:
  mCallbacks = nullptr;
  return rv;
}

NS_IMETHODIMP nsExtProtocolChannel::Open(nsIInputStream **_retval)
{
  return OpenURL();
}

NS_IMETHODIMP nsExtProtocolChannel::Open2(nsIInputStream** aStream)
{
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return Open(aStream);
}

NS_IMETHODIMP nsExtProtocolChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
  if (mConnectedParent) {
    return NS_OK;
  }

  MOZ_ASSERT(!mLoadInfo ||
             mLoadInfo->GetSecurityMode() == 0 ||
             mLoadInfo->GetInitialSecurityCheckDone() ||
             (mLoadInfo->GetSecurityMode() == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL &&
              nsContentUtils::IsSystemPrincipal(mLoadInfo->LoadingPrincipal())),
             "security flags in loadInfo but asyncOpen2() not called");

  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  mWasOpened = true;

  return OpenURL();
}

NS_IMETHODIMP nsExtProtocolChannel::AsyncOpen2(nsIStreamListener *aListener)
{
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return AsyncOpen(listener, nullptr);
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

NS_IMETHODIMP nsExtProtocolChannel::GetContentDisposition(uint32_t *aContentDisposition)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExtProtocolChannel::SetContentDisposition(uint32_t aContentDisposition)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentDispositionFilename(nsAString &aContentDispositionFilename)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExtProtocolChannel::SetContentDispositionFilename(const nsAString &aContentDispositionFilename)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentDispositionHeader(nsACString &aContentDispositionHeader)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentLength(int64_t * aContentLength)
{
  *aContentLength = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsExtProtocolChannel::SetContentLength(int64_t aContentLength)
{
  NS_NOTREACHED("SetContentLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::GetOwner(nsISupports * *aPrincipal)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::SetOwner(nsISupports * aPrincipal)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::GetLoadInfo(nsILoadInfo **aLoadInfo)
{
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetLoadInfo(nsILoadInfo *aLoadInfo)
{
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIRequest
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsExtProtocolChannel::GetName(nsACString &result)
{
  return mUrl->GetSpec(result);
}

NS_IMETHODIMP nsExtProtocolChannel::IsPending(bool *result)
{
  *result = false;
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
// From nsIChildChannel
//////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsExtProtocolChannel::ConnectParent(uint32_t registrarId)
{
  mozilla::dom::ContentChild::GetSingleton()->
    SendExtProtocolChannelConnectParent(registrarId);
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::CompleteRedirectSetup(nsIStreamListener *listener,
                                                          nsISupports *context)
{
  // For redirects to external protocols we AsyncOpen on the child
  // (not the parent) because child channel has the right docshell
  // (which is needed for the select dialog).
  return AsyncOpen(listener, context);
}

///////////////////////////////////////////////////////////////////////
// From nsIParentChannel (derives from nsIStreamListener)
//////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsExtProtocolChannel::SetParentListener(HttpChannelParentListener* aListener)
{
  // This is called as part of the connect parent operation from
  // ContentParent::RecvExtProtocolChannelConnectParent.  Setting
  // this flag tells this channel to not proceed and makes AsyncOpen
  // just no-op.  Actual operation will happen from the child process
  // via CompleteRedirectSetup call on the child channel.
  mConnectedParent = true;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::NotifyTrackingProtectionDisabled()
{
  // nothing to do
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::NotifyTrackingResource()
{
  // nothing to do
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::Delete()
{
  // nothing to do
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::OnStartRequest(nsIRequest *aRequest,
                                                   nsISupports *aContext)
{
  // no data is expected
  MOZ_CRASH("No data expected from external protocol channel");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsExtProtocolChannel::OnStopRequest(nsIRequest *aRequest,
                                                  nsISupports *aContext,
                                                  nsresult aStatusCode)
{
  // no data is expected
  MOZ_CRASH("No data expected from external protocol channel");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsExtProtocolChannel::OnDataAvailable(nsIRequest *aRequest,
                                                    nsISupports *aContext,
                                                    nsIInputStream *aInputStream,
                                                    uint64_t aOffset,
                                                    uint32_t aCount)
{
  // no data is expected
  MOZ_CRASH("No data expected from external protocol channel");
  return NS_ERROR_UNEXPECTED;
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

NS_IMPL_ADDREF(nsExternalProtocolHandler)
NS_IMPL_RELEASE(nsExternalProtocolHandler)

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

NS_IMETHODIMP nsExternalProtocolHandler::GetDefaultPort(int32_t *aDefaultPort)
{
  *aDefaultPort = 0;
    return NS_OK;
}

NS_IMETHODIMP 
nsExternalProtocolHandler::AllowPort(int32_t port, const char *scheme, bool *_retval)
{
    // don't override anything.  
    *_retval = false;
    return NS_OK;
}
// returns TRUE if the OS can handle this protocol scheme and false otherwise.
bool nsExternalProtocolHandler::HaveExternalProtocolHandler(nsIURI * aURI)
{
  MOZ_ASSERT(aURI);
  nsAutoCString scheme;
  aURI->GetScheme(scheme);

  nsCOMPtr<nsIExternalProtocolService> extProtSvc(do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));
  if (!extProtSvc) {
    return false;
  }

  bool haveHandler = false;
  extProtSvc->ExternalProtocolHandlerExists(scheme.get(), &haveHandler);
  return haveHandler;
}

NS_IMETHODIMP nsExternalProtocolHandler::GetProtocolFlags(uint32_t *aUritype)
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

NS_IMETHODIMP
nsExternalProtocolHandler::NewChannel2(nsIURI* aURI,
                                       nsILoadInfo* aLoadInfo,
                                       nsIChannel** aRetval)
{
  NS_ENSURE_TRUE(aURI, NS_ERROR_UNKNOWN_PROTOCOL);
  NS_ENSURE_TRUE(aRetval, NS_ERROR_UNKNOWN_PROTOCOL);

  // Only try to return a channel if we have a protocol handler for the url.
  // nsOSHelperAppService::LoadUriInternal relies on this to check trustedness
  // for some platforms at least.  (win uses ::ShellExecute and unix uses
  // gnome_url_show.)
  if (!HaveExternalProtocolHandler(aURI)) {
    return NS_ERROR_UNKNOWN_PROTOCOL;
  }

  nsCOMPtr<nsIChannel> channel = new nsExtProtocolChannel(aURI, aLoadInfo);
  channel.forget(aRetval);
  return NS_OK;
}

NS_IMETHODIMP nsExternalProtocolHandler::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
  return NewChannel2(aURI, nullptr, _retval);
}

///////////////////////////////////////////////////////////////////////
// External protocol handler interface implementation
//////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsExternalProtocolHandler::ExternalAppExistsForScheme(const nsACString& aScheme, bool *_retval)
{
  nsCOMPtr<nsIExternalProtocolService> extProtSvc(do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));
  if (extProtSvc)
    return extProtSvc->ExternalProtocolHandlerExists(
      PromiseFlatCString(aScheme).get(), _retval);

  // In case we don't have external protocol service.
  *_retval = false;
  return NS_OK;
}
