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

#include "nscore.h"
#include "nsPluginInstancePeer.h"
#include "nsIPluginInstance.h"

nsPluginInstancePeerImpl :: nsPluginInstancePeerImpl()
{
  mInstance = nsnull;
}

nsPluginInstancePeerImpl :: ~nsPluginInstancePeerImpl()
{
  mInstance = nsnull;
}

NS_IMPL_ADDREF(nsPluginInstancePeerImpl);
NS_IMPL_RELEASE(nsPluginInstancePeerImpl);

static NS_DEFINE_IID(kIPluginTagInfoIID, NS_IPLUGINTAGINFO_IID); 
static NS_DEFINE_IID(kIPluginInstancePeerIID, NS_IPLUGININSTANCEPEER_IID); 
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsresult nsPluginInstancePeerImpl :: QueryInterface(const nsIID& iid, void** instance)
{
    if (instance == NULL)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIPluginInstancePeerIID))
    {
        *instance = (void *)(nsISupports *)(nsIPluginInstancePeer *)this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kIPluginTagInfoIID))
    {
        *instance = (void *)(nsISupports *)(nsIPluginTagInfo *)this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kISupportsIID))
    {
        *instance = (void *)(nsISupports *)(nsIPluginTagInfo *)this;
        AddRef();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetValue(nsPluginInstancePeerVariable variable, void *value)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: SetValue(nsPluginInstancePeerVariable variable, void *value)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetMIMEType(nsMIMEType *result)
{
  *result = "model/vrml";
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetMode(nsPluginMode *result)
{
  *result = nsPluginMode_Full;
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: ShowStatus(const char* message)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetAttributes(PRUint16& n, const char*const*& names, const char*const*& values)
{
  n = 0;
  names = nsnull;
  values = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetAttribute(const char* name, const char* *result)
{
  *result = 0;
  return NS_OK;
}

nsresult nsPluginInstancePeerImpl :: Initialize(nsIPluginInstance *aInstance)
{
  //don't add a ref to precent circular references... MMP
  mInstance = aInstance;

  return NS_OK;
}
