/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <olectl.h>
#include <algorithm>

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
#include "mozilla/WindowsVersion.h"

#define INPUTSCOPE_INIT_GUID
#define TEXTATTRS_INIT_GUID
#include "nsTextStore.h"

using namespace mozilla;
using namespace mozilla::widget;

static const char* kPrefNameEnableTSF = "intl.tsf.enable";
static const char* kPrefNameForceEnableTSF = "intl.tsf.force_enable";

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
    description.Append(')');
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
GetGUIDNameStr(REFGUID aGUID)
{
  OLECHAR str[40];
  int len = ::StringFromGUID2(aGUID, str, ArrayLength(str));
  if (!len || !str[0]) {
    return EmptyCString();
  }

  return NS_ConvertUTF16toUTF8(str);
}

static nsCString
GetGUIDNameStrWithTable(REFGUID aGUID)
{
#define RETURN_GUID_NAME(aNamedGUID) \
  if (IsEqualGUID(aGUID, aNamedGUID)) { \
    return NS_LITERAL_CSTRING(#aNamedGUID); \
  }

  RETURN_GUID_NAME(GUID_PROP_INPUTSCOPE)
  RETURN_GUID_NAME(TSATTRID_OTHERS)
  RETURN_GUID_NAME(TSATTRID_Font)
  RETURN_GUID_NAME(TSATTRID_Font_FaceName)
  RETURN_GUID_NAME(TSATTRID_Font_SizePts)
  RETURN_GUID_NAME(TSATTRID_Font_Style)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Bold)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Italic)
  RETURN_GUID_NAME(TSATTRID_Font_Style_SmallCaps)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Capitalize)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Uppercase)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Lowercase)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Animation)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Animation_LasVegasLights)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Animation_BlinkingBackground)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Animation_SparkleText)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Animation_MarchingBlackAnts)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Animation_MarchingRedAnts)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Animation_Shimmer)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Animation_WipeDown)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Animation_WipeRight)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Emboss)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Engrave)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Hidden)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Kerning)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Outlined)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Position)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Protected)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Shadow)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Spacing)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Weight)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Height)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Underline)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Underline_Single)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Underline_Double)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Strikethrough)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Strikethrough_Single)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Strikethrough_Double)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Overline)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Overline_Single)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Overline_Double)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Blink)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Subscript)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Superscript)
  RETURN_GUID_NAME(TSATTRID_Font_Style_Color)
  RETURN_GUID_NAME(TSATTRID_Font_Style_BackgroundColor)
  RETURN_GUID_NAME(TSATTRID_Text)
  RETURN_GUID_NAME(TSATTRID_Text_VerticalWriting)
  RETURN_GUID_NAME(TSATTRID_Text_RightToLeft)
  RETURN_GUID_NAME(TSATTRID_Text_Orientation)
  RETURN_GUID_NAME(TSATTRID_Text_Language)
  RETURN_GUID_NAME(TSATTRID_Text_ReadOnly)
  RETURN_GUID_NAME(TSATTRID_Text_EmbeddedObject)
  RETURN_GUID_NAME(TSATTRID_Text_Alignment)
  RETURN_GUID_NAME(TSATTRID_Text_Alignment_Left)
  RETURN_GUID_NAME(TSATTRID_Text_Alignment_Right)
  RETURN_GUID_NAME(TSATTRID_Text_Alignment_Center)
  RETURN_GUID_NAME(TSATTRID_Text_Alignment_Justify)
  RETURN_GUID_NAME(TSATTRID_Text_Link)
  RETURN_GUID_NAME(TSATTRID_Text_Hyphenation)
  RETURN_GUID_NAME(TSATTRID_Text_Para)
  RETURN_GUID_NAME(TSATTRID_Text_Para_FirstLineIndent)
  RETURN_GUID_NAME(TSATTRID_Text_Para_LeftIndent)
  RETURN_GUID_NAME(TSATTRID_Text_Para_RightIndent)
  RETURN_GUID_NAME(TSATTRID_Text_Para_SpaceAfter)
  RETURN_GUID_NAME(TSATTRID_Text_Para_SpaceBefore)
  RETURN_GUID_NAME(TSATTRID_Text_Para_LineSpacing)
  RETURN_GUID_NAME(TSATTRID_Text_Para_LineSpacing_Single)
  RETURN_GUID_NAME(TSATTRID_Text_Para_LineSpacing_OnePtFive)
  RETURN_GUID_NAME(TSATTRID_Text_Para_LineSpacing_Double)
  RETURN_GUID_NAME(TSATTRID_Text_Para_LineSpacing_AtLeast)
  RETURN_GUID_NAME(TSATTRID_Text_Para_LineSpacing_Exactly)
  RETURN_GUID_NAME(TSATTRID_Text_Para_LineSpacing_Multiple)
  RETURN_GUID_NAME(TSATTRID_List)
  RETURN_GUID_NAME(TSATTRID_List_LevelIndel)
  RETURN_GUID_NAME(TSATTRID_List_Type)
  RETURN_GUID_NAME(TSATTRID_List_Type_Bullet)
  RETURN_GUID_NAME(TSATTRID_List_Type_Arabic)
  RETURN_GUID_NAME(TSATTRID_List_Type_LowerLetter)
  RETURN_GUID_NAME(TSATTRID_List_Type_UpperLetter)
  RETURN_GUID_NAME(TSATTRID_List_Type_LowerRoman)
  RETURN_GUID_NAME(TSATTRID_List_Type_UpperRoman)
  RETURN_GUID_NAME(TSATTRID_App)
  RETURN_GUID_NAME(TSATTRID_App_IncorrectSpelling)
  RETURN_GUID_NAME(TSATTRID_App_IncorrectGrammar)

#undef RETURN_GUID_NAME

  return GetGUIDNameStr(aGUID);
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
  wchar_t buf[256];
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

static const char*
GetEventMessageName(uint32_t aMessage)
{
  switch (aMessage) {
    case NS_MOUSE_BUTTON_DOWN:
      return "NS_MOUSE_BUTTON_DOWN";
    case NS_MOUSE_BUTTON_UP:
      return "NS_MOUSE_BUTTON_UP";
    default:
      return "Unknown";
  }
}

static const char*
GetMouseButtonName(int16_t aButton)
{
  switch (aButton) {
    case WidgetMouseEventBase::eLeftButton:
      return "LeftButton";
    case WidgetMouseEventBase::eMiddleButton:
      return "MiddleButton";
    case WidgetMouseEventBase::eRightButton:
      return "RightButton";
    default:
      return "UnknownButton";
  }
}

#define ADD_SEPARATOR_IF_NECESSARY(aStr) \
  if (!aStr.IsEmpty()) { \
    aStr.AppendLiteral(", "); \
  }

static nsCString
GetMouseButtonsName(int16_t aButtons)
{
  if (!aButtons) {
    return NS_LITERAL_CSTRING("no buttons");
  }
  nsAutoCString names;
  if (aButtons & WidgetMouseEventBase::eLeftButtonFlag) {
    names = "LeftButton";
  }
  if (aButtons & WidgetMouseEventBase::eRightButtonFlag) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += "RightButton";
  }
  if (aButtons & WidgetMouseEventBase::eMiddleButtonFlag) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += "MiddleButton";
  }
  if (aButtons & WidgetMouseEventBase::e4thButtonFlag) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += "4thButton";
  }
  if (aButtons & WidgetMouseEventBase::e5thButtonFlag) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += "5thButton";
  }
  return names;
}

static nsCString
GetModifiersName(Modifiers aModifiers)
{
  if (aModifiers == MODIFIER_NONE) {
    return NS_LITERAL_CSTRING("no modifiers");
  }
  nsAutoCString names;
  if (aModifiers & MODIFIER_ALT) {
    names = NS_DOM_KEYNAME_ALT;
  }
  if (aModifiers & MODIFIER_ALTGRAPH) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += NS_DOM_KEYNAME_ALTGRAPH;
  }
  if (aModifiers & MODIFIER_CAPSLOCK) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += NS_DOM_KEYNAME_CAPSLOCK;
  }
  if (aModifiers & MODIFIER_CONTROL) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += NS_DOM_KEYNAME_CONTROL;
  }
  if (aModifiers & MODIFIER_FN) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += NS_DOM_KEYNAME_FN;
  }
  if (aModifiers & MODIFIER_META) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += NS_DOM_KEYNAME_META;
  }
  if (aModifiers & MODIFIER_NUMLOCK) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += NS_DOM_KEYNAME_NUMLOCK;
  }
  if (aModifiers & MODIFIER_SCROLLLOCK) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += NS_DOM_KEYNAME_SCROLLLOCK;
  }
  if (aModifiers & MODIFIER_SHIFT) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += NS_DOM_KEYNAME_SHIFT;
  }
  if (aModifiers & MODIFIER_SYMBOLLOCK) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += NS_DOM_KEYNAME_SYMBOLLOCK;
  }
  if (aModifiers & MODIFIER_OS) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += NS_DOM_KEYNAME_OS;
  }
  return names;
}

#endif // #ifdef PR_LOGGING

/******************************************************************/
/* InputScopeImpl                                                 */
/******************************************************************/

class InputScopeImpl MOZ_FINAL : public ITfInputScope
{
  ~InputScopeImpl() {}

public:
  InputScopeImpl(const nsTArray<InputScope>& aList)
    : mInputScopes(aList)
  {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
      ("TSF: 0x%p InputScopeImpl()", this));
  }

  NS_INLINE_DECL_IUNKNOWN_REFCOUNTING(InputScopeImpl)

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
  nsTArray<InputScope> mInputScopes;
};

/******************************************************************/
/* TSFStaticSink                                                  */
/******************************************************************/

class TSFStaticSink MOZ_FINAL : public ITfActiveLanguageProfileNotifySink
                              , public ITfInputProcessorProfileActivationSink
{
public:
  static TSFStaticSink* GetInstance()
  {
    if (!sInstance) {
      sInstance = new TSFStaticSink();
    }
    return sInstance;
  }

  static void Shutdown()
  {
    if (sInstance) {
      sInstance->Destroy();
      sInstance = nullptr;
    }
  }

  bool Init(ITfThreadMgr* aThreadMgr,
            ITfInputProcessorProfiles* aInputProcessorProfiles);
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
  {
    *ppv = nullptr;
    if (IID_IUnknown == riid ||
        IID_ITfActiveLanguageProfileNotifySink == riid) {
      *ppv = static_cast<ITfActiveLanguageProfileNotifySink*>(this);
    } else if (IID_ITfInputProcessorProfileActivationSink == riid) {
      *ppv = static_cast<ITfInputProcessorProfileActivationSink*>(this);
    }
    if (*ppv) {
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  NS_INLINE_DECL_IUNKNOWN_REFCOUNTING(TSFStaticSink)

  const nsString& GetActiveTIPKeyboardDescription() const
  {
    return mActiveTIPKeyboardDescription;
  }

  static bool IsIMM_IME()
  {
    if (!sInstance || !sInstance->EnsureInitActiveTIPKeyboard()) {
      return IsIMM_IME(::GetKeyboardLayout(0));
    }
    return sInstance->mIsIMM_IME;
  }

  static bool IsIMM_IME(HKL aHKL)
  {
     return (::ImmGetIMEFileNameW(aHKL, nullptr, 0) > 0);
  }

  bool EnsureInitActiveTIPKeyboard();

public: // ITfActiveLanguageProfileNotifySink
  STDMETHODIMP OnActivated(REFCLSID clsid, REFGUID guidProfile,
                           BOOL fActivated);

public: // ITfInputProcessorProfileActivationSink
  STDMETHODIMP OnActivated(DWORD, LANGID, REFCLSID, REFGUID, REFGUID,
                           HKL, DWORD);

private:
  TSFStaticSink();
  virtual ~TSFStaticSink() {}

  void Destroy();

  void GetTIPDescription(REFCLSID aTextService, LANGID aLangID,
                         REFGUID aProfile, nsAString& aDescription);
  bool IsTIPCategoryKeyboard(REFCLSID aTextService, LANGID aLangID,
                             REFGUID aProfile);

  // Cookie of installing ITfInputProcessorProfileActivationSink
  DWORD mIPProfileCookie;
  // Cookie of installing ITfActiveLanguageProfileNotifySink
  DWORD mLangProfileCookie;

  // True if current IME is implemented with IMM.
  bool mIsIMM_IME;
  // True if OnActivated() is already called
  bool mOnActivatedCalled;

  nsRefPtr<ITfThreadMgr> mThreadMgr;
  nsRefPtr<ITfInputProcessorProfiles> mInputProcessorProfiles;

  // Active TIP keyboard's description.  If active language profile isn't TIP,
  // i.e., IMM-IME or just a keyboard layout, this is empty.
  nsString mActiveTIPKeyboardDescription;

  static StaticRefPtr<TSFStaticSink> sInstance;
};

StaticRefPtr<TSFStaticSink> TSFStaticSink::sInstance;

TSFStaticSink::TSFStaticSink()
  : mIPProfileCookie(TF_INVALID_COOKIE)
  , mLangProfileCookie(TF_INVALID_COOKIE)
  , mIsIMM_IME(false)
  , mOnActivatedCalled(false)
{
}

bool
TSFStaticSink::Init(ITfThreadMgr* aThreadMgr,
                    ITfInputProcessorProfiles* aInputProcessorProfiles)
{
  MOZ_ASSERT(!mThreadMgr && !mInputProcessorProfiles,
             "TSFStaticSink::Init() must be called only once");

  mThreadMgr = aThreadMgr;
  mInputProcessorProfiles = aInputProcessorProfiles;

  nsRefPtr<ITfSource> source;
  HRESULT hr =
    mThreadMgr->QueryInterface(IID_ITfSource, getter_AddRefs(source));
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p TSFStaticSink::Init() FAILED to get ITfSource "
       "instance (0x%08X)", this, hr));
    return false;
  }

  // On Vista or later, Windows let us know activate IME changed only with
  // ITfInputProcessorProfileActivationSink.  However, it's not available on XP.
  // On XP, ITfActiveLanguageProfileNotifySink is available for it.
  // NOTE: Each OnActivated() should be called when TSF becomes available.
  if (IsVistaOrLater()) {
    hr = source->AdviseSink(IID_ITfInputProcessorProfileActivationSink,
                   static_cast<ITfInputProcessorProfileActivationSink*>(this),
                   &mIPProfileCookie);
    if (FAILED(hr) || mIPProfileCookie == TF_INVALID_COOKIE) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF: 0x%p TSFStaticSink::Init() FAILED to install "
         "ITfInputProcessorProfileActivationSink (0x%08X)", this, hr));
      return false;
    }
  } else {
    hr = source->AdviseSink(IID_ITfActiveLanguageProfileNotifySink,
                   static_cast<ITfActiveLanguageProfileNotifySink*>(this),
                   &mLangProfileCookie);
    if (FAILED(hr) || mLangProfileCookie == TF_INVALID_COOKIE) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF: 0x%p TSFStaticSink::Init() FAILED to install "
         "ITfActiveLanguageProfileNotifySink (0x%08X)", this, hr));
      return false;
    }
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p TSFStaticSink::Init(), "
     "mIPProfileCookie=0x%08X, mLangProfileCookie=0x%08X",
     this, mIPProfileCookie, mLangProfileCookie));
  return true;
}

void
TSFStaticSink::Destroy()
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p TSFStaticSink::Shutdown() "
     "mIPProfileCookie=0x%08X, mLangProfileCookie=0x%08X",
     this, mIPProfileCookie, mLangProfileCookie));

  if (mIPProfileCookie != TF_INVALID_COOKIE) {
    nsRefPtr<ITfSource> source;
    HRESULT hr =
      mThreadMgr->QueryInterface(IID_ITfSource, getter_AddRefs(source));
    if (FAILED(hr)) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF: 0x%p   TSFStaticSink::Shutdown() FAILED to get "
         "ITfSource instance (0x%08X)", this, hr));
    } else {
      hr = source->UnadviseSink(mIPProfileCookie);
      if (FAILED(hr)) {
        PR_LOG(sTextStoreLog, PR_LOG_ERROR,
          ("TSF: 0x%p   nsTextStore::Shutdown() FAILED to uninstall "
           "ITfInputProcessorProfileActivationSink (0x%08X)",
           this, hr));
      }
    }
  }

  if (mLangProfileCookie != TF_INVALID_COOKIE) {
    nsRefPtr<ITfSource> source;
    HRESULT hr =
      mThreadMgr->QueryInterface(IID_ITfSource, getter_AddRefs(source));
    if (FAILED(hr)) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF: 0x%p   TSFStaticSink::Shutdown() FAILED to get "
         "ITfSource instance (0x%08X)", this, hr));
    } else {
      hr = source->UnadviseSink(mLangProfileCookie);
      if (FAILED(hr)) {
        PR_LOG(sTextStoreLog, PR_LOG_ERROR,
          ("TSF: 0x%p   TSFStaticSink::Shutdown() FAILED to uninstall "
           "ITfActiveLanguageProfileNotifySink (0x%08X)",
           this, hr));
      }
    }
  }

  mThreadMgr = nullptr;
  mInputProcessorProfiles = nullptr;
}

STDMETHODIMP
TSFStaticSink::OnActivated(REFCLSID clsid, REFGUID guidProfile,
                           BOOL fActivated)
{
  // NOTE: This is installed only on XP or Server 2003.
  if (fActivated) {
    // TODO: We should check if the profile's category is keyboard or not.
    mOnActivatedCalled = true;
    mIsIMM_IME = IsIMM_IME(::GetKeyboardLayout(0));

    LANGID langID;
    HRESULT hr = mInputProcessorProfiles->GetCurrentLanguage(&langID);
    if (FAILED(hr)) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: TSFStaticSink::OnActivated() FAILED due to "
              "GetCurrentLanguage() failure, hr=0x%08X", hr));
    } else if (IsTIPCategoryKeyboard(clsid, langID, guidProfile)) {
      GetTIPDescription(clsid, langID, guidProfile,
                        mActiveTIPKeyboardDescription);
    } else if (clsid == CLSID_NULL || guidProfile == GUID_NULL) {
      // Perhaps, this case is that keyboard layout without TIP is activated.
      mActiveTIPKeyboardDescription.Truncate();
    }
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p TSFStaticSink::OnActivated(rclsid=%s, guidProfile=%s, "
          "fActivated=%s), mIsIMM_IME=%s, mActiveTIPDescription=\"%s\"",
          this, GetCLSIDNameStr(clsid).get(),
          GetGUIDNameStr(guidProfile).get(), GetBoolName(fActivated),
          GetBoolName(mIsIMM_IME),
          NS_ConvertUTF16toUTF8(mActiveTIPKeyboardDescription).get()));
  return S_OK;
}

STDMETHODIMP
TSFStaticSink::OnActivated(DWORD dwProfileType,
                           LANGID langid,
                           REFCLSID rclsid,
                           REFGUID catid,
                           REFGUID guidProfile,
                           HKL hkl,
                           DWORD dwFlags)
{
  // NOTE: This is installed only on Vista or later.  However, this may be
  //       called by EnsureInitActiveLanguageProfile() even on XP or Server
  //       2003.
  if ((dwFlags & TF_IPSINK_FLAG_ACTIVE) &&
      (dwProfileType == TF_PROFILETYPE_KEYBOARDLAYOUT ||
       catid == GUID_TFCAT_TIP_KEYBOARD)) {
    mOnActivatedCalled = true;
    mIsIMM_IME = IsIMM_IME(hkl);
    GetTIPDescription(rclsid, langid, guidProfile,
                      mActiveTIPKeyboardDescription);
  }
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p TSFStaticSink::OnActivated(dwProfileType=%s (0x%08X), "
          "langid=0x%08X, rclsid=%s, catid=%s, guidProfile=%s, hkl=0x%08X, "
          "dwFlags=0x%08X (TF_IPSINK_FLAG_ACTIVE: %s)), mIsIMM_IME=%s, "
          "mActiveTIPDescription=\"%s\"",
          this, dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR ?
                  "TF_PROFILETYPE_INPUTPROCESSOR" :
                dwProfileType == TF_PROFILETYPE_KEYBOARDLAYOUT ?
                  "TF_PROFILETYPE_KEYBOARDLAYOUT" : "Unknown", dwProfileType,
          langid, GetCLSIDNameStr(rclsid).get(), GetGUIDNameStr(catid).get(),
          GetGUIDNameStr(guidProfile).get(), hkl, dwFlags,
          GetBoolName(dwFlags & TF_IPSINK_FLAG_ACTIVE),
          GetBoolName(mIsIMM_IME),
          NS_ConvertUTF16toUTF8(mActiveTIPKeyboardDescription).get()));
  return S_OK;
}

bool
TSFStaticSink::EnsureInitActiveTIPKeyboard()
{
  if (mOnActivatedCalled) {
    return true;
  }

  if (IsVistaOrLater()) {
    nsRefPtr<ITfInputProcessorProfileMgr> profileMgr;
    HRESULT hr =
      mInputProcessorProfiles->QueryInterface(IID_ITfInputProcessorProfileMgr,
                                              getter_AddRefs(profileMgr));
    if (FAILED(hr) || !profileMgr) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF: 0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
         "to get input processor profile manager, hr=0x%08X", this, hr));
      return false;
    }

    TF_INPUTPROCESSORPROFILE profile;
    hr = profileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &profile);
    if (hr == S_FALSE) {
      PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
        ("TSF: 0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
         "to get active keyboard layout profile due to no active profile, "
         "hr=0x%08X", this, hr));
      // XXX Should we call OnActivated() with arguments like non-TIP in this
      //     case?
      return false;
    }
    if (FAILED(hr)) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF: 0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
         "to get active TIP keyboard, hr=0x%08X", this, hr));
      return false;
    }

    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
      ("TSF: 0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), "
       "calling OnActivated() manually...", this));
    OnActivated(profile.dwProfileType, profile.langid, profile.clsid,
                profile.catid, profile.guidProfile, ::GetKeyboardLayout(0),
                TF_IPSINK_FLAG_ACTIVE);
    return true;
  }

  LANGID langID;
  HRESULT hr = mInputProcessorProfiles->GetCurrentLanguage(&langID);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
       "to get current language ID, hr=0x%08X", this, hr));
    return false;
  }

  nsRefPtr<IEnumTfLanguageProfiles> enumLangProfiles;
  hr = mInputProcessorProfiles->EnumLanguageProfiles(langID,
                                  getter_AddRefs(enumLangProfiles));
  if (FAILED(hr) || !enumLangProfiles) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
       "to get language profiles enumerator, hr=0x%08X", this, hr));
    return false;
  }

  TF_LANGUAGEPROFILE profile;
  ULONG fetch = 0;
  while (SUCCEEDED(enumLangProfiles->Next(1, &profile, &fetch)) && fetch) {
    if (!profile.fActive || profile.catid != GUID_TFCAT_TIP_KEYBOARD) {
      continue;
    }
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
      ("TSF: 0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), "
       "calling OnActivated() manually...", this));
    bool isTIP = profile.guidProfile != GUID_NULL;
    OnActivated(isTIP ? TF_PROFILETYPE_INPUTPROCESSOR :
                        TF_PROFILETYPE_KEYBOARDLAYOUT,
                profile.langid, profile.clsid, profile.catid,
                profile.guidProfile, ::GetKeyboardLayout(0),
                TF_IPSINK_FLAG_ACTIVE);
    return true;
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), "
     "calling OnActivated() without active TIP manually...", this));
  OnActivated(TF_PROFILETYPE_KEYBOARDLAYOUT,
              langID, CLSID_NULL, GUID_TFCAT_TIP_KEYBOARD,
              GUID_NULL, ::GetKeyboardLayout(0),
              TF_IPSINK_FLAG_ACTIVE);
  return true;
}

void
TSFStaticSink::GetTIPDescription(REFCLSID aTextService, LANGID aLangID,
                                 REFGUID aProfile, nsAString& aDescription)
{
  aDescription.Truncate();

  if (aTextService == CLSID_NULL || aProfile == GUID_NULL) {
    return;
  }

  BSTR description = nullptr;
  HRESULT hr =
    mInputProcessorProfiles->GetLanguageProfileDescription(aTextService,
                                                           aLangID,
                                                           aProfile,
                                                           &description);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   TSFStaticSink::InitActiveTIPDescription() FAILED "
            "due to GetLanguageProfileDescription() failure, hr=0x%08X",
            this, hr));
    return;
  }

  if (description && description[0]) {
    aDescription.Assign(description);
  }
  ::SysFreeString(description);
}

bool
TSFStaticSink::IsTIPCategoryKeyboard(REFCLSID aTextService, LANGID aLangID,
                                     REFGUID aProfile)
{
  if (aTextService == CLSID_NULL || aProfile == GUID_NULL) {
    return false;
  }

  nsRefPtr<IEnumTfLanguageProfiles> enumLangProfiles;
  HRESULT hr =
    mInputProcessorProfiles->EnumLanguageProfiles(aLangID,
                               getter_AddRefs(enumLangProfiles));
  if (FAILED(hr) || !enumLangProfiles) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   TSFStaticSink::IsTIPCategoryKeyboard(), FAILED "
       "to get language profiles enumerator, hr=0x%08X", this, hr));
    return false;
  }

  TF_LANGUAGEPROFILE profile;
  ULONG fetch = 0;
  while (SUCCEEDED(enumLangProfiles->Next(1, &profile, &fetch)) && fetch) {
    // XXX We're not sure a profile is registered with two or more categories.
    if (profile.clsid == aTextService &&
        profile.guidProfile == aProfile &&
        profile.catid == GUID_TFCAT_TIP_KEYBOARD) {
      return true;
    }
  }
  return false;
}

/******************************************************************/
/* nsTextStore                                                    */
/******************************************************************/

StaticRefPtr<ITfThreadMgr> nsTextStore::sThreadMgr;
StaticRefPtr<ITfMessagePump> nsTextStore::sMessagePump;
StaticRefPtr<ITfKeystrokeMgr> nsTextStore::sKeystrokeMgr;
StaticRefPtr<ITfDisplayAttributeMgr> nsTextStore::sDisplayAttrMgr;
StaticRefPtr<ITfCategoryMgr> nsTextStore::sCategoryMgr;
StaticRefPtr<ITfDocumentMgr> nsTextStore::sDisabledDocumentMgr;
StaticRefPtr<ITfContext> nsTextStore::sDisabledContext;
StaticRefPtr<ITfInputProcessorProfiles> nsTextStore::sInputProcessorProfiles;
StaticRefPtr<nsTextStore> nsTextStore::sEnabledTextStore;
DWORD nsTextStore::sClientId  = 0;

bool nsTextStore::sCreateNativeCaretForATOK = false;
bool nsTextStore::sDoNotReturnNoLayoutErrorToFreeChangJie = false;
bool nsTextStore::sDoNotReturnNoLayoutErrorToEasyChangjei = false;

#define TIP_NAME_BEGINS_WITH_ATOK \
  (NS_LITERAL_STRING("ATOK "))
// NOTE: Free ChangJie 2010 missspells its name...
#define TIP_NAME_FREE_CHANG_JIE_2010 \
  (NS_LITERAL_STRING("Free CangJie IME 10"))
#define TIP_NAME_EASY_CHANGJEI \
  (NS_LITERAL_STRING( \
     "\x4E2D\x6587 (\x7E41\x9AD4) - \x6613\x9821\x8F38\x5165\x6CD5"))

#define TEXTSTORE_DEFAULT_VIEW (1)

nsTextStore::nsTextStore()
  : mLockedContent(mComposition, mSelection)
  , mEditCookie(0)
  , mSinkMask(0)
  , mLock(0)
  , mLockQueued(0)
  , mRequestedAttrValues(false)
  , mIsRecordingActionsWithoutLock(false)
  , mPendingOnSelectionChange(false)
  , mPendingOnLayoutChange(false)
  , mPendingDestroy(false)
  , mNativeCaretIsCreated(false)
{
  for (int32_t i = 0; i < NUM_OF_SUPPORTED_ATTRS; i++) {
    mRequestedAttrs[i] = false;
  }

  // We hope that 5 or more actions don't occur at once.
  mPendingActions.SetCapacity(5);

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p nsTextStore::nsTestStore() SUCCEEDED", this));
}

nsTextStore::~nsTextStore()
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p nsTextStore instance is destroyed", this));
}

bool
nsTextStore::Init(nsWindowBase* aWidget)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p nsTextStore::Init(aWidget=0x%p)",
     this, aWidget));

  TSFStaticSink::GetInstance()->EnsureInitActiveTIPKeyboard();

  if (mDocumentMgr) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::Init() FAILED due to already initialized",
       this));
    return false;
  }

  // Create document manager
  HRESULT hr = sThreadMgr->CreateDocumentMgr(getter_AddRefs(mDocumentMgr));
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::Init() FAILED to create DocumentMgr "
       "(0x%08X)", this, hr));
    return false;
  }
  mWidget = aWidget;

  // Create context and add it to document manager
  hr = mDocumentMgr->CreateContext(sClientId, 0,
                                   static_cast<ITextStoreACP*>(this),
                                   getter_AddRefs(mContext), &mEditCookie);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::Init() FAILED to create the context "
       "(0x%08X)", this, hr));
    mDocumentMgr = nullptr;
    return false;
  }

  hr = mDocumentMgr->Push(mContext);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: 0x%p   nsTextStore::Init() FAILED to push the context (0x%08X)",
       this, hr));
    // XXX Why don't we use NS_IF_RELEASE() here??
    mContext = nullptr;
    mDocumentMgr = nullptr;
    return false;
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p   nsTextStore::Init() succeeded: "
     "mDocumentMgr=0x%p, mContext=0x%p, mEditCookie=0x%08X",
     this, mDocumentMgr.get(), mContext.get(), mEditCookie));

  return true;
}

bool
nsTextStore::Destroy()
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: 0x%p nsTextStore::Destroy(), mLock=%s, "
     "mComposition.IsComposing()=%s",
     this, GetLockFlagNameStr(mLock).get(),
     GetBoolName(mComposition.IsComposing())));

  if (mLock) {
    mPendingDestroy = true;
    return true;
  }

  // If there is composition, TSF keeps the composition even after the text
  // store destroyed.  So, we should clear the composition here.
  if (mComposition.IsComposing()) {
    NS_WARNING("Composition is still alive at destroying the text store");
    CommitCompositionInternal(false);
  }

  mLockedContent.Clear();
  mSelection.MarkDirty();

  mContext = nullptr;
  if (mDocumentMgr) {
    mDocumentMgr->Pop(TF_POPF_ALL);
    mDocumentMgr = nullptr;
  }
  mSink = nullptr;
  mWidget = nullptr;

  if (!mMouseTrackers.IsEmpty()) {
    PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
      ("TSF: 0x%p   nsTextStore::Destroy(), removing a mouse tracker...",
       this));
    mMouseTrackers.Clear();
  }

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
  } else if (IID_ITfMouseTrackerACP == riid) {
    *ppv = static_cast<ITfMouseTrackerACP*>(this);
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
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
      ("TSF: 0x%p   Locking (%s) >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
       ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>",
       this, GetLockFlagNameStr(mLock).get()));
    // Don't release this instance during this lock because this is called by
    // TSF but they don't grab us during this call.
    nsRefPtr<nsTextStore> kungFuDeathGrip(this);
    *phrSession = mSink->OnLockGranted(mLock);
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
      ("TSF: 0x%p   Unlocked (%s) <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
       "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<",
       this, GetLockFlagNameStr(mLock).get()));
    DidLockGranted();
    while (mLockQueued) {
      mLock = mLockQueued;
      mLockQueued = 0;
      PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
        ("TSF: 0x%p   Locking for the request in the queue (%s) >>>>>>>>>>>>>>"
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>",
         this, GetLockFlagNameStr(mLock).get()));
      mSink->OnLockGranted(mLock);
      PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
        ("TSF: 0x%p   Unlocked (%s) <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
         "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<",
         this, GetLockFlagNameStr(mLock).get()));
      DidLockGranted();
    }

    // The document is now completely unlocked.
    mLock = 0;

    if (!mPendingDestroy && mPendingOnLayoutChange) {
      mPendingOnLayoutChange = false;
      if (mSink) {
        PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
               ("TSF: 0x%p   nsTextStore::RequestLock(), "
                "calling ITextStoreACPSink::OnLayoutChange()...", this));
        mSink->OnLayoutChange(TS_LC_CHANGE, TEXTSTORE_DEFAULT_VIEW);
      }
      // The layout change caused by composition string change should cause
      // calling ITfContextOwnerServices::OnLayoutChange() too.
      if (mContext) {
        nsRefPtr<ITfContextOwnerServices> service;
        mContext->QueryInterface(IID_ITfContextOwnerServices,
                                 getter_AddRefs(service));
        if (service) {
          PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
                 ("TSF: 0x%p   nsTextStore::RequestLock(), "
                  "calling ITfContextOwnerServices::OnLayoutChange()...",
                  this));
          service->OnLayoutChange();
        }
      }
    }

    if (!mPendingDestroy && mPendingOnSelectionChange) {
      mPendingOnSelectionChange = false;
      if (mSink) {
        PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
               ("TSF: 0x%p   nsTextStore::RequestLock(), "
                "calling ITextStoreACPSink::OnSelectionChange()...", this));
        mSink->OnSelectionChange();
      }
    }

    if (mPendingDestroy) {
      Destroy();
    }

    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
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
nsTextStore::DidLockGranted()
{
  if (mNativeCaretIsCreated) {
    ::DestroyCaret();
    mNativeCaretIsCreated = false;
  }
  if (IsReadWriteLocked()) {
    // FreeCJ (TIP for Traditional Chinese) calls SetSelection() to set caret
    // to the start of composition string and insert a full width space for
    // a placeholder with a call of SetText().  After that, it calls
    // OnUpdateComposition() without new range.  Therefore, let's record the
    // composition update information here.
    CompleteLastActionIfStillIncomplete();

    FlushPendingActions();
  }

  // If the widget has gone, we don't need to notify anything.
  if (!mWidget || mWidget->Destroyed()) {
    mPendingOnSelectionChange = false;
    mPendingOnLayoutChange = false;
  }
}

void
nsTextStore::FlushPendingActions()
{
  if (!mWidget || mWidget->Destroyed()) {
    mPendingActions.Clear();
    mLockedContent.Clear();
    mPendingOnSelectionChange = false;
    mPendingOnLayoutChange = false;
    return;
  }

  mLockedContent.Clear();

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

        if (action.mAdjustSelection) {
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
        break;
      }
      case PendingAction::COMPOSITION_UPDATE: {
        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions() "
                "flushing COMPOSITION_UPDATE={ mData=\"%s\", "
                "mRanges=0x%p, mRanges->Length()=%d }",
                this, NS_ConvertUTF16toUTF8(action.mData).get(), action.mRanges.get(),
                action.mRanges ? action.mRanges->Length() : 0));

        if (!action.mRanges) {
          NS_WARNING("How does this case occur?");
          action.mRanges = new TextRangeArray();
        }

        // Adjust offsets in the ranges for XP linefeed character (only \n).
        // XXX Following code is the safest approach.  However, it wastes
        //     a little performance.  For ensuring the clauses do not
        //     overlap each other, we should redesign TextRange later.
        for (uint32_t i = 0; i < action.mRanges->Length(); ++i) {
          TextRange& range = action.mRanges->ElementAt(i);
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

        action.mData.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                                      NS_LITERAL_STRING("\n"));

        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions(), "
                "dispatching text event...", this));
        WidgetTextEvent textEvent(true, NS_TEXT_TEXT, mWidget);
        mWidget->InitEvent(textEvent);
        textEvent.theText = action.mData;
        if (action.mRanges->IsEmpty()) {
          TextRange wholeRange;
          wholeRange.mStartOffset = 0;
          wholeRange.mEndOffset = textEvent.theText.Length();
          wholeRange.mRangeType = NS_TEXTRANGE_RAWINPUT;
          action.mRanges->AppendElement(wholeRange);
        }
        textEvent.mRanges = action.mRanges;
        mWidget->DispatchWindowEvent(&textEvent);
        // Be aware, the mWidget might already have been destroyed.
        break;
      }
      case PendingAction::COMPOSITION_END: {
        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions() "
                "flushing COMPOSITION_END={ mData=\"%s\" }",
                this, NS_ConvertUTF16toUTF8(action.mData).get()));

        action.mData.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                                      NS_LITERAL_STRING("\n"));

        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions(), "
                "dispatching text event...", this));
        WidgetTextEvent textEvent(true, NS_TEXT_TEXT, mWidget);
        mWidget->InitEvent(textEvent);
        textEvent.theText = action.mData;
        mWidget->DispatchWindowEvent(&textEvent);
        if (!mWidget || mWidget->Destroyed()) {
          break;
        }

        PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
               ("TSF: 0x%p   nsTextStore::FlushPendingActions(), "
                "dispatching compositionend event...", this));
        WidgetCompositionEvent compositionEnd(true, NS_COMPOSITION_END,
                                              mWidget);
        compositionEnd.data = textEvent.theText;
        mWidget->InitEvent(compositionEnd);
        mWidget->DispatchWindowEvent(&compositionEnd);
        if (!mWidget || mWidget->Destroyed()) {
          break;
        }
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

    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::FlushPendingActions(), "
            "qutting since the mWidget has gone", this));
    break;
  }
  mPendingActions.Clear();
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
nsTextStore::LockedContent()
{
  MOZ_ASSERT(IsReadLocked(),
             "LockedContent must be called only during the document is locked");
  if (!IsReadLocked()) {
    mLockedContent.Clear();
    return mLockedContent;
  }

  Selection& currentSel = CurrentSelection();
  if (currentSel.IsDirty()) {
    mLockedContent.Clear();
    return mLockedContent;
  }

  if (!mLockedContent.IsInitialized()) {
    nsAutoString text;
    if (NS_WARN_IF(!GetCurrentText(text))) {
      mLockedContent.Clear();
      return mLockedContent;
    }

    mLockedContent.Init(text);
  }

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::LockedContent(): "
          "mLockedContent={ mText.Length()=%d }",
          this, mLockedContent.Text().Length()));

  return mLockedContent;
}

bool
nsTextStore::GetCurrentText(nsAString& aTextContent)
{
  if (mLockedContent.IsInitialized()) {
    aTextContent = mLockedContent.Text();
    return true;
  }

  MOZ_ASSERT(mWidget && !mWidget->Destroyed());

  WidgetQueryContentEvent queryText(true, NS_QUERY_TEXT_CONTENT, mWidget);
  queryText.InitForQueryTextContent(0, UINT32_MAX);
  mWidget->InitEvent(queryText);
  mWidget->DispatchWindowEvent(&queryText);
  if (NS_WARN_IF(!queryText.mSucceeded)) {
    aTextContent.Truncate();
    return false;
  }

  aTextContent = queryText.mReply.mString;
  return true;
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
    hr = RestartComposition(pComposition, composingRange);
    if (FAILED(hr)) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::RestartCompositionIfNecessary() FAILED "
              "due to RestartComposition() failure", this));
      return hr;
    }
  }

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::RestartCompositionIfNecessary() succeeded",
          this));
  return S_OK;
}

HRESULT
nsTextStore::RestartComposition(ITfCompositionView* aCompositionView,
                                ITfRange* aNewRange)
{
  Selection& currentSelection = CurrentSelection();

  LONG newStart, newLength;
  HRESULT hr = GetRangeExtent(aNewRange, &newStart, &newLength);
  LONG newEnd = newStart + newLength;

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::RestartComposition(aCompositionView=0x%p, "
          "aNewRange=0x%p { newStart=%d, newLength=%d }), "
          "mComposition={ mStart=%d, mCompositionString.Length()=%d }, "
          "currentSelection={ IsDirty()=%s, StartOffset()=%d, Length()=%d }",
          this, aCompositionView, aNewRange, newStart, newLength,
          mComposition.mStart, mComposition.mString.Length(),
          GetBoolName(currentSelection.IsDirty()),
          currentSelection.StartOffset(), currentSelection.Length()));

  if (currentSelection.IsDirty()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RestartComposition() FAILED "
            "due to CurrentSelection() failure", this));
    return E_FAIL;
  }

  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RestartComposition() FAILED "
            "due to GetRangeExtent() failure", this));
    return hr;
  }

  // If the new range has no overlap with the crrent range, we just commit
  // the composition and restart new composition with the new range but
  // current selection range should be preserved.
  if (newStart >= mComposition.EndOffset() || newEnd <= mComposition.mStart) {
    RecordCompositionEndAction();
    RecordCompositionStartAction(aCompositionView, newStart, newLength, true);
    return S_OK;
  }

  // If the new range has an overlap with the current one, we should not commit
  // the whole current range to avoid creating an odd undo transaction.
  // I.e., the overlapped range which is being composed should not appear in
  // undo transaction.

  // Backup current composition data and selection data.
  Composition oldComposition = mComposition;
  Selection oldSelection = currentSelection;

  // Commit only the part of composition.
  LONG keepComposingStartOffset = std::max(mComposition.mStart, newStart);
  LONG keepComposingEndOffset = std::min(mComposition.EndOffset(), newEnd);
  MOZ_ASSERT(keepComposingStartOffset <= keepComposingEndOffset,
    "Why keepComposingEndOffset is smaller than keepComposingStartOffset?");
  LONG keepComposingLength = keepComposingEndOffset - keepComposingStartOffset;
  // Remove the overlapped part from the commit string.
  nsAutoString commitString(mComposition.mString);
  commitString.Cut(keepComposingStartOffset - mComposition.mStart,
                   keepComposingLength);
  // Update the composition string.
  Content& lockedContent = LockedContent();
  if (!lockedContent.IsInitialized()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RestartComposition() FAILED "
            "due to LockedContent() failure", this));
    return E_FAIL;
  }
  lockedContent.ReplaceTextWith(mComposition.mStart,
                                mComposition.mString.Length(),
                                commitString);
  // Record a compositionupdate action for commit the part of composing string.
  PendingAction* action = LastOrNewPendingCompositionUpdate();
  action->mData = mComposition.mString;
  action->mRanges->Clear();
  TextRange caretRange;
  caretRange.mStartOffset = caretRange.mEndOffset =
    uint32_t(oldComposition.mStart + commitString.Length());
  caretRange.mRangeType = NS_TEXTRANGE_CARETPOSITION;
  action->mRanges->AppendElement(caretRange);
  action->mIncomplete = false;

  // Record compositionend action.
  RecordCompositionEndAction();

  // Record compositionstart action only with the new start since this method
  // hasn't restored composing string yet.
  RecordCompositionStartAction(aCompositionView, newStart, 0, false);

  // Restore the latest text content and selection.
  lockedContent.ReplaceSelectedTextWith(
    nsDependentSubstring(oldComposition.mString,
                         keepComposingStartOffset - oldComposition.mStart,
                         keepComposingLength));
  currentSelection = oldSelection;

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::RestartComposition() succeeded, "
          "mComposition={ mStart=%d, mCompositionString.Length()=%d }, "
          "currentSelection={ IsDirty()=%s, StartOffset()=%d, Length()=%d }",
          this, mComposition.mStart, mComposition.mString.Length(),
          GetBoolName(currentSelection.IsDirty()),
          currentSelection.StartOffset(), currentSelection.Length()));

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
          "mComposition={ mView=0x%p, mStart=%d, mString=\"%s\" "
          "(Length()=%d) }",
          this, mComposition.mView.get(), mComposition.mStart,
          NS_ConvertUTF16toUTF8(mComposition.mString).get(),
          mComposition.mString.Length()));

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

  PendingAction* action = LastOrNewPendingCompositionUpdate();
  action->mData = mComposition.mString;
  // The ranges might already have been initialized, however, if this is
  // called again, that means we need to overwrite the ranges with current
  // information.
  action->mRanges->Clear();

  TextRange newRange;
  // No matter if we have display attribute info or not,
  // we always pass in at least one range to NS_TEXT_TEXT
  newRange.mStartOffset = 0;
  newRange.mEndOffset = action->mData.Length();
  newRange.mRangeType = NS_TEXTRANGE_RAWINPUT;
  action->mRanges->AppendElement(newRange);

  nsRefPtr<ITfRange> range;
  while (S_OK == enumRanges->Next(1, getter_AddRefs(range), nullptr) && range) {

    LONG rangeStart = 0, rangeLength = 0;
    if (FAILED(GetRangeExtent(range, &rangeStart, &rangeLength))) {
      continue;
    }
    // The range may include out of composition string.  We should ignore
    // outside of the composition string.
    LONG start = std::min(std::max(rangeStart, mComposition.mStart),
                          mComposition.EndOffset());
    LONG end = std::max(std::min(rangeStart + rangeLength,
                                 mComposition.EndOffset()),
                        mComposition.mStart);
    LONG length = end - start;
    if (length < 0) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::RecordCompositionUpdateAction() "
              "ignores invalid range (%d-%d)",
              this, rangeStart - mComposition.mStart,
              rangeStart - mComposition.mStart + rangeLength));
      continue;
    }
    if (!length) {
      PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
             ("TSF: 0x%p   nsTextStore::RecordCompositionUpdateAction() "
              "ignores a range due to outside of the composition or empty "
              "(%d-%d)",
              this, rangeStart - mComposition.mStart,
              rangeStart - mComposition.mStart + rangeLength));
      continue;
    }

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

    TextRange& lastRange = action->mRanges->LastElement();
    if (lastRange.mStartOffset == newRange.mStartOffset) {
      // Replace range if last range is the same as this one
      // So that ranges don't overlap and confuse the editor
      lastRange = newRange;
    } else {
      lastRange.mEndOffset = newRange.mStartOffset;
      action->mRanges->AppendElement(newRange);
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
  if (!currentSel.IsCollapsed() && action->mRanges->Length() == 1) {
    TextRange& range = action->mRanges->ElementAt(0);
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
  action->mRanges->AppendElement(caretRange);

  action->mIncomplete = false;

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
         ("TSF: 0x%p   nsTextStore::SetSelectionInternal(pSelection={ "
          "acpStart=%ld, acpEnd=%ld, style={ ase=%s, fInterimChar=%s} }, "
          "aDispatchTextEvent=%s), mComposition.IsComposing()=%s",
          this, pSelection->acpStart, pSelection->acpEnd,
          GetActiveSelEndName(pSelection->style.ase),
          GetBoolName(pSelection->style.fInterimChar),
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

  CompleteLastActionIfStillIncomplete();
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
         ("TSF: 0x%p nsTextStore::SetSelection(ulCount=%lu, pSelection=%p { "
          "acpStart=%ld, acpEnd=%ld, style={ ase=%s, fInterimChar=%s } }), "
          "mComposition.IsComposing()=%s",
          this, ulCount, pSelection,
          pSelection ? pSelection->acpStart : 0,
          pSelection ? pSelection->acpEnd : 0,
          pSelection ? GetActiveSelEndName(pSelection->style.ase) : "",
          pSelection ? GetBoolName(pSelection->style.fInterimChar) : "",
          GetBoolName(mComposition.IsComposing())));

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
     "mString.Length()=%lu, IsComposing()=%s }",
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

  // Making sure to null-terminate string just to be on the safe side
  *pcchPlainOut = 0;
  if (pchPlain && cchPlainReq) *pchPlain = 0;
  if (pulRunInfoOut) *pulRunInfoOut = 0;
  if (pacpNext) *pacpNext = acpStart;
  if (prgRunInfo && ulRunInfoReq) {
    prgRunInfo->uCount = 0;
    prgRunInfo->type = TS_RT_PLAIN;
  }

  Content& lockedContent = LockedContent();
  if (!lockedContent.IsInitialized()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetText() FAILED due to "
            "LockedContent() failure", this));
    return E_FAIL;
  }
  if (lockedContent.Text().Length() < static_cast<uint32_t>(acpStart)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetText() FAILED due to "
            "acpStart is larger offset than the actual text length", this));
    return TS_E_INVALIDPOS;
  }
  if (acpEnd != -1 &&
      lockedContent.Text().Length() < static_cast<uint32_t>(acpEnd)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetText() FAILED due to "
            "acpEnd is larger offset than the actual text length", this));
    return TS_E_INVALIDPOS;
  }
  uint32_t length = (acpEnd == -1) ?
    lockedContent.Text().Length() - static_cast<uint32_t>(acpStart) :
    static_cast<uint32_t>(acpEnd - acpStart);
  if (cchPlainReq && cchPlainReq - 1 < length) {
    length = cchPlainReq - 1;
  }
  if (length) {
    if (pchPlain && cchPlainReq) {
      const char16_t* startChar =
        lockedContent.Text().BeginReading() + acpStart;
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
         ("TSF: 0x%p nsTextStore::SetText(dwFlags=%s, acpStart=%ld, "
          "acpEnd=%ld, pchText=0x%p \"%s\", cch=%lu, pChange=0x%p), "
          "mComposition.IsComposing()=%s",
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

int32_t
nsTextStore::GetRequestedAttrIndex(const TS_ATTRID& aAttrID)
{
  if (IsEqualGUID(aAttrID, GUID_PROP_INPUTSCOPE)) {
    return eInputScope;
  }
  if (IsEqualGUID(aAttrID, TSATTRID_Text_VerticalWriting)) {
    return eTextVerticalWriting;
  }
  return eNotSupported;
}

TS_ATTRID
nsTextStore::GetAttrID(int32_t aIndex)
{
  switch (aIndex) {
    case eInputScope:
      return GUID_PROP_INPUTSCOPE;
    case eTextVerticalWriting:
      return TSATTRID_Text_VerticalWriting;
    default:
      MOZ_CRASH("Invalid index? Or not implemented yet?");
      return GUID_NULL;
  }
}

HRESULT
nsTextStore::HandleRequestAttrs(DWORD aFlags,
                                ULONG aFilterCount,
                                const TS_ATTRID* aFilterAttrs)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::HandleRequestAttrs(aFlags=%s, "
          "aFilterCount=%u)",
          this, GetFindFlagName(aFlags).get(), aFilterCount));

  // This is a little weird! RequestSupportedAttrs gives us advanced notice
  // of a support query via RetrieveRequestedAttrs for a specific attribute.
  // RetrieveRequestedAttrs needs to return valid data for all attributes we
  // support, but the text service will only want the input scope object
  // returned in RetrieveRequestedAttrs if the dwFlags passed in here contains
  // TS_ATTR_FIND_WANT_VALUE.
  for (int32_t i = 0; i < NUM_OF_SUPPORTED_ATTRS; i++) {
    mRequestedAttrs[i] = false;
  }
  mRequestedAttrValues = !!(aFlags & TS_ATTR_FIND_WANT_VALUE);

  for (uint32_t i = 0; i < aFilterCount; i++) {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::HandleRequestAttrs(), "
            "requested attr=%s",
            this, GetGUIDNameStrWithTable(aFilterAttrs[i]).get()));
    int32_t index = GetRequestedAttrIndex(aFilterAttrs[i]);
    if (index != eNotSupported) {
      mRequestedAttrs[index] = true;
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
         ("TSF: 0x%p nsTextStore::RequestSupportedAttrs(dwFlags=%s, "
          "cFilterAttrs=%lu)",
          this, GetFindFlagName(dwFlags).get(), cFilterAttrs));

  return HandleRequestAttrs(dwFlags, cFilterAttrs, paFilterAttrs);
}

STDMETHODIMP
nsTextStore::RequestAttrsAtPosition(LONG acpPos,
                                    ULONG cFilterAttrs,
                                    const TS_ATTRID *paFilterAttrs,
                                    DWORD dwFlags)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::RequestAttrsAtPosition(acpPos=%ld, "
          "cFilterAttrs=%lu, dwFlags=%s)",
          this, acpPos, cFilterAttrs, GetFindFlagName(dwFlags).get()));

  return HandleRequestAttrs(dwFlags | TS_ATTR_FIND_WANT_VALUE,
                            cFilterAttrs, paFilterAttrs);
}

STDMETHODIMP
nsTextStore::RequestAttrsTransitioningAtPosition(LONG acpPos,
                                                 ULONG cFilterAttrs,
                                                 const TS_ATTRID *paFilterAttr,
                                                 DWORD dwFlags)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::RequestAttrsTransitioningAtPosition("
          "acpPos=%ld, cFilterAttrs=%lu, dwFlags=%s) called but not supported "
          "(S_OK)",
          this, acpPos, cFilterAttrs, GetFindFlagName(dwFlags).get()));

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
           ("TSF:   0x%p nsTextStore::FindNextAttrTransition() FAILED due to "
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
  if (!pcFetched || !paAttrVals) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p nsTextStore::RetrieveRequestedAttrs() FAILED due to "
            "null argument", this));
    return E_INVALIDARG;
  }

  ULONG expectedCount = 0;
  for (int32_t i = 0; i < NUM_OF_SUPPORTED_ATTRS; i++) {
    if (mRequestedAttrs[i]) {
      expectedCount++;
    }
  }
  if (ulCount < expectedCount) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p nsTextStore::RetrieveRequestedAttrs() FAILED due to "
            "not enough count ulCount=%u, expectedCount=%u",
            this, ulCount, expectedCount));
    return E_INVALIDARG;
  }

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::RetrieveRequestedAttrs() called "
          "ulCount=%d, mRequestedAttrValues=%s",
          this, ulCount, GetBoolName(mRequestedAttrValues)));

  int32_t count = 0;
  for (int32_t i = 0; i < NUM_OF_SUPPORTED_ATTRS; i++) {
    if (!mRequestedAttrs[i]) {
      continue;
    }
    mRequestedAttrs[i] = false;

    TS_ATTRID attrID = GetAttrID(i);

    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::RetrieveRequestedAttrs() for %s",
            this, GetGUIDNameStrWithTable(attrID).get()));

    paAttrVals[count].idAttr = attrID;
    paAttrVals[count].dwOverlapId = 0;

    if (!mRequestedAttrValues) {
      paAttrVals[count].varValue.vt = VT_EMPTY;
    } else {
      switch (i) {
        case eInputScope: {
          paAttrVals[count].varValue.vt = VT_UNKNOWN;
          nsRefPtr<IUnknown> inputScope = new InputScopeImpl(mInputScopes);
          paAttrVals[count].varValue.punkVal = inputScope.forget().take();
          break;
        }
        case eTextVerticalWriting:
          // Currently, we don't support vertical writing mode.
          paAttrVals[count].varValue.vt = VT_BOOL;
          paAttrVals[count].varValue.boolVal = VARIANT_FALSE;
          break;
        default:
          MOZ_CRASH("Invalid index? Or not implemented yet?");
          break;
      }
    }
    count++;
  }

  mRequestedAttrValues = false;

  if (count) {
    *pcFetched = count;
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

  Content& lockedContent = LockedContent();
  if (!lockedContent.IsInitialized()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetEndACP() FAILED due to "
            "LockedContent() failure", this));
    return E_FAIL;
  }
  *pacp = static_cast<LONG>(lockedContent.Text().Length());
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

  if (mLockedContent.IsLayoutChanged()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetACPFromPoint() FAILED due to "
            "layout not recomputed", this));
    mPendingOnLayoutChange = true;
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

  // Free ChangJie 2010 and Easy Changjei 1.0.12.0 doesn't handle
  // ITfContextView::GetTextExt() properly.  Prehaps, it's due to a bug of TSF.
  // TSF (at least on Win 8.1) doesn't return TS_E_NOLAYOUT to the caller
  // even if we return it.  It's converted to just E_FAIL.
  // TODO: On Win 9, we need to check this hack is still necessary.
  const nsString& activeTIPKeyboardDescription =
    TSFStaticSink::GetInstance()->GetActiveTIPKeyboardDescription();
  if ((sDoNotReturnNoLayoutErrorToFreeChangJie &&
       activeTIPKeyboardDescription.Equals(TIP_NAME_FREE_CHANG_JIE_2010)) ||
      (sDoNotReturnNoLayoutErrorToEasyChangjei &&
       activeTIPKeyboardDescription.Equals(TIP_NAME_EASY_CHANGJEI)) &&
      mComposition.IsComposing() &&
      mLockedContent.IsLayoutChangedAfter(acpEnd) &&
      mComposition.mStart < acpEnd) {
    acpEnd = mComposition.mStart;
    acpStart = std::min(acpStart, acpEnd);
    PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
           ("TSF: 0x%p   nsTextStore::GetTextExt() hacked the offsets for TIP "
            "acpStart=%d, acpEnd=%d", this, acpStart, acpEnd));
  }

  if (mLockedContent.IsLayoutChangedAfter(acpEnd)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::GetTextExt() FAILED due to "
            "layout not recomputed at %d", this, acpEnd));
    mPendingOnLayoutChange = true;
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

  // ATOK refers native caret position and size on Desktop applications for
  // deciding candidate window.  Therefore, we need to create native caret
  // for hacking the bug.
  if (sCreateNativeCaretForATOK &&
      StringBeginsWith(
        activeTIPKeyboardDescription, TIP_NAME_BEGINS_WITH_ATOK) &&
      mComposition.IsComposing() &&
      mComposition.mStart <= acpStart && mComposition.EndOffset() >= acpStart &&
      mComposition.mStart <= acpEnd && mComposition.EndOffset() >= acpEnd) {
    if (mNativeCaretIsCreated) {
      ::DestroyCaret();
      mNativeCaretIsCreated = false;
    }
    CreateNativeCaret();
  }

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

  Content& lockedContent = LockedContent();
  if (!lockedContent.IsInitialized()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::InsertTextAtSelectionInternal() failed "
            "due to LockedContent() failure()", this));
    return false;
  }

  TS_SELECTION_ACP oldSelection = lockedContent.Selection().ACP();
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

  lockedContent.ReplaceSelectedTextWith(aInsertStr);

  if (aTextChange) {
    aTextChange->acpStart = oldSelection.acpStart;
    aTextChange->acpOldEnd = oldSelection.acpEnd;
    aTextChange->acpNewEnd = lockedContent.Selection().EndOffset();
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
nsTextStore::RecordCompositionStartAction(ITfCompositionView* aComposition,
                                          ITfRange* aRange,
                                          bool aPreserveSelection)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::RecordCompositionStartAction("
          "aComposition=0x%p, aRange=0x%p, aPreserveSelection=%s), "
          "mComposition.mView=0x%p",
          this, aComposition, aRange, GetBoolName(aPreserveSelection),
          mComposition.mView.get()));

  LONG start = 0, length = 0;
  HRESULT hr = GetRangeExtent(aRange, &start, &length);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RecordCompositionStartAction() FAILED "
            "due to GetRangeExtent() failure", this));
    return hr;
  }

  return RecordCompositionStartAction(aComposition, start, length,
                                      aPreserveSelection);
}

HRESULT
nsTextStore::RecordCompositionStartAction(ITfCompositionView* aComposition,
                                          LONG aStart,
                                          LONG aLength,
                                          bool aPreserveSelection)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::RecordCompositionStartAction("
          "aComposition=0x%p, aStart=%d, aLength=%d, aPreserveSelection=%s), "
          "mComposition.mView=0x%p",
          this, aComposition, aStart, aLength, GetBoolName(aPreserveSelection),
          mComposition.mView.get()));

  Content& lockedContent = LockedContent();
  if (!lockedContent.IsInitialized()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RecordCompositionStartAction() FAILED "
            "due to LockedContent() failure", this));
    return E_FAIL;
  }

  CompleteLastActionIfStillIncomplete();
  PendingAction* action = mPendingActions.AppendElement();
  action->mType = PendingAction::COMPOSITION_START;
  action->mSelectionStart = aStart;
  action->mSelectionLength = aLength;

  Selection& currentSel = CurrentSelection();
  if (currentSel.IsDirty()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RecordCompositionStartAction() FAILED "
            "due to CurrentSelection() failure", this));
    action->mAdjustSelection = true;
  } else if (currentSel.MinOffset() != aStart ||
             currentSel.MaxOffset() != aStart + aLength) {
    // If new composition range is different from current selection range,
    // we need to set selection before dispatching compositionstart event.
    action->mAdjustSelection = true;
  } else {
    // We shouldn't dispatch selection set event before dispatching
    // compositionstart event because it may cause put caret different
    // position in HTML editor since generated flat text content and offset in
    // it are lossy data of HTML contents.
    action->mAdjustSelection = false;
  }

  lockedContent.StartComposition(aComposition, *action, aPreserveSelection);

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
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
         ("TSF: 0x%p   nsTextStore::RecordCompositionEndAction(), "
          "mComposition={ mView=0x%p, mString=\"%s\" }",
          this, mComposition.mView.get(),
          NS_ConvertUTF16toUTF8(mComposition.mString).get()));

  MOZ_ASSERT(mComposition.IsComposing());

  CompleteLastActionIfStillIncomplete();
  PendingAction* action = mPendingActions.AppendElement();
  action->mType = PendingAction::COMPOSITION_END;
  action->mData = mComposition.mString;

  Content& lockedContent = LockedContent();
  if (!lockedContent.IsInitialized()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::RecordCompositionEndAction() FAILED due "
            "to LockedContent() failure", this));
    return E_FAIL;
  }
  lockedContent.EndComposition(*action);

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
    PendingAction* action = LastOrNewPendingCompositionUpdate();
    action->mIncomplete = true;
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

STDMETHODIMP
nsTextStore::AdviseMouseSink(ITfRangeACP* range,
                             ITfMouseSink* pSink,
                             DWORD* pdwCookie)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::AdviseMouseSink(range=0x%p, pSink=0x%p, "
          "pdwCookie=0x%p)", this, range, pSink, pdwCookie));

  if (!pdwCookie) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::AdviseMouseSink() FAILED due to the "
            "pdwCookie is null", this));
    return E_INVALIDARG;
  }
  // Initialize the result with invalid cookie for safety.
  *pdwCookie = MouseTracker::kInvalidCookie;

  if (!range) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::AdviseMouseSink() FAILED due to the "
            "range is null", this));
    return E_INVALIDARG;
  }
  if (!pSink) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::AdviseMouseSink() FAILED due to the "
            "pSink is null", this));
    return E_INVALIDARG;
  }

  // Looking for an unusing tracker.
  MouseTracker* tracker = nullptr;
  for (size_t i = 0; i < mMouseTrackers.Length(); i++) {
    if (mMouseTrackers[i].IsUsing()) {
      continue;
    }
    tracker = &mMouseTrackers[i];
  }
  // If there is no unusing tracker, create new one.
  // XXX Should we make limitation of the number of installs?
  if (!tracker) {
    tracker = mMouseTrackers.AppendElement();
    HRESULT hr = tracker->Init(this);
    if (FAILED(hr)) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF: 0x%p   nsTextStore::AdviseMouseSink() FAILED due to "
              "failure of MouseTracker::Init()", this));
      return hr;
    }
  }
  HRESULT hr = tracker->AdviseSink(this, range, pSink);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::AdviseMouseSink() FAILED due to failure "
            "of MouseTracker::Init()", this));
    return hr;
  }
  *pdwCookie = tracker->Cookie();
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::AdviseMouseSink(), succeeded, "
          "*pdwCookie=%d", this, *pdwCookie));
  return S_OK;
}

STDMETHODIMP
nsTextStore::UnadviseMouseSink(DWORD dwCookie)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p nsTextStore::UnadviseMouseSink(dwCookie=%d)",
          this, dwCookie));
  if (dwCookie == MouseTracker::kInvalidCookie) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::UnadviseMouseSink() FAILED due to "
            "the cookie is invalid value", this));
    return E_INVALIDARG;
  }
  // The cookie value must be an index of mMouseTrackers.
  // We can use this shortcut for now.
  if (static_cast<size_t>(dwCookie) >= mMouseTrackers.Length()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::UnadviseMouseSink() FAILED due to "
            "the cookie is too large value", this));
    return E_INVALIDARG;
  }
  MouseTracker& tracker = mMouseTrackers[dwCookie];
  if (!tracker.IsUsing()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::UnadviseMouseSink() FAILED due to "
            "the found tracker uninstalled already", this));
    return E_INVALIDARG;
  }
  tracker.UnadviseSink();
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::UnadviseMouseSink(), succeeded", this));
  return S_OK;
}

// static
nsresult
nsTextStore::OnFocusChange(bool aGotFocus,
                           nsWindowBase* aFocusedWidget,
                           const InputContext& aContext)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF:   nsTextStore::OnFocusChange(aGotFocus=%s, "
          "aFocusedWidget=0x%p, aContext={ mIMEState={ mEnabled=%s }, "
          "mHTMLInputType=\"%s\" }), "
          "sThreadMgr=0x%p, sEnabledTextStore=0x%p",
          GetBoolName(aGotFocus), aFocusedWidget,
          GetIMEEnabledName(aContext.mIMEState.mEnabled),
          NS_ConvertUTF16toUTF8(aContext.mHTMLInputType).get(),
          sThreadMgr.get(), sEnabledTextStore.get()));

  if (NS_WARN_IF(!IsInTSFMode())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If currently sEnableTextStore has focus, notifies TSF of losing focus.
  if (ThinksHavingFocus()) {
    nsRefPtr<ITfDocumentMgr> prevFocusedDocumentMgr;
    DebugOnly<HRESULT> hr =
      sThreadMgr->AssociateFocus(
        sEnabledTextStore->mWidget->GetWindowHandle(),
        nullptr, getter_AddRefs(prevFocusedDocumentMgr));
    NS_ASSERTION(SUCCEEDED(hr), "Disassociating focus failed");
    NS_ASSERTION(prevFocusedDocumentMgr == sEnabledTextStore->mDocumentMgr,
                 "different documentMgr has been associated with the window");
  }

  // If there is sEnabledTextStore, we don't use it in the new focused editor.
  // Release it now.
  if (sEnabledTextStore) {
    sEnabledTextStore->Destroy();
    sEnabledTextStore = nullptr;
  }

  // If this is a notification of blur, move focus to the dummy document
  // manager.
  if (!aGotFocus || !aContext.mIMEState.IsEditable()) {
    HRESULT hr = sThreadMgr->SetFocus(sDisabledDocumentMgr);
    if (NS_WARN_IF(FAILED(hr))) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
             ("TSF:   nsTextStore::OnFocusChange() FAILED due to "
              "ITfThreadMgr::SetFocus() failure"));
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

  // If an editor is getting focus, create new TextStore and set focus.
  if (NS_WARN_IF(!CreateAndSetFocus(aFocusedWidget, aContext))) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF:   nsTextStore::OnFocusChange() FAILED due to "
            "ITfThreadMgr::CreateAndSetFocus() failure"));
    // If setting focus, we should destroy the TextStore completely because
    // it causes memory leak.
    if (sEnabledTextStore) {
      sEnabledTextStore->Destroy();
      sEnabledTextStore = nullptr;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// static
bool
nsTextStore::CreateAndSetFocus(nsWindowBase* aFocusedWidget,
                               const InputContext& aContext)
{
  // TSF might do something which causes that we need to access static methods
  // of nsTextStore.  At that time, sEnabledTextStore may be necessary.
  // So, we should set sEnabledTextStore directly.
  sEnabledTextStore = new nsTextStore();
  if (NS_WARN_IF(!sEnabledTextStore->Init(aFocusedWidget))) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF:   nsTextStore::CreateAndSetFocus() FAILED due to "
            "nsTextStore::Init() failure"));
    return false;
  }
  if (NS_WARN_IF(!sEnabledTextStore->mDocumentMgr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF:   nsTextStore::CreateAndSetFocus() FAILED due to "
            "invalid nsTextStore::mDocumentMgr"));
    return false;
  }
  if (aContext.mIMEState.mEnabled == IMEState::PASSWORD) {
    MarkContextAsKeyboardDisabled(sEnabledTextStore->mContext);
    nsRefPtr<ITfContext> topContext;
    sEnabledTextStore->mDocumentMgr->GetTop(getter_AddRefs(topContext));
    if (topContext && topContext != sEnabledTextStore->mContext) {
      MarkContextAsKeyboardDisabled(topContext);
    }
  }
  HRESULT hr = sThreadMgr->SetFocus(sEnabledTextStore->mDocumentMgr);
  if (NS_WARN_IF(FAILED(hr))) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF:   nsTextStore::CreateAndSetFocus() FAILED due to "
            "ITfTheadMgr::SetFocus() failure"));
    return false;
  }
  // Use AssociateFocus() for ensuring that any native focus event
  // never steal focus from our documentMgr.
  nsRefPtr<ITfDocumentMgr> prevFocusedDocumentMgr;
  hr = sThreadMgr->AssociateFocus(aFocusedWidget->GetWindowHandle(),
                                  sEnabledTextStore->mDocumentMgr,
                                  getter_AddRefs(prevFocusedDocumentMgr));
  if (NS_WARN_IF(FAILED(hr))) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF:   nsTextStore::CreateAndSetFocus() FAILED due to "
            "ITfTheadMgr::AssociateFocus() failure"));
    return false;
  }
  sEnabledTextStore->SetInputScope(aContext.mHTMLInputType);
  return true;
}

// static
nsIMEUpdatePreference
nsTextStore::GetIMEUpdatePreference()
{
  if (sThreadMgr && sEnabledTextStore && sEnabledTextStore->mDocumentMgr) {
    nsRefPtr<ITfDocumentMgr> docMgr;
    sThreadMgr->GetFocus(getter_AddRefs(docMgr));
    if (docMgr == sEnabledTextStore->mDocumentMgr) {
      nsIMEUpdatePreference updatePreference(
        nsIMEUpdatePreference::NOTIFY_SELECTION_CHANGE |
        nsIMEUpdatePreference::NOTIFY_TEXT_CHANGE |
        nsIMEUpdatePreference::NOTIFY_POSITION_CHANGE |
        nsIMEUpdatePreference::NOTIFY_MOUSE_BUTTON_EVENT_ON_CHAR |
        nsIMEUpdatePreference::NOTIFY_DURING_DEACTIVE);
      // nsTextStore shouldn't notify TSF of selection change and text change
      // which are caused by composition.
      updatePreference.DontNotifyChangesCausedByComposition();
      return updatePreference;
    }
  }
  return nsIMEUpdatePreference();
}

nsresult
nsTextStore::OnTextChangeInternal(const IMENotification& aIMENotification)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::OnTextChangeInternal(aIMENotification={ "
          "mMessage=0x%08X, mTextChangeData={ mStartOffset=%lu, "
          "mOldEndOffset=%lu, mNewEndOffset=%lu}), mSink=0x%p, mSinkMask=%s, "
          "mComposition.IsComposing()=%s",
          this, aIMENotification.mMessage,
          aIMENotification.mTextChangeData.mStartOffset,
          aIMENotification.mTextChangeData.mOldEndOffset,
          aIMENotification.mTextChangeData.mNewEndOffset, mSink.get(),
          GetSinkMaskNameStr(mSinkMask).get(),
          GetBoolName(mComposition.IsComposing())));

  if (IsReadLocked()) {
    return NS_OK;
  }

  mSelection.MarkDirty();

  if (!mSink || !(mSinkMask & TS_AS_TEXT_CHANGE)) {
    return NS_OK;
  }

  if (!aIMENotification.mTextChangeData.IsInInt32Range()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::OnTextChangeInternal() FAILED due to "
            "offset is too big for calling mSink->OnTextChange()...",
            this));
    return NS_OK;
  }

  // Some TIPs are confused by text change notification during composition.
  // Especially, some of them stop working for composition in our process.
  // For preventing it, let's commit the composition.
  if (mComposition.IsComposing()) {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::OnTextChangeInternal(), "
            "committing the composition for avoiding making TIP confused...",
            this));
    CommitCompositionInternal(false);
    return NS_OK;
  }

  TS_TEXTCHANGE textChange;
  textChange.acpStart =
    static_cast<LONG>(aIMENotification.mTextChangeData.mStartOffset);
  textChange.acpOldEnd =
    static_cast<LONG>(aIMENotification.mTextChangeData.mOldEndOffset);
  textChange.acpNewEnd =
    static_cast<LONG>(aIMENotification.mTextChangeData.mNewEndOffset);

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::OnTextChangeInternal(), calling "
          "mSink->OnTextChange(0, { acpStart=%ld, acpOldEnd=%ld, "
          "acpNewEnd=%ld })...", this, textChange.acpStart,
          textChange.acpOldEnd, textChange.acpNewEnd));
  mSink->OnTextChange(0, &textChange);

  return NS_OK;
}

nsresult
nsTextStore::OnSelectionChangeInternal(void)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::OnSelectionChangeInternal(), "
          "mSink=0x%p, mSinkMask=%s, mIsRecordingActionsWithoutLock=%s, "
          "mComposition.IsComposing()=%s",
          this, mSink.get(), GetSinkMaskNameStr(mSinkMask).get(),
          GetBoolName(mIsRecordingActionsWithoutLock),
          GetBoolName(mComposition.IsComposing())));

  if (IsReadLocked()) {
    return NS_OK;
  }

  mSelection.MarkDirty();

  if (!mSink || !(mSinkMask & TS_AS_SEL_CHANGE)) {
    return NS_OK;
  }

  // Some TIPs are confused by selection change notification during composition.
  // Especially, some of them stop working for composition in our process.
  // For preventing it, let's commit the composition.
  if (mComposition.IsComposing()) {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::OnSelectionChangeInternal(), "
            "committing the composition for avoiding making TIP confused...",
            this));
    CommitCompositionInternal(false);
    return NS_OK;
  }

  if (!mIsRecordingActionsWithoutLock) {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::OnSelectionChangeInternal(), calling "
            "mSink->OnSelectionChange()...", this));
    mSink->OnSelectionChange();
  } else {
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: 0x%p   nsTextStore::OnSelectionChangeInternal(), pending "
            "a call of mSink->OnSelectionChange()...", this));
    mPendingOnSelectionChange = true;
  }
  return NS_OK;
}

nsresult
nsTextStore::OnLayoutChangeInternal()
{
  NS_ENSURE_TRUE(mContext, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSink, NS_ERROR_FAILURE);

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: 0x%p   nsTextStore::OnLayoutChangeInternal(), calling "
          "mSink->OnLayoutChange()...", this));
  HRESULT hr = mSink->OnLayoutChange(TS_LC_CHANGE, TEXTSTORE_DEFAULT_VIEW);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
nsTextStore::OnMouseButtonEventInternal(const IMENotification& aIMENotification)
{
  if (mMouseTrackers.IsEmpty()) {
    return NS_OK;
  }

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::OnMouseButtonEventInternal("
          "aIMENotification={ mEventMessage=%s, mOffset=%u, mCursorPos={ "
          "mX=%d, mY=%d }, mCharRect={ mX=%d, mY=%d, mWidth=%d, mHeight=%d }, "
          "mButton=%s, mButtons=%s, mModifiers=%s })",
          this, GetEventMessageName(
                  aIMENotification.mMouseButtonEventData.mEventMessage),
          aIMENotification.mMouseButtonEventData.mOffset,
          aIMENotification.mMouseButtonEventData.mCursorPos.mX,
          aIMENotification.mMouseButtonEventData.mCursorPos.mY,
          aIMENotification.mMouseButtonEventData.mCharRect.mX,
          aIMENotification.mMouseButtonEventData.mCharRect.mY,
          aIMENotification.mMouseButtonEventData.mCharRect.mWidth,
          aIMENotification.mMouseButtonEventData.mCharRect.mHeight,
          GetMouseButtonName(aIMENotification.mMouseButtonEventData.mButton),
          GetMouseButtonsName(
            aIMENotification.mMouseButtonEventData.mButtons).get(),
          GetModifiersName(
            aIMENotification.mMouseButtonEventData.mModifiers).get()));

  uint32_t offset = aIMENotification.mMouseButtonEventData.mOffset;
  nsIntRect charRect =
    aIMENotification.mMouseButtonEventData.mCharRect.AsIntRect();
  nsIntPoint cursorPos =
    aIMENotification.mMouseButtonEventData.mCursorPos.AsIntPoint();
  ULONG quadrant = 1;
  if (charRect.width > 0) {
    int32_t cursorXInChar = cursorPos.x - charRect.x;
    quadrant = cursorXInChar * 4 / charRect.width;
    quadrant = (quadrant + 2) % 4;
  }
  ULONG edge = quadrant < 2 ? offset + 1 : offset;
  DWORD buttonStatus = 0;
  bool isMouseUp =
    aIMENotification.mMouseButtonEventData.mEventMessage == NS_MOUSE_BUTTON_UP;
  if (!isMouseUp) {
    switch (aIMENotification.mMouseButtonEventData.mButton) {
      case WidgetMouseEventBase::eLeftButton:
        buttonStatus = MK_LBUTTON;
        break;
      case WidgetMouseEventBase::eMiddleButton:
        buttonStatus = MK_MBUTTON;
        break;
      case WidgetMouseEventBase::eRightButton:
        buttonStatus = MK_RBUTTON;
        break;
    }
  }
  if (aIMENotification.mMouseButtonEventData.mModifiers & MODIFIER_CONTROL) {
    buttonStatus |= MK_CONTROL;
  }
  if (aIMENotification.mMouseButtonEventData.mModifiers & MODIFIER_SHIFT) {
    buttonStatus |= MK_SHIFT;
  }
  for (size_t i = 0; i < mMouseTrackers.Length(); i++) {
    MouseTracker& tracker = mMouseTrackers[i];
    if (!tracker.IsUsing() || !tracker.InRange(offset)) {
      continue;
    }
    if (tracker.OnMouseButtonEvent(edge - tracker.RangeStart(),
                                   quadrant, buttonStatus)) {
      return NS_SUCCESS_EVENT_CONSUMED;
    }
  }
  return NS_OK;
}

void
nsTextStore::CreateNativeCaret()
{
  // This method must work only on desktop application.
  if (XRE_GetWindowsEnvironment() != WindowsEnvironmentType_Desktop) {
    return;
  }

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::CreateNativeCaret(), "
          "mComposition.IsComposing()=%s",
          this, GetBoolName(mComposition.IsComposing())));

  Selection& currentSel = CurrentSelection();
  if (currentSel.IsDirty()) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::CreateNativeCaret() FAILED due to "
            "CurrentSelection() failure", this));
    return;
  }

  // XXX If this is called without composition and the selection isn't
  //     collapsed, is it OK?
  uint32_t caretOffset = currentSel.MaxOffset();

  WidgetQueryContentEvent queryCaretRect(true, NS_QUERY_CARET_RECT, mWidget);
  queryCaretRect.InitForQueryCaretRect(caretOffset);
  mWidget->InitEvent(queryCaretRect);
  mWidget->DispatchWindowEvent(&queryCaretRect);
  if (!queryCaretRect.mSucceeded) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::CreateNativeCaret() FAILED due to "
            "NS_QUERY_CARET_RECT failure (offset=%d)", this, caretOffset));
    return;
  }

  nsIntRect& caretRect = queryCaretRect.mReply.mRect;
  mNativeCaretIsCreated = ::CreateCaret(mWidget->GetWindowHandle(), nullptr,
                                        caretRect.width, caretRect.height);
  if (!mNativeCaretIsCreated) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::CreateNativeCaret() FAILED due to "
            "CreateCaret() failure", this));
    return;
  }

  nsWindow* window = static_cast<nsWindow*>(mWidget.get());
  nsWindow* toplevelWindow = window->GetTopLevelWindow(false);
  if (!toplevelWindow) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::CreateNativeCaret() FAILED due to "
            "no top level window", this));
    return;
  }

  if (toplevelWindow != window) {
    caretRect.MoveBy(toplevelWindow->WidgetToScreenOffset());
    caretRect.MoveBy(-window->WidgetToScreenOffset());
  }

  ::SetCaretPos(caretRect.x, caretRect.y);
}

void
nsTextStore::CommitCompositionInternal(bool aDiscard)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::CommitCompositionInternal(aDiscard=%s), "
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
  if (!GetCompartment(sThreadMgr,
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
  comp->SetValue(sClientId, &variant);
}

// static
bool
nsTextStore::GetIMEOpenState(void)
{
  nsRefPtr<ITfCompartment> comp;
  if (!GetCompartment(sThreadMgr,
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
          "sEnabledTextStore=0x%p, ThinksHavingFocus()=%s",
          aWidget, GetIMEEnabledName(aContext.mIMEState.mEnabled),
          GetFocusChangeName(aAction.mFocusChange), sEnabledTextStore.get(),
          GetBoolName(ThinksHavingFocus())));

  NS_ENSURE_TRUE_VOID(IsInTSFMode());

  if (aAction.mFocusChange != InputContextAction::FOCUS_NOT_CHANGED) {
    if (sEnabledTextStore) {
      sEnabledTextStore->SetInputScope(aContext.mHTMLInputType);
    }
    return;
  }

  // If focus isn't actually changed but the enabled state is changed,
  // emulate the focus move.
  if (!ThinksHavingFocus() && aContext.mIMEState.IsEditable()) {
    OnFocusChange(true, aWidget, aContext);
  } else if (ThinksHavingFocus() && !aContext.mIMEState.IsEditable()) {
    OnFocusChange(false, aWidget, aContext);
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
  comp->SetValue(sClientId, &variant_int4_value1);
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
  comp->SetValue(sClientId, &variant_int4_value1);
}

// static
void
nsTextStore::Initialize()
{
#ifdef PR_LOGGING
  if (!sTextStoreLog) {
    sTextStoreLog = PR_NewLogModule("nsTextStoreWidgets");
  }
#endif

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF: nsTextStore::Initialize() is called..."));

  if (sThreadMgr) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF:   nsTextStore::Initialize() FAILED due to already initialized"));
    return;
  }

  bool enableTsf =
    Preferences::GetBool(kPrefNameForceEnableTSF, false) ||
    (IsVistaOrLater() && Preferences::GetBool(kPrefNameEnableTSF, false));
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF:   nsTextStore::Initialize(), TSF is %s",
     enableTsf ? "enabled" : "disabled"));
  if (!enableTsf) {
    return;
  }

  // XXX MSDN documents that ITfInputProcessorProfiles is available only on
  //     desktop apps.  However, there is no known way to obtain
  //     ITfInputProcessorProfileMgr instance without ITfInputProcessorProfiles
  //     instance.
  nsRefPtr<ITfInputProcessorProfiles> inputProcessorProfiles;
  HRESULT hr =
    ::CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                       CLSCTX_INPROC_SERVER,
                       IID_ITfInputProcessorProfiles,
                       getter_AddRefs(inputProcessorProfiles));
  if (FAILED(hr) || !inputProcessorProfiles) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF:   nsTextStore::Initialize() FAILED to create input processor "
       "profiles, hr=0x%08X", hr));
    return;
  }

  nsRefPtr<ITfThreadMgr> threadMgr;
  hr = ::CoCreateInstance(CLSID_TF_ThreadMgr, nullptr,
                          CLSCTX_INPROC_SERVER, IID_ITfThreadMgr,
                          getter_AddRefs(threadMgr));
  if (FAILED(hr) || !threadMgr) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF:   nsTextStore::Initialize() FAILED to "
       "create the thread manager, hr=0x%08X", hr));
    return;
  }

  nsRefPtr<ITfMessagePump> messagePump;
  hr = threadMgr->QueryInterface(IID_ITfMessagePump,
                                 getter_AddRefs(messagePump));
  if (FAILED(hr) || !messagePump) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF:   nsTextStore::Initialize() FAILED to "
       "QI message pump from the thread manager, hr=0x%08X", hr));
    return;
  }

  nsRefPtr<ITfKeystrokeMgr> keystrokeMgr;
  hr = threadMgr->QueryInterface(IID_ITfKeystrokeMgr,
                                 getter_AddRefs(keystrokeMgr));
  if (FAILED(hr) || !keystrokeMgr) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF:   nsTextStore::Initialize() FAILED to "
       "QI keystroke manager from the thread manager, hr=0x%08X", hr));
    return;
  }

  hr = threadMgr->Activate(&sClientId);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF:   nsTextStore::Initialize() FAILED to activate, hr=0x%08X", hr));
    return;
  }

  nsRefPtr<ITfDisplayAttributeMgr> displayAttributeMgr;
  hr = ::CoCreateInstance(CLSID_TF_DisplayAttributeMgr, nullptr,
                          CLSCTX_INPROC_SERVER, IID_ITfDisplayAttributeMgr,
                          getter_AddRefs(displayAttributeMgr));
  if (FAILED(hr) || !displayAttributeMgr) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF:   nsTextStore::Initialize() FAILED to create "
       "a display attribute manager instance, hr=0x%08X", hr));
    return;
  }

  nsRefPtr<ITfCategoryMgr> categoryMgr;
  hr = ::CoCreateInstance(CLSID_TF_CategoryMgr, nullptr,
                          CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                          getter_AddRefs(categoryMgr));
  if (FAILED(hr) || !categoryMgr) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF:   nsTextStore::Initialize() FAILED to create "
       "a category manager instance, hr=0x%08X", hr));
    return;
  }

  nsRefPtr<ITfDocumentMgr> disabledDocumentMgr;
  hr = threadMgr->CreateDocumentMgr(getter_AddRefs(disabledDocumentMgr));
  if (FAILED(hr) || !disabledDocumentMgr) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF:   nsTextStore::Initialize() FAILED to create "
       "a document manager for disabled mode, hr=0x%08X", hr));
    return;
  }

  nsRefPtr<ITfContext> disabledContext;
  DWORD editCookie = 0;
  hr = disabledDocumentMgr->CreateContext(sClientId, 0, nullptr,
                                          getter_AddRefs(disabledContext),
                                          &editCookie);
  if (FAILED(hr) || !disabledContext) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF:   nsTextStore::Initialize() FAILED to create "
       "a context for disabled mode, hr=0x%08X", hr));
    return;
  }

  MarkContextAsKeyboardDisabled(disabledContext);
  MarkContextAsEmpty(disabledContext);

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF:   nsTextStore::Initialize() is creating "
     "a TSFStaticSink instance..."));
  TSFStaticSink* staticSink = TSFStaticSink::GetInstance();
  if (!staticSink->Init(threadMgr, inputProcessorProfiles)) {
    TSFStaticSink::Shutdown();
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF:   nsTextStore::Initialize() FAILED to initialize TSFStaticSink "
       "instance"));
    return;
  }

  sInputProcessorProfiles = inputProcessorProfiles;
  sThreadMgr = threadMgr;
  sMessagePump = messagePump;
  sKeystrokeMgr = keystrokeMgr;
  sDisplayAttrMgr = displayAttributeMgr;
  sCategoryMgr = categoryMgr;
  sDisabledDocumentMgr = disabledDocumentMgr;
  sDisabledContext = disabledContext;

  sCreateNativeCaretForATOK =
    Preferences::GetBool("intl.tsf.hack.atok.create_native_caret", true);
  sDoNotReturnNoLayoutErrorToFreeChangJie =
    Preferences::GetBool(
      "intl.tsf.hack.free_chang_jie.do_not_return_no_layout_error", true);
  sDoNotReturnNoLayoutErrorToEasyChangjei =
    Preferences::GetBool(
      "intl.tsf.hack.easy_changjei.do_not_return_no_layout_error", true);

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
    ("TSF:   nsTextStore::Initialize(), sThreadMgr=0x%p, "
     "sClientId=0x%08X, sDisplayAttrMgr=0x%p, "
     "sCategoryMgr=0x%p, sDisabledDocumentMgr=0x%p, sDisabledContext=%p, "
     "sCreateNativeCaretForATOK=%s, "
     "sDoNotReturnNoLayoutErrorToFreeChangJie=%s, "
     "sDoNotReturnNoLayoutErrorToEasyChangjei=%s",
     sThreadMgr.get(), sClientId, sDisplayAttrMgr.get(),
     sCategoryMgr.get(), sDisabledDocumentMgr.get(), sDisabledContext.get(),
     GetBoolName(sCreateNativeCaretForATOK),
     GetBoolName(sDoNotReturnNoLayoutErrorToFreeChangJie),
     GetBoolName(sDoNotReturnNoLayoutErrorToEasyChangjei)));
}

// static
void
nsTextStore::Terminate(void)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS, ("TSF: nsTextStore::Terminate()"));

  TSFStaticSink::Shutdown();

  sDisplayAttrMgr = nullptr;
  sCategoryMgr = nullptr;
  sEnabledTextStore = nullptr;
  sDisabledDocumentMgr = nullptr;
  sDisabledContext = nullptr;
  sInputProcessorProfiles = nullptr;
  sClientId = 0;
  if (sThreadMgr) {
    sThreadMgr->Deactivate();
    sThreadMgr = nullptr;
    sMessagePump = nullptr;
    sKeystrokeMgr = nullptr;
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

// static
void
nsTextStore::ProcessMessage(nsWindowBase* aWindow, UINT aMessage,
                            WPARAM& aWParam, LPARAM& aLParam,
                            MSGResult& aResult)
{
  switch (aMessage) {
    case WM_IME_SETCONTEXT:
      // If a windowless plugin had focus and IME was handled on it, composition
      // window was set the position.  After that, even in TSF mode, WinXP keeps
      // to use composition window at the position if the active IME is not
      // aware TSF.  For avoiding this issue, we need to hide the composition
      // window here.
      if (aWParam) {
        aLParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
      }
      break;
    case WM_ENTERIDLE:
      // When an modal dialog such as a file picker is open, composition
      // should be committed because IME might be used on it.
      if (!IsComposingOn(aWindow)) {
        break;
      }
      CommitComposition(false);
      break;
  }
}

// static
bool
nsTextStore::IsIMM_IME()
{
  return TSFStaticSink::IsIMM_IME();
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

/******************************************************************************
 *  nsTextStore::MouseTracker
 *****************************************************************************/

nsTextStore::MouseTracker::MouseTracker()
  : mStart(-1)
  , mLength(-1)
  , mCookie(kInvalidCookie)
{
}

HRESULT
nsTextStore::MouseTracker::Init(nsTextStore* aTextStore)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::MouseTracker::Init(aTextStore=0x%p), "
          "aTextStore->mMouseTrackers.Length()=%d",
          this, aTextStore->mMouseTrackers.Length()));

  if (&aTextStore->mMouseTrackers.LastElement() != this) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::MouseTracker::Init() FAILED due to "
            "this is not the last element of mMouseTrackers", this));
    return E_FAIL;
  }
  if (aTextStore->mMouseTrackers.Length() > kInvalidCookie) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::MouseTracker::Init() FAILED due to "
            "no new cookie available", this));
    return E_FAIL;
  }
  MOZ_ASSERT(!aTextStore->mMouseTrackers.IsEmpty(),
             "This instance must be in nsTextStore::mMouseTrackers");
  mCookie = static_cast<DWORD>(aTextStore->mMouseTrackers.Length() - 1);
  return S_OK;
}

HRESULT
nsTextStore::MouseTracker::AdviseSink(nsTextStore* aTextStore,
                                      ITfRangeACP* aTextRange,
                                      ITfMouseSink* aMouseSink)
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::MouseTracker::AdviseSink(aTextStore=0x%p, "
          "aTextRange=0x%p, aMouseSink=0x%p), mCookie=%d, mSink=0x%p",
          this, aTextStore, aTextRange, aMouseSink, mCookie, mSink.get()));
  MOZ_ASSERT(mCookie != kInvalidCookie, "This hasn't been initalized?");

  if (mSink) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::MouseTracker::AdviseMouseSink() FAILED "
            "due to already being used", this));
    return E_FAIL;
  }

  HRESULT hr = aTextRange->GetExtent(&mStart, &mLength);
  if (FAILED(hr)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::MouseTracker::AdviseMouseSink() FAILED "
            "due to failure of ITfRangeACP::GetExtent()", this));
    return hr;
  }

  if (mStart < 0 || mLength <= 0) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::MouseTracker::AdviseMouseSink() FAILED "
            "due to odd result of ITfRangeACP::GetExtent(), "
            "mStart=%d, mLength=%d", this, mStart, mLength));
    return E_INVALIDARG;
  }

  nsAutoString textContent;
  if (NS_WARN_IF(!aTextStore->GetCurrentText(textContent))) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::MouseTracker::AdviseMouseSink() FAILED "
            "due to failure of nsTextStore::GetCurrentText()", this));
    return E_FAIL;
  }

  if (textContent.Length() <= static_cast<uint32_t>(mStart) ||
      textContent.Length() < static_cast<uint32_t>(mStart + mLength)) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
           ("TSF: 0x%p   nsTextStore::MouseTracker::AdviseMouseSink() FAILED "
            "due to out of range, mStart=%d, mLength=%d, "
            "textContent.Length()=%d",
            this, mStart, mLength, textContent.Length()));
    return E_INVALIDARG;
  }

  mSink = aMouseSink;

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::MouseTracker::AdviseMouseSink(), "
          "succeeded, mStart=%d, mLength=%d, textContent.Length()=%d",
          this, mStart, mLength, textContent.Length()));
  return S_OK;
}

void
nsTextStore::MouseTracker::UnadviseSink()
{
  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::MouseTracker::UnadviseSink(), "
          "mCookie=%d, mSink=0x%p, mStart=%d, mLength=%d",
          this, mCookie, mSink.get(), mStart, mLength));
  mSink = nullptr;
  mStart = mLength = -1;
}

bool
nsTextStore::MouseTracker::OnMouseButtonEvent(ULONG aEdge,
                                              ULONG aQuadrant,
                                              DWORD aButtonStatus)
{
  MOZ_ASSERT(IsUsing(), "The caller must check before calling OnMouseEvent()");

  BOOL eaten = FALSE;
  HRESULT hr = mSink->OnMouseEvent(aEdge, aQuadrant, aButtonStatus, &eaten);

  PR_LOG(sTextStoreLog, PR_LOG_DEBUG,
         ("TSF: 0x%p   nsTextStore::MouseTracker::OnMouseEvent(aEdge=%d, "
          "aQuadrant=%d, aButtonStatus=0x%08X), hr=0x%08X, eaten=%s",
          this, aEdge, aQuadrant, aButtonStatus, hr, GetBoolName(!!eaten)));

  return SUCCEEDED(hr) && eaten;
}

#ifdef DEBUG
// static
bool
nsTextStore::CurrentKeyboardLayoutHasIME()
{
  if (!sInputProcessorProfiles) {
    PR_LOG(sTextStoreLog, PR_LOG_ERROR,
      ("TSF: nsTextStore::CurrentKeyboardLayoutHasIME() FAILED due to there is "
       "no input processor profiles instance"));
    return false;
  }
  nsRefPtr<ITfInputProcessorProfileMgr> profileMgr;
  HRESULT hr =
    sInputProcessorProfiles->QueryInterface(IID_ITfInputProcessorProfileMgr,
                                            getter_AddRefs(profileMgr));
  if (FAILED(hr) || !profileMgr) {
    // On Windows Vista or later, ImmIsIME() API always returns true.
    // If we failed to obtain the profile manager, we cannot know if current
    // keyboard layout has IME.
    if (IsVistaOrLater()) {
      PR_LOG(sTextStoreLog, PR_LOG_ERROR,
        ("TSF:   nsTextStore::CurrentKeyboardLayoutHasIME() FAILED to query "
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
      ("TSF:   nsTextStore::CurrentKeyboardLayoutHasIME() FAILED to retreive "
       "active profile"));
    return false;
  }
  return (profile.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR);
}
#endif // #ifdef DEBUG
