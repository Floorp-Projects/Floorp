/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsURIDispatcher.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsString.h"

nsURIDispatcher::nsURIDispatcher()
{
  NS_INIT_ISUPPORTS();
  m_listeners = new nsVoidArray();
}

nsURIDispatcher::~nsURIDispatcher()
{
  if (m_listeners)
    delete m_listeners;
}

NS_IMPL_ISUPPORTS1(nsURIDispatcher, nsIURIDispatcher)

NS_IMETHODIMP nsURIDispatcher::RegisterContentListener(nsIURIContentListener * aContentListener)
{
  nsresult rv = NS_OK;
  if (m_listeners)
    m_listeners->AppendElement(aContentListener);
  else
    rv = NS_ERROR_FAILURE;

  return rv;
} 

NS_IMETHODIMP nsURIDispatcher::UnRegisterContentListener(nsIURIContentListener * aContentListener)
{
  if (m_listeners)
    m_listeners->RemoveElement(aContentListener);
  return NS_OK;
  
}

NS_IMETHODIMP nsURIDispatcher::OpenURI(nsIURI *aURI, nsIStreamObserver *aStreamObserver, 
                                       nsIURIContentListener *aContentListener, nsISupports *aContext, 
                                       nsIURI *aReferringURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

