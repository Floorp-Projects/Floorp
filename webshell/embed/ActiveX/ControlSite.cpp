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
	m_bWindowless = FALSE;

	// Initialise ambient properties
	m_nAmbientLocale = 0;
	m_clrAmbientForeColor = RGB(255,255,255);
	m_clrAmbientBackColor = RGB(255,0,0);
	m_bAmbientUserMode = true;
	m_bAmbientShowHatching = true;
	m_bAmbientShowGrabHandles = true;
}


CControlSite::~CControlSite()
{
}


HRESULT CControlSite::Attach(REFCLSID clsid, HWND hwndParent, const RECT &rcPos, IStream *pInitStream)
{
	m_hwndParent = hwndParent;
	m_rcObjectPos = rcPos;

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

	// Initialise the control from store
	CComQIPtr<IPersistStream, &IID_IPersistStream> spIPersistStream = m_spIOleObject;
	CComQIPtr<IPersistStreamInit, &IID_IPersistStreamInit> spIPersistStreamInit = m_spIOleObject;
	if (spIPersistStreamInit && pInitStream == NULL)
	{
		spIPersistStreamInit->InitNew();
	}
	else if (pInitStream)
	{
		spIPersistStream->Load(pInitStream);
	}

	m_spIOleInPlaceObject = m_spObject;
	m_spIOleInPlaceObject->SetObjectRects(&m_rcObjectPos, &m_rcObjectPos);

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

	return S_OK;
}


HRESULT CControlSite::Detach()
{
	if (m_spIOleInPlaceObject)
	{
		m_spIOleInPlaceObject->InPlaceDeactivate();
		m_spIOleInPlaceObject.Release();
	}

	if (m_spIOleObject)
	{
		m_spIOleObject->SetClientSite(this);
		m_spIOleObject.Release();
	}

	m_spIViewObject.Release();
	m_spObject.Release();
	
	return S_OK;
}


HRESULT CControlSite::GetControlUnknown(IUnknown **ppObject)
{
	*ppObject = NULL;
	if (m_spObject)
	{
		m_spObject->QueryInterface(IID_IUnknown, (void **) ppObject);
	}
	return S_OK;
}


HRESULT CControlSite::Draw(HDC hdc)
{
	// Draw only when control is windowless or deactivated
	if (m_spIViewObject)
	{
		if (m_bWindowless || !m_bInPlaceActive)
		{
			RECTL *prcBounds = (m_bWindowless) ? NULL : (RECTL *) &m_rcObjectPos;
			m_spIViewObject->Draw(DVASPECT_CONTENT, -1, NULL, NULL, NULL, hdc, prcBounds, NULL, NULL, 0);
		}
	}
	return S_OK;
}

HRESULT CControlSite::DoVerb(LONG nVerb, LPMSG lpMsg)
{
	if (m_spIOleObject == NULL)
	{
		return E_FAIL;
	}

	return m_spIOleObject->DoVerb(nVerb, lpMsg, this, 0, m_hwndParent, &m_rcObjectPos);
}


HRESULT CControlSite::SetPosition(const RECT &rcPos)
{
	m_rcObjectPos = rcPos;
	m_spIOleInPlaceObject->SetObjectRects(&m_rcObjectPos, &m_rcObjectPos);
	return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
// IDispatch implementation

HRESULT STDMETHODCALLTYPE CControlSite::GetTypeInfoCount(/* [out] */ UINT __RPC_FAR *pctinfo)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CControlSite::GetTypeInfo(/* [in] */ UINT iTInfo, /* [in] */ LCID lcid, /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CControlSite::GetIDsOfNames(/* [in] */ REFIID riid, /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames, /* [in] */ UINT cNames, /* [in] */ LCID lcid, /* [size_is][out] */ DISPID __RPC_FAR *rgDispId)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CControlSite::Invoke(/* [in] */ DISPID dispIdMember, /* [in] */ REFIID riid, /* [in] */ LCID lcid, /* [in] */ WORD wFlags, /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams, /* [out] */ VARIANT __RPC_FAR *pVarResult, /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo, /* [out] */ UINT __RPC_FAR *puArgErr)
{
	if (wFlags & DISPATCH_PROPERTYGET)
	{
		CComVariant vResult;

		switch (dispIdMember)
		{
		case DISPID_AMBIENT_FORECOLOR:
			vResult = CComVariant((long) m_clrAmbientForeColor);
			break;

		case DISPID_AMBIENT_BACKCOLOR:
			vResult = CComVariant((long) m_clrAmbientBackColor);
			break;

		case DISPID_AMBIENT_LOCALEID:
			vResult = CComVariant((long) m_nAmbientLocale);
			break;

		case DISPID_AMBIENT_USERMODE:
			vResult = CComVariant(m_bAmbientUserMode);
			break;
		
		case DISPID_AMBIENT_SHOWGRABHANDLES:
			vResult = CComVariant(m_bAmbientShowGrabHandles);
			break;
		
		case DISPID_AMBIENT_SHOWHATCHING:
			vResult = CComVariant(m_bAmbientShowHatching);
			break;

		default:
			return DISP_E_MEMBERNOTFOUND;
		}

		VariantCopy(pVarResult, &vResult);
		return S_OK;
	}

	return E_FAIL;
}


///////////////////////////////////////////////////////////////////////////////
// IAdviseSink implementation

void STDMETHODCALLTYPE CControlSite::OnDataChange(/* [unique][in] */ FORMATETC __RPC_FAR *pFormatetc, /* [unique][in] */ STGMEDIUM __RPC_FAR *pStgmed)
{
}


void STDMETHODCALLTYPE CControlSite::OnViewChange(/* [in] */ DWORD dwAspect, /* [in] */ LONG lindex)
{
	// TODO redraw
}


void STDMETHODCALLTYPE CControlSite::OnRename(/* [in] */ IMoniker __RPC_FAR *pmk)
{
}


void STDMETHODCALLTYPE CControlSite::OnSave(void)
{
}


void STDMETHODCALLTYPE CControlSite::OnClose(void)
{
}


///////////////////////////////////////////////////////////////////////////////
// IAdviseSink2 implementation
void STDMETHODCALLTYPE CControlSite::OnLinkSrcChange(/* [unique][in] */ IMoniker __RPC_FAR *pmk)
{
}


///////////////////////////////////////////////////////////////////////////////
// IAdviseSinkEx implementation

void STDMETHODCALLTYPE CControlSite::OnViewStatusChange(/* [in] */ DWORD dwViewStatus)
{
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
	*lprcPosRect = m_rcObjectPos;
	*lprcClipRect = m_rcObjectPos;

	lpFrameInfo->fMDIApp = FALSE;
	lpFrameInfo->hwndFrame = NULL;
	lpFrameInfo->haccel = NULL;
	lpFrameInfo->cAccelEntries = 0;

	return S_OK;
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
// IOleInPlaceSiteEx implementation

HRESULT STDMETHODCALLTYPE CControlSite::OnInPlaceActivateEx(/* [out] */ BOOL __RPC_FAR *pfNoRedraw, /* [in] */ DWORD dwFlags)
{
	// TODO check if control is windowless
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::OnInPlaceDeactivateEx(/* [in] */ BOOL fNoRedraw)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::RequestUIActivate(void)
{
	return S_FALSE;
}


///////////////////////////////////////////////////////////////////////////////
// IOleInPlaceSiteWindowless implementation

HRESULT STDMETHODCALLTYPE CControlSite::CanWindowlessActivate(void)
{
	// TODO allow windowless activation
	return S_FALSE;
}

HRESULT STDMETHODCALLTYPE CControlSite::GetCapture(void)
{
	// TODO capture the mouse for the object
	return S_FALSE;
}

HRESULT STDMETHODCALLTYPE CControlSite::SetCapture(/* [in] */ BOOL fCapture)
{
	// TODO capture the mouse for the object
	return S_FALSE;
}

HRESULT STDMETHODCALLTYPE CControlSite::GetFocus(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::SetFocus(/* [in] */ BOOL fFocus)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CControlSite::GetDC(/* [in] */ LPCRECT pRect, /* [in] */ DWORD grfFlags, /* [out] */ HDC __RPC_FAR *phDC)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CControlSite::ReleaseDC(/* [in] */ HDC hDC)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CControlSite::InvalidateRect(/* [in] */ LPCRECT pRect, /* [in] */ BOOL fErase)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CControlSite::InvalidateRgn(/* [in] */ HRGN hRGN, /* [in] */ BOOL fErase)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CControlSite::ScrollRect(/* [in] */ INT dx, /* [in] */ INT dy, /* [in] */ LPCRECT pRectScroll, /* [in] */ LPCRECT pRectClip)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CControlSite::AdjustRect(/* [out][in] */ LPRECT prc)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CControlSite::OnDefWindowMessage(/* [in] */ UINT msg, /* [in] */ WPARAM wParam, /* [in] */ LPARAM lParam, /* [out] */ LRESULT __RPC_FAR *plResult)
{
	return E_NOTIMPL;
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
	HRESULT hr = S_OK;

	if (pPtlHimetric == NULL)
	{
		return E_INVALIDARG;
	}
	if (pPtfContainer == NULL)
	{
		return E_INVALIDARG;
	}

	HDC hdc = ::GetDC(m_hwndParent);
	::SetMapMode(hdc, MM_HIMETRIC);
	POINT rgptConvert[2];
	rgptConvert[0].x = 0;
	rgptConvert[0].y = 0;

	if (dwFlags & XFORMCOORDS_HIMETRICTOCONTAINER)
	{
		rgptConvert[1].x = pPtlHimetric->x;
		rgptConvert[1].y = pPtlHimetric->y;
		::LPtoDP(hdc, rgptConvert, 2);
		if (dwFlags & XFORMCOORDS_SIZE)
		{
			pPtfContainer->x = (float)(rgptConvert[1].x - rgptConvert[0].x);
			pPtfContainer->y = (float)(rgptConvert[0].y - rgptConvert[1].y);
		}
		else if (dwFlags & XFORMCOORDS_POSITION)
		{
			pPtfContainer->x = (float)rgptConvert[1].x;
			pPtfContainer->y = (float)rgptConvert[1].y;
		}
		else
		{
			hr = E_INVALIDARG;
		}
	}
	else if (dwFlags & XFORMCOORDS_CONTAINERTOHIMETRIC)
	{
		rgptConvert[1].x = (int)(pPtfContainer->x);
		rgptConvert[1].y = (int)(pPtfContainer->y);
		::DPtoLP(hdc, rgptConvert, 2);
		if (dwFlags & XFORMCOORDS_SIZE)
		{
			pPtlHimetric->x = rgptConvert[1].x - rgptConvert[0].x;
			pPtlHimetric->y = rgptConvert[0].y - rgptConvert[1].y;
		}
		else if (dwFlags & XFORMCOORDS_POSITION)
		{
			pPtlHimetric->x = rgptConvert[1].x;
			pPtlHimetric->y = rgptConvert[1].y;
		}
		else
		{
			hr = E_INVALIDARG;
		}
	}
	else
	{
		hr = E_INVALIDARG;
	}

	::ReleaseDC(m_hwndParent, hdc);

	return hr;
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
