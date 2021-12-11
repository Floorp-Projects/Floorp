// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0.If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "WebBrowser.h"
#include <mshtmdid.h>

WebBrowser::WebBrowser(HWND _hWndParent) : mHwndParent(_hWndParent) {
  // Whatever executed this constructor owns our first reference.
  AddRef();

  HRESULT hr = ::OleCreate(CLSID_WebBrowser, IID_IOleObject, OLERENDER_DRAW, 0,
                           this, this, (void**)&mOleObject);
  if (FAILED(hr)) {
    return;
  }

  RECT posRect;
  ::GetClientRect(mHwndParent, &posRect);

  hr = mOleObject->DoVerb(OLEIVERB_INPLACEACTIVATE, nullptr, this, 0,
                          mHwndParent, &posRect);
  if (FAILED(hr)) {
    mOleObject->Release();
    mOleObject = nullptr;
    return;
  }

  SetRect(posRect);

  hr = mOleObject->QueryInterface(&mWebBrowser2);
  if (FAILED(hr)) {
    mOleObject->Release();
    mOleObject = nullptr;
    return;
  }

  mWebBrowser2->put_Silent(VARIANT_TRUE);
}

WebBrowser::~WebBrowser() {
  if (mWebBrowser2) {
    mWebBrowser2->Release();
    mWebBrowser2 = nullptr;
  }
  if (mOleInPlaceActiveObject) {
    mOleInPlaceActiveObject->Release();
    mOleInPlaceActiveObject = nullptr;
  }
  if (mOleInPlaceObject) {
    mOleInPlaceObject->Release();
    mOleInPlaceObject = nullptr;
  }
  if (mOleObject) {
    mOleObject->Release();
    mOleObject = nullptr;
  }
}

void WebBrowser::Shutdown() {
  if (mOleObject) {
    mOleObject->Close(OLECLOSE_NOSAVE);
    mOleObject->SetClientSite(nullptr);
  }
}

bool WebBrowser::IsInitialized() { return mOleObject != nullptr; }

HRESULT WebBrowser::ActiveObjectTranslateAccelerator(bool tab, LPMSG lpmsg) {
  if (IsInitialized() && mOleInPlaceActiveObject) {
    HRESULT hr = mOleInPlaceActiveObject->TranslateAcceleratorW(lpmsg);
    if (hr == S_FALSE && tab) {
      // The browser control will give up focus, but it is the only control so
      // it would get focus again via IsDialogMessage. This does not result in
      // the focus returning to the web page, though, so instead let the
      // control process the tab again.
      hr = mOleInPlaceActiveObject->TranslateAcceleratorW(lpmsg);
    }
    return hr;
  } else {
    return S_FALSE;
  }
}

void WebBrowser::SetRect(const RECT& _rc) {
  mRect = _rc;

  if (mOleInPlaceObject) {
    mOleInPlaceObject->SetObjectRects(&mRect, &mRect);
  }
}

void WebBrowser::Resize(DWORD width, DWORD height) {
  RECT r = mRect;
  r.bottom = r.top + height;
  r.right = r.left + width;
  SetRect(r);
}

void WebBrowser::Navigate(wchar_t* url) {
  if (!IsInitialized()) {
    return;
  }

  VARIANT flags;
  VariantInit(&flags);
  flags.vt = VT_I4;
  flags.intVal = navNoHistory | navEnforceRestricted | navUntrustedForDownload |
                 navBlockRedirectsXDomain;

  mWebBrowser2->Navigate(url, &flags, nullptr, nullptr, nullptr);
}

void WebBrowser::AddCustomFunction(wchar_t* name, CustomFunction function,
                                   void* arg) {
  CustomFunctionRecord record = {name, function, arg};

// We've disabled exceptions but push_back can throw on an allocation
// failure, so we need to suppress a warning trying to tell us that
// that combination doesn't make any sense.
#pragma warning(suppress : 4530)
  mCustomFunctions.push_back(record);
}

//////////////////////////////////////////////////////////////////////////////
// IUnknown
//////////////////////////////////////////////////////////////////////////////
// This is a standard IUnknown implementation, we don't need anything special.

HRESULT STDMETHODCALLTYPE WebBrowser::QueryInterface(REFIID riid,
                                                     void** ppvObject) {
  if (riid == __uuidof(IUnknown)) {
    *ppvObject = static_cast<IOleClientSite*>(this);
  } else if (riid == __uuidof(IOleClientSite)) {
    *ppvObject = static_cast<IOleClientSite*>(this);
  } else if (riid == __uuidof(IOleInPlaceSite)) {
    *ppvObject = static_cast<IOleInPlaceSite*>(this);
  } else if (riid == __uuidof(IDropTarget)) {
    *ppvObject = static_cast<IDropTarget*>(this);
  } else if (riid == __uuidof(IStorage)) {
    *ppvObject = static_cast<IStorage*>(this);
  } else if (riid == __uuidof(IDocHostUIHandler)) {
    *ppvObject = static_cast<IDocHostUIHandler*>(this);
  } else if (riid == __uuidof(IDocHostShowUI)) {
    *ppvObject = static_cast<IDocHostShowUI*>(this);
  } else if (riid == __uuidof(IDispatch)) {
    *ppvObject = static_cast<IDispatch*>(this);
  } else {
    *ppvObject = nullptr;
    return E_NOINTERFACE;
  }

  AddRef();
  return S_OK;
}

ULONG STDMETHODCALLTYPE WebBrowser::AddRef() {
  return InterlockedIncrement(&mComRefCount);
}

ULONG STDMETHODCALLTYPE WebBrowser::Release() {
  ULONG refCount = InterlockedDecrement(&mComRefCount);
  if (refCount == 0) {
    delete this;
  }
  return refCount;
}

//////////////////////////////////////////////////////////////////////////////
// IOleWindow
//////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE
WebBrowser::GetWindow(__RPC__deref_out_opt HWND* phwnd) {
  *phwnd = mHwndParent;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBrowser::ContextSensitiveHelp(BOOL fEnterMode) {
  // We don't provide context-sensitive help.
  return E_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////////
// IOleInPlaceSite
//////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE WebBrowser::CanInPlaceActivate() {
  // We always support in-place activation.
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBrowser::OnInPlaceActivate() {
  OleLockRunning(mOleObject, TRUE, FALSE);
  mOleObject->QueryInterface(&mOleInPlaceObject);
  mOleInPlaceObject->QueryInterface(&mOleInPlaceActiveObject);
  mOleInPlaceObject->SetObjectRects(&mRect, &mRect);

  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBrowser::OnUIActivate() {
  // Nothing to do before activating the control's UI.
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBrowser::GetWindowContext(
    __RPC__deref_out_opt IOleInPlaceFrame** ppFrame,
    __RPC__deref_out_opt IOleInPlaceUIWindow** ppDoc,
    __RPC__out LPRECT lprcPosRect, __RPC__out LPRECT lprcClipRect,
    __RPC__inout LPOLEINPLACEFRAMEINFO lpFrameInfo) {
  *ppFrame = nullptr;
  *ppDoc = nullptr;
  *lprcPosRect = mRect;
  *lprcClipRect = mRect;

  lpFrameInfo->fMDIApp = false;
  lpFrameInfo->hwndFrame = mHwndParent;
  lpFrameInfo->haccel = nullptr;
  lpFrameInfo->cAccelEntries = 0;

  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBrowser::Scroll(SIZE scrollExtant) {
  // We should have disabled all scrollbars.
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebBrowser::OnUIDeactivate(BOOL fUndoable) {
  // Nothing to do after deactivating the control's UI.
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBrowser::OnInPlaceDeactivate() {
  if (mOleInPlaceObject) {
    mOleInPlaceObject->Release();
    mOleInPlaceObject = nullptr;
  }

  return S_OK;
}

// We don't support the concept of undo.
HRESULT STDMETHODCALLTYPE WebBrowser::DiscardUndoState() { return E_NOTIMPL; }

HRESULT STDMETHODCALLTYPE WebBrowser::DeactivateAndUndo() { return E_NOTIMPL; }

// We don't support moving or resizing the control.
HRESULT STDMETHODCALLTYPE
WebBrowser::OnPosRectChange(__RPC__in LPCRECT lprcPosRect) {
  return E_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////////
// IOleClientSite
//////////////////////////////////////////////////////////////////////////////
// We don't need anything that IOleClientSite does, because we're doing OLE
// only in the most basic sense and we don't support linking (or, indeed,
// embedding), but some implementation of this interface is required for
// OleCreate to work, so we have to have a stub version.

HRESULT STDMETHODCALLTYPE WebBrowser::SaveObject() { return E_NOTIMPL; }

HRESULT STDMETHODCALLTYPE
WebBrowser::GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker,
                       __RPC__deref_out_opt IMoniker** ppmk) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WebBrowser::GetContainer(__RPC__deref_out_opt IOleContainer** ppContainer) {
  *ppContainer = nullptr;
  return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE WebBrowser::ShowObject() { return S_OK; }

HRESULT STDMETHODCALLTYPE WebBrowser::OnShowWindow(BOOL fShow) { return S_OK; }

HRESULT STDMETHODCALLTYPE WebBrowser::RequestNewObjectLayout() {
  return E_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////////
// IDropTarget
//////////////////////////////////////////////////////////////////////////////
// This is a stub implementation which blocks all dropping. The main reason we
// want to do that is prevent accidentally dropping something on the control
// and having it navigate, because there's no recovering from that except to
// restart the app, and also it would look ridiculous. There could also be
// security implications though, which we'd rather just avoid engaging with
// altogether if we can.

HRESULT STDMETHODCALLTYPE
WebBrowser::DragEnter(__RPC__in_opt IDataObject* pDataObj, DWORD grfKeyState,
                      POINTL pt, __RPC__inout DWORD* pdwEffect) {
  *pdwEffect = DROPEFFECT_NONE;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBrowser::DragOver(DWORD grfKeyState, POINTL pt,
                                               __RPC__inout DWORD* pdwEffect) {
  *pdwEffect = DROPEFFECT_NONE;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBrowser::DragLeave() { return S_OK; }

HRESULT STDMETHODCALLTYPE WebBrowser::Drop(__RPC__in_opt IDataObject* pDataObj,
                                           DWORD grfKeyState, POINTL pt,
                                           __RPC__inout DWORD* pdwEffect) {
  *pdwEffect = DROPEFFECT_NONE;
  return S_OK;
}

//////////////////////////////////////////////////////////////////////////////
// IStorage
//////////////////////////////////////////////////////////////////////////////
// We don't need anything that IStorage does, but we have to pass some
// implementation of it to OleCreate, so we need to have a stub version.

HRESULT STDMETHODCALLTYPE WebBrowser::CreateStream(
    __RPC__in_string const OLECHAR* pwcsName, DWORD grfMode, DWORD reserved1,
    DWORD reserved2, __RPC__deref_out_opt IStream** ppstm) {
  *ppstm = nullptr;
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebBrowser::OpenStream(const OLECHAR* pwcsName,
                                                 void* reserved1, DWORD grfMode,
                                                 DWORD reserved2,
                                                 IStream** ppstm) {
  *ppstm = nullptr;
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebBrowser::CreateStorage(
    __RPC__in_string const OLECHAR* pwcsName, DWORD grfMode, DWORD reserved1,
    DWORD reserved2, __RPC__deref_out_opt IStorage** ppstg) {
  *ppstg = nullptr;
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WebBrowser::OpenStorage(__RPC__in_opt_string const OLECHAR* pwcsName,
                        __RPC__in_opt IStorage* pstgPriority, DWORD grfMode,
                        __RPC__deref_opt_in_opt SNB snbExclude, DWORD reserved,
                        __RPC__deref_out_opt IStorage** ppstg) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebBrowser::CopyTo(DWORD ciidExclude,
                                             const IID* rgiidExclude,
                                             __RPC__in_opt SNB snbExclude,
                                             IStorage* pstgDest) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebBrowser::MoveElementTo(
    __RPC__in_string const OLECHAR* pwcsName, __RPC__in_opt IStorage* pstgDest,
    __RPC__in_string const OLECHAR* pwcsNewName, DWORD grfFlags) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebBrowser::Commit(DWORD grfCommitFlags) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebBrowser::Revert() { return E_NOTIMPL; }

HRESULT STDMETHODCALLTYPE WebBrowser::EnumElements(DWORD reserved1,
                                                   void* reserved2,
                                                   DWORD reserved3,
                                                   IEnumSTATSTG** ppenum) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WebBrowser::DestroyElement(__RPC__in_string const OLECHAR* pwcsName) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WebBrowser::RenameElement(__RPC__in_string const OLECHAR* pwcsOldName,
                          __RPC__in_string const OLECHAR* pwcsNewName) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebBrowser::SetElementTimes(
    __RPC__in_opt_string const OLECHAR* pwcsName,
    __RPC__in_opt const FILETIME* pctime, __RPC__in_opt const FILETIME* patime,
    __RPC__in_opt const FILETIME* pmtime) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebBrowser::SetClass(__RPC__in REFCLSID clsid) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBrowser::SetStateBits(DWORD grfStateBits,
                                                   DWORD grfMask) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebBrowser::Stat(__RPC__out STATSTG* pstatstg,
                                           DWORD grfStatFlag) {
  return E_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////////
// IDocHostUIHandler
//////////////////////////////////////////////////////////////////////////////
// Our implementation for this interface is basically all about disabling
// things that we don't want/need.

HRESULT __stdcall WebBrowser::ShowContextMenu(DWORD dwID, POINT* ppt,
                                              IUnknown* pcmdtReserved,
                                              IDispatch* pdispReserved) {
  // Returning S_OK signals that we've handled the request for a context menu
  // (which we did, by doing nothing), so the control won't try to open one.
  return S_OK;
}

HRESULT __stdcall WebBrowser::GetHostInfo(DOCHOSTUIINFO* pInfo) {
  pInfo->cbSize = sizeof(DOCHOSTUIINFO);
  pInfo->dwFlags =
      DOCHOSTUIFLAG_DIALOG | DOCHOSTUIFLAG_DISABLE_HELP_MENU |
      DOCHOSTUIFLAG_NO3DBORDER | DOCHOSTUIFLAG_SCROLL_NO |
      DOCHOSTUIFLAG_OPENNEWWIN | DOCHOSTUIFLAG_OVERRIDEBEHAVIORFACTORY |
      DOCHOSTUIFLAG_THEME | DOCHOSTUIFLAG_LOCAL_MACHINE_ACCESS_CHECK |
      DOCHOSTUIFLAG_DISABLE_UNTRUSTEDPROTOCOL | DOCHOSTUIFLAG_DPI_AWARE;
  pInfo->dwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;
  pInfo->pchHostCss = nullptr;
  pInfo->pchHostNS = nullptr;
  return S_OK;
}

HRESULT __stdcall WebBrowser::ShowUI(DWORD dwID,
                                     IOleInPlaceActiveObject* pActiveObject,
                                     IOleCommandTarget* pCommandTarget,
                                     IOleInPlaceFrame* pFrame,
                                     IOleInPlaceUIWindow* pDoc) {
  return E_NOTIMPL;
}

HRESULT __stdcall WebBrowser::HideUI() { return E_NOTIMPL; }

HRESULT __stdcall WebBrowser::UpdateUI() { return E_NOTIMPL; }

HRESULT __stdcall WebBrowser::EnableModeless(BOOL fEnable) { return E_NOTIMPL; }

HRESULT __stdcall WebBrowser::OnDocWindowActivate(BOOL fActivate) {
  return E_NOTIMPL;
}

HRESULT __stdcall WebBrowser::OnFrameWindowActivate(BOOL fActivate) {
  return E_NOTIMPL;
}

HRESULT __stdcall WebBrowser::ResizeBorder(LPCRECT prcBorder,
                                           IOleInPlaceUIWindow* pUIWindow,
                                           BOOL fRameWindow) {
  return E_NOTIMPL;
}

HRESULT __stdcall WebBrowser::TranslateAccelerator(LPMSG lpMsg,
                                                   const GUID* pguidCmdGroup,
                                                   DWORD nCmdID) {
  return S_FALSE;
}

HRESULT __stdcall WebBrowser::GetOptionKeyPath(LPOLESTR* pchKey, DWORD dw) {
  return E_NOTIMPL;
}

HRESULT __stdcall WebBrowser::GetDropTarget(IDropTarget* pDropTarget,
                                            IDropTarget** ppDropTarget) {
  // The IDropTarget implementation that we need is an empty stub, so we'll do
  // the easy and convenient thing and just use this object.
  return QueryInterface(IID_PPV_ARGS(ppDropTarget));
}

HRESULT __stdcall WebBrowser::GetExternal(IDispatch** ppDispatch) {
  // This object has to implement IDispatch anyway so that we can use
  // DISPID_AMBIENT_DLCONTROL, so we'll make this the external handler also.
  return QueryInterface(IID_PPV_ARGS(ppDispatch));
}

HRESULT __stdcall WebBrowser::TranslateUrl(DWORD dwTranslate, LPWSTR pchURLIn,
                                           LPWSTR* ppchURLOut) {
  *ppchURLOut = nullptr;
  return E_NOTIMPL;
}

HRESULT __stdcall WebBrowser::FilterDataObject(IDataObject* pDO,
                                               IDataObject** ppDORet) {
  *ppDORet = nullptr;
  return E_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////////
// IDocHostShowUI
//////////////////////////////////////////////////////////////////////////////

HRESULT __stdcall WebBrowser::ShowMessage(HWND hwnd, LPOLESTR lpstrText,
                                          LPOLESTR lpstrCaption, DWORD dwType,
                                          LPOLESTR lpstrHelpFile,
                                          DWORD dwHelpContext,
                                          LRESULT* plResult) {
  // Don't allow MSHTML to generate message boxes.
  return S_OK;
}

HRESULT __stdcall WebBrowser::ShowHelp(HWND hwnd, LPOLESTR pszHelpFile,
                                       UINT uCommand, DWORD dwData,
                                       POINT ptMouse,
                                       IDispatch* pDispatchObjectHit) {
  // Don't allow MSHTML to show any help.
  return S_OK;
}

//////////////////////////////////////////////////////////////////////////////
// IDispatch
//////////////////////////////////////////////////////////////////////////////

// We're not using a type library.
HRESULT __stdcall WebBrowser::GetTypeInfoCount(UINT* pctinfo) {
  if (pctinfo) {
    *pctinfo = 0;
  }
  return S_OK;
}

HRESULT __stdcall WebBrowser::GetTypeInfo(UINT iTInfo, LCID lcid,
                                          ITypeInfo** ppTInfo) {
  return E_NOTIMPL;
}

HRESULT __stdcall WebBrowser::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames,
                                            UINT cNames, LCID lcid,
                                            DISPID* rgDispId) {
  if (cNames != 1) {
    return E_NOTIMPL;
  }

  for (size_t i = 0; i < mCustomFunctions.size(); ++i) {
    if (mCustomFunctions[i].mName == rgszNames[0]) {
      // DISPID values need to be 1-indexed because 0 is reserved
      // (DISPID_VALUE).
      *rgDispId = i + 1;
      return S_OK;
    }
  }

  *rgDispId = DISPID_UNKNOWN;
  return DISP_E_UNKNOWNNAME;
}

HRESULT __stdcall WebBrowser::Invoke(DISPID dispIdMember, REFIID riid,
                                     LCID lcid, WORD wFlags,
                                     DISPPARAMS* pDispParams,
                                     VARIANT* pVarResult, EXCEPINFO* pExcepInfo,
                                     UINT* puArgErr) {
  if (dispIdMember == DISPID_AMBIENT_DLCONTROL && pVarResult) {
    VariantClear(pVarResult);
    pVarResult->vt = VT_I4;
    // As a light security measure, disable a bunch of stuff we don't want
    // to be able to run in the web control.
    pVarResult->intVal = DLCTL_NO_JAVA | DLCTL_NO_DLACTIVEXCTLS |
                         DLCTL_NO_RUNACTIVEXCTLS | DLCTL_NO_FRAMEDOWNLOAD |
                         DLCTL_NO_BEHAVIORS | DLCTL_NO_CLIENTPULL |
                         DLCTL_NOFRAMES | DLCTL_FORCEOFFLINE | DLCTL_SILENT |
                         DLCTL_OFFLINE | DLCTL_DLIMAGES;
    return S_OK;
  }

  // Otherwise this should be one of our custom functions.
  // We only support invoking these as methods, not property access.
  if ((wFlags & DISPATCH_METHOD) == 0) {
    return DISP_E_TYPEMISMATCH;
  }

  // Make sure this DISPID is valid in our custom functions list.
  // DISPID values are 1-indexed because 0 is reserved (DISPID_VALUE).
  DISPID customFunctionIndex = dispIdMember - 1;
  if (customFunctionIndex < 0 ||
      customFunctionIndex >= (DISPID)mCustomFunctions.size()) {
    return DISP_E_MEMBERNOTFOUND;
  }

  // If the caller passed an argument to this custom function, use it.
  // If not, make an empty VARIANT we can pass to it instead.
  VARIANT argument;
  VariantInit(&argument);
  if (pDispParams->cArgs > 0) {
    argument = pDispParams->rgvarg[0];
  }

  CustomFunctionRecord foundFunction = mCustomFunctions[customFunctionIndex];
  foundFunction.mFunction(foundFunction.mArg, argument, pVarResult);

  return S_OK;
}
