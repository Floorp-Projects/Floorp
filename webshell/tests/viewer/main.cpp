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

#include "nsViewer.h"
#include "nsMotifMenu.h"
#include "nsIImageManager.h"
#include <stdlib.h>


class nsMotifViewer : public nsViewer {
    virtual void AddMenu(nsIWidget* aMainWindow, PRBool aForPrintPreview);
};

//--------------------------------------------------------
nsresult ExecuteViewer(nsViewer* aViewer, int argc, char **argv)
{
  nsIWidget *mainWindow = nsnull;
  nsDocLoader* dl = aViewer->SetupViewer(&mainWindow, argc, argv);
  nsresult result = aViewer->Run();
  aViewer->CleanupViewer(dl);

  return result;
}

//--------------------------------------------------------
nsMotifViewer * gViewer = nsnull;

//--------------------------------------------------------
void main(int argc, char **argv)
{

  // Hack to get il_ss set so it doesn't fail in xpcompat.c
  nsIImageManager *manager;
  NS_NewImageManager(&manager);

  gViewer = new nsMotifViewer();
  SetViewer(gViewer);
  gViewer->ProcessArguments(argc, argv);
  ExecuteViewer(gViewer, argc, argv);
}

//--------------------------------------------------------
void MenuProc(PRUint32 aId) 
{
  gViewer->DispatchMenuItem(aId);
}

//--------------------------------------------------------
void nsMotifViewer::AddMenu(nsIWidget* aMainWindow, PRBool aForPrintPreview)
{
  CreateViewerMenus(XtParent((Widget)aMainWindow->GetNativeData(NS_NATIVE_WIDGET)), MenuProc);
}


