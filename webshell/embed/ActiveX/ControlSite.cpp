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

#include "StdAfx.h"

#include "ControlSite.h"

CControlSite::CControlSite()
{
	m_hwndParent = NULL;
	m_bSetClientSiteFirst = FALSE;
	m_bVisibleAtRuntime = TRUE;
	memset(&m_rcObjectPos, 0, sizeof(m_rcObjectPos));

	m_bInPlaceActive = FALSE;
	m_bUIActive = FALSE;
	m_bInPlaceLocked = FALSE;
}

CControlSite::~CControlSite()
{
}

HRESULT CControlSite::InitNew(REFCLSID clsid)
{
	// TODO see if object is script safe
	
	// Create the object
	HRESULT hr = CoCreateInstance(clsid, NULL, CLSCTX_ALL, IID_IUnknown, (void **) &m_spObject);
	if (FAILED(hr))
	{
		return E_FAIL;
	}
	
	m_spIViewObject = m_spObject;
	m_spIOleObject = m_spObject;
	
	if (m_spIOleObject == NULL)
	{
		return E_FAIL;
	}
	
	DWORD dwMiscStatus;
	m_spIOleObject->GetMiscStatus(DVASPECT_CONTENT, &dwMiscStatus);

	if (dwMiscStatus & OLEMISC_SETCLIENTSITEFIRST)
	{
		m_bSetClientSiteFirst = TRUE;
	}
	if (dwMiscStatus & OLEMISC_INVISIBLEATRUNTIME)
	{
		m_bVisibleAtRuntime = FALSE;
	}

	// Some objects like to have the client site as the first thing
	// to be initialised (for ambient properties and so forth)
	if (m_bSetClientSiteFirst)
	{
		m_spIOleObject->SetClientSite(this);
	}

	// TODO load the control from store

	// In-place activate the object
	if (m_bVisibleAtRuntime)
	{
		DoVerb(OLEIVERB_INPLACEACTIVATE);
	}

	// For those objects which haven't had their client site set yet,
	// it's done here.
	if (!m_bSetClientSiteFirst)
	{
		m_spIOleObject->SetClientSite(this);
	}

	// TODO create new
	return E_NOTIMPL;
}


HRESULT CControlSite::DoVerb(LONG nVerb, LPMSG lpMsg)
{
	if (m_spIOleObject == NULL)
	{
		return E_FAIL;
	}

	return m_spIOleObject->DoVerb(nVerb, lpMsg, this, 0, m_hwndParent, &m_rcObjectPos);
}


HRESULT CControlSite::SetObjectPos(const RECT &rcPos)
{
	m_rcObjectPos = rcPos;
	return E_NOTIMPL;
}


///////////////////////////////////////////////////////////////////////////////
// IOleWindow implementation

HRESULT STDMETHODCALLTYPE CControlSite::GetWindow(/* [out] */ HWND __RPC_FAR *phwnd)
{
	*phwnd = m_hwndParent;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::ContextSensitiveHelp(/* [in] */ BOOL fEnterMode)
{
	return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
// IOleClientSite implementation

HRESULT STDMETHODCALLTYPE CControlSite::SaveObject(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::GetMoniker(/* [in] */ DWORD dwAssign, /* [in] */ DWORD dwWhichMoniker, /* [out] */ IMoniker __RPC_FAR *__RPC_FAR *ppmk)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CControlSite::GetContainer(/* [out] */ IOleContainer __RPC_FAR *__RPC_FAR *ppContainer)
{
	return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE CControlSite::ShowObject(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::OnShowWindow(/* [in] */ BOOL fShow)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::RequestNewObjectLayout(void)
{
	return E_NOTIMPL;
}


///////////////////////////////////////////////////////////////////////////////
// IOleInPlaceSite implementation


HRESULT STDMETHODCALLTYPE CControlSite::CanInPlaceActivate(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::OnInPlaceActivate(void)
{
	m_bInPlaceActive = TRUE;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::OnUIActivate(void)
{
	m_bUIActive = TRUE;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::GetWindowContext(/* [out] */ IOleInPlaceFrame __RPC_FAR *__RPC_FAR *ppFrame, /* [out] */ IOleInPlaceUIWindow __RPC_FAR *__RPC_FAR *ppDoc, /* [out] */ LPRECT lprcPosRect, /* [out] */ LPRECT lprcClipRect, /* [out][in] */ LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CControlSite::Scroll(/* [in] */ SIZE scrollExtant)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::OnUIDeactivate(/* [in] */ BOOL fUndoable)
{
	m_bUIActive = FALSE;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::OnInPlaceDeactivate(void)
{
	m_bInPlaceActive = FALSE;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::DiscardUndoState(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::DeactivateAndUndo(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::OnPosRectChange(/* [in] */ LPCRECT lprcPosRect)
{
	return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
// IOleControlSite implementation

HRESULT STDMETHODCALLTYPE CControlSite::OnControlInfoChanged(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::LockInPlaceActive(/* [in] */ BOOL fLock)
{
	m_bInPlaceLocked = fLock;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::GetExtendedControl(/* [out] */ IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CControlSite::TransformCoords(/* [out][in] */ POINTL __RPC_FAR *pPtlHimetric, /* [out][in] */ POINTF __RPC_FAR *pPtfContainer, /* [in] */ DWORD dwFlags)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CControlSite::TranslateAccelerator(/* [in] */ MSG __RPC_FAR *pMsg, /* [in] */ DWORD grfModifiers)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CControlSite::OnFocus(/* [in] */ BOOL fGotFocus)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::ShowPropertyFrame(void)
{
	return E_NOTIMPL;
}
