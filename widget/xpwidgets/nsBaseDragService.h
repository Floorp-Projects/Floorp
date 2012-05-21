/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseDragService_h__
#define nsBaseDragService_h__

#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsITransferable.h"
#include "nsISupportsArray.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDataTransfer.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsPoint.h"

#include "gfxImageSurface.h"

// translucency level for drag images
#define DRAG_TRANSLUCENCY 0.65

class nsIDOMNode;
class nsIFrame;
class nsPresContext;
class nsIImageLoadingContent;
class nsICanvasElementExternal;

/**
 * XP DragService wrapper base class
 */

class nsBaseDragService : public nsIDragService,
                          public nsIDragSession
{

public:
  nsBaseDragService();
  virtual ~nsBaseDragService();

  //nsISupports
  NS_DECL_ISUPPORTS

  //nsIDragSession and nsIDragService
  NS_DECL_NSIDRAGSERVICE
  NS_DECL_NSIDRAGSESSION

  void SetDragEndPoint(nsIntPoint aEndDragPoint) { mEndDragPoint = aEndDragPoint; }

  PRUint16 GetInputSource() { return mInputSource; }

protected:

  /**
   * Draw the drag image, if any, to a surface and return it. The drag image
   * is constructed from mImage if specified, or aDOMNode if mImage is null.
   *
   * aRegion may be used to draw only a subset of the element. This region
   * should be supplied using x and y coordinates measured in css pixels
   * that are relative to the upper-left corner of the window.
   *
   * aScreenX and aScreenY should be the screen coordinates of the mouse click
   * for the drag.
   *
   * On return, aScreenDragRect will contain the screen coordinates of the
   * area being dragged. This is used by the platform-specific part of the
   * drag service to determine the drag feedback.
   *
   * If there is no drag image, the returned surface will be null, but
   * aScreenDragRect will still be set to the drag area.
   *
   * aPresContext will be set to the nsPresContext used determined from
   * whichever of mImage or aDOMNode is used.
   */
  nsresult DrawDrag(nsIDOMNode* aDOMNode,
                    nsIScriptableRegion* aRegion,
                    PRInt32 aScreenX, PRInt32 aScreenY,
                    nsIntRect* aScreenDragRect,
                    gfxASurface** aSurface,
                    nsPresContext **aPresContext);

  /**
   * Draw a drag image for an image node specified by aImageLoader or aCanvas.
   * This is called by DrawDrag.
   */
  nsresult DrawDragForImage(nsPresContext* aPresContext,
                            nsIImageLoadingContent* aImageLoader,
                            nsICanvasElementExternal* aCanvas,
                            PRInt32 aScreenX, PRInt32 aScreenY,
                            nsIntRect* aScreenDragRect,
                            gfxASurface** aSurface);

  /**
   * Convert aScreenX and aScreenY from CSS pixels into unscaled device pixels.
   */
  void
  ConvertToUnscaledDevPixels(nsPresContext* aPresContext,
                             PRInt32* aScreenX, PRInt32* aScreenY);

  /**
   * If the drag image is a popup, open the popup when the drag begins.
   */
  void OpenDragPopup();

  bool mCanDrop;
  bool mOnlyChromeDrop;
  bool mDoingDrag;
  // true if mImage should be used to set a drag image
  bool mHasImage;
  // true if the user cancelled the drag operation
  bool mUserCancelled;

  PRUint32 mDragAction;
  nsSize mTargetSize;
  nsCOMPtr<nsIDOMNode> mSourceNode;
  nsCOMPtr<nsIDOMDocument> mSourceDocument;       // the document at the drag source. will be null
                                                  //  if it came from outside the app.
  nsCOMPtr<nsIDOMDataTransfer> mDataTransfer;

  // used to determine the image to appear on the cursor while dragging
  nsCOMPtr<nsIDOMNode> mImage;
  // offset of cursor within the image 
  PRInt32 mImageX;
  PRInt32 mImageY;

  // set if a selection is being dragged
  nsCOMPtr<nsISelection> mSelection;

  // set if the image in mImage is a popup. If this case, the popup will be opened
  // and moved instead of using a drag image.
  nsCOMPtr<nsIContent> mDragPopup;

  // the screen position where drag gesture occurred, used for positioning the
  // drag image when no image is specified. If a value is -1, no event was
  // supplied so the screen position is not known
  PRInt32 mScreenX;
  PRInt32 mScreenY;

  // the screen position where the drag ended
  nsIntPoint mEndDragPoint;

  PRUint32 mSuppressLevel;

  // The input source of the drag event. Possible values are from nsIDOMMouseEvent.
  PRUint16 mInputSource;
};

#endif // nsBaseDragService_h__
