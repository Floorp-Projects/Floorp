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
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "DummyChannel.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"


NS_IMPL_ISUPPORTS1(DummyChannel, nsIChannel)

DummyChannel::DummyChannel(imgIRequest *aRequest, nsILoadGroup *aLoadGroup) :
  mRequest(aRequest)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

DummyChannel::~DummyChannel()
{
  /* destructor code */
}

/* attribute nsIURI originalURI; */
NS_IMETHODIMP DummyChannel::GetOriginalURI(nsIURI * *aOriginalURI)
{
  return mRequest->GetURI(aOriginalURI);
}
NS_IMETHODIMP DummyChannel::SetOriginalURI(nsIURI * aOriginalURI)
{
  return NS_ERROR_FAILURE;
}

/* attribute nsIURI URI; */
NS_IMETHODIMP DummyChannel::GetURI(nsIURI * *aURI)
{
  return mRequest->GetURI(aURI);
}

/* attribute nsISupports owner; */
NS_IMETHODIMP DummyChannel::GetOwner(nsISupports * *aOwner)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP DummyChannel::SetOwner(nsISupports * aOwner)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIInterfaceRequestor notificationCallbacks; */
NS_IMETHODIMP DummyChannel::GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks)
{
    return NS_OK;
}
NS_IMETHODIMP DummyChannel::SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks)
{
    return NS_OK;
}

/* readonly attribute nsISupports securityInfo; */
NS_IMETHODIMP DummyChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute string contentType; */
NS_IMETHODIMP DummyChannel::GetContentType(char * *aContentType)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP DummyChannel::SetContentType(const char * aContentType)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long contentLength; */
NS_IMETHODIMP DummyChannel::GetContentLength(PRInt32 *aContentLength)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP DummyChannel::SetContentLength(PRInt32 aContentLength)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIInputStream open (); */
NS_IMETHODIMP DummyChannel::Open(nsIInputStream **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void asyncOpen (in nsIStreamListener listener, in nsISupports ctxt); */
NS_IMETHODIMP DummyChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
