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

#ifndef nsIShellInstance_h___
#define nsIShellInstance_h___

#include "nsISupports.h"
#include "nsIApplicationShell.h"
#include "nsIWidget.h"
#include "nsshell.h"
#include "nscore.h"

class nsIApplicationShell;

// bf88e640-df99-11d1-9244-00805f8a7ab6
#define NS_ISHELLINSTANCE_IID      \
 { 0xbf88e640, 0xdf99, 0x11d1, \
   {0x92, 0x44, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

// Interface to the application shell.
class nsIShellInstance : public nsISupports {
public:
  // Create a native window for this web widget; may be called once
  NS_IMETHOD Init() = 0;

  NS_IMETHOD Run() = 0;

  NS_IMETHOD_(void *) GetNativeInstance() = 0; 
  NS_IMETHOD_(void)   SetNativeInstance(void * aNativeInstance) = 0;

  NS_IMETHOD_(nsIApplicationShell *) GetApplicationShell() = 0; 
  NS_IMETHOD_(void)   SetApplicationShell(nsIApplicationShell * aApplicationShell) = 0;

  NS_IMETHOD_(nsIWidget *) CreateApplicationWindow(const nsRect &aRect,
                                                   EVENT_CALLBACK aHandleEventFunction) = 0 ;

  NS_IMETHOD ShowApplicationWindow(PRBool show) = 0 ;

  NS_IMETHOD_(void *) GetApplicationWindowNativeInstance() = 0; 
  NS_IMETHOD_(nsIWidget *) GetApplicationWidget() = 0; 

  NS_IMETHOD ExitApplication() = 0 ;

};

#endif /* nsIShellInstance_h___ */

