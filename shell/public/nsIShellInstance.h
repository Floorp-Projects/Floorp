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

#include "nscore.h"
#include "nsshell.h"
#include "nsISupports.h"
#include "nsIApplicationShell.h"
#include "nsIWidget.h"

class nsIApplicationShell;

#define NS_ISHELLINSTANCE_IID      \
 { 0xbf88e640, 0xdf99, 0x11d1, \
   {0x92, 0x44, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

#define NS_SHELLINSTANCE_CID      \
 { 0x90487580, 0xfefe, 0x11d1, \
   {0xbe, 0xcd, 0x00, 0x80, 0x5f, 0x8a, 0x8d, 0xbd} }


// An Interface for interacting with a specific Application Instance
class nsIShellInstance : public nsISupports 
{
public:

  /**
   * Initialize the ShellInstance
   * @result The result of the initialization, NS_Ok if no errors
   */
  NS_IMETHOD Init() = 0;

  /**
   * Start the Shell's Event Loop.  
   * @result The result of the event loop execution, NS_Ok if appropriate Exit Message occured
   */
  NS_IMETHOD Run() = 0;

  /**
   * Get Platform Specific Native Instance Handle representing this application
   * @result An opaque pointer to the native application instance
   */
  NS_IMETHOD_(void *) GetNativeInstance() = 0; 

  /**
   * Set the Platform Specific Native Instance Handle representing this application
   * @param aNativeInstance Opaque handle to the native instance
   * @result none
   */
  NS_IMETHOD_(void)   SetNativeInstance(void * aNativeInstance) = 0;

  /**
   * Get a Handle to the ApplicationShell Interface for this Instance
   * @result nsIApplicationShell* a Pointer to the ApplicationShell
   */
  NS_IMETHOD_(nsIApplicationShell *) GetApplicationShell() = 0; 

  /**
   * Set a Handle to the ApplicationShell Interface for this Instance
   * @param aApplicationShell nsIApplicationShell Object
   * @result none
   */
  NS_IMETHOD_(void)   SetApplicationShell(nsIApplicationShell * aApplicationShell) = 0;

  /**
   * Create a toplevel Application Window for this application
   * @param nsRect Rect in screen coordinates of toplevel window
   * @param aHandleEventFunction Event Loop Callbacl function
   * @result a Pointer to the nsIWidget representing the toplevel Window
   */
  NS_IMETHOD_(nsIWidget *) CreateApplicationWindow(const nsRect &aRect,
                                                   EVENT_CALLBACK aHandleEventFunction) = 0 ;

  /**
   * Show/Hide the toplevel Application Window
   * @param show Boolean of PR_TRUE/PR_FALSE on whether to Show/Hide the window
   * @result nsresult NS_OK upon successful completion
   */
  NS_IMETHOD ShowApplicationWindow(PRBool show) = 0 ;

  /**
   * Get the native window associated with the toplevel Application Instance
   * @result Opaque handle to the application window native instance
   */
  NS_IMETHOD_(void *) GetApplicationWindowNativeInstance() = 0; 

  /**
   * Get the widget associated with the toplevel Application Instance
   * @result a Pointer to the nsIWidget representing the toplevel Window
   */  
  NS_IMETHOD_(nsIWidget *) GetApplicationWidget() = 0; 

  /**
   * Exit this application
   * @result nsresult NS_OK upon succcessful completion
   */  
  NS_IMETHOD ExitApplication() = 0 ;

};

#endif /* nsIShellInstance_h___ */

