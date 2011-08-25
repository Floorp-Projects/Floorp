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
 * The Original Code is mozilla.org code.
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

#include "nsNativeDragSource.h"
#include <stdio.h>
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsToolkit.h"
#include "nsWidgetsCID.h"
#include "nsIDragService.h"

static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

/*
 * class nsNativeDragSource
 */
nsNativeDragSource::nsNativeDragSource(nsIDOMDataTransfer* aDataTransfer) :
  m_cRef(0),
  m_hCursor(nsnull),
  mUserCancelled(PR_FALSE)
{
  mDataTransfer = do_QueryInterface(aDataTransfer);
}

nsNativeDragSource::~nsNativeDragSource()
{
}

STDMETHODIMP
nsNativeDragSource::QueryInterface(REFIID riid, void** ppv)
{
  *ppv=NULL;

  if (IID_IUnknown==riid || IID_IDropSource==riid)
    *ppv=this;

  if (NULL!=*ppv) {
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
  nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
  if (dragService) {
    DWORD pos = ::GetMessagePos();
    dragService->DragMoved(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));
  }

  if (fEsc) {
    mUserCancelled = PR_TRUE;
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
      m_hCursor =  nsnull;
    }
  }

  if (m_hCursor) {
    ::SetCursor(m_hCursor);
    return S_OK;
  }
  
  // Let the system choose which cursor to apply.
  return DRAGDROP_S_USEDEFAULTCURSORS;
}
