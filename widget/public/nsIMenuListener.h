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

#ifndef nsIMenuListener_h__
#define nsIMenuListener_h__

#include "nsISupports.h"
#include "nsEvent.h"

class nsIWidget;

// {f2e79602-1700-11d5-bb6f-90f240fe493c}
#define NS_IMENULISTENER_IID      \
{ 0xf2e79602, 0x1700, 0x11d5, \
  { 0xbb, 0x6f, 0x90, 0xf2, 0x40, 0xfe, 0x49, 0x3c } };

/**
 *
 * Menu event listener
 * This interface should only be implemented by the menu manager
 * These are registered with nsWindows to recieve menu events
 */

class nsIMenuListener : public nsISupports {

  public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMENULISTENER_IID)

	 /**
     * Processes a menu item selected event
     * @param aMenuEvent See nsGUIEvent.h 
     * @return whether the event was consumed or ignored. See nsEventStatus
     */
    virtual nsEventStatus MenuItemSelected(const nsMenuEvent & aMenuEvent) = 0;

    /**
     * Processes a menu selected event
     * @param aMenuEvent See nsGUIEvent.h 
     * @return whether the event was consumed or ignored. See nsEventStatus
     */
    virtual nsEventStatus MenuSelected(const nsMenuEvent & aMenuEvent) = 0;

    /**
     * Processes a menu deselect event
     * @param aMenuEvent See nsGUIEvent.h 
     * @return whether the event was consumed or ignored. See nsEventStatus
     */
    virtual nsEventStatus MenuDeselected(const nsMenuEvent & aMenuEvent) = 0;

    virtual nsEventStatus MenuConstruct( const nsMenuEvent & aMenuEvent,
                                          nsIWidget* aParentWindow, void* aNode,
                                          void* aWebShell) = 0;

    virtual nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent) = 0;
    
    virtual nsEventStatus CheckRebuild(PRBool & aMenuEvent) = 0;
    virtual nsEventStatus SetRebuild(PRBool aMenuEvent) = 0;
};

#endif // nsIMenuListener_h__
