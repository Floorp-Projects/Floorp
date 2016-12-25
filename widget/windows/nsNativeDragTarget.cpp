/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "nsIDragService.h"
#include "nsWidgetsCID.h"
#include "nsNativeDragTarget.h"
#include "nsDragService.h"
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"

#include "nsIWidget.h"
#include "nsWindow.h"
#include "nsClipboard.h"
#include "KeyboardLayout.h"

#include "mozilla/MouseEvents.h"

using namespace mozilla;
using namespace mozilla::widget;

/* Define Interface IDs */
static NS_DEFINE_IID(kIDragServiceIID, NS_IDRAGSERVICE_IID);

// This is cached for Leave notification
static POINTL gDragLastPoint;

bool nsNativeDragTarget::gDragImageChanged = false;

/*
 * class nsNativeDragTarget
 */
nsNativeDragTarget::nsNativeDragTarget(nsIWidget * aWidget)
  : m_cRef(0), 
    mEffectsAllowed(DROPEFFECT_MOVE | DROPEFFECT_COPY | DROPEFFECT_LINK),
    mEffectsPreferred(DROPEFFECT_NONE),
    mTookOwnRef(false), mWidget(aWidget), mDropTargetHelper(nullptr)
{
  static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

  mHWnd = (HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW);

  /*
   * Create/Get the DragService that we have implemented
   */
  CallGetService(kCDragServiceCID, &mDragService);
}

nsNativeDragTarget::~nsNativeDragTarget()
{
  NS_RELEASE(mDragService);

  if (mDropTargetHelper) {
    mDropTargetHelper->Release();
    mDropTargetHelper = nullptr;
  }
}

// IUnknown methods - see iunknown.h for documentation
STDMETHODIMP
nsNativeDragTarget::QueryInterface(REFIID riid, void** ppv)
{
  *ppv=nullptr;

  if (IID_IUnknown == riid || IID_IDropTarget == riid)
    *ppv=this;

  if (nullptr!=*ppv) {
    ((LPUNKNOWN)*ppv)->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
nsNativeDragTarget::AddRef(void)
{
  ++m_cRef;
  NS_LOG_ADDREF(this, m_cRef, "nsNativeDragTarget", sizeof(*this));
  return m_cRef;
}

STDMETHODIMP_(ULONG) nsNativeDragTarget::Release(void)
{
  --m_cRef;
  NS_LOG_RELEASE(this, m_cRef, "nsNativeDragTarget");
  if (0 != m_cRef)
    return m_cRef;

  delete this;
  return 0;
}

void
nsNativeDragTarget::GetGeckoDragAction(DWORD grfKeyState, LPDWORD pdwEffect,
                                       uint32_t * aGeckoAction)
{
  // If a window is disabled or a modal window is on top of it
  // (which implies it is disabled), then we should not allow dropping.
  if (!mWidget->IsEnabled()) {
    *pdwEffect = DROPEFFECT_NONE;
    *aGeckoAction = nsIDragService::DRAGDROP_ACTION_NONE;
    return;
  }

  // If the user explicitly uses a modifier key, they want the associated action
  // Shift + Control -> LINK, Shift -> MOVE, Ctrl -> COPY
  DWORD desiredEffect = DROPEFFECT_NONE;
  if ((grfKeyState & MK_CONTROL) && (grfKeyState & MK_SHIFT)) {
    desiredEffect = DROPEFFECT_LINK;
  } else if (grfKeyState & MK_SHIFT) {
    desiredEffect = DROPEFFECT_MOVE;
  } else if (grfKeyState & MK_CONTROL) {
    desiredEffect = DROPEFFECT_COPY;
  }

  // Determine the desired effect from what is allowed and preferred.
  if (!(desiredEffect &= mEffectsAllowed)) {
    // No modifier key effect is set which is also allowed, check
    // the preference of the data.
    desiredEffect = mEffectsPreferred & mEffectsAllowed;
    if (!desiredEffect) {
      // No preference is set, so just fall back to the allowed effect itself
      desiredEffect = mEffectsAllowed;
    }
  }

  // Otherwise we should specify the first available effect 
  // from MOVE, COPY, or LINK.
  if (desiredEffect & DROPEFFECT_MOVE) {
    *pdwEffect = DROPEFFECT_MOVE;
    *aGeckoAction = nsIDragService::DRAGDROP_ACTION_MOVE;
  } else if (desiredEffect & DROPEFFECT_COPY) {
    *pdwEffect = DROPEFFECT_COPY;
    *aGeckoAction = nsIDragService::DRAGDROP_ACTION_COPY;
  } else if (desiredEffect & DROPEFFECT_LINK) {
    *pdwEffect = DROPEFFECT_LINK;
    *aGeckoAction = nsIDragService::DRAGDROP_ACTION_LINK;
  } else {
    *pdwEffect = DROPEFFECT_NONE;
    *aGeckoAction = nsIDragService::DRAGDROP_ACTION_NONE;
  } 
}

inline
bool
IsKeyDown(char key)
{
  return GetKeyState(key) < 0;
}

void
nsNativeDragTarget::DispatchDragDropEvent(EventMessage aEventMessage,
                                          const POINTL& aPT)
{
  nsEventStatus status;
  WidgetDragEvent event(true, aEventMessage, mWidget);

  nsWindow * win = static_cast<nsWindow *>(mWidget);
  win->InitEvent(event);
  POINT cpos;

  cpos.x = aPT.x;
  cpos.y = aPT.y;

  if (mHWnd != nullptr) {
    ::ScreenToClient(mHWnd, &cpos);
    event.mRefPoint = LayoutDeviceIntPoint(cpos.x, cpos.y);
  } else {
    event.mRefPoint = LayoutDeviceIntPoint(0, 0);
  }

  ModifierKeyState modifierKeyState;
  modifierKeyState.InitInputEvent(event);

  event.inputSource = static_cast<nsBaseDragService*>(mDragService)->GetInputSource();

  mWidget->DispatchEvent(&event, status);
}

void
nsNativeDragTarget::ProcessDrag(EventMessage aEventMessage,
                                DWORD        grfKeyState,
                                POINTL       ptl,
                                DWORD*       pdwEffect)
{
  // Before dispatching the event make sure we have the correct drop action set
  uint32_t geckoAction;
  GetGeckoDragAction(grfKeyState, pdwEffect, &geckoAction);

  // Set the current action into the Gecko specific type
  nsCOMPtr<nsIDragSession> currSession;
  mDragService->GetCurrentSession(getter_AddRefs(currSession));
  if (!currSession) {
    return;
  }

  currSession->SetDragAction(geckoAction);

  // Dispatch the event into Gecko
  DispatchDragDropEvent(aEventMessage, ptl);

  // If TakeChildProcessDragAction returns something other than
  // DRAGDROP_ACTION_UNINITIALIZED, it means that the last event was sent
  // to the child process and this event is also being sent to the child
  // process. In this case, use the last event's action instead.
  nsDragService* dragService = static_cast<nsDragService *>(mDragService);
  currSession->GetDragAction(&geckoAction);

  int32_t childDragAction = dragService->TakeChildProcessDragAction();
  if (childDragAction != nsIDragService::DRAGDROP_ACTION_UNINITIALIZED) {
    geckoAction = childDragAction;
  }

  if (nsIDragService::DRAGDROP_ACTION_LINK & geckoAction) {
    *pdwEffect = DROPEFFECT_LINK;
  }
  else if (nsIDragService::DRAGDROP_ACTION_COPY & geckoAction) {
    *pdwEffect = DROPEFFECT_COPY;
  }
  else if (nsIDragService::DRAGDROP_ACTION_MOVE & geckoAction) {
    *pdwEffect = DROPEFFECT_MOVE;
  }
  else {
    *pdwEffect = DROPEFFECT_NONE;
  }

  if (aEventMessage != eDrop) {
    // Get the cached drag effect from the drag service, the data member should
    // have been set by whoever handled the WidgetGUIEvent or nsIDOMEvent on
    // drags.
    bool canDrop;
    currSession->GetCanDrop(&canDrop);
    if (!canDrop) {
      *pdwEffect = DROPEFFECT_NONE;
    }
  }

  // Clear the cached value
  currSession->SetCanDrop(false);
}

// IDropTarget methods
STDMETHODIMP
nsNativeDragTarget::DragEnter(LPDATAOBJECT pIDataSource,
                              DWORD        grfKeyState,
                              POINTL       ptl,
                              DWORD*       pdwEffect)
{
  if (!mDragService) {
    return E_FAIL;
  }

  mEffectsAllowed = *pdwEffect;
  AddLinkSupportIfCanBeGenerated(pIDataSource);

  // Drag and drop image helper
  if (GetDropTargetHelper()) {
    POINT pt = { ptl.x, ptl.y };
    GetDropTargetHelper()->DragEnter(mHWnd, pIDataSource, &pt, *pdwEffect);
  }

  // save a ref to this, in case the window is destroyed underneath us
  NS_ASSERTION(!mTookOwnRef, "own ref already taken!");
  this->AddRef();
  mTookOwnRef = true;

  // tell the drag service about this drag (it may have come from an
  // outside app).
  mDragService->StartDragSession();

  void* tempOutData = nullptr;
  uint32_t tempDataLen = 0;
  nsresult loadResult = nsClipboard::GetNativeDataOffClipboard(
      pIDataSource, 0, ::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT), nullptr, &tempOutData, &tempDataLen);
  if (NS_SUCCEEDED(loadResult) && tempOutData) {
    mEffectsPreferred = *((DWORD*)tempOutData);
    free(tempOutData);
  } else {
    // We have no preference if we can't obtain it
    mEffectsPreferred = DROPEFFECT_NONE;
  }

  // Set the native data object into drag service
  //
  // This cast is ok because in the constructor we created a
  // the actual implementation we wanted, so we know this is
  // a nsDragService. It should be a private interface, though.
  nsDragService * winDragService =
    static_cast<nsDragService *>(mDragService);
  winDragService->SetIDataObject(pIDataSource);

  // Now process the native drag state and then dispatch the event
  ProcessDrag(eDragEnter, grfKeyState, ptl, pdwEffect);

  return S_OK;
}

void 
nsNativeDragTarget::AddLinkSupportIfCanBeGenerated(LPDATAOBJECT aIDataSource) 
{
  // If we don't have a link effect, but we can generate one, fix the 
  // drop effect to include it.
  if (!(mEffectsAllowed & DROPEFFECT_LINK) && aIDataSource) {
    if (S_OK == ::OleQueryLinkFromData(aIDataSource)) {
      mEffectsAllowed |= DROPEFFECT_LINK;
    }
  }
}

STDMETHODIMP
nsNativeDragTarget::DragOver(DWORD   grfKeyState,
                             POINTL  ptl,
                             LPDWORD pdwEffect)
{
  if (!mDragService) {
    return E_FAIL;
  }

  bool dragImageChanged = gDragImageChanged;
  gDragImageChanged = false;

  // If a LINK effect could be generated previously from a DragEnter(),
  // then we should include it as an allowed effect.
  mEffectsAllowed = (*pdwEffect) | (mEffectsAllowed & DROPEFFECT_LINK);

  nsCOMPtr<nsIDragSession> currentDragSession;
  mDragService->GetCurrentSession(getter_AddRefs(currentDragSession));
  if (!currentDragSession) {
    return S_OK;  // Drag was canceled.
  }

  // without the AddRef() |this| can get destroyed in an event handler
  this->AddRef();

  // Drag and drop image helper
  if (GetDropTargetHelper()) {
    if (dragImageChanged) {
      // The drop helper only updates the image during DragEnter, so emulate
      // a DragEnter if the image was changed.
      POINT pt = { ptl.x, ptl.y };
      nsDragService* dragService = static_cast<nsDragService *>(mDragService);
      GetDropTargetHelper()->DragEnter(mHWnd, dragService->GetDataObject(), &pt, *pdwEffect);

    }
    POINT pt = { ptl.x, ptl.y };
    GetDropTargetHelper()->DragOver(&pt, *pdwEffect);
  }

  mDragService->FireDragEventAtSource(eDrag);
  // Now process the native drag state and then dispatch the event
  ProcessDrag(eDragOver, grfKeyState, ptl, pdwEffect);

  this->Release();

  return S_OK;
}

STDMETHODIMP
nsNativeDragTarget::DragLeave()
{
  if (!mDragService) {
    return E_FAIL;
  }

  // Drag and drop image helper
  if (GetDropTargetHelper()) {
    GetDropTargetHelper()->DragLeave();
  }

  // dispatch the event into Gecko
  DispatchDragDropEvent(eDragExit, gDragLastPoint);

  nsCOMPtr<nsIDragSession> currentDragSession;
  mDragService->GetCurrentSession(getter_AddRefs(currentDragSession));

  if (currentDragSession) {
    nsCOMPtr<nsIDOMNode> sourceNode;
    currentDragSession->GetSourceNode(getter_AddRefs(sourceNode));

    if (!sourceNode) {
      // We're leaving a window while doing a drag that was
      // initiated in a different app. End the drag session, since
      // we're done with it for now (until the user drags back into
      // mozilla).
      mDragService->EndDragSession(false);
    }
  }

  // release the ref that was taken in DragEnter
  NS_ASSERTION(mTookOwnRef, "want to release own ref, but not taken!");
  if (mTookOwnRef) {
    this->Release();
    mTookOwnRef = false;
  }

  return S_OK;
}

void
nsNativeDragTarget::DragCancel()
{
  // Cancel the drag session if we did DragEnter.
  if (mTookOwnRef) {
    if (GetDropTargetHelper()) {
      GetDropTargetHelper()->DragLeave();
    }
    if (mDragService) {
      mDragService->EndDragSession(false);
    }
    this->Release(); // matching the AddRef in DragEnter
    mTookOwnRef = false;
  }
}

STDMETHODIMP
nsNativeDragTarget::Drop(LPDATAOBJECT pData,
                         DWORD        grfKeyState,
                         POINTL       aPT,
                         LPDWORD      pdwEffect)
{
  if (!mDragService) {
    return E_FAIL;
  }

  mEffectsAllowed = *pdwEffect;
  AddLinkSupportIfCanBeGenerated(pData);

  // Drag and drop image helper
  if (GetDropTargetHelper()) {
    POINT pt = { aPT.x, aPT.y };
    GetDropTargetHelper()->Drop(pData, &pt, *pdwEffect);
  }

  // Set the native data object into the drag service
  //
  // This cast is ok because in the constructor we created a
  // the actual implementation we wanted, so we know this is
  // a nsDragService (but it should still be a private interface)
  nsDragService* winDragService = static_cast<nsDragService*>(mDragService);
  winDragService->SetIDataObject(pData);

  // NOTE: ProcessDrag spins the event loop which may destroy arbitrary objects.
  // We use strong refs to prevent it from destroying these:
  RefPtr<nsNativeDragTarget> kungFuDeathGrip = this;
  nsCOMPtr<nsIDragService> serv = mDragService;

  // Now process the native drag state and then dispatch the event
  ProcessDrag(eDrop, grfKeyState, aPT, pdwEffect);

  nsCOMPtr<nsIDragSession> currentDragSession;
  serv->GetCurrentSession(getter_AddRefs(currentDragSession));
  if (!currentDragSession) {
    return S_OK;  // DragCancel() was called.
  }

  // Let the win drag service know whether this session experienced 
  // a drop event within the application. Drop will not oocur if the
  // drop landed outside the app. (used in tab tear off, bug 455884)
  winDragService->SetDroppedLocal();

  // tell the drag service we're done with the session
  // Use GetMessagePos to get the position of the mouse at the last message
  // seen by the event loop. (Bug 489729)
  DWORD pos = ::GetMessagePos();
  POINT cpos;
  cpos.x = GET_X_LPARAM(pos);
  cpos.y = GET_Y_LPARAM(pos);
  winDragService->SetDragEndPoint(nsIntPoint(cpos.x, cpos.y));
  serv->EndDragSession(true);

  // release the ref that was taken in DragEnter
  NS_ASSERTION(mTookOwnRef, "want to release own ref, but not taken!");
  if (mTookOwnRef) {
    this->Release();
    mTookOwnRef = false;
  }

  return S_OK;
}

/**
 * By lazy loading mDropTargetHelper we save 50-70ms of startup time
 * which is ~5% of startup time.
*/
IDropTargetHelper*
nsNativeDragTarget::GetDropTargetHelper()
{
  if (!mDropTargetHelper) { 
    CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER,
                     IID_IDropTargetHelper, (LPVOID*)&mDropTargetHelper);
  }

  return mDropTargetHelper;
}
