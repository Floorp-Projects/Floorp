/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsPluginStreamPeer.h"
#include "nsIURL.h"
#include "prmem.h"
#include "nsString.h"

nsPluginStreamPeer ::  nsPluginStreamPeer()
{
  NS_INIT_REFCNT();

  mURL = nsnull;
  mLength = 0;
  mLastMod = 0;
  mNotifyData = nsnull;
  mMIMEType = nsnull;
  mURLSpec = nsnull;
  mReason = nsPluginReason_NoReason;
}

nsPluginStreamPeer :: ~nsPluginStreamPeer()
{
  NS_IF_RELEASE(mURL);

  if (nsnull != mMIMEType)
  {
    PR_Free((void *)mMIMEType);
    mMIMEType = nsnull;
  }

  if (nsnull != mURLSpec)
  {
    PR_Free((void *)mURLSpec);
    mURLSpec = nsnull;
  }
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
  if (nsnull != mURL)
  {
    if (nsnull == mURLSpec)
    {
      PRUnichar* str;
      mURL->ToString(&str);
      nsString string = str;
      delete str;

      mURLSpec = (char *)PR_Malloc(string.Length() + 1);

      if (nsnull != mURLSpec)
        string.ToCString(mURLSpec, string.Length() + 1);
      else
        return NS_ERROR_OUT_OF_MEMORY;
    }

    *result = mURLSpec;

    return NS_OK;
  }
  else
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsPluginStreamPeer :: GetEnd(PRUint32 *result)
{
  *result = mLength;
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamPeer :: GetLastModified(PRUint32 *result)
{
  *result = mLastMod;
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamPeer :: GetNotifyData(void* *result)
{
  *result = mNotifyData;
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamPeer :: GetReason(nsPluginReason *result)
{
  *result = mReason;
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamPeer :: GetMIMEType(nsMIMEType *result)
{
  *result = mMIMEType;
  return NS_OK;
}

nsresult nsPluginStreamPeer :: Initialize(nsIURI *aURL, PRUint32 aLength,
                                          PRUint32 aLastMod, nsMIMEType aMIMEType,
                                          void *aNotifyData)
{
  mURL = aURL;
  NS_ADDREF(mURL);

  mLength = aLength;
  mLastMod = aLastMod;

  if (nsnull != aMIMEType)
  {
    PRInt32   len = strlen(aMIMEType);
    mMIMEType = (char *)PR_Malloc(len + 1);

    if (nsnull != mMIMEType)
      strcpy((char *)mMIMEType, (char *)aMIMEType);
    else
      return NS_ERROR_OUT_OF_MEMORY;
  }

  mNotifyData = aNotifyData;

  return NS_OK;
}

nsresult nsPluginStreamPeer :: SetReason(nsPluginReason aReason)
{
  mReason = aReason;
  return NS_OK;
}

// eof
