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

#include "nsXPFCToolkit.h"
#include "nsxpfcCIID.h"
#include "nsXPFCCanvasManager.h"

static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kXPFCToolkitIID, NS_IXPFC_TOOLKIT_IID);

NS_XPFC nsXPFCToolkit * gXPFCToolkit = nsnull;
nsEventStatus PR_CALLBACK HandleEventApplication(nsGUIEvent *aEvent);

nsXPFCToolkit :: nsXPFCToolkit()
{
  NS_INIT_REFCNT();

  if (gXPFCToolkit == nsnull)
    gXPFCToolkit = (nsXPFCToolkit *)this;

  mApplicationShell = nsnull;
  mCanvasManager    = nsnull;

}

nsXPFCToolkit :: ~nsXPFCToolkit()  
{
  NS_RELEASE(mCanvasManager);
  gXPFCToolkit = nsnull;
}

NS_IMPL_ADDREF(nsXPFCToolkit)
NS_IMPL_RELEASE(nsXPFCToolkit)
NS_IMPL_QUERY_INTERFACE(nsXPFCToolkit, kXPFCToolkitIID)

nsresult nsXPFCToolkit::Init(nsIApplicationShell * aApplicationShell)
{
  mApplicationShell = aApplicationShell;

  /*
   * Create the canvas manager
   */

  static NS_DEFINE_IID(kCXPFCCanvasManagerCID, NS_XPFC_CANVASMANAGER_CID);
  static NS_DEFINE_IID(kCXPFCCanvasManagerIID, NS_IXPFC_CANVAS_MANAGER_IID);

  nsresult res = nsRepository::CreateInstance(kCXPFCCanvasManagerCID, 
                                              nsnull, 
                                              kCXPFCCanvasManagerIID, 
                                              (void **)&mCanvasManager);

  if (NS_OK != res)
    return res ;

  mCanvasManager->Init();

  return NS_OK;
}

nsresult nsXPFCToolkit::SetApplicationShell(nsIApplicationShell * aApplicationShell)
{
  mApplicationShell = aApplicationShell;
  return NS_OK;
}

nsIApplicationShell * nsXPFCToolkit::GetApplicationShell()
{
  return (mApplicationShell);
}

nsresult nsXPFCToolkit::SetCanvasManager(nsIXPFCCanvasManager * aCanvasManager)
{
  mCanvasManager = aCanvasManager;
  return NS_OK;
}

nsIXPFCCanvasManager * nsXPFCToolkit::GetCanvasManager()
{
  return (mCanvasManager);
}

nsresult nsXPFCToolkit::GetRootCanvas(nsIXPFCCanvas ** aCanvas)
{
  return (mCanvasManager->GetRootCanvas(aCanvas));
}

nsIViewManager * nsXPFCToolkit::GetViewManager()
{
  return (mCanvasManager->GetViewManager());
}

EVENT_CALLBACK nsXPFCToolkit::GetShellEventCallback()
{
  return ((EVENT_CALLBACK)HandleEventApplication);
}
