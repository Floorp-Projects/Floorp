/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 * 
 * Contributor(s): 
 *      Scott MacGregor <mscott@netscape.com>
 *   
 */

#include "nsIURI.h"
#include "nsIURL.h"
#include "nsExternalProtocolHandler.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"

// used to dispatch urls to default protocol handlers
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);


////////////////////////////////////////////////////////////////////////
// a stub channel implemenation which will map calls to AsyncRead and OpenInputStream
// to calls in the OS for loading the url.
////////////////////////////////////////////////////////////////////////

class nsExtProtocolChannel : public nsIChannel
{
public:

	  NS_DECL_ISUPPORTS
    NS_DECL_NSICHANNEL
    NS_DECL_NSIREQUEST
	
    nsExtProtocolChannel();
	  virtual ~nsExtProtocolChannel();

    nsresult SetURI(nsIURI*);

protected:
  nsCOMPtr<nsIURI> mUrl;
  nsresult mStatus;

  nsresult OpenURL();
};

NS_IMPL_THREADSAFE_ADDREF(nsExtProtocolChannel)
NS_IMPL_THREADSAFE_RELEASE(nsExtProtocolChannel)

NS_INTERFACE_MAP_BEGIN(nsExtProtocolChannel)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChannel)
   NS_INTERFACE_MAP_ENTRY(nsIChannel)
   NS_INTERFACE_MAP_ENTRY(nsIRequest)
NS_INTERFACE_MAP_END_THREADSAFE

nsExtProtocolChannel::nsExtProtocolChannel() : mStatus(NS_OK)
{
  NS_INIT_ISUPPORTS();
}

nsExtProtocolChannel::~nsExtProtocolChannel()
{}

NS_IMETHODIMP nsExtProtocolChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
	return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  *aNotificationCallbacks = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
	return NS_OK;       // don't fail when trying to set this
}

NS_IMETHODIMP 
nsExtProtocolChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  *aSecurityInfo = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetOriginalURI(nsIURI* *aURI)
{
  *aURI = nsnull;
  return NS_OK; 
}
 
NS_IMETHODIMP nsExtProtocolChannel::SetOriginalURI(nsIURI* aURI)
{
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
  nsCOMPtr<nsIExternalProtocolService> extProtService (do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));
  PRBool haveHandler = PR_FALSE;
  nsXPIDLCString urlScheme;
  mUrl->GetScheme(getter_Copies(urlScheme));

  if (extProtService)
  {
    extProtService->ExternalProtocolHandlerExists(urlScheme, &haveHandler);
    if (haveHandler)
      return extProtService->LoadUrl(mUrl);
  }
  
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsExtProtocolChannel::Open(nsIInputStream **_retval)
{
  OpenURL();  // force caller to abort.
  return NS_ERROR_FAILURE; // force caller to abort.
}

NS_IMETHODIMP nsExtProtocolChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
  OpenURL();
  return NS_ERROR_FAILURE; // force caller to abort.
}

NS_IMETHODIMP nsExtProtocolChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
  *aLoadFlags = 0;
	return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
	return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentType(char * *aContentType)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::SetContentType(const char *aContentType)
{
  return NS_ERROR_FAILURE;
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

NS_IMETHODIMP nsExtProtocolChannel::GetName(PRUnichar* *result)
{
  NS_NOTREACHED("nsExtProtocolChannel::GetName");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::IsPending(PRBool *result)
{
  *result = PR_TRUE;
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
  NS_INIT_REFCNT();
	m_schemeName = "default";
}


nsExternalProtocolHandler::~nsExternalProtocolHandler()
{}

NS_IMPL_THREADSAFE_ADDREF(nsExternalProtocolHandler)
NS_IMPL_THREADSAFE_RELEASE(nsExternalProtocolHandler)

NS_INTERFACE_MAP_BEGIN(nsExternalProtocolHandler)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIProtocolHandler)
   NS_INTERFACE_MAP_ENTRY(nsIProtocolHandler)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMETHODIMP nsExternalProtocolHandler::GetScheme(char * *aScheme)
{
	*aScheme = m_schemeName.ToNewCString();
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
PRBool nsExternalProtocolHandler::HaveProtocolHandler(nsIURI * aURI)
{
  PRBool haveHandler = PR_FALSE;
  nsXPIDLCString scheme;
  if (aURI)
  {
    aURI->GetScheme(getter_Copies(scheme));
    nsCOMPtr<nsIExternalProtocolService> extProtService (do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));
    extProtService->ExternalProtocolHandlerExists(scheme, &haveHandler);
  }

  return haveHandler;
}

NS_IMETHODIMP nsExternalProtocolHandler::GetProtocolFlags(PRUint32 *aUritype)
{
    // Make it norelative since it is a simple uri
    *aUritype = URI_NORELATIVE;
    return NS_OK;
}

NS_IMETHODIMP nsExternalProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
  nsresult rv = NS_ERROR_UNKNOWN_PROTOCOL;
  nsCOMPtr<nsIURI> uri = do_CreateInstance(kSimpleURICID, &rv);
  if (uri)
  {
    uri->SetSpec(aSpec);
    PRBool haveHandler = HaveProtocolHandler(uri);

    if (haveHandler)
    {
      *_retval = uri;
      NS_IF_ADDREF(*_retval);
      return NS_OK;
    }
  }

  return NS_ERROR_UNKNOWN_PROTOCOL;
}

NS_IMETHODIMP nsExternalProtocolHandler::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
  // only try to return a channel if we have a protocol handler for the url

  PRBool haveHandler = HaveProtocolHandler(aURI);
  if (haveHandler)
  {
    nsCOMPtr<nsIChannel> channel;
    NS_NEWXPCOM(channel, nsExtProtocolChannel);
    if (!channel) return NS_ERROR_OUT_OF_MEMORY;

    ((nsExtProtocolChannel*) channel.get())->SetURI(aURI);

    if (_retval)
    {
      *_retval = channel;
      NS_IF_ADDREF(*_retval);
      return NS_OK;
    }
  }

  return NS_ERROR_UNKNOWN_PROTOCOL;
}
