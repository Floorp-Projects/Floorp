/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsNativeDragSource.h"
#include <stdio.h>


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
#ifdef DEBUG
  //printf("QueryContinueDrag: ");
#endif
  if (fEsc) {
#ifdef DEBUG
    //printf("fEsc\n");
#endif
    return ResultFromScode(DRAGDROP_S_CANCEL);
  }

  if (!(grfKeyState & MK_LBUTTON)) {
#ifdef DEBUG
    //printf("grfKeyState & MK_LBUTTON\n");
#endif
    return ResultFromScode(DRAGDROP_S_DROP);
  }

#ifdef DEBUG
  //printf("NOERROR\n");
#endif
	return NOERROR;
}

STDMETHODIMP nsNativeDragSource::GiveFeedback(DWORD dwEffect)
{
#ifdef DEBUG
  //printf("GiveFeedback\n");
#endif
	return ResultFromScode(DRAGDROP_S_USEDEFAULTCURSORS);
}
