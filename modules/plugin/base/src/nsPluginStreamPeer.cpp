/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

#include "nsPluginStreamPeer.h"

nsPluginStreamPeer ::  nsPluginStreamPeer()
{
}

nsPluginStreamPeer :: ~nsPluginStreamPeer()
{
}

NS_IMPL_ADDREF(nsPluginStreamPeer);
NS_IMPL_RELEASE(nsPluginStreamPeer);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPluginStreamPeerIID, NS_IPLUGINSTREAMPEER_IID);
//static NS_DEFINE_IID(kISeekablePluginStreamPeerIID, NS_ISEEKABLEPLUGINSTREAMPEER_IID);

NS_IMETHODIMP nsPluginStreamPeer :: QueryInterface(const nsIID& iid, void** instance)
{
  if (instance == NULL)
      return NS_ERROR_NULL_POINTER;

  if (iid.Equals(kIPluginStreamPeerIID))
  {
      *instance = (void *)(nsIPluginStreamPeer *)this;
      AddRef();
      return NS_OK;
  }
  else if (iid.Equals(kISupportsIID))
  {
      *instance = (void *)(nsISupports *)this;
      AddRef();
      return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMETHODIMP nsPluginStreamPeer :: GetURL(const char* *result)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamPeer :: GetEnd(PRUint32 *result)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamPeer :: GetLastModified(PRUint32 *result)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamPeer :: GetNotifyData(void* *result)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamPeer :: GetReason(nsPluginReason *result)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamPeer :: GetMIMEType(nsMIMEType *result)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamPeer :: Initialize(void)
{
  return NS_OK;
}
