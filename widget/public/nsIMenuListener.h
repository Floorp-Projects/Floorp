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

#ifndef nsIMenuListener_h__
#define nsIMenuListener_h__

#include "nsGUIEvent.h"
#include "nsISupports.h"

// TODO: This needs to be generated!
// {BC658C81-4BEB-11d2-8DBB-00609703C14E}
#define NS_IMENULISTENER_IID      \
{ 0xbc658c81, 0x4beb, 0x11d2, \
  { 0x8d, 0xbb, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x9e } }
  
static NS_DEFINE_IID(kIMenuListenerIID, NS_IMENULISTENER_IID);

/**
 *
 * Menu event listener
 * This interface should only be implemented by the menu manager
 * These are registered with nsWindows to recieve menu events
 */

class nsIMenuListener : public nsISupports {

  public:
    static const nsIID& GetIID() { static nsIID iid = NS_IMENULISTENER_IID; return iid; }

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

    virtual nsEventStatus MenuConstruct(const nsMenuEvent & aMenuEvent) = 0;
    virtual nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent) = 0;
};

#endif // nsIMenuListener_h__
