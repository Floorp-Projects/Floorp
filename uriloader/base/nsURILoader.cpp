/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsURILoader.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsString.h"

nsURILoader::nsURILoader()
{
  NS_INIT_ISUPPORTS();
  m_listeners = new nsVoidArray();
}

nsURILoader::~nsURILoader()
{
  if (m_listeners)
    delete m_listeners;
}

NS_IMPL_ISUPPORTS1(nsURILoader, nsIURILoader)

NS_IMETHODIMP nsURILoader::RegisterContentListener(nsIURIContentListener * aContentListener)
{
  nsresult rv = NS_OK;
  if (m_listeners)
    m_listeners->AppendElement(aContentListener);
  else
    rv = NS_ERROR_FAILURE;

  return rv;
} 

NS_IMETHODIMP nsURILoader::UnRegisterContentListener(nsIURIContentListener * aContentListener)
{
  if (m_listeners)
    m_listeners->RemoveElement(aContentListener);
  return NS_OK;
  
}

NS_IMETHODIMP nsURILoader::OpenURI(nsIURI *aURI, nsIStreamObserver *aStreamObserver, 
                                       nsIURIContentListener *aContentListener, nsISupports *aContext, 
                                       nsIURI *aReferringURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

