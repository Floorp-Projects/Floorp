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
static NS_DEFINE_IID(kIPluginTagInfo2IID, NS_IPLUGINTAGINFO2_IID); 
static NS_DEFINE_IID(kIPluginInstancePeerIID, NS_IPLUGININSTANCEPEER_IID); 
static NS_DEFINE_IID(kIJVMPluginTagInfoIID, NS_IJVMPLUGINTAGINFO_IID); 
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

    if (iid.Equals(kIPluginTagInfo2IID))
    {
        *instance = (void *)(nsIPluginTagInfo2 *)this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kIJVMPluginTagInfoIID))
    {
        *instance = (void *)(nsIJVMPluginTagInfo *)this;
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
  if (nsnull != mOwner)
    return mOwner->ShowStatus(message);
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetAttributes(PRUint16& n, const char*const*& names, const char*const*& values)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo  *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetAttributes(n, names, values);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
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
  {
    nsIPluginTagInfo  *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetAttribute(name, result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetTagType(nsPluginTagType *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetTagType(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = nsPluginTagType_Unknown;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetTagText(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetTagText(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetParameters(PRUint16& n, const char*const*& names, const char*const*& values)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetParameters(n, names, values);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    n = 0;
    names = nsnull;
    values = nsnull;

    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetParameter(const char* name, const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetParameter(name, result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetDocumentBase(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetDocumentBase(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetDocumentEncoding(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetDocumentEncoding(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetAlignment(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetAlignment(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetWidth(PRUint32 *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetWidth(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetHeight(PRUint32 *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetHeight(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetBorderVertSpace(PRUint32 *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetBorderVertSpace(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetBorderHorizSpace(PRUint32 *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetBorderHorizSpace(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetUniqueID(PRUint32 *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetUniqueID(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetCode(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIJVMPluginTagInfo *tinfo;
    nsresult            rv;

    rv = mOwner->QueryInterface(kIJVMPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetCode(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetCodeBase(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIJVMPluginTagInfo *tinfo;
    nsresult            rv;

    rv = mOwner->QueryInterface(kIJVMPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetCodeBase(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetArchive(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIJVMPluginTagInfo *tinfo;
    nsresult            rv;

    rv = mOwner->QueryInterface(kIJVMPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetArchive(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetName(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIJVMPluginTagInfo *tinfo;
    nsresult            rv;

    rv = mOwner->QueryInterface(kIJVMPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetName(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetMayScript(PRBool *result)
{
  if (nsnull != mOwner)
  {
    nsIJVMPluginTagInfo *tinfo;
    nsresult            rv;

    rv = mOwner->QueryInterface(kIJVMPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetMayScript(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
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
