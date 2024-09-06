/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseDragService_h__
#define nsBaseDragService_h__

#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsCOMPtr.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsString.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/RemoteDragStartData.h"
#include "mozilla/Logging.h"
#include "nsTArray.h"
#include "nsRegion.h"
#include "Units.h"

extern mozilla::LazyLogModule sWidgetDragServiceLog;
#define MOZ_DRAGSERVICE_LOG(...) \
  MOZ_LOG(sWidgetDragServiceLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#define MOZ_DRAGSERVICE_LOG_ENABLED() \
  MOZ_LOG_TEST(sWidgetDragServiceLog, mozilla::LogLevel::Debug)

// translucency level for drag images
#define DRAG_TRANSLUCENCY 0.65

class nsIContent;

class nsINode;
class nsPresContext;
class nsIImageLoadingContent;

namespace mozilla {
namespace gfx {
class SourceSurface;
}  // namespace gfx

namespace dom {
class DataTransfer;
class Selection;
}  // namespace dom

namespace test {
class MockDragServiceController;
}  // namespace test
}  // namespace mozilla

/**
 * Platform-agnostic base for nsIDragSession.
 */
class nsBaseDragSession : public nsIDragSession {
 public:
  NS_DECL_NSIDRAGSESSION

  int32_t TakeChildProcessDragAction();

  void SetDragEndPoint(nsIntPoint aEndDragPoint) {
    mEndDragPoint =
        mozilla::LayoutDeviceIntPoint::FromUnknownPoint(aEndDragPoint);
  }
  void SetDragEndPoint(mozilla::LayoutDeviceIntPoint aEndDragPoint) {
    mEndDragPoint = aEndDragPoint;
  }

 protected:
  ~nsBaseDragSession();

  // Returns true if a drag event was dispatched to a child process after
  // the previous TakeDragEventDispatchedToChildProcess() call.
  bool TakeDragEventDispatchedToChildProcess() {
    bool retval = mDragEventDispatchedToChildProcess;
    mDragEventDispatchedToChildProcess = false;
    return retval;
  }

  /**
   * Free resources contained in DataTransferItems that aren't needed by JS.
   */
  void DiscardInternalTransferData();

  /**
   * If the drag image is a popup, open the popup when the drag begins.
   */
  void OpenDragPopup();

  RefPtr<mozilla::dom::WindowContext> mSourceWindowContext;
  RefPtr<mozilla::dom::WindowContext> mSourceTopWindowContext;
  nsCOMPtr<nsINode> mSourceNode;
  // the document at the drag source. will be null if it came from outside the
  // app.
  RefPtr<mozilla::dom::Document> mSourceDocument;
  // set if a selection is being dragged
  RefPtr<mozilla::dom::Selection> mSelection;

  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  nsCOMPtr<nsIContentSecurityPolicy> mCsp;
  RefPtr<mozilla::dom::DataTransfer> mDataTransfer;

  // used to determine the image to appear on the cursor while dragging
  nsCOMPtr<nsINode> mImage;
  // offset of cursor within the image
  mozilla::CSSIntPoint mImageOffset;
  // set if the image in mImage is a popup. If this case, the popup will be
  // opened and moved instead of using a drag image.
  nsCOMPtr<mozilla::dom::Element> mDragPopup;

  // the screen position where drag gesture occurred, used for positioning the
  // drag image.
  mozilla::CSSIntPoint mScreenPosition;

  // The position relative to the top level widget where the drag ended.
  mozilla::LayoutDeviceIntPoint mEndDragPoint;

  uint32_t mDragAction = nsIDragService::DRAGDROP_ACTION_NONE;
  uint32_t mDragActionFromChildProcess =
      nsIDragService::DRAGDROP_ACTION_UNINITIALIZED;

  // mEffectAllowedForTests stores allowed effects at invoking the drag
  // for tests.
  uint32_t mEffectAllowedForTests =
      nsIDragService::DRAGDROP_ACTION_UNINITIALIZED;

  // The input source of the drag event. Possible values are from MouseEvent.
  uint16_t mInputSource = mozilla::dom::MouseEvent_Binding::MOZ_SOURCE_MOUSE;

  bool mDoingDrag = false;

  bool mCanDrop = false;
  bool mOnlyChromeDrop = false;

  // true if the user cancelled the drag operation
  bool mUserCancelled = false;

  bool mDragEventDispatchedToChildProcess = false;

  bool mIsDraggingTextInTextControl = false;
  bool mSessionIsSynthesizedForTests = false;
};

/**
 * Platform-agnostic base for nsIDragService.
 * NB: This class temporarily subclasses nsBaseDragSession while we move
 * methods from it to nsBaseDragSession.  The inheritance relationship
 * will be severed by the end of this patch series.
 */
class nsBaseDragService : public nsIDragService, public nsBaseDragSession {
 public:
  typedef mozilla::gfx::SourceSurface SourceSurface;

  nsBaseDragService();

  // nsISupports
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDRAGSERVICE

  uint16_t GetInputSource() { return mInputSource; }

  using nsIDragService::GetCurrentSession;

  uint32_t GetSuppressLevel() { return mSuppressLevel; };

 protected:
  virtual ~nsBaseDragService();

  /**
   * Starts a modal drag session with an array of transaferables.
   *
   * @param  aPrincipal - the triggering principal of the drag, or null if
   *                      it's from browser chrome or OS
   * @param aCsp - The csp of the triggering Document
   * @param  aTransferables - an array of transferables to be dragged
   * @param  aActionType - specified which of copy/move/link are allowed
   * @param  aContentPolicyType - the contentPolicyType that will be
   *           passed to the loadInfo when creating a new channel
   *           (defaults to TYPE_OTHER)
   */
  MOZ_CAN_RUN_SCRIPT virtual nsresult InvokeDragSession(
      nsIWidget* aWidget, nsINode* aDOMNode, nsIPrincipal* aPrincipal,
      nsIContentSecurityPolicy* aCsp, nsICookieJarSettings* aCookieJarSettings,
      nsIArray* aTransferableArray, uint32_t aActionType,
      nsContentPolicyType aContentPolicyType = nsIContentPolicy::TYPE_OTHER);

   /**
   * Called from nsBaseDragService to initiate a platform drag from a source
   * in this process.  This is expected to ensure that StartDragSession() and
   * EndDragSession() get called if the platform drag is successfully invoked.
   */
  MOZ_CAN_RUN_SCRIPT virtual nsresult InvokeDragSessionImpl(
      nsIArray* aTransferableArray,
      const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
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
  nsresult DrawDrag(nsINode* aDOMNode,
                    const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
                    mozilla::CSSIntPoint aScreenPosition,
                    mozilla::LayoutDeviceIntRect* aScreenDragRect,
                    RefPtr<SourceSurface>* aSurface,
                    nsPresContext** aPresContext);

  /**
   * Draw a drag image for an image node specified by aImageLoader or aCanvas.
   * This is called by DrawDrag.
   */
  nsresult DrawDragForImage(nsPresContext* aPresContext,
                            nsIImageLoadingContent* aImageLoader,
                            mozilla::dom::HTMLCanvasElement* aCanvas,
                            mozilla::LayoutDeviceIntRect* aScreenDragRect,
                            RefPtr<SourceSurface>* aSurface);

  virtual bool IsMockService() { return false; }

  // true if in EndDragSession
  bool mEndingSession;
  // true if mImage should be used to set a drag image
  bool mHasImage;

  // the contentpolicy type passed to the channel when initiating the drag
  // session
  nsContentPolicyType mContentPolicyType;

  // remote drag data
  RefPtr<mozilla::dom::RemoteDragStartData> mDragStartData;

  uint32_t mSuppressLevel;

  // Weak references to PBrowsers that are currently engaged in drags
  nsTArray<nsWeakPtr> mBrowsers;

  // Sub-region for tree-selections.
  mozilla::Maybe<mozilla::CSSIntRegion> mRegion;

  RefPtr<mozilla::test::MockDragServiceController> mMockController;

  // If this is set, mSessionIsSynthesizedForTests should not become true.
  // This hack is used to bypass the "old" drag-drop test behavior.
  // See nsIDragService.idl for details.
  bool mNeverAllowSessionIsSynthesizedForTests = false;
};

#endif  // nsBaseDragService_h__
