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

#ifndef nsIView_h___
#define nsIView_h___

#include <stdio.h>
#include "nsISupports.h"
#include "nsCoord.h"
#include "nsIWidget.h"
#include "nsGUIEvent.h"

class nsIViewManager;
class nsRegion;
class nsIRenderingContext;
class nsTransform2D;
class nsIFrame;
struct nsRect;

// Enumerated type to indicate the visibility of a layer.
// hide - the layer is not shown.
// show - the layer is shown irrespective of the visibility of 
//        the layer's parent.
// inherit - the layer inherits its visibility from its parent.
typedef enum
{
  nsViewVisibility_kHide = 0,
  nsViewVisibility_kShow = 1,
  nsViewVisibility_kInherit = 2
} nsViewVisibility;

// IID for the nsIView interface
#define NS_IVIEW_IID    \
{ 0xf0a21c40, 0xa7e1, 0x11d1, \
{ 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

//----------------------------------------------------------------------

// View interface

// Note that nsIView does not support reference counting; view object
// have their lifetime bound to the view manager that contains them.
class nsIView : public nsISupports
{
public:
  virtual nsresult Init(nsIViewManager* aManager,
						const nsRect &aBounds,
						nsIView *aParent,
						const nsIID *aWindowIID = nsnull,
						nsNativeWindow aNative = nsnull,
						PRInt32 aZIndex = 0,
						const nsRect *aClipRect = nsnull,
						float aOpacity = 1.0f,
						nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow) = 0;

  virtual void Destroy() = 0;

  virtual nsIViewManager * GetViewManager() = 0;

  // In 4.0, the "cutout" nature of a layer is queryable.
  // If we believe that all cutout layers have a native widget, this
  // could be a replacement.
  virtual nsIWidget * GetWidget() = 0;

  // Called to indicate that the specified region of the layer
  // needs to be drawn via the rendering context. The region
  // is specified in layer coordinates.
  virtual void Paint(nsIRenderingContext& rc, const nsRect& rect) = 0;
  virtual void Paint(nsIRenderingContext& rc, const nsRegion& region) = 0;
  
  // Called to indicate that the specified event should be handled
  // by the layer. This method should return nsEventStatus_eConsumeDoDefault or nsEventStatus_eConsumeNoDefault if the event has been
  // handled.
  virtual nsEventStatus HandleEvent(nsGUIEvent *event, PRBool aCheckParent = PR_TRUE, PRBool aCheckChildren = PR_TRUE) = 0;

  // Called to indicate that the position of the layer has been changed.
  // The specified coordinates are in the parent layer's coordinate space.
  virtual void SetPosition(nscoord x, nscoord y) = 0;
  virtual void GetPosition(nscoord *x, nscoord *y) = 0;
  
  // Called to indicate that the dimensions of the layer (actually the
  // width and height of the clip) have been changed. 
  virtual void SetDimensions(nscoord width, nscoord height) = 0;
  virtual void GetDimensions(nscoord *width, nscoord *height) = 0;

  // Called to indicate that the dimensions of the layer (actually the
  // width and height of the clip) have been changed. 
  virtual void SetBounds(const nsRect &aBounds) = 0;
  virtual void SetBounds(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) = 0;
  virtual void GetBounds(nsRect &aBounds) = 0;

  // Called to indicate that the clip of the layer has been changed.
  // The clip is relative to the origin of the layer.
  virtual void SetClip(const nsRect &aClip) = 0;
  virtual void SetClip(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) = 0;
  virtual PRBool GetClip(nsRect *aClip) = 0;

  // Called to indicate that the visibility of a layer has been
  // changed.
  virtual void SetVisibility(nsViewVisibility visibility) = 0;
  virtual nsViewVisibility GetVisibility() = 0;

  // Called to indicate that the z-index of a layer has been changed.
  // The z-index is relative to all siblings of the layer.
  virtual void SetZIndex(PRInt32 zindex) = 0;
  virtual PRInt32 GetZIndex() = 0;

  // Called to set the parent of the layer.
  virtual void SetParent(nsIView *aParent) = 0;
  virtual nsIView *GetParent() = 0;

  // Sibling pointer used to link together views
  virtual nsIView* GetNextSibling() const = 0;
  virtual void SetNextSibling(nsIView* aNextSibling) = 0;

  // Used to insert a child after the specified sibling. In general,
  // child insertion will happen through the layer manager and it
  // will determine the ordering of children in the child list.
  virtual void InsertChild(nsIView *child, nsIView *sibling) = 0;

  // Remove a child from the child list. Again, the removal will driven
  // through the layer manager.
  virtual void RemoveChild(nsIView *child) = 0;
  
  // Get the number of children for this layer.
  virtual PRInt32 GetChildCount() = 0;
  
  // Get a child at a specific index. Could be replaced by some sort of
  // enumeration API.
  virtual nsIView * GetChild(PRInt32 index) = 0;

  // Note: This didn't exist in 4.0. This transform might include scaling
  // but probably not rotation for the first pass.
  virtual void SetTransform(nsTransform2D *transform) = 0;
  virtual nsTransform2D * GetTransform() = 0;

  // Note: This didn't exist in 4.0. Called to set the opacity of a layer. 
  // A value of 0.0 means completely transparent. A value of 1.0 means
  // completely opaque.
  virtual void SetOpacity(float opacity) = 0;
  virtual float GetOpacity() = 0;

  // Used to ask a layer if it has any areas within its bounding box
  // that are transparent. This is not the same as opacity - opacity can
  // be set externally, transparency is a quality of the layer itself.
  // Returns true if there are transparent areas, false otherwise.
  virtual PRBool HasTransparency() = 0;

  //this is the link to the content world for this view
  virtual void SetFrame(nsIFrame *aFrame) = 0;
  virtual nsIFrame * GetFrame() = 0;

  //move child widgets around by (dx, dy). deltas are in widget
  //coordinate space.
  virtual void AdjustChildWidgets(nscoord aDx, nscoord aDy) = 0;

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;
};

#endif
