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
 * The Original Code is Mozilla Communicator client code.
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

/* Define Class IDs */
static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kIDragServiceIID, NS_IDRAGSERVICE_IID);

// This is cached for Leave notification
static POINTL gDragLastPoint;

/*
 * class nsNativeDragTarget
 */
nsNativeDragTarget::nsNativeDragTarget(nsIWidget * aWnd)
  : m_cRef(0), mEffect(DROPEFFECT_MOVE | DROPEFFECT_COPY | DROPEFFECT_LINK), 
    mTookOwnRef(PR_FALSE), mWindow(aWnd), mDropTargetHelper(nsnull)
{
  mHWnd = (HWND)mWindow->GetNativeData(NS_NATIVE_WINDOW);

  /*
   * Create/Get the DragService that we have implemented
   */
  CallGetService(kCDragServiceCID, &mDragService);

  // Drag target helper for drag image support
  CoCreateInstance(CLSID_DragDropHelper, NULL, CLSCTX_INPROC_SERVER,
                   IID_IDropTargetHelper, (LPVOID*)&mDropTargetHelper);
}

nsNativeDragTarget::~nsNativeDragTarget()
{
  NS_RELEASE(mDragService);

  if (mDropTargetHelper) {
    mDropTargetHelper->Release();
    mDropTargetHelper = nsnull;
  }
}

// IUnknown methods - see iunknown.h for documentation
STDMETHODIMP
nsNativeDragTarget::QueryInterface(REFIID riid, void** ppv)
{
  *ppv=NULL;

  if (IID_IUnknown == riid || IID_IDropTarget == riid)
    *ppv=this;

  if (NULL!=*ppv) {
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
                                       PRUint32 * aGeckoAction)
{  
  // Default is move if we can, in fact drop here,
  // and if the drop source supports a move operation.
  // If move is not preferred (mMovePreferred is false)
  // move only when the shift key is down.
  if ((mEffect & DROPEFFECT_MOVE) && 
      (mMovePreferred || (grfKeyState & MK_SHIFT))) {
    *aGeckoAction = nsIDragService::DRAGDROP_ACTION_MOVE;
    *pdwEffect    = DROPEFFECT_MOVE;
  } 
  else if (!(mEffect & DROPEFFECT_MOVE) && 
           !(mEffect & DROPEFFECT_COPY) && 
           (mEffect & DROPEFFECT_LINK)) {
    *aGeckoAction = nsIDragService::DRAGDROP_ACTION_LINK;
    *pdwEffect    = DROPEFFECT_LINK;
  }
  else {
    *aGeckoAction = nsIDragService::DRAGDROP_ACTION_COPY;
    *pdwEffect    = DROPEFFECT_COPY;
  }

  // Given the key modifiers figure out what state we are in for both
  // the native system and Gecko
  if (grfKeyState & MK_CONTROL) {
    if ((mEffect & DROPEFFECT_LINK) && (grfKeyState & MK_SHIFT)) {
      *aGeckoAction = nsIDragService::DRAGDROP_ACTION_LINK;
      *pdwEffect    = DROPEFFECT_LINK;
    } else {
      *aGeckoAction = nsIDragService::DRAGDROP_ACTION_COPY;
      *pdwEffect    = DROPEFFECT_COPY;
    }
  }
}

inline
PRBool
IsKeyDown(char key)
{
  return GetKeyState(key) < 0;
}

void
nsNativeDragTarget::DispatchDragDropEvent(PRUint32 aEventType, POINTL aPT)
{
  nsEventStatus status;
  nsDragEvent event(PR_TRUE, aEventType, mWindow);

  nsWindow * win = static_cast<nsWindow *>(mWindow);
  win->InitEvent(event);
  POINT cpos;

  cpos.x = aPT.x;
  cpos.y = aPT.y;

  if (mHWnd != NULL) {
    ::ScreenToClient(mHWnd, &cpos);
    event.refPoint.x = cpos.x;
    event.refPoint.y = cpos.y;
  } else {
    event.refPoint.x = 0;
    event.refPoint.y = 0;
  }

  event.isShift   = IsKeyDown(NS_VK_SHIFT);
  event.isControl = IsKeyDown(NS_VK_CONTROL);
  event.isMeta    = PR_FALSE;
  event.isAlt     = IsKeyDown(NS_VK_ALT);
  event.inputSource = static_cast<nsBaseDragService*>(mDragService)->GetInputSource();

  mWindow->DispatchEvent(&event, status);
}

void
nsNativeDragTarget::ProcessDrag(PRUint32     aEventType,
                                DWORD        grfKeyState,
                                POINTL       ptl,
                                DWORD*       pdwEffect)
{
  // Before dispatching the event make sure we have the correct drop action set
  PRUint32 geckoAction;
  GetGeckoDragAction(grfKeyState, pdwEffect, &geckoAction);

  // Set the current action into the Gecko specific type
  nsCOMPtr<nsIDragSession> currSession;
  mDragService->GetCurrentSession(getter_AddRefs(currSession));
  if (!currSession) {
    return;
  }

  currSession->SetDragAction(geckoAction);

  // Dispatch the event into Gecko
  DispatchDragDropEvent(aEventType, ptl);

  // Now get the cached Drag effect from the drag service
  // the data memeber should have been set by who ever handled the
  // nsGUIEvent or nsIDOMEvent
  PRBool canDrop;
  currSession->GetCanDrop(&canDrop);
  if (!canDrop)
    *pdwEffect = DROPEFFECT_NONE;

  // Clear the cached value
  currSession->SetCanDrop(PR_FALSE);
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

  // Drag and drop image helper
  if (mDropTargetHelper) {
    POINT pt = { ptl.x, ptl.y };
    mDropTargetHelper->DragEnter(mHWnd, pIDataSource, &pt, *pdwEffect);
  }

  // save a ref to this, in case the window is destroyed underneath us
  NS_ASSERTION(!mTookOwnRef, "own ref already taken!");
  this->AddRef();
  mTookOwnRef = PR_TRUE;

  // tell the drag service about this drag (it may have come from an
  // outside app).
  mDragService->StartDragSession();

  mEffect = *pdwEffect;
  // If we don't have a link effect, but we can generate one, fix the 
  // drop effect to include it
  if (!(mEffect & DROPEFFECT_LINK) && pIDataSource) {
    if (S_OK == ::OleQueryLinkFromData(pIDataSource)) {
      mEffect |= DROPEFFECT_LINK;
    }
  }

  void* tempOutData = nsnull;
  PRUint32 tempDataLen = 0;
  nsresult loadResult = nsClipboard::GetNativeDataOffClipboard(
      pIDataSource, 0, ::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT), nsnull, &tempOutData, &tempDataLen);
  if (NS_SUCCEEDED(loadResult) && tempOutData) {
    NS_ASSERTION(tempDataLen == 2, "Expected word size");
    WORD preferredEffect = *((WORD*)tempOutData);

    // Mask effect coming from function call with effect preferred by the source.
    mMovePreferred = (preferredEffect & DROPEFFECT_MOVE) != 0;
  }
  else
    mMovePreferred = (mEffect & DROPEFFECT_MOVE) != 0;

  // Set the native data object into drag service
  //
  // This cast is ok because in the constructor we created a
  // the actual implementation we wanted, so we know this is
  // a nsDragService. It should be a private interface, though.
  nsDragService * winDragService =
    static_cast<nsDragService *>(mDragService);
  winDragService->SetIDataObject(pIDataSource);

  // Now process the native drag state and then dispatch the event
  ProcessDrag(NS_DRAGDROP_ENTER, grfKeyState, ptl, pdwEffect);

  return S_OK;
}

STDMETHODIMP
nsNativeDragTarget::DragOver(DWORD   grfKeyState,
                             POINTL  ptl,
                             LPDWORD pdwEffect)
{
  if (!mDragService) {
    return E_FAIL;
  }

  nsCOMPtr<nsIDragSession> currentDragSession;
  mDragService->GetCurrentSession(getter_AddRefs(currentDragSession));
  if (!currentDragSession) {
    return S_OK;  // Drag was canceled.
  }

  // without the AddRef() |this| can get destroyed in an event handler
  this->AddRef();

  // Drag and drop image helper
  if (mDropTargetHelper) {
    POINT pt = { ptl.x, ptl.y };
    mDropTargetHelper->DragOver(&pt, *pdwEffect);
  }

  mDragService->FireDragEventAtSource(NS_DRAGDROP_DRAG);
  // Now process the native drag state and then dispatch the event
  ProcessDrag(NS_DRAGDROP_OVER, grfKeyState, ptl, pdwEffect);

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
  if (mDropTargetHelper) {
    mDropTargetHelper->DragLeave();
  }

  // dispatch the event into Gecko
  DispatchDragDropEvent(NS_DRAGDROP_EXIT, gDragLastPoint);

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
      mDragService->EndDragSession(PR_FALSE);
    }
  }

  // release the ref that was taken in DragEnter
  NS_ASSERTION(mTookOwnRef, "want to release own ref, but not taken!");
  if (mTookOwnRef) {
    this->Release();
    mTookOwnRef = PR_FALSE;
  }

  return S_OK;
}

void
nsNativeDragTarget::DragCancel()
{
  // Cancel the drag session if we did DragEnter.
  if (mTookOwnRef) {
    if (mDropTargetHelper) {
      mDropTargetHelper->DragLeave();
    }
    if (mDragService) {
      mDragService->EndDragSession(PR_FALSE);
    }
    this->Release(); // matching the AddRef in DragEnter
    mTookOwnRef = PR_FALSE;
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

  // Drag and drop image helper
  if (mDropTargetHelper) {
    POINT pt = { aPT.x, aPT.y };
    mDropTargetHelper->Drop(pData, &pt, *pdwEffect);
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
  nsRefPtr<nsNativeDragTarget> kungFuDeathGrip = this;
  nsCOMPtr<nsIDragService> serv = mDragService;

  // Now process the native drag state and then dispatch the event
  ProcessDrag(NS_DRAGDROP_DROP, grfKeyState, aPT, pdwEffect);

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
  serv->EndDragSession(PR_TRUE);

  // release the ref that was taken in DragEnter
  NS_ASSERTION(mTookOwnRef, "want to release own ref, but not taken!");
  if (mTookOwnRef) {
    this->Release();
    mTookOwnRef = PR_FALSE;
  }

  return S_OK;
}
