/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include <stdio.h>
#include "Ddcomm.h"
#include "nsIDragService.h"
#include "nsWidgetsCID.h"
#include "nsNativeDragTarget.h"
#include "nsDragService.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nsIWidget.h"
#include "nsWindow.h"

#define DRAG_DEBUG 0

/* Define Class IDs */
static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kIDragServiceIID, NS_IDRAGSERVICE_IID);

// This is cached for Leave notification
static POINTL gDragLastPoint;

/*
 * class nsNativeDragTarget
 */

//-----------------------------------------------------
// construction
//-----------------------------------------------------
nsNativeDragTarget::nsNativeDragTarget(nsIWidget * aWnd)
{
  m_cRef = 0;

  mWindow  = aWnd; // don't ref count this
  mHWnd    = (HWND)mWindow->GetNativeData(NS_NATIVE_WINDOW);
  mDataObj = NULL;

  /*
   * Create/Get the DragService that we have implemented
   */
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             kIDragServiceIID,
                                             (nsISupports**)&mDragService);
}


//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsNativeDragTarget::~nsNativeDragTarget()
{
  nsServiceManager::ReleaseService(kCDragServiceCID, mDragService);
  NS_IF_RELEASE(mDataObj);
}

//-----------------------------------------------------
// IUnknown methods - see iunknown.h for documentation
//-----------------------------------------------------
STDMETHODIMP nsNativeDragTarget::QueryInterface(REFIID riid, void** ppv)
{
    *ppv=NULL;

	 if (IID_IUnknown == riid || IID_IDropTarget == riid)
        *ppv=this;

	 if (NULL!=*ppv) {
        ((LPUNKNOWN)*ppv)->AddRef();
        return NOERROR;
	 }

    return ResultFromScode(E_NOINTERFACE);
}


//-----------------------------------------------------
STDMETHODIMP_(ULONG) nsNativeDragTarget::AddRef(void)
{
    return ++m_cRef;
}

//-----------------------------------------------------
STDMETHODIMP_(ULONG) nsNativeDragTarget::Release(void)
{
	 if (0 != --m_cRef)
        return m_cRef;

    delete this;
    return 0;
}

//-----------------------------------------------------
void nsNativeDragTarget::GetGeckoDragAction(DWORD grfKeyState, LPDWORD pdwEffect, PRUint32 * aGeckoAction) 
{

  //Check if we can link from this data object as well.
  PRBool canLink = PR_FALSE;
  if (NULL != mDataObj) {
    canLink = (S_OK == ::OleQueryLinkFromData(mDataObj)?PR_TRUE:PR_FALSE);
  }

  // Default is move if we can, in fact drop here.
  *pdwEffect    = DROPEFFECT_MOVE;
  *aGeckoAction = DRAGDROP_ACTION_MOVE;

  // Given the key modifiers gifure out what state we are in for both
  // the native system and Gecko
  if (grfKeyState & MK_CONTROL) {
    if (canLink && (grfKeyState & MK_SHIFT)) {
      *aGeckoAction = DRAGDROP_ACTION_LINK;
      *pdwEffect    = DROPEFFECT_LINK;
    } else {
      *aGeckoAction = DRAGDROP_ACTION_COPY;
      *pdwEffect    = DROPEFFECT_COPY;
    }
  }
}

//-----------------------------------------------------
void nsNativeDragTarget::DispatchDragDropEvent(PRUint32 aEventType, 
                                               POINTL   aPT)
{
  nsEventStatus status;
  nsGUIEvent event;

  nsWindow * win = NS_STATIC_CAST(nsWindow *, mWindow);
  win->InitEvent(event, aEventType);

  POINT cpos;

  cpos.x = aPT.x;
  cpos.y = aPT.y;

  if (mHWnd != NULL) {
    ::ScreenToClient(mHWnd, &cpos);
    event.point.x = cpos.x;
    event.point.y = cpos.y;
  } else {
    event.point.x = 0;
    event.point.y = 0;
  }

  mWindow->DispatchEvent(&event, status);
}

//-----------------------------------------------------
void nsNativeDragTarget::ProcessDrag(PRUint32     aEventType, 
                                     DWORD        grfKeyState,
												             POINTL       pt, 
                                     DWORD*       pdwEffect)
{
  // Before dispatching the event make sure we have the correct drop action set
  PRUint32 geckoAction;
  GetGeckoDragAction(grfKeyState, pdwEffect, &geckoAction);

  // Set the current action into the Gecko specific type
  mDragService->SetDragAction(geckoAction);

  // Dispatch the event into Gecko
  DispatchDragDropEvent(aEventType, pt);

  // Now get the cached Drag effect from the drag service
  // the data memeber should have been set by who ever handled the 
  // nsGUIEvent or nsIDOMEvent
  PRBool canDrop;
  mDragService->GetCanDrop(&canDrop);
  if (!canDrop) {
    *pdwEffect = DROPEFFECT_NONE;
  }

  // Clear the cached value
  mDragService->SetCanDrop(PR_FALSE);
}

//-----------------------------------------------------
// IDropTarget methods
//-----------------------------------------------------
STDMETHODIMP nsNativeDragTarget::DragEnter(LPDATAOBJECT pIDataSource, 
                                           DWORD        grfKeyState,
												                   POINTL       pt, 
                                           DWORD*       pdwEffect)
{
  if (DRAG_DEBUG) printf("DragEnter\n");

	if (mDragService) {
    // We a new IDataObject, release the old if necessary and
    // keep a pointer to the new on
    NS_IF_RELEASE(mDataObj);
    mDataObj = pIDataSource;
    NS_ADDREF(mDataObj);

    // This cast is ok because in the constructor we created a 
    // the actual implementation we wanted, so we know this is
    // a nsDragService
    nsDragService * winDragService = NS_STATIC_CAST(nsDragService *, mDragService);

    // Set the native data object into drage service
    winDragService->SetIDataObject(pIDataSource);

    // Now process the native drag state and then dispatch the event
    ProcessDrag(NS_DRAGDROP_ENTER, grfKeyState, pt, pdwEffect);

		return NOERROR;
	} else {
		return ResultFromScode(E_FAIL);
	}
}


//-----------------------------------------------------
STDMETHODIMP nsNativeDragTarget::DragOver(DWORD   grfKeyState, 
                                          POINTL  pt, 
                                          LPDWORD pdwEffect)
{
  if (DRAG_DEBUG) printf("DragOver\n");
	if (mDragService) {
    // Now process the native drag state and then dispatch the event
    ProcessDrag(NS_DRAGDROP_OVER, grfKeyState, pt, pdwEffect);
		return NOERROR;
	} else {
		return ResultFromScode(E_FAIL);
	}
}

//-----------------------------------------------------
STDMETHODIMP nsNativeDragTarget::DragLeave() {

  if (DRAG_DEBUG) printf("DragLeave\n");
	if (mDragService) {
    // dispatch the event into Gecko
    DispatchDragDropEvent(NS_DRAGDROP_EXIT, gDragLastPoint);
		return NOERROR;
	} else {
		return ResultFromScode(E_FAIL);
	}
}


//-----------------------------------------------------
STDMETHODIMP nsNativeDragTarget::Drop(LPDATAOBJECT pIDataSource, 
                                      DWORD        grfKeyState,
										                  POINTL       aPT, 
                                      LPDWORD      pdwEffect)
{
  if (DRAG_DEBUG) printf("Drop\n");

	if (mDragService) {
    if (mDataObj != NULL) {
      // Make sure we have a valid IDataObject and 
      // that it matches the one we got on the drag enter
      if (pIDataSource == mDataObj) {
        mDataObj = pIDataSource;
        NS_ADDREF(mDataObj);
      } else {
        // Boy this is weird, they should be the same
        // XXX should assert here, 
        // but instead we will just recover....
        NS_IF_RELEASE(mDataObj);
        mDataObj = pIDataSource;
        NS_ADDREF(mDataObj);
      }
    } else {
      // Boy this is weird, it should be null
      // XXX should assert here
    }

    // This cast is ok because in the constructor we created a 
    // the actual implementation we wanted, so we know this is
    // a nsDragService
    nsDragService * winDragService = NS_STATIC_CAST(nsDragService *, mDragService);

    // Set the native data object into drage service
    winDragService->SetIDataObject(pIDataSource);

    // Now process the native drag state and then dispatch the event
    ProcessDrag(NS_DRAGDROP_DROP, grfKeyState, aPT, pdwEffect);

    return S_OK;
	} else {
		return ResultFromScode(E_FAIL);
	}
}


