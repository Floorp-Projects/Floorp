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

#ifndef nsMenuItem_h__
#define nsMenuItem_h__

#include "nsdefs.h"
#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsSwitchToUIThread.h"

#include "nsIMenuItem.h"
class nsIMenu;

/**
 * Native Win32 MenuItem wrapper
 */

class nsMenuItem : public nsIMenuItem
{

public:
  nsMenuItem();
  virtual ~nsMenuItem();

  // nsISupports
  NS_DECL_ISUPPORTS
  
  NS_IMETHOD   Create(nsIWidget *aParent,
                         const nsRect &aRect,
                         EVENT_CALLBACK aHandleEventFunction,
                         nsIDeviceContext *aContext,
                         nsIAppShell *aAppShell = nsnull,
                         nsIToolkit *aToolkit = nsnull,
                         nsWidgetInitData *aInitData = nsnull);
  NS_IMETHOD   Create(nsNativeWidget aParent,
                         const nsRect &aRect,
                         EVENT_CALLBACK aHandleEventFunction,
                         nsIDeviceContext *aContext,
                         nsIAppShell *aAppShell = nsnull,
                         nsIToolkit *aToolkit = nsnull,
                         nsWidgetInitData *aInitData = nsnull);

  // nsIMenuBar Methods
  NS_IMETHOD SetLabel(const nsString &aText);
  NS_IMETHOD GetLabel(nsString &aText);
  NS_IMETHOD SetMenu(nsIMenu * aMenu);
  NS_IMETHOD SetCommand(PRUint32 aCommand);
  NS_IMETHOD GetCommand(PRUint32 & aCommand);


protected:
  nsString   mLabel;
  nsIMenu  * mMenu;
  PRUint32   mCommand;
};

#endif // nsMenuItem_h__
