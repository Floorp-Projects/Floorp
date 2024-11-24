/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseDragService_h__
#define nsBaseDragService_h__

#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"
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
class BrowserParent;
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
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDRAGSESSION

  int32_t TakeChildProcessDragAction();

  void SetDragEndPoint(nsIntPoint aEndDragPoint) {
    mEndDragPoint =
        mozilla::LayoutDeviceIntPoint::FromUnknownPoint(aEndDragPoint);
  }
  void SetDragEndPoint(mozilla::LayoutDeviceIntPoint aEndDragPoint) {
    mEndDragPoint = aEndDragPoint;
  }

  uint16_t GetInputSource() { return mInputSource; }

  // The nsIDragService uses this to create nsIDragSessions when dragging
  // from a child process.
  MOZ_CAN_RUN_SCRIPT nsresult InitWithRemoteImage(
      nsIWidget* aWidget, nsINode* aDOMNode, nsIPrincipal* aPrincipal,
      nsIContentSecurityPolicy* aCsp, nsICookieJarSettings* aCookieJarSettings,
      nsIArray* aTransferableArray, uint32_t aActionType,
      mozilla::dom::RemoteDragStartData* aDragStartData,
      mozilla::dom::DragEvent* aDragEvent,
      mozilla::dom::DataTransfer* aDataTransfer, bool aIsSynthesizedForTests);

  // The nsIDragService uses this to create nsIDragSessions for dragging
  // a selected region from a Gecko window.
  MOZ_CAN_RUN_SCRIPT nsresult InitWithSelection(
      nsIWidget* aWidget, mozilla::dom::Selection* aSelection,
      nsIPrincipal* aPrincipal, nsIContentSecurityPolicy* aCsp,
      nsICookieJarSettings* aCookieJarSettings, nsIArray* aTransferableArray,
      uint32_t aActionType, mozilla::dom::DragEvent* aDragEvent,
      mozilla::dom::DataTransfer* aDataTransfer, bool aIsSynthesizedForTests);

  // The nsIDragService uses this to create nsIDragSessions when dragging
  // from anywhere else.
  MOZ_CAN_RUN_SCRIPT nsresult InitWithImage(
      nsIWidget* aWidget, nsINode* aDOMNode, nsIPrincipal* aPrincipal,
      nsIContentSecurityPolicy* aCsp, nsICookieJarSettings* aCookieJarSettings,
      nsIArray* aTransferableArray, uint32_t aActionType, nsINode* aImage,
      int32_t aImageX, int32_t aImageY, mozilla::dom::DragEvent* aDragEvent,
      mozilla::dom::DataTransfer* aDataTransfer, bool aIsSynthesizedForTests);

 protected:
  nsBaseDragSession();
  virtual ~nsBaseDragSession();

  /**
   * Starts a modal drag session with an array of transaferables.
   *
   * @param  aPrincipal - the triggering principal of the drag, or null if
   *                      it's from browser chrome or OS
   * @param  aCsp - The csp of the triggering Document
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
   * Called to create a drag session with a Gecko source.  Like all drag
   * sessions, the resulting session needs to be eventually ended with a call
   * to nsIDragSession::EndDragSession.
   */
  MOZ_CAN_RUN_SCRIPT virtual nsresult InvokeDragSessionImpl(
      nsIWidget* aWidget, nsIArray* aTransferableArray,
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
                    RefPtr<mozilla::gfx::SourceSurface>* aSurface,
                    nsPresContext** aPresContext);

  /**
   * Draw a drag image for an image node specified by aImageLoader or aCanvas.
   * This is called by DrawDrag.
   */
  nsresult DrawDragForImage(nsPresContext* aPresContext,
                            nsIImageLoadingContent* aImageLoader,
                            mozilla::dom::HTMLCanvasElement* aCanvas,
                            mozilla::LayoutDeviceIntRect* aScreenDragRect,
                            RefPtr<mozilla::gfx::SourceSurface>* aSurface);

  MOZ_CAN_RUN_SCRIPT virtual nsresult EndDragSessionImpl(
      bool aDoneDrag, uint32_t aKeyModifiers);

  // Returns true if a drag event was dispatched to a child process after
  // the previous TakeDragEventDispatchedToChildProcess() call.
  bool TakeDragEventDispatchedToChildProcess() {
    bool retval = mDragEventDispatchedToChildProcess;
    mDragEventDispatchedToChildProcess = false;
    return retval;
  }

  // Takes the list of PBrowsers that are engaged in the current drag session
  // from the drag service and stores them here.
  void TakeSessionBrowserListFromService();

  /**
   * Free resources contained in DataTransferItems that aren't needed by JS.
   */
  void DiscardInternalTransferData();

  /**
   * If the drag image is a popup, open the popup when the drag begins.
   */
  void OpenDragPopup();

  // Data from a prior call to EndDragSession.
  struct EndDragSessionData {
    bool mDoneDrag = false;
    uint32_t mKeyModifiers = 0;
  };

  // When we delay a drop event in a content process, if we subsequently need to
  // also delay an EndDragSession call, this records the parameters to that
  // call.
  mozilla::Maybe<EndDragSessionData> mEndDragSessionData;
  // When we delay a drop event in a content process, this is the dom::Element
  // that the drop was targetted to.
  nsWeakPtr mDelayedDropTarget;
  // When we delay a drop event in a content process, this is the nsIFrame that
  // would handle the drop.
  WeakFrame mDelayedDropFrame;

  // Parent process PBrowser with a the remote drag session that corresponds to
  // this one and is currently delayed.
  RefPtr<mozilla::dom::BrowserParent> mDelayedDropBrowserParent;

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

  // Weak references to PBrowsers that are currently engaged in drags
  nsTArray<nsWeakPtr> mBrowsers;
  // remote drag data
  RefPtr<mozilla::dom::RemoteDragStartData> mDragStartData;

  // Sub-region for tree-selections.
  mozilla::Maybe<mozilla::CSSIntRegion> mRegion;

  // the screen position where drag gesture occurred, used for positioning the
  // drag image.
  mozilla::CSSIntPoint mScreenPosition;

  // The position relative to the top level widget where the drag ended.
  mozilla::LayoutDeviceIntPoint mEndDragPoint;

  // the contentpolicy type passed to the channel when initiating the drag
  // session
  nsContentPolicyType mContentPolicyType = nsIContentPolicy::TYPE_OTHER;

  uint32_t mDragAction = nsIDragService::DRAGDROP_ACTION_NONE;
  uint32_t mDragActionFromChildProcess =
      nsIDragService::DRAGDROP_ACTION_UNINITIALIZED;

  // mEffectAllowedForTests stores allowed effects at invoking the drag
  // for tests.
  uint32_t mEffectAllowedForTests =
      nsIDragService::DRAGDROP_ACTION_UNINITIALIZED;

  // The input source of the drag event. Possible values are from MouseEvent.
  uint16_t mInputSource = mozilla::dom::MouseEvent_Binding::MOZ_SOURCE_MOUSE;

  // false after EndDragSession has run
  bool mDoingDrag = true;

  bool mCanDrop = false;
  bool mOnlyChromeDrop = false;

  // true if the user cancelled the drag operation
  bool mUserCancelled = false;

  bool mDragEventDispatchedToChildProcess = false;

  bool mIsDraggingTextInTextControl = false;
  bool mSessionIsSynthesizedForTests = false;

  // true if in EndDragSession
  bool mEndingSession = false;
  // true if mImage should be used to set a drag image
  bool mHasImage = false;
};

/**
 * Platform-agnostic base for nsIDragService.
 */
class nsBaseDragService : public nsIDragService {
 public:
  nsBaseDragService();

  // nsISupports
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDRAGSERVICE

  using nsIDragService::GetCurrentSession;

  uint32_t GetSuppressLevel() { return mSuppressLevel; };

  nsTArray<nsWeakPtr> TakeSessionBrowserList() { return std::move(mBrowsers); }

  void ClearCurrentParentDragSession() { mCurrentParentDragSession = nullptr; }

  static nsIWidget* GetWidgetFromWidgetProvider(nsISupports* aWidgetProvider);

 protected:
  virtual ~nsBaseDragService();

  virtual already_AddRefed<nsIDragSession> CreateDragSession() = 0;

  // The drag session representing the user's current actions, if any.
  // This value is only valid in the parent process.  For child
  // process drag sessions, see BrowserChild and PuppetWidget.
  RefPtr<nsIDragSession> mCurrentParentDragSession;

  // Sub-region for tree-selections.
  mozilla::Maybe<mozilla::CSSIntRegion> mRegion;

  RefPtr<mozilla::test::MockDragServiceController> mMockController;

  // Weak references to PBrowsers that are currently engaged in drags.
  // Once an nsIDragSession is created for the remote drag, these browsers
  // will be moved to that object.
  nsTArray<nsWeakPtr> mBrowsers;

  uint32_t mSuppressLevel = 0;

  // If this is set, mSessionIsSynthesizedForTests should not become true.
  // This hack is used to bypass the "old" drag-drop test behavior.
  // See nsIDragService.idl for details.
  bool mNeverAllowSessionIsSynthesizedForTests = false;
};

#endif  // nsBaseDragService_h__
