// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0.If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <exdisp.h>
#include <mshtmhst.h>

#include <vector>
#include <string>

class WebBrowser final :
    /* public IUnknown, */
    /* public IOleWindow, */
    public IOleInPlaceSite,
    public IOleClientSite,
    public IDropTarget,
    public IStorage,
    public IDocHostUIHandler,
    public IDocHostShowUI,
    public IDispatch {
 public:
  /////////////////////////////////////////////////////////////////////////////
  // Our own methods
  /////////////////////////////////////////////////////////////////////////////
  WebBrowser(HWND hWndParent);
  ~WebBrowser();

  WebBrowser(const WebBrowser&) = delete;
  WebBrowser& operator=(const WebBrowser&) = delete;

  void Shutdown();

  bool IsInitialized();

  HRESULT ActiveObjectTranslateAccelerator(bool tab, LPMSG lpmsg);
  void SetRect(const RECT& _rc);
  void Resize(DWORD width, DWORD height);
  void Navigate(wchar_t* szUrl);

  using CustomFunction = void (*)(void* context, VARIANT parameter,
                                  VARIANT* retVal);
  void AddCustomFunction(wchar_t* name, CustomFunction function, void* arg);

  /////////////////////////////////////////////////////////////////////////////
  // Data members
  /////////////////////////////////////////////////////////////////////////////
 private:
  IOleObject* mOleObject = nullptr;
  IOleInPlaceObject* mOleInPlaceObject = nullptr;
  IOleInPlaceActiveObject* mOleInPlaceActiveObject = nullptr;
  IWebBrowser2* mWebBrowser2 = nullptr;

  LONG mComRefCount = 0;

  RECT mRect = {0, 0, 0, 0};

  HWND mHwndParent = nullptr;

  struct CustomFunctionRecord {
    std::wstring mName;
    CustomFunction mFunction;
    void* mArg;
  };
  std::vector<CustomFunctionRecord> mCustomFunctions;

  //////////////////////////////////////////////////////////////////////////////
  // COM interface methods
  //////////////////////////////////////////////////////////////////////////////
 public:
  // IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void** ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  // IOleWindow
  HRESULT STDMETHODCALLTYPE
  GetWindow(__RPC__deref_out_opt HWND* phwnd) override;
  HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL fEnterMode) override;

  // IOleInPlaceSite
  HRESULT STDMETHODCALLTYPE CanInPlaceActivate() override;
  HRESULT STDMETHODCALLTYPE OnInPlaceActivate() override;
  HRESULT STDMETHODCALLTYPE OnUIActivate() override;
  HRESULT STDMETHODCALLTYPE GetWindowContext(
      __RPC__deref_out_opt IOleInPlaceFrame** ppFrame,
      __RPC__deref_out_opt IOleInPlaceUIWindow** ppDoc,
      __RPC__out LPRECT lprcPosRect, __RPC__out LPRECT lprcClipRect,
      __RPC__inout LPOLEINPLACEFRAMEINFO lpFrameInfo) override;
  HRESULT STDMETHODCALLTYPE Scroll(SIZE scrollExtant) override;
  HRESULT STDMETHODCALLTYPE OnUIDeactivate(BOOL fUndoable) override;
  HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate() override;
  HRESULT STDMETHODCALLTYPE DiscardUndoState() override;
  HRESULT STDMETHODCALLTYPE DeactivateAndUndo() override;
  HRESULT STDMETHODCALLTYPE
  OnPosRectChange(__RPC__in LPCRECT lprcPosRect) override;

  // IOleClientSite
  HRESULT STDMETHODCALLTYPE SaveObject() override;
  HRESULT STDMETHODCALLTYPE
  GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker,
             __RPC__deref_out_opt IMoniker** ppmk) override;
  HRESULT STDMETHODCALLTYPE
  GetContainer(__RPC__deref_out_opt IOleContainer** ppContainer) override;
  HRESULT STDMETHODCALLTYPE ShowObject() override;
  HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL fShow) override;
  HRESULT STDMETHODCALLTYPE RequestNewObjectLayout() override;

  // IDropTarget
  HRESULT STDMETHODCALLTYPE DragEnter(__RPC__in_opt IDataObject* pDataObj,
                                      DWORD grfKeyState, POINTL pt,
                                      __RPC__inout DWORD* pdwEffect) override;
  HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt,
                                     __RPC__inout DWORD* pdwEffect) override;
  HRESULT STDMETHODCALLTYPE DragLeave() override;
  HRESULT STDMETHODCALLTYPE Drop(__RPC__in_opt IDataObject* pDataObj,
                                 DWORD grfKeyState, POINTL pt,
                                 __RPC__inout DWORD* pdwEffect) override;

  // IStorage
  HRESULT STDMETHODCALLTYPE CreateStream(
      __RPC__in_string const OLECHAR* pwcsName, DWORD grfMode, DWORD reserved1,
      DWORD reserved2, __RPC__deref_out_opt IStream** ppstm) override;
  HRESULT STDMETHODCALLTYPE OpenStream(const OLECHAR* pwcsName, void* reserved1,
                                       DWORD grfMode, DWORD reserved2,
                                       IStream** ppstm) override;
  HRESULT STDMETHODCALLTYPE CreateStorage(
      __RPC__in_string const OLECHAR* pwcsName, DWORD grfMode, DWORD reserved1,
      DWORD reserved2, __RPC__deref_out_opt IStorage** ppstg) override;
  HRESULT STDMETHODCALLTYPE
  OpenStorage(__RPC__in_opt_string const OLECHAR* pwcsName,
              __RPC__in_opt IStorage* pstgPriority, DWORD grfMode,
              __RPC__deref_opt_in_opt SNB snbExclude, DWORD reserved,
              __RPC__deref_out_opt IStorage** ppstg) override;
  HRESULT STDMETHODCALLTYPE CopyTo(DWORD ciidExclude, const IID* rgiidExclude,
                                   __RPC__in_opt SNB snbExclude,
                                   IStorage* pstgDest) override;
  HRESULT STDMETHODCALLTYPE MoveElementTo(
      __RPC__in_string const OLECHAR* pwcsName,
      __RPC__in_opt IStorage* pstgDest,
      __RPC__in_string const OLECHAR* pwcsNewName, DWORD grfFlags) override;
  HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags) override;
  HRESULT STDMETHODCALLTYPE Revert(void) override;
  HRESULT STDMETHODCALLTYPE EnumElements(DWORD reserved1, void* reserved2,
                                         DWORD reserved3,
                                         IEnumSTATSTG** ppenum) override;
  HRESULT STDMETHODCALLTYPE
  DestroyElement(__RPC__in_string const OLECHAR* pwcsName) override;
  HRESULT STDMETHODCALLTYPE
  RenameElement(__RPC__in_string const OLECHAR* pwcsOldName,
                __RPC__in_string const OLECHAR* pwcsNewName) override;
  HRESULT STDMETHODCALLTYPE
  SetElementTimes(__RPC__in_opt_string const OLECHAR* pwcsName,
                  __RPC__in_opt const FILETIME* pctime,
                  __RPC__in_opt const FILETIME* patime,
                  __RPC__in_opt const FILETIME* pmtime) override;
  HRESULT STDMETHODCALLTYPE SetClass(__RPC__in REFCLSID clsid) override;
  HRESULT STDMETHODCALLTYPE SetStateBits(DWORD grfStateBits,
                                         DWORD grfMask) override;
  HRESULT STDMETHODCALLTYPE Stat(__RPC__out STATSTG* pstatstg,
                                 DWORD grfStatFlag) override;

  // IDocHostUIHandler
  HRESULT STDMETHODCALLTYPE ShowContextMenu(
      _In_ DWORD dwID, _In_ POINT* ppt, _In_ IUnknown* pcmdtReserved,
      _In_ IDispatch* pdispReserved) override;
  HRESULT STDMETHODCALLTYPE GetHostInfo(_Inout_ DOCHOSTUIINFO* pInfo) override;
  HRESULT STDMETHODCALLTYPE ShowUI(_In_ DWORD dwID,
                                   _In_ IOleInPlaceActiveObject* pActiveObject,
                                   _In_ IOleCommandTarget* pCommandTarget,
                                   _In_ IOleInPlaceFrame* pFrame,
                                   _In_ IOleInPlaceUIWindow* pDoc) override;
  HRESULT STDMETHODCALLTYPE HideUI() override;
  HRESULT STDMETHODCALLTYPE UpdateUI() override;
  HRESULT STDMETHODCALLTYPE EnableModeless(BOOL fEnable) override;
  HRESULT STDMETHODCALLTYPE OnDocWindowActivate(BOOL fActivate) override;
  HRESULT STDMETHODCALLTYPE OnFrameWindowActivate(BOOL fActivate) override;
  HRESULT STDMETHODCALLTYPE ResizeBorder(_In_ LPCRECT prcBorder,
                                         _In_ IOleInPlaceUIWindow* pUIWindow,
                                         _In_ BOOL fRameWindow) override;
  HRESULT STDMETHODCALLTYPE TranslateAccelerator(LPMSG lpMsg,
                                                 const GUID* pguidCmdGroup,
                                                 DWORD nCmdID) override;
  HRESULT STDMETHODCALLTYPE GetOptionKeyPath(_Out_ LPOLESTR* pchKey,
                                             DWORD dw) override;
  HRESULT STDMETHODCALLTYPE
  GetDropTarget(_In_ IDropTarget* pDropTarget,
                _Outptr_ IDropTarget** ppDropTarget) override;
  HRESULT STDMETHODCALLTYPE
  GetExternal(_Outptr_result_maybenull_ IDispatch** ppDispatch) override;
  HRESULT STDMETHODCALLTYPE TranslateUrl(DWORD dwTranslate,
                                         _In_ LPWSTR pchURLIn,
                                         _Outptr_ LPWSTR* ppchURLOut) override;
  HRESULT STDMETHODCALLTYPE
  FilterDataObject(_In_ IDataObject* pDO,
                   _Outptr_result_maybenull_ IDataObject** ppDORet) override;

  // IDocHostShowUI
  HRESULT STDMETHODCALLTYPE ShowMessage(
      /* [in] */ HWND hwnd,
      /* [annotation][in] */
      _In_ LPOLESTR lpstrText,
      /* [annotation][in] */
      _In_ LPOLESTR lpstrCaption,
      /* [in] */ DWORD dwType,
      /* [annotation][in] */
      _In_ LPOLESTR lpstrHelpFile,
      /* [in] */ DWORD dwHelpContext,
      /* [out] */ LRESULT* plResult) override;
  HRESULT STDMETHODCALLTYPE ShowHelp(
      /* [in] */ HWND hwnd,
      /* [annotation][in] */
      _In_ LPOLESTR pszHelpFile,
      /* [in] */ UINT uCommand,
      /* [in] */ DWORD dwData,
      /* [in] */ POINT ptMouse,
      /* [out] */ IDispatch* pDispatchObjectHit) override;

  // IDispatch
  HRESULT STDMETHODCALLTYPE GetTypeInfoCount(
      /* [out] */ __RPC__out UINT* pctinfo) override;
  HRESULT STDMETHODCALLTYPE GetTypeInfo(
      /* [in] */ UINT iTInfo,
      /* [in] */ LCID lcid,
      /* [out] */ __RPC__deref_out_opt ITypeInfo** ppTInfo) override;
  HRESULT STDMETHODCALLTYPE GetIDsOfNames(
      /* [in] */ __RPC__in REFIID riid,
      /* [size_is][in] */ __RPC__in_ecount_full(cNames) LPOLESTR* rgszNames,
      /* [range][in] */ __RPC__in_range(0, 16384) UINT cNames,
      /* [in] */ LCID lcid,
      /* [size_is][out] */ __RPC__out_ecount_full(cNames) DISPID* rgDispId)
      override;
  /* [local] */ HRESULT STDMETHODCALLTYPE Invoke(
      /* [annotation][in] */
      _In_ DISPID dispIdMember,
      /* [annotation][in] */
      _In_ REFIID riid,
      /* [annotation][in] */
      _In_ LCID lcid,
      /* [annotation][in] */
      _In_ WORD wFlags,
      /* [annotation][out][in] */
      _In_ DISPPARAMS* pDispParams,
      /* [annotation][out] */
      _Out_opt_ VARIANT* pVarResult,
      /* [annotation][out] */
      _Out_opt_ EXCEPINFO* pExcepInfo,
      /* [annotation][out] */
      _Out_opt_ UINT* puArgErr) override;
};