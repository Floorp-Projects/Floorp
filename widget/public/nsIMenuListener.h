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

/**
 *
 * Menu event listener
 * This interface should only be implemented by the menu manager
 * These are registered with nsWindows to recieve menu events
 */

class nsIMenuListener {

  public:

    /**
     * Processes a menu selected event
     * @param aMenuEvent See nsGUIEvent.h 
     * @return whether the event was consumed or ignored. See nsEventStatus
     */
    virtual nsEventStatus MenuSelected(const nsGUIEvent & aMenuEvent) = 0;

};

#endif // nsIMenuListener_h__
