/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "stdafx.h"


CControlSiteIPFrame::CControlSiteIPFrame()
{
	m_hwndFrame = NULL;
}

CControlSiteIPFrame::~CControlSiteIPFrame()
{
}

///////////////////////////////////////////////////////////////////////////////
// IOleWindow implementation

HRESULT STDMETHODCALLTYPE CControlSiteIPFrame::GetWindow(/* [out] */ HWND __RPC_FAR *phwnd)
{
	if (phwnd == NULL)
	{
		return E_INVALIDARG;
	}
	*phwnd = m_hwndFrame;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSiteIPFrame::ContextSensitiveHelp(/* [in] */ BOOL fEnterMode)
{
	return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
// IOleInPlaceUIWindow implementation

HRESULT STDMETHODCALLTYPE CControlSiteIPFrame::GetBorder(/* [out] */ LPRECT lprectBorder)
{
	return INPLACE_E_NOTOOLSPACE;
}

HRESULT STDMETHODCALLTYPE CControlSiteIPFrame::RequestBorderSpace(/* [unique][in] */ LPCBORDERWIDTHS pborderwidths)
{
	return INPLACE_E_NOTOOLSPACE;
}

HRESULT STDMETHODCALLTYPE CControlSiteIPFrame::SetBorderSpace(/* [unique][in] */ LPCBORDERWIDTHS pborderwidths)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSiteIPFrame::SetActiveObject(/* [unique][in] */ IOleInPlaceActiveObject __RPC_FAR *pActiveObject, /* [unique][string][in] */ LPCOLESTR pszObjName)
{
	return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
// IOleInPlaceFrame implementation

HRESULT STDMETHODCALLTYPE CControlSiteIPFrame::InsertMenus(/* [in] */ HMENU hmenuShared, /* [out][in] */ LPOLEMENUGROUPWIDTHS lpMenuWidths)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSiteIPFrame::SetMenu(/* [in] */ HMENU hmenuShared, /* [in] */ HOLEMENU holemenu, /* [in] */ HWND hwndActiveObject)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSiteIPFrame::RemoveMenus(/* [in] */ HMENU hmenuShared)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSiteIPFrame::SetStatusText(/* [in] */ LPCOLESTR pszStatusText)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSiteIPFrame::EnableModeless(/* [in] */ BOOL fEnable)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSiteIPFrame::TranslateAccelerator(/* [in] */ LPMSG lpmsg, /* [in] */ WORD wID)
{
	return E_NOTIMPL;
}

