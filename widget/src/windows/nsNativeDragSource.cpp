/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsNativeDragSource.h"
#include <stdio.h>
#include "nslog.h"

NS_IMPL_LOG(nsNativeDragSourceLog)
#define PRINTF NS_LOG_PRINTF(nsNativeDragSourceLog)
#define FLUSH  NS_LOG_FLUSH(nsNativeDragSourceLog)


/*
 * class nsNativeDragSource
 */
nsNativeDragSource::nsNativeDragSource()
{
	 m_cRef     = 0;
}

nsNativeDragSource::~nsNativeDragSource()
{
}

// IUnknown methods - see iunknown.h for documentation

STDMETHODIMP nsNativeDragSource::QueryInterface(REFIID riid, void** ppv)
{
    *ppv=NULL;

    if (IID_IUnknown==riid || IID_IDropSource==riid)
        *ppv=this;

	 if (NULL!=*ppv) {
        ((LPUNKNOWN)*ppv)->AddRef();
        return NOERROR;
	 }

    return ResultFromScode(E_NOINTERFACE);
}


STDMETHODIMP_(ULONG) nsNativeDragSource::AddRef(void)
{
    return ++m_cRef;
}

STDMETHODIMP_(ULONG) nsNativeDragSource::Release(void)
{
	 if (0 != --m_cRef)
        return m_cRef;

    delete this;
    return 0;
}

// IDataSource methods - see idropsrc.h for documentation

STDMETHODIMP nsNativeDragSource::QueryContinueDrag(BOOL fEsc, DWORD grfKeyState)
{
  //PRINTF("QueryContinueDrag\n");
  if (fEsc) {
    //PRINTF("QueryContinueDrag: fEsc\n");
    return ResultFromScode(DRAGDROP_S_CANCEL);
  }

  if (!(grfKeyState & MK_LBUTTON)) {
    //PRINTF("QueryContinueDrag: grfKeyState & MK_LBUTTON\n");
    return ResultFromScode(DRAGDROP_S_DROP);
  }

  //PRINTF("QueryContinueDrag: NOERROR\n");
	return NOERROR;
}

STDMETHODIMP nsNativeDragSource::GiveFeedback(DWORD dwEffect)
{
  //PRINTF("GiveFeedback\n");
	return ResultFromScode(DRAGDROP_S_USEDEFAULTCURSORS);
}
