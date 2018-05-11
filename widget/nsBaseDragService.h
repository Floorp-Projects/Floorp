/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseDragService_h__
#define nsBaseDragService_h__

#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsITransferable.h"
#include "nsCOMPtr.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsString.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsTArray.h"
#include "Units.h"

// translucency level for drag images
#define DRAG_TRANSLUCENCY 0.65

class nsIContent;
class nsIDOMNode;
class nsIDocument;
class nsPresContext;
class nsIImageLoadingContent;

namespace mozilla {
namespace gfx {
class SourceSurface;
} // namespace gfx

namespace dom {
class DataTransfer;
class Selection;
} // namespace dom
} // namespace mozilla

/**
 * XP DragService wrapper base class
 */

class nsBaseDragService : public nsIDragService,
                          public nsIDragSession
{

public:
  typedef mozilla::gfx::SourceSurface SourceSurface;

  nsBaseDragService();

  //nsISupports
  NS_DECL_ISUPPORTS

  //nsIDragSession and nsIDragService
  NS_DECL_NSIDRAGSERVICE
  NS_DECL_NSIDRAGSESSION

  void SetDragEndPoint(nsIntPoint aEndDragPoint)
  {
    mEndDragPoint = mozilla::LayoutDeviceIntPoint::FromUnknownPoint(aEndDragPoint);
  }
  void SetDragEndPoint(mozilla::LayoutDeviceIntPoint aEndDragPoint)
  {
    mEndDragPoint = aEndDragPoint;
  }

  uint16_t GetInputSource() { return mInputSource; }

  int32_t TakeChildProcessDragAction();

protected:
  virtual ~nsBaseDragService();

  /**
   * Called from nsBaseDragService to initiate a platform drag from a source
   * in this process.  This is expected to ensure that StartDragSession() and
   * EndDragSession() get called if the platform drag is successfully invoked.
   */
  virtual nsresult InvokeDragSessionImpl(nsIArray* aTransferableArray,
                                         nsIScriptableRegion* aDragRgn,
                                         uint32_t aActionType) = 0;

  /**
   * Draw the drag image, if any, to a surface and return it. The drag image
   * is constructed from mImage if specified, or aDOMNode if mImage is null.
   *
   * aRegion may be used to draw only a subset of the element. This region
   * should be supplied using x and y coordinates measured in css pixels
   * that are relative to the upper-left corner of the window.
   *
   * aScreenPosition should be the screen coordinates of the mouse click
   * for the drag. These are in CSS pixels.
   *
   * On return, aScreenDragRect will contain the screen coordinates of the
   * area being dragged. This is used by the platform-specific part of the
   * drag service to determine the drag feedback. This rect will be in the
   * device pixels of the presContext.
   *
   * If there is no drag image, the returned surface will be null, but
   * aScreenDragRect will still be set to the drag area.
   *
   * aPresContext will be set to the nsPresContext used determined from
   * whichever of mImage or aDOMNode is used.
   */
  nsresult DrawDrag(nsIDOMNode* aDOMNode,
                    nsIScriptableRegion* aRegion,
                    mozilla::CSSIntPoint aScreenPosition,
                    mozilla::LayoutDeviceIntRect* aScreenDragRect,
                    RefPtr<SourceSurface>* aSurface,
                    nsPresContext **aPresContext);

  /**
   * Draw a drag image for an image node specified by aImageLoader or aCanvas.
   * This is called by DrawDrag.
   */
  nsresult DrawDragForImage(nsPresContext *aPresContext,
                            nsIImageLoadingContent* aImageLoader,
                            mozilla::dom::HTMLCanvasElement* aCanvas,
                            mozilla::LayoutDeviceIntRect* aScreenDragRect,
                            RefPtr<SourceSurface>* aSurface);

  /**
   * Convert aScreenPosition from CSS pixels into unscaled device pixels.
   */
  mozilla::LayoutDeviceIntPoint
  ConvertToUnscaledDevPixels(nsPresContext* aPresContext,
                             mozilla::CSSIntPoint aScreenPosition);

  /**
   * If the drag image is a popup, open the popup when the drag begins.
   */
  void OpenDragPopup();

  /**
   * Free resources contained in DataTransferItems that aren't needed by JS.
   */
  void DiscardInternalTransferData();

  // Returns true if a drag event was dispatched to a child process after
  // the previous TakeDragEventDispatchedToChildProcess() call.
  bool TakeDragEventDispatchedToChildProcess()
  {
    bool retval = mDragEventDispatchedToChildProcess;
    mDragEventDispatchedToChildProcess = false;
    return retval;
  }

  bool mCanDrop;
  bool mOnlyChromeDrop;
  bool mDoingDrag;
  // true if mImage should be used to set a drag image
  bool mHasImage;
  // true if the user cancelled the drag operation
  bool mUserCancelled;

  bool mDragEventDispatchedToChildProcess;

  uint32_t mDragAction;
  uint32_t mDragActionFromChildProcess;

  nsSize mTargetSize;
  nsCOMPtr<nsIDOMNode> mSourceNode;
  nsCString mTriggeringPrincipalURISpec;
  nsCOMPtr<nsIDocument> mSourceDocument;          // the document at the drag source. will be null
                                                  //  if it came from outside the app.
  nsContentPolicyType mContentPolicyType;         // the contentpolicy type passed to the channel
                                                  // when initiating the drag session
  RefPtr<mozilla::dom::DataTransfer> mDataTransfer;

  // used to determine the image to appear on the cursor while dragging
  nsCOMPtr<nsIDOMNode> mImage;
  // offset of cursor within the image
  mozilla::CSSIntPoint mImageOffset;

  // set if a selection is being dragged
  RefPtr<mozilla::dom::Selection> mSelection;

  // set if the image in mImage is a popup. If this case, the popup will be opened
  // and moved instead of using a drag image.
  nsCOMPtr<nsIContent> mDragPopup;

  // the screen position where drag gesture occurred, used for positioning the
  // drag image.
  mozilla::CSSIntPoint mScreenPosition;

  // the screen position where the drag ended
  mozilla::LayoutDeviceIntPoint mEndDragPoint;

  uint32_t mSuppressLevel;

  // The input source of the drag event. Possible values are from MouseEvent.
  uint16_t mInputSource;

  nsTArray<RefPtr<mozilla::dom::ContentParent>> mChildProcesses;
};

#endif // nsBaseDragService_h__
