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
						public IOleInPlaceSiteWindowless,
						public IOleControlSite,
						public IAdviseSinkEx,
						public IOleItemContainer,
						public IDispatch
{
	// Handle to parent window
	HWND m_hWndParent;
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
	// Flag indicating if the site allows windowless controls
	BOOL m_bSupportWindowlessActivation;
	// Flag indicating if control is windowless
	BOOL m_bWindowless;

	// Raw pointer to the object
	CComPtr<IUnknown> m_spObject;
	// Pointer to objects IViewObject interface
	CComQIPtr<IViewObject, &IID_IViewObject> m_spIViewObject;
	// Pointer to object's IOleObject interface
	CComQIPtr<IOleObject, &IID_IOleObject> m_spIOleObject;

	// Pointer to object's IOleInPlaceObject interface
	CComQIPtr<IOleInPlaceObject, &IID_IOleInPlaceObject> m_spIOleInPlaceObject;
	// Pointer to object's IOleInPlaceObjectWindowless interface
	CComQIPtr<IOleInPlaceObjectWindowless, &IID_IOleInPlaceObjectWindowless> m_spIOleInPlaceObjectWindowless;

	// Name of this control
	tstring m_szName;
	// CLSID of the control
	CLSID m_clsid;
	// Parameter list
	PropertyList m_ParameterList;
	// List of controls
	static std::list<CControlSite *> m_cControlList;

	// Double buffer drawing variables used for windowless controls
	
	// Area of buffer
	RECT m_rcBuffer;
	// Bitmap to buffer
	HBITMAP m_hBMBuffer;
	// Bitmap to buffer
	HBITMAP m_hBMBufferOld;
	// Device context
	HDC m_hDCBuffer;
	// Clipping area of site
	HRGN m_hRgnBuffer;
	// Flags indicating how the buffer was painted
	DWORD m_dwBufferFlags;


	// Ambient properties

	// Locale ID
	LCID m_nAmbientLocale;
	// Foreground colour
	COLORREF m_clrAmbientForeColor;
	// Background colour
	COLORREF m_clrAmbientBackColor;
	// Flag indicating if control should hatch itself
	bool m_bAmbientShowHatching;
	// Flag indicating if control should have grab handles
	bool m_bAmbientShowGrabHandles;
	// Flag indicating if control is in edit/user mode
	bool m_bAmbientUserMode;

public:
	CControlSite();
	virtual ~CControlSite();

BEGIN_COM_MAP(CControlSite)
	COM_INTERFACE_ENTRY(IOleWindow)
	COM_INTERFACE_ENTRY(IOleClientSite)
	COM_INTERFACE_ENTRY(IOleInPlaceSite)
	COM_INTERFACE_ENTRY_IID(IID_IOleInPlaceSite, IOleInPlaceSiteWindowless)
	COM_INTERFACE_ENTRY_IID(IID_IOleInPlaceSiteEx, IOleInPlaceSiteWindowless)
	COM_INTERFACE_ENTRY_IID(IID_IOleInPlaceSiteWindowless, IOleInPlaceSiteWindowless)
	COM_INTERFACE_ENTRY(IOleControlSite)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY_IID(IID_IAdviseSink, IAdviseSinkEx)
	COM_INTERFACE_ENTRY_IID(IID_IAdviseSink2, IAdviseSinkEx)
	COM_INTERFACE_ENTRY_IID(IID_IAdviseSinkEx, IAdviseSinkEx)
	COM_INTERFACE_ENTRY_IID(IID_IParseDisplayName, IOleItemContainer)
	COM_INTERFACE_ENTRY_IID(IID_IOleContainer, IOleItemContainer)
	COM_INTERFACE_ENTRY_IID(IID_IOleItemContainer, IOleItemContainer)
END_COM_MAP()

	virtual HRESULT Create(REFCLSID clsid, PropertyList &pl, const tstring szName = _T(""));
	virtual HRESULT Attach(HWND hwndParent, const RECT &rcPos, IUnknown *pInitStream);
	virtual HRESULT Detach();
	virtual HRESULT GetControlUnknown(IUnknown **ppObject);
	virtual HRESULT SetPosition(const RECT &rcPos);
	virtual HRESULT Draw(HDC hdc);
	virtual HRESULT DoVerb(LONG nVerb, LPMSG lpMsg = NULL);

	virtual const CLSID &GetObjectCLSID() const
	{
		return m_clsid;
	}

	virtual const tstring &GetObjectName() const
	{
		return m_szName;
	}

	virtual HWND GetParentWindow() const
	{
		return m_hWndParent;
	}

	virtual BOOL IsInPlaceActive() const
	{
		return m_bInPlaceActive;
	}

	// IDispatch
	virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(/* [out] */ UINT __RPC_FAR *pctinfo);
	virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(/* [in] */ UINT iTInfo, /* [in] */ LCID lcid, /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
	virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(/* [in] */ REFIID riid, /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames, /* [in] */ UINT cNames, /* [in] */ LCID lcid, /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
	virtual /* [local] */ HRESULT STDMETHODCALLTYPE Invoke(/* [in] */ DISPID dispIdMember, /* [in] */ REFIID riid, /* [in] */ LCID lcid, /* [in] */ WORD wFlags, /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams, /* [out] */ VARIANT __RPC_FAR *pVarResult, /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo, /* [out] */ UINT __RPC_FAR *puArgErr);

	// IAdviseSink implementation
    virtual /* [local] */ void STDMETHODCALLTYPE OnDataChange(/* [unique][in] */ FORMATETC __RPC_FAR *pFormatetc, /* [unique][in] */ STGMEDIUM __RPC_FAR *pStgmed);
    virtual /* [local] */ void STDMETHODCALLTYPE OnViewChange(/* [in] */ DWORD dwAspect, /* [in] */ LONG lindex);
    virtual /* [local] */ void STDMETHODCALLTYPE OnRename(/* [in] */ IMoniker __RPC_FAR *pmk);
    virtual /* [local] */ void STDMETHODCALLTYPE OnSave(void);
    virtual /* [local] */ void STDMETHODCALLTYPE OnClose(void);

	// IAdviseSink2
	virtual /* [local] */ void STDMETHODCALLTYPE OnLinkSrcChange(/* [unique][in] */ IMoniker __RPC_FAR *pmk);

	// IAdviseSinkEx implementation
    virtual /* [local] */ void STDMETHODCALLTYPE OnViewStatusChange(/* [in] */ DWORD dwViewStatus);

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

	// IOleInPlaceSiteEx implementation
	virtual HRESULT STDMETHODCALLTYPE OnInPlaceActivateEx(/* [out] */ BOOL __RPC_FAR *pfNoRedraw, /* [in] */ DWORD dwFlags);
	virtual HRESULT STDMETHODCALLTYPE OnInPlaceDeactivateEx(/* [in] */ BOOL fNoRedraw);
	virtual HRESULT STDMETHODCALLTYPE RequestUIActivate(void);

	// IOleInPlaceSiteWindowless implementation
	virtual HRESULT STDMETHODCALLTYPE CanWindowlessActivate(void);
	virtual HRESULT STDMETHODCALLTYPE GetCapture(void);
	virtual HRESULT STDMETHODCALLTYPE SetCapture(/* [in] */ BOOL fCapture);
	virtual HRESULT STDMETHODCALLTYPE GetFocus(void);
	virtual HRESULT STDMETHODCALLTYPE SetFocus(/* [in] */ BOOL fFocus);
	virtual HRESULT STDMETHODCALLTYPE GetDC(/* [in] */ LPCRECT pRect, /* [in] */ DWORD grfFlags, /* [out] */ HDC __RPC_FAR *phDC);
	virtual HRESULT STDMETHODCALLTYPE ReleaseDC(/* [in] */ HDC hDC);
	virtual HRESULT STDMETHODCALLTYPE InvalidateRect(/* [in] */ LPCRECT pRect, /* [in] */ BOOL fErase);
	virtual HRESULT STDMETHODCALLTYPE InvalidateRgn(/* [in] */ HRGN hRGN, /* [in] */ BOOL fErase);
	virtual HRESULT STDMETHODCALLTYPE ScrollRect(/* [in] */ INT dx, /* [in] */ INT dy, /* [in] */ LPCRECT pRectScroll, /* [in] */ LPCRECT pRectClip);
	virtual HRESULT STDMETHODCALLTYPE AdjustRect(/* [out][in] */ LPRECT prc);
	virtual HRESULT STDMETHODCALLTYPE OnDefWindowMessage(/* [in] */ UINT msg, /* [in] */ WPARAM wParam, /* [in] */ LPARAM lParam, /* [out] */ LRESULT __RPC_FAR *plResult);

	// IOleControlSite implementation
	virtual HRESULT STDMETHODCALLTYPE OnControlInfoChanged(void);
	virtual HRESULT STDMETHODCALLTYPE LockInPlaceActive(/* [in] */ BOOL fLock);
	virtual HRESULT STDMETHODCALLTYPE GetExtendedControl(/* [out] */ IDispatch __RPC_FAR *__RPC_FAR *ppDisp);
	virtual HRESULT STDMETHODCALLTYPE TransformCoords(/* [out][in] */ POINTL __RPC_FAR *pPtlHimetric, /* [out][in] */ POINTF __RPC_FAR *pPtfContainer, /* [in] */ DWORD dwFlags);
	virtual HRESULT STDMETHODCALLTYPE TranslateAccelerator(/* [in] */ MSG __RPC_FAR *pMsg, /* [in] */ DWORD grfModifiers);
	virtual HRESULT STDMETHODCALLTYPE OnFocus(/* [in] */ BOOL fGotFocus);
	virtual HRESULT STDMETHODCALLTYPE ShowPropertyFrame( void);

	// IParseDisplayName implementation
	virtual HRESULT STDMETHODCALLTYPE ParseDisplayName(/* [unique][in] */ IBindCtx __RPC_FAR *pbc, /* [in] */ LPOLESTR pszDisplayName, /* [out] */ ULONG __RPC_FAR *pchEaten, /* [out] */ IMoniker __RPC_FAR *__RPC_FAR *ppmkOut);

	// IOleContainer implementation
	virtual HRESULT STDMETHODCALLTYPE EnumObjects(/* [in] */ DWORD grfFlags, /* [out] */ IEnumUnknown __RPC_FAR *__RPC_FAR *ppenum);
	virtual HRESULT STDMETHODCALLTYPE LockContainer(/* [in] */ BOOL fLock);

	// IOleItemContainer implementation
	virtual /* [local] */ HRESULT STDMETHODCALLTYPE GetObject(/* [in] */ LPOLESTR pszItem, /* [in] */ DWORD dwSpeedNeeded, /* [unique][in] */ IBindCtx __RPC_FAR *pbc, /* [in] */ REFIID riid, /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
	virtual /* [local] */ HRESULT STDMETHODCALLTYPE GetObjectStorage(/* [in] */ LPOLESTR pszItem, /* [unique][in] */ IBindCtx __RPC_FAR *pbc, /* [in] */ REFIID riid, /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvStorage);
	virtual HRESULT STDMETHODCALLTYPE IsRunning(/* [in] */ LPOLESTR pszItem);  
};

typedef CComObject<CControlSite> CControlSiteInstance;

#endif