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

#ifndef nsViewManager_h___
#define nsViewManager_h___

#include "nsIViewManager.h"
#include "nsCRT.h"
#include "nsIPresContext.h"
#include "nsIWidget.h"
#include "nsITimer.h"

class nsViewManager : public nsIViewManager
{
public:
  nsViewManager();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  virtual nsresult Init(nsIPresContext* aPresContext);

  // Get the window for which the manager is responsible.
  virtual void SetRootWindow(nsIWidget *aRootWindow);
  virtual nsIWidget *GetRootWindow();

  // Get and set the root of the layer tree.
  virtual nsIView *GetRootView();
  virtual void SetRootView(nsIView *aView);

  // Get and set the current framerate i.e. the rate at which timed
  // refreshes occur. A framerate of 0 indicates that timed refreshes
  // should not occur.
  virtual PRUint32 GetFrameRate();
  virtual nsresult SetFrameRate(PRUint32 frameRate);

  // Get and set the dimensions of the root window. The latter should
  // be called if the root window is resized.. The dimensions are in
  // twips
  virtual void GetWindowDimensions(nscoord *width, nscoord *height);
  virtual void SetWindowDimensions(nscoord width, nscoord height);

  // Get and set the position of the root window relative to the
  // composited area. The latter should be called if the root window
  // is scrolled.
  virtual void GetWindowOffsets(nscoord *xoffset, nscoord *yoffset);
  virtual void SetWindowOffsets(nscoord xoffset, nscoord yoffset);

  // Called to refresh an area of the root window. The coordinates of
  // the region or rectangle passed in should be in the window's
  // coordinate space. Often called in response to a paint/redraw event
  // from the native windowing system.
  virtual void Refresh(nsIRenderingContext *aContext, nsRegion *region,
                       PRUint32 aUpdateFlags);
  virtual void Refresh(nsIView* aView, nsIRenderingContext *aContext,
                       nsRect *rect, PRUint32 aUpdateFlags);

  // Called to force a redrawing of any dirty areas.
  virtual void Composite();

  // Called to inform the layer manager that some portion of a layer
  // is dirty and needs to be redrawn. The region or rect passed in
  // should be in the layer's coordinate space.
  virtual void UpdateView(nsIView *aView, nsRegion *aRegion,
                          PRUint32 aUpdateFlags);
  virtual void UpdateView(nsIView *aView, nsRect &aRect, PRUint32 aUpdateFlags);

  // Called to dispatch an event to the appropriate layer. Often called
  // as a result of receiving a mouse or keyboard event from the native
  // event system.
  virtual PRBool DispatchEvent(nsIEvent *event);

  // Used to grab/capture all events of the type (mouse or keyboard) for
  // a specific layer, irrespective of the cursor position at which the
  // event occurred.
  virtual PRBool GrabMouseEvents(nsIView *aView);
  virtual PRBool GrabKeyEvents(nsIView *aView);

  // Get the current layer, if any, that's capturing a specific type of
  // event.
  virtual nsIView* GetMouseEventGrabber();
  virtual nsIView* GetKeyEventGrabber();

  // Given a parent layer, insert another layer as its child. If above
  // is PR_TRUE, the layer is inserted above (in z-order) the sibling. If
  // it is nsfalse, the layer is inserted below.
  // The layer manager generates the appopriate dirty regions.
  virtual void InsertChild(nsIView *parent, nsIView *child, nsIView *sibling,
                           PRBool above);

  // Given a parent layer, insert another layer as its child. The zindex
  // indicates where the child should be inserted relative to other
  // children of the parent.
  // The layer manager generates the appopriate dirty regions.
  virtual void InsertChild(nsIView *parent, nsIView *child,
                           PRInt32 zindex);

  // Remove a specific child of a layer.
  // The layer manager generates the appopriate dirty regions.
  virtual void RemoveChild(nsIView *parent, nsIView *child);

  // Move a layer's position by the specified amount.
  // The layer manager generates the appopriate dirty regions.
  virtual void MoveViewBy(nsIView *aView, nscoord x, nscoord y);

  // Move a layer to the specified position, provided in parent coordinates.
  // The layer manager generates the appopriate dirty regions.
  virtual void MoveViewTo(nsIView *aView, nscoord x, nscoord y);

  // Resize a layer to the specified width and height.
  // The layer manager generates the appopriate dirty regions.
  virtual void ResizeView(nsIView *aView, nscoord width, nscoord height);

  // Set the clip of a layer.
  // The layer manager generates the appopriate dirty regions.
  virtual void SetViewClip(nsIView *aView, nsRect *rect);

  // Set the visibility of a layer.
  // The layer manager generates the appopriate dirty regions.
  virtual void SetViewVisibility(nsIView *aView, nsViewVisibility visible);

  // Set the z-index of a layer. Positive z-indices mean that a layer
  // is above its parent in z-order. Negative z-indices mean that a
  // layer is below its parent.
  // The layer manager generates the appopriate dirty regions.
  virtual void SetViewZindex(nsIView *aView, PRInt32 zindex);

  // Used to move a layer above or below another in z-order.
  // The layer manager generates the appopriate dirty regions.
  virtual void MoveViewAbove(nsIView *aView, nsIView *other);
  virtual void MoveViewBelow(nsIView *aView, nsIView *other);

  // Returns whether a layer is actually shown (based on its visibility
  // and that of its ancestors).
  virtual PRBool IsViewShown(nsIView *aView);

  // Returns the clipping area of layer in absolute coordinates.
  virtual PRBool GetViewClipAbsolute(nsIView *aView, nsRect *rect);

  // Get the presentation context associated with this manager
  virtual nsIPresContext* GetPresContext();

  virtual void ClearDirtyRegion();

  nsDrawingSurface GetDrawingSurface(nsIRenderingContext &aContext, nsRect& aBounds);

private:
  ~nsViewManager();
  nsIRenderingContext *CreateRenderingContext(nsIView &aView);

  nsIPresContext    *mContext;
  nsIWidget         *mRootWindow;
  nsPoint           mOffset;
  nsRect            mDSBounds;
  nsDrawingSurface  mDrawingSurface;
                            
public:
  //these are public so that our timer callback can poke them.
  nsITimer          *mTimer;
  nsRect            mDirtyRect;
  nsIView           *mRootView;
  PRUint32           mFrameRate;
};

#endif

