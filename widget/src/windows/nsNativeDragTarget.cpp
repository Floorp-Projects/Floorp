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
#include "nsIServiceManager.h"

#include "nsIWidget.h"
#include "nsWindow.h"

/* Define Class IDs */
static NS_DEFINE_IID(kDragServiceCID,  NS_DRAGSERVICE_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kIDragServiceIID, NS_IDRAGSERVICE_IID);


/*
 * class nsNativeDragTarget
 */

// construction, destruction

nsNativeDragTarget::nsNativeDragTarget(nsIWidget * aWnd)
{
  m_cRef = 0;

  mWindow = aWnd;
  NS_ADDREF(mWindow);
  /*
   * Get the DragService
   */
  nsresult rv = nsServiceManager::GetService(kDragServiceCID,
                                             kIDragServiceIID,
                                             (nsISupports**)&mDragService);
}


nsNativeDragTarget::~nsNativeDragTarget()
{
  NS_IF_RELEASE(mWindow);
}

// IUnknown methods - see iunknown.h for documentation

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


STDMETHODIMP_(ULONG) nsNativeDragTarget::AddRef(void)
{
    return ++m_cRef;
}

STDMETHODIMP_(ULONG) nsNativeDragTarget::Release(void)
{
	 if (0 != --m_cRef)
        return m_cRef;

    delete this;
    return 0;
}

void nsNativeDragTarget::DispatchDragDropEvent(PRUint32 aEventType)
{
  nsEventStatus status;
  nsGUIEvent event;
  ((nsWindow *)mWindow)->InitEvent(event, aEventType);
  mWindow->DispatchEvent(&event, status);
}

// IDropTarget methods

STDMETHODIMP nsNativeDragTarget::DragEnter(LPDATAOBJECT pIDataSource, DWORD grfKeyState,
												                   POINTL pt, DWORD* pdwEffect)
{
  printf("DragEnter\n");
	if (mDragService) {
    DispatchDragDropEvent(NS_DRAGDROP_ENTER);
		return NOERROR;
	} else {
		return ResultFromScode(E_FAIL);
	}
}


STDMETHODIMP nsNativeDragTarget::DragOver(DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect)
{
  printf("DragOver\n");
	if (mDragService) {
    DispatchDragDropEvent(NS_DRAGDROP_OVER);
		return NOERROR;
	} else {
		return ResultFromScode(E_FAIL);
	}
}


STDMETHODIMP nsNativeDragTarget::DragLeave() {
  printf("DragLeave\n");
	if (mDragService) {
    DispatchDragDropEvent(NS_DRAGDROP_EXIT);
		return NOERROR;
	} else {
		return ResultFromScode(E_FAIL);
	}
}


STDMETHODIMP nsNativeDragTarget::Drop(LPDATAOBJECT pIDataSource, DWORD grfKeyState,
										                  POINTL pt, LPDWORD pdwEffect)
{
  printf("Drop\n");
	if (mDragService) {

    // Clear the native clipboard
    ::OleFlushClipboard();

    ::OleSetClipboard(pIDataSource);

    DispatchDragDropEvent(NS_DRAGDROP_DROP);
    return NOERROR;
	} else {
		return ResultFromScode(E_FAIL);
	}
}


