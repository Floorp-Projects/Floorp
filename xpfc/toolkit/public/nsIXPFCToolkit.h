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

#ifndef nsIXPFCToolkit_h___
#define nsIXPFCToolkit_h___

#include "nsISupports.h"
#include "nsIXPFCCanvasManager.h"
#include "nsIXPFCCanvas.h"


class nsIApplicationShell;
class nsIViewManager;

//d666a630-32e1-11d2-9248-00805f8a7ab6
#define NS_IXPFC_TOOLKIT_IID   \
{ 0xd666a630, 0x32e1, 0x11d2,    \
{ 0x92, 0x48, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsIXPFCToolkit : public nsISupports
{

public:

  NS_IMETHOD Init(nsIApplicationShell * aApplicationShell) = 0;

  NS_IMETHOD SetCanvasManager(nsIXPFCCanvasManager * aCanvasManager) = 0;
  NS_IMETHOD_(nsIXPFCCanvasManager *) GetCanvasManager() = 0;

  NS_IMETHOD GetRootCanvas(nsIXPFCCanvas ** aCanvas) = 0;
  NS_IMETHOD_(nsIViewManager *) GetViewManager() = 0;
  NS_IMETHOD_(EVENT_CALLBACK) GetShellEventCallback() = 0;

  NS_IMETHOD SetApplicationShell(nsIApplicationShell * aApplicationShell) = 0;
  NS_IMETHOD_(nsIApplicationShell *) GetApplicationShell() = 0;

};

#endif /* nsIXPFCToolkit_h___ */


