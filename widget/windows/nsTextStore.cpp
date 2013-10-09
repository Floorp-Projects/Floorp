/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <olectl.h>
#include <algorithm>

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif // MOZ_LOGGING
#include "prlog.h"

#include "nscore.h"
#include "nsWindow.h"
#ifdef MOZ_METRO
#include "winrt/MetroWidget.h"
#endif
#include "nsPrintfCString.h"
#include "WinUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"

#define INPUTSCOPE_INIT_GUID
#include "nsTextStore.h"

using namespace mozilla;
using namespace mozilla::widget;

#ifdef PR_LOGGING
/**
 * TSF related code should log its behavior even on release build especially
 * in the interface methods.
 *
 * In interface methods, use PR_LOG_ALWAYS.
 * In internal methods, use PR_LOG_DEBUG for logging normal behavior.
 * For logging error, use PR_LOG_ERROR.
 *
 * When an instance method is called, start with following text:
 *   "TSF: 0x%p nsFoo::Bar(", the 0x%p should be the "this" of the nsFoo.
 * after that, start with:
 *   "TSF: 0x%p   nsFoo::Bar("
 * In an internal method, start with following text:
 *   "TSF: 0x%p   nsFoo::Bar("
 * When a static method is called, start with following text:
 *   "TSF: nsFoo::Bar("
 */

PRLogModuleInfo* sTextStoreLog = nullptr;
#endif // #ifdef PR_LOGGING

/******************************************************************/
/* InputScopeImpl                                                 */
/******************************************************************/

class InputScopeImpl MOZ_FINAL : public ITfInputScope
{
public:
  InputScopeImpl(const nsTArray<InputScope>& aList) :
    mRefCnt(1),
    mInputScopes(aList)
  {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
      ("TSF: 0x%p InputScopeImpl()", this));
  }

  STDMETHODIMP_(ULONG) AddRef(void) { return ++mRefCnt; }

  STDMETHODIMP_(ULONG) Release(void)
  {
    --mRefCnt;
    if (mRefCnt) {
      return mRefCnt;
    }
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
      ("TSF: 0x%p InputScopeImpl::Release() final", this));
    delete this;
    return 0;
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
  {
    *ppv=nullptr;
    if ( (IID_IUnknown == riid) || (IID_ITfInputScope == riid) ) {
      *ppv = static_cast<ITfInputScope*>(this);
    }
    if (*ppv) {
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHODIMP GetInputScopes(InputScope** pprgInputScopes, UINT* pcCount)
  {
    uint32_t count = (mInputScopes.IsEmpty() ? 1 : mInputScopes.Length());

    InputScope* pScope = (InputScope*) CoTaskMemAlloc(sizeof(InputScope) * count);
    NS_ENSURE_TRUE(pScope, E_OUTOFMEMORY);

    if (mInputScopes.IsEmpty()) {
      *pScope = IS_DEFAULT;
      *pcCount = 1;
      *pprgInputScopes = pScope;
      return S_OK;
    }

    *pcCount = 0;

    for (uint32_t idx = 0; idx < count; idx++) {
      *(pScope + idx) = mInputScopes[idx];
      (*pcCount)++;
    }

    *pprgInputScopes = pScope;
    return S_OK;
  }

  STDMETHODIMP GetPhrase(BSTR **ppbstrPhrases, UINT *pcCount) { return E_NOTIMPL; }
  STDMETHODIMP GetRegularExpression(BSTR *pbstrRegExp) { return E_NOTIMPL; }
  STDMETHODIMP GetSRGS(BSTR *pbstrSRGS) { return E_NOTIMPL; }
  STDMETHODIMP GetXML(BSTR *pbstrXML) { return E_NOTIMPL; }

private:
  DWORD mRefCnt;
  nsTArray<InputScope> mInputScopes;
};

/******************************************************************/
/* nsTextStore                                                    */
/******************************************************************/

ITfThreadMgr*           nsTextStore::sTsfThreadMgr   = nullptr;
ITfMessagePump*         nsTextStore::sMessagePump    = nullptr;
ITfKeystrokeMgr*        nsTextStore::sKeystrokeMgr   = nullptr;
ITfDisplayAttributeMgr* nsTextStore::sDisplayAttrMgr = nullptr;
ITfCategoryMgr*         nsTextStore::sCategoryMgr    = nullptr;
ITfDocumentMgr*         nsTextStore::sTsfDisabledDocumentMgr = nullptr;
ITfContext*             nsTextStore::sTsfDisabledContext = nullptr;
DWORD         nsTextStore::sTsfClientId  = 0;
nsTextStore*  nsTextStore::sTsfTextStore = nullptr;

UINT nsTextStore::sFlushTIPInputMessage  = 0;

#define TEXTSTORE_DEFAULT_VIEW (1)

#ifdef PR_LOGGING

static const char*
GetBoolName(bool aBool)
{
  return aBool ? "true" : "false";
}

static void
HandleSeparator(nsCString& aDesc)
{
  if (!aDesc.IsEmpty()) {
    aDesc.AppendLiteral(" | ");
  }
}

static const nsCString
GetFindFlagName(DWORD aFindFlag)
{
  nsAutoCString description;
  if (!aFindFlag) {
    description.AppendLiteral("no flags (0)");
    return description;
  }
  if (aFindFlag & TS_ATTR_FIND_BACKWARDS) {
    description.AppendLiteral("TS_ATTR_FIND_BACKWARDS");
  }
  if (aFindFlag & TS_ATTR_FIND_WANT_OFFSET) {
    HandleSeparator(description);
    description.AppendLiteral("TS_ATTR_FIND_WANT_OFFSET");
  }
  if (aFindFlag & TS_ATTR_FIND_UPDATESTART) {
    HandleSeparator(description);
    description.AppendLiteral("TS_ATTR_FIND_UPDATESTART");
  }
  if (aFindFlag & TS_ATTR_FIND_WANT_VALUE) {
    HandleSeparator(description);
    description.AppendLiteral("TS_ATTR_FIND_WANT_VALUE");
  }
  if (aFindFlag & TS_ATTR_FIND_WANT_END) {
    HandleSeparator(description);
    description.AppendLiteral("TS_ATTR_FIND_WANT_END");
  }
  if (aFindFlag & TS_ATTR_FIND_HIDDEN) {
    HandleSeparator(description);
    description.AppendLiteral("TS_ATTR_FIND_HIDDEN");
  }
  if (description.IsEmpty()) {
    description.AppendLiteral("Unknown (");
    description.AppendInt(static_cast<uint32_t>(aFindFlag));
    description.AppendLiteral(")");
  }
  return description;
}

static const char*
GetIMEEnabledName(IMEState::Enabled aIMEEnabled)
{
  switch (aIMEEnabled) {
    case IMEState::DISABLED:
      return "DISABLED";
    case IMEState::ENABLED:
      return "ENABLED";
    case IMEState::PASSWORD:
      return "PASSWORD";
    case IMEState::PLUGIN:
      return "PLUGIN";
    default:
      return "Invalid";
  }
}

static const char*
GetFocusChangeName(InputContextAction::FocusChange aFocusChange)
{
  switch (aFocusChange) {
    case InputContextAction::FOCUS_NOT_CHANGED:
      return "FOCUS_NOT_CHANGED";
    case InputContextAction::GOT_FOCUS:
      return "GOT_FOCUS";
    case InputContextAction::LOST_FOCUS:
      return "LOST_FOCUS";
    case InputContextAction::MENU_GOT_PSEUDO_FOCUS:
      return "MENU_GOT_PSEUDO_FOCUS";
    case InputContextAction::MENU_LOST_PSEUDO_FOCUS:
      return "MENU_LOST_PSEUDO_FOCUS";
    default:
      return "Unknown";
  }
}

static nsCString
GetCLSIDNameStr(REFCLSID aCLSID)
{
  LPOLESTR str = nullptr;
  HRESULT hr = ::StringFromCLSID(aCLSID, &str);
  if (FAILED(hr) || !str || !str[0]) {
    return EmptyCString();
  }

  nsAutoCString result;
  result = NS_ConvertUTF16toUTF8(str);
  ::CoTaskMemFree(str);
  return result;
}

static nsCString
GetRIIDNameStr(REFIID aRIID)
{
  LPOLESTR str = nullptr;
  HRESULT hr = ::StringFromIID(aRIID, &str);
  if (FAILED(hr) || !str || !str[0]) {
    return EmptyCString();
  }

  nsAutoString key(L"Interface\\");
  key += str;

  nsAutoCString result;
  PRUnichar buf[256];
  if (WinUtils::GetRegistryKey(HKEY_CLASSES_ROOT, key.get(), nullptr,
                               buf, sizeof(buf))) {
    result = NS_ConvertUTF16toUTF8(buf);
  } else {
    result = NS_ConvertUTF16toUTF8(str);
  }

  ::CoTaskMemFree(str);
  return result;
}

static const char*
GetCommonReturnValueName(HRESULT aResult)
{
  switch (aResult) {
    case S_OK:
      return "S_OK";
    case E_ABORT:
      return "E_ABORT";
    case E_ACCESSDENIED:
      return "E_ACCESSDENIED";
    case E_FAIL:
      return "E_FAIL";
    case E_HANDLE:
      return "E_HANDLE";
    case E_INVALIDARG:
      return "E_INVALIDARG";
    case E_NOINTERFACE:
      return "E_NOINTERFACE";
    case E_NOTIMPL:
      return "E_NOTIMPL";
    case E_OUTOFMEMORY:
      return "E_OUTOFMEMORY";
    case E_POINTER:
      return "E_POINTER";
    case E_UNEXPECTED:
      return "E_UNEXPECTED";
    default:
      return SUCCEEDED(aResult) ? "Succeeded" : "Failed";
  }
}

static const char*
GetTextStoreReturnValueName(HRESULT aResult)
{
  switch (aResult) {
    case TS_E_FORMAT:
      return "TS_E_FORMAT";
    case TS_E_INVALIDPOINT:
      return "TS_E_INVALIDPOINT";
    case TS_E_INVALIDPOS:
      return "TS_E_INVALIDPOS";
    case TS_E_NOINTERFACE:
      return "TS_E_NOINTERFACE";
    case TS_E_NOLAYOUT:
      return "TS_E_NOLAYOUT";
    case TS_E_NOLOCK:
      return "TS_E_NOLOCK";
    case TS_E_NOOBJECT:
      return "TS_E_NOOBJECT";
    case TS_E_NOSELECTION:
      return "TS_E_NOSELECTION";
    case TS_E_NOSERVICE:
      return "TS_E_NOSERVICE";
    case TS_E_READONLY:
      return "TS_E_READONLY";
    case TS_E_SYNCHRONOUS:
      return "TS_E_SYNCHRONOUS";
    case TS_S_ASYNC:
      return "TS_S_ASYNC";
    default:
      return GetCommonReturnValueName(aResult);
  }
}

static const nsCString
GetSinkMaskNameStr(DWORD aSinkMask)
{
  nsAutoCString description;
  if (aSinkMask & TS_AS_TEXT_CHANGE) {
    description.AppendLiteral("TS_AS_TEXT_CHANGE");
  }
  if (aSinkMask & TS_AS_SEL_CHANGE) {
    HandleSeparator(description);
    description.AppendLiteral("TS_AS_SEL_CHANGE");
  }
  if (aSinkMask & TS_AS_LAYOUT_CHANGE) {
    HandleSeparator(description);
    description.AppendLiteral("TS_AS_LAYOUT_CHANGE");
  }
  if (aSinkMask & TS_AS_ATTR_CHANGE) {
    HandleSeparator(description);
    description.AppendLiteral("TS_AS_ATTR_CHANGE");
  }
  if (aSinkMask & TS_AS_STATUS_CHANGE) {
    HandleSeparator(description);
    description.AppendLiteral("TS_AS_STATUS_CHANGE");
  }
  if (description.IsEmpty()) {
    description.AppendLiteral("not-specified");
  }
  return description;
}

static const char*
GetActiveSelEndName(TsActiveSelEnd aSelEnd)
{
  return aSelEnd == TS_AE_NONE  ? "TS_AE_NONE" :
         aSelEnd == TS_AE_START ? "TS_AE_START" :
         aSelEnd == TS_AE_END   ? "TS_AE_END" : "Unknown";
}

static const nsCString
GetLockFlagNameStr(DWORD aLockFlags)
{
  nsAutoCString description;
  if ((aLockFlags & TS_LF_READWRITE) == TS_LF_READWRITE) {
    description.AppendLiteral("TS_LF_READWRITE");
  } else if (aLockFlags & TS_LF_READ) {
    description.AppendLiteral("TS_LF_READ");
  }
  if (aLockFlags & TS_LF_SYNC) {
    if (!description.IsEmpty()) {
      description.AppendLiteral(" | ");
    }
    description.AppendLiteral("TS_LF_SYNC");
  }
  if (description.IsEmpty()) {
    description.AppendLiteral("not-specified");
  }
  return description;
}

static const char*
GetTextRunTypeName(TsRunType aRunType)
{
  switch (aRunType) {
    case TS_RT_PLAIN:
      return "TS_RT_PLAIN";
    case TS_RT_HIDDEN:
      return "TS_RT_HIDDEN";
    case  TS_RT_OPAQUE:
      return "TS_RT_OPAQUE";
    default:
      return "Unknown";
  }
}

static nsCString
GetColorName(const TF_DA_COLOR &aColor)
{
  switch (aColor.type) {
    case TF_CT_NONE:
      return NS_LITERAL_CSTRING("TF_CT_NONE");
    case TF_CT_SYSCOLOR:
      return nsPrintfCString("TF_CT_SYSCOLOR, nIndex:0x%08X",
                             static_cast<int32_t>(aColor.nIndex));
    case TF_CT_COLORREF:
      return nsPrintfCString("TF_CT_COLORREF, cr:0x%08X",
                             static_cast<int32_t>(aColor.cr));
      break;
    default:
      return nsPrintfCString("Unknown(%08X)",
                             static_cast<int32_t>(aColor.type));
  }
}

static nsCString
GetLineStyleName(TF_DA_LINESTYLE aLineStyle)
{
  switch (aLineStyle) {
    case TF_LS_NONE:
      return NS_LITERAL_CSTRING("TF_LS_NONE");
    case TF_LS_SOLID:
      return NS_LITERAL_CSTRING("TF_LS_SOLID");
    case TF_LS_DOT:
      return NS_LITERAL_CSTRING("TF_LS_DOT");
    case TF_LS_DASH:
      return NS_LITERAL_CSTRING("TF_LS_DASH");
    case TF_LS_SQUIGGLE:
      return NS_LITERAL_CSTRING("TF_LS_SQUIGGLE");
    default: {
      return nsPrintfCString("Unknown(%08X)", static_cast<int32_t>(aLineStyle));
    }
  }
}

static nsCString
GetClauseAttrName(TF_DA_ATTR_INFO aAttr)
{
  switch (aAttr) {
    case TF_ATTR_INPUT:
      return NS_LITERAL_CSTRING("TF_ATTR_INPUT");
    case TF_ATTR_TARGET_CONVERTED:
      return NS_LITERAL_CSTRING("TF_ATTR_TARGET_CONVERTED");
    case TF_ATTR_CONVERTED:
      return NS_LITERAL_CSTRING("TF_ATTR_CONVERTED");
    case TF_ATTR_TARGET_NOTCONVERTED:
      return NS_LITERAL_CSTRING("TF_ATTR_TARGET_NOTCONVERTED");
    case TF_ATTR_INPUT_ERROR:
      return NS_LITERAL_CSTRING("TF_ATTR_INPUT_ERROR");
    case TF_ATTR_FIXEDCONVERTED:
      return NS_LITERAL_CSTRING("TF_ATTR_FIXEDCONVERTED");
    case TF_ATTR_OTHER:
      return NS_LITERAL_CSTRING("TF_ATTR_OTHER");
    default: {
      return nsPrintfCString("Unknown(%08X)", static_cast<int32_t>(aAttr));
    }
  }
}

static nsCString
GetDisplayAttrStr(const TF_DISPLAYATTRIBUTE &aDispAttr)
{
  nsAutoCString str;
  str = "crText:{ ";
  str += GetColorName(aDispAttr.crText);
  str += " }, crBk:{ ";
  str += GetColorName(aDispAttr.crBk);
  str += " }, lsStyle: ";
  str += GetLineStyleName(aDispAttr.lsStyle);
  str += ", fBoldLine: ";
  str += GetBoolName(aDispAttr.fBoldLine);
  str += ", crLine:{ ";
  str += GetColorName(aDispAttr.crLine);
  str += " }, bAttr: ";
  str += GetClauseAttrName(aDispAttr.bAttr);
  return str;
}

#endif // #ifdef PR_LOGGING

nsTextStore::nsTextStore()
 : mContent(mComposition, mSelection)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p nsTextStore::nsTestStore(): instance is created", this));

  mRefCnt = 1;
  mEditCookie = 0;
  mSinkMask = 0;
  mLock = 0;
  mLockQueued = 0;
  mTextChange.acpStart = INT32_MAX;
  mTextChange.acpOldEnd = mTextChange.acpNewEnd = 0;
  mInputScopeDetected = false;
  mInputScopeRequested = false;
  mIsRecordingActionsWithoutLock = false;
  mNotifySelectionChange = false;
  // We hope that 5 or more actions don't occur at once.
  mPendingActions.SetCapacity(5);
}

nsTextStore::~nsTextStore()
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p nsTextStore instance is destroyed, "
     "mWidget=0x%p, mDocumentMgr=0x%p, mContext=0x%p",
     this, mWidget.get(), mDocumentMgr.get(), mContext.get()));

  mComposition.EnsureLayoutChangeTimerStopped();
}

bool
nsTextStore::Create(nsWindowBase* aWidget)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p nsTextStore::Create(aWidget=0x%p)",
     this, aWidget));

  if (mDocumentMgr) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::Create() FAILED due to already initialized",
       this));
    return false;
  }

  // Create document manager
  HRESULT hr = sTsfThreadMgr->CreateDocumentMgr(
                                  getter_AddRefs(mDocumentMgr));
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::Create() FAILED to create DocumentMgr "
       "(0x%08X)", this, hr));
    return false;
  }
  mWidget = aWidget;

  // Create context and add it to document manager
  hr = mDocumentMgr->CreateContext(sTsfClientId, 0,
                                   static_cast<ITextStoreACP*>(this),
                                   getter_AddRefs(mContext), &mEditCookie);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::Create() FAILED to create the context "
       "(0x%08X)", this, hr));
    mDocumentMgr = nullptr;
    return false;
  }

  hr = mDocumentMgr->Push(mContext);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::Create() FAILED to push the context (0x%08X)",
       this, hr));
    // XXX Why don't we use NS_IF_RELEASE() here??
    mContext = nullptr;
    mDocumentMgr = nullptr;
    return false;
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p   nsTextStore::Create() succeeded: "
     "mDocumentMgr=0x%p, mContext=0x%p, mEditCookie=0x%08X",
     this, mDocumentMgr.get(), mContext.get(), mEditCookie));

  return true;
}

bool
nsTextStore::Destroy(void)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p nsTextStore::Destroy()", this));

  mContent.Clear();
  mSelection.MarkDirty();

  if (mWidget) {
    // When blurred, Tablet Input Panel posts "blur" messages
    // and try to insert text when the message is retrieved later.
    // But by that time the text store is already destroyed,
    // so try to get the message early
    MSG msg;
    if (WinUtils::PeekMessage(&msg, mWidget->GetWindowHandle(),
                              sFlushTIPInputMessage, sFlushTIPInputMessage,
                              PM_REMOVE)) {
      ::DispatchMessageW(&msg);
    }
  }
  mContext = nullptr;
  if (mDocumentMgr) {
    mDocumentMgr->Pop(TF_POPF_ALL);
    mDocumentMgr = nullptr;
  }
  mSink = nullptr;
  mWidget = nullptr;

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p   nsTextStore::Destroy() succeeded", this));
  return true;
}

STDMETHODIMP
nsTextStore::QueryInterface(REFIID riid,
                            void** ppv)
{
  *ppv=nullptr;
  if ( (IID_IUnknown == riid) || (IID_ITextStoreACP == riid) ) {
    *ppv = static_cast<ITextStoreACP*>(this);
  } else if (IID_ITfContextOwnerCompositionSink == riid) {
    *ppv = static_cast<ITfContextOwnerCompositionSink*>(this);
  }
  if (*ppv) {
    AddRef();
    return S_OK;
  }

  PR_LOG(sTextStoreLog, PR_LOG_ERROR,
    ("TSF: 0x%p nsTextStore::QueryInterface() FAILED, riid=%s",
     this, GetRIIDNameStr(riid).get()));
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) nsTextStore::AddRef()
{
  return ++mRefCnt;
}

STDMETHODIMP_(ULONG) nsTextStore::Release()
{
  --mRefCnt;
  if (0 != mRefCnt)
    return mRefCnt;
  delete this;
  return 0;
}

STDMETHODIMP
nsTextStore::AdviseSink(REFIID riid,
                        IUnknown *punk,
                        DWORD dwMask)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p nsTextStore::AdviseSink(riid=%s, punk=0x%p, dwMask=%s), "
     "mSink=0x%p, mSinkMask=%s",
     this, GetRIIDNameStr(riid).get(), punk, GetSinkMaskNameStr(dwMask).get(),
     mSink.get(), GetSinkMaskNameStr(mSinkMask).get()));

  if (!punk) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::AdviseSink() FAILED due to the null punk",
       this));
    return E_UNEXPECTED;
  }

  if (IID_ITextStoreACPSink != riid) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::AdviseSink() FAILED due to "
       "unsupported interface", this));
    return E_INVALIDARG; // means unsupported interface.
  }

  if (!mSink) {
    // Install sink
    punk->QueryInterface(IID_ITextStoreACPSink, getter_AddRefs(mSink));
    if (!mSink) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF: 0x%p   nsTextStore::AdviseSink() FAILED due to "
         "punk not having the interface", this));
      return E_UNEXPECTED;
    }
  } else {
    // If sink is already installed we check to see if they are the same
    // Get IUnknown from both sides for comparison
    nsRefPtr<IUnknown> comparison1, comparison2;
    punk->QueryInterface(IID_IUnknown, getter_AddRefs(comparison1));
    mSink->QueryInterface(IID_IUnknown, getter_AddRefs(comparison2));
    if (comparison1 != comparison2) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF: 0x%p   nsTextStore::AdviseSink() FAILED due to "
         "the sink being different from the stored sink", this));
      return CONNECT_E_ADVISELIMIT;
    }
  }
  // Update mask either for a new sink or an existing sink
  mSinkMask = dwMask;
  return S_OK;
}

STDMETHODIMP
nsTextStore::UnadviseSink(IUnknown *punk)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p nsTextStore::UnadviseSink(punk=0x%p), mSink=0x%p",
     this, punk, mSink.get()));

  if (!punk) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::UnadviseSink() FAILED due to the null punk",
       this));
    return E_INVALIDARG;
  }
  if (!mSink) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::UnadviseSink() FAILED due to "
       "any sink not stored", this));
    return CONNECT_E_NOCONNECTION;
  }
  // Get IUnknown from both sides for comparison
  nsRefPtr<IUnknown> comparison1, comparison2;
  punk->QueryInterface(IID_IUnknown, getter_AddRefs(comparison1));
  mSink->QueryInterface(IID_IUnknown, getter_AddRefs(comparison2));
  // Unadvise only if sinks are the same
  if (comparison1 != comparison2) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::UnadviseSink() FAILED due to "
       "the sink being different from the stored sink", this));
    return CONNECT_E_NOCONNECTION;
  }
  mSink = nullptr;
  mSinkMask = 0;
  return S_OK;
}

STDMETHODIMP
nsTextStore::RequestLock(DWORD dwLockFlags,
                         HRESULT *phrSession)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p nsTextStore::RequestLock(dwLockFlags=%s, phrSession=0x%p), "
     "mLock=%s", this, GetLockFlagNameStr(dwLockFlags).get(), phrSession,
     GetLockFlagNameStr(mLock).get()));

  if (!mSink) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::RequestLock() FAILED due to "
       "any sink not stored", this));
    return E_FAIL;
  }
  if (!phrSession) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::RequestLock() FAILED due to "
       "null phrSession", this));
    return E_INVALIDARG;
  }

  if (!mLock) {
    // put on lock
    mLock = dwLockFlags & (~TS_LF_SYNC);
    PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
      ("TSF: 0x%p   Locking (%s) >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
       ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
       ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>",
       this, GetLockFlagNameStr(mLock).get()));
    *phrSession = mSink->OnLockGranted(mLock);
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
      ("TSF: 0x%p   Unlocked (%s) <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
       "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
       "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<",
       this, GetLockFlagNameStr(mLock).get()));
    if (IsReadWriteLocked()) {
      FlushPendingActions();
    }
    while (mLockQueued) {
      mLock = mLockQueued;
      mLockQueued = 0;
      PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
        ("TSF: 0x%p   Locking for the request in the queue (%s) >>>>>>>>>>>>>>"
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>",
         this, GetLockFlagNameStr(mLock).get()));
      mSink->OnLockGranted(mLock);
      PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
        ("TSF: 0x%p   Unlocked (%s) <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
         "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
         "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<",
         this, GetLockFlagNameStr(mLock).get()));
      if (IsReadWriteLocked()) {
        FlushPendingActions();
      }
    }
    mLock = 0;
    PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
      ("TSF: 0x%p   nsTextStore::RequestLock() succeeded: *phrSession=%s",
       this, GetTextStoreReturnValueName(*phrSession)));
    return S_OK;
  }

  // only time when reentrant lock is allowed is when caller holds a
  // read-only lock and is requesting an async write lock
  if (IsReadLocked() && !IsReadWriteLocked() && IsReadWriteLock(dwLockFlags) &&
      !(dwLockFlags & TS_LF_SYNC)) {
    *phrSession = TS_S_ASYNC;
    mLockQueued = dwLockFlags & (~TS_LF_SYNC);

    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
      ("TSF: 0x%p   nsTextStore::RequestLock() stores the request in the "
       "queue, *phrSession=TS_S_ASYNC", this));
    return S_OK;
  }

  // no more locks allowed
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p   nsTextStore::RequestLock() didn't allow to lock, "
     "*phrSession=TS_E_SYNCHRONOUS", this));
  *phrSession = TS_E_SYNCHRONOUS;
  return E_FAIL;
}

void
nsTextStore::FlushPendingActions()
{
  if (!mWidget || mWidget->Destroyed()) {
    mPendingActions.Clear();
    mContent.Clear();
    mNotifySelectionChange = false;
    return;
  }

  bool notifyTSFOfLayoutChange = mContent.NeedToNotifyTSFOfLayoutChange();
  mContent.Clear();

  nsRefPtr<nsWindowBase> kungFuDeathGrip(mWidget);
  for (uint32_t i = 0; i < mPendingActions.Length(); i++) {
    PendingAction& action = mPendingActions[i];
    switch (action.mType) {
      case PendingAction::COMPOSITION_START: {
        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions() "
                "flushing COMPOSITION_START={ mSelectionStart=%d, "
                "mSelectionLength=%d }",
                this, action.mSelectionStart, action.mSelectionLength));

        MOZ_ASSERT(mComposition.mLastData.IsEmpty());

        // Select composition range so the new composition replaces the range
        WidgetSelectionEvent selectionSet(true, NS_SELECTION_SET, mWidget);
        mWidget->InitEvent(selectionSet);
        selectionSet.mOffset = static_cast<uint32_t>(action.mSelectionStart);
        selectionSet.mLength = static_cast<uint32_t>(action.mSelectionLength);
        selectionSet.mReversed = false;
        mWidget->DispatchWindowEvent(&selectionSet);
        if (!selectionSet.mSucceeded) {
          PR_LOG(sTextStoreLog, PR_LOG_ERROR,
                 ("TSF: 0x%p   nsTextStore::FlushPendingActions() "
                  "FAILED due to NS_SELECTION_SET failure", this));
          break;
        }
        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions() "
                "dispatching compositionstart event...", this));
        WidgetCompositionEvent compositionStart(true, NS_COMPOSITION_START,
                                                mWidget);
        mWidget->InitEvent(compositionStart);
        mWidget->DispatchWindowEvent(&compositionStart);
        if (!mWidget || mWidget->Destroyed()) {
          break;
        }
        mComposition.StartLayoutChangeTimer(this);
        break;
      }
      case PendingAction::COMPOSITION_UPDATE: {
        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions() "
                "flushing COMPOSITION_UPDATE={ mData=\"%s\", "
                "mRanges.Length()=%d }",
                this, NS_ConvertUTF16toUTF8(action.mData).get(),
                action.mRanges.Length()));

        if (action.mRanges.IsEmpty()) {
          TextRange wholeRange;
          wholeRange.mStartOffset = 0;
          wholeRange.mEndOffset = action.mData.Length();
          wholeRange.mRangeType = NS_TEXTRANGE_RAWINPUT;
          action.mRanges.AppendElement(wholeRange);
        } else {
          // Adjust offsets in the ranges for XP linefeed character (only \n).
          // XXX Following code is the safest approach.  However, it wastes
          //     a little performance.  For ensuring the clauses do not
          //     overlap each other, we should redesign TextRange later.
          for (uint32_t i = 0; i < action.mRanges.Length(); ++i) {
            TextRange& range = action.mRanges[i];
            TextRange nativeRange = range;
            if (nativeRange.mStartOffset > 0) {
              nsAutoString preText(
                Substring(action.mData, 0, nativeRange.mStartOffset));
              preText.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                                       NS_LITERAL_STRING("\n"));
              range.mStartOffset = preText.Length();
            }
            if (nativeRange.Length() == 0) {
              range.mEndOffset = range.mStartOffset;
            } else {
              nsAutoString clause(
                Substring(action.mData,
                          nativeRange.mStartOffset, nativeRange.Length()));
              clause.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                                      NS_LITERAL_STRING("\n"));
              range.mEndOffset = range.mStartOffset + clause.Length();
            }
          }
        }

        action.mData.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                                      NS_LITERAL_STRING("\n"));

        if (action.mData != mComposition.mLastData) {
          PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
                 ("TSF: 0x%p   nsTextStore::FlushPendingActions(), "
                  "dispatching compositionupdate event...", this));
          WidgetCompositionEvent compositionUpdate(true, NS_COMPOSITION_UPDATE,
                                                   mWidget);
          mWidget->InitEvent(compositionUpdate);
          compositionUpdate.data = action.mData;
          mComposition.mLastData = compositionUpdate.data;
          mWidget->DispatchWindowEvent(&compositionUpdate);
          if (!mWidget || mWidget->Destroyed()) {
            break;
          }
        }

        MOZ_ASSERT(action.mData == mComposition.mLastData);

        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions(), "
                "dispatching text event...", this));
        WidgetTextEvent textEvent(true, NS_TEXT_TEXT, mWidget);
        mWidget->InitEvent(textEvent);
        textEvent.theText = mComposition.mLastData;
        if (action.mRanges.IsEmpty()) {
          TextRange wholeRange;
          wholeRange.mStartOffset = 0;
          wholeRange.mEndOffset = textEvent.theText.Length();
          wholeRange.mRangeType = NS_TEXTRANGE_RAWINPUT;
          action.mRanges.AppendElement(wholeRange);
        }
        textEvent.rangeArray = action.mRanges.Elements();
        textEvent.rangeCount = action.mRanges.Length();
        mWidget->DispatchWindowEvent(&textEvent);
        // Be aware, the mWidget might already have been destroyed.
        break;
      }
      case PendingAction::COMPOSITION_END: {
        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions() "
                "flushing COMPOSITION_END={ mData=\"%s\" }",
                this, NS_ConvertUTF16toUTF8(action.mData).get()));

        mComposition.EnsureLayoutChangeTimerStopped();

        action.mData.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                                      NS_LITERAL_STRING("\n"));
        if (action.mData != mComposition.mLastData) {
          PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
                 ("TSF: 0x%p   nsTextStore::FlushPendingActions(), "
                  "dispatching compositionupdate event...", this));
          WidgetCompositionEvent compositionUpdate(true, NS_COMPOSITION_UPDATE,
                                                   mWidget);
          mWidget->InitEvent(compositionUpdate);
          compositionUpdate.data = action.mData;
          mComposition.mLastData = compositionUpdate.data;
          mWidget->DispatchWindowEvent(&compositionUpdate);
          if (!mWidget || mWidget->Destroyed()) {
            break;
          }
        }

        MOZ_ASSERT(action.mData == mComposition.mLastData);

        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions(), "
                "dispatching text event...", this));
        WidgetTextEvent textEvent(true, NS_TEXT_TEXT, mWidget);
        mWidget->InitEvent(textEvent);
        textEvent.theText = mComposition.mLastData;
        mWidget->DispatchWindowEvent(&textEvent);
        if (!mWidget || mWidget->Destroyed()) {
          break;
        }

        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions(), "
                "dispatching compositionend event...", this));
        WidgetCompositionEvent compositionEnd(true, NS_COMPOSITION_END,
                                              mWidget);
        compositionEnd.data = mComposition.mLastData;
        mWidget->InitEvent(compositionEnd);
        mWidget->DispatchWindowEvent(&compositionEnd);
        if (!mWidget || mWidget->Destroyed()) {
          break;
        }
        mComposition.mLastData.Truncate();
        break;
      }
      case PendingAction::SELECTION_SET: {
        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions() "
                "flushing SELECTION_SET={ mSelectionStart=%d, "
                "mSelectionLength=%d, mSelectionReversed=%s }",
                this, action.mSelectionStart, action.mSelectionLength,
                GetBoolName(action.mSelectionReversed)));

        WidgetSelectionEvent selectionSet(true, NS_SELECTION_SET, mWidget);
        selectionSet.mOffset = 
          static_cast<uint32_t>(action.mSelectionStart);
        selectionSet.mLength =
          static_cast<uint32_t>(action.mSelectionLength);
        selectionSet.mReversed = action.mSelectionReversed;
        break;
      }
      default:
        MOZ_CRASH("unexpected action type");
    }

    if (mWidget && !mWidget->Destroyed()) {
      continue;
    }

    mComposition.EnsureLayoutChangeTimerStopped();

    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::FlushPendingActions(), "
            "qutting since the mWidget has gone", this));
    break;
  }
  mPendingActions.Clear();

  if (notifyTSFOfLayoutChange && mWidget && !mWidget->Destroyed()) {
    if (mSink) {
      PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
             ("TSF: 0x%p   nsTextStore::FlushPendingActions(), "
              "calling ITextStoreACPSink::OnLayoutChange()...", this));
      mSink->OnLayoutChange(TS_LC_CHANGE, TEXTSTORE_DEFAULT_VIEW);
    }
    // The layout change caused by composition string change should cause
    // calling ITfContextOwnerServices::OnLayoutChange() too.
    // Actually, MS-IME 2002 (The default Japanese IME of WinXP) needs this.
    if (mContext) {
      nsRefPtr<ITfContextOwnerServices> service;
      mContext->QueryInterface(IID_ITfContextOwnerServices,
                               getter_AddRefs(service));
      if (service) {
        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions(), "
                "calling ITfContextOwnerServices::OnLayoutChange()...", this));
        service->OnLayoutChange();
      }
    }
  }

  if (mNotifySelectionChange && mSink && mWidget && !mWidget->Destroyed()) {
    mSink->OnSelectionChange();
  }
  mNotifySelectionChange = false;
}

STDMETHODIMP
nsTextStore::GetStatus(TS_STATUS *pdcs)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p nsTextStore::GetStatus(pdcs=0x%p)", this, pdcs));

  if (!pdcs) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::GetStatus() FAILED due to null pdcs", this));
    return E_INVALIDARG;
  }
  pdcs->dwDynamicFlags = 0;
  // we use a "flat" text model for TSF support so no hidden text
  pdcs->dwStaticFlags = TS_SS_NOHIDDENTEXT;
  return S_OK;
}

STDMETHODIMP
nsTextStore::QueryInsert(LONG acpTestStart,
                         LONG acpTestEnd,
                         ULONG cch,
                         LONG *pacpResultStart,
                         LONG *pacpResultEnd)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::QueryInsert(acpTestStart=%ld, "
          "acpTestEnd=%ld, cch=%lu, pacpResultStart=0x%p, pacpResultEnd=0x%p)",
          this, acpTestStart, acpTestEnd, cch, acpTestStart, acpTestEnd));

  if (!pacpResultStart || !pacpResultEnd) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::QueryInsert() FAILED due to "
            "the null argument", this));
    return E_INVALIDARG;
  }

  if (acpTestStart < 0 || acpTestStart > acpTestEnd) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::QueryInsert() FAILED due to "
            "wrong argument", this));
    return E_INVALIDARG;
  }

  // XXX need to adjust to cluster boundary
  // Assume we are given good offsets for now
  *pacpResultStart = acpTestStart;
  *pacpResultEnd = acpTestStart + cch;

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p  nsTextStore::QueryInsert() succeeded: "
          "*pacpResultStart=%ld, *pacpResultEnd=%ld)",
          this, *pacpResultStart, *pacpResultEnd));
  return S_OK;
}

STDMETHODIMP
nsTextStore::GetSelection(ULONG ulIndex,
                          ULONG ulCount,
                          TS_SELECTION_ACP *pSelection,
                          ULONG *pcFetched)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::GetSelection(ulIndex=%lu, ulCount=%lu, "
          "pSelection=0x%p, pcFetched=0x%p)",
          this, ulIndex, ulCount, pSelection, pcFetched));

  if (!IsReadLocked()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetSelection() FAILED due to not locked",
            this));
    return TS_E_NOLOCK;
  }
  if (!ulCount || !pSelection || !pcFetched) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetSelection() FAILED due to "
            "null argument", this));
    return E_INVALIDARG;
  }

  *pcFetched = 0;

  if (ulIndex != static_cast<ULONG>(TS_DEFAULT_SELECTION) &&
      ulIndex != 0) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetSelection() FAILED due to "
            "unsupported selection", this));
    return TS_E_NOSELECTION;
  }

  Selection& currentSel = CurrentSelection();
  if (currentSel.IsDirty()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetSelection() FAILED due to "
            "CurrentSelection() failure", this));
    return E_FAIL;
  }
  *pSelection = currentSel.ACP();
  *pcFetched = 1;
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::GetSelection() succeeded", this));
  return S_OK;
}

nsTextStore::Content&
nsTextStore::CurrentContent()
{
  Selection& currentSel = CurrentSelection();
  if (currentSel.IsDirty()) {
    mContent.Clear();
    return mContent;
  }

  if (!mContent.IsInitialized()) {
    MOZ_ASSERT(mWidget && !mWidget->Destroyed());

    WidgetQueryContentEvent queryText(true, NS_QUERY_TEXT_CONTENT, mWidget);
    queryText.InitForQueryTextContent(0, UINT32_MAX);
    mWidget->InitEvent(queryText);
    mWidget->DispatchWindowEvent(&queryText);
    NS_ENSURE_TRUE(queryText.mSucceeded, mContent);

    mContent.Init(queryText.mReply.mString);
  }

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::CurrentContent(): "
          "mContent={ mText.Length()=%d }",
          this, mContent.Text().Length()));

  return mContent;
}

nsTextStore::Selection&
nsTextStore::CurrentSelection()
{
  if (mSelection.IsDirty()) {
    // If the window has never been available, we should crash since working
    // with broken values may make TIP confused.
    if (!mWidget || mWidget->Destroyed()) {
      MOZ_CRASH();
    }

    WidgetQueryContentEvent querySelection(true, NS_QUERY_SELECTED_TEXT,
                                           mWidget);
    mWidget->InitEvent(querySelection);
    mWidget->DispatchWindowEvent(&querySelection);
    NS_ENSURE_TRUE(querySelection.mSucceeded, mSelection);

    mSelection.SetSelection(querySelection.mReply.mOffset,
                            querySelection.mReply.mString.Length(),
                            querySelection.mReply.mReversed);
  }

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::CurrentSelection(): "
          "acpStart=%d, acpEnd=%d (length=%d), reverted=%s",
          this, mSelection.StartOffset(), mSelection.EndOffset(),
          mSelection.Length(),
          GetBoolName(mSelection.IsReversed())));

  return mSelection;
}

static HRESULT
GetRangeExtent(ITfRange* aRange, LONG* aStart, LONG* aLength)
{
  nsRefPtr<ITfRangeACP> rangeACP;
  aRange->QueryInterface(IID_ITfRangeACP, getter_AddRefs(rangeACP));
  NS_ENSURE_TRUE(rangeACP, E_FAIL);
  return rangeACP->GetExtent(aStart, aLength);
}

static uint32_t
GetGeckoSelectionValue(TF_DISPLAYATTRIBUTE &aDisplayAttr)
{
  uint32_t result;
  switch (aDisplayAttr.bAttr) {
    case TF_ATTR_TARGET_CONVERTED:
      result = NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
      break;
    case TF_ATTR_CONVERTED:
      result = NS_TEXTRANGE_CONVERTEDTEXT;
      break;
    case TF_ATTR_TARGET_NOTCONVERTED:
      result = NS_TEXTRANGE_SELECTEDRAWTEXT;
      break;
    default:
      result = NS_TEXTRANGE_RAWINPUT;
      break;
  }
  return result;
}

HRESULT
nsTextStore::GetDisplayAttribute(ITfProperty* aAttrProperty,
                                 ITfRange* aRange,
                                 TF_DISPLAYATTRIBUTE* aResult)
{
  NS_ENSURE_TRUE(aAttrProperty, E_FAIL);
  NS_ENSURE_TRUE(aRange, E_FAIL);
  NS_ENSURE_TRUE(aResult, E_FAIL);

  HRESULT hr;

#ifdef PR_LOGGING
  if (PR_LOG_TEST(sTextStoreLog, PR_LOG_DEBUG)) {
    LONG start = 0, length = 0;
    hr = GetRangeExtent(aRange, &start, &length);
    PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
           ("TSF: 0x%p   nsTextStore::GetDisplayAttribute(): "
            "GetDisplayAttribute range=%ld-%ld (hr=%s)",
            this, start - mComposition.mStart,
            start - mComposition.mStart + length,
            GetCommonReturnValueName(hr)));
  }
#endif

  VARIANT propValue;
  ::VariantInit(&propValue);
  hr = aAttrProperty->GetValue(TfEditCookie(mEditCookie), aRange, &propValue);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetDisplayAttribute() FAILED due to "
            "ITfProperty::GetValue() failed", this));
    return hr;
  }
  if (VT_I4 != propValue.vt) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetDisplayAttribute() FAILED due to "
            "ITfProperty::GetValue() returns non-VT_I4 value", this));
    ::VariantClear(&propValue);
    return E_FAIL;
  }

  NS_ENSURE_TRUE(sCategoryMgr, E_FAIL);
  GUID guid;
  hr = sCategoryMgr->GetGUID(DWORD(propValue.lVal), &guid);
  ::VariantClear(&propValue);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetDisplayAttribute() FAILED due to "
            "ITfCategoryMgr::GetGUID() failed", this));
    return hr;
  }

  NS_ENSURE_TRUE(sDisplayAttrMgr, E_FAIL);
  nsRefPtr<ITfDisplayAttributeInfo> info;
  hr = sDisplayAttrMgr->GetDisplayAttributeInfo(guid, getter_AddRefs(info),
                                                nullptr);
  if (FAILED(hr) || !info) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetDisplayAttribute() FAILED due to "
            "ITfDisplayAttributeMgr::GetDisplayAttributeInfo() failed", this));
    return hr;
  }

  hr = info->GetAttributeInfo(aResult);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetDisplayAttribute() FAILED due to "
            "ITfDisplayAttributeInfo::GetAttributeInfo() failed", this));
    return hr;
  }

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::GetDisplayAttribute() succeeded: "
          "Result={ %s }", this, GetDisplayAttrStr(*aResult).get()));
  return S_OK;
}

HRESULT
nsTextStore::RestartCompositionIfNecessary(ITfRange* aRangeNew)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::RestartCompositionIfNecessary("
          "aRangeNew=0x%p), mComposition.mView=0x%p",
          this, aRangeNew, mComposition.mView.get()));

  if (!mComposition.IsComposing()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RestartCompositionIfNecessary() FAILED "
            "due to no composition view", this));
    return E_FAIL;
  }

  HRESULT hr;
  nsRefPtr<ITfCompositionView> pComposition(mComposition.mView);
  nsRefPtr<ITfRange> composingRange(aRangeNew);
  if (!composingRange) {
    hr = pComposition->GetRange(getter_AddRefs(composingRange));
    if (FAILED(hr)) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::RestartCompositionIfNecessary() FAILED "
              "due to pComposition->GetRange() failure", this));
      return hr;
    }
  }

  // Get starting offset of the composition
  LONG compStart = 0, compLength = 0;
  hr = GetRangeExtent(composingRange, &compStart, &compLength);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RestartCompositionIfNecessary() FAILED "
            "due to GetRangeExtent() failure", this));
    return hr;
  }

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::RestartCompositionIfNecessary(), "
          "range=%ld-%ld, mComposition={ mStart=%ld, mString.Length()=%lu }",
          this, compStart, compStart + compLength, mComposition.mStart,
          mComposition.mString.Length()));

  if (mComposition.mStart != compStart ||
      mComposition.mString.Length() != (ULONG)compLength) {
    // If the queried composition length is different from the length
    // of our composition string, OnUpdateComposition is being called
    // because a part of the original composition was committed.
    // Reflect that by committing existing composition and starting
    // a new one. RecordCompositionEndAction() followed by
    // RecordCompositionStartAction() will accomplish this automagically.
    RecordCompositionEndAction();
    RecordCompositionStartAction(pComposition, composingRange, true);
  }

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::RestartCompositionIfNecessary() succeeded",
          this));
  return S_OK;
}

static bool
GetColor(const TF_DA_COLOR &aTSFColor, nscolor &aResult)
{
  switch (aTSFColor.type) {
    case TF_CT_SYSCOLOR: {
      DWORD sysColor = ::GetSysColor(aTSFColor.nIndex);
      aResult = NS_RGB(GetRValue(sysColor), GetGValue(sysColor),
                       GetBValue(sysColor));
      return true;
    }
    case TF_CT_COLORREF:
      aResult = NS_RGB(GetRValue(aTSFColor.cr), GetGValue(aTSFColor.cr),
                       GetBValue(aTSFColor.cr));
      return true;
    case TF_CT_NONE:
    default:
      return false;
  }
}

static bool
GetLineStyle(TF_DA_LINESTYLE aTSFLineStyle, uint8_t &aTextRangeLineStyle)
{
  switch (aTSFLineStyle) {
    case TF_LS_NONE:
      aTextRangeLineStyle = TextRangeStyle::LINESTYLE_NONE;
      return true;
    case TF_LS_SOLID:
      aTextRangeLineStyle = TextRangeStyle::LINESTYLE_SOLID;
      return true;
    case TF_LS_DOT:
      aTextRangeLineStyle = TextRangeStyle::LINESTYLE_DOTTED;
      return true;
    case TF_LS_DASH:
      aTextRangeLineStyle = TextRangeStyle::LINESTYLE_DASHED;
      return true;
    case TF_LS_SQUIGGLE:
      aTextRangeLineStyle = TextRangeStyle::LINESTYLE_WAVY;
      return true;
    default:
      return false;
  }
}

HRESULT
nsTextStore::RecordCompositionUpdateAction()
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::RecordCompositionUpdateAction(), "
          "mComposition={ mView=0x%p, mString=\"%s\" }",
          this, mComposition.mView.get(),
          NS_ConvertUTF16toUTF8(mComposition.mString).get()));

  if (!mComposition.IsComposing()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RecordCompositionUpdateAction() FAILED "
            "due to no composition view", this));
    return E_FAIL;
  }

  // Getting display attributes is *really* complicated!
  // We first get the context and the property objects to query for
  // attributes, but since a big range can have a variety of values for
  // the attribute, we have to find out all the ranges that have distinct
  // attribute values. Then we query for what the value represents through
  // the display attribute manager and translate that to TextRange to be
  // sent in NS_TEXT_TEXT

  nsRefPtr<ITfProperty> attrPropetry;
  HRESULT hr = mContext->GetProperty(GUID_PROP_ATTRIBUTE,
                                     getter_AddRefs(attrPropetry));
  if (FAILED(hr) || !attrPropetry) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RecordCompositionUpdateAction() FAILED "
            "due to mContext->GetProperty() failure", this));
    return FAILED(hr) ? hr : E_FAIL;
  }

  nsRefPtr<ITfRange> composingRange;
  hr = mComposition.mView->GetRange(getter_AddRefs(composingRange));
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RecordCompositionUpdateAction() "
            "FAILED due to mComposition.mView->GetRange() failure", this));
    return hr;
  }

  nsRefPtr<IEnumTfRanges> enumRanges;
  hr = attrPropetry->EnumRanges(TfEditCookie(mEditCookie),
                                getter_AddRefs(enumRanges), composingRange);
  if (FAILED(hr) || !enumRanges) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RecordCompositionUpdateAction() FAILED "
            "due to attrPropetry->EnumRanges() failure", this));
    return FAILED(hr) ? hr : E_FAIL;
  }

  // First, put the log of content and selection here.
  Selection& currentSel = CurrentSelection();
  if (currentSel.IsDirty()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RecordCompositionUpdateAction() FAILED "
            "due to CurrentSelection() failure", this));
    return E_FAIL;
  }

  PendingAction* action = GetPendingCompositionUpdate();
  action->mData = mComposition.mString;
  nsTArray<TextRange>& textRanges = action->mRanges;
  // The ranges might already have been initialized already, however, if this
  // is called again, that means we need to overwrite the ranges with current
  // information.
  textRanges.Clear();

  TextRange newRange;
  // No matter if we have display attribute info or not,
  // we always pass in at least one range to NS_TEXT_TEXT
  newRange.mStartOffset = 0;
  newRange.mEndOffset = action->mData.Length();
  newRange.mRangeType = NS_TEXTRANGE_RAWINPUT;
  textRanges.AppendElement(newRange);

  nsRefPtr<ITfRange> range;
  while (S_OK == enumRanges->Next(1, getter_AddRefs(range), nullptr) && range) {

    LONG start = 0, length = 0;
    if (FAILED(GetRangeExtent(range, &start, &length)))
      continue;

    TextRange newRange;
    newRange.mStartOffset = uint32_t(start - mComposition.mStart);
    // The end of the last range in the array is
    // always kept at the end of composition
    newRange.mEndOffset = mComposition.mString.Length();

    TF_DISPLAYATTRIBUTE attr;
    hr = GetDisplayAttribute(attrPropetry, range, &attr);
    if (FAILED(hr)) {
      newRange.mRangeType = NS_TEXTRANGE_RAWINPUT;
    } else {
      newRange.mRangeType = GetGeckoSelectionValue(attr);
      if (GetColor(attr.crText, newRange.mRangeStyle.mForegroundColor)) {
        newRange.mRangeStyle.mDefinedStyles |=
                               TextRangeStyle::DEFINED_FOREGROUND_COLOR;
      }
      if (GetColor(attr.crBk, newRange.mRangeStyle.mBackgroundColor)) {
        newRange.mRangeStyle.mDefinedStyles |=
                               TextRangeStyle::DEFINED_BACKGROUND_COLOR;
      }
      if (GetColor(attr.crLine, newRange.mRangeStyle.mUnderlineColor)) {
        newRange.mRangeStyle.mDefinedStyles |=
                               TextRangeStyle::DEFINED_UNDERLINE_COLOR;
      }
      if (GetLineStyle(attr.lsStyle, newRange.mRangeStyle.mLineStyle)) {
        newRange.mRangeStyle.mDefinedStyles |=
                               TextRangeStyle::DEFINED_LINESTYLE;
        newRange.mRangeStyle.mIsBoldLine = attr.fBoldLine != 0;
      }
    }

    TextRange& lastRange = textRanges[textRanges.Length() - 1];
    if (lastRange.mStartOffset == newRange.mStartOffset) {
      // Replace range if last range is the same as this one
      // So that ranges don't overlap and confuse the editor
      lastRange = newRange;
    } else {
      lastRange.mEndOffset = newRange.mStartOffset;
      textRanges.AppendElement(newRange);
    }
  }

  // We need to hack for Korean Input System which is Korean standard TIP.
  // It sets no change style to IME selection (the selection is always only
  // one).  So, the composition string looks like normal (or committed) string.
  // At this time, current selection range is same as the composition string
  // range.  Other applications set a wide caret which covers the composition
  // string,  however, Gecko doesn't support the wide caret drawing now (Gecko
  // doesn't support XOR drawing), unfortunately.  For now, we should change
  // the range style to undefined.
  if (!currentSel.IsCollapsed() && textRanges.Length() == 1) {
    TextRange& range = textRanges[0];
    LONG start = currentSel.MinOffset();
    LONG end = currentSel.MaxOffset();
    if ((LONG)range.mStartOffset == start - mComposition.mStart &&
        (LONG)range.mEndOffset == end - mComposition.mStart &&
        range.mRangeStyle.IsNoChangeStyle()) {
      range.mRangeStyle.Clear();
      // The looks of selected type is better than others.
      range.mRangeType = NS_TEXTRANGE_SELECTEDRAWTEXT;
    }
  }

  // The caret position has to be collapsed.
  LONG caretPosition = currentSel.MaxOffset();
  caretPosition -= mComposition.mStart;
  TextRange caretRange;
  caretRange.mStartOffset = caretRange.mEndOffset = uint32_t(caretPosition);
  caretRange.mRangeType = NS_TEXTRANGE_CARETPOSITION;
  textRanges.AppendElement(caretRange);

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::RecordCompositionUpdateAction() "
          "succeeded", this));

  return S_OK;
}

HRESULT
nsTextStore::SetSelectionInternal(const TS_SELECTION_ACP* pSelection,
                                  bool aDispatchTextEvent)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::SetSelectionInternal(pSelection=%ld-%ld, "
          "aDispatchTextEvent=%s), IsComposing()=%s",
          this, pSelection->acpStart, pSelection->acpEnd,
          GetBoolName(aDispatchTextEvent),
          GetBoolName(mComposition.IsComposing())));

  MOZ_ASSERT(IsReadWriteLocked());

  Selection& currentSel = CurrentSelection();
  if (currentSel.IsDirty()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
       ("TSF: 0x%p   nsTextStore::SetSelectionInternal() FAILED due to "
        "CurrentSelection() failure", this));
    return E_FAIL;
  }

  if (mComposition.IsComposing()) {
    if (aDispatchTextEvent) {
      HRESULT hr = RestartCompositionIfNecessary();
      if (FAILED(hr)) {
        PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::SetSelectionInternal() FAILED due to "
            "RestartCompositionIfNecessary() failure", this));
        return hr;
      }
    }
    if (pSelection->acpStart < mComposition.mStart ||
        pSelection->acpEnd > mComposition.EndOffset()) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
         ("TSF: 0x%p   nsTextStore::SetSelectionInternal() FAILED due to "
          "the selection being out of the composition string", this));
      return TS_E_INVALIDPOS;
    }
    // Emulate selection during compositions
    currentSel.SetSelection(*pSelection);
    if (aDispatchTextEvent) {
      HRESULT hr = RecordCompositionUpdateAction();
      if (FAILED(hr)) {
        PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::SetSelectionInternal() FAILED due to "
            "RecordCompositionUpdateAction() failure", this));
        return hr;
      }
    }
    return S_OK;
  }

  PendingAction* action = mPendingActions.AppendElement();
  action->mType = PendingAction::SELECTION_SET;
  action->mSelectionStart = pSelection->acpStart;
  action->mSelectionLength = pSelection->acpEnd - pSelection->acpStart;
  action->mSelectionReversed = (pSelection->style.ase == TS_AE_START);

  currentSel.SetSelection(*pSelection);

  return S_OK;
}

STDMETHODIMP
nsTextStore::SetSelection(ULONG ulCount,
                          const TS_SELECTION_ACP *pSelection)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::SetSelection(ulCount=%lu)",
          this, ulCount));

  if (!IsReadWriteLocked()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::SetSelection() FAILED due to "
            "not locked (read-write)", this));
    return TS_E_NOLOCK;
  }
  if (ulCount != 1) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::SetSelection() FAILED due to "
            "trying setting multiple selection", this));
    return E_INVALIDARG;
  }
  if (!pSelection) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::SetSelection() FAILED due to "
            "null argument", this));
    return E_INVALIDARG;
  }

  HRESULT hr = SetSelectionInternal(pSelection, true);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::SetSelection() FAILED due to "
            "SetSelectionInternal() failure", this));
  } else {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::SetSelection() succeeded", this));
  }
  return hr;
}

STDMETHODIMP
nsTextStore::GetText(LONG acpStart,
                     LONG acpEnd,
                     WCHAR *pchPlain,
                     ULONG cchPlainReq,
                     ULONG *pcchPlainOut,
                     TS_RUNINFO *prgRunInfo,
                     ULONG ulRunInfoReq,
                     ULONG *pulRunInfoOut,
                     LONG *pacpNext)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p nsTextStore::GetText(acpStart=%ld, acpEnd=%ld, pchPlain=0x%p, "
     "cchPlainReq=%lu, pcchPlainOut=0x%p, prgRunInfo=0x%p, ulRunInfoReq=%lu, "
     "pulRunInfoOut=0x%p, pacpNext=0x%p), mComposition={ mStart=%ld, "
     "mString.Length()=%lu IsComposing()=%s }",
     this, acpStart, acpEnd, pchPlain, cchPlainReq, pcchPlainOut,
     prgRunInfo, ulRunInfoReq, pulRunInfoOut, pacpNext,
     mComposition.mStart, mComposition.mString.Length(),
     GetBoolName(mComposition.IsComposing())));

  if (!IsReadLocked()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetText() FAILED due to "
            "not locked (read)", this));
    return TS_E_NOLOCK;
  }

  if (!pcchPlainOut || (!pchPlain && !prgRunInfo) ||
      !cchPlainReq != !pchPlain || !ulRunInfoReq != !prgRunInfo) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetText() FAILED due to "
            "invalid argument", this));
    return E_INVALIDARG;
  }

  if (acpStart < 0 || acpEnd < -1 || (acpEnd != -1 && acpStart > acpEnd)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetText() FAILED due to "
            "invalid position", this));
    return TS_E_INVALIDPOS;
  }

  // Making sure to NULL-terminate string just to be on the safe side
  *pcchPlainOut = 0;
  if (pchPlain && cchPlainReq) *pchPlain = 0;
  if (pulRunInfoOut) *pulRunInfoOut = 0;
  if (pacpNext) *pacpNext = acpStart;
  if (prgRunInfo && ulRunInfoReq) {
    prgRunInfo->uCount = 0;
    prgRunInfo->type = TS_RT_PLAIN;
  }

  Content& currentContent = CurrentContent();
  if (!currentContent.IsInitialized()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetText() FAILED due to "
            "CurrentContent() failure", this));
    return E_FAIL;
  }
  if (currentContent.Text().Length() < static_cast<uint32_t>(acpStart)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetText() FAILED due to "
            "acpStart is larger offset than the actual text length", this));
    return TS_E_INVALIDPOS;
  }
  if (acpEnd != -1 &&
      currentContent.Text().Length() < static_cast<uint32_t>(acpEnd)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetText() FAILED due to "
            "acpEnd is larger offset than the actual text length", this));
    return TS_E_INVALIDPOS;
  }
  uint32_t length = (acpEnd == -1) ?
    currentContent.Text().Length() - static_cast<uint32_t>(acpStart) :
    static_cast<uint32_t>(acpEnd - acpStart);
  if (cchPlainReq && cchPlainReq - 1 < length) {
    length = cchPlainReq - 1;
  }
  if (length) {
    if (pchPlain && cchPlainReq) {
      const PRUnichar* startChar =
        currentContent.Text().BeginReading() + acpStart;
      memcpy(pchPlain, startChar, length * sizeof(*pchPlain));
      pchPlain[length] = 0;
      *pcchPlainOut = length;
    }
    if (prgRunInfo && ulRunInfoReq) {
      prgRunInfo->uCount = length;
      prgRunInfo->type = TS_RT_PLAIN;
      if (pulRunInfoOut) *pulRunInfoOut = 1;
    }
    if (pacpNext) *pacpNext = acpStart + length;
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::GetText() succeeded: pcchPlainOut=0x%p, "
          "*prgRunInfo={ uCount=%lu, type=%s }, *pulRunInfoOut=%lu, "
          "*pacpNext=%ld)",
          this, pcchPlainOut, prgRunInfo ? prgRunInfo->uCount : 0,
          prgRunInfo ? GetTextRunTypeName(prgRunInfo->type) : "N/A",
          pulRunInfoOut ? pulRunInfoOut : 0, pacpNext ? pacpNext : 0));
  return S_OK;
}

STDMETHODIMP
nsTextStore::SetText(DWORD dwFlags,
                     LONG acpStart,
                     LONG acpEnd,
                     const WCHAR *pchText,
                     ULONG cch,
                     TS_TEXTCHANGE *pChange)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::SetText(dwFlags=%s, acpStart=%ld, acpEnd=%ld, "
          "pchText=0x%p \"%s\", cch=%lu, pChange=0x%p), IsComposing()=%s",
          this, dwFlags == TS_ST_CORRECTION ? "TS_ST_CORRECTION" :
                                              "not-specified",
          acpStart, acpEnd, pchText,
          pchText && cch ?
            NS_ConvertUTF16toUTF8(pchText, cch).get() : "",
          cch, pChange, GetBoolName(mComposition.IsComposing())));

  // Per SDK documentation, and since we don't have better
  // ways to do this, this method acts as a helper to
  // call SetSelection followed by InsertTextAtSelection
  if (!IsReadWriteLocked()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::SetText() FAILED due to "
            "not locked (read)", this));
    return TS_E_NOLOCK;
  }

  TS_SELECTION_ACP selection;
  selection.acpStart = acpStart;
  selection.acpEnd = acpEnd;
  selection.style.ase = TS_AE_END;
  selection.style.fInterimChar = 0;
  // Set selection to desired range
  HRESULT hr = SetSelectionInternal(&selection);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::SetText() FAILED due to "
            "SetSelectionInternal() failure", this));
    return hr;
  }
  // Replace just selected text
  if (!InsertTextAtSelectionInternal(nsDependentSubstring(pchText, cch),
                                     pChange)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::SetText() FAILED due to "
            "InsertTextAtSelectionInternal() failure", this));
    return E_FAIL;
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::SetText() succeeded: pChange={ "
          "acpStart=%ld, acpOldEnd=%ld, acpNewEnd=%ld }",
          this, pChange ? pChange->acpStart  : 0,
          pChange ? pChange->acpOldEnd : 0, pChange ? pChange->acpNewEnd : 0));
  return S_OK;
}

STDMETHODIMP
nsTextStore::GetFormattedText(LONG acpStart,
                              LONG acpEnd,
                              IDataObject **ppDataObject)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::GetFormattedText() called "
          "but not supported (E_NOTIMPL)", this));

  // no support for formatted text
  return E_NOTIMPL;
}

STDMETHODIMP
nsTextStore::GetEmbedded(LONG acpPos,
                         REFGUID rguidService,
                         REFIID riid,
                         IUnknown **ppunk)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::GetEmbedded() called "
          "but not supported (E_NOTIMPL)", this));

  // embedded objects are not supported
  return E_NOTIMPL;
}

STDMETHODIMP
nsTextStore::QueryInsertEmbedded(const GUID *pguidService,
                                 const FORMATETC *pFormatEtc,
                                 BOOL *pfInsertable)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::QueryInsertEmbedded() called "
          "but not supported, *pfInsertable=FALSE (S_OK)", this));

  // embedded objects are not supported
  *pfInsertable = FALSE;
  return S_OK;
}

STDMETHODIMP
nsTextStore::InsertEmbedded(DWORD dwFlags,
                            LONG acpStart,
                            LONG acpEnd,
                            IDataObject *pDataObject,
                            TS_TEXTCHANGE *pChange)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::InsertEmbedded() called "
          "but not supported (E_NOTIMPL)", this));

  // embedded objects are not supported
  return E_NOTIMPL;
}

void
nsTextStore::SetInputScope(const nsString& aHTMLInputType)
{
  mInputScopes.Clear();
  if (aHTMLInputType.IsEmpty() || aHTMLInputType.EqualsLiteral("text")) {
    return;
  }
  
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-input-element.html
  if (aHTMLInputType.EqualsLiteral("url")) {
    mInputScopes.AppendElement(IS_URL);
  } else if (aHTMLInputType.EqualsLiteral("search")) {
    mInputScopes.AppendElement(IS_SEARCH);
  } else if (aHTMLInputType.EqualsLiteral("email")) {
    mInputScopes.AppendElement(IS_EMAIL_SMTPEMAILADDRESS);
  } else if (aHTMLInputType.EqualsLiteral("password")) {
    mInputScopes.AppendElement(IS_PASSWORD);
  } else if (aHTMLInputType.EqualsLiteral("datetime") ||
             aHTMLInputType.EqualsLiteral("datetime-local")) {
    mInputScopes.AppendElement(IS_DATE_FULLDATE);
    mInputScopes.AppendElement(IS_TIME_FULLTIME);
  } else if (aHTMLInputType.EqualsLiteral("date") ||
             aHTMLInputType.EqualsLiteral("month") ||
             aHTMLInputType.EqualsLiteral("week")) {
    mInputScopes.AppendElement(IS_DATE_FULLDATE);
  } else if (aHTMLInputType.EqualsLiteral("time")) {
    mInputScopes.AppendElement(IS_TIME_FULLTIME);
  } else if (aHTMLInputType.EqualsLiteral("tel")) {
    mInputScopes.AppendElement(IS_TELEPHONE_FULLTELEPHONENUMBER);
    mInputScopes.AppendElement(IS_TELEPHONE_LOCALNUMBER);
  } else if (aHTMLInputType.EqualsLiteral("number")) {
    mInputScopes.AppendElement(IS_NUMBER);
  }
}

HRESULT
nsTextStore::ProcessScopeRequest(DWORD dwFlags,
                                 ULONG cFilterAttrs,
                                 const TS_ATTRID *paFilterAttrs)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::ProcessScopeRequest() called "
          "cFilterAttrs=%d dwFlags=%s", this, cFilterAttrs,
          GetFindFlagName(dwFlags).get()));

  // This is a little weird! RequestSupportedAttrs gives us advanced notice
  // of a support query via RetrieveRequestedAttrs for a specific attribute.
  // RetrieveRequestedAttrs needs to return valid data for all attributes we
  // support, but the text service will only want the input scope object
  // returned in RetrieveRequestedAttrs if the dwFlags passed in here contains
  // TS_ATTR_FIND_WANT_VALUE.
  mInputScopeDetected = mInputScopeRequested = false;

  // Currently we only support GUID_PROP_INPUTSCOPE
  for (uint32_t idx = 0; idx < cFilterAttrs; ++idx) {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::ProcessScopeRequest() "
            "requested attr=%s", this, GetCLSIDNameStr(paFilterAttrs[idx]).get()));
    if (IsEqualGUID(paFilterAttrs[idx], GUID_PROP_INPUTSCOPE)) {
      PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
             ("TSF: 0x%p   nsTextStore::ProcessScopeRequest() "
              "GUID_PROP_INPUTSCOPE queried", this));
      mInputScopeDetected = true;
      if (dwFlags & TS_ATTR_FIND_WANT_VALUE) {
        PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
               ("TSF: 0x%p   nsTextStore::ProcessScopeRequest() "
                "TS_ATTR_FIND_WANT_VALUE specified", this));
        mInputScopeRequested = true;
      }
      break;
    }
  }
  return S_OK;
}

STDMETHODIMP
nsTextStore::RequestSupportedAttrs(DWORD dwFlags,
                                   ULONG cFilterAttrs,
                                   const TS_ATTRID *paFilterAttrs)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::RequestSupportedAttrs() called "
          "cFilterAttrs=%d dwFlags=%s", this, cFilterAttrs,
          GetFindFlagName(dwFlags).get()));

  return ProcessScopeRequest(dwFlags, cFilterAttrs, paFilterAttrs);
}

STDMETHODIMP
nsTextStore::RequestAttrsAtPosition(LONG acpPos,
                                    ULONG cFilterAttrs,
                                    const TS_ATTRID *paFilterAttrs,
                                    DWORD dwFlags)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::RequestAttrsAtPosition() called "
          "acpPos=%d cFilterAttrs=%d dwFlags=%s", this, acpPos, cFilterAttrs,
          GetFindFlagName(dwFlags).get()));

  return ProcessScopeRequest(dwFlags | TS_ATTR_FIND_WANT_VALUE,
                             cFilterAttrs, paFilterAttrs);
}

STDMETHODIMP
nsTextStore::RequestAttrsTransitioningAtPosition(LONG acpPos,
                                                 ULONG cFilterAttrs,
                                                 const TS_ATTRID *paFilterAttr,
                                                 DWORD dwFlags)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::RequestAttrsTransitioningAtPosition() called "
          "but not supported (S_OK)", this));

  // no per character attributes defined
  return S_OK;
}

STDMETHODIMP
nsTextStore::FindNextAttrTransition(LONG acpStart,
                                    LONG acpHalt,
                                    ULONG cFilterAttrs,
                                    const TS_ATTRID *paFilterAttrs,
                                    DWORD dwFlags,
                                    LONG *pacpNext,
                                    BOOL *pfFound,
                                    LONG *plFoundOffset)
{
  if (!pacpNext || !pfFound || !plFoundOffset) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p nsTextStore::FindNextAttrTransition() FAILED due to "
            "null argument", this));
    return E_INVALIDARG;
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::FindNextAttrTransition() called "
          "but not supported (S_OK)", this));

  // no per character attributes defined
  *pacpNext = *plFoundOffset = acpHalt;
  *pfFound = FALSE;
  return S_OK;
}

STDMETHODIMP
nsTextStore::RetrieveRequestedAttrs(ULONG ulCount,
                                    TS_ATTRVAL *paAttrVals,
                                    ULONG *pcFetched)
{
  if (!pcFetched || !ulCount || !paAttrVals) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p nsTextStore::RetrieveRequestedAttrs() FAILED due to "
            "null argument", this));
    return E_INVALIDARG;
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::RetrieveRequestedAttrs() called "
          "ulCount=%d", this, ulCount));

  if (mInputScopeDetected || mInputScopeRequested) {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::RetrieveRequestedAttrs() for "
            "GUID_PROP_INPUTSCOPE: "
            "mInputScopeDetected=%s mInputScopeRequested=%s",
            this, GetBoolName(mInputScopeDetected),
            GetBoolName(mInputScopeRequested)));

    paAttrVals->idAttr = GUID_PROP_INPUTSCOPE;
    paAttrVals->dwOverlapId = 0;
    paAttrVals->varValue.vt = VT_EMPTY;
    *pcFetched = 1;

    if (mInputScopeRequested) {
      paAttrVals->varValue.vt = VT_UNKNOWN;
      paAttrVals->varValue.punkVal = (IUnknown*) new InputScopeImpl(mInputScopes);
    }

    mInputScopeDetected = mInputScopeRequested = false;
    return S_OK;
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::RetrieveRequestedAttrs() called "
          "for unknown TS_ATTRVAL, *pcFetched=0 (S_OK)", this));

  paAttrVals->dwOverlapId = 0;
  paAttrVals->varValue.vt = VT_EMPTY;
  *pcFetched = 0;
  return S_OK;
}

STDMETHODIMP
nsTextStore::GetEndACP(LONG *pacp)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::GetEndACP(pacp=0x%p)", this, pacp));

  if (!IsReadLocked()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetEndACP() FAILED due to "
            "not locked (read)", this));
    return TS_E_NOLOCK;
  }

  if (!pacp) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetEndACP() FAILED due to "
            "null argument", this));
    return E_INVALIDARG;
  }

  Content& currentContent = CurrentContent();
  if (!currentContent.IsInitialized()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetEndACP() FAILED due to "
            "CurrentContent() failure", this));
    return E_FAIL;
  }
  *pacp = static_cast<LONG>(currentContent.Text().Length());
  return S_OK;
}

STDMETHODIMP
nsTextStore::GetActiveView(TsViewCookie *pvcView)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::GetActiveView(pvcView=0x%p)", this, pvcView));

  if (!pvcView) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetActiveView() FAILED due to "
            "null argument", this));
    return E_INVALIDARG;
  }

  *pvcView = TEXTSTORE_DEFAULT_VIEW;

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::GetActiveView() succeeded: *pvcView=%ld",
          this, *pvcView));
  return S_OK;
}

STDMETHODIMP
nsTextStore::GetACPFromPoint(TsViewCookie vcView,
                             const POINT *pt,
                             DWORD dwFlags,
                             LONG *pacp)
{
  if (!IsReadLocked()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetACPFromPoint() FAILED due to "
            "not locked (read)", this));
    return TS_E_NOLOCK;
  }

  if (vcView != TEXTSTORE_DEFAULT_VIEW) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetACPFromPoint() FAILED due to "
            "called with invalid view", this));
    return E_INVALIDARG;
  }

  if (mContent.IsLayoutChanged()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetACPFromPoint() FAILED due to "
            "layout not recomputed", this));
    mContent.NeedsToNotifyTSFOfLayoutChange();
    return TS_E_NOLAYOUT;
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::GetACPFromPoint(vcView=%ld, "
          "pt(0x%p)={ x=%ld, y=%ld }, dwFlags=%s, pacp=0x%p) called "
          "but not supported (E_NOTIMPL)", this));

  // not supported for now
  return E_NOTIMPL;
}

STDMETHODIMP
nsTextStore::GetTextExt(TsViewCookie vcView,
                        LONG acpStart,
                        LONG acpEnd,
                        RECT *prc,
                        BOOL *pfClipped)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::GetTextExt(vcView=%ld, "
          "acpStart=%ld, acpEnd=%ld, prc=0x%p, pfClipped=0x%p)",
          this, vcView, acpStart, acpEnd, prc, pfClipped));

  if (!IsReadLocked()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetTextExt() FAILED due to "
            "not locked (read)", this));
    return TS_E_NOLOCK;
  }

  if (vcView != TEXTSTORE_DEFAULT_VIEW) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetTextExt() FAILED due to "
            "called with invalid view", this));
    return E_INVALIDARG;
  }

  if (!prc || !pfClipped) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetTextExt() FAILED due to "
            "null argument", this));
    return E_INVALIDARG;
  }

  if (acpStart < 0 || acpEnd < acpStart) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetTextExt() FAILED due to "
            "invalid position", this));
    return TS_E_INVALIDPOS;
  }

  if (mContent.IsLayoutChangedAfter(acpEnd)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetTextExt() FAILED due to "
            "layout not recomputed at %d", this, acpEnd));
    mContent.NeedsToNotifyTSFOfLayoutChange();
    return TS_E_NOLAYOUT;
  }

  // use NS_QUERY_TEXT_RECT to get rect in system, screen coordinates
  WidgetQueryContentEvent event(true, NS_QUERY_TEXT_RECT, mWidget);
  mWidget->InitEvent(event);
  event.InitForQueryTextRect(acpStart, acpEnd - acpStart);
  mWidget->DispatchWindowEvent(&event);
  if (!event.mSucceeded) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetTextExt() FAILED due to "
            "NS_QUERY_TEXT_RECT failure", this));
    return TS_E_INVALIDPOS; // but unexpected failure, maybe.
  }
  // IMEs don't like empty rects, fix here
  if (event.mReply.mRect.width <= 0)
    event.mReply.mRect.width = 1;
  if (event.mReply.mRect.height <= 0)
    event.mReply.mRect.height = 1;

  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Desktop) {
    // convert to unclipped screen rect
    nsWindow* refWindow = static_cast<nsWindow*>(
      event.mReply.mFocusedWidget ? event.mReply.mFocusedWidget : mWidget);
    // Result rect is in top level widget coordinates
    refWindow = refWindow->GetTopLevelWindow(false);
    if (!refWindow) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::GetTextExt() FAILED due to "
              "no top level window", this));
      return E_FAIL;
    }

    event.mReply.mRect.MoveBy(refWindow->WidgetToScreenOffset());
  }

  // get bounding screen rect to test for clipping
  if (!GetScreenExtInternal(*prc)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetTextExt() FAILED due to "
            "GetScreenExtInternal() failure", this));
    return E_FAIL;
  }

  // clip text rect to bounding rect
  RECT textRect;
  ::SetRect(&textRect, event.mReply.mRect.x, event.mReply.mRect.y,
            event.mReply.mRect.XMost(), event.mReply.mRect.YMost());
  if (!::IntersectRect(prc, prc, &textRect))
    // Text is not visible
    ::SetRectEmpty(prc);

  // not equal if text rect was clipped
  *pfClipped = !::EqualRect(prc, &textRect);

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::GetTextExt() succeeded: "
          "*prc={ left=%ld, top=%ld, right=%ld, bottom=%ld }, *pfClipped=%s",
          this, prc->left, prc->top, prc->right, prc->bottom,
          GetBoolName(*pfClipped)));

  return S_OK;
}

STDMETHODIMP
nsTextStore::GetScreenExt(TsViewCookie vcView,
                          RECT *prc)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::GetScreenExt(vcView=%ld, prc=0x%p)",
          this, vcView, prc));

  if (vcView != TEXTSTORE_DEFAULT_VIEW) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetScreenExt() FAILED due to "
            "called with invalid view", this));
    return E_INVALIDARG;
  }

  if (!prc) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetScreenExt() FAILED due to "
            "null argument", this));
    return E_INVALIDARG;
  }

  if (!GetScreenExtInternal(*prc)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetScreenExt() FAILED due to "
            "GetScreenExtInternal() failure", this));
    return E_FAIL;
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::GetScreenExt() succeeded: "
          "*prc={ left=%ld, top=%ld, right=%ld, bottom=%ld }",
          this, prc->left, prc->top, prc->right, prc->bottom));
  return S_OK;
}

bool
nsTextStore::GetScreenExtInternal(RECT &aScreenExt)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::GetScreenExtInternal()", this));

  // use NS_QUERY_EDITOR_RECT to get rect in system, screen coordinates
  WidgetQueryContentEvent event(true, NS_QUERY_EDITOR_RECT, mWidget);
  mWidget->InitEvent(event);
  mWidget->DispatchWindowEvent(&event);
  if (!event.mSucceeded) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetScreenExtInternal() FAILED due to "
            "NS_QUERY_EDITOR_RECT failure", this));
    return false;
  }

  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Metro) {
    nsIntRect boundRect;
    if (NS_FAILED(mWidget->GetClientBounds(boundRect))) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::GetScreenExtInternal() FAILED due to "
              "failed to get the client bounds", this));
      return false;
    }
    ::SetRect(&aScreenExt, boundRect.x, boundRect.y,
              boundRect.XMost(), boundRect.YMost());
  } else {
    NS_ASSERTION(XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Desktop,
                 "environment isn't WindowsEnvironmentType_Desktop!");
    nsWindow* refWindow = static_cast<nsWindow*>(
      event.mReply.mFocusedWidget ?
        event.mReply.mFocusedWidget : mWidget);
    // Result rect is in top level widget coordinates
    refWindow = refWindow->GetTopLevelWindow(false);
    if (!refWindow) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::GetScreenExtInternal() FAILED due to "
              "no top level window", this));
      return false;
    }

    nsIntRect boundRect;
    if (NS_FAILED(refWindow->GetClientBounds(boundRect))) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::GetScreenExtInternal() FAILED due to "
              "failed to get the client bounds", this));
      return false;
    }

    boundRect.MoveTo(0, 0);

    // Clip frame rect to window rect
    boundRect.IntersectRect(event.mReply.mRect, boundRect);
    if (!boundRect.IsEmpty()) {
      boundRect.MoveBy(refWindow->WidgetToScreenOffset());
      ::SetRect(&aScreenExt, boundRect.x, boundRect.y,
                boundRect.XMost(), boundRect.YMost());
    } else {
      ::SetRectEmpty(&aScreenExt);
    }
  }

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::GetScreenExtInternal() succeeded: "
          "aScreenExt={ left=%ld, top=%ld, right=%ld, bottom=%ld }",
          this, aScreenExt.left, aScreenExt.top,
          aScreenExt.right, aScreenExt.bottom));
  return true;
}

STDMETHODIMP
nsTextStore::GetWnd(TsViewCookie vcView,
                    HWND *phwnd)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::GetWnd(vcView=%ld, phwnd=0x%p), "
          "mWidget=0x%p",
          this, vcView, phwnd, mWidget.get()));

  if (vcView != TEXTSTORE_DEFAULT_VIEW) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetWnd() FAILED due to "
            "called with invalid view", this));
    return E_INVALIDARG;
  }

  if (!phwnd) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetScreenExt() FAILED due to "
            "null argument", this));
    return E_INVALIDARG;
  }

  *phwnd = mWidget->GetWindowHandle();

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::GetWnd() succeeded: *phwnd=0x%p",
          this, static_cast<void*>(*phwnd)));
  return S_OK;
}

STDMETHODIMP
nsTextStore::InsertTextAtSelection(DWORD dwFlags,
                                   const WCHAR *pchText,
                                   ULONG cch,
                                   LONG *pacpStart,
                                   LONG *pacpEnd,
                                   TS_TEXTCHANGE *pChange)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::InsertTextAtSelection(dwFlags=%s, "
          "pchText=0x%p \"%s\", cch=%lu, pacpStart=0x%p, pacpEnd=0x%p, "
          "pChange=0x%p), IsComposing()=%s",
          this, dwFlags == 0 ? "0" :
                dwFlags == TF_IAS_NOQUERY ? "TF_IAS_NOQUERY" :
                dwFlags == TF_IAS_QUERYONLY ? "TF_IAS_QUERYONLY" : "Unknown",
          pchText,
          pchText && cch ? NS_ConvertUTF16toUTF8(pchText, cch).get() : "",
          cch, pacpStart, pacpEnd, pChange,
          GetBoolName(mComposition.IsComposing())));

  if (cch && !pchText) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::InsertTextAtSelection() FAILED due to "
            "null pchText", this));
    return E_INVALIDARG;
  }

  if (TS_IAS_QUERYONLY == dwFlags) {
    if (!IsReadLocked()) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::InsertTextAtSelection() FAILED due to "
              "not locked (read)", this));
      return TS_E_NOLOCK;
    }

    if (!pacpStart || !pacpEnd) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::InsertTextAtSelection() FAILED due to "
              "null argument", this));
      return E_INVALIDARG;
    }

    // Get selection first
    Selection& currentSel = CurrentSelection();
    if (currentSel.IsDirty()) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::InsertTextAtSelection() FAILED due to "
              "CurrentSelection() failure", this));
      return E_FAIL;
    }

    // Simulate text insertion
    *pacpStart = currentSel.StartOffset();
    *pacpEnd = currentSel.EndOffset();
    if (pChange) {
      pChange->acpStart = currentSel.StartOffset();
      pChange->acpOldEnd = currentSel.EndOffset();
      pChange->acpNewEnd = currentSel.StartOffset() + static_cast<LONG>(cch);
    }
  } else {
    if (!IsReadWriteLocked()) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::InsertTextAtSelection() FAILED due to "
              "not locked (read-write)", this));
      return TS_E_NOLOCK;
    }

    if (!pChange) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::InsertTextAtSelection() FAILED due to "
              "null pChange", this));
      return E_INVALIDARG;
    }

    if (TS_IAS_NOQUERY != dwFlags && (!pacpStart || !pacpEnd)) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::InsertTextAtSelection() FAILED due to "
              "null argument", this));
      return E_INVALIDARG;
    }

    if (!InsertTextAtSelectionInternal(nsDependentSubstring(pchText, cch),
                                       pChange)) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::InsertTextAtSelection() FAILED due to "
              "InsertTextAtSelectionInternal() failure", this));
      return E_FAIL;
    }

    if (TS_IAS_NOQUERY != dwFlags) {
      *pacpStart = pChange->acpStart;
      *pacpEnd = pChange->acpNewEnd;
    }
  }
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::InsertTextAtSelection() succeeded: "
          "*pacpStart=%ld, *pacpEnd=%ld, "
          "*pChange={ acpStart=%ld, acpOldEnd=%ld, acpNewEnd=%ld })",
          this, pacpStart ? *pacpStart : 0, pacpEnd ? *pacpEnd : 0,
          pChange ? pChange->acpStart: 0, pChange ? pChange->acpOldEnd : 0,
          pChange ? pChange->acpNewEnd : 0));
  return S_OK;
}

bool
nsTextStore::InsertTextAtSelectionInternal(const nsAString &aInsertStr,
                                           TS_TEXTCHANGE* aTextChange)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::InsertTextAtSelectionInternal("
          "aInsertStr=\"%s\", aTextChange=0x%p), IsComposing=%s",
          this, NS_ConvertUTF16toUTF8(aInsertStr).get(), aTextChange,
          GetBoolName(mComposition.IsComposing())));

  Content& currentContent = CurrentContent();
  if (!currentContent.IsInitialized()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::InsertTextAtSelectionInternal() failed "
            "due to CurrentContent() failure()", this));
    return false;
  }

  TS_SELECTION_ACP oldSelection = currentContent.Selection().ACP();
  if (!mComposition.IsComposing()) {
    // Use a temporary composition to contain the text
    PendingAction* compositionStart = mPendingActions.AppendElement();
    compositionStart->mType = PendingAction::COMPOSITION_START;
    compositionStart->mSelectionStart = oldSelection.acpStart;
    compositionStart->mSelectionLength =
      oldSelection.acpEnd - oldSelection.acpStart;

    PendingAction* compositionEnd = mPendingActions.AppendElement();
    compositionEnd->mType = PendingAction::COMPOSITION_END;
    compositionEnd->mData = aInsertStr;
  }

  currentContent.ReplaceSelectedTextWith(aInsertStr);

  if (aTextChange) {
    aTextChange->acpStart = oldSelection.acpStart;
    aTextChange->acpOldEnd = oldSelection.acpEnd;
    aTextChange->acpNewEnd = currentContent.Selection().EndOffset();
  }

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::InsertTextAtSelectionInternal() succeeded: "
          "mWidget=0x%p, mWidget->Destroyed()=%s, aTextChange={ acpStart=%ld, "
          "acpOldEnd=%ld, acpNewEnd=%ld }",
          this, mWidget.get(),
          GetBoolName(mWidget ? mWidget->Destroyed() : true),
          aTextChange ? aTextChange->acpStart : 0,
          aTextChange ? aTextChange->acpOldEnd : 0,
          aTextChange ? aTextChange->acpNewEnd : 0));
  return true;
}

STDMETHODIMP
nsTextStore::InsertEmbeddedAtSelection(DWORD dwFlags,
                                       IDataObject *pDataObject,
                                       LONG *pacpStart,
                                       LONG *pacpEnd,
                                       TS_TEXTCHANGE *pChange)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::InsertEmbeddedAtSelection() called "
          "but not supported (E_NOTIMPL)", this));

  // embedded objects are not supported
  return E_NOTIMPL;
}

HRESULT
nsTextStore::RecordCompositionStartAction(ITfCompositionView* pComposition,
                                          ITfRange* aRange,
                                          bool aPreserveSelection)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::RecordCompositionStartAction("
          "pComposition=0x%p, aRange=0x%p, aPreserveSelection=%s), "
          "mComposition.mView=0x%p",
          this, pComposition, aRange, GetBoolName(aPreserveSelection),
          mComposition.mView.get()));

  LONG start = 0, length = 0;
  HRESULT hr = GetRangeExtent(aRange, &start, &length);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RecordCompositionStartAction() FAILED "
            "due to GetRangeExtent() failure", this));
    return hr;
  }

  Content& currentContent = CurrentContent();
  if (!currentContent.IsInitialized()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RecordCompositionStartAction() FAILED "
            "due to CurrentContent() failure", this));
    return E_FAIL;
  }

  PendingAction* action = mPendingActions.AppendElement();
  action->mType = PendingAction::COMPOSITION_START;
  action->mSelectionStart = start;
  action->mSelectionLength = length;

  currentContent.StartComposition(pComposition, *action, aPreserveSelection);

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::RecordCompositionStartAction() succeeded: "
          "mComposition={ mStart=%ld, mString.Length()=%ld, "
          "mSelection={ acpStart=%ld, acpEnd=%ld, style.ase=%s, "
          "style.fInterimChar=%s } }",
          this, mComposition.mStart, mComposition.mString.Length(),
          mSelection.StartOffset(), mSelection.EndOffset(),
          GetActiveSelEndName(mSelection.ActiveSelEnd()),
          GetBoolName(mSelection.IsInterimChar())));
  return S_OK;
}

HRESULT
nsTextStore::RecordCompositionEndAction()
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p nsTextStore::RecordCompositionEndAction(), "
          "mComposition={ mView=0x%p, mString=\"%s\" }",
          this, mComposition.mView.get(),
          NS_ConvertUTF16toUTF8(mComposition.mString).get()));

  MOZ_ASSERT(mComposition.IsComposing());

  PendingAction* action = mPendingActions.AppendElement();
  action->mType = PendingAction::COMPOSITION_END;
  action->mData = mComposition.mString;

  Content& currentContent = CurrentContent();
  if (!currentContent.IsInitialized()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RecordCompositionEndAction() FAILED due "
            "to CurrentContent() failure", this));
    return E_FAIL;
  }
  currentContent.EndComposition(*action);

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::RecordCompositionEndAction(), succeeded",
          this));
  return S_OK;
}

STDMETHODIMP
nsTextStore::OnStartComposition(ITfCompositionView* pComposition,
                                BOOL* pfOk)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::OnStartComposition(pComposition=0x%p, "
          "pfOk=0x%p), mComposition.mView=0x%p",
          this, pComposition, pfOk, mComposition.mView.get()));

  AutoPendingActionAndContentFlusher flusher(this);

  *pfOk = FALSE;

  // Only one composition at a time
  if (mComposition.IsComposing()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::OnStartComposition() FAILED due to "
            "there is another composition already (but returns S_OK)", this));
    return S_OK;
  }

  nsRefPtr<ITfRange> range;
  HRESULT hr = pComposition->GetRange(getter_AddRefs(range));
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::OnStartComposition() FAILED due to "
            "pComposition->GetRange() failure", this));
    return hr;
  }
  hr = RecordCompositionStartAction(pComposition, range, false);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::OnStartComposition() FAILED due to "
            "RecordCompositionStartAction() failure", this));
    return hr;
  }

  *pfOk = TRUE;
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::OnStartComposition() succeeded", this));
  return S_OK;
}

STDMETHODIMP
nsTextStore::OnUpdateComposition(ITfCompositionView* pComposition,
                                 ITfRange* pRangeNew)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::OnUpdateComposition(pComposition=0x%p, "
          "pRangeNew=0x%p), mComposition.mView=0x%p",
          this, pComposition, pRangeNew, mComposition.mView.get()));

  AutoPendingActionAndContentFlusher flusher(this);

  if (!mDocumentMgr || !mContext) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::OnUpdateComposition() FAILED due to "
            "not ready for the composition", this));
    return E_UNEXPECTED;
  }
  if (!mComposition.IsComposing()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::OnUpdateComposition() FAILED due to "
            "no active composition", this));
    return E_UNEXPECTED;
  }
  if (mComposition.mView != pComposition) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::OnUpdateComposition() FAILED due to "
            "different composition view specified", this));
    return E_UNEXPECTED;
  }

  // pRangeNew is null when the update is not complete
  if (!pRangeNew) {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::OnUpdateComposition() succeeded but "
            "not complete", this));
    return S_OK;
  }

  HRESULT hr = RestartCompositionIfNecessary(pRangeNew);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::OnUpdateComposition() FAILED due to "
            "RestartCompositionIfNecessary() failure", this));
    return hr;
  }

  hr = RecordCompositionUpdateAction();
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::OnUpdateComposition() FAILED due to "
            "RecordCompositionUpdateAction() failure", this));
    return hr;
  }

#ifdef PR_LOGGING
  if (PR_LOG_TEST(sTextStoreLog, PR_LOG_ALWAYS)) {
    Selection& currentSel = CurrentSelection();
    if (currentSel.IsDirty()) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::OnUpdateComposition() FAILED due to "
              "CurrentSelection() failure", this));
      return E_FAIL;
    }
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::OnUpdateComposition() succeeded: "
            "mComposition={ mStart=%ld, mString=\"%s\" }, "
            "CurrentSelection()={ acpStart=%ld, acpEnd=%ld, style.ase=%s }",
            this, mComposition.mStart,
            NS_ConvertUTF16toUTF8(mComposition.mString).get(),
            currentSel.StartOffset(), currentSel.EndOffset(),
            GetActiveSelEndName(currentSel.ActiveSelEnd())));
  }
#endif // #ifdef PR_LOGGING
  return S_OK;
}

STDMETHODIMP
nsTextStore::OnEndComposition(ITfCompositionView* pComposition)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::OnEndComposition(pComposition=0x%p), "
          "mComposition={ mView=0x%p, mString=\"%s\" }",
          this, pComposition, mComposition.mView.get(),
          NS_ConvertUTF16toUTF8(mComposition.mString).get()));

  AutoPendingActionAndContentFlusher flusher(this);

  if (!mComposition.IsComposing()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::OnEndComposition() FAILED due to "
            "no active composition", this));
    return E_UNEXPECTED;
  }

  if (mComposition.mView != pComposition) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::OnEndComposition() FAILED due to "
            "different composition view specified", this));
    return E_UNEXPECTED;
  }

  HRESULT hr = RecordCompositionEndAction();
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::OnEndComposition() FAILED due to "
            "RecordCompositionEndAction() failure", this));
    return hr;
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::OnEndComposition(), succeeded", this));
  return S_OK;
}

// static
nsresult
nsTextStore::OnFocusChange(bool aGotFocus,
                           nsWindowBase* aFocusedWidget,
                           IMEState::Enabled aIMEEnabled)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: nsTextStore::OnFocusChange(aGotFocus=%s, "
          "aFocusedWidget=0x%p, aIMEEnabled=%s), sTsfThreadMgr=0x%p, "
          "sTsfTextStore=0x%p",
          GetBoolName(aGotFocus), aFocusedWidget,
          GetIMEEnabledName(aIMEEnabled), sTsfThreadMgr, sTsfTextStore));

  // no change notifications if TSF is disabled
  NS_ENSURE_TRUE(sTsfThreadMgr && sTsfTextStore, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<ITfDocumentMgr> prevFocusedDocumentMgr;
  if (aGotFocus && (aIMEEnabled == IMEState::ENABLED ||
                    aIMEEnabled == IMEState::PASSWORD)) {
    bool bRet = sTsfTextStore->Create(aFocusedWidget);
    NS_ENSURE_TRUE(bRet, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(sTsfTextStore->mDocumentMgr, NS_ERROR_FAILURE);
    if (aIMEEnabled == IMEState::PASSWORD) {
      MarkContextAsKeyboardDisabled(sTsfTextStore->mContext);
      nsRefPtr<ITfContext> topContext;
      sTsfTextStore->mDocumentMgr->GetTop(getter_AddRefs(topContext));
      if (topContext && topContext != sTsfTextStore->mContext) {
        MarkContextAsKeyboardDisabled(topContext);
      }
    }
    HRESULT hr = sTsfThreadMgr->SetFocus(sTsfTextStore->mDocumentMgr);
    NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);
    // Use AssociateFocus() for ensuring that any native focus event
    // never steal focus from our documentMgr.
    hr = sTsfThreadMgr->AssociateFocus(aFocusedWidget->GetWindowHandle(),
                                       sTsfTextStore->mDocumentMgr,
                                       getter_AddRefs(prevFocusedDocumentMgr));
    NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);
  } else {
    if (ThinksHavingFocus()) {
      DebugOnly<HRESULT> hr = sTsfThreadMgr->AssociateFocus(
                                sTsfTextStore->mWidget->GetWindowHandle(),
                                nullptr, getter_AddRefs(prevFocusedDocumentMgr));
      NS_ASSERTION(SUCCEEDED(hr), "Disassociating focus failed");
      NS_ASSERTION(prevFocusedDocumentMgr == sTsfTextStore->mDocumentMgr,
                   "different documentMgr has been associated with the window");
      sTsfTextStore->Destroy();
    }
    HRESULT hr = sTsfThreadMgr->SetFocus(sTsfDisabledDocumentMgr);
    NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);
  }
  return NS_OK;
}

// static
nsIMEUpdatePreference
nsTextStore::GetIMEUpdatePreference()
{
  int8_t notifications = nsIMEUpdatePreference::NOTIFY_NOTHING;
  if (sTsfThreadMgr && sTsfTextStore && sTsfTextStore->mDocumentMgr) {
    nsRefPtr<ITfDocumentMgr> docMgr;
    sTsfThreadMgr->GetFocus(getter_AddRefs(docMgr));
    if (docMgr == sTsfTextStore->mDocumentMgr) {
      notifications = (nsIMEUpdatePreference::NOTIFY_SELECTION_CHANGE |
                       nsIMEUpdatePreference::NOTIFY_TEXT_CHANGE);
    }
  }
  return nsIMEUpdatePreference(notifications, false);
}

nsresult
nsTextStore::OnTextChangeInternal(uint32_t aStart,
                                  uint32_t aOldEnd,
                                  uint32_t aNewEnd)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p nsTextStore::OnTextChangeInternal(aStart=%lu, "
          "aOldEnd=%lu, aNewEnd=%lu), mSink=0x%p, mSinkMask=%s, "
          "mTextChange={ acpStart=%ld, acpOldEnd=%ld, acpNewEnd=%ld }",
          this, aStart, aOldEnd, aNewEnd, mSink.get(),
          GetSinkMaskNameStr(mSinkMask).get(), mTextChange.acpStart,
          mTextChange.acpOldEnd, mTextChange.acpNewEnd));

  if (IsReadLocked()) {
    return NS_OK;
  }

  NS_ASSERTION(!mComposition.IsComposing(), "text changed during composition");
  mSelection.MarkDirty();

  if (mSink && 0 != (mSinkMask & TS_AS_TEXT_CHANGE)) {
    mTextChange.acpStart = std::min(mTextChange.acpStart, LONG(aStart));
    mTextChange.acpOldEnd = std::max(mTextChange.acpOldEnd, LONG(aOldEnd));
    mTextChange.acpNewEnd = std::max(mTextChange.acpNewEnd, LONG(aNewEnd));
    ::PostMessageW(mWidget->GetWindowHandle(),
                   WM_USER_TSF_TEXTCHANGE, 0, 0);
  }
  return NS_OK;
}

void
nsTextStore::OnTextChangeMsgInternal(void)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p nsTextStore::OnTextChangeMsgInternal(), "
          "mSink=0x%p, mSinkMask=%s, mTextChange={ acpStart=%ld, "
          "acpOldEnd=%ld, acpNewEnd=%ld }",
          this, mSink.get(),
          GetSinkMaskNameStr(mSinkMask).get(), mTextChange.acpStart,
          mTextChange.acpOldEnd, mTextChange.acpNewEnd));

  if (!mLock && mSink && 0 != (mSinkMask & TS_AS_TEXT_CHANGE) &&
      INT32_MAX > mTextChange.acpStart) {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::OnTextChangeMsgInternal(), calling"
            "mSink->OnTextChange(0, { acpStart=%ld, acpOldEnd=%ld, "
            "acpNewEnd=%ld })...", this, mTextChange.acpStart,
            mTextChange.acpOldEnd, mTextChange.acpNewEnd));
    mSink->OnTextChange(0, &mTextChange);
    mTextChange.acpStart = INT32_MAX;
    mTextChange.acpOldEnd = mTextChange.acpNewEnd = 0;
  }
}

nsresult
nsTextStore::OnSelectionChangeInternal(void)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p nsTextStore::OnSelectionChangeInternal(), "
          "mSink=0x%p, mSinkMask=%s, mIsRecordingActionsWithoutLock=%s",
          this, mSink.get(), GetSinkMaskNameStr(mSinkMask).get(),
          GetBoolName(mIsRecordingActionsWithoutLock)));

  if (IsReadLocked()) {
    return NS_OK;
  }

  NS_ASSERTION(!mComposition.IsComposing(),
               "selection changed during composition");
  mSelection.MarkDirty();

  if (mSink && 0 != (mSinkMask & TS_AS_SEL_CHANGE)) {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::OnSelectionChangeInternal(), calling "
            "mSink->OnSelectionChange()...", this));
    if (!mIsRecordingActionsWithoutLock) {
      mSink->OnSelectionChange();
    } else {
      mNotifySelectionChange = true;
    }
  }
  return NS_OK;
}

nsresult
nsTextStore::OnLayoutChange()
{
  NS_ENSURE_TRUE(mContext, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSink, NS_ERROR_FAILURE);

  // XXXmnakano We always call OnLayoutChange for now, but this might use CPU
  // power when the focused editor has very long text. Ideally, we should call
  // this only when the composition string screen position is changed by window
  // moving, resizing. And also reflowing and scrolling the contents.
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::OnLayoutChange(), calling "
          "mSink->OnLayoutChange()...", this));
  HRESULT hr = mSink->OnLayoutChange(TS_LC_CHANGE, TEXTSTORE_DEFAULT_VIEW);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  return NS_OK;
}

void
nsTextStore::CommitCompositionInternal(bool aDiscard)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p nsTextStore::CommitCompositionInternal(aDiscard=%s), "
          "mSink=0x%p, mContext=0x%p, mComposition.mView=0x%p, "
          "mComposition.mString=\"%s\"",
          this, GetBoolName(aDiscard), mSink.get(), mContext.get(),
          mComposition.mView.get(),
          NS_ConvertUTF16toUTF8(mComposition.mString).get()));

  if (mComposition.IsComposing() && aDiscard) {
    LONG endOffset = mComposition.EndOffset();
    mComposition.mString.Truncate(0);
    if (mSink && !mLock) {
      TS_TEXTCHANGE textChange;
      textChange.acpStart = mComposition.mStart;
      textChange.acpOldEnd = endOffset;
      textChange.acpNewEnd = mComposition.mStart;
      PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
             ("TSF: 0x%p   nsTextStore::CommitCompositionInternal(), calling"
              "mSink->OnTextChange(0, { acpStart=%ld, acpOldEnd=%ld, "
              "acpNewEnd=%ld })...", this, textChange.acpStart,
              textChange.acpOldEnd, textChange.acpNewEnd));
      mSink->OnTextChange(0, &textChange);
    }
  }
  // Terminate two contexts, the base context (mContext) and the top
  // if the top context is not the same as the base context
  nsRefPtr<ITfContext> context = mContext;
  do {
    if (context) {
      nsRefPtr<ITfContextOwnerCompositionServices> services;
      context->QueryInterface(IID_ITfContextOwnerCompositionServices,
                              getter_AddRefs(services));
      if (services) {
        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::CommitCompositionInternal(), "
                "requesting TerminateComposition() for the context 0x%p...",
                this, context.get()));
        services->TerminateComposition(nullptr);
      }
    }
    if (context != mContext)
      break;
    if (mDocumentMgr)
      mDocumentMgr->GetTop(getter_AddRefs(context));
  } while (context != mContext);
}

static
bool
GetCompartment(IUnknown* pUnk,
               const GUID& aID,
               ITfCompartment** aCompartment)
{
  if (!pUnk) return false;

  nsRefPtr<ITfCompartmentMgr> compMgr;
  pUnk->QueryInterface(IID_ITfCompartmentMgr, getter_AddRefs(compMgr));
  if (!compMgr) return false;

  return SUCCEEDED(compMgr->GetCompartment(aID, aCompartment)) &&
         (*aCompartment) != nullptr;
}

// static
void
nsTextStore::SetIMEOpenState(bool aState)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: nsTextStore::SetIMEOpenState(aState=%s)", GetBoolName(aState)));

  nsRefPtr<ITfCompartment> comp;
  if (!GetCompartment(sTsfThreadMgr,
                      GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                      getter_AddRefs(comp))) {
    PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
           ("TSF:   nsTextStore::SetIMEOpenState() FAILED due to"
            "no compartment available"));
    return;
  }

  VARIANT variant;
  variant.vt = VT_I4;
  variant.lVal = aState;
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF:   nsTextStore::SetIMEOpenState(), setting "
          "0x%04X to GUID_COMPARTMENT_KEYBOARD_OPENCLOSE...",
          variant.lVal));
  comp->SetValue(sTsfClientId, &variant);
}

// static
bool
nsTextStore::GetIMEOpenState(void)
{
  nsRefPtr<ITfCompartment> comp;
  if (!GetCompartment(sTsfThreadMgr,
                      GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                      getter_AddRefs(comp)))
    return false;

  VARIANT variant;
  ::VariantInit(&variant);
  if (SUCCEEDED(comp->GetValue(&variant)) && variant.vt == VT_I4)
    return variant.lVal != 0;

  ::VariantClear(&variant); // clear up in case variant.vt != VT_I4
  return false;
}

// static
void
nsTextStore::SetInputContext(nsWindowBase* aWidget,
                             const InputContext& aContext,
                             const InputContextAction& aAction)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: nsTextStore::SetInputContext(aWidget=%p, "
          "aContext.mIMEState.mEnabled=%s, aAction.mFocusChange=%s), "
          "ThinksHavingFocus()=%s",
          aWidget, GetIMEEnabledName(aContext.mIMEState.mEnabled),
          GetFocusChangeName(aAction.mFocusChange),
          GetBoolName(ThinksHavingFocus())));

  NS_ENSURE_TRUE_VOID(sTsfTextStore);
  sTsfTextStore->SetInputScope(aContext.mHTMLInputType);

  if (aAction.mFocusChange != InputContextAction::FOCUS_NOT_CHANGED) {
    return;
  }

  // If focus isn't actually changed but the enabled state is changed,
  // emulate the focus move.
  if (!ThinksHavingFocus() &&
      aContext.mIMEState.mEnabled == IMEState::ENABLED) {
    OnFocusChange(true, aWidget, aContext.mIMEState.mEnabled);
  } else if (ThinksHavingFocus() &&
             aContext.mIMEState.mEnabled != IMEState::ENABLED) {
    OnFocusChange(false, aWidget, aContext.mIMEState.mEnabled);
  }
}

// static
void
nsTextStore::MarkContextAsKeyboardDisabled(ITfContext* aContext)
{
  VARIANT variant_int4_value1;
  variant_int4_value1.vt = VT_I4;
  variant_int4_value1.lVal = 1;

  nsRefPtr<ITfCompartment> comp;
  if (!GetCompartment(aContext,
                      GUID_COMPARTMENT_KEYBOARD_DISABLED,
                      getter_AddRefs(comp))) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: nsTextStore::MarkContextAsKeyboardDisabled() failed"
            "aContext=0x%p...", aContext));
    return;
  }

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: nsTextStore::MarkContextAsKeyboardDisabled(), setting "
          "to disable context 0x%p...",
          aContext));
  comp->SetValue(sTsfClientId, &variant_int4_value1);
}

// static
void
nsTextStore::MarkContextAsEmpty(ITfContext* aContext)
{
  VARIANT variant_int4_value1;
  variant_int4_value1.vt = VT_I4;
  variant_int4_value1.lVal = 1;

  nsRefPtr<ITfCompartment> comp;
  if (!GetCompartment(aContext,
                      GUID_COMPARTMENT_EMPTYCONTEXT,
                      getter_AddRefs(comp))) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: nsTextStore::MarkContextAsEmpty() failed"
            "aContext=0x%p...", aContext));
    return;
  }

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: nsTextStore::MarkContextAsEmpty(), setting "
          "to mark empty context 0x%p...", aContext));
  comp->SetValue(sTsfClientId, &variant_int4_value1);
}

// static
void
nsTextStore::Initialize(void)
{
#ifdef PR_LOGGING
  if (!sTextStoreLog) {
    sTextStoreLog = PR_NewLogModule("nsTextStoreWidgets");
  }
#endif

  bool enableTsf = Preferences::GetBool("intl.enable_tsf_support", false);
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: nsTextStore::Initialize(), TSF is %s",
     enableTsf ? "enabled" : "disabled"));
  if (!enableTsf) {
    return;
  }

  if (!sTsfThreadMgr) {
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_ThreadMgr, nullptr,
          CLSCTX_INPROC_SERVER, IID_ITfThreadMgr,
          reinterpret_cast<void**>(&sTsfThreadMgr)))) {
      DebugOnly<HRESULT> hr =
        sTsfThreadMgr->QueryInterface(IID_ITfMessagePump,
                                      reinterpret_cast<void**>(&sMessagePump));
      MOZ_ASSERT(SUCCEEDED(hr));
      MOZ_ASSERT(sMessagePump);
      hr =
        sTsfThreadMgr->QueryInterface(IID_ITfKeystrokeMgr,
                                      reinterpret_cast<void**>(&sKeystrokeMgr));
      MOZ_ASSERT(SUCCEEDED(hr));
      MOZ_ASSERT(sKeystrokeMgr);
      PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
        ("TSF:   nsTextStore::Initialize() succeeded to "
         "create the thread manager, activating..."));
      if (FAILED(sTsfThreadMgr->Activate(&sTsfClientId))) {
        PR_LOG(sTextStoreLog, PR_LOG_ERROR,
          ("TSF:   nsTextStore::Initialize() FAILED to activate, "
           "releasing the thread manager..."));
        NS_RELEASE(sTsfThreadMgr);
      }
    }
#ifdef PR_LOGGING
    else {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF:   nsTextStore::Initialize() FAILED to "
         "create the thread manager"));
    }
#endif // #ifdef PR_LOGGING
  }
  if (sTsfThreadMgr && !sTsfTextStore) {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
      ("TSF:   nsTextStore::Initialize() is creating "
       "an nsTextStore instance..."));
    sTsfTextStore = new nsTextStore();
  }
  if (sTsfThreadMgr && !sDisplayAttrMgr) {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
      ("TSF:   nsTextStore::Initialize() is creating "
       "a display attribute manager instance..."));
    HRESULT hr =
      ::CoCreateInstance(CLSID_TF_DisplayAttributeMgr, nullptr,
                         CLSCTX_INPROC_SERVER, IID_ITfDisplayAttributeMgr,
                         reinterpret_cast<void**>(&sDisplayAttrMgr));
    if (FAILED(hr) || !sDisplayAttrMgr) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF:   nsTextStore::Initialize() FAILED to create "
         "a display attribute manager instance"));
    }
  }
  if (sTsfThreadMgr && sDisplayAttrMgr && !sCategoryMgr) {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
      ("TSF:   nsTextStore::Initialize() is creating "
       "a category manager instance..."));
    HRESULT hr =
      ::CoCreateInstance(CLSID_TF_CategoryMgr, nullptr,
                         CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                         reinterpret_cast<void**>(&sCategoryMgr));
    if (FAILED(hr) || !sCategoryMgr) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF:   nsTextStore::Initialize() FAILED to create "
         "a category manager instance"));
      // release the display manager because it cannot work without the
      // category manager
      NS_RELEASE(sDisplayAttrMgr);
    }
  }

  if (sTsfThreadMgr && sTsfTextStore) {
    HRESULT hr =
      sTsfThreadMgr->CreateDocumentMgr(&sTsfDisabledDocumentMgr);
    if (FAILED(hr) || !sTsfDisabledDocumentMgr) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF:   nsTextStore::Initialize() FAILED to create "
         "a document manager for disabled mode"));
    }
    if (sTsfDisabledDocumentMgr) {
      DWORD editCookie = 0;
      hr = sTsfDisabledDocumentMgr->CreateContext(sTsfClientId, 0, nullptr,
                                                  &sTsfDisabledContext,
                                                  &editCookie);
      if (FAILED(hr) || !sTsfDisabledContext) {
        PR_LOG(sTextStoreLog, PR_LOG_ERROR,
          ("TSF:   nsTextStore::Initialize() FAILED to create "
           "a context for disabled mode"));
      }
      if (sTsfDisabledContext) {
        MarkContextAsKeyboardDisabled(sTsfDisabledContext);
        MarkContextAsEmpty(sTsfDisabledContext);
      }
    }
  }

  if (sTsfThreadMgr && !sFlushTIPInputMessage) {
    sFlushTIPInputMessage = ::RegisterWindowMessageW(
        NS_LITERAL_STRING("Flush TIP Input Message").get());
  }

  if (!sTsfThreadMgr) {
    NS_IF_RELEASE(sMessagePump);
    NS_IF_RELEASE(sKeystrokeMgr);
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF:   nsTextStore::Initialize(), sTsfThreadMgr=0x%p, "
     "sTsfClientId=0x%08X, sTsfTextStore=0x%p, sDisplayAttrMgr=0x%p, "
     "sCategoryMgr=0x%p, sTsfDisabledDocumentMgr=0x%p, sTsfDisabledContext=%p",
     sTsfThreadMgr, sTsfClientId, sTsfTextStore, sDisplayAttrMgr, sCategoryMgr,
     sTsfDisabledDocumentMgr, sTsfDisabledContext));
}

// static
void
nsTextStore::Terminate(void)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS, ("TSF: nsTextStore::Terminate()"));

  NS_IF_RELEASE(sDisplayAttrMgr);
  NS_IF_RELEASE(sCategoryMgr);
  NS_IF_RELEASE(sTsfTextStore);
  NS_IF_RELEASE(sTsfDisabledDocumentMgr);
  NS_IF_RELEASE(sTsfDisabledContext);
  sTsfClientId = 0;
  if (sTsfThreadMgr) {
    sTsfThreadMgr->Deactivate();
    NS_RELEASE(sTsfThreadMgr);
    NS_RELEASE(sMessagePump);
    NS_RELEASE(sKeystrokeMgr);
  }
}

// static
bool
nsTextStore::ProcessRawKeyMessage(const MSG& aMsg)
{
  if (!sKeystrokeMgr) {
    return false; // not in TSF mode
  }

  if (aMsg.message == WM_KEYDOWN) {
    BOOL eaten;
    HRESULT hr = sKeystrokeMgr->TestKeyDown(aMsg.wParam, aMsg.lParam, &eaten);
    if (FAILED(hr) || !eaten) {
      return false;
    }
    hr = sKeystrokeMgr->KeyDown(aMsg.wParam, aMsg.lParam, &eaten);
    return SUCCEEDED(hr) && eaten;
  }
  if (aMsg.message == WM_KEYUP) {
    BOOL eaten;
    HRESULT hr = sKeystrokeMgr->TestKeyUp(aMsg.wParam, aMsg.lParam, &eaten);
    if (FAILED(hr) || !eaten) {
      return false;
    }
    hr = sKeystrokeMgr->KeyUp(aMsg.wParam, aMsg.lParam, &eaten);
    return SUCCEEDED(hr) && eaten;
  }
  return false;
}

/******************************************************************/
/* nsTextStore::Composition                                       */
/******************************************************************/

void
nsTextStore::Composition::Start(ITfCompositionView* aCompositionView,
                                LONG aCompositionStartOffset,
                                const nsAString& aCompositionString)
{
  mView = aCompositionView;
  mString = aCompositionString;
  mStart = aCompositionStartOffset;
}

void
nsTextStore::Composition::End()
{
  mView = nullptr;
  mString.Truncate();
}

void
nsTextStore::Composition::StartLayoutChangeTimer(nsTextStore* aTextStore)
{
  MOZ_ASSERT(!mLayoutChangeTimer);
  mLayoutChangeTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  mLayoutChangeTimer->InitWithFuncCallback(TimerCallback, aTextStore,
    GetLayoutChangeIntervalTime(), nsITimer::TYPE_REPEATING_SLACK);
}

void
nsTextStore::Composition::EnsureLayoutChangeTimerStopped()
{
  if (!mLayoutChangeTimer) {
    return;
  }
  mLayoutChangeTimer->Cancel();
  mLayoutChangeTimer = nullptr;
}

// static
void
nsTextStore::Composition::TimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsTextStore *ts = static_cast<nsTextStore*>(aClosure);
  ts->OnLayoutChange();
}

// static
uint32_t
nsTextStore::Composition::GetLayoutChangeIntervalTime()
{
  static int32_t sTime = -1;
  if (sTime > 0) {
    return static_cast<uint32_t>(sTime);
  }

  sTime = std::max(10,
    Preferences::GetInt("intl.tsf.on_layout_change_interval", 100));
  return static_cast<uint32_t>(sTime);
}

/******************************************************************************
 *  nsTextStore::Content
 *****************************************************************************/

const nsDependentSubstring
nsTextStore::Content::GetSelectedText() const
{
  MOZ_ASSERT(mInitialized);
  return GetSubstring(static_cast<uint32_t>(mSelection.StartOffset()),
                      static_cast<uint32_t>(mSelection.Length()));
}

const nsDependentSubstring
nsTextStore::Content::GetSubstring(uint32_t aStart, uint32_t aLength) const
{
  MOZ_ASSERT(mInitialized);
  return nsDependentSubstring(mText, aStart, aLength);
}

void
nsTextStore::Content::ReplaceSelectedTextWith(const nsAString& aString)
{
  MOZ_ASSERT(mInitialized);
  ReplaceTextWith(mSelection.StartOffset(), mSelection.Length(), aString);
}

inline uint32_t
FirstDifferentCharOffset(const nsAString& aStr1, const nsAString& aStr2)
{
  MOZ_ASSERT(aStr1 != aStr2);
  uint32_t i = 0;
  uint32_t minLength = std::min(aStr1.Length(), aStr2.Length());
  for (; i < minLength && aStr1[i] == aStr2[i]; i++) {
    /* nothing to do */
  }
  return i;
}

void
nsTextStore::Content::ReplaceTextWith(LONG aStart, LONG aLength,
                                      const nsAString& aReplaceString)
{
  MOZ_ASSERT(mInitialized);
  const nsDependentSubstring replacedString =
    GetSubstring(static_cast<uint32_t>(aStart),
                 static_cast<uint32_t>(aLength));
  if (aReplaceString != replacedString) {
    uint32_t firstDifferentOffset =
      static_cast<uint32_t>(aStart) + FirstDifferentCharOffset(aReplaceString,
                                                               replacedString);
    mMinTextModifiedOffset =
      std::min(mMinTextModifiedOffset, firstDifferentOffset);
    if (mComposition.IsComposing()) {
      // Emulate text insertion during compositions, because during a
      // composition, editor expects the whole composition string to
      // be sent in NS_TEXT_TEXT, not just the inserted part.
      // The actual NS_TEXT_TEXT will be sent in SetSelection or
      // OnUpdateComposition.
      MOZ_ASSERT(aStart >= mComposition.mStart);
      MOZ_ASSERT(aStart + aLength <= mComposition.EndOffset());
      mComposition.mString.Replace(
        static_cast<uint32_t>(aStart - mComposition.mStart),
        static_cast<uint32_t>(aLength), aReplaceString);
    }
    mText.Replace(static_cast<uint32_t>(aStart),
                  static_cast<uint32_t>(aLength), aReplaceString);
  }
  // Selection should be collapsed at the end of the inserted string.
  mSelection.CollapseAt(
    static_cast<uint32_t>(aStart) + aReplaceString.Length());
}

void
nsTextStore::Content::StartComposition(ITfCompositionView* aCompositionView,
                                       const PendingAction& aCompStart,
                                       bool aPreserveSelection)
{
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(aCompositionView);
  MOZ_ASSERT(!mComposition.mView);
  MOZ_ASSERT(aCompStart.mType == PendingAction::COMPOSITION_START);

  mComposition.Start(aCompositionView, aCompStart.mSelectionStart,
    GetSubstring(static_cast<uint32_t>(aCompStart.mSelectionStart),
                 static_cast<uint32_t>(aCompStart.mSelectionLength)));
  if (!aPreserveSelection) {
    mSelection.SetSelection(mComposition.mStart, mComposition.mString.Length(),
                            false);
  }
}

void
nsTextStore::Content::EndComposition(const PendingAction& aCompEnd)
{
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(mComposition.mView);
  MOZ_ASSERT(aCompEnd.mType == PendingAction::COMPOSITION_END);

  mSelection.CollapseAt(mComposition.mStart + aCompEnd.mData.Length());
  mComposition.End();
}

#ifdef DEBUG
// static
bool
nsTextStore::CurrentKeyboardLayoutHasIME()
{
  // XXX MSDN documents that ITfInputProcessorProfiles is available only on
  //     desktop apps.  However, there is no known way to obtain
  //     ITfInputProcessorProfileMgr instance without ITfInputProcessorProfiles
  //     instance.
  nsRefPtr<ITfInputProcessorProfiles> profiles;
  HRESULT hr = ::CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_ITfInputProcessorProfiles,
                                  getter_AddRefs(profiles));
  if (FAILED(hr) || !profiles) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: nsTextStore::CurrentKeyboardLayoutHasIME() FAILED to create "
       "an input processor profiles instance"));
    return false;
  }
  nsRefPtr<ITfInputProcessorProfileMgr> profileMgr;
  hr = profiles->QueryInterface(IID_ITfInputProcessorProfileMgr,
                                getter_AddRefs(profileMgr));
  if (FAILED(hr) || !profileMgr) {
    // On Windows Vista or later, ImmIsIME() API always returns true.
    // If we failed to obtain the profile manager, we cannot know if current
    // keyboard layout has IME.
    if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF: nsTextStore::CurrentKeyboardLayoutHasIME() FAILED to query "
         "ITfInputProcessorProfileMgr"));
      return false;
    }
    // If the profiles instance doesn't have ITfInputProcessorProfileMgr
    // interface, that means probably we're running on WinXP or WinServer2003
    // (except WinServer2003 R2).  Then, we should use ImmIsIME().
    return ::ImmIsIME(::GetKeyboardLayout(0));
  }

  TF_INPUTPROCESSORPROFILE profile;
  hr = profileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &profile);
  if (hr == S_FALSE) {
    return false; // not found or not active
  }
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: nsTextStore::CurrentKeyboardLayoutHasIME() FAILED to retreive "
       "active profile"));
    return false;
  }
  return (profile.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR);
}
#endif // #ifdef DEBUG
