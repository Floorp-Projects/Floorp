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

  mWindow = aWnd;
  mHWnd   = (HWND)mWindow->GetNativeData(NS_NATIVE_WINDOW);

  /*
   * Get the DragService
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
void nsNativeDragTarget::DispatchDragDropEvent(PRUint32 aEventType, 
                                               POINTL   aPT)
{
  nsEventStatus status;
  nsGUIEvent event;
  ((nsWindow *)mWindow)->InitEvent(event, aEventType);

  //DWORD pos = ::GetMessagePos();
  POINT cpos;

  cpos.x = aPT.x;
  cpos.y = aPT.y;

  //cpos.x = LOWORD(pos);
  //cpos.y = HIWORD(pos);

  if (mHWnd != NULL) {
    ::ScreenToClient(mHWnd, &cpos);
    event.point.x = cpos.x;
    event.point.y = cpos.y;
  } else {
    event.point.x = 0;
    event.point.y = 0;
  }

  //event.point.x = aPT.x;
  //event.point.y = aPT.y;
  mWindow->DispatchEvent(&event, status);
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
    DispatchDragDropEvent(NS_DRAGDROP_ENTER, pt);
    PRBool canDrop;
    mDragService->GetCanDrop(&canDrop);
    if (!canDrop) {
      *pdwEffect = DROPEFFECT_NONE;
    }
    mDragService->SetCanDrop(PR_FALSE);
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
    DispatchDragDropEvent(NS_DRAGDROP_OVER, pt);
    gDragLastPoint = pt;
    PRBool canDrop;
    mDragService->GetCanDrop(&canDrop);
    printf("Can Drop %d\n", canDrop);
    if (!canDrop) {
      *pdwEffect = DROPEFFECT_NONE;
    }
    mDragService->SetCanDrop(PR_FALSE);
		return NOERROR;
	} else {
		return ResultFromScode(E_FAIL);
	}
}

//-----------------------------------------------------
STDMETHODIMP nsNativeDragTarget::DragLeave() {
  if (DRAG_DEBUG) printf("DragLeave\n");
	if (mDragService) {
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

    nsDragService * dragService = (nsDragService *)mDragService;
    dragService->SetIDataObject(pIDataSource);


    DispatchDragDropEvent(NS_DRAGDROP_DROP, aPT);
    PRBool canDrop;
    mDragService->GetCanDrop(&canDrop);
    if (!canDrop) {
      *pdwEffect = DROPEFFECT_NONE;
    }
    mDragService->SetCanDrop(PR_FALSE);
    return NOERROR;
	} else {
		return ResultFromScode(E_FAIL);
	}
}


