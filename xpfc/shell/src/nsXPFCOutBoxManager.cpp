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

#include "nsXPFCOutBoxManager.h"
#include "nsxpfcCIID.h"
#include "nsxpfcutil.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCXPFCOutBoxManagerCID, NS_XPFCOUTBOX_MANAGER_CID);
static NS_DEFINE_IID(kIXPFCOutBoxManagerIID, NS_IXPFCOUTBOX_MANAGER_IID);

nsXPFCOutBoxManager :: nsXPFCOutBoxManager()
{
  NS_INIT_REFCNT();
}

nsXPFCOutBoxManager :: ~nsXPFCOutBoxManager()
{
}

NS_IMPL_QUERY_INTERFACE(nsXPFCOutBoxManager, kIXPFCOutBoxManagerIID)
NS_IMPL_ADDREF(nsXPFCOutBoxManager)
NS_IMPL_RELEASE(nsXPFCOutBoxManager)

nsresult
nsXPFCOutBoxManager :: Init()
{
  return NS_OK ;
}

nsresult
nsXPFCOutBoxManager :: AddItem(nsString& mimeType, nsIInputStream& inStream)
{
  return NS_OK ;
}

nsresult
nsXPFCOutBoxManager :: FindItem(nsString& mimeType, nsIOutputStream* outSteam)
{
  return NS_OK ;
}

nsresult
nsXPFCOutBoxManager :: ItemsCount(PRInt32 *OutBoxItems)
{
  return NS_OK ;
}

nsresult
nsXPFCOutBoxManager :: ItemsCount(nsString& mimeType, PRInt32 *OutBoxItems)
{
  return NS_OK ;
}

nsresult
nsXPFCOutBoxManager :: ItemsSize(PRInt32 *OutBoxItemsSize)
{
  return NS_OK ;
}

nsresult
nsXPFCOutBoxManager :: AddHandler(nsIXPFCOutBoxItemHandler*& AnOutBoxHandler, nsIXPFCOutBoxItemHandlerCallback*& host)
{
  return NS_OK ;
}

nsresult
nsXPFCOutBoxManager :: SendItems()
{
  return NS_OK ;
}


