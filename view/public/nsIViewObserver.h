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

#ifndef nsIViewObserver_h___
#define nsIViewObserver_h___

#include "nsISupports.h"
#include "nsGUIEvent.h"

class nsIRenderingContext;
struct nsRect;

#define NS_IVIEWOBSERVER_IID   \
{ 0x6a1529e0, 0x3d2c, 0x11d2, \
{ 0xa8, 0x32, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

class nsIViewObserver : public nsISupports
{
public:
  
  static const nsIID& GetIID() { static nsIID iid = NS_IVIEWOBSERVER_IID; return iid; }

  /* called when the observer needs to paint
   * @param aRenderingContext - rendering context to paint to
   * @param aDirtyRect - rectangle of dirty area
   * @return error status
   */
  NS_IMETHOD Paint(nsIView *            aView,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect) = 0;

  /* called when the observer needs to handle an event
   * @param aEvent - event notification
   * @param aEventStatus - out parameter for event handling
   *                       status
   * @return error status
   */
  NS_IMETHOD HandleEvent(nsIView *       aView,
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus) = 0;

  /* called when the view has been repositioned due to scrolling
   * @return error status
   */
  NS_IMETHOD Scrolled(nsIView * aView) = 0;

  /* called when the view has been resized and the
   * content within the view needs to be reflowed.
   * @param aWidth - new width of view
   * @param aHeight - new height of view
   * @return error status
   */
  NS_IMETHOD ResizeReflow(nsIView * aView, nscoord aWidth, nscoord aHeight) = 0;
};

#endif
