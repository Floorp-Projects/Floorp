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
class nsIRegion;
class nsIRenderingContext;
class nsTransform2D;
class nsIFrame;
struct nsRect;

//this is used by the view clipping APIs since the description of
//a clip rect is different than a rect

typedef struct
{
  nscoord mLeft;
  nscoord mRight;
  nscoord mTop;
  nscoord mBottom;
} nsViewClip;

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

class nsIView : public nsISupports
{
public:
  /**
   * Initialize the view
   * @param aManager view manager that "owns" the view
   * @param aBounds initial bounds for view
   * @param aParent parent view
   * @param aWindowIID IID for Widget type that this view
   *        should have associated with it. if nsull, then no
   *        width will be created for this view
   * @param aWidgetInitData data used to initialize this view's widget before
   *        its create is called.
   * @param aNative native window that will be used as parent of
   *        aWindowIID. if nsnull, then parent will be derived from
   *        parent view and it's ancestors
   * @param aZIndex initial z depth of view
   * @param aCilpRect initial clip rect of view
   * @param aOpacity initial opacity of view
   * @param aVisibilityFlag initial visibility state of view
   * @result The result of the initialization, NS_OK if no errors
   */
  virtual nsresult Init(nsIViewManager* aManager,
						const nsRect &aBounds,
						nsIView *aParent,
						const nsIID *aWindowIID = nsnull,
            nsWidgetInitData *aWidgetInitData = nsnull,
						nsNativeWindow aNative = nsnull,
						PRInt32 aZIndex = 0,
						const nsViewClip *aClip = nsnull,
						float aOpacity = 1.0f,
						nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow) = 0;

  /**
   * Destroy the view
   */
  virtual void Destroy() = 0;

  /**
   * Get the view manager the "owns" the view
   * @result view manager
   */
  virtual nsIViewManager * GetViewManager() = 0;

  /**
   * In 4.0, the "cutout" nature of a view is queryable.
   * If we believe that all cutout view have a native widget, this
   * could be a replacement.
   * @result widget that this view contains
   */
  virtual nsIWidget * GetWidget() = 0;

  /**
   * Called to indicate that the specified rect of the view
   * needs to be drawn via the rendering context. The rect
   * is specified in view coordinates.
   * @param rc rendering context to paint into
   * @param rect damage area
   * @param aPaintFlags see nsIView.h for flag definitions
   * @param aBackstop if we will need to do back to front
   *        painting, this is the view that, once rendered
   *        ends the back to front pass.
   * @return PR_TRUE if the entire clip region has been eliminated, else PR_FALSE
   */
  virtual PRBool Paint(nsIRenderingContext& rc, const nsRect& rect,
                       PRUint32 aPaintFlags, nsIView *aBackstop = nsnull) = 0;

  /**
   * Called to indicate that the specified region of the view
   * needs to be drawn via the rendering context. The region
   * is specified in view coordinates.
   * @param rc rendering context to paint into
   * @param region damage area
   * @param aPaintFlags see nsIView.h for flag definitions
   * @return PR_TRUE if the entire clip region has been eliminated, else PR_FALSE
   */
  virtual PRBool Paint(nsIRenderingContext& rc, const nsIRegion& region, PRUint32 aPaintFlags) = 0;
  
  /**
   * Called to indicate that the specified event should be handled
   * by the view. This method should return nsEventStatus_eConsumeDoDefault
   * or nsEventStatus_eConsumeNoDefault if the event has been handled.
   * @param event event to process
   * @param aEventFlags see nsIView.h for flag definitions
   * @result processing status
   */
  virtual nsEventStatus HandleEvent(nsGUIEvent *event, PRUint32 aEventFlags) = 0;

  /**
   * Called to indicate that the position of the view has been changed.
   * The specified coordinates are in the parent view's coordinate space.
   * @param x new x position
   * @param y new y position
   */
  virtual void SetPosition(nscoord x, nscoord y) = 0;

  /**
   * Called to get the position of a view.
   * The specified coordinates are in the parent view's coordinate space.
   * @param x out parameter for x position
   * @param y out parameter for y position
   */
  virtual void GetPosition(nscoord *x, nscoord *y) = 0;
  
  /**
   * Called to indicate that the dimensions of the view (actually the
   * width and height of the clip) have been changed. 
   * @param width new width
   * @param height new height
   */
  virtual void SetDimensions(nscoord width, nscoord height) = 0;
  virtual void GetDimensions(nscoord *width, nscoord *height) = 0;

  /**
   * Called to indicate that the dimensions and position of the view have
   * been changed.
   * @param aBounds new bounds
   */
  virtual void SetBounds(const nsRect &aBounds) = 0;

  /**
   * Called to indicate that the dimensions and position of the view have
   * been changed.
   * @param aX new x position
   * @param aY new y position
   * @param aWidth new width
   * @param aHeight new height
   */
  virtual void SetBounds(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) = 0;

  /**
   * Called to get the dimensions and position of the view.
   * @param aBounds out parameter for bounds
   */
  virtual void GetBounds(nsRect &aBounds) = 0;

  /**
   * Called to indicate that the clip of the view has been changed.
   * The clip is relative to the origin of the view.
   * @param aLeft new left position
   * @param aTop new top position
   * @param aRight new right position
   * @param aBottom new bottom position
   */
  virtual void SetClip(nscoord aLeft, nscoord aTop, nscoord aRight, nscoord aBottom) = 0;

  /**
   * Called to get the dimensions and position of the clip for the view.
   * @param aLeft left position
   * @param aTop top position
   * @param aRight right position
   * @param aBottom bottom position
   * @result PR_TRUE of there actually is a clip for the view, else PR_FALSE
   */
  virtual PRBool GetClip(nscoord *aLeft, nscoord *aTop, nscoord *aRight, nscoord *aBottom) = 0;

  /**
   * Called to indicate that the visibility of a view has been
   * changed.
   * @param visibility new visibility state
   */
  virtual void SetVisibility(nsViewVisibility visibility) = 0;

  /**
   * Called to query the visibility state of a view.
   * @result current visibility state
   */
  virtual nsViewVisibility GetVisibility() = 0;

  /**
   * Called to indicate that the z-index of a view has been changed.
   * The z-index is relative to all siblings of the view.
   * @param zindex new z depth
   */
  virtual void SetZIndex(PRInt32 zindex) = 0;

  /**
   * Called to query the z-index of a view.
   * The z-index is relative to all siblings of the view.
   * @result current z depth
   */
  virtual PRInt32 GetZIndex() = 0;

  /**
   * Called to set the parent of the view.
   * @param aParent new parent
   */
  virtual void SetParent(nsIView *aParent) = 0;

  /**
   * Called to query the parent of the view.
   * @result view's parent
   */
  virtual nsIView *GetParent() = 0;

  /**
   * Called to query the next sibling of the view.
   * @result view's next sibling
   */
  virtual nsIView* GetNextSibling() const = 0;

  /**
   * Called to set the next sibling of the view.
   * @param aNextSibling new next sibling
   */
  virtual void SetNextSibling(nsIView* aNextSibling) = 0;

  /**
   * Used to insert a child after the specified sibling. In general,
   * child insertion will happen through the view manager and it
   * will determine the ordering of children in the child list.
   * @param child to insert in this view's child list
   * @param sibling view to set as previous sibling of child
   *                if nsnull, then child is inserted at head of list
   */
  virtual void InsertChild(nsIView *child, nsIView *sibling) = 0;

  /**
   * Remove a child from the child list. The removal will be driven
   * through the view manager.
   * @param child to remove
   */
  virtual void RemoveChild(nsIView *child) = 0;
  
  /**
   * Get the number of children for this view.
   * @result child count
   */
  virtual PRInt32 GetChildCount() = 0;
  
  /**
   * Get a child at a specific index. Could be replaced by some sort of
   * enumeration API.
   * @param index of desired child view
   * @result the view at index or nsnull if there is no such child
   */
  virtual nsIView * GetChild(PRInt32 index) = 0;

  /**
   * Note: This didn't exist in 4.0. This transform might include scaling
   * but probably not rotation for the first pass.
   * @param transform new transformation of view
   */
  virtual void SetTransform(nsTransform2D &aXForm) = 0;

  /**
   * Note: This didn't exist in 4.0. This transform might include scaling
   * but probably not rotation for the first pass.
   * @result view's transformation
   */
  virtual void GetTransform(nsTransform2D &aXForm) = 0;

  /**
   * Note: This didn't exist in 4.0. Called to set the opacity of a view. 
   * A value of 0.0 means completely transparent. A value of 1.0 means
   * completely opaque.
   * @param opacity new opacity value
   */
  virtual void SetOpacity(float opacity) = 0;

  /**
   * Note: This didn't exist in 4.0. Called to set the opacity of a view. 
   * A value of 0.0 means completely transparent. A value of 1.0 means
   * completely opaque.
   * @result view's opacity value
   */
  virtual float GetOpacity() = 0;

  /**
   * Used to ask a view if it has any areas within its bounding box
   * that are transparent. This is not the same as opacity - opacity can
   * be set externally, transparency is a quality of the view itself.
   * @result Returns PR_TRUE if there are transparent areas, PR_FALSE otherwise.
   */
  virtual PRBool HasTransparency() = 0;

  /**
   * Used set the transparency status of the content in a view. see
   * HasTransparency().
   * @param aTransparent PR_TRUE if there are transparent areas, PR_FALSE otherwise.
   */
  virtual void SetContentTransparency(PRBool aTransparent) = 0;

  /**
   * Set the view's link to the nsIFrame part of the universe.
   * @param aFrame frame to associate with view. nsnull to disassociate
   */
  virtual void SetFrame(nsIFrame *aFrame) = 0;

  /**
   * Query the view for it's link to the nsIFrame part of the universe.
   * @result frame associated with view or nsnull if there is none.
   */
  virtual nsIFrame * GetFrame() = 0;

  /**
   * Get the nearest widget in this view or a parent of this view and
   * the offset from the view that contains the widget to this view
   * @param aDx out parameter for x offset
   * @param aDy out parameter for y offset
   * @return widget (if there is one) closest to view
   */
  virtual nsIWidget * GetOffsetFromWidget(nscoord *aDx, nscoord *aDy) = 0;

  /**
   * Get the visible offset of scrollable view (if any) that contains this view
   * @param aDx out parameter for x offset
   * @param aDy out parameter for y offset
   */
  virtual void GetScrollOffset(nscoord *aDx, nscoord *aDy) = 0;

  /**
   * Output debug info to FILE
   * @param out output file handle
   * @param aIndent indentation depth
   */
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;
};

//this is passed down to child views during painting and event handling
//so that a child can determine if it is hidden or shown when it's
//visibility state is set to inherit
#define NS_VIEW_FLAG_PARENT_HIDDEN  0x0001

//when painting, if we have determined that we need to do a combination
//of front to back and back to front painting, this flag will be set
//while in the back to front pass
#define NS_VIEW_FLAG_BACK_TO_FRONT  0x0002

//during event propagation, see if parent views can handle the event
#define NS_VIEW_FLAG_CHECK_PARENT   0x0004

//during event propagation, see if child views can handle the event
#define NS_VIEW_FLAG_CHECK_CHILDREN 0x0008

//during event propagation, see if sibling views can handle the event
#define NS_VIEW_FLAG_CHECK_SIBLINGS 0x0010

//passed down through the class hierarchy
//to indicate that the clip is set by an
//outer class
#define NS_VIEW_FLAG_CLIP_SET       0x0020

#endif
