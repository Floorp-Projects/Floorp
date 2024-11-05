/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBaseDragService.h"
#include "nsITransferable.h"

#include "nsArrayUtils.h"
#include "nsITransferable.h"
#include "nsSize.h"
#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIFrame.h"
#include "nsFrameLoaderOwner.h"
#include "nsIContent.h"
#include "nsViewManager.h"
#include "nsINode.h"
#include "nsPresContext.h"
#include "nsIImageLoadingContent.h"
#include "imgIContainer.h"
#include "imgIRequest.h"
#include "ImageRegion.h"
#include "nsQueryObject.h"
#include "nsRegion.h"
#include "nsXULPopupManager.h"
#include "nsMenuPopupFrame.h"
#include "nsTreeBodyFrame.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/SVGImageContext.h"
#include "mozilla/TextControlElement.h"
#include "mozilla/Unused.h"
#include "mozilla/ViewportUtils.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DataTransferItemList.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/DragEvent.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/gfx/2D.h"
#include "nsFrameLoader.h"
#include "nsIMutableArray.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "nscore.h"
#include "MockDragServiceController.h"

#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

LazyLogModule sWidgetDragServiceLog("WidgetDragService");

#define DRAGIMAGES_PREF "nglayout.enable_drag_images"

// TODO: Temporary hack to share drag session suppression level.
// Removed later in this patch series.
uint32_t GetSuppressLevel() {
  nsCOMPtr<nsIDragService> svc =
      do_GetService("@mozilla.org/widget/dragservice;1");
  NS_ENSURE_TRUE(svc, 0);
  return static_cast<nsBaseDragService*>(svc.get())->GetSuppressLevel();
}

nsBaseDragService::nsBaseDragService() = default;
nsBaseDragService::~nsBaseDragService() = default;

nsBaseDragSession::nsBaseDragSession() { TakeSessionBrowserListFromService(); }
nsBaseDragSession::~nsBaseDragSession() = default;

NS_IMPL_ISUPPORTS(nsBaseDragService, nsIDragService)
NS_IMPL_ISUPPORTS(nsBaseDragSession, nsIDragSession)

NS_IMETHODIMP nsBaseDragService::GetIsMockService(bool* aRet) {
  *aRet = false;
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsBaseDragSession::SetCanDrop(bool aCanDrop) {
  mCanDrop = aCanDrop;
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsBaseDragSession::GetCanDrop(bool* aCanDrop) {
  *aCanDrop = mCanDrop;
  return NS_OK;
}
//---------------------------------------------------------
NS_IMETHODIMP
nsBaseDragSession::SetOnlyChromeDrop(bool aOnlyChrome) {
  mOnlyChromeDrop = aOnlyChrome;
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsBaseDragSession::GetOnlyChromeDrop(bool* aOnlyChrome) {
  *aOnlyChrome = mOnlyChromeDrop;
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsBaseDragSession::SetDragAction(uint32_t anAction) {
  mDragAction = anAction;
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsBaseDragSession::GetDragAction(uint32_t* anAction) {
  *anAction = mDragAction;
  return NS_OK;
}

//-------------------------------------------------------------------------

NS_IMETHODIMP
nsBaseDragSession::GetNumDropItems(uint32_t* aNumItems) {
  *aNumItems = 0;
  return NS_ERROR_FAILURE;
}

//
// GetSourceWindowContext
//
// Returns the window context where the drag was initiated. This will be
// nullptr if the drag began outside of our application.
//
NS_IMETHODIMP
nsBaseDragSession::GetSourceWindowContext(
    WindowContext** aSourceWindowContext) {
  *aSourceWindowContext = mSourceWindowContext.get();
  NS_IF_ADDREF(*aSourceWindowContext);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::SetSourceWindowContext(WindowContext* aSourceWindowContext) {
  // This should only be called in a child process.
  MOZ_ASSERT(!XRE_IsParentProcess());
  mSourceWindowContext = aSourceWindowContext;
  return NS_OK;
}

//
// GetSourceTopWindowContext
//
// Returns the top-level window context where the drag was initiated. This will
// be nullptr if the drag began outside of our application.
//
NS_IMETHODIMP
nsBaseDragSession::GetSourceTopWindowContext(
    WindowContext** aSourceTopWindowContext) {
  *aSourceTopWindowContext = mSourceTopWindowContext.get();
  NS_IF_ADDREF(*aSourceTopWindowContext);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::SetSourceTopWindowContext(
    WindowContext* aSourceTopWindowContext) {
  // This should only be called in a child process.
  MOZ_ASSERT(!XRE_IsParentProcess());
  mSourceTopWindowContext = aSourceTopWindowContext;
  return NS_OK;
}

//
// GetSourceNode
//
// Returns the DOM node where the drag was initiated. This will be
// nullptr if the drag began outside of our application.
//
NS_IMETHODIMP
nsBaseDragSession::GetSourceNode(nsINode** aSourceNode) {
  *aSourceNode = do_AddRef(mSourceNode).take();
  return NS_OK;
}

void nsBaseDragSession::UpdateSource(nsINode* aNewSourceNode,
                                     Selection* aNewSelection) {
  MOZ_ASSERT(mSourceNode);
  MOZ_ASSERT(aNewSourceNode);
  MOZ_ASSERT(mSourceNode->IsInNativeAnonymousSubtree() ||
             aNewSourceNode->IsInNativeAnonymousSubtree());
  MOZ_ASSERT(mSourceDocument == aNewSourceNode->OwnerDoc());
  mSourceNode = aNewSourceNode;
  // Don't set mSelection if the session was invoked without selection or
  // making it becomes nullptr.  The latter occurs when the old frame is
  // being destroyed.
  if (mSelection && aNewSelection) {
    // XXX If the dragging image is created once (e.g., at drag start), the
    //     image won't be updated unless we notify `DrawDrag` callers.
    //     However, it must be okay for now to keep using older image of
    //     Selection.
    mSelection = aNewSelection;
  }
}

NS_IMETHODIMP
nsBaseDragSession::GetTriggeringPrincipal(nsIPrincipal** aPrincipal) {
  NS_IF_ADDREF(*aPrincipal = mTriggeringPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::SetTriggeringPrincipal(nsIPrincipal* aPrincipal) {
  mTriggeringPrincipal = aPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::GetCsp(nsIContentSecurityPolicy** aCsp) {
  NS_IF_ADDREF(*aCsp = mCsp);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::SetCsp(nsIContentSecurityPolicy* aCsp) {
  mCsp = aCsp;
  return NS_OK;
}

//-------------------------------------------------------------------------

NS_IMETHODIMP
nsBaseDragSession::GetData(nsITransferable* aTransferable,
                           uint32_t aItemIndex) {
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsBaseDragSession::IsDataFlavorSupported(const char* aDataFlavor,
                                         bool* _retval) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsBaseDragSession::GetDataTransferXPCOM(DataTransfer** aDataTransfer) {
  *aDataTransfer = mDataTransfer;
  NS_IF_ADDREF(*aDataTransfer);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::SetDataTransferXPCOM(DataTransfer* aDataTransfer) {
  NS_ENSURE_STATE(aDataTransfer);
  mDataTransfer = aDataTransfer;
  return NS_OK;
}

DataTransfer* nsBaseDragSession::GetDataTransfer() { return mDataTransfer; }

void nsBaseDragSession::SetDataTransfer(DataTransfer* aDataTransfer) {
  mDataTransfer = aDataTransfer;
}

bool nsBaseDragSession::IsSynthesizedForTests() {
  return mSessionIsSynthesizedForTests;
}

bool nsBaseDragSession::IsDraggingTextInTextControl() {
  return mIsDraggingTextInTextControl;
}

uint32_t nsBaseDragSession::GetEffectAllowedForTests() {
  MOZ_ASSERT(mSessionIsSynthesizedForTests);
  return mEffectAllowedForTests;
}

NS_IMETHODIMP nsBaseDragSession::SetDragEndPointForTests(int32_t aScreenX,
                                                         int32_t aScreenY) {
  MOZ_ASSERT(mDoingDrag);
  MOZ_ASSERT(mSourceDocument);
  MOZ_ASSERT(mSessionIsSynthesizedForTests);

  if (!mDoingDrag || !mSourceDocument || !mSessionIsSynthesizedForTests) {
    return NS_ERROR_FAILURE;
  }
  nsPresContext* pc = mSourceDocument->GetPresContext();
  if (NS_WARN_IF(!pc)) {
    return NS_ERROR_FAILURE;
  }
  auto p = LayoutDeviceIntPoint::Round(CSSIntPoint(aScreenX, aScreenY) *
                                       pc->CSSToDevPixelScale());
  // p is screen-relative, and we want them to be top-level-widget-relative.
  if (nsCOMPtr<nsIWidget> widget = pc->GetRootWidget()) {
    p -= widget->WidgetToScreenOffset();
    p += widget->WidgetToTopLevelWidgetOffset();
  }
  SetDragEndPoint(p);
  return NS_OK;
}

//-------------------------------------------------------------------------
nsresult nsBaseDragSession::InvokeDragSession(
    nsIWidget* aWidget, nsINode* aDOMNode, nsIPrincipal* aPrincipal,
    nsIContentSecurityPolicy* aCsp, nsICookieJarSettings* aCookieJarSettings,
    nsIArray* aTransferableArray, uint32_t aActionType,
    nsContentPolicyType aContentPolicyType) {
  AUTO_PROFILER_LABEL("nsBaseDragService::InvokeDragSession", OTHER);

  NS_ENSURE_TRUE(aDOMNode, NS_ERROR_INVALID_ARG);

  // stash the document of the dom node
  mSourceDocument = aDOMNode->OwnerDoc();
  mTriggeringPrincipal = aPrincipal;
  mCsp = aCsp;
  mSourceNode = aDOMNode;
  mIsDraggingTextInTextControl =
      mSourceNode->IsInNativeAnonymousSubtree() &&
      TextControlElement::FromNodeOrNull(
          mSourceNode->GetClosestNativeAnonymousSubtreeRootParentOrHost());
  mContentPolicyType = aContentPolicyType;
  mEndDragPoint = LayoutDeviceIntPoint(0, 0);

  // When the mouse goes down, the selection code starts a mouse
  // capture. However, this gets in the way of determining drag
  // feedback for things like trees because the event coordinates
  // are in the wrong coord system, so turn off mouse capture.
  PresShell::ClearMouseCapture();

  if (mSessionIsSynthesizedForTests) {
    mDoingDrag = true;
    mDragAction = aActionType;
    mEffectAllowedForTests = aActionType;
    return NS_OK;
  }

  if (XRE_IsParentProcess()) {
    // If you're hitting this, a test is causing the browser to attempt to enter
    // the drag-drop native nested event loop, which will put the browser in a
    // state that won't run tests properly until there's manual intervention
    // to exit the drag-drop loop (either by moving the mouse or hitting
    // escape), which can't be done from script since we're in the nested loop.
    //
    // The best way to avoid this is to use the mock service in tests.  See
    // synthesizeMockDragAndDrop.
    nsCOMPtr<nsIDragService> dragService =
        do_GetService("@mozilla.org/widget/dragservice;1");
    MOZ_ASSERT(dragService);
    MOZ_ASSERT(
        !xpc::IsInAutomation() || dragService->IsMockService(),
        "About to start drag-drop native loop on which will prevent later "
        "tests from running properly.");
  }

  uint32_t length = 0;
  mozilla::Unused << aTransferableArray->GetLength(&length);
  if (!length) {
    nsCOMPtr<nsIMutableArray> mutableArray =
        do_QueryInterface(aTransferableArray);
    if (mutableArray) {
      // In order to be able trigger dnd, we need to have some transferable
      // object.
      nsCOMPtr<nsITransferable> trans =
          do_CreateInstance("@mozilla.org/widget/transferable;1");
      trans->Init(nullptr);
      trans->SetDataPrincipal(mSourceNode->NodePrincipal());
      trans->SetContentPolicyType(mContentPolicyType);
      trans->SetCookieJarSettings(aCookieJarSettings);
      mutableArray->AppendElement(trans);
    }
  } else {
    for (uint32_t i = 0; i < length; ++i) {
      nsCOMPtr<nsITransferable> trans =
          do_QueryElementAt(aTransferableArray, i);
      if (trans) {
        // Set the dataPrincipal on the transferable.
        trans->SetDataPrincipal(mSourceNode->NodePrincipal());
        trans->SetContentPolicyType(mContentPolicyType);
        trans->SetCookieJarSettings(aCookieJarSettings);
      }
    }
  }

  nsresult rv =
      InvokeDragSessionImpl(aWidget, aTransferableArray, mRegion, aActionType);

  if (NS_FAILED(rv)) {
    // Set mDoingDrag so that EndDragSession cleans up and sends the dragend
    // event after the aborted drag.
    mDoingDrag = true;
    EndDragSession(true, 0);
  }

  return rv;
}

NS_IMETHODIMP
nsBaseDragService::InvokeDragSessionWithImage(
    nsINode* aDOMNode, nsIPrincipal* aPrincipal, nsIContentSecurityPolicy* aCsp,
    nsICookieJarSettings* aCookieJarSettings, nsIArray* aTransferableArray,
    uint32_t aActionType, nsINode* aImage, int32_t aImageX, int32_t aImageY,
    DragEvent* aDragEvent, DataTransfer* aDataTransfer) {
  nsCOMPtr<nsIWidget> widget =
      aDragEvent->WidgetEventPtr()->AsDragEvent()->mWidget;
  MOZ_ASSERT(widget);

  NS_ENSURE_TRUE(aDragEvent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aDataTransfer, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(mSuppressLevel == 0, NS_ERROR_FAILURE);

  RefPtr<nsBaseDragSession> session =
      CreateDragSession().downcast<nsBaseDragSession>();
  if (XRE_IsParentProcess()) {
    mCurrentParentDragSession = session;
  }
  bool isSynthesized =
      aDragEvent->WidgetEventPtr()->mFlags.mIsSynthesizedForTests &&
      !GetNeverAllowSessionIsSynthesizedForTests();
  return session->InitWithImage(widget, aDOMNode, aPrincipal, aCsp,
                                aCookieJarSettings, aTransferableArray,
                                aActionType, aImage, aImageX, aImageY,
                                aDragEvent, aDataTransfer, isSynthesized);
}

nsresult nsBaseDragSession::InitWithImage(
    nsIWidget* aWidget, nsINode* aDOMNode, nsIPrincipal* aPrincipal,
    nsIContentSecurityPolicy* aCsp, nsICookieJarSettings* aCookieJarSettings,
    nsIArray* aTransferableArray, uint32_t aActionType, nsINode* aImage,
    int32_t aImageX, int32_t aImageY, DragEvent* aDragEvent,
    DataTransfer* aDataTransfer, bool aIsSynthesizedForTests) {
  mSessionIsSynthesizedForTests = aIsSynthesizedForTests;
  mDataTransfer = aDataTransfer;
  mSelection = nullptr;
  mHasImage = true;
  mDragPopup = nullptr;
  mImage = aImage;
  mImageOffset = CSSIntPoint(aImageX, aImageY);
  mDragStartData = nullptr;
  mSourceWindowContext =
      aDOMNode ? aDOMNode->OwnerDoc()->GetWindowContext() : nullptr;
  mSourceTopWindowContext =
      mSourceWindowContext ? mSourceWindowContext->TopWindowContext() : nullptr;

  mScreenPosition = aDragEvent->ScreenPoint(CallerType::System);
  mInputSource = aDragEvent->InputSource(CallerType::System);

  // If dragging within a XUL tree and no custom drag image was
  // set, the region argument to InvokeDragSessionWithImage needs
  // to be set to the area encompassing the selected rows of the
  // tree to ensure that the drag feedback gets clipped to those
  // rows. For other content, region should be null.
  mRegion = Nothing();
  if (aDOMNode && aDOMNode->IsContent() && !aImage) {
    if (aDOMNode->NodeInfo()->Equals(nsGkAtoms::treechildren,
                                     kNameSpaceID_XUL)) {
      nsTreeBodyFrame* treeBody =
          do_QueryFrame(aDOMNode->AsContent()->GetPrimaryFrame());
      if (treeBody) {
        mRegion = treeBody->GetSelectionRegion();
      }
    }
  }

  nsresult rv = InvokeDragSession(
      aWidget, aDOMNode, aPrincipal, aCsp, aCookieJarSettings,
      aTransferableArray, aActionType, nsIContentPolicy::TYPE_INTERNAL_IMAGE);
  mRegion = Nothing();
  return rv;
}

NS_IMETHODIMP
nsBaseDragService::InvokeDragSessionWithRemoteImage(
    nsINode* aDOMNode, nsIPrincipal* aPrincipal, nsIContentSecurityPolicy* aCsp,
    nsICookieJarSettings* aCookieJarSettings, nsIArray* aTransferableArray,
    uint32_t aActionType, RemoteDragStartData* aDragStartData,
    DragEvent* aDragEvent, DataTransfer* aDataTransfer) {
  nsCOMPtr<nsIWidget> widget =
      aDragEvent->WidgetEventPtr()->AsDragEvent()->mWidget;
  MOZ_ASSERT(widget);

  NS_ENSURE_TRUE(aDragEvent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aDataTransfer, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(mSuppressLevel == 0, NS_ERROR_FAILURE);

  RefPtr<nsBaseDragSession> session =
      CreateDragSession().downcast<nsBaseDragSession>();
  if (XRE_IsParentProcess()) {
    mCurrentParentDragSession = session;
  }
  bool isSynthesized =
      aDragEvent->WidgetEventPtr()->mFlags.mIsSynthesizedForTests &&
      !GetNeverAllowSessionIsSynthesizedForTests();
  return session->InitWithRemoteImage(widget, aDOMNode, aPrincipal, aCsp,
                                      aCookieJarSettings, aTransferableArray,
                                      aActionType, aDragStartData, aDragEvent,
                                      aDataTransfer, isSynthesized);
}

nsresult nsBaseDragSession::InitWithRemoteImage(
    nsIWidget* aWidget, nsINode* aDOMNode, nsIPrincipal* aPrincipal,
    nsIContentSecurityPolicy* aCsp, nsICookieJarSettings* aCookieJarSettings,
    nsIArray* aTransferableArray, uint32_t aActionType,
    RemoteDragStartData* aDragStartData, DragEvent* aDragEvent,
    DataTransfer* aDataTransfer, bool aIsSynthesizedForTests) {
  mSessionIsSynthesizedForTests = aIsSynthesizedForTests;
  mDataTransfer = aDataTransfer;
  mSelection = nullptr;
  mHasImage = true;
  mDragPopup = nullptr;
  mImage = nullptr;
  mDragStartData = aDragStartData;
  mImageOffset = CSSIntPoint(0, 0);
  mSourceWindowContext = mDragStartData->GetSourceWindowContext();
  mSourceTopWindowContext = mDragStartData->GetSourceTopWindowContext();

  mScreenPosition = aDragEvent->ScreenPoint(CallerType::System);
  mInputSource = aDragEvent->InputSource(CallerType::System);

  nsresult rv = InvokeDragSession(
      aWidget, aDOMNode, aPrincipal, aCsp, aCookieJarSettings,
      aTransferableArray, aActionType, nsIContentPolicy::TYPE_INTERNAL_IMAGE);
  mRegion = Nothing();
  return rv;
}

NS_IMETHODIMP
nsBaseDragService::InvokeDragSessionWithSelection(
    Selection* aSelection, nsIPrincipal* aPrincipal,
    nsIContentSecurityPolicy* aCsp, nsICookieJarSettings* aCookieJarSettings,
    nsIArray* aTransferableArray, uint32_t aActionType, DragEvent* aDragEvent,
    DataTransfer* aDataTransfer) {
  nsCOMPtr<nsIWidget> widget =
      aDragEvent->WidgetEventPtr()->AsDragEvent()->mWidget;
  MOZ_ASSERT(widget);

  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aDragEvent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(mSuppressLevel == 0, NS_ERROR_FAILURE);

  RefPtr<nsBaseDragSession> session =
      CreateDragSession().downcast<nsBaseDragSession>();
  if (XRE_IsParentProcess()) {
    mCurrentParentDragSession = session;
  }
  bool isSynthesized =
      aDragEvent->WidgetEventPtr()->mFlags.mIsSynthesizedForTests &&
      !GetNeverAllowSessionIsSynthesizedForTests();
  return session->InitWithSelection(widget, aSelection, aPrincipal, aCsp,
                                    aCookieJarSettings, aTransferableArray,
                                    aActionType, aDragEvent, aDataTransfer,
                                    isSynthesized);
}

nsresult nsBaseDragSession::InitWithSelection(
    nsIWidget* aWidget, Selection* aSelection, nsIPrincipal* aPrincipal,
    nsIContentSecurityPolicy* aCsp, nsICookieJarSettings* aCookieJarSettings,
    nsIArray* aTransferableArray, uint32_t aActionType, DragEvent* aDragEvent,
    DataTransfer* aDataTransfer, bool aIsSynthesizedForTests) {
  mSessionIsSynthesizedForTests = aIsSynthesizedForTests;
  mDataTransfer = aDataTransfer;
  mSelection = aSelection;
  mHasImage = true;
  mDragPopup = nullptr;
  mImage = nullptr;
  mImageOffset = CSSIntPoint();
  mDragStartData = nullptr;
  mRegion = Nothing();

  mScreenPosition.x = aDragEvent->ScreenX(CallerType::System);
  mScreenPosition.y = aDragEvent->ScreenY(CallerType::System);
  mInputSource = aDragEvent->InputSource(CallerType::System);

  // just get the focused node from the selection
  // XXXndeakin this should actually be the deepest node that contains both
  // endpoints of the selection
  nsCOMPtr<nsINode> node = aSelection->GetFocusNode();
  mSourceWindowContext = node ? node->OwnerDoc()->GetWindowContext() : nullptr;
  mSourceTopWindowContext =
      mSourceWindowContext ? mSourceWindowContext->TopWindowContext() : nullptr;

  return InvokeDragSession(aWidget, node, aPrincipal, aCsp, aCookieJarSettings,
                           aTransferableArray, aActionType,
                           nsIContentPolicy::TYPE_OTHER);
}

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsBaseDragService::GetCurrentSession(nsISupports* aWidgetProvider,
                                     nsIDragSession** aSession) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!aSession) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!mSuppressLevel && mCurrentParentDragSession) {
    RefPtr<nsIDragSession> session = mCurrentParentDragSession;
    session.forget(aSession);
  } else {
    *aSession = nullptr;
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
nsIDragSession* nsBaseDragService::StartDragSession(
    nsISupports* aWidgetProvider) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!aWidgetProvider) {
    return nullptr;
  }
  if (mCurrentParentDragSession) {
    return mCurrentParentDragSession;
  }

  RefPtr<nsIDragSession> session = CreateDragSession();
  mCurrentParentDragSession = session;
  return session;
}

NS_IMETHODIMP
nsBaseDragSession::InitForTests(uint32_t aAllowedEffect) {
  mDragAction = aAllowedEffect;
  mEffectAllowedForTests = aAllowedEffect;
  mSessionIsSynthesizedForTests = true;
  return NS_OK;
}

NS_IMETHODIMP nsBaseDragService::StartDragSessionForTests(
    nsISupports* aWidgetProvider, uint32_t aAllowedEffect) {
  // This method must set mSessionIsSynthesizedForTests
  MOZ_ASSERT(!mNeverAllowSessionIsSynthesizedForTests);

  RefPtr<nsIDragSession> session = StartDragSession(aWidgetProvider);
  MOZ_ASSERT(session);
  session->InitForTests(aAllowedEffect);
  return NS_OK;
}

void nsBaseDragSession::OpenDragPopup() {
  if (mDragPopup) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      pm->ShowPopupAtScreen(mDragPopup, mScreenPosition.x - mImageOffset.x,
                            mScreenPosition.y - mImageOffset.y, false, nullptr);
    }
  }
}

int32_t nsBaseDragSession::TakeChildProcessDragAction() {
  // If the last event was dispatched to the child process, use the drag action
  // assigned from it instead and return it. DRAGDROP_ACTION_UNINITIALIZED is
  // returned otherwise.
  int32_t retval = nsIDragService::DRAGDROP_ACTION_UNINITIALIZED;
  if (TakeDragEventDispatchedToChildProcess() &&
      mDragActionFromChildProcess !=
          nsIDragService::DRAGDROP_ACTION_UNINITIALIZED) {
    retval = mDragActionFromChildProcess;
  }

  return retval;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsBaseDragSession::EndDragSession(bool aDoneDrag, uint32_t aKeyModifiers) {
  if (mDelayedDropTarget) {
    if (!mEndDragSessionData) {
      EndDragSessionData edsData = {aDoneDrag, aKeyModifiers};
      mEndDragSessionData = Some(edsData);
    }
    return NS_OK;
  }
  return EndDragSessionImpl(aDoneDrag, aKeyModifiers);
}

nsresult nsBaseDragSession::EndDragSessionImpl(bool aDoneDrag,
                                               uint32_t aKeyModifiers) {
  MOZ_DRAGSERVICE_LOG("[%p] EndDragSession | mDoingDrag %s", this,
                      mDoingDrag ? "true" : "false");
  if (!mDoingDrag || mEndingSession) {
    return NS_ERROR_FAILURE;
  }

  mEndingSession = true;

  if (aDoneDrag && !GetSuppressLevel()) {
    FireDragEventAtSource(eDragEnd, aKeyModifiers);
  }

  if (mDragPopup) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      pm->HidePopup(mDragPopup, {HidePopupOption::DeselectMenu});
    }
  }

  uint32_t dropEffect = nsIDragService::DRAGDROP_ACTION_NONE;
  if (mDataTransfer) {
    dropEffect = mDataTransfer->DropEffectInt();
  }

  for (nsWeakPtr& browser : mBrowsers) {
    nsCOMPtr<BrowserParent> bp = do_QueryReferent(browser);
    if (NS_WARN_IF(!bp)) {
      continue;
    }
    mozilla::Unused << bp->SendEndDragSession(
        aDoneDrag, mUserCancelled, mEndDragPoint, aKeyModifiers, dropEffect);
    // Continue sending input events with input priority when stopping the dnd
    // session.
    bp->Manager()->SetInputPriorityEventEnabled(true);
  }
  mBrowsers.Clear();

  // mDataTransfer and the items it owns are going to die anyway, but we
  // explicitly deref the contained data here so that we don't have to wait for
  // CC to reclaim the memory.
  if (XRE_IsParentProcess()) {
    DiscardInternalTransferData();
    nsCOMPtr<nsIDragService> svc =
        do_GetService("@mozilla.org/widget/dragservice;1");
    if (svc) {
      static_cast<nsBaseDragService*>(svc.get())
          ->ClearCurrentParentDragSession();
    }
  }

  mDoingDrag = false;
  mSessionIsSynthesizedForTests = false;
  mIsDraggingTextInTextControl = false;
  mEffectAllowedForTests = nsIDragService::DRAGDROP_ACTION_UNINITIALIZED;
  mEndingSession = false;
  mCanDrop = false;

  // release the source we've been holding on to.
  mSourceDocument = nullptr;
  mSourceNode = nullptr;
  mSourceWindowContext = nullptr;
  mSourceTopWindowContext = nullptr;
  mTriggeringPrincipal = nullptr;
  mCsp = nullptr;
  mSelection = nullptr;
  mDataTransfer = nullptr;
  mHasImage = false;
  mUserCancelled = false;
  mDragPopup = nullptr;
  mDragStartData = nullptr;
  mImage = nullptr;
  mImageOffset = CSSIntPoint();
  mScreenPosition = CSSIntPoint();
  mEndDragPoint = LayoutDeviceIntPoint(0, 0);
  mInputSource = MouseEvent_Binding::MOZ_SOURCE_MOUSE;
  mRegion = Nothing();

  return NS_OK;
}

void nsBaseDragSession::DiscardInternalTransferData() {
  if (mDataTransfer && mSourceNode) {
    MOZ_ASSERT(mDataTransfer);

    DataTransferItemList* items = mDataTransfer->Items();
    for (size_t i = 0; i < items->Length(); i++) {
      bool found;
      DataTransferItem* item = items->IndexedGetter(i, found);

      // Non-OTHER items may still be needed by JS. Skip them.
      if (!found || item->Kind() != DataTransferItem::KIND_OTHER) {
        continue;
      }

      nsCOMPtr<nsIVariant> variant = item->DataNoSecurityCheck();
      nsCOMPtr<nsIWritableVariant> writable = do_QueryInterface(variant);

      if (writable) {
        writable->SetAsEmpty();
      }
    }
  }
}

NS_IMETHODIMP
nsBaseDragSession::FireDragEventAtSource(EventMessage aEventMessage,
                                         uint32_t aKeyModifiers) {
  if (!mSourceNode || !mSourceDocument || GetSuppressLevel()) {
    return NS_OK;
  }
  RefPtr<PresShell> presShell = mSourceDocument->GetPresShell();
  if (!presShell) {
    return NS_OK;
  }

  RefPtr<nsPresContext> pc = presShell->GetPresContext();
  nsCOMPtr<nsIWidget> widget = pc ? pc->GetRootWidget() : nullptr;

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetDragEvent event(true, aEventMessage, widget);
  event.mFlags.mIsSynthesizedForTests = mSessionIsSynthesizedForTests;
  event.mInputSource = mInputSource;
  if (aEventMessage == eDragEnd) {
    event.mRefPoint = mEndDragPoint;
    if (widget) {
      event.mRefPoint -= widget->WidgetToTopLevelWidgetOffset();
    }
    event.mUserCancelled = mUserCancelled;
  }
  event.mModifiers = aKeyModifiers;

  // Most drag events aren't able to converted to MouseEvent except to
  // eDragStart and eDragEnd.
  if (widget && event.CanConvertToInputData()) {
    // Send the drag event to APZ, which needs to know about them to be
    // able to accurately detect the end of a drag gesture.
    widget->DispatchEventToAPZOnly(&event);
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(mSourceNode);
  return presShell->HandleDOMEventWithTarget(content, &event, &status);
}

/* This is used by Windows and Mac to update the position of a popup being
 * used as a drag image during the drag. This isn't used on GTK as it manages
 * the drag popup itself.
 */
NS_IMETHODIMP
nsBaseDragSession::DragMoved(int32_t aX, int32_t aY) {
  if (mDragPopup) {
    nsIFrame* frame = mDragPopup->GetPrimaryFrame();
    if (frame && frame->IsMenuPopupFrame()) {
      CSSIntPoint cssPos =
          RoundedToInt(LayoutDeviceIntPoint(aX, aY) /
                       frame->PresContext()->CSSToDevPixelScale()) -
          mImageOffset;
      static_cast<nsMenuPopupFrame*>(frame)->MoveTo(cssPos, true);
    }
  }

  return NS_OK;
}

static PresShell* GetPresShellForContent(nsINode* aDOMNode) {
  nsCOMPtr<nsIContent> content = do_QueryInterface(aDOMNode);
  if (!content) return nullptr;

  RefPtr<Document> document = content->GetComposedDoc();
  if (document) {
    document->FlushPendingNotifications(FlushType::Layout);
    return document->GetPresShell();
  }

  return nullptr;
}

nsresult nsBaseDragSession::DrawDrag(nsINode* aDOMNode,
                                     const Maybe<CSSIntRegion>& aRegion,
                                     CSSIntPoint aScreenPosition,
                                     LayoutDeviceIntRect* aScreenDragRect,
                                     RefPtr<SourceSurface>* aSurface,
                                     nsPresContext** aPresContext) {
  *aSurface = nullptr;
  *aPresContext = nullptr;

  // use a default size, in case of an error.
  aScreenDragRect->SetRect(aScreenPosition.x - mImageOffset.x,
                           aScreenPosition.y - mImageOffset.y, 1, 1);

  // if a drag image was specified, use that, otherwise, use the source node
  nsCOMPtr<nsINode> dragNode = mImage ? mImage.get() : aDOMNode;

  // get the presshell for the node being dragged. If the drag image is not in
  // a document or has no frame, get the presshell from the source drag node
  PresShell* presShell = GetPresShellForContent(dragNode);
  if (!presShell && mImage) {
    presShell = GetPresShellForContent(aDOMNode);
  }
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  *aPresContext = presShell->GetPresContext();

  if (mDragStartData) {
    if (mImage) {
      // Just clear the surface if chrome has overridden it with an image.
      *aSurface = nullptr;
    } else {
      *aSurface = mDragStartData->TakeVisualization(aScreenDragRect);
    }

    mDragStartData = nullptr;
    return NS_OK;
  }

  // convert mouse position to dev pixels of the prescontext
  const CSSIntPoint screenPosition = aScreenPosition - mImageOffset;
  const auto screenPoint = LayoutDeviceIntPoint::Round(
      screenPosition * (*aPresContext)->CSSToDevPixelScale());
  aScreenDragRect->MoveTo(screenPoint.x, screenPoint.y);

  // check if drag images are disabled
  bool enableDragImages = Preferences::GetBool(DRAGIMAGES_PREF, true);

  // didn't want an image, so just set the screen rectangle to the frame size
  if (!enableDragImages || !mHasImage) {
    // This holds a quantity in RelativeTo{presShell->GetRootFrame(),
    // ViewportType::Layout} space.
    nsRect presLayoutRect;
    if (aRegion) {
      // if a region was specified, set the screen rectangle to the area that
      // the region occupies
      presLayoutRect = ToAppUnits(aRegion->GetBounds(), AppUnitsPerCSSPixel());
    } else {
      // otherwise, there was no region so just set the rectangle to
      // the size of the primary frame of the content.
      nsCOMPtr<nsIContent> content = do_QueryInterface(dragNode);
      if (nsIFrame* frame = content->GetPrimaryFrame()) {
        presLayoutRect = frame->GetBoundingClientRect();
      }
    }

    LayoutDeviceRect screenVisualRect = ViewportUtils::ToScreenRelativeVisual(
        LayoutDeviceRect::FromAppUnits(presLayoutRect,
                                       (*aPresContext)->AppUnitsPerDevPixel()),
        *aPresContext);
    aScreenDragRect->SizeTo(screenVisualRect.Width(),
                            screenVisualRect.Height());
    return NS_OK;
  }

  // draw the image for selections
  if (mSelection) {
    LayoutDeviceIntPoint pnt(aScreenDragRect->TopLeft());
    *aSurface = presShell->RenderSelection(
        mSelection, pnt, aScreenDragRect,
        mImage ? RenderImageFlags::None : RenderImageFlags::AutoScale);
    return NS_OK;
  }

  // if a custom image was specified, check if it is an image node and draw
  // using the source rather than the displayed image. But if mImage isn't
  // an image or canvas, fall through to RenderNode below.
  if (mImage) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(dragNode);
    HTMLCanvasElement* canvas = HTMLCanvasElement::FromNodeOrNull(content);
    if (canvas) {
      return DrawDragForImage(*aPresContext, nullptr, canvas, aScreenDragRect,
                              aSurface);
    }

    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(dragNode);
    // for image nodes, create the drag image from the actual image data
    if (imageLoader) {
      return DrawDragForImage(*aPresContext, imageLoader, nullptr,
                              aScreenDragRect, aSurface);
    }

    // If the image is a popup, use that as the image. This allows custom drag
    // images that can change during the drag, but means that any platform
    // default image handling won't occur.
    // XXXndeakin this should be chrome-only

    nsIFrame* frame = content->GetPrimaryFrame();
    if (frame && frame->IsMenuPopupFrame()) {
      mDragPopup = content->AsElement();
    }
  }

  if (!mDragPopup) {
    // otherwise, just draw the node
    RenderImageFlags renderFlags =
        mImage ? RenderImageFlags::None : RenderImageFlags::AutoScale;
    if (renderFlags != RenderImageFlags::None) {
      // check if the dragged node itself is an img element
      if (dragNode->NodeName().LowerCaseEqualsLiteral("img")) {
        renderFlags = renderFlags | RenderImageFlags::IsImage;
      } else {
        nsINodeList* childList = dragNode->ChildNodes();
        uint32_t length = childList->Length();
        // check every childnode for being an img element
        // XXXbz why don't we need to check descendants recursively?
        for (uint32_t count = 0; count < length; ++count) {
          if (childList->Item(count)->NodeName().LowerCaseEqualsLiteral(
                  "img")) {
            // if the dragnode contains an image, set RenderImageFlags::IsImage
            // flag
            renderFlags = renderFlags | RenderImageFlags::IsImage;
            break;
          }
        }
      }
    }
    LayoutDeviceIntPoint pnt(aScreenDragRect->TopLeft());
    *aSurface = presShell->RenderNode(dragNode, aRegion, pnt, aScreenDragRect,
                                      renderFlags);
  }

  // If an image was specified, reset the position from the offset that was
  // supplied.
  if (mImage) {
    aScreenDragRect->MoveTo(screenPoint.x, screenPoint.y);
  }

  return NS_OK;
}

nsresult nsBaseDragSession::DrawDragForImage(
    nsPresContext* aPresContext, nsIImageLoadingContent* aImageLoader,
    HTMLCanvasElement* aCanvas, LayoutDeviceIntRect* aScreenDragRect,
    RefPtr<SourceSurface>* aSurface) {
  nsCOMPtr<imgIContainer> imgContainer;
  if (aImageLoader) {
    nsCOMPtr<imgIRequest> imgRequest;
    nsresult rv = aImageLoader->GetRequest(
        nsIImageLoadingContent::CURRENT_REQUEST, getter_AddRefs(imgRequest));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!imgRequest) return NS_ERROR_NOT_AVAILABLE;

    rv = imgRequest->GetImage(getter_AddRefs(imgContainer));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!imgContainer) return NS_ERROR_NOT_AVAILABLE;

    // use the size of the image as the size of the drag image
    int32_t imageWidth, imageHeight;
    rv = imgContainer->GetWidth(&imageWidth);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = imgContainer->GetHeight(&imageHeight);
    NS_ENSURE_SUCCESS(rv, rv);

    aScreenDragRect->SizeTo(aPresContext->CSSPixelsToDevPixels(imageWidth),
                            aPresContext->CSSPixelsToDevPixels(imageHeight));
  } else {
    // XXX The canvas size should be converted to dev pixels.
    NS_ASSERTION(aCanvas, "both image and canvas are null");
    nsIntSize sz = aCanvas->GetSize();
    aScreenDragRect->SizeTo(sz.width, sz.height);
  }

  nsIntSize destSize;
  destSize.width = aScreenDragRect->Width();
  destSize.height = aScreenDragRect->Height();
  if (destSize.width == 0 || destSize.height == 0) return NS_ERROR_FAILURE;

  nsresult result = NS_OK;
  if (aImageLoader) {
    RefPtr<DrawTarget> dt =
        gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
            destSize, SurfaceFormat::B8G8R8A8);
    if (!dt || !dt->IsValid()) return NS_ERROR_FAILURE;

    gfxContext ctx(dt);

    ImgDrawResult res = imgContainer->Draw(
        &ctx, destSize, ImageRegion::Create(destSize),
        imgIContainer::FRAME_CURRENT, SamplingFilter::GOOD, SVGImageContext(),
        imgIContainer::FLAG_SYNC_DECODE, 1.0);
    if (res == ImgDrawResult::BAD_IMAGE || res == ImgDrawResult::BAD_ARGS ||
        res == ImgDrawResult::NOT_SUPPORTED) {
      return NS_ERROR_FAILURE;
    }
    *aSurface = dt->Snapshot();
  } else {
    *aSurface = aCanvas->GetSurfaceSnapshot();
  }

  return result;
}

NS_IMETHODIMP
nsBaseDragService::Suppress() {
  RefPtr<nsIDragSession> session = mCurrentParentDragSession;
  if (session) {
    session->EndDragSession(false, 0);
  }
  ++mSuppressLevel;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragService::Unsuppress() {
  --mSuppressLevel;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::UserCancelled() {
  mUserCancelled = true;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::UpdateDragEffect() {
  mDragActionFromChildProcess = mDragAction;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::UpdateDragImage(nsINode* aImage, int32_t aImageX,
                                   int32_t aImageY) {
  // Don't change the image if this is a drag from another source or if there
  // is a drag popup.
  if (!mSourceNode || mDragPopup) return NS_OK;

  mImage = aImage;
  mImageOffset = CSSIntPoint(aImageX, aImageY);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::DragEventDispatchedToChildProcess() {
  mDragEventDispatchedToChildProcess = true;
  return NS_OK;
}

static bool MaybeAddBrowser(nsTArray<nsWeakPtr>& aBrowsers,
                            BrowserParent* aBP) {
  nsWeakPtr browser = do_GetWeakReference(aBP);

  // Equivalent to `InsertElementSorted`, avoiding inserting a duplicate
  // element. See bug 1896166.
  size_t index = aBrowsers.IndexOfFirstElementGt(browser);
  if (index == 0 || aBrowsers[index - 1] != browser) {
    aBrowsers.InsertElementAt(index, browser);
    return true;
  }
  return false;
}

static bool RemoveAllBrowsers(nsTArray<nsWeakPtr>& aBrowsers) {
  for (auto& weakBrowser : aBrowsers) {
    nsCOMPtr<BrowserParent> browser = do_QueryReferent(weakBrowser);
    if (NS_WARN_IF(!browser)) {
      continue;
    }
    mozilla::Unused << browser->SendEndDragSession(
        true, false, LayoutDeviceIntPoint(), 0,
        nsIDragService::DRAGDROP_ACTION_NONE);
  }

  aBrowsers.Clear();
  return true;
}

bool nsBaseDragService::MaybeAddBrowser(BrowserParent* aBP) {
  nsCOMPtr<nsIDragSession> session;
  GetCurrentSession(nullptr, getter_AddRefs(session));
  if (session) {
    return session->MaybeAddBrowser(aBP);
  }
  return ::MaybeAddBrowser(mBrowsers, aBP);
}

bool nsBaseDragService::RemoveAllBrowsers() {
  nsCOMPtr<nsIDragSession> session;
  GetCurrentSession(nullptr, getter_AddRefs(session));
  if (session) {
    return session->RemoveAllBrowsers();
  }
  return ::RemoveAllBrowsers(mBrowsers);
}

bool nsBaseDragSession::MaybeAddBrowser(BrowserParent* aBP) {
  return ::MaybeAddBrowser(mBrowsers, aBP);
}

bool nsBaseDragSession::RemoveAllBrowsers() {
  return ::RemoveAllBrowsers(mBrowsers);
}

bool nsBaseDragSession::MustUpdateDataTransfer(EventMessage aMessage) {
  return false;
}

NS_IMETHODIMP
nsBaseDragSession::MaybeEditorDeletedSourceNode(Element* aEditingHost) {
  // If builtin editor of Blink and WebKit deletes the source node,they retarget
  // the source node to the editing host.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/page/drag_controller.cc;l=724;drc=d9ba13b8cd8ac0faed7afc3d1f7e4b67ebac2a0b
  // That allows editor apps listens to "dragend" event in editing host or its
  // ancestors.  Therefore, we should follow them for compatibility.
  if (mSourceNode && !mSourceNode->IsInComposedDoc()) {
    mSourceNode = aEditingHost;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragService::GetMockDragController(
    nsIMockDragServiceController** aController) {
#ifdef ENABLE_TESTS
  if (XRE_IsContentProcess()) {
    // The mock drag controller is only available in the parent process.
    MOZ_ASSERT(!XRE_IsContentProcess());
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (!mMockController) {
    mMockController = new mozilla::test::MockDragServiceController();
  }
  auto controller = mMockController;
  controller.forget(aController);
  return NS_OK;
#else
  *aController = nullptr;
  MOZ_ASSERT(false, "CreateMockDragController may only be called for testing");
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP
nsBaseDragService::GetNeverAllowSessionIsSynthesizedForTests(
    bool* aNeverAllow) {
  *aNeverAllow = mNeverAllowSessionIsSynthesizedForTests;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragService::SetNeverAllowSessionIsSynthesizedForTests(bool aNeverAllow) {
  mNeverAllowSessionIsSynthesizedForTests = aNeverAllow;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::SetDragEndPoint(int32_t aScreenX, int32_t aScreenY) {
  SetDragEndPoint(LayoutDeviceIntPoint(aScreenX, aScreenY));
  return NS_OK;
}

void nsBaseDragSession::TakeSessionBrowserListFromService() {
  nsCOMPtr<nsIDragService> svc =
      do_GetService("@mozilla.org/widget/dragservice;1");
  NS_ENSURE_TRUE_VOID(svc);
  mBrowsers =
      static_cast<nsBaseDragService*>(svc.get())->TakeSessionBrowserList();
}

/* static */
nsIWidget* nsBaseDragService::GetWidgetFromWidgetProvider(
    nsISupports* aWidgetProvider) {
  nsCOMPtr<nsIWidget> widget = do_QueryObject(aWidgetProvider);
  if (widget) {
    return widget;
  }

  nsPIDOMWindowOuter* outer;
  if (aWidgetProvider) {
    nsCOMPtr<mozIDOMWindow> window = do_GetInterface(aWidgetProvider);
    NS_ENSURE_TRUE(window, nullptr);
    RefPtr<nsPIDOMWindowInner> innerWin = nsGlobalWindowInner::Cast(window);
    NS_ENSURE_TRUE(innerWin, nullptr);
    outer = innerWin->GetOuterWindow();
  } else {
    nsCOMPtr<nsPIDOMWindowInner> winInner;
    winInner = do_QueryInterface(GetEntryGlobal());
    NS_ENSURE_TRUE(winInner, nullptr);
    outer = winInner->GetOuterWindow();
  }
  NS_ENSURE_TRUE(outer, nullptr);
  nsIDocShell* docShell = outer->GetDocShell();
  NS_ENSURE_TRUE(docShell, nullptr);
  PresShell* presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, nullptr);
  nsViewManager* vm = presShell->GetViewManager();
  NS_ENSURE_TRUE(vm, nullptr);
  return vm->GetRootWidget();
}

NS_IMETHODIMP
nsBaseDragSession::UriForEvent(DragEvent* aEvent, nsIURI** aUri) {
  MOZ_ASSERT(XRE_IsParentProcess());
  *aUri = nullptr;
  auto* widgetEvent = aEvent->WidgetEventPtr();
  MOZ_ASSERT(widgetEvent);
  auto* bp =
      dom::BrowserParent::GetBrowserParentFromLayersId(widgetEvent->mLayersId);
  NS_ENSURE_TRUE(bp, NS_ERROR_FAILURE);
  auto* bc = bp->GetBrowsingContext();
  NS_ENSURE_TRUE(bc, NS_ERROR_FAILURE);
  RefPtr<nsIURI> ret = bc->GetCurrentURI();
  ret.forget(aUri);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::SendStoreDropTargetAndDelayEndDragSession(
    DragEvent* aEvent) {
  mDelayedDropBrowserParent = dom::BrowserParent::GetBrowserParentFromLayersId(
      aEvent->WidgetEventPtr()->mLayersId);
  NS_ENSURE_TRUE(mDelayedDropBrowserParent, NS_ERROR_FAILURE);
  uint32_t dropEffect = nsIDragService::DRAGDROP_ACTION_NONE;
  if (mDataTransfer) {
    dropEffect = mDataTransfer->DropEffectInt();
  }
  Unused
      << mDelayedDropBrowserParent->SendStoreDropTargetAndDelayEndDragSession(
             aEvent->WidgetEventPtr()->mRefPoint, dropEffect, mDragAction,
             mTriggeringPrincipal, mCsp);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::SendDispatchToDropTargetAndResumeEndDragSession(
    bool aShouldDrop) {
  MOZ_ASSERT(mDelayedDropBrowserParent);
  Unused << mDelayedDropBrowserParent
                ->SendDispatchToDropTargetAndResumeEndDragSession(aShouldDrop);
  mDelayedDropBrowserParent = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::StoreDropTargetAndDelayEndDragSession(
    mozilla::dom::Element* aElement, nsIFrame* aFrame) {
  MOZ_ASSERT(XRE_IsContentProcess());
  MOZ_DRAGSERVICE_LOG(
      "[%p] StoreDropTargetAndDelayEndDragSession | aElement: %p | aFrame: %p",
      this, aElement, aFrame);
  mDelayedDropTarget = do_GetWeakReference(aElement);
  mDelayedDropFrame = aFrame;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragSession::DispatchToDropTargetAndResumeEndDragSession(
    nsIWidget* aWidget, const LayoutDeviceIntPoint& aPt, bool aShouldDrop) {
  MOZ_ASSERT(XRE_IsContentProcess());
  MOZ_DRAGSERVICE_LOG(
      "[%p] DispatchToDropTargetAndResumeEndDragSession | pt=(%d, %d) | "
      "shouldDrop: %s",
      this, static_cast<int32_t>(aPt.x), static_cast<int32_t>(aPt.y),
      aShouldDrop ? "true" : "false");

  RefPtr<Element> delayedDropTarget = do_QueryReferent(mDelayedDropTarget);
  mDelayedDropTarget = nullptr;
  nsIFrame* delayedDropFrame = mDelayedDropFrame;
  mDelayedDropFrame = nullptr;
  auto edsData = std::move(mEndDragSessionData);

  if (!delayedDropTarget) {
    MOZ_ASSERT(!edsData && !delayedDropFrame);
    return NS_OK;
  }
  if (!delayedDropFrame) {
    // Weak frame was deleted
    return NS_OK;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  RefPtr<PresShell> ps = delayedDropFrame->PresContext()->GetPresShell();
  auto event = MakeUnique<WidgetDragEvent>(
      true, aShouldDrop ? eDrop : eDragExit, aWidget);
  event->mRefPoint = aPt;
  ps->HandleEventWithTarget(event.get(), delayedDropFrame, delayedDropTarget,
                            &status);

  // If EndDragSession was delayed, issue it now.
  if (edsData) {
    EndDragSession(edsData->mDoneDrag, edsData->mKeyModifiers);
  }
  return NS_OK;
}
