// CBrowserCtlSite.cpp : Implementation of CBrowserCtlSite
#include "stdafx.h"
#include "Cbrowse.h"
#include "CBrowserCtlSite.h"

/////////////////////////////////////////////////////////////////////////////
// CBrowserCtlSite

/////////////////////////////////////////////////////////////////////////////
// IDocHostUIHandler

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::ShowContextMenu(/* [in] */ DWORD dwID, /* [in] */ POINT __RPC_FAR *ppt, /* [in] */ IUnknown __RPC_FAR *pcmdtReserved, /* [in] */ IDispatch __RPC_FAR *pdispReserved)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::GetHostInfo(/* [out][in] */ DOCHOSTUIINFO __RPC_FAR *pInfo)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::ShowUI(/* [in] */ DWORD dwID, /* [in] */ IOleInPlaceActiveObject __RPC_FAR *pActiveObject, /* [in] */ IOleCommandTarget __RPC_FAR *pCommandTarget, /* [in] */ IOleInPlaceFrame __RPC_FAR *pFrame, /* [in] */ IOleInPlaceUIWindow __RPC_FAR *pDoc)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::HideUI(void)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::UpdateUI(void)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::EnableModeless(/* [in] */ BOOL fEnable)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::OnDocWindowActivate(/* [in] */ BOOL fActivate)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::OnFrameWindowActivate(/* [in] */ BOOL fActivate)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::ResizeBorder(/* [in] */ LPCRECT prcBorder, /* [in] */ IOleInPlaceUIWindow __RPC_FAR *pUIWindow, /* [in] */ BOOL fRameWindow)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::TranslateAccelerator(/* [in] */ LPMSG lpMsg, /* [in] */ const GUID __RPC_FAR *pguidCmdGroup, /* [in] */ DWORD nCmdID)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::GetOptionKeyPath(/* [out] */ LPOLESTR __RPC_FAR *pchKey, /* [in] */ DWORD dw)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::GetDropTarget(/* [in] */ IDropTarget __RPC_FAR *pDropTarget, /* [out] */ IDropTarget __RPC_FAR *__RPC_FAR *ppDropTarget)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::GetExternal(/* [out] */ IDispatch __RPC_FAR *__RPC_FAR *ppDispatch)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::TranslateUrl(/* [in] */ DWORD dwTranslate, /* [in] */ OLECHAR __RPC_FAR *pchURLIn, /* [out] */ OLECHAR __RPC_FAR *__RPC_FAR *ppchURLOut)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::FilterDataObject(/* [in] */ IDataObject __RPC_FAR *pDO, /* [out] */ IDataObject __RPC_FAR *__RPC_FAR *ppDORet)
{
	return E_NOTIMPL;
}


///////////////////////////////////////////////////////////////////////////////
// IDocHostShowUI

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::ShowMessage(/* [in] */ HWND hwnd, /* [in] */ LPOLESTR lpstrText, /* [in] */ LPOLESTR lpstrCaption, /* [in] */ DWORD dwType, /* [in] */ LPOLESTR lpstrHelpFile, /* [in] */ DWORD dwHelpContext,/* [out] */ LRESULT __RPC_FAR *plResult)
{
	return S_FALSE;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::ShowHelp(/* [in] */ HWND hwnd, /* [in] */ LPOLESTR pszHelpFile, /* [in] */ UINT uCommand, /* [in] */ DWORD dwData, /* [in] */ POINT ptMouse, /* [out] */ IDispatch __RPC_FAR *pDispatchObjectHit)
{
	return S_FALSE;
}

