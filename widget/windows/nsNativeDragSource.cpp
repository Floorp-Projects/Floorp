/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeDragSource.h"
#include <stdio.h>
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsToolkit.h"
#include "nsWidgetsCID.h"
#include "nsIDragService.h"

/*
 * class nsNativeDragSource
 */
nsNativeDragSource::nsNativeDragSource(nsIDOMDataTransfer* aDataTransfer) :
  m_cRef(0),
  m_hCursor(nullptr),
  mUserCancelled(false)
{
  mDataTransfer = do_QueryInterface(aDataTransfer);
}

nsNativeDragSource::~nsNativeDragSource()
{
}

STDMETHODIMP
nsNativeDragSource::QueryInterface(REFIID riid, void** ppv)
{
  *ppv=nullptr;

  if (IID_IUnknown==riid || IID_IDropSource==riid)
    *ppv=this;

  if (nullptr!=*ppv) {
    ((LPUNKNOWN)*ppv)->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
nsNativeDragSource::AddRef(void)
{
  ++m_cRef;
  NS_LOG_ADDREF(this, m_cRef, "nsNativeDragSource", sizeof(*this));
  return m_cRef;
}

STDMETHODIMP_(ULONG)
nsNativeDragSource::Release(void)
{
  --m_cRef;
  NS_LOG_RELEASE(this, m_cRef, "nsNativeDragSource");
  if (0 != m_cRef)
    return m_cRef;

  delete this;
  return 0;
}

STDMETHODIMP
nsNativeDragSource::QueryContinueDrag(BOOL fEsc, DWORD grfKeyState)
{
  static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

  nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
  if (dragService) {
    DWORD pos = ::GetMessagePos();
    dragService->DragMoved(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));
  }

  if (fEsc) {
    mUserCancelled = true;
    return DRAGDROP_S_CANCEL;
  }

  if (!(grfKeyState & MK_LBUTTON) || (grfKeyState & MK_RBUTTON))
    return DRAGDROP_S_DROP;

  return S_OK;
}

STDMETHODIMP
nsNativeDragSource::GiveFeedback(DWORD dwEffect)
{
  // For drags involving tabs, we do some custom work with cursors. 
  if (mDataTransfer) {
    nsAutoString cursor;
    mDataTransfer->GetMozCursor(cursor);
    if (cursor.EqualsLiteral("default")) {
      m_hCursor = ::LoadCursor(0, IDC_ARROW);
    } else {
      m_hCursor =  nullptr;
    }
  }

  if (m_hCursor) {
    ::SetCursor(m_hCursor);
    return S_OK;
  }
  
  // Let the system choose which cursor to apply.
  return DRAGDROP_S_USEDEFAULTCURSORS;
}
