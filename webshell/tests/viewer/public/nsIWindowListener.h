/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

