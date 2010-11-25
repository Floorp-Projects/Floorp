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
#include "nsColor.h"
#include "nsRect.h"

class nsIRenderingContext;
class nsGUIEvent;

#define NS_IVIEWOBSERVER_IID  \
  { 0xc8ba5804, 0x2459, 0x4b62, \
    { 0xa4, 0x15, 0x02, 0x84, 0x1a, 0xd7, 0x93, 0xa7 } }

class nsIViewObserver : public nsISupports
{
public:
  
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IVIEWOBSERVER_IID)

  /* called when the observer needs to paint. This paints the entire
   * frame subtree rooted at aViewToPaint, including frame subtrees from
   * subdocuments.
   * @param aViewToPaint the view for the widget that is being painted
   * @param aWidgetToPaint the widget that is being painted, the widget of
   * aViewToPaint
   * @param aDirtyRegion the region to be painted, in appunits of aDisplayRoot
   * and relative to aDisplayRoot
   * @param aIntDirtyRegion the region to be painted, in dev pixels, in the
   * coordinates of aWidgetToPaint. This conveys the same information as
   * aDirtyRegion but in a different format.
   * @param aPaintDefaultBackground just paint the default background,
   * don't try to paint any content. This is set when the observer
   * needs to paint something, but the view tree is unstable, so it
   * must *not* paint, or even examine, the frame subtree rooted at the
   * view.  (It is, however, safe to inspect the state of the view itself,
   * and any associated widget.) The name illustrates the expected behavior,
   * which is to paint some default background color over the dirty region.
   * @return error status
   */
  NS_IMETHOD Paint(nsIView*           aDisplayRoot,
                   nsIView*           aViewToPaint,
                   nsIWidget*         aWidgetToPaint,
                   const nsRegion&    aDirtyRegion,
                   const nsIntRegion& aIntDirtyRegion,
                   PRBool             aPaintDefaultBackground,
                   PRBool             aWillSendDidPaint) = 0;

  /* called when the observer needs to handle an event
   * @param aView  - where to start processing the event; the root view,
   * or the view that's currently capturing this sort of event; must be a view
   * for this presshell
   * @param aEvent - event notification
   * @param aEventStatus - out parameter for event handling
   *                       status
   * @param aHandled - whether the correct frame was found to
   *                   handle the event
   * @return error status
   */
  NS_IMETHOD HandleEvent(nsIView*       aView,
                         nsGUIEvent*    aEvent,
                         PRBool         aDontRetargetEvents,
                         nsEventStatus* aEventStatus) = 0;

  /* called when the view has been resized and the
   * content within the view needs to be reflowed.
   * @param aWidth - new width of view
   * @param aHeight - new height of view
   * @return error status
   */
  NS_IMETHOD ResizeReflow(nsIView * aView, nscoord aWidth, nscoord aHeight) = 0;

  /**
   * Returns true if the view observer wants to drop all invalidation right now
   * because painting is suppressed. It will invalidate everything when it
   * unsuppresses.
   */
  NS_IMETHOD_(PRBool) ShouldIgnoreInvalidation() = 0;

  /**
   * Notify the observer that we're about to start painting.  This
   * gives the observer a chance to make some last-minute invalidates
   * and geometry changes if it wants to.
   */
  NS_IMETHOD_(void) WillPaint(PRBool aWillSendDidPaint) = 0;

  /**
   * Notify the observer that we finished painting.  This
   * gives the observer a chance to make some last-minute invalidates
   * and geometry changes if it wants to.
   */
  NS_IMETHOD_(void) DidPaint() = 0;

  /**
   * Dispatch the given synthesized mouse move event, and if
   * aFlushOnHoverChange is true, flush layout if :hover changes cause
   * any restyles.
   */
  NS_IMETHOD_(void) DispatchSynthMouseMove(nsGUIEvent *aEvent,
                                           PRBool aFlushOnHoverChange) = 0;

  /**
   * If something within aView is capturing the mouse, clear the capture.
   * if aView is null, clear the mouse capture no matter what is capturing it.
   */
  NS_IMETHOD_(void) ClearMouseCapture(nsIView* aView) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIViewObserver, NS_IVIEWOBSERVER_IID)

#endif
