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


#ifndef nsIMouseListener_h__
#define nsIMouseListener_h__

#include "nsGUIEvent.h"

/**
 *
 * Mouse up/down/move event listener
 *
 */

class nsIMouseListener {

  public:

    /**
     * Processes a mouse pressed event
     * @param aMouseEvent See nsGUIEvent.h 
     * @return whether the event was consumed or ignored. See nsEventStatus
     */
    virtual nsEventStatus MousePressed(const nsGUIEvent & aMouseEvent) = 0;

    /**
     * Processes a mouse release event
     * @param aMouseEvent See nsGUIEvent.h 
     * @return whether the event was consumed or ignored. See nsEventStatus
     */
    virtual nsEventStatus MouseReleased(const nsGUIEvent & aMouseEvent) = 0;

    /**
     * Processes a mouse clicked event
     * @param aMouseEvent See nsGUIEvent.h 
     * @return whether the event was consumed or ignored. See nsEventStatus
     *
     */
    virtual nsEventStatus MouseClicked(const nsGUIEvent & aMouseEvent) = 0;

    /**
     * Processes a mouse moved event
     * @param aMouseEvent See nsGUIEvent.h 
     * @return whether the event was consumed or ignored. See nsEventStatus
     */
    virtual nsEventStatus MouseMoved(const nsGUIEvent & aMouseEvent) = 0;

};

#endif // nsIMouseListener_h__
