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
#ifndef nsIAppShell_h__
#define nsIAppShell_h__

#include "nsString.h"

/**
 * Flags for the getNativeData function.
 * See GetNativeData()
 */
#define NS_NATIVE_SHELL   0


#define NS_IAPPSHELL_IID \
{ 0xa0757c31, 0xeeac, 0x11d1, { 0x9e, 0xc1, 0x0, 0xaa, 0x0, 0x2f, 0xb8, 0x21 } }


/**
 * During the nsIAppShell Run method notify this listener 
 * after each message dispatch.
 * @see SetDispatchListener member function of nsIAppShell 
 */

class nsDispatchListener {
  public:
    virtual void AfterDispatch() = 0;
};

class nsIWidget;

/**
 * Application shell used for Test applications
 */

class nsIAppShell : public nsISupports 
{

public:

 /**
  * Creates an application shell
  */ 

  NS_IMETHOD Create(int* argc, char ** argv) = 0;

 /**
  * Enter an event loop.
  * Don't leave until application exits.
  */
  
  virtual nsresult Run() = 0;

 /**
  * Prepare to process events
  */
  
  NS_IMETHOD Spinup() = 0;

 /**
  * Prepare to stop processing events
  */
  
  NS_IMETHOD Spindown() = 0;

 /**
  * After event dispatch execute app specific code
  */
  
  NS_IMETHOD GetNativeEvent(PRBool &aRealEvent, void *&aEvent) = 0;

 /**
  * After event dispatch execute app specific code
  */
  
  NS_IMETHOD DispatchNativeEvent(PRBool aRealEvent, void * aEvent) = 0;

 /**
  * After event dispatch execute app specific code
  */
  
  NS_IMETHOD SetDispatchListener(nsDispatchListener* aDispatchListener) = 0;

  /**
   * Exit the handle event loop
   */

  NS_IMETHOD Exit() = 0;

  /**
   * Returns Native Data
   */

  virtual void* GetNativeData(PRUint32 aDataType) = 0;

  /**
   * Determines whether a given event should be processed assuming the given
   * widget is a currently active modal window
   */
  NS_IMETHOD EventIsForModalWindow(PRBool aRealEvent, void *aEvent, nsIWidget *aWidget,
                                  PRBool *aForWindow) = 0;

};

#endif // nsIAppShell_h__


