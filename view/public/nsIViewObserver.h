/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIViewObserver_h___
#define nsIViewObserver_h___

#include "nsISupports.h"
#include "nsEvent.h"

class nsIRenderingContext;
struct nsRect;
struct nsGUIEvent;

#define NS_IVIEWOBSERVER_IID   \
{ 0x6a1529e0, 0x3d2c, 0x11d2, \
{ 0xa8, 0x32, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

class nsIViewObserver : public nsISupports
{
public:
  
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IVIEWOBSERVER_IID)

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
   * @param aHandled - whether the correct frame was found to
   *                   handle the event
   * @return error status
   */
  NS_IMETHOD HandleEvent(nsIView *       aView,
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus,
                         PRBool          aForceHandle,
                         PRBool&         aHandled) = 0;

  /* called when the view has been resized and the
   * content within the view needs to be reflowed.
   * @param aWidth - new width of view
   * @param aHeight - new height of view
   * @return error status
   */
  NS_IMETHOD ResizeReflow(nsIView * aView, nscoord aWidth, nscoord aHeight) = 0;
};

#endif
