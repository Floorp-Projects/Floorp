/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIView_h___
#define nsIView_h___

#include "nsISupports.h"
#include "nsCoord.h"
#include <stdio.h>

class nsIViewManager;
class nsIWidget;
struct nsWidgetInitData;
typedef void* nsNativeWidget;
struct nsRect;

// Enumerated type to indicate the visibility of a layer.
// hide - the layer is not shown.
// show - the layer is shown irrespective of the visibility of 
//        the layer's parent.
enum nsViewVisibility {
  nsViewVisibility_kHide = 0,
  nsViewVisibility_kShow = 1
};

// IID for the nsIView interface
#define NS_IVIEW_IID    \
{ 0xf0a21c40, 0xa7e1, 0x11d1, \
{ 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

//----------------------------------------------------------------------

/**
 * View interface
 *
 * Views are NOT reference counted. Use the Destroy() member function to
 * destroy a view.
 *
 * The lifetime of the view hierarchy is bounded by the lifetime of the
 * view manager that owns the views.
 *
 * Most of the methods here are read-only. To set the corresponding properties
 * of a view, go through nsIViewManager.
 */
class nsIView : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IVIEW_IID)

  /**
   * Initialize the view
   * @param aManager view manager that "owns" the view. The view does NOT
   *        hold a reference to the view manager
   * @param aBounds initial bounds for view
   * @param aParent intended parent for view. this is not actually set in the
   *        nsIView through this method. it is only used by the initialization
   *        code to walk up the view tree, if necessary, to find resources.
   * @param aVisibilityFlag initial visibility state of view
   * @result The result of the initialization, NS_OK if no errors
   */
  NS_IMETHOD  Init(nsIViewManager* aManager,
						       const nsRect &aBounds,
                   const nsIView *aParent,
        					 nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow) = 0;

  /**
   * Destroy the view.
   *
   * The view destroys its child views, and destroys and releases its
   * widget (if it has one).
   *
   * Also informs the view manager that the view is destroyed by calling
   * SetRootView(NULL) if the view is the root view and calling RemoveChild()
   * otherwise.
   */
  NS_IMETHOD  Destroy() = 0;

  /**
   * Get the view manager which "owns" the view.
   * This method might require some expensive traversal work in the future. If you can get the
   * view manager from somewhere else, do that instead.
   * @result the view manager
   */
  NS_IMETHOD  GetViewManager(nsIViewManager *&aViewMgr) const = 0;

  /**
   * Called to get the position of a view.
   * The specified coordinates are in the parent view's coordinate space.
   * This is the (0, 0) origin of the coordinate space established by this view.
   * @param x out parameter for x position
   * @param y out parameter for y position
   */
  NS_IMETHOD  GetPosition(nscoord *aX, nscoord *aY) const = 0;
  
  /**
   * Called to get the dimensions and position of the view's bounds.
   * The view's bounds (x,y) are in the coordinate space of the parent view.
   * The view's bounds (x,y) might not be the same as the view's position,
   * if the view has content above or to the left of its origin.
   * @param aBounds out parameter for bounds
   */
  NS_IMETHOD  GetBounds(nsRect &aBounds) const = 0;

  /**
   * Called to query the visibility state of a view.
   * @result current visibility state
   */
  NS_IMETHOD  GetVisibility(nsViewVisibility &aVisibility) const = 0;

  /**
   * Called to query the z-index of a view.
   * The z-index is relative to all siblings of the view.
   * @param aAuto  PR_TRUE if the view is zindex:auto
   * @param aZIndex explicit z-index value. 
   * @param aTopMost used when this view is zindex:auto
   *        PR_TRUE if the view is topmost when compared
   *        with another z-index:auto view
   *        
   */
  NS_IMETHOD  GetZIndex(PRBool &aAuto, PRInt32 &aZIndex, PRBool &aTopMost) const = 0;

  /**
   * Get whether the view "floats" above all other views,
   * which tells the compositor not to consider higher views in
   * the view hierarchy that would geometrically intersect with
   * this view. This is a hack, but it fixes some problems with
   * views that need to be drawn in front of all other views.
   * @result PR_TRUE if the view floats, PR_FALSE otherwise.
   */
  NS_IMETHOD  GetFloating(PRBool &aFloatingView) const = 0;

  /**
   * Called to query the parent of the view.
   * @result view's parent
   */
  NS_IMETHOD  GetParent(nsIView *&aParent) const = 0;

  /**
   * The view's first child is the child which is earliest in document order.
   * @result first child
   */
  NS_IMETHOD  GetFirstChild(nsIView* &aChild) const = 0;

  /**
   * Called to query the next sibling of the view.
   * @result view's next sibling
   */
  NS_IMETHOD  GetNextSibling(nsIView *&aNextSibling) const = 0;

  /**
   * Note: This didn't exist in 4.0. Called to get the opacity of a view. 
   * A value of 0.0 means completely transparent. A value of 1.0 means
   * completely opaque.
   * @result view's opacity value
   */
  NS_IMETHOD  GetOpacity(float &aOpacity) const = 0;

  /**
   * Used to ask a view if it has any areas within its bounding box
   * that are transparent. This is not the same as opacity - opacity can
   * be set externally, transparency is a quality of the view itself.
   * @result Returns PR_TRUE if there are transparent areas, PR_FALSE otherwise.
   */
  NS_IMETHOD  HasTransparency(PRBool &aTransparent) const = 0;

  /**
   * Set the view's link to client owned data.
   * @param aData - data to associate with view. nsnull to disassociate
   */
  NS_IMETHOD  SetClientData(void *aData) = 0;

  /**
   * Query the view for it's link to client owned data.
   * @result data associated with view or nsnull if there is none.
   */
  NS_IMETHOD  GetClientData(void *&aData) const = 0;

  /**
   * Get the nearest widget in this view or a parent of this view and
   * the offset from the view that contains the widget to this view
   * @param aDx out parameter for x offset
   * @param aDy out parameter for y offset
   * @return widget (if there is one) closest to view
   */
  NS_IMETHOD  GetOffsetFromWidget(nscoord *aDx, nscoord *aDy, nsIWidget *&aWidget) = 0;

  /**
   * Create a widget to associate with this view. This is a helper
   * function for SetWidget.
   * @param aWindowIID IID for Widget type that this view
   *        should have associated with it. if nsull, then no
   *        width will be created for this view
   * @param aWidgetInitData data used to initialize this view's widget before
   *        its create is called.
   * @param aNative native window that will be used as parent of
   *        aWindowIID. if nsnull, then parent will be derived from
   *        parent view and it's ancestors
   * @return error status
   */
  NS_IMETHOD CreateWidget(const nsIID &aWindowIID,
                          nsWidgetInitData *aWidgetInitData = nsnull,
        					        nsNativeWidget aNative = nsnull,
                          PRBool aEnableDragDrop = PR_TRUE,
                          PRBool aResetVisibility = PR_TRUE) = 0;

  /**
   * In 4.0, the "cutout" nature of a view is queryable.
   * If we believe that all cutout view have a native widget, this
   * could be a replacement.
   * @param aWidget out parameter for widget that this view contains,
   *        or nsnull if there is none.
   */
  NS_IMETHOD GetWidget(nsIWidget *&aWidget) const = 0;

  /**
   * Returns PR_TRUE if the view has a widget associated with it.
   * @param aHasWidget out parameter that indicates whether a view has a widget.
   */
  NS_IMETHOD HasWidget(PRBool *aHasWidget) const = 0;

  // XXX Temporary for Bug #19416
  NS_IMETHOD IgnoreSetPosition(PRBool aShouldIgnore) = 0;

  /**
   * Output debug info to FILE
   * @param out output file handle
   * @param aIndent indentation depth
   */
  NS_IMETHOD  List(FILE* out, PRInt32 aIndent = 0) const = 0;

private:
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;
};

#endif
