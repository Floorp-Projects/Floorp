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
#include <stdio.h>
#include "nsIApplicationShell.h"
#include "nsIFactory.h"
#include "nscore.h"
#include "nsweb.h"
#include "nsCRT.h"
#include "nsIShellInstance.h"

// platform independent native handle to application instance
typedef void * nsNativeApplicationInstance ;

class nsShellInstance : public nsIShellInstance 
{
public:

  /**
   * Constructor and Destructor
   */

  nsShellInstance();
  ~nsShellInstance();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  /**
   * ISupports Interface
   */
  NS_DECL_ISUPPORTS

  /**
   * Initialize Method
   * @result The result of the initialization, NS_OK if no errors
   */
  NS_IMETHOD Init();

  /**
   * Event Loop Execution
   * @result The result of the initialization, NS_OK if no errors
   */
  NS_IMETHOD Run();

  /**
   * Application Wide Factory Registration
   * @result The result of the initialization, NS_OK if no errors
   */
  NS_IMETHOD RegisterFactories();

  /**
   * Get the native instance associated with this shell instance
   * @result An opaque native instance pointer
   */
  NS_IMETHOD_(void *) GetNativeInstance(); 

  /**
   * Set the Native Instance associated with this shell instance
   * @param aNativeInstance The native instance
   * @result None
   */
  NS_IMETHOD_(void)   SetNativeInstance(void * aNativeInstance);

  /**
   * Get the Application Shell
   * @result nsIApplicationShell Application Shell
   */
  NS_IMETHOD_(nsIApplicationShell *) GetApplicationShell(); 

  /**
   * Set the Application Shell for this Instance
   * @param aApplicationShell the Application Shell
   * @result None
   */
  NS_IMETHOD_(void) SetApplicationShell(nsIApplicationShell * aApplicationShell);

  /**
   * Create a toplevel Application Window for this application
   * @param nsRect Rect in screen coordinates of toplevel window
   * @param aHandleEventFunction Event Loop Callbacl function
   * @result a Pointer to the nsIWidget representing the toplevel Window
   */
  NS_IMETHOD_(nsIWidget *) CreateApplicationWindow(const nsRect &aRect,
                                                   EVENT_CALLBACK aHandleEventFunction) ;

  /**
   * Show/Hide the toplevel Application Window
   * @param show Boolean of PR_TRUE/PR_FALSE on whether to Show/Hide the window
   * @result nsresult NS_OK upon successful completion
   */
  NS_IMETHOD ShowApplicationWindow(PRBool show) ;

  /**
   * Get the native window associated with the toplevel Application Instance
   * @result Opaque handle to the application window native instance
   */
  NS_IMETHOD_(void *) GetApplicationWindowNativeInstance() ; 

  /**
   * Get the widget associated with the toplevel Application Instance
   * @result a Pointer to the nsIWidget representing the toplevel Window
   */  
  NS_IMETHOD_(nsIWidget *) GetApplicationWidget() ; 

  /**
   * Exit this application
   * @result nsresult NS_OK upon succcessful completion
   */  
  NS_IMETHOD ExitApplication() ;

private:
  nsNativeApplicationInstance mNativeInstance ;
  nsIApplicationShell *       mApplicationShell ;
  nsIWidget *                 mApplicationWindow ;

};


