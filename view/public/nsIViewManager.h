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

#ifndef nsIViewManager_h___
#define nsIViewManager_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsCoord.h"
#include "nsIView.h"
class nsRegion;
class nsIEvent;
class nsIPresContext;
class nsIView;
class nsIWidget;
struct nsRect;

#define NS_IVIEWMANAGER_IID   \
{ 0x3a8863d0, 0xa7f3, 0x11d1, \
  { 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

class nsIViewManager : public nsISupports
{
public:
  // Note: this instance does not hold a reference to the presentation
  // context because it holds a reference to this instance.
  virtual nsresult Init(nsIPresContext* aPresContext) = 0;

  // Get the window for which the manager is responsible.
  virtual void SetRootWindow(nsIWidget *aRootWindow) = 0;
  virtual nsIWidget *GetRootWindow() = 0;

  // Get and set the root of the layer tree.
  virtual nsIView *GetRootView() = 0;
  virtual void SetRootView(nsIView *aView) = 0;

  // Get and set the current framerate i.e. the rate at which timed
  // refreshes occur. A framerate of 0 indicates that timed refreshes
  // should not occur. framerate is in terms of frames per second
  virtual PRUint32 GetFrameRate() = 0;
  virtual nsresult SetFrameRate(PRUint32 frameRate) = 0;

  // Get and set the dimensions of the root window. The latter should
  // be called if the root window is resized.. The dimensions are in
  // twips
  virtual void GetWindowDimensions(nscoord *width, nscoord *height) = 0;
  virtual void SetWindowDimensions(nscoord width, nscoord height) = 0;

  // Get and set the position of the root window relative to the
  // composited area. The latter should be called if the root window
  // is scrolled.
  virtual void GetWindowOffsets(nscoord *xoffset, nscoord *yoffset) = 0;
  virtual void SetWindowOffsets(nscoord xoffset, nscoord yoffset) = 0;

  // Called to refresh an area of the root window. The coordinates of
  // the region or rectangle passed in should be in the window's
  // coordinate space (pixels). Often called in response to a
  // paint/redraw event from the native windowing system.
  virtual void Refresh(nsIRenderingContext *aContext, nsRegion *region,
                       PRUint32 aUpdateFlags) = 0;
  virtual void Refresh(nsIView* aView, nsIRenderingContext *aContext,
                       nsRect *rect, PRUint32 aUpdateFlags) = 0;

  // Called to force a redrawing of any dirty areas.
  virtual void Composite() = 0;

  // Called to inform the layer manager that some portion of a layer
  // is dirty and needs to be redrawn. The region or rect passed in
  // should be in the layer's coordinate space.
  virtual void UpdateView(nsIView *aView, nsRegion *region,
                          PRUint32 aUpdateFlags) = 0;
  virtual void UpdateView(nsIView *aView, nsRect *rect, PRUint32 aUpdateFlags) = 0;

  // Called to dispatch an event to the appropriate layer. Often called
  // as a result of receiving a mouse or keyboard event from the native
  // event system.
  virtual PRBool DispatchEvent(nsIEvent *event) = 0;

  // Used to grab/capture all events of the type (mouse or keyboard) for
  // a specific layer, irrespective of the cursor position at which the
  // event occurred.
  virtual PRBool GrabMouseEvents(nsIView *aView) = 0;
  virtual PRBool GrabKeyEvents(nsIView *aView) = 0;

  // Get the current layer, if any, that's capturing a specific type of
  // event.
  virtual nsIView* GetMouseEventGrabber() = 0;
  virtual nsIView* GetKeyEventGrabber() = 0;

  // Given a parent layer, insert another layer as its child. If above
  // is nstrue, the layer is inserted above (in z-order) the sibling. If
  // it is nsfalse, the layer is inserted below.
  // The layer manager generates the appopriate dirty regions.
  virtual void InsertChild(nsIView *parent, nsIView *child, nsIView *sibling,
                           PRBool above) = 0;

  // Given a parent layer, insert another layer as its child. The zindex
  // indicates where the child should be inserted relative to other
  // children of the parent.
  // The layer manager generates the appopriate dirty regions.
  virtual void InsertChild(nsIView *parent, nsIView *child,
                           PRInt32 zindex) = 0;

  // Remove a specific child of a layer.
  // The layer manager generates the appopriate dirty regions.
  virtual void RemoveChild(nsIView *parent, nsIView *child) = 0;

  // Move a layer's position by the specified amount.
  // The layer manager generates the appopriate dirty regions.
  virtual void MoveViewBy(nsIView *aView, nscoord x, nscoord y) = 0;

  // Move a layer to the specified position, provided in parent coordinates.
  // The layer manager generates the appopriate dirty regions.
  virtual void MoveViewTo(nsIView *aView, nscoord x, nscoord y) = 0;

  // Resize a layer to the specified width and height.
  // The layer manager generates the appopriate dirty regions.
  virtual void ResizeView(nsIView *aView, nscoord width, nscoord height) = 0;

  // Set the clip of a layer.
  // The layer manager generates the appopriate dirty regions.
  virtual void SetViewClip(nsIView *aView, nsRect *rect) = 0;

  // Set the visibility of a layer.
  // The layer manager generates the appopriate dirty regions.
  virtual void SetViewVisibility(nsIView *aView, nsViewVisibility visible) = 0;

  // Set the z-index of a layer. Positive z-indices mean that a layer
  // is above its parent in z-order. Negative z-indices mean that a
  // layer is below its parent.
  // The layer manager generates the appopriate dirty regions.
  virtual void SetViewZindex(nsIView *aView, PRInt32 zindex) = 0;

  // Used to move a layer above or below another in z-order.
  // The layer manager generates the appopriate dirty regions.
  virtual void MoveViewAbove(nsIView *aView, nsIView *other) = 0;
  virtual void MoveViewBelow(nsIView *aView, nsIView *other) = 0;

  // Returns whether a layer is actually shown (based on its visibility
  // and that of its ancestors).
  virtual PRBool IsViewShown(nsIView *aView) = 0;

  // Returns the clipping area of layer in absolute coordinates.
  virtual PRBool GetViewClipAbsolute(nsIView *aView, nsRect *rect) = 0;

  // Get the presentation context associated with this manager
  virtual nsIPresContext* GetPresContext() = 0;

  // Set the area that the view manager considers to be "dirty"
  // to an empty state
  virtual void ClearDirtyRegion() = 0;
};

//when the refresh happens, should it be double buffered?
#define NS_VMREFRESH_DOUBLE_BUFFER  0x0001
//is the damagerect in top-level window space?
#define NS_VMREFRESH_SCREEN_RECT    0x0002
//update view now?
#define NS_VMREFRESH_IMMEDIATE      0x0004

#endif  // nsIViewManager_h___
