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
#include <stdio.h>
#include "prmem.h"
#include "plstr.h"

nsPluginInstancePeerImpl :: nsPluginInstancePeerImpl()
{
  NS_INIT_REFCNT();

  mInstance = nsnull;
  mOwner = nsnull;
  mMIMEType = nsnull;
}

nsPluginInstancePeerImpl :: ~nsPluginInstancePeerImpl()
{
  mInstance = nsnull;
  mOwner = nsnull;

  if (nsnull != mMIMEType)
  {
    PR_Free((void *)mMIMEType);
    mMIMEType = nsnull;
  }
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
        *instance = (void *)(nsIPluginInstancePeer *)this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kIPluginTagInfoIID))
    {
        *instance = (void *)(nsIPluginTagInfo *)this;
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
printf("instance peer getvalue %d called\n", variable);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetMIMEType(nsMIMEType *result)
{
  if (nsnull == mMIMEType)
    *result = "";
  else
    *result = mMIMEType;

  return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetMode(nsPluginMode *result)
{
  if (nsnull != mOwner)
    return mOwner->GetMode(result);
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result)
{
printf("instance peer newstream called\n");
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: ShowStatus(const char* message)
{
printf("instance peer showstatus called\n");
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetAttributes(PRUint16& n, const char*const*& names, const char*const*& values)
{
  if (nsnull != mOwner)
    return mOwner->GetAttributes(n, names, values);
  else
  {
    n = 0;
    names = nsnull;
    values = nsnull;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetAttribute(const char* name, const char* *result)
{
  if (nsnull != mOwner)
    return mOwner->GetAttribute(name, result);
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: SetWindowSize(PRUint32 width, PRUint32 height)
{
printf("instance peer setwindowsize called\n");
  return NS_OK;
}

nsresult nsPluginInstancePeerImpl :: Initialize(nsIPluginInstanceOwner *aOwner,
                                                const nsMIMEType aMIMEType)
{
  //don't add a ref to precent circular references... MMP
  mOwner = aOwner;

  aOwner->GetInstance(mInstance);
  //release this one too... MMP
  NS_IF_RELEASE(mInstance);

  if (nsnull != aMIMEType)
  {
    mMIMEType = (nsMIMEType)PR_Malloc(PL_strlen(aMIMEType) + 1);

    if (nsnull != mMIMEType)
      PL_strcpy((char *)mMIMEType, aMIMEType);
  }

  return NS_OK;
}

nsresult nsPluginInstancePeerImpl :: GetOwner(nsIPluginInstanceOwner *&aOwner)
{
  aOwner = mOwner;
  NS_IF_ADDREF(mOwner);

  if (nsnull != mOwner)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}
