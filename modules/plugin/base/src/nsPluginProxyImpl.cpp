/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsPluginProxyImpl.h"
#include "nsCRT.h"

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsPluginProxyImpl, nsIProxy)

  nsPluginProxyImpl::nsPluginProxyImpl() : 
    mProxyHost(nsnull), mProxyType(nsnull), mProxyPort(-1)
{
  NS_INIT_ISUPPORTS();
  
}

nsPluginProxyImpl::~nsPluginProxyImpl()
{
  if (mProxyHost) {
    nsCRT::free(mProxyHost);
  }
  if (mProxyType) {
    nsCRT::free(mProxyType);
  }
  mProxyPort = -1;
}

/* attribute string proxyHost; */
NS_IMETHODIMP nsPluginProxyImpl::GetProxyHost(char * *aProxyHost)
{
  if (!aProxyHost) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rc = NS_ERROR_FAILURE;

  if (mProxyHost) {
    if (*aProxyHost = nsCRT::strdup(mProxyHost)) {
      rc = NS_OK;
    }
  }
  return rc;
}
NS_IMETHODIMP nsPluginProxyImpl::SetProxyHost(const char * aProxyHost)
{
  nsresult rc = NS_ERROR_FAILURE;

  if (mProxyHost) {
    nsCRT::free(mProxyHost);
    mProxyHost = nsnull;
  }
  if (aProxyHost) {
    if (mProxyHost = nsCRT::strdup(aProxyHost)) {
      rc = NS_OK;
    }
  }
  else {
    // setting our mProxyHost to nsnull
    rc = NS_OK;
  }

  return rc;
}

/* attribute long proxyPort; */
NS_IMETHODIMP nsPluginProxyImpl::GetProxyPort(PRInt32 *aProxyPort)
{
  if (!aProxyPort) {
    return NS_ERROR_NULL_POINTER;
  }
  *aProxyPort = mProxyPort;
  return NS_OK;
}
NS_IMETHODIMP nsPluginProxyImpl::SetProxyPort(PRInt32 aProxyPort)
{
  mProxyPort = aProxyPort;
  return NS_OK;
}

/* attribute string proxyType; */
NS_IMETHODIMP nsPluginProxyImpl::GetProxyType(char * *aProxyType)
{
  if (!aProxyType) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rc = NS_ERROR_FAILURE;

  if (mProxyType) {
    if (*aProxyType = nsCRT::strdup(mProxyType)) {
      rc = NS_OK;
    }
  }
  return rc;
}
NS_IMETHODIMP nsPluginProxyImpl::SetProxyType(const char * aProxyType)
{
  nsresult rc = NS_ERROR_FAILURE;

  if (mProxyType) {
    nsCRT::free(mProxyType);
    mProxyType = nsnull;
  }
  if (aProxyType) {
    if (mProxyType = nsCRT::strdup(aProxyType)) {
      rc = NS_OK;
    }
  }
  else {
    // setting our mProxyType to nsnull
    rc = NS_OK;
  }

  return rc;
}
