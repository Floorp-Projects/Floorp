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
#ifndef CACTIVESITE_H
#define CACTIVESITE_H

class CControlSite :	public CComObjectRootEx<CComSingleThreadModel>,
						public IOleClientSite,
						public IOleInPlaceSite,
						public IOleControlSite
{
	// Handle to parent window
	HWND m_hwndParent;
	// Position of the site and the contained object
	RECT m_rcObjectPos;
	// Flag indicating if client site should be set early or late
	BOOL m_bSetClientSiteFirst;
	// Flag indicating whether control is visible or not
	BOOL m_bVisibleAtRuntime;
	// Flag indicating if control is in-place active
	BOOL m_bInPlaceActive;
	// Flag indicating if control is UI active
	BOOL m_bUIActive;
	// Flag indicating if control is in-place locked and cannot be deactivated
	BOOL m_bInPlaceLocked;

	// Raw pointer to the object
	CComPtr<IUnknown> m_spObject;
	// Pointer to objects IViewObject interface
	CComQIPtr<IViewObject, &IID_IViewObject> m_spIViewObject;
	// Pointer to objects IOleObject interface
	CComQIPtr<IOleObject, &IID_IOleObject> m_spIOleObject;

public:
	CControlSite();
	virtual ~CControlSite();

//DECLARE_REGISTRY_RESOURCEID(IDR_MOZILLABROWSER)

BEGIN_COM_MAP(CControlSite)
	COM_INTERFACE_ENTRY(IOleWindow)
	COM_INTERFACE_ENTRY(IOleClientSite)
	COM_INTERFACE_ENTRY(IOleInPlaceSite)
	COM_INTERFACE_ENTRY(IOleControlSite)
END_COM_MAP()


	virtual HRESULT InitNew(REFCLSID clsid);
	virtual HRESULT SetObjectPos(const RECT &rcPos);
	virtual HRESULT DoVerb(LONG nVerb, LPMSG lpMsg = NULL);

	// IOleWindow implementation
	virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE GetWindow(/* [out] */ HWND __RPC_FAR *phwnd);
	virtual HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(/* [in] */ BOOL fEnterMode);

	// IOleClientSite implementation
	virtual HRESULT STDMETHODCALLTYPE SaveObject(void);
	virtual HRESULT STDMETHODCALLTYPE GetMoniker(/* [in] */ DWORD dwAssign, /* [in] */ DWORD dwWhichMoniker, /* [out] */ IMoniker __RPC_FAR *__RPC_FAR *ppmk);
	virtual HRESULT STDMETHODCALLTYPE GetContainer(/* [out] */ IOleContainer __RPC_FAR *__RPC_FAR *ppContainer);
	virtual HRESULT STDMETHODCALLTYPE ShowObject(void);
	virtual HRESULT STDMETHODCALLTYPE OnShowWindow(/* [in] */ BOOL fShow);
	virtual HRESULT STDMETHODCALLTYPE RequestNewObjectLayout(void);

	// IOleInPlaceSite implementation
	virtual HRESULT STDMETHODCALLTYPE CanInPlaceActivate(void);
	virtual HRESULT STDMETHODCALLTYPE OnInPlaceActivate(void);
	virtual HRESULT STDMETHODCALLTYPE OnUIActivate(void);
	virtual HRESULT STDMETHODCALLTYPE GetWindowContext(/* [out] */ IOleInPlaceFrame __RPC_FAR *__RPC_FAR *ppFrame, /* [out] */ IOleInPlaceUIWindow __RPC_FAR *__RPC_FAR *ppDoc, /* [out] */ LPRECT lprcPosRect, /* [out] */ LPRECT lprcClipRect, /* [out][in] */ LPOLEINPLACEFRAMEINFO lpFrameInfo);
	virtual HRESULT STDMETHODCALLTYPE Scroll(/* [in] */ SIZE scrollExtant);
	virtual HRESULT STDMETHODCALLTYPE OnUIDeactivate(/* [in] */ BOOL fUndoable);
	virtual HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate(void);
	virtual HRESULT STDMETHODCALLTYPE DiscardUndoState(void);
	virtual HRESULT STDMETHODCALLTYPE DeactivateAndUndo(void);
	virtual HRESULT STDMETHODCALLTYPE OnPosRectChange(/* [in] */ LPCRECT lprcPosRect);

	// IOleControlSite implementation
	virtual HRESULT STDMETHODCALLTYPE OnControlInfoChanged(void);
	virtual HRESULT STDMETHODCALLTYPE LockInPlaceActive(/* [in] */ BOOL fLock);
	virtual HRESULT STDMETHODCALLTYPE GetExtendedControl(/* [out] */ IDispatch __RPC_FAR *__RPC_FAR *ppDisp);
	virtual HRESULT STDMETHODCALLTYPE TransformCoords(/* [out][in] */ POINTL __RPC_FAR *pPtlHimetric, /* [out][in] */ POINTF __RPC_FAR *pPtfContainer, /* [in] */ DWORD dwFlags);
	virtual HRESULT STDMETHODCALLTYPE TranslateAccelerator(/* [in] */ MSG __RPC_FAR *pMsg, /* [in] */ DWORD grfModifiers);
	virtual HRESULT STDMETHODCALLTYPE OnFocus(/* [in] */ BOOL fGotFocus);
	virtual HRESULT STDMETHODCALLTYPE ShowPropertyFrame( void);
};

#endif