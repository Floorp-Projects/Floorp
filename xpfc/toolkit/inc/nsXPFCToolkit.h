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

#ifndef nsXPFCToolkit_h___
#define nsXPFCToolkit_h___

#include "nsxpfc.h"
#include "nsIXPFCToolkit.h"
#include "nsIApplicationShell.h"
#include "nsIXPFCCanvasManager.h"
#include "nsIViewManager.h"

CLASS_EXPORT_XPFC nsXPFCToolkit : public nsIXPFCToolkit
{
public:
  nsXPFCToolkit();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIApplicationShell * aApplicationShell) ;

  NS_IMETHOD SetCanvasManager(nsIXPFCCanvasManager * aCanvasManager);
  NS_IMETHOD_(nsIXPFCCanvasManager *) GetCanvasManager();

  NS_IMETHOD GetRootCanvas(nsIXPFCCanvas ** aCanvas);
  NS_IMETHOD_(EVENT_CALLBACK) GetShellEventCallback() ;

  NS_IMETHOD SetApplicationShell(nsIApplicationShell * aApplicationShell) ;
  NS_IMETHOD_(nsIApplicationShell *) GetApplicationShell() ;

  NS_IMETHOD_(nsIViewManager *) GetViewManager() ;

protected:
  ~nsXPFCToolkit();

private:
  nsIApplicationShell * mApplicationShell;
  nsIXPFCCanvasManager * mCanvasManager;

};

// XXX: Need a SessionManager to manage various toolkits across
//      different applications.  For now, this is a convenient
//      way to access the global application shell
extern NS_XPFC nsXPFCToolkit * gXPFCToolkit;

#endif /* nsXPFCToolkit_h___ */
