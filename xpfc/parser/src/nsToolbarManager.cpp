/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsToolbarManager.h"
#include "nsxpfcCIID.h"
#include "nsxpfcutil.h"
#include "nsIXPFCToolbar.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCToolbarManagerCID, NS_TOOLBAR_MANAGER_CID);
static NS_DEFINE_IID(kIToolbarManagerIID, NS_ITOOLBAR_MANAGER_IID);

nsToolbarManager :: nsToolbarManager()
{
  NS_INIT_REFCNT();
  mToolbars = nsnull;
}

nsToolbarManager :: ~nsToolbarManager()
{
  if (mToolbars != nsnull) 
  {
    mToolbars->RemoveAll();
    NS_RELEASE(mToolbars);
  }

}

NS_IMPL_QUERY_INTERFACE(nsToolbarManager, kIToolbarManagerIID)
NS_IMPL_ADDREF(nsToolbarManager)
NS_IMPL_RELEASE(nsToolbarManager)

nsresult nsToolbarManager :: Init()
{
  static NS_DEFINE_IID(kCVectorCID, NS_VECTOR_CID);
  nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                     nsnull, 
                                     kCVectorCID, 
                                     (void **)&mToolbars);

  if (NS_OK != res)
    return res ;

  mToolbars->Init();

  return NS_OK ;
}

nsresult nsToolbarManager :: AddToolbar(nsIXPFCToolbar * aToolbar)
{
  mToolbars->Append(aToolbar);
  return NS_OK ;
}

