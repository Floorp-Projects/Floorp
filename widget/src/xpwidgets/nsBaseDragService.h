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
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

  PRPackedBool mCanDrop;
  PRPackedBool mOnlyChromeDrop;
  PRPackedBool mDoingDrag;
  // true if mImage should be used to set a drag image
  PRPackedBool mHasImage;
  // true if the user cancelled the drag operation
  PRPackedBool mUserCancelled;

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

  // The input source of the drag event. Possible values are from nsIDOMNSMouseEvent.
  PRUint16 mInputSource;
};

#endif // nsBaseDragService_h__
