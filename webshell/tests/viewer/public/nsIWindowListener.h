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

#ifndef nsIWindowListener_h__
#define nsIWindowListener_h__

#include "prtypes.h"

class nsIDOMEvent;
class nsIXPBaseWindow;

/**
 * Listens for window mouse events, key clicks etc.
 * Also initializes the contents for form elements.
 */
class nsIWindowListener {

public:
    
   /**
    * Method called when the user clicks the mouse.
    * Clicks are only generated when the mouse-up event happens
    * over a widget.
    *
    * @param aMouseEvent DOM event holding mouse click info.
    * @param aWindow Window which generated the mouse click event
    */

    virtual void MouseClick(nsIDOMEvent* aMouseEvent, nsIXPBaseWindow * aWindow, PRBool &aStatus) = 0;

   /**
    * Method called After the URL passed to the dialog box or window has
    * completed loading. Usually it is used to set place the initial settings
    * in form elements. 
    * @param aWindow the window to initialize form element settings for. 
    */

    virtual void Initialize(nsIXPBaseWindow * aWindow) = 0;

   /**
    * Method called when dialog box or window is no longer visibleg
    * @param aWindow the window which is about to be destroyed 
    */

    virtual void Destroy(nsIXPBaseWindow * aWindow) = 0;
 
};

#endif  // nsIWindowListener_h__

