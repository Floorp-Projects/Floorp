/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <olectl.h>
#include <algorithm>

#include "mozilla/Logging.h"

#include "nscore.h"
#include "nsWindow.h"
#include "nsPrintfCString.h"
#include "WinUtils.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"
#include "mozilla/WindowsVersion.h"
#include "nsIXULRuntime.h"

#define INPUTSCOPE_INIT_GUID
#define TEXTATTRS_INIT_GUID
#include "TSFTextStore.h"

namespace mozilla {
namespace widget {

static const char* kPrefNameEnableTSF = "intl.tsf.enable";

/**
 * TSF related code should log its behavior even on release build especially
 * in the interface methods.
 *
 * In interface methods, use LogLevel::Info.
 * In internal methods, use LogLevel::Debug for logging normal behavior.
 * For logging error, use LogLevel::Error.
 *
 * When an instance method is called, start with following text:
 *   "0x%p TSFFoo::Bar(", the 0x%p should be the "this" of the nsFoo.
 * after that, start with:
 *   "0x%p   TSFFoo::Bar("
 * In an internal method, start with following text:
 *   "0x%p   TSFFoo::Bar("
 * When a static method is called, start with following text:
 *   "TSFFoo::Bar("
 */

LazyLogModule sTextStoreLog("nsTextStoreWidgets");

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

class GetACPFromPointFlagName : public nsAutoCString
{
public:
  explicit GetACPFromPointFlagName(DWORD aFlags)
  {
    if (!aFlags) {
      AppendLiteral("no flags (0)");
      return;
    }
    if (aFlags & GXFPF_ROUND_NEAREST) {
      AppendLiteral("GXFPF_ROUND_NEAREST");
      aFlags &= ~GXFPF_ROUND_NEAREST;
    }
    if (aFlags & GXFPF_NEAREST) {
      HandleSeparator(*this);
      AppendLiteral("GXFPF_NEAREST");
      aFlags &= ~GXFPF_NEAREST;
    }
    if (aFlags) {
      HandleSeparator(*this);
      AppendLiteral("Unknown(");
      AppendInt(static_cast<uint32_t>(aFlags));
      Append(')');
    }
  }
  virtual ~GetACPFromPointFlagName() {}
};

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
GetColorName(const TF_DA_COLOR& aColor)
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
GetDisplayAttrStr(const TF_DISPLAYATTRIBUTE& aDispAttr)
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
  if (aModifiers & MODIFIER_FNLOCK) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += NS_DOM_KEYNAME_FNLOCK;
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
  if (aModifiers & MODIFIER_SYMBOL) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += NS_DOM_KEYNAME_SYMBOL;
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

class GetWritingModeName : public nsAutoCString
{
public:
  explicit GetWritingModeName(const WritingMode& aWritingMode)
  {
    if (!aWritingMode.IsVertical()) {
      AssignLiteral("Horizontal");
      return;
    }
    if (aWritingMode.IsVerticalLR()) {
      AssignLiteral("Vertical (LR)");
      return;
    }
    AssignLiteral("Vertical (RL)");
  }
  virtual ~GetWritingModeName() {}
};

class GetEscapedUTF8String final : public NS_ConvertUTF16toUTF8
{
public:
  explicit GetEscapedUTF8String(const nsAString& aString)
    : NS_ConvertUTF16toUTF8(aString)
  {
    Escape();
  }
  explicit GetEscapedUTF8String(const char16ptr_t aString)
    : NS_ConvertUTF16toUTF8(aString)
  {
    Escape();
  }
  GetEscapedUTF8String(const char16ptr_t aString, uint32_t aLength)
    : NS_ConvertUTF16toUTF8(aString, aLength)
  {
    Escape();
  }

private:
  void Escape()
  {
    ReplaceSubstring("\r", "\\r");
    ReplaceSubstring("\n", "\\n");
    ReplaceSubstring("\t", "\\t");
  }
};

class GetIMEStateString : public nsAutoCString
{
public:
  explicit GetIMEStateString(const IMEState& aIMEState)
  {
    AppendLiteral("{ mEnabled=");
    switch (aIMEState.mEnabled) {
      case IMEState::DISABLED:
        AppendLiteral("DISABLED");
        break;
      case IMEState::ENABLED:
        AppendLiteral("ENABLED");
        break;
      case IMEState::PASSWORD:
        AppendLiteral("PASSWORD");
        break;
      case IMEState::PLUGIN:
        AppendLiteral("PLUGIN");
        break;
      case IMEState::UNKNOWN:
        AppendLiteral("UNKNOWN");
        break;
      default:
        AppendPrintf("Unknown value (%d)", aIMEState.mEnabled);
        break;
    }
    AppendLiteral(", mOpen=");
    switch (aIMEState.mOpen) {
      case IMEState::OPEN_STATE_NOT_SUPPORTED:
        AppendLiteral("OPEN_STATE_NOT_SUPPORTED or DONT_CHANGE_OPEN_STATE");
        break;
      case IMEState::OPEN:
        AppendLiteral("OPEN");
        break;
      case IMEState::CLOSED:
        AppendLiteral("CLOSED");
        break;
      default:
        AppendPrintf("Unknown value (%d)", aIMEState.mOpen);
        break;
    }
    AppendLiteral(" }");
  }
};

class GetInputContextString : public nsAutoCString
{
public:
  explicit GetInputContextString(const InputContext& aInputContext)
  {
    AppendPrintf("{ mIMEState=%s, ",
                 GetIMEStateString(aInputContext.mIMEState).get());
    AppendLiteral("mOrigin=");
    switch (aInputContext.mOrigin) {
      case InputContext::ORIGIN_MAIN:
        AppendLiteral("ORIGIN_MAIN");
        break;
      case InputContext::ORIGIN_CONTENT:
        AppendLiteral("ORIGIN_CONTENT");
        break;
      default:
        AppendPrintf("Unknown value (%d)", aInputContext.mOrigin);
        break;
    }
    AppendPrintf(", mHTMLInputType=\"%s\", mHTMLInputInputmode=\"%s\", "
                 "mActionHint=\"%s\", mMayBeIMEUnaware=%s }",
                 NS_ConvertUTF16toUTF8(aInputContext.mHTMLInputType).get(),
                 NS_ConvertUTF16toUTF8(aInputContext.mHTMLInputInputmode).get(),
                 NS_ConvertUTF16toUTF8(aInputContext.mActionHint).get(),
                 GetBoolName(aInputContext.mMayBeIMEUnaware));
  }
};

class GetInputScopeString : public nsAutoCString
{
public:
  explicit GetInputScopeString(const nsTArray<InputScope>& aList)
  {
    for (InputScope inputScope : aList) {
      if (!IsEmpty()) {
        AppendLiteral(", ");
      }
      switch (inputScope) {
        case IS_DEFAULT:
          AppendLiteral("IS_DEFAULT");
          break;
        case IS_URL:
          AppendLiteral("IS_URL");
          break;
        case IS_FILE_FULLFILEPATH:
          AppendLiteral("IS_FILE_FULLFILEPATH");
          break;
        case IS_FILE_FILENAME:
          AppendLiteral("IS_FILE_FILENAME");
          break;
        case IS_EMAIL_USERNAME:
          AppendLiteral("IS_EMAIL_USERNAME");
          break;
        case IS_EMAIL_SMTPEMAILADDRESS:
          AppendLiteral("IS_EMAIL_SMTPEMAILADDRESS");
          break;
        case IS_LOGINNAME:
          AppendLiteral("IS_LOGINNAME");
          break;
        case IS_PERSONALNAME_FULLNAME:
          AppendLiteral("IS_PERSONALNAME_FULLNAME");
          break;
        case IS_PERSONALNAME_PREFIX:
          AppendLiteral("IS_PERSONALNAME_PREFIX");
          break;
        case IS_PERSONALNAME_GIVENNAME:
          AppendLiteral("IS_PERSONALNAME_GIVENNAME");
          break;
        case IS_PERSONALNAME_MIDDLENAME:
          AppendLiteral("IS_PERSONALNAME_MIDDLENAME");
          break;
        case IS_PERSONALNAME_SURNAME:
          AppendLiteral("IS_PERSONALNAME_SURNAME");
          break;
        case IS_PERSONALNAME_SUFFIX:
          AppendLiteral("IS_PERSONALNAME_SUFFIX");
          break;
        case IS_ADDRESS_FULLPOSTALADDRESS:
          AppendLiteral("IS_ADDRESS_FULLPOSTALADDRESS");
          break;
        case IS_ADDRESS_POSTALCODE:
          AppendLiteral("IS_ADDRESS_POSTALCODE");
          break;
        case IS_ADDRESS_STREET:
          AppendLiteral("IS_ADDRESS_STREET");
          break;
        case IS_ADDRESS_STATEORPROVINCE:
          AppendLiteral("IS_ADDRESS_STATEORPROVINCE");
          break;
        case IS_ADDRESS_CITY:
          AppendLiteral("IS_ADDRESS_CITY");
          break;
        case IS_ADDRESS_COUNTRYNAME:
          AppendLiteral("IS_ADDRESS_COUNTRYNAME");
          break;
        case IS_ADDRESS_COUNTRYSHORTNAME:
          AppendLiteral("IS_ADDRESS_COUNTRYSHORTNAME");
          break;
        case IS_CURRENCY_AMOUNTANDSYMBOL:
          AppendLiteral("IS_CURRENCY_AMOUNTANDSYMBOL");
          break;
        case IS_CURRENCY_AMOUNT:
          AppendLiteral("IS_CURRENCY_AMOUNT");
          break;
        case IS_DATE_FULLDATE:
          AppendLiteral("IS_DATE_FULLDATE");
          break;
        case IS_DATE_MONTH:
          AppendLiteral("IS_DATE_MONTH");
          break;
        case IS_DATE_DAY:
          AppendLiteral("IS_DATE_DAY");
          break;
        case IS_DATE_YEAR:
          AppendLiteral("IS_DATE_YEAR");
          break;
        case IS_DATE_MONTHNAME:
          AppendLiteral("IS_DATE_MONTHNAME");
          break;
        case IS_DATE_DAYNAME:
          AppendLiteral("IS_DATE_DAYNAME");
          break;
        case IS_DIGITS:
          AppendLiteral("IS_DIGITS");
          break;
        case IS_NUMBER:
          AppendLiteral("IS_NUMBER");
          break;
        case IS_ONECHAR:
          AppendLiteral("IS_ONECHAR");
          break;
        case IS_PASSWORD:
          AppendLiteral("IS_PASSWORD");
          break;
        case IS_TELEPHONE_FULLTELEPHONENUMBER:
          AppendLiteral("IS_TELEPHONE_FULLTELEPHONENUMBER");
          break;
        case IS_TELEPHONE_COUNTRYCODE:
          AppendLiteral("IS_TELEPHONE_COUNTRYCODE");
          break;
        case IS_TELEPHONE_AREACODE:
          AppendLiteral("IS_TELEPHONE_AREACODE");
          break;
        case IS_TELEPHONE_LOCALNUMBER:
          AppendLiteral("IS_TELEPHONE_LOCALNUMBER");
          break;
        case IS_TIME_FULLTIME:
          AppendLiteral("IS_TIME_FULLTIME");
          break;
        case IS_TIME_HOUR:
          AppendLiteral("IS_TIME_HOUR");
          break;
        case IS_TIME_MINORSEC:
          AppendLiteral("IS_TIME_MINORSEC");
          break;
        case IS_NUMBER_FULLWIDTH:
          AppendLiteral("IS_NUMBER_FULLWIDTH");
          break;
        case IS_ALPHANUMERIC_HALFWIDTH:
          AppendLiteral("IS_ALPHANUMERIC_HALFWIDTH");
          break;
        case IS_ALPHANUMERIC_FULLWIDTH:
          AppendLiteral("IS_ALPHANUMERIC_FULLWIDTH");
          break;
        case IS_CURRENCY_CHINESE:
          AppendLiteral("IS_CURRENCY_CHINESE");
          break;
        case IS_BOPOMOFO:
          AppendLiteral("IS_BOPOMOFO");
          break;
        case IS_HIRAGANA:
          AppendLiteral("IS_HIRAGANA");
          break;
        case IS_KATAKANA_HALFWIDTH:
          AppendLiteral("IS_KATAKANA_HALFWIDTH");
          break;
        case IS_KATAKANA_FULLWIDTH:
          AppendLiteral("IS_KATAKANA_FULLWIDTH");
          break;
        case IS_HANJA:
          AppendLiteral("IS_HANJA");
          break;
        case IS_PHRASELIST:
          AppendLiteral("IS_PHRASELIST");
          break;
        case IS_REGULAREXPRESSION:
          AppendLiteral("IS_REGULAREXPRESSION");
          break;
        case IS_SRGS:
          AppendLiteral("IS_SRGS");
          break;
        case IS_XML:
          AppendLiteral("IS_XML");
          break;
        default:
          AppendPrintf("Unknown Value(%d)", inputScope);
          break;
      }
    }
  }
};

/******************************************************************/
/* InputScopeImpl                                                 */
/******************************************************************/

class InputScopeImpl final : public ITfInputScope
{
  ~InputScopeImpl() {}

public:
  explicit InputScopeImpl(const nsTArray<InputScope>& aList)
    : mInputScopes(aList)
  {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p InputScopeImpl(%s)", this, GetInputScopeString(aList).get()));
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

  STDMETHODIMP GetPhrase(BSTR **ppbstrPhrases, UINT* pcCount)
  {
    return E_NOTIMPL;
  }
  STDMETHODIMP GetRegularExpression(BSTR* pbstrRegExp) { return E_NOTIMPL; }
  STDMETHODIMP GetSRGS(BSTR* pbstrSRGS) { return E_NOTIMPL; }
  STDMETHODIMP GetXML(BSTR* pbstrXML) { return E_NOTIMPL; }

private:
  nsTArray<InputScope> mInputScopes;
};

/******************************************************************/
/* TSFStaticSink                                                  */
/******************************************************************/

class TSFStaticSink final : public ITfInputProcessorProfileActivationSink
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
        IID_ITfInputProcessorProfileActivationSink == riid) {
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

  /****************************************************************************
   * Japanese TIP
   ****************************************************************************/

  // Note that TIP name may depend on the language of the environment.
  // For example, some TIP may use localized name for its target language
  // environment but English name for the others.

  bool IsMSJapaneseIMEActive() const
  {
    // FYI: Name of MS-IME for Japanese is same as MS-IME for Korean.
    //      Therefore, we need to check the langid too.
    return mLangID == 0x411 &&
      (mActiveTIPKeyboardDescription.EqualsLiteral("Microsoft IME") ||
       mActiveTIPKeyboardDescription.Equals(
         NS_LITERAL_STRING(u"Microsoft \xC785\xB825\xAE30")) ||
       mActiveTIPKeyboardDescription.Equals(
         NS_LITERAL_STRING(u"\x5FAE\x8F6F\x8F93\x5165\x6CD5")) ||
       mActiveTIPKeyboardDescription.Equals(
         NS_LITERAL_STRING(u"\x5FAE\x8EDF\x8F38\x5165\x6CD5")));
  }

  bool IsMSOfficeJapaneseIME2010Active() const
  {
    // {54EDCC94-1524-4BB1-9FB7-7BABE4F4CA64}
    static const GUID kGUID = {
      0x54EDCC94, 0x1524, 0x4BB1,
        { 0x9F, 0xB7, 0x7B, 0xAB, 0xE4, 0xF4, 0xCA, 0x64 }
    };
    return mActiveTIPGUID == kGUID;
  }

  bool IsATOKActive() const
  {
    // FYI: Name of ATOK includes the release year like "ATOK 2015".
    return StringBeginsWith(mActiveTIPKeyboardDescription,
                            NS_LITERAL_STRING("ATOK "));
  }

  bool IsATOK2011Active() const
  {
    // {F9C24A5C-8A53-499D-9572-93B2FF582115}
    static const GUID kGUID = {
      0xF9C24A5C, 0x8A53, 0x499D,
        { 0x95, 0x72, 0x93, 0xB2, 0xFF, 0x58, 0x21, 0x15 }
    };
    return mActiveTIPGUID == kGUID;
  }

  bool IsATOK2012Active() const
  {
    // {1DE01562-F445-401B-B6C3-E5B18DB79461}
    static const GUID kGUID = {
      0x1DE01562, 0xF445, 0x401B,
        { 0xB6, 0xC3, 0xE5, 0xB1, 0x8D, 0xB7, 0x94, 0x61 }
    };
    return mActiveTIPGUID == kGUID;
  }

  bool IsATOK2013Active() const
  {
    // {3C4DB511-189A-4168-B6EA-BFD0B4C85615}
    static const GUID kGUID = {
      0x3C4DB511, 0x189A, 0x4168,
        { 0xB6, 0xEA, 0xBF, 0xD0, 0xB4, 0xC8, 0x56, 0x15 }
    };
    return mActiveTIPGUID == kGUID;
  }

  bool IsATOK2014Active() const
  {
    // {4EF33B79-6AA9-4271-B4BF-9321C279381B}
    static const GUID kGUID = {
      0x4EF33B79, 0x6AA9, 0x4271,
        { 0xB4, 0xBF, 0x93, 0x21, 0xC2, 0x79, 0x38, 0x1B }
    };
    return mActiveTIPGUID == kGUID;
  }

  bool IsATOK2015Active() const
  {
    // {EAB4DC00-CE2E-483D-A86A-E6B99DA9599A}
    static const GUID kGUID = {
      0xEAB4DC00, 0xCE2E, 0x483D,
        { 0xA8, 0x6A, 0xE6, 0xB9, 0x9D, 0xA9, 0x59, 0x9A }
    };
    return mActiveTIPGUID == kGUID;
  }

  bool IsATOK2016Active() const
  {
    // {0B557B4C-5740-4110-A60A-1493FA10BF2B}
    static const GUID kGUID = {
      0x0B557B4C, 0x5740, 0x4110,
        { 0xA6, 0x0A, 0x14, 0x93, 0xFA, 0x10, 0xBF, 0x2B }
    };
    return mActiveTIPGUID == kGUID;
  }

  // Note that ATOK 2011 - 2016 refers native caret position for deciding its
  // popup window position.
  bool IsATOKReferringNativeCaretActive() const
  {
    return IsATOKActive() &&
           (IsATOK2011Active() || IsATOK2012Active() || IsATOK2013Active() ||
            IsATOK2014Active() || IsATOK2015Active() || IsATOK2016Active());
  }

  /****************************************************************************
   * Traditional Chinese TIP
   ****************************************************************************/

  bool IsMSChangJieActive() const
  {
    return mActiveTIPKeyboardDescription.EqualsLiteral("Microsoft ChangJie") ||
      mActiveTIPKeyboardDescription.Equals(
        NS_LITERAL_STRING(u"\x5FAE\x8F6F\x4ED3\x9889")) ||
      mActiveTIPKeyboardDescription.Equals(
        NS_LITERAL_STRING(u"\x5FAE\x8EDF\x5009\x9821"));
  }

  bool IsMSQuickQuickActive() const
  {
    return mActiveTIPKeyboardDescription.EqualsLiteral("Microsoft Quick") ||
      mActiveTIPKeyboardDescription.Equals(
        NS_LITERAL_STRING(u"\x5FAE\x8F6F\x901F\x6210")) ||
      mActiveTIPKeyboardDescription.Equals(
        NS_LITERAL_STRING(u"\x5FAE\x8EDF\x901F\x6210"));
  }

  bool IsFreeChangJieActive() const
  {
    // FYI: The TIP name is misspelled...
    return mActiveTIPKeyboardDescription.EqualsLiteral("Free CangJie IME 10");
  }

  bool IsEasyChangjeiActive() const
  {
    return
      mActiveTIPKeyboardDescription.Equals(
        NS_LITERAL_STRING(
          u"\x4E2D\x6587 (\x7E41\x9AD4) - \x6613\x9821\x8F38\x5165\x6CD5"));
  }

  /****************************************************************************
   * Simplified Chinese TIP
   ****************************************************************************/

  bool IsMSPinyinActive() const
  {
    return mActiveTIPKeyboardDescription.EqualsLiteral("Microsoft Pinyin") ||
      mActiveTIPKeyboardDescription.Equals(
        NS_LITERAL_STRING(u"\x5FAE\x8F6F\x62FC\x97F3")) ||
      mActiveTIPKeyboardDescription.Equals(
        NS_LITERAL_STRING(u"\x5FAE\x8EDF\x62FC\x97F3"));
  }

  bool IsMSWubiActive() const
  {
    return mActiveTIPKeyboardDescription.EqualsLiteral("Microsoft Wubi") ||
      mActiveTIPKeyboardDescription.Equals(
        NS_LITERAL_STRING(u"\x5FAE\x8F6F\x4E94\x7B14")) ||
      mActiveTIPKeyboardDescription.Equals(
        NS_LITERAL_STRING(u"\x5FAE\x8EDF\x4E94\x7B46"));
  }

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

  LANGID mLangID;

  // True if current IME is implemented with IMM.
  bool mIsIMM_IME;
  // True if OnActivated() is already called
  bool mOnActivatedCalled;

  RefPtr<ITfThreadMgr> mThreadMgr;
  RefPtr<ITfInputProcessorProfiles> mInputProcessorProfiles;

  // Active TIP keyboard's description.  If active language profile isn't TIP,
  // i.e., IMM-IME or just a keyboard layout, this is empty.
  nsString mActiveTIPKeyboardDescription;

  // Active TIP's GUID
  GUID mActiveTIPGUID;

  static StaticRefPtr<TSFStaticSink> sInstance;
};

StaticRefPtr<TSFStaticSink> TSFStaticSink::sInstance;

TSFStaticSink::TSFStaticSink()
  : mIPProfileCookie(TF_INVALID_COOKIE)
  , mLangID(0)
  , mIsIMM_IME(false)
  , mOnActivatedCalled(false)
  , mActiveTIPGUID(GUID_NULL)
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

  RefPtr<ITfSource> source;
  HRESULT hr =
    mThreadMgr->QueryInterface(IID_ITfSource, getter_AddRefs(source));
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p TSFStaticSink::Init() FAILED to get ITfSource "
       "instance (0x%08X)", this, hr));
    return false;
  }

  // NOTE: On Vista or later, Windows let us know activate IME changed only
  //       with ITfInputProcessorProfileActivationSink.
  hr = source->AdviseSink(IID_ITfInputProcessorProfileActivationSink,
                 static_cast<ITfInputProcessorProfileActivationSink*>(this),
                 &mIPProfileCookie);
  if (FAILED(hr) || mIPProfileCookie == TF_INVALID_COOKIE) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p TSFStaticSink::Init() FAILED to install "
       "ITfInputProcessorProfileActivationSink (0x%08X)", this, hr));
    return false;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFStaticSink::Init(), "
     "mIPProfileCookie=0x%08X",
     this, mIPProfileCookie));
  return true;
}

void
TSFStaticSink::Destroy()
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFStaticSink::Shutdown() "
     "mIPProfileCookie=0x%08X",
     this, mIPProfileCookie));

  if (mIPProfileCookie != TF_INVALID_COOKIE) {
    RefPtr<ITfSource> source;
    HRESULT hr =
      mThreadMgr->QueryInterface(IID_ITfSource, getter_AddRefs(source));
    if (FAILED(hr)) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFStaticSink::Shutdown() FAILED to get "
         "ITfSource instance (0x%08X)", this, hr));
    } else {
      hr = source->UnadviseSink(mIPProfileCookie);
      if (FAILED(hr)) {
        MOZ_LOG(sTextStoreLog, LogLevel::Error,
          ("0x%p   TSFTextStore::Shutdown() FAILED to uninstall "
           "ITfInputProcessorProfileActivationSink (0x%08X)",
           this, hr));
      }
    }
  }

  mThreadMgr = nullptr;
  mInputProcessorProfiles = nullptr;
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
  if ((dwFlags & TF_IPSINK_FLAG_ACTIVE) &&
      (dwProfileType == TF_PROFILETYPE_KEYBOARDLAYOUT ||
       catid == GUID_TFCAT_TIP_KEYBOARD)) {
    mOnActivatedCalled = true;
    mActiveTIPGUID = guidProfile;
    mLangID = langid;
    mIsIMM_IME = IsIMM_IME(hkl);
    GetTIPDescription(rclsid, mLangID, guidProfile,
                      mActiveTIPKeyboardDescription);
  }
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFStaticSink::OnActivated(dwProfileType=%s (0x%08X), "
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

  RefPtr<ITfInputProcessorProfileMgr> profileMgr;
  HRESULT hr =
    mInputProcessorProfiles->QueryInterface(IID_ITfInputProcessorProfileMgr,
                                            getter_AddRefs(profileMgr));
  if (FAILED(hr) || !profileMgr) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
       "to get input processor profile manager, hr=0x%08X", this, hr));
    return false;
  }

  TF_INPUTPROCESSORPROFILE profile;
  hr = profileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &profile);
  if (hr == S_FALSE) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
       "to get active keyboard layout profile due to no active profile, "
       "hr=0x%08X", this, hr));
    // XXX Should we call OnActivated() with arguments like non-TIP in this
    //     case?
    return false;
  }
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
       "to get active TIP keyboard, hr=0x%08X", this, hr));
    return false;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), "
     "calling OnActivated() manually...", this));
  OnActivated(profile.dwProfileType, profile.langid, profile.clsid,
              profile.catid, profile.guidProfile, ::GetKeyboardLayout(0),
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
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFStaticSink::InitActiveTIPDescription() FAILED "
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

  RefPtr<IEnumTfLanguageProfiles> enumLangProfiles;
  HRESULT hr =
    mInputProcessorProfiles->EnumLanguageProfiles(aLangID,
                               getter_AddRefs(enumLangProfiles));
  if (FAILED(hr) || !enumLangProfiles) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFStaticSink::IsTIPCategoryKeyboard(), FAILED "
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
/* TSFTextStore                                                   */
/******************************************************************/

StaticRefPtr<ITfThreadMgr> TSFTextStore::sThreadMgr;
StaticRefPtr<ITfMessagePump> TSFTextStore::sMessagePump;
StaticRefPtr<ITfKeystrokeMgr> TSFTextStore::sKeystrokeMgr;
StaticRefPtr<ITfDisplayAttributeMgr> TSFTextStore::sDisplayAttrMgr;
StaticRefPtr<ITfCategoryMgr> TSFTextStore::sCategoryMgr;
StaticRefPtr<ITfDocumentMgr> TSFTextStore::sDisabledDocumentMgr;
StaticRefPtr<ITfContext> TSFTextStore::sDisabledContext;
StaticRefPtr<ITfInputProcessorProfiles> TSFTextStore::sInputProcessorProfiles;
StaticRefPtr<TSFTextStore> TSFTextStore::sEnabledTextStore;
DWORD TSFTextStore::sClientId  = 0;

bool TSFTextStore::sCreateNativeCaretForLegacyATOK = false;
bool TSFTextStore::sDoNotReturnNoLayoutErrorToATOKOfCompositionString = false;
bool TSFTextStore::sDoNotReturnNoLayoutErrorToMSSimplifiedTIP = false;
bool TSFTextStore::sDoNotReturnNoLayoutErrorToMSTraditionalTIP = false;
bool TSFTextStore::sDoNotReturnNoLayoutErrorToFreeChangJie = false;
bool TSFTextStore::sDoNotReturnNoLayoutErrorToEasyChangjei = false;
bool TSFTextStore::sDoNotReturnNoLayoutErrorToMSJapaneseIMEAtFirstChar = false;
bool TSFTextStore::sDoNotReturnNoLayoutErrorToMSJapaneseIMEAtCaret = false;
bool TSFTextStore::sHackQueryInsertForMSSimplifiedTIP = false;
bool TSFTextStore::sHackQueryInsertForMSTraditionalTIP = false;

#define TEXTSTORE_DEFAULT_VIEW (1)

TSFTextStore::TSFTextStore()
  : mEditCookie(0)
  , mSinkMask(0)
  , mLock(0)
  , mLockQueued(0)
  , mHandlingKeyMessage(0)
  , mContentForTSF(mComposition, mSelectionForTSF)
  , mRequestedAttrValues(false)
  , mIsRecordingActionsWithoutLock(false)
  , mHasReturnedNoLayoutError(false)
  , mWaitingQueryLayout(false)
  , mPendingDestroy(false)
  , mDeferClearingContentForTSF(false)
  , mNativeCaretIsCreated(false)
  , mDeferNotifyingTSF(false)
  , mDeferCommittingComposition(false)
  , mDeferCancellingComposition(false)
  , mDestroyed(false)
  , mBeingDestroyed(false)
{
  for (int32_t i = 0; i < NUM_OF_SUPPORTED_ATTRS; i++) {
    mRequestedAttrs[i] = false;
  }

  // We hope that 5 or more actions don't occur at once.
  mPendingActions.SetCapacity(5);

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::TSFTextStore() SUCCEEDED", this));
}

TSFTextStore::~TSFTextStore()
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore instance is destroyed", this));
}

bool
TSFTextStore::Init(nsWindowBase* aWidget,
                   const InputContext& aContext)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::Init(aWidget=0x%p)",
     this, aWidget));

  TSFStaticSink::GetInstance()->EnsureInitActiveTIPKeyboard();

  if (mDocumentMgr) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::Init() FAILED due to already initialized",
       this));
    return false;
  }

  // Create document manager
  HRESULT hr = sThreadMgr->CreateDocumentMgr(getter_AddRefs(mDocumentMgr));
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::Init() FAILED to create DocumentMgr "
       "(0x%08X)", this, hr));
    return false;
  }
  mWidget = aWidget;
  if (NS_WARN_IF(!mWidget)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::Init() FAILED "
       "due to aWidget is nullptr ", this));
    return false;
  }
  mDispatcher = mWidget->GetTextEventDispatcher();
  if (NS_WARN_IF(!mDispatcher)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::Init() FAILED "
       "due to aWidget->GetTextEventDispatcher() failure", this));
    return false;
  }

  // Create context and add it to document manager
  hr = mDocumentMgr->CreateContext(sClientId, 0,
                                   static_cast<ITextStoreACP*>(this),
                                   getter_AddRefs(mContext), &mEditCookie);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::Init() FAILED to create the context "
       "(0x%08X)", this, hr));
    mDocumentMgr = nullptr;
    return false;
  }

  SetInputScope(aContext.mHTMLInputType, aContext.mHTMLInputInputmode);

  hr = mDocumentMgr->Push(mContext);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::Init() FAILED to push the context (0x%08X)",
       this, hr));
    // XXX Why don't we use NS_IF_RELEASE() here??
    mContext = nullptr;
    mDocumentMgr = nullptr;
    return false;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::Init() succeeded: "
     "mDocumentMgr=0x%p, mContext=0x%p, mEditCookie=0x%08X",
     this, mDocumentMgr.get(), mContext.get(), mEditCookie));

  return true;
}

void
TSFTextStore::Destroy()
{
  if (mBeingDestroyed) {
    return;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::Destroy(), mLock=%s, "
     "mComposition.IsComposing()=%s, mHandlingKeyMessage=%u",
     this, GetLockFlagNameStr(mLock).get(),
     GetBoolName(mComposition.IsComposing()),
     mHandlingKeyMessage));

  mDestroyed = true;

  // Destroy native caret first because it's not directly related to TSF and
  // there may be another textstore which gets focus.  So, we should avoid
  // to destroy caret after the new one recreates caret.
  MaybeDestroyNativeCaret();

  if (mLock) {
    mPendingDestroy = true;
    return;
  }

  AutoRestore<bool> savedBeingDestroyed(mBeingDestroyed);
  mBeingDestroyed = true;

  // If there is composition, TSF keeps the composition even after the text
  // store destroyed.  So, we should clear the composition here.
  if (mComposition.IsComposing()) {
    CommitCompositionInternal(false);
  }

  if (mSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::Destroy(), calling "
       "ITextStoreACPSink::OnLayoutChange(TS_LC_DESTROY)...",
       this));
    mSink->OnLayoutChange(TS_LC_DESTROY, TEXTSTORE_DEFAULT_VIEW);
  }

  // If this is called during handling a keydown or keyup message, we should
  // put off to release TSF objects until it completely finishes since
  // MS-IME for Japanese refers some objects without grabbing them.
  if (!mHandlingKeyMessage) {
    ReleaseTSFObjects();
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::Destroy() succeeded", this));
}

void
TSFTextStore::ReleaseTSFObjects()
{
  MOZ_ASSERT(!mHandlingKeyMessage);

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::ReleaseTSFObjects()", this));

  mContext = nullptr;
  if (mDocumentMgr) {
    mDocumentMgr->Pop(TF_POPF_ALL);
    mDocumentMgr = nullptr;
  }
  mSink = nullptr;
  mWidget = nullptr;
  mDispatcher = nullptr;

  if (!mMouseTrackers.IsEmpty()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::ReleaseTSFObjects(), "
       "removing a mouse tracker...",
       this));
    mMouseTrackers.Clear();
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::ReleaseTSFObjects() completed", this));
}

STDMETHODIMP
TSFTextStore::QueryInterface(REFIID riid,
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

  MOZ_LOG(sTextStoreLog, LogLevel::Error,
    ("0x%p TSFTextStore::QueryInterface() FAILED, riid=%s",
     this, GetRIIDNameStr(riid).get()));
  return E_NOINTERFACE;
}

STDMETHODIMP
TSFTextStore::AdviseSink(REFIID riid,
                         IUnknown* punk,
                         DWORD dwMask)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::AdviseSink(riid=%s, punk=0x%p, dwMask=%s), "
     "mSink=0x%p, mSinkMask=%s",
     this, GetRIIDNameStr(riid).get(), punk, GetSinkMaskNameStr(dwMask).get(),
     mSink.get(), GetSinkMaskNameStr(mSinkMask).get()));

  if (!punk) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::AdviseSink() FAILED due to the null punk",
       this));
    return E_UNEXPECTED;
  }

  if (IID_ITextStoreACPSink != riid) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::AdviseSink() FAILED due to "
       "unsupported interface", this));
    return E_INVALIDARG; // means unsupported interface.
  }

  if (!mSink) {
    // Install sink
    punk->QueryInterface(IID_ITextStoreACPSink, getter_AddRefs(mSink));
    if (!mSink) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::AdviseSink() FAILED due to "
         "punk not having the interface", this));
      return E_UNEXPECTED;
    }
  } else {
    // If sink is already installed we check to see if they are the same
    // Get IUnknown from both sides for comparison
    RefPtr<IUnknown> comparison1, comparison2;
    punk->QueryInterface(IID_IUnknown, getter_AddRefs(comparison1));
    mSink->QueryInterface(IID_IUnknown, getter_AddRefs(comparison2));
    if (comparison1 != comparison2) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::AdviseSink() FAILED due to "
         "the sink being different from the stored sink", this));
      return CONNECT_E_ADVISELIMIT;
    }
  }
  // Update mask either for a new sink or an existing sink
  mSinkMask = dwMask;
  return S_OK;
}

STDMETHODIMP
TSFTextStore::UnadviseSink(IUnknown* punk)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::UnadviseSink(punk=0x%p), mSink=0x%p",
     this, punk, mSink.get()));

  if (!punk) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::UnadviseSink() FAILED due to the null punk",
       this));
    return E_INVALIDARG;
  }
  if (!mSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::UnadviseSink() FAILED due to "
       "any sink not stored", this));
    return CONNECT_E_NOCONNECTION;
  }
  // Get IUnknown from both sides for comparison
  RefPtr<IUnknown> comparison1, comparison2;
  punk->QueryInterface(IID_IUnknown, getter_AddRefs(comparison1));
  mSink->QueryInterface(IID_IUnknown, getter_AddRefs(comparison2));
  // Unadvise only if sinks are the same
  if (comparison1 != comparison2) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::UnadviseSink() FAILED due to "
       "the sink being different from the stored sink", this));
    return CONNECT_E_NOCONNECTION;
  }
  mSink = nullptr;
  mSinkMask = 0;
  return S_OK;
}

STDMETHODIMP
TSFTextStore::RequestLock(DWORD dwLockFlags,
                          HRESULT* phrSession)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::RequestLock(dwLockFlags=%s, phrSession=0x%p), "
     "mLock=%s, mDestroyed=%s", this, GetLockFlagNameStr(dwLockFlags).get(),
     phrSession, GetLockFlagNameStr(mLock).get(), GetBoolName(mDestroyed)));

  if (!mSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RequestLock() FAILED due to "
       "any sink not stored", this));
    return E_FAIL;
  }
  if (mDestroyed &&
      (!mContentForTSF.IsInitialized() || mSelectionForTSF.IsDirty())) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RequestLock() FAILED due to "
       "being destroyed and no information of the contents", this));
    return E_FAIL;
  }
  if (!phrSession) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RequestLock() FAILED due to "
       "null phrSession", this));
    return E_INVALIDARG;
  }

  if (!mLock) {
    // put on lock
    mLock = dwLockFlags & (~TS_LF_SYNC);
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   Locking (%s) >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
       ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>",
       this, GetLockFlagNameStr(mLock).get()));
    // Don't release this instance during this lock because this is called by
    // TSF but they don't grab us during this call.
    RefPtr<TSFTextStore> kungFuDeathGrip(this);
    *phrSession = mSink->OnLockGranted(mLock);
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   Unlocked (%s) <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
       "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<",
       this, GetLockFlagNameStr(mLock).get()));
    DidLockGranted();
    while (mLockQueued) {
      mLock = mLockQueued;
      mLockQueued = 0;
      MOZ_LOG(sTextStoreLog, LogLevel::Info,
        ("0x%p   Locking for the request in the queue (%s) >>>>>>>>>>>>>>"
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>",
         this, GetLockFlagNameStr(mLock).get()));
      mSink->OnLockGranted(mLock);
      MOZ_LOG(sTextStoreLog, LogLevel::Info,
        ("0x%p   Unlocked (%s) <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
         "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<",
         this, GetLockFlagNameStr(mLock).get()));
      DidLockGranted();
    }

    // The document is now completely unlocked.
    mLock = 0;

    MaybeFlushPendingNotifications();

    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::RequestLock() succeeded: *phrSession=%s",
       this, GetTextStoreReturnValueName(*phrSession)));
    return S_OK;
  }

  // only time when reentrant lock is allowed is when caller holds a
  // read-only lock and is requesting an async write lock
  if (IsReadLocked() && !IsReadWriteLocked() && IsReadWriteLock(dwLockFlags) &&
      !(dwLockFlags & TS_LF_SYNC)) {
    *phrSession = TS_S_ASYNC;
    mLockQueued = dwLockFlags & (~TS_LF_SYNC);

    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::RequestLock() stores the request in the "
       "queue, *phrSession=TS_S_ASYNC", this));
    return S_OK;
  }

  // no more locks allowed
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::RequestLock() didn't allow to lock, "
     "*phrSession=TS_E_SYNCHRONOUS", this));
  *phrSession = TS_E_SYNCHRONOUS;
  return E_FAIL;
}

void
TSFTextStore::DidLockGranted()
{
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
  if (mDestroyed || !mWidget || mWidget->Destroyed()) {
    mPendingSelectionChangeData.Clear();
    mHasReturnedNoLayoutError = false;
  }
}

void
TSFTextStore::DispatchEvent(WidgetGUIEvent& aEvent)
{
  if (NS_WARN_IF(!mWidget) || NS_WARN_IF(mWidget->Destroyed())) {
    return;
  }
  // If the event isn't a query content event, the event may be handled
  // asynchronously.  So, we should put off to answer from GetTextExt() etc.
  if (!aEvent.AsQueryContentEvent()) {
    mDeferNotifyingTSF = true;
  }
  mWidget->DispatchWindowEvent(&aEvent);
}

void
TSFTextStore::FlushPendingActions()
{
  if (!mWidget || mWidget->Destroyed()) {
    // Note that don't clear mContentForTSF because TIP may try to commit
    // composition with a document lock.  In such case, TSFTextStore needs to
    // behave as expected by TIP.
    mPendingActions.Clear();
    mPendingSelectionChangeData.Clear();
    mHasReturnedNoLayoutError = false;
    return;
  }

  RefPtr<nsWindowBase> widget(mWidget);
  nsresult rv = mDispatcher->BeginNativeInputTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::FlushPendingActions() "
       "FAILED due to BeginNativeInputTransaction() failure", this));
    return;
  }
  for (uint32_t i = 0; i < mPendingActions.Length(); i++) {
    PendingAction& action = mPendingActions[i];
    switch (action.mType) {
      case PendingAction::COMPOSITION_START: {
        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::FlushPendingActions() "
           "flushing COMPOSITION_START={ mSelectionStart=%d, "
           "mSelectionLength=%d }, mDestroyed=%s",
           this, action.mSelectionStart, action.mSelectionLength,
           GetBoolName(mDestroyed)));

        if (mDestroyed) {
          MOZ_LOG(sTextStoreLog, LogLevel::Warning,
            ("0x%p   TSFTextStore::FlushPendingActions() "
             "IGNORED pending compositionstart due to already destroyed",
             this));
          break;
        }

        if (action.mAdjustSelection) {
          // Select composition range so the new composition replaces the range
          WidgetSelectionEvent selectionSet(true, eSetSelection, widget);
          widget->InitEvent(selectionSet);
          selectionSet.mOffset = static_cast<uint32_t>(action.mSelectionStart);
          selectionSet.mLength = static_cast<uint32_t>(action.mSelectionLength);
          selectionSet.mReversed = false;
          DispatchEvent(selectionSet);
          if (!selectionSet.mSucceeded) {
            MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::FlushPendingActions() "
               "FAILED due to eSetSelection failure", this));
            break;
          }
        }

        // eCompositionStart always causes
        // NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED.  Therefore, we should
        // wait to clear mContentForTSF until it's notified.
        mDeferClearingContentForTSF = true;

        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::FlushPendingActions() "
           "dispatching compositionstart event...", this));
        WidgetEventTime eventTime = widget->CurrentMessageWidgetEventTime();
        nsEventStatus status;
        rv = mDispatcher->StartComposition(status, &eventTime);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::FlushPendingActions() "
             "FAILED to dispatch compositionstart event, "
             "IsComposingInContent()=%s",
             this, GetBoolName(!IsComposingInContent())));
          mDeferClearingContentForTSF = !IsComposingInContent();
        }
        if (!widget || widget->Destroyed()) {
          break;
        }
        break;
      }
      case PendingAction::COMPOSITION_UPDATE: {
        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::FlushPendingActions() "
           "flushing COMPOSITION_UPDATE={ mData=\"%s\", "
           "mRanges=0x%p, mRanges->Length()=%d }",
           this, GetEscapedUTF8String(action.mData).get(),
           action.mRanges.get(),
           action.mRanges ? action.mRanges->Length() : 0));

        // eCompositionChange causes a DOM text event, the IME will be notified
        // of NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED.  In this case, we
        // should not clear mContentForTSF until we notify the IME of the
        // composition update.
        mDeferClearingContentForTSF = true;

        rv = mDispatcher->SetPendingComposition(action.mData,
                                                action.mRanges);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::FlushPendingActions() "
             "FAILED to setting pending composition... "
             "IsComposingInContent()=%s",
             this, GetBoolName(IsComposingInContent())));
          mDeferClearingContentForTSF = !IsComposingInContent();
        } else {
          MOZ_LOG(sTextStoreLog, LogLevel::Debug,
            ("0x%p   TSFTextStore::FlushPendingActions() "
             "dispatching compositionchange event...", this));
          WidgetEventTime eventTime = widget->CurrentMessageWidgetEventTime();
          nsEventStatus status;
          rv = mDispatcher->FlushPendingComposition(status, &eventTime);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::FlushPendingActions() "
               "FAILED to dispatch compositionchange event, "
               "IsComposingInContent()=%s",
               this, GetBoolName(IsComposingInContent())));
            mDeferClearingContentForTSF = !IsComposingInContent();
          }
          // Be aware, the mWidget might already have been destroyed.
        }
        break;
      }
      case PendingAction::COMPOSITION_END: {
        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::FlushPendingActions() "
           "flushing COMPOSITION_END={ mData=\"%s\" }",
           this, GetEscapedUTF8String(action.mData).get()));

        // Dispatching eCompositionCommit causes a DOM text event, then,
        // the IME will be notified of NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED.
        // In this case, we should not clear mContentForTSFuntil we notify
        // the IME of the composition update.
        mDeferClearingContentForTSF = true;

        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::FlushPendingActions(), "
           "dispatching compositioncommit event...", this));
        WidgetEventTime eventTime = widget->CurrentMessageWidgetEventTime();
        nsEventStatus status;
        rv = mDispatcher->CommitComposition(status, &action.mData, &eventTime);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::FlushPendingActions() "
             "FAILED to dispatch compositioncommit event, "
             "IsComposingInContent()=%s",
             this, GetBoolName(IsComposingInContent())));
          mDeferClearingContentForTSF = !IsComposingInContent();
        }
        break;
      }
      case PendingAction::SET_SELECTION: {
        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::FlushPendingActions() "
           "flushing SET_SELECTION={ mSelectionStart=%d, "
           "mSelectionLength=%d, mSelectionReversed=%s }, "
           "mDestroyed=%s",
           this, action.mSelectionStart, action.mSelectionLength,
           GetBoolName(action.mSelectionReversed),
           GetBoolName(mDestroyed)));

        if (mDestroyed) {
          MOZ_LOG(sTextStoreLog, LogLevel::Warning,
            ("0x%p   TSFTextStore::FlushPendingActions() "
             "IGNORED pending selectionset due to already destroyed",
             this));
          break;
        }

        WidgetSelectionEvent selectionSet(true, eSetSelection, widget);
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

    if (widget && !widget->Destroyed()) {
      continue;
    }

    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::FlushPendingActions(), "
       "qutting since the mWidget has gone", this));
    break;
  }
  mPendingActions.Clear();
}

void
TSFTextStore::MaybeFlushPendingNotifications()
{
  if (IsReadLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
       "putting off flushing pending notifications due to being the "
       "document locked...", this));
    return;
  }

  if (mDeferCommittingComposition) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
       "calling TSFTextStore::CommitCompositionInternal(false)...", this));
    mDeferCommittingComposition = mDeferCancellingComposition = false;
    CommitCompositionInternal(false);
  } else if (mDeferCancellingComposition) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
       "calling TSFTextStore::CommitCompositionInternal(true)...", this));
    mDeferCommittingComposition = mDeferCancellingComposition = false;
    CommitCompositionInternal(true);
  }

  if (mDeferNotifyingTSF) {
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
       "putting off flushing pending notifications due to being "
       "dispatching events...", this));
    return;
  }

  if (mPendingDestroy) {
    Destroy();
    return;
  }

  if (mDestroyed) {
    // If it's already been destroyed completely, this shouldn't notify TSF of
    // anything anymore.
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
       "does nothing because this has already destroyed completely...", this));
    return;
  }

  if (!mDeferClearingContentForTSF && mContentForTSF.IsInitialized()) {
    mContentForTSF.Clear();
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
       "mContentForTSF is cleared", this));
  }

  // When there is no cached content, we can sync actual contents and TSF/TIP
  // expecting contents.
  if (!mContentForTSF.IsInitialized()) {
    if (mPendingTextChangeData.IsValid()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Info,
        ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
         "calling TSFTextStore::NotifyTSFOfTextChange()...", this));
      NotifyTSFOfTextChange();
    }
    if (mPendingSelectionChangeData.IsValid()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Info,
        ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
         "calling TSFTextStore::NotifyTSFOfSelectionChange()...", this));
      NotifyTSFOfSelectionChange();
    }
  }

  if (mHasReturnedNoLayoutError) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
       "calling TSFTextStore::NotifyTSFOfLayoutChange()...", this));
    NotifyTSFOfLayoutChange();
  }
}

STDMETHODIMP
TSFTextStore::GetStatus(TS_STATUS* pdcs)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::GetStatus(pdcs=0x%p)", this, pdcs));

  if (!pdcs) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetStatus() FAILED due to null pdcs", this));
    return E_INVALIDARG;
  }
  pdcs->dwDynamicFlags = 0;
  // we use a "flat" text model for TSF support so no hidden text
  pdcs->dwStaticFlags = TS_SS_NOHIDDENTEXT;
  return S_OK;
}

STDMETHODIMP
TSFTextStore::QueryInsert(LONG acpTestStart,
                          LONG acpTestEnd,
                          ULONG cch,
                          LONG* pacpResultStart,
                          LONG* pacpResultEnd)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::QueryInsert(acpTestStart=%ld, "
     "acpTestEnd=%ld, cch=%lu, pacpResultStart=0x%p, pacpResultEnd=0x%p)",
     this, acpTestStart, acpTestEnd, cch, acpTestStart, acpTestEnd));

  if (!pacpResultStart || !pacpResultEnd) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::QueryInsert() FAILED due to "
       "the null argument", this));
    return E_INVALIDARG;
  }

  if (acpTestStart < 0 || acpTestStart > acpTestEnd) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::QueryInsert() FAILED due to "
       "wrong argument", this));
    return E_INVALIDARG;
  }

  // XXX need to adjust to cluster boundary
  // Assume we are given good offsets for now
  const TSFStaticSink* kSink = TSFStaticSink::GetInstance();
  if (IsWin8OrLater() && !mComposition.IsComposing() &&
      ((sHackQueryInsertForMSTraditionalTIP &&
         (kSink->IsMSChangJieActive() || kSink->IsMSQuickQuickActive())) ||
       (sHackQueryInsertForMSSimplifiedTIP &&
         (kSink->IsMSPinyinActive() || kSink->IsMSWubiActive())))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Warning,
      ("0x%p   TSFTextStore::QueryInsert() WARNING using different "
       "result for the TIP", this));
    // Chinese TIPs of Microsoft assume that QueryInsert() returns selected
    // range which should be removed.
    *pacpResultStart = acpTestStart;
    *pacpResultEnd = acpTestEnd;
  } else {
    *pacpResultStart = acpTestStart;
    *pacpResultEnd = acpTestStart + cch;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p  TSFTextStore::QueryInsert() succeeded: "
     "*pacpResultStart=%ld, *pacpResultEnd=%ld)",
     this, *pacpResultStart, *pacpResultEnd));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::GetSelection(ULONG ulIndex,
                           ULONG ulCount,
                           TS_SELECTION_ACP* pSelection,
                           ULONG* pcFetched)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::GetSelection(ulIndex=%lu, ulCount=%lu, "
     "pSelection=0x%p, pcFetched=0x%p)",
     this, ulIndex, ulCount, pSelection, pcFetched));

  if (!IsReadLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetSelection() FAILED due to not locked",
       this));
    return TS_E_NOLOCK;
  }
  if (!ulCount || !pSelection || !pcFetched) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetSelection() FAILED due to "
       "null argument", this));
    return E_INVALIDARG;
  }

  *pcFetched = 0;

  if (ulIndex != static_cast<ULONG>(TS_DEFAULT_SELECTION) &&
      ulIndex != 0) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetSelection() FAILED due to "
       "unsupported selection", this));
    return TS_E_NOSELECTION;
  }

  Selection& selectionForTSF = SelectionForTSFRef();
  if (selectionForTSF.IsDirty()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetSelection() FAILED due to "
       "SelectionForTSFRef() failure", this));
    return E_FAIL;
  }
  *pSelection = selectionForTSF.ACP();
  *pcFetched = 1;
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::GetSelection() succeeded", this));
  return S_OK;
}

bool
TSFTextStore::IsComposingInContent() const
{
  if (!mDispatcher) {
    return false;
  }
  if (!mDispatcher->IsInNativeInputTransaction()) {
    return false;
  }
  return mDispatcher->IsComposing();
}

TSFTextStore::Content&
TSFTextStore::ContentForTSFRef()
{
  // This should be called when the document is locked or the content hasn't
  // been abandoned yet.
  if (NS_WARN_IF(!IsReadLocked() && !mContentForTSF.IsInitialized())) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::ContentForTSFRef(), FAILED, due to "
       "called wrong timing, IsReadLocked()=%s, "
       "mContentForTSF.IsInitialized()=%s",
       this, GetBoolName(IsReadLocked()),
       GetBoolName(mContentForTSF.IsInitialized())));
    mContentForTSF.Clear();
    return mContentForTSF;
  }

  Selection& selectionForTSF = SelectionForTSFRef();
  if (selectionForTSF.IsDirty()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::ContentForTSFRef(), FAILED, due to "
       "SelectionForTSFRef() failure", this));
    mContentForTSF.Clear();
    return mContentForTSF;
  }

  if (!mContentForTSF.IsInitialized()) {
    nsAutoString text;
    if (NS_WARN_IF(!GetCurrentText(text))) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::ContentForTSFRef(), FAILED, due to "
         "GetCurrentText() failure", this));
      mContentForTSF.Clear();
      return mContentForTSF;
    }

    mContentForTSF.Init(text);
    // Basically, the cached content which is expected by TSF/TIP should be
    // cleared after active composition is committed or the document lock is
    // unlocked.  However, in e10s mode, content will be modified
    // asynchronously.  In such case, mDeferClearingContentForTSF may be
    // true until whole dispatched events are handled by the focused editor.
    mDeferClearingContentForTSF = false;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::ContentForTSFRef(): "
     "mContentForTSF={ mText=\"%s\" (Length()=%u), "
     "mLastCompositionString=\"%s\" (Length()=%u), "
     "mMinTextModifiedOffset=%u }",
     this, mContentForTSF.Text().Length() <= 40 ?
       GetEscapedUTF8String(mContentForTSF.Text()).get() : "<omitted>",
     mContentForTSF.Text().Length(),
     GetEscapedUTF8String(mContentForTSF.LastCompositionString()).get(),
     mContentForTSF.LastCompositionString().Length(),
     mContentForTSF.MinTextModifiedOffset()));

  return mContentForTSF;
}

bool
TSFTextStore::CanAccessActualContentDirectly() const
{
  if (!mContentForTSF.IsInitialized() || mSelectionForTSF.IsDirty()) {
    return true;
  }

  // If the cached content has been changed by something except composition,
  // the content cache may be different from actual content.
  if (mPendingTextChangeData.IsValid() &&
      !mPendingTextChangeData.mCausedOnlyByComposition) {
    return false;
  }

  // If the cached selection isn't changed, cached content and actual content
  // should be same.
  if (!mPendingSelectionChangeData.IsValid()) {
    return true;
  }

  return mSelectionForTSF.EqualsExceptDirection(mPendingSelectionChangeData);
}

bool
TSFTextStore::GetCurrentText(nsAString& aTextContent)
{
  if (mContentForTSF.IsInitialized()) {
    aTextContent = mContentForTSF.Text();
    return true;
  }

  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(mWidget && !mWidget->Destroyed());

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::GetCurrentText(): "
     "retrieving text from the content...", this));

  WidgetQueryContentEvent queryText(true, eQueryTextContent, mWidget);
  queryText.InitForQueryTextContent(0, UINT32_MAX);
  mWidget->InitEvent(queryText);
  DispatchEvent(queryText);
  if (NS_WARN_IF(!queryText.mSucceeded)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetCurrentText(), FAILED, due to "
       "eQueryTextContent failure", this));
    aTextContent.Truncate();
    return false;
  }

  aTextContent = queryText.mReply.mString;
  return true;
}

TSFTextStore::Selection&
TSFTextStore::SelectionForTSFRef()
{
  if (mSelectionForTSF.IsDirty()) {
    MOZ_ASSERT(!mDestroyed);
    // If the window has never been available, we should crash since working
    // with broken values may make TIP confused.
    if (!mWidget || mWidget->Destroyed()) {
      MOZ_CRASH();
    }

    WidgetQueryContentEvent querySelection(true, eQuerySelectedText, mWidget);
    mWidget->InitEvent(querySelection);
    DispatchEvent(querySelection);
    if (NS_WARN_IF(!querySelection.mSucceeded)) {
      return mSelectionForTSF;
    }

    mSelectionForTSF.SetSelection(querySelection.mReply.mOffset,
                                  querySelection.mReply.mString.Length(),
                                  querySelection.mReply.mReversed,
                                  querySelection.GetWritingMode());
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::SelectionForTSFRef(): "
     "acpStart=%d, acpEnd=%d (length=%d), reverted=%s",
     this, mSelectionForTSF.StartOffset(), mSelectionForTSF.EndOffset(),
     mSelectionForTSF.Length(),
     GetBoolName(mSelectionForTSF.IsReversed())));

  return mSelectionForTSF;
}

static HRESULT
GetRangeExtent(ITfRange* aRange, LONG* aStart, LONG* aLength)
{
  RefPtr<ITfRangeACP> rangeACP;
  aRange->QueryInterface(IID_ITfRangeACP, getter_AddRefs(rangeACP));
  NS_ENSURE_TRUE(rangeACP, E_FAIL);
  return rangeACP->GetExtent(aStart, aLength);
}

static TextRangeType
GetGeckoSelectionValue(TF_DISPLAYATTRIBUTE& aDisplayAttr)
{
  switch (aDisplayAttr.bAttr) {
    case TF_ATTR_TARGET_CONVERTED:
      return TextRangeType::eSelectedClause;
    case TF_ATTR_CONVERTED:
      return TextRangeType::eConvertedClause;
    case TF_ATTR_TARGET_NOTCONVERTED:
      return TextRangeType::eSelectedRawClause;
    default:
      return TextRangeType::eRawClause;
  }
}

HRESULT
TSFTextStore::GetDisplayAttribute(ITfProperty* aAttrProperty,
                                  ITfRange* aRange,
                                  TF_DISPLAYATTRIBUTE* aResult)
{
  NS_ENSURE_TRUE(aAttrProperty, E_FAIL);
  NS_ENSURE_TRUE(aRange, E_FAIL);
  NS_ENSURE_TRUE(aResult, E_FAIL);

  HRESULT hr;

  if (MOZ_LOG_TEST(sTextStoreLog, LogLevel::Debug)) {
    LONG start = 0, length = 0;
    hr = GetRangeExtent(aRange, &start, &length);
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::GetDisplayAttribute(): "
       "GetDisplayAttribute range=%ld-%ld (hr=%s)",
       this, start - mComposition.mStart,
       start - mComposition.mStart + length,
       GetCommonReturnValueName(hr)));
  }

  VARIANT propValue;
  ::VariantInit(&propValue);
  hr = aAttrProperty->GetValue(TfEditCookie(mEditCookie), aRange, &propValue);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetDisplayAttribute() FAILED due to "
       "ITfProperty::GetValue() failed", this));
    return hr;
  }
  if (VT_I4 != propValue.vt) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetDisplayAttribute() FAILED due to "
       "ITfProperty::GetValue() returns non-VT_I4 value", this));
    ::VariantClear(&propValue);
    return E_FAIL;
  }

  NS_ENSURE_TRUE(sCategoryMgr, E_FAIL);
  GUID guid;
  hr = sCategoryMgr->GetGUID(DWORD(propValue.lVal), &guid);
  ::VariantClear(&propValue);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetDisplayAttribute() FAILED due to "
       "ITfCategoryMgr::GetGUID() failed", this));
    return hr;
  }

  NS_ENSURE_TRUE(sDisplayAttrMgr, E_FAIL);
  RefPtr<ITfDisplayAttributeInfo> info;
  hr = sDisplayAttrMgr->GetDisplayAttributeInfo(guid, getter_AddRefs(info),
                                                nullptr);
  if (FAILED(hr) || !info) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetDisplayAttribute() FAILED due to "
       "ITfDisplayAttributeMgr::GetDisplayAttributeInfo() failed", this));
    return hr;
  }

  hr = info->GetAttributeInfo(aResult);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetDisplayAttribute() FAILED due to "
       "ITfDisplayAttributeInfo::GetAttributeInfo() failed", this));
    return hr;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::GetDisplayAttribute() succeeded: "
     "Result={ %s }", this, GetDisplayAttrStr(*aResult).get()));
  return S_OK;
}

HRESULT
TSFTextStore::RestartCompositionIfNecessary(ITfRange* aRangeNew)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::RestartCompositionIfNecessary("
     "aRangeNew=0x%p), mComposition.mView=0x%p",
     this, aRangeNew, mComposition.mView.get()));

  if (!mComposition.IsComposing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RestartCompositionIfNecessary() FAILED "
       "due to no composition view", this));
    return E_FAIL;
  }

  HRESULT hr;
  RefPtr<ITfCompositionView> pComposition(mComposition.mView);
  RefPtr<ITfRange> composingRange(aRangeNew);
  if (!composingRange) {
    hr = pComposition->GetRange(getter_AddRefs(composingRange));
    if (FAILED(hr)) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::RestartCompositionIfNecessary() "
         "FAILED due to pComposition->GetRange() failure", this));
      return hr;
    }
  }

  // Get starting offset of the composition
  LONG compStart = 0, compLength = 0;
  hr = GetRangeExtent(composingRange, &compStart, &compLength);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RestartCompositionIfNecessary() FAILED "
       "due to GetRangeExtent() failure", this));
    return hr;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::RestartCompositionIfNecessary(), "
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
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::RestartCompositionIfNecessary() "
         "FAILED due to RestartComposition() failure", this));
      return hr;
    }
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::RestartCompositionIfNecessary() succeeded",
     this));
  return S_OK;
}

HRESULT
TSFTextStore::RestartComposition(ITfCompositionView* aCompositionView,
                                 ITfRange* aNewRange)
{
  Selection& selectionForTSF = SelectionForTSFRef();

  LONG newStart, newLength;
  HRESULT hr = GetRangeExtent(aNewRange, &newStart, &newLength);
  LONG newEnd = newStart + newLength;

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::RestartComposition(aCompositionView=0x%p, "
     "aNewRange=0x%p { newStart=%d, newLength=%d }), "
     "mComposition={ mStart=%d, mCompositionString.Length()=%d }, "
     "selectionForTSF={ IsDirty()=%s, StartOffset()=%d, Length()=%d }",
     this, aCompositionView, aNewRange, newStart, newLength,
     mComposition.mStart, mComposition.mString.Length(),
     GetBoolName(selectionForTSF.IsDirty()),
     selectionForTSF.StartOffset(), selectionForTSF.Length()));

  if (selectionForTSF.IsDirty()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RestartComposition() FAILED "
       "due to SelectionForTSFRef() failure", this));
    return E_FAIL;
  }

  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RestartComposition() FAILED "
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
  Selection oldSelection = selectionForTSF;

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
  Content& contentForTSF = ContentForTSFRef();
  if (!contentForTSF.IsInitialized()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RestartComposition() FAILED "
       "due to ContentForTSFRef() failure", this));
    return E_FAIL;
  }
  contentForTSF.ReplaceTextWith(mComposition.mStart,
                                mComposition.mString.Length(),
                                commitString);
  // Record a compositionupdate action for commit the part of composing string.
  PendingAction* action = LastOrNewPendingCompositionUpdate();
  action->mData = mComposition.mString;
  action->mRanges->Clear();
  // Note that we shouldn't append ranges when composition string
  // is empty because it may cause TextComposition confused.
  if (!action->mData.IsEmpty()) {
    TextRange caretRange;
    caretRange.mStartOffset = caretRange.mEndOffset =
      uint32_t(oldComposition.mStart + commitString.Length());
    caretRange.mRangeType = TextRangeType::eCaret;
    action->mRanges->AppendElement(caretRange);
  }
  action->mIncomplete = false;

  // Record compositionend action.
  RecordCompositionEndAction();

  // Record compositionstart action only with the new start since this method
  // hasn't restored composing string yet.
  RecordCompositionStartAction(aCompositionView, newStart, 0, false);

  // Restore the latest text content and selection.
  contentForTSF.ReplaceSelectedTextWith(
    nsDependentSubstring(oldComposition.mString,
                         keepComposingStartOffset - oldComposition.mStart,
                         keepComposingLength));
  selectionForTSF = oldSelection;

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::RestartComposition() succeeded, "
     "mComposition={ mStart=%d, mCompositionString.Length()=%d }, "
     "selectionForTSF={ IsDirty()=%s, StartOffset()=%d, Length()=%d }",
     this, mComposition.mStart, mComposition.mString.Length(),
     GetBoolName(selectionForTSF.IsDirty()),
     selectionForTSF.StartOffset(), selectionForTSF.Length()));

  return S_OK;
}

static bool
GetColor(const TF_DA_COLOR& aTSFColor, nscolor& aResult)
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
GetLineStyle(TF_DA_LINESTYLE aTSFLineStyle, uint8_t& aTextRangeLineStyle)
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
TSFTextStore::RecordCompositionUpdateAction()
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::RecordCompositionUpdateAction(), "
     "mComposition={ mView=0x%p, mStart=%d, mString=\"%s\" "
     "(Length()=%d) }",
     this, mComposition.mView.get(), mComposition.mStart,
     GetEscapedUTF8String(mComposition.mString).get(),
     mComposition.mString.Length()));

  if (!mComposition.IsComposing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RecordCompositionUpdateAction() FAILED "
       "due to no composition view", this));
    return E_FAIL;
  }

  // Getting display attributes is *really* complicated!
  // We first get the context and the property objects to query for
  // attributes, but since a big range can have a variety of values for
  // the attribute, we have to find out all the ranges that have distinct
  // attribute values. Then we query for what the value represents through
  // the display attribute manager and translate that to TextRange to be
  // sent in eCompositionChange

  RefPtr<ITfProperty> attrPropetry;
  HRESULT hr = mContext->GetProperty(GUID_PROP_ATTRIBUTE,
                                     getter_AddRefs(attrPropetry));
  if (FAILED(hr) || !attrPropetry) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RecordCompositionUpdateAction() FAILED "
       "due to mContext->GetProperty() failure", this));
    return FAILED(hr) ? hr : E_FAIL;
  }

  RefPtr<ITfRange> composingRange;
  hr = mComposition.mView->GetRange(getter_AddRefs(composingRange));
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RecordCompositionUpdateAction() "
       "FAILED due to mComposition.mView->GetRange() failure", this));
    return hr;
  }

  RefPtr<IEnumTfRanges> enumRanges;
  hr = attrPropetry->EnumRanges(TfEditCookie(mEditCookie),
                                getter_AddRefs(enumRanges), composingRange);
  if (FAILED(hr) || !enumRanges) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RecordCompositionUpdateAction() FAILED "
       "due to attrPropetry->EnumRanges() failure", this));
    return FAILED(hr) ? hr : E_FAIL;
  }

  // First, put the log of content and selection here.
  Selection& selectionForTSF = SelectionForTSFRef();
  if (selectionForTSF.IsDirty()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RecordCompositionUpdateAction() FAILED "
       "due to SelectionForTSFRef() failure", this));
    return E_FAIL;
  }

  PendingAction* action = LastOrNewPendingCompositionUpdate();
  action->mData = mComposition.mString;
  // The ranges might already have been initialized, however, if this is
  // called again, that means we need to overwrite the ranges with current
  // information.
  action->mRanges->Clear();

  // Note that we shouldn't append ranges when composition string
  // is empty because it may cause TextComposition confused.
  if (!action->mData.IsEmpty()) {
    TextRange newRange;
    // No matter if we have display attribute info or not,
    // we always pass in at least one range to eCompositionChange
    newRange.mStartOffset = 0;
    newRange.mEndOffset = action->mData.Length();
    newRange.mRangeType = TextRangeType::eRawClause;
    action->mRanges->AppendElement(newRange);

    RefPtr<ITfRange> range;
    while (enumRanges->Next(1, getter_AddRefs(range), nullptr) == S_OK) {
      if (NS_WARN_IF(!range)) {
        break;
      }

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
        MOZ_LOG(sTextStoreLog, LogLevel::Error,
          ("0x%p   TSFTextStore::RecordCompositionUpdateAction() "
           "ignores invalid range (%d-%d)",
           this, rangeStart - mComposition.mStart,
           rangeStart - mComposition.mStart + rangeLength));
        continue;
      }
      if (!length) {
        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::RecordCompositionUpdateAction() "
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
        newRange.mRangeType = TextRangeType::eRawClause;
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
    // one).  So, the composition string looks like normal (or committed)
    // string.  At this time, current selection range is same as the
    // composition string range.  Other applications set a wide caret which
    // covers the composition string,  however, Gecko doesn't support the wide
    // caret drawing now (Gecko doesn't support XOR drawing), unfortunately.
    // For now, we should change the range style to undefined.
    if (!selectionForTSF.IsCollapsed() && action->mRanges->Length() == 1) {
      TextRange& range = action->mRanges->ElementAt(0);
      LONG start = selectionForTSF.MinOffset();
      LONG end = selectionForTSF.MaxOffset();
      if ((LONG)range.mStartOffset == start - mComposition.mStart &&
          (LONG)range.mEndOffset == end - mComposition.mStart &&
          range.mRangeStyle.IsNoChangeStyle()) {
        range.mRangeStyle.Clear();
        // The looks of selected type is better than others.
        range.mRangeType = TextRangeType::eSelectedRawClause;
      }
    }

    // The caret position has to be collapsed.
    uint32_t caretPosition =
      static_cast<uint32_t>(selectionForTSF.MaxOffset() - mComposition.mStart);

    // If caret is in the target clause and it doesn't have specific style,
    // the target clause will be painted as normal selection range.  Since
    // caret shouldn't be in selection range on Windows, we shouldn't append
    // caret range in such case.
    const TextRange* targetClause = action->mRanges->GetTargetClause();
    if (!targetClause || targetClause->mRangeStyle.IsDefined() ||
        caretPosition < targetClause->mStartOffset ||
        caretPosition > targetClause->mEndOffset) {
      TextRange caretRange;
      caretRange.mStartOffset = caretRange.mEndOffset = caretPosition;
      caretRange.mRangeType = TextRangeType::eCaret;
      action->mRanges->AppendElement(caretRange);
    }
  }

  action->mIncomplete = false;

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::RecordCompositionUpdateAction() "
     "succeeded", this));

  return S_OK;
}

HRESULT
TSFTextStore::SetSelectionInternal(const TS_SELECTION_ACP* pSelection,
                                   bool aDispatchCompositionChangeEvent)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::SetSelectionInternal(pSelection={ "
     "acpStart=%ld, acpEnd=%ld, style={ ase=%s, fInterimChar=%s} }, "
     "aDispatchCompositionChangeEvent=%s), mComposition.IsComposing()=%s",
     this, pSelection->acpStart, pSelection->acpEnd,
     GetActiveSelEndName(pSelection->style.ase),
     GetBoolName(pSelection->style.fInterimChar),
     GetBoolName(aDispatchCompositionChangeEvent),
     GetBoolName(mComposition.IsComposing())));

  MOZ_ASSERT(IsReadWriteLocked());

  Selection& selectionForTSF = SelectionForTSFRef();
  if (selectionForTSF.IsDirty()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::SetSelectionInternal() FAILED due to "
       "SelectionForTSFRef() failure", this));
    return E_FAIL;
  }

  // If actually the range is not changing, we should do nothing.
  // Perhaps, we can ignore the difference change because it must not be
  // important for following edit.
  if (selectionForTSF.EqualsExceptDirection(*pSelection)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::SetSelectionInternal() Succeeded but "
       "did nothing because the selection range isn't changing", this));
    selectionForTSF.SetSelection(*pSelection);
    return S_OK;
  }

  if (mComposition.IsComposing()) {
    if (aDispatchCompositionChangeEvent) {
      HRESULT hr = RestartCompositionIfNecessary();
      if (FAILED(hr)) {
        MOZ_LOG(sTextStoreLog, LogLevel::Error,
          ("0x%p   TSFTextStore::SetSelectionInternal() FAILED due to "
           "RestartCompositionIfNecessary() failure", this));
        return hr;
      }
    }
    if (pSelection->acpStart < mComposition.mStart ||
        pSelection->acpEnd > mComposition.EndOffset()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::SetSelectionInternal() FAILED due to "
         "the selection being out of the composition string", this));
      return TS_E_INVALIDPOS;
    }
    // Emulate selection during compositions
    selectionForTSF.SetSelection(*pSelection);
    if (aDispatchCompositionChangeEvent) {
      HRESULT hr = RecordCompositionUpdateAction();
      if (FAILED(hr)) {
        MOZ_LOG(sTextStoreLog, LogLevel::Error,
          ("0x%p   TSFTextStore::SetSelectionInternal() FAILED due to "
           "RecordCompositionUpdateAction() failure", this));
        return hr;
      }
    }
    return S_OK;
  }

  TS_SELECTION_ACP selectionInContent(*pSelection);

  // If mContentForTSF caches old contents which is now different from
  // actual contents, we need some complicated hack here...
  // Note that this hack assumes that this is used for reconversion.
  if (mContentForTSF.IsInitialized() &&
      mPendingTextChangeData.IsValid() &&
      !mPendingTextChangeData.mCausedOnlyByComposition) {
    uint32_t startOffset = static_cast<uint32_t>(selectionInContent.acpStart);
    uint32_t endOffset = static_cast<uint32_t>(selectionInContent.acpEnd);
    if (mPendingTextChangeData.mStartOffset >= endOffset) {
      // Setting selection before any changed ranges is fine.
    } else if (mPendingTextChangeData.mRemovedEndOffset <= startOffset) {
      // Setting selection after removed range is fine with following
      // adjustment.
      selectionInContent.acpStart += mPendingTextChangeData.Difference();
      selectionInContent.acpEnd += mPendingTextChangeData.Difference();
    } else if (startOffset == endOffset) {
      // Moving caret position may be fine in most cases even if the insertion
      // point has already gone but in this case, composition will be inserted
      // to unexpected position, though.
      // It seems that moving caret into middle of the new text is odd.
      // Perhaps, end of it is expected by users in most cases.
      selectionInContent.acpStart = mPendingTextChangeData.mAddedEndOffset;
      selectionInContent.acpEnd = selectionInContent.acpStart;
    } else {
      // Otherwise, i.e., setting range has already gone, we cannot set
      // selection properly.
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::SetSelectionInternal() FAILED due to "
         "there is unknown content change", this));
      return E_FAIL;
    }
  }

  CompleteLastActionIfStillIncomplete();
  PendingAction* action = mPendingActions.AppendElement();
  action->mType = PendingAction::SET_SELECTION;
  action->mSelectionStart = selectionInContent.acpStart;
  action->mSelectionLength =
    selectionInContent.acpEnd - selectionInContent.acpStart;
  action->mSelectionReversed = (selectionInContent.style.ase == TS_AE_START);

  // Use TSF specified selection for updating mSelectionForTSF.
  selectionForTSF.SetSelection(*pSelection);

  return S_OK;
}

STDMETHODIMP
TSFTextStore::SetSelection(ULONG ulCount,
                           const TS_SELECTION_ACP* pSelection)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::SetSelection(ulCount=%lu, pSelection=%p { "
     "acpStart=%ld, acpEnd=%ld, style={ ase=%s, fInterimChar=%s } }), "
     "mComposition.IsComposing()=%s",
     this, ulCount, pSelection,
     pSelection ? pSelection->acpStart : 0,
     pSelection ? pSelection->acpEnd : 0,
     pSelection ? GetActiveSelEndName(pSelection->style.ase) : "",
     pSelection ? GetBoolName(pSelection->style.fInterimChar) : "",
     GetBoolName(mComposition.IsComposing())));

  if (!IsReadWriteLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::SetSelection() FAILED due to "
       "not locked (read-write)", this));
    return TS_E_NOLOCK;
  }
  if (ulCount != 1) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::SetSelection() FAILED due to "
       "trying setting multiple selection", this));
    return E_INVALIDARG;
  }
  if (!pSelection) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::SetSelection() FAILED due to "
       "null argument", this));
    return E_INVALIDARG;
  }

  HRESULT hr = SetSelectionInternal(pSelection, true);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::SetSelection() FAILED due to "
       "SetSelectionInternal() failure", this));
  } else {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::SetSelection() succeeded", this));
  }
  return hr;
}

STDMETHODIMP
TSFTextStore::GetText(LONG acpStart,
                      LONG acpEnd,
                      WCHAR* pchPlain,
                      ULONG cchPlainReq,
                      ULONG* pcchPlainOut,
                      TS_RUNINFO* prgRunInfo,
                      ULONG ulRunInfoReq,
                      ULONG* pulRunInfoOut,
                      LONG* pacpNext)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::GetText(acpStart=%ld, acpEnd=%ld, pchPlain=0x%p, "
     "cchPlainReq=%lu, pcchPlainOut=0x%p, prgRunInfo=0x%p, ulRunInfoReq=%lu, "
     "pulRunInfoOut=0x%p, pacpNext=0x%p), mComposition={ mStart=%ld, "
     "mString.Length()=%lu, IsComposing()=%s }",
     this, acpStart, acpEnd, pchPlain, cchPlainReq, pcchPlainOut,
     prgRunInfo, ulRunInfoReq, pulRunInfoOut, pacpNext,
     mComposition.mStart, mComposition.mString.Length(),
     GetBoolName(mComposition.IsComposing())));

  if (!IsReadLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetText() FAILED due to "
       "not locked (read)", this));
    return TS_E_NOLOCK;
  }

  if (!pcchPlainOut || (!pchPlain && !prgRunInfo) ||
      !cchPlainReq != !pchPlain || !ulRunInfoReq != !prgRunInfo) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetText() FAILED due to "
       "invalid argument", this));
    return E_INVALIDARG;
  }

  if (acpStart < 0 || acpEnd < -1 || (acpEnd != -1 && acpStart > acpEnd)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetText() FAILED due to "
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

  Content& contentForTSF = ContentForTSFRef();
  if (!contentForTSF.IsInitialized()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetText() FAILED due to "
       "ContentForTSFRef() failure", this));
    return E_FAIL;
  }
  if (contentForTSF.Text().Length() < static_cast<uint32_t>(acpStart)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetText() FAILED due to "
       "acpStart is larger offset than the actual text length", this));
    return TS_E_INVALIDPOS;
  }
  if (acpEnd != -1 &&
      contentForTSF.Text().Length() < static_cast<uint32_t>(acpEnd)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetText() FAILED due to "
       "acpEnd is larger offset than the actual text length", this));
    return TS_E_INVALIDPOS;
  }
  uint32_t length = (acpEnd == -1) ?
    contentForTSF.Text().Length() - static_cast<uint32_t>(acpStart) :
    static_cast<uint32_t>(acpEnd - acpStart);
  if (cchPlainReq && cchPlainReq - 1 < length) {
    length = cchPlainReq - 1;
  }
  if (length) {
    if (pchPlain && cchPlainReq) {
      const char16_t* startChar =
        contentForTSF.Text().BeginReading() + acpStart;
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

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::GetText() succeeded: pcchPlainOut=0x%p, "
     "*prgRunInfo={ uCount=%lu, type=%s }, *pulRunInfoOut=%lu, "
     "*pacpNext=%ld)",
     this, pcchPlainOut, prgRunInfo ? prgRunInfo->uCount : 0,
     prgRunInfo ? GetTextRunTypeName(prgRunInfo->type) : "N/A",
     pulRunInfoOut ? *pulRunInfoOut : 0, pacpNext ? *pacpNext : 0));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::SetText(DWORD dwFlags,
                      LONG acpStart,
                      LONG acpEnd,
                      const WCHAR* pchText,
                      ULONG cch,
                      TS_TEXTCHANGE* pChange)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::SetText(dwFlags=%s, acpStart=%ld, "
     "acpEnd=%ld, pchText=0x%p \"%s\", cch=%lu, pChange=0x%p), "
     "mComposition.IsComposing()=%s",
     this, dwFlags == TS_ST_CORRECTION ? "TS_ST_CORRECTION" :
                                         "not-specified",
     acpStart, acpEnd, pchText,
     pchText && cch ?
       GetEscapedUTF8String(pchText, cch).get() : "",
     cch, pChange, GetBoolName(mComposition.IsComposing())));

  // Per SDK documentation, and since we don't have better
  // ways to do this, this method acts as a helper to
  // call SetSelection followed by InsertTextAtSelection
  if (!IsReadWriteLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::SetText() FAILED due to "
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
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::SetText() FAILED due to "
       "SetSelectionInternal() failure", this));
    return hr;
  }
  // Replace just selected text
  if (!InsertTextAtSelectionInternal(nsDependentSubstring(pchText, cch),
                                     pChange)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::SetText() FAILED due to "
       "InsertTextAtSelectionInternal() failure", this));
    return E_FAIL;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::SetText() succeeded: pChange={ "
     "acpStart=%ld, acpOldEnd=%ld, acpNewEnd=%ld }",
     this, pChange ? pChange->acpStart  : 0,
     pChange ? pChange->acpOldEnd : 0, pChange ? pChange->acpNewEnd : 0));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::GetFormattedText(LONG acpStart,
                               LONG acpEnd,
                               IDataObject** ppDataObject)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::GetFormattedText() called "
     "but not supported (E_NOTIMPL)", this));

  // no support for formatted text
  return E_NOTIMPL;
}

STDMETHODIMP
TSFTextStore::GetEmbedded(LONG acpPos,
                          REFGUID rguidService,
                          REFIID riid,
                          IUnknown** ppunk)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::GetEmbedded() called "
     "but not supported (E_NOTIMPL)", this));

  // embedded objects are not supported
  return E_NOTIMPL;
}

STDMETHODIMP
TSFTextStore::QueryInsertEmbedded(const GUID* pguidService,
                                  const FORMATETC* pFormatEtc,
                                  BOOL* pfInsertable)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::QueryInsertEmbedded() called "
     "but not supported, *pfInsertable=FALSE (S_OK)", this));

  // embedded objects are not supported
  *pfInsertable = FALSE;
  return S_OK;
}

STDMETHODIMP
TSFTextStore::InsertEmbedded(DWORD dwFlags,
                             LONG acpStart,
                             LONG acpEnd,
                             IDataObject* pDataObject,
                             TS_TEXTCHANGE* pChange)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::InsertEmbedded() called "
     "but not supported (E_NOTIMPL)", this));

  // embedded objects are not supported
  return E_NOTIMPL;
}

void
TSFTextStore::SetInputScope(const nsString& aHTMLInputType,
                            const nsString& aHTMLInputInputMode)
{
  mInputScopes.Clear();
  if (aHTMLInputType.IsEmpty() || aHTMLInputType.EqualsLiteral("text")) {
    if (aHTMLInputInputMode.EqualsLiteral("url")) {
      mInputScopes.AppendElement(IS_URL);
    } else if (aHTMLInputInputMode.EqualsLiteral("email")) {
      mInputScopes.AppendElement(IS_EMAIL_SMTPEMAILADDRESS);
    } else if (aHTMLInputType.EqualsLiteral("tel")) {
      mInputScopes.AppendElement(IS_TELEPHONE_FULLTELEPHONENUMBER);
      mInputScopes.AppendElement(IS_TELEPHONE_LOCALNUMBER);
    } else if (aHTMLInputType.EqualsLiteral("numeric")) {
      mInputScopes.AppendElement(IS_NUMBER);
    }
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
TSFTextStore::GetRequestedAttrIndex(const TS_ATTRID& aAttrID)
{
  if (IsEqualGUID(aAttrID, GUID_PROP_INPUTSCOPE)) {
    return eInputScope;
  }
  if (IsEqualGUID(aAttrID, TSATTRID_Text_VerticalWriting)) {
    return eTextVerticalWriting;
  }
  if (IsEqualGUID(aAttrID, TSATTRID_Text_Orientation)) {
    return eTextOrientation;
  }
  return eNotSupported;
}

TS_ATTRID
TSFTextStore::GetAttrID(int32_t aIndex)
{
  switch (aIndex) {
    case eInputScope:
      return GUID_PROP_INPUTSCOPE;
    case eTextVerticalWriting:
      return TSATTRID_Text_VerticalWriting;
    case eTextOrientation:
      return TSATTRID_Text_Orientation;
    default:
      MOZ_CRASH("Invalid index? Or not implemented yet?");
      return GUID_NULL;
  }
}

HRESULT
TSFTextStore::HandleRequestAttrs(DWORD aFlags,
                                 ULONG aFilterCount,
                                 const TS_ATTRID* aFilterAttrs)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::HandleRequestAttrs(aFlags=%s, "
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
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::HandleRequestAttrs(), "
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
TSFTextStore::RequestSupportedAttrs(DWORD dwFlags,
                                    ULONG cFilterAttrs,
                                    const TS_ATTRID* paFilterAttrs)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::RequestSupportedAttrs(dwFlags=%s, "
     "cFilterAttrs=%lu)",
     this, GetFindFlagName(dwFlags).get(), cFilterAttrs));

  return HandleRequestAttrs(dwFlags, cFilterAttrs, paFilterAttrs);
}

STDMETHODIMP
TSFTextStore::RequestAttrsAtPosition(LONG acpPos,
                                     ULONG cFilterAttrs,
                                     const TS_ATTRID* paFilterAttrs,
                                     DWORD dwFlags)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::RequestAttrsAtPosition(acpPos=%ld, "
     "cFilterAttrs=%lu, dwFlags=%s)",
     this, acpPos, cFilterAttrs, GetFindFlagName(dwFlags).get()));

  return HandleRequestAttrs(dwFlags | TS_ATTR_FIND_WANT_VALUE,
                            cFilterAttrs, paFilterAttrs);
}

STDMETHODIMP
TSFTextStore::RequestAttrsTransitioningAtPosition(LONG acpPos,
                                                  ULONG cFilterAttrs,
                                                  const TS_ATTRID* paFilterAttr,
                                                  DWORD dwFlags)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::RequestAttrsTransitioningAtPosition("
     "acpPos=%ld, cFilterAttrs=%lu, dwFlags=%s) called but not supported "
     "(S_OK)",
     this, acpPos, cFilterAttrs, GetFindFlagName(dwFlags).get()));

  // no per character attributes defined
  return S_OK;
}

STDMETHODIMP
TSFTextStore::FindNextAttrTransition(LONG acpStart,
                                     LONG acpHalt,
                                     ULONG cFilterAttrs,
                                     const TS_ATTRID* paFilterAttrs,
                                     DWORD dwFlags,
                                     LONG* pacpNext,
                                     BOOL* pfFound,
                                     LONG* plFoundOffset)
{
  if (!pacpNext || !pfFound || !plFoundOffset) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  0x%p TSFTextStore::FindNextAttrTransition() FAILED due to "
       "null argument", this));
    return E_INVALIDARG;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::FindNextAttrTransition() called "
     "but not supported (S_OK)", this));

  // no per character attributes defined
  *pacpNext = *plFoundOffset = acpHalt;
  *pfFound = FALSE;
  return S_OK;
}

STDMETHODIMP
TSFTextStore::RetrieveRequestedAttrs(ULONG ulCount,
                                     TS_ATTRVAL* paAttrVals,
                                     ULONG* pcFetched)
{
  if (!pcFetched || !paAttrVals) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p TSFTextStore::RetrieveRequestedAttrs() FAILED due to "
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
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p TSFTextStore::RetrieveRequestedAttrs() FAILED due to "
       "not enough count ulCount=%u, expectedCount=%u",
       this, ulCount, expectedCount));
    return E_INVALIDARG;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::RetrieveRequestedAttrs() called "
     "ulCount=%d, mRequestedAttrValues=%s",
     this, ulCount, GetBoolName(mRequestedAttrValues)));

  int32_t count = 0;
  for (int32_t i = 0; i < NUM_OF_SUPPORTED_ATTRS; i++) {
    if (!mRequestedAttrs[i]) {
      continue;
    }
    mRequestedAttrs[i] = false;

    TS_ATTRID attrID = GetAttrID(i);

    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::RetrieveRequestedAttrs() for %s",
       this, GetGUIDNameStrWithTable(attrID).get()));

    paAttrVals[count].idAttr = attrID;
    paAttrVals[count].dwOverlapId = 0;

    if (!mRequestedAttrValues) {
      paAttrVals[count].varValue.vt = VT_EMPTY;
    } else {
      switch (i) {
        case eInputScope: {
          paAttrVals[count].varValue.vt = VT_UNKNOWN;
          RefPtr<IUnknown> inputScope = new InputScopeImpl(mInputScopes);
          paAttrVals[count].varValue.punkVal = inputScope.forget().take();
          break;
        }
        case eTextVerticalWriting: {
          Selection& selectionForTSF = SelectionForTSFRef();
          paAttrVals[count].varValue.vt = VT_BOOL;
          paAttrVals[count].varValue.boolVal =
            selectionForTSF.GetWritingMode().IsVertical() ? VARIANT_TRUE :
                                                            VARIANT_FALSE;
          break;
        }
        case eTextOrientation: {
          Selection& selectionForTSF = SelectionForTSFRef();
          paAttrVals[count].varValue.vt = VT_I4;
          paAttrVals[count].varValue.lVal =
            selectionForTSF.GetWritingMode().IsVertical() ? 2700 : 0;
          break;
        }
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

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::RetrieveRequestedAttrs() called "
     "for unknown TS_ATTRVAL, *pcFetched=0 (S_OK)", this));

  paAttrVals->dwOverlapId = 0;
  paAttrVals->varValue.vt = VT_EMPTY;
  *pcFetched = 0;
  return S_OK;
}

STDMETHODIMP
TSFTextStore::GetEndACP(LONG* pacp)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::GetEndACP(pacp=0x%p)", this, pacp));

  if (!IsReadLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetEndACP() FAILED due to "
       "not locked (read)", this));
    return TS_E_NOLOCK;
  }

  if (!pacp) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetEndACP() FAILED due to "
       "null argument", this));
    return E_INVALIDARG;
  }

  Content& contentForTSF = ContentForTSFRef();
  if (!contentForTSF.IsInitialized()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetEndACP() FAILED due to "
       "ContentForTSFRef() failure", this));
    return E_FAIL;
  }
  *pacp = static_cast<LONG>(contentForTSF.Text().Length());
  return S_OK;
}

STDMETHODIMP
TSFTextStore::GetActiveView(TsViewCookie* pvcView)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::GetActiveView(pvcView=0x%p)",
     this, pvcView));

  if (!pvcView) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetActiveView() FAILED due to "
       "null argument", this));
    return E_INVALIDARG;
  }

  *pvcView = TEXTSTORE_DEFAULT_VIEW;

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::GetActiveView() succeeded: *pvcView=%ld",
     this, *pvcView));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::GetACPFromPoint(TsViewCookie vcView,
                              const POINT* pt,
                              DWORD dwFlags,
                              LONG* pacp)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::GetACPFromPoint(pvcView=%d, pt=%p (x=%d, "
     "y=%d), dwFlags=%s, pacp=%p, mDeferNotifyingTSF=%s, "
     "mWaitingQueryLayout=%s",
     this, vcView, pt, pt ? pt->x : 0, pt ? pt->y : 0,
     GetACPFromPointFlagName(dwFlags).get(), pacp,
     GetBoolName(mDeferNotifyingTSF), GetBoolName(mWaitingQueryLayout)));

  if (!IsReadLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to "
       "not locked (read)", this));
    return TS_E_NOLOCK;
  }

  if (vcView != TEXTSTORE_DEFAULT_VIEW) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to "
       "called with invalid view", this));
    return E_INVALIDARG;
  }

  if (!pt) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to "
       "null pt", this));
    return E_INVALIDARG;
  }

  if (!pacp) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to "
       "null pacp", this));
    return E_INVALIDARG;
  }

  mWaitingQueryLayout = false;

  if (mDestroyed || mContentForTSF.IsLayoutChanged()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetACPFromPoint() returned "
       "TS_E_NOLAYOUT", this));
    mHasReturnedNoLayoutError = true;
    return TS_E_NOLAYOUT;
  }

  LayoutDeviceIntPoint ourPt(pt->x, pt->y);
  // Convert to widget relative coordinates from screen's.
  ourPt -= mWidget->WidgetToScreenOffset();

  // NOTE: Don't check if the point is in the widget since the point can be
  //       outside of the widget if focused editor is in a XUL <panel>.

  WidgetQueryContentEvent charAtPt(true, eQueryCharacterAtPoint, mWidget);
  mWidget->InitEvent(charAtPt, &ourPt);

  // FYI: WidgetQueryContentEvent may cause flushing pending layout and it
  //      may cause focus change or something.
  RefPtr<TSFTextStore> kungFuDeathGrip(this);
  DispatchEvent(charAtPt);
  if (!mWidget || mWidget->Destroyed()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to "
       "mWidget was destroyed during eQueryCharacterAtPoint", this));
    return E_FAIL;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::GetACPFromPoint(), charAtPt={ "
     "mSucceeded=%s, mReply={ mOffset=%u, mTentativeCaretOffset=%u }}",
     this, GetBoolName(charAtPt.mSucceeded), charAtPt.mReply.mOffset,
     charAtPt.mReply.mTentativeCaretOffset));

  if (NS_WARN_IF(!charAtPt.mSucceeded)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to "
       "eQueryCharacterAtPoint failure", this));
    return E_FAIL;
  }

  // If dwFlags isn't set and the point isn't in any character's bounding box,
  // we should return TS_E_INVALIDPOINT.
  if (!(dwFlags & GXFPF_NEAREST) &&
      charAtPt.mReply.mOffset == WidgetQueryContentEvent::NOT_FOUND) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to the "
       "point contained by no bounding box", this));
    return TS_E_INVALIDPOINT;
  }

  // Although, we're not sure if mTentativeCaretOffset becomes NOT_FOUND,
  // let's assume that there is no content in such case.
  if (NS_WARN_IF(charAtPt.mReply.mTentativeCaretOffset ==
                   WidgetQueryContentEvent::NOT_FOUND)) {
    charAtPt.mReply.mTentativeCaretOffset = 0;
  }

  uint32_t offset;

  // If dwFlags includes GXFPF_ROUND_NEAREST, we should return tentative
  // caret offset (MSDN calls it "range position").
  if (dwFlags & GXFPF_ROUND_NEAREST) {
    offset = charAtPt.mReply.mTentativeCaretOffset;
  } else if (charAtPt.mReply.mOffset != WidgetQueryContentEvent::NOT_FOUND) {
    // Otherwise, we should return character offset whose bounding box contains
    // the point.
    offset = charAtPt.mReply.mOffset;
  } else {
    // If the point isn't in any character's bounding box but we need to return
    // the nearest character from the point, we should *guess* the character
    // offset since there is no inexpensive API to check it strictly.
    // XXX If we retrieve 2 bounding boxes, one is before the offset and
    //     the other is after the offset, we could resolve the offset.
    //     However, dispatching 2 eQueryTextRect may be expensive.

    // So, use tentative offset for now.
    offset = charAtPt.mReply.mTentativeCaretOffset;

    // However, if it's after the last character, we need to decrement the
    // offset.
    Content& contentForTSF = ContentForTSFRef();
    if (!contentForTSF.IsInitialized()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to "
         "ContentForTSFRef() failure", this));
      return E_FAIL;
    }
    if (contentForTSF.Text().Length() <= offset) {
      // If the tentative caret is after the last character, let's return
      // the last character's offset.
      offset = contentForTSF.Text().Length() - 1;
    }
  }

  if (NS_WARN_IF(offset > LONG_MAX)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to out of "
       "range of the result", this));
    return TS_E_INVALIDPOINT;
  }

  *pacp = static_cast<LONG>(offset);
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::GetACPFromPoint() succeeded: *pacp=%d",
     this, *pacp));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::GetTextExt(TsViewCookie vcView,
                         LONG acpStart,
                         LONG acpEnd,
                         RECT* prc,
                         BOOL* pfClipped)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::GetTextExt(vcView=%ld, "
     "acpStart=%ld, acpEnd=%ld, prc=0x%p, pfClipped=0x%p), "
     "mDeferNotifyingTSF=%s, mWaitingQueryLayout=%s",
     this, vcView, acpStart, acpEnd, prc, pfClipped,
     GetBoolName(mDeferNotifyingTSF), GetBoolName(mWaitingQueryLayout)));

  if (!IsReadLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
       "not locked (read)", this));
    return TS_E_NOLOCK;
  }

  if (vcView != TEXTSTORE_DEFAULT_VIEW) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
       "called with invalid view", this));
    return E_INVALIDARG;
  }

  if (!prc || !pfClipped) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
       "null argument", this));
    return E_INVALIDARG;
  }

  if (acpStart < 0 || acpEnd < acpStart) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
       "invalid position", this));
    return TS_E_INVALIDPOS;
  }

  mWaitingQueryLayout = false;

  // NOTE: TSF (at least on Win 8.1) doesn't return TS_E_NOLAYOUT to the
  // caller even if we return it.  It's converted to just E_FAIL.
  // However, this is fixed on Win 10.

  bool dontReturnNoLayoutError = false;

  const TSFStaticSink* kSink = TSFStaticSink::GetInstance();
  if (mComposition.IsComposing() && mComposition.mStart < acpEnd &&
      mContentForTSF.IsLayoutChangedAt(acpEnd)) {
    const Selection& selectionForTSF = SelectionForTSFRef();
    // The bug of Microsoft Office IME 2010 for Japanese is similar to
    // MS-IME for Win 8.1 and Win 10.  Newer version of MS Office IME is not
    // released yet.  So, we can hack it without prefs  because there must be
    // no developers who want to disable this hack for tests.
    const bool kIsMSOfficeJapaneseIME2010 =
      kSink->IsMSOfficeJapaneseIME2010Active();
    // MS IME for Japanese doesn't support asynchronous handling at deciding
    // its suggest list window position.  The feature was implemented
    // starting from Windows 8.  And also we may meet same trouble in e10s
    // mode on Win7.  So, we should never return TS_E_NOLAYOUT to MS IME for
    // Japanese.
    if (kIsMSOfficeJapaneseIME2010 ||
        ((sDoNotReturnNoLayoutErrorToMSJapaneseIMEAtFirstChar ||
          sDoNotReturnNoLayoutErrorToMSJapaneseIMEAtCaret) &&
         kSink->IsMSJapaneseIMEActive())) {
      // Basically, MS-IME tries to retrieve whole composition string rect
      // at deciding suggest window immediately after unlocking the document.
      // However, in e10s mode, the content hasn't updated yet in most cases.
      // Therefore, if the first character at the retrieving range rect is
      // available, we should use it as the result.
      if ((kIsMSOfficeJapaneseIME2010 ||
           sDoNotReturnNoLayoutErrorToMSJapaneseIMEAtFirstChar) &&
          acpStart < acpEnd) {
        acpEnd = acpStart;
        dontReturnNoLayoutError = true;
      }
      // Although, the condition is not clear, MS-IME sometimes retrieves the
      // caret rect immediately after modifying the composition string but
      // before unlocking the document.  In such case, we should return the
      // nearest character rect.
      else if ((kIsMSOfficeJapaneseIME2010 ||
                sDoNotReturnNoLayoutErrorToMSJapaneseIMEAtCaret) &&
               acpStart == acpEnd &&
               selectionForTSF.IsCollapsed() &&
               selectionForTSF.EndOffset() == acpEnd) {
        if (mContentForTSF.MinOffsetOfLayoutChanged() > LONG_MAX) {
          MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetTextExt(), FAILED due to the text "
             "is too big for TSF (cannot treat modified offset as LONG), "
             "mContentForTSF.MinOffsetOfLayoutChanged()=%u",
             this, mContentForTSF.MinOffsetOfLayoutChanged()));
          return E_FAIL;
        }
        int32_t minOffsetOfLayoutChanged =
          static_cast<int32_t>(mContentForTSF.MinOffsetOfLayoutChanged());
        acpEnd = acpStart = std::max(minOffsetOfLayoutChanged - 1, 0);
        dontReturnNoLayoutError = true;
      }
    }
    // ATOK fails to handle TS_E_NOLAYOUT only when it decides the position of
    // suggest window.  In such case, ATOK tries to query rect of whole
    // composition string.
    // XXX For testing with legacy ATOK, we should hack it even if current ATOK
    //     refers native caret rect on windows whose window class is one of
    //     Mozilla window classes and we stop creating native caret for ATOK
    //     because creating native caret causes ATOK refers caret position
    //     when GetTextExt() returns TS_E_NOLAYOUT.
    else if (sDoNotReturnNoLayoutErrorToATOKOfCompositionString &&
             kSink->IsATOKActive() &&
             (!kSink->IsATOKReferringNativeCaretActive() ||
              !sCreateNativeCaretForLegacyATOK) &&
             mComposition.mStart == acpStart &&
             mComposition.EndOffset() == acpEnd) {
      dontReturnNoLayoutError = true;
    }
    // Free ChangJie 2010 and Easy Changjei 1.0.12.0 doesn't handle
    // ITfContextView::GetTextExt() properly.  Prehaps, it's due to the bug of
    // TSF.  We need to check if this is necessary on Windows 10 before
    // disabling this on Windows 10.
    else if ((sDoNotReturnNoLayoutErrorToFreeChangJie &&
              kSink->IsFreeChangJieActive()) ||
             (sDoNotReturnNoLayoutErrorToEasyChangjei &&
              kSink->IsEasyChangjeiActive())) {
      acpEnd = mComposition.mStart;
      acpStart = std::min(acpStart, acpEnd);
      dontReturnNoLayoutError = true;
    }
    // Some Chinese TIPs of Microsoft doesn't show candidate window in e10s
    // mode on Win8 or later.
    else if (IsWin8OrLater() &&
             ((sDoNotReturnNoLayoutErrorToMSTraditionalTIP &&
               (kSink->IsMSChangJieActive() ||
                kSink->IsMSQuickQuickActive())) ||
              (sDoNotReturnNoLayoutErrorToMSSimplifiedTIP &&
                (kSink->IsMSPinyinActive() ||
                 kSink->IsMSWubiActive())))) {
      acpEnd = mComposition.mStart;
      acpStart = std::min(acpStart, acpEnd);
      dontReturnNoLayoutError = true;
    }

    // If we hack the queried range for active TIP, that means we should not
    // return TS_E_NOLAYOUT even if hacked offset is still modified.  So, as
    // far as possible, we should adjust the offset.
    if (dontReturnNoLayoutError) {
      MOZ_ASSERT(mContentForTSF.IsLayoutChanged());
      if (mContentForTSF.MinOffsetOfLayoutChanged() > LONG_MAX) {
        MOZ_LOG(sTextStoreLog, LogLevel::Error,
          ("0x%p   TSFTextStore::GetTextExt(), FAILED due to the text "
           "is too big for TSF (cannot treat modified offset as LONG), "
           "mContentForTSF.MinOffsetOfLayoutChanged()=%u",
           this, mContentForTSF.MinOffsetOfLayoutChanged()));
        return E_FAIL;
      }
      // Note that even if all characters in the editor or the composition
      // string was modified, 0 or start offset of the composition string is
      // useful because it may return caret rect or old character's rect which
      // the user still see.  That must be useful information for TIP.
      int32_t firstModifiedOffset =
        static_cast<int32_t>(mContentForTSF.MinOffsetOfLayoutChanged());
      LONG lastUnmodifiedOffset = std::max(firstModifiedOffset - 1, 0);
      if (mContentForTSF.IsLayoutChangedAt(acpStart)) {
        // If TSF queries text rect in composition string, we should return
        // rect at start of the composition even if its layout is changed.
        if (acpStart >= mComposition.mStart) {
          acpStart = mComposition.mStart;
        }
        // Otherwise, use first character's rect.  Even if there is no
        // characters, the query event will return caret rect instead.
        else {
          acpStart = lastUnmodifiedOffset;
        }
        MOZ_ASSERT(acpStart <= acpEnd);
      }
      if (mContentForTSF.IsLayoutChangedAt(acpEnd)) {
        // Use max larger offset of last unmodified offset or acpStart which
        // may be the first character offset of the composition string.
        acpEnd = std::max(acpStart, lastUnmodifiedOffset);
      }
      MOZ_LOG(sTextStoreLog, LogLevel::Debug,
        ("0x%p   TSFTextStore::GetTextExt() hacked the queried range "
         "for not returning TS_E_NOLAYOUT, new values are: "
         "acpStart=%d, acpEnd=%d", this, acpStart, acpEnd));
    }
  }

  if (!dontReturnNoLayoutError && mContentForTSF.IsLayoutChangedAt(acpEnd)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetTextExt() returned TS_E_NOLAYOUT "
       "(acpEnd=%d)", this, acpEnd));
    mHasReturnedNoLayoutError = true;
    return TS_E_NOLAYOUT;
  }

  if (mDestroyed) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetTextExt() returned TS_E_NOLAYOUT "
       "(acpEnd=%d) because this has already been destroyed",
       this, acpEnd));
    mHasReturnedNoLayoutError = true;
    return TS_E_NOLAYOUT;
  }

  // use eQueryTextRect to get rect in system, screen coordinates
  WidgetQueryContentEvent event(true, eQueryTextRect, mWidget);
  mWidget->InitEvent(event);

  WidgetQueryContentEvent::Options options;
  int64_t startOffset = acpStart;
  if (mComposition.IsComposing()) {
    // If there is a composition, TSF must want character rects related to
    // the composition.  Therefore, we should use insertion point relative
    // query because the composition might be at different position from
    // the position where TSFTextStore believes it at.
    options.mRelativeToInsertionPoint = true;
    startOffset -= mComposition.mStart;
  } else if (!CanAccessActualContentDirectly()) {
    // If TSF/TIP cannot access actual content directly, there may be pending
    // text and/or selection changes which have not been notified TSF yet.
    // Therefore, we should use relative to insertion point query since
    // TSF/TIP computes the offset from the cached selection.
    options.mRelativeToInsertionPoint = true;
    startOffset -= mSelectionForTSF.StartOffset();
  }
  // ContentEventHandler and ContentCache return actual caret rect when
  // the queried range is collapsed and selection is collapsed at the
  // queried range.  Then, its height (in horizontal layout, width in vertical
  // layout) may be different from actual font height of the line.  In such
  // case, users see "dancing" of candidate or suggest window of TIP.
  // For preventing it, we should query text rect with at least 1 length.
  uint32_t length = std::max(static_cast<int32_t>(acpEnd - acpStart), 1);
  event.InitForQueryTextRect(startOffset, length, options);

  DispatchEvent(event);
  if (NS_WARN_IF(!event.mSucceeded)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
       "eQueryTextRect failure", this));
    return TS_E_INVALIDPOS; // but unexpected failure, maybe.
  }

  // IMEs don't like empty rects, fix here
  if (event.mReply.mRect.width <= 0)
    event.mReply.mRect.width = 1;
  if (event.mReply.mRect.height <= 0)
    event.mReply.mRect.height = 1;

  // convert to unclipped screen rect
  nsWindow* refWindow = static_cast<nsWindow*>(
    event.mReply.mFocusedWidget ? event.mReply.mFocusedWidget : mWidget);
  // Result rect is in top level widget coordinates
  refWindow = refWindow->GetTopLevelWindow(false);
  if (!refWindow) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
       "no top level window", this));
    return E_FAIL;
  }

  event.mReply.mRect.MoveBy(refWindow->WidgetToScreenOffset());

  // get bounding screen rect to test for clipping
  if (!GetScreenExtInternal(*prc)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
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

  // ATOK 2011 - 2016 refers native caret position and size on windows whose
  // class name is one of Mozilla's windows for deciding candidate window
  // position.  Therefore, we need to create native caret only when ATOK 2011 -
  // 2016 is active.
  if (sCreateNativeCaretForLegacyATOK &&
      kSink->IsATOKReferringNativeCaretActive() &&
      mComposition.IsComposing() &&
      mComposition.mStart <= acpStart && mComposition.EndOffset() >= acpStart &&
      mComposition.mStart <= acpEnd && mComposition.EndOffset() >= acpEnd) {
    CreateNativeCaret();
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::GetTextExt() succeeded: "
     "*prc={ left=%ld, top=%ld, right=%ld, bottom=%ld }, *pfClipped=%s",
     this, prc->left, prc->top, prc->right, prc->bottom,
     GetBoolName(*pfClipped)));

  return S_OK;
}

STDMETHODIMP
TSFTextStore::GetScreenExt(TsViewCookie vcView,
                           RECT* prc)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::GetScreenExt(vcView=%ld, prc=0x%p)",
     this, vcView, prc));

  if (vcView != TEXTSTORE_DEFAULT_VIEW) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetScreenExt() FAILED due to "
       "called with invalid view", this));
    return E_INVALIDARG;
  }

  if (!prc) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetScreenExt() FAILED due to "
       "null argument", this));
    return E_INVALIDARG;
  }

  if (mDestroyed) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetScreenExt() returns empty rect "
       "due to already destroyed", this));
    prc->left = prc->top = prc->right = prc->left = 0;
    return S_OK;
  }

  if (!GetScreenExtInternal(*prc)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetScreenExt() FAILED due to "
       "GetScreenExtInternal() failure", this));
    return E_FAIL;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::GetScreenExt() succeeded: "
     "*prc={ left=%ld, top=%ld, right=%ld, bottom=%ld }",
     this, prc->left, prc->top, prc->right, prc->bottom));
  return S_OK;
}

bool
TSFTextStore::GetScreenExtInternal(RECT& aScreenExt)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::GetScreenExtInternal()", this));

  MOZ_ASSERT(!mDestroyed);

  // use NS_QUERY_EDITOR_RECT to get rect in system, screen coordinates
  WidgetQueryContentEvent event(true, eQueryEditorRect, mWidget);
  mWidget->InitEvent(event);
  DispatchEvent(event);
  if (!event.mSucceeded) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetScreenExtInternal() FAILED due to "
       "eQueryEditorRect failure", this));
    return false;
  }

  nsWindow* refWindow = static_cast<nsWindow*>(
    event.mReply.mFocusedWidget ?
      event.mReply.mFocusedWidget : mWidget);
  // Result rect is in top level widget coordinates
  refWindow = refWindow->GetTopLevelWindow(false);
  if (!refWindow) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetScreenExtInternal() FAILED due to "
       "no top level window", this));
    return false;
  }

  LayoutDeviceIntRect boundRect = refWindow->GetClientBounds();
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

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::GetScreenExtInternal() succeeded: "
     "aScreenExt={ left=%ld, top=%ld, right=%ld, bottom=%ld }",
     this, aScreenExt.left, aScreenExt.top,
     aScreenExt.right, aScreenExt.bottom));
  return true;
}

STDMETHODIMP
TSFTextStore::GetWnd(TsViewCookie vcView,
                     HWND* phwnd)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::GetWnd(vcView=%ld, phwnd=0x%p), "
     "mWidget=0x%p",
     this, vcView, phwnd, mWidget.get()));

  if (vcView != TEXTSTORE_DEFAULT_VIEW) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetWnd() FAILED due to "
       "called with invalid view", this));
    return E_INVALIDARG;
  }

  if (!phwnd) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::GetScreenExt() FAILED due to "
       "null argument", this));
    return E_INVALIDARG;
  }

  *phwnd = mWidget ? mWidget->GetWindowHandle() : nullptr;

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::GetWnd() succeeded: *phwnd=0x%p",
     this, static_cast<void*>(*phwnd)));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::InsertTextAtSelection(DWORD dwFlags,
                                    const WCHAR* pchText,
                                    ULONG cch,
                                    LONG* pacpStart,
                                    LONG* pacpEnd,
                                    TS_TEXTCHANGE* pChange)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::InsertTextAtSelection(dwFlags=%s, "
     "pchText=0x%p \"%s\", cch=%lu, pacpStart=0x%p, pacpEnd=0x%p, "
     "pChange=0x%p), IsComposing()=%s",
     this, dwFlags == 0 ? "0" :
           dwFlags == TF_IAS_NOQUERY ? "TF_IAS_NOQUERY" :
           dwFlags == TF_IAS_QUERYONLY ? "TF_IAS_QUERYONLY" : "Unknown",
     pchText,
     pchText && cch ? GetEscapedUTF8String(pchText, cch).get() : "",
     cch, pacpStart, pacpEnd, pChange,
     GetBoolName(mComposition.IsComposing())));

  if (cch && !pchText) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
       "null pchText", this));
    return E_INVALIDARG;
  }

  if (TS_IAS_QUERYONLY == dwFlags) {
    if (!IsReadLocked()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
         "not locked (read)", this));
      return TS_E_NOLOCK;
    }

    if (!pacpStart || !pacpEnd) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
         "null argument", this));
      return E_INVALIDARG;
    }

    // Get selection first
    Selection& selectionForTSF = SelectionForTSFRef();
    if (selectionForTSF.IsDirty()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
         "SelectionForTSFRef() failure", this));
      return E_FAIL;
    }

    // Simulate text insertion
    *pacpStart = selectionForTSF.StartOffset();
    *pacpEnd = selectionForTSF.EndOffset();
    if (pChange) {
      pChange->acpStart = selectionForTSF.StartOffset();
      pChange->acpOldEnd = selectionForTSF.EndOffset();
      pChange->acpNewEnd =
        selectionForTSF.StartOffset() + static_cast<LONG>(cch);
    }
  } else {
    if (!IsReadWriteLocked()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
         "not locked (read-write)", this));
      return TS_E_NOLOCK;
    }

    if (!pChange) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
         "null pChange", this));
      return E_INVALIDARG;
    }

    if (TS_IAS_NOQUERY != dwFlags && (!pacpStart || !pacpEnd)) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
         "null argument", this));
      return E_INVALIDARG;
    }

    if (!InsertTextAtSelectionInternal(nsDependentSubstring(pchText, cch),
                                       pChange)) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
         "InsertTextAtSelectionInternal() failure", this));
      return E_FAIL;
    }

    if (TS_IAS_NOQUERY != dwFlags) {
      *pacpStart = pChange->acpStart;
      *pacpEnd = pChange->acpNewEnd;
    }
  }
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::InsertTextAtSelection() succeeded: "
     "*pacpStart=%ld, *pacpEnd=%ld, "
     "*pChange={ acpStart=%ld, acpOldEnd=%ld, acpNewEnd=%ld })",
     this, pacpStart ? *pacpStart : 0, pacpEnd ? *pacpEnd : 0,
     pChange ? pChange->acpStart: 0, pChange ? pChange->acpOldEnd : 0,
     pChange ? pChange->acpNewEnd : 0));
  return S_OK;
}

bool
TSFTextStore::InsertTextAtSelectionInternal(const nsAString& aInsertStr,
                                            TS_TEXTCHANGE* aTextChange)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::InsertTextAtSelectionInternal("
     "aInsertStr=\"%s\", aTextChange=0x%p), IsComposing=%s",
     this, GetEscapedUTF8String(aInsertStr).get(), aTextChange,
     GetBoolName(mComposition.IsComposing())));

  Content& contentForTSF = ContentForTSFRef();
  if (!contentForTSF.IsInitialized()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::InsertTextAtSelectionInternal() failed "
       "due to ContentForTSFRef() failure()", this));
    return false;
  }

  TS_SELECTION_ACP oldSelection = contentForTSF.Selection().ACP();
  if (!mComposition.IsComposing()) {
    // Use a temporary composition to contain the text
    PendingAction* compositionStart = mPendingActions.AppendElement();
    compositionStart->mType = PendingAction::COMPOSITION_START;
    compositionStart->mSelectionStart = oldSelection.acpStart;
    compositionStart->mSelectionLength =
      oldSelection.acpEnd - oldSelection.acpStart;
    compositionStart->mAdjustSelection = false;

    PendingAction* compositionEnd = mPendingActions.AppendElement();
    compositionEnd->mType = PendingAction::COMPOSITION_END;
    compositionEnd->mData = aInsertStr;

    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::InsertTextAtSelectionInternal() "
       "appending pending compositionstart and compositionend... "
       "PendingCompositionStart={ mSelectionStart=%d, "
       "mSelectionLength=%d }, PendingCompositionEnd={ mData=\"%s\" "
       "(Length()=%u) }",
       this, compositionStart->mSelectionStart,
       compositionStart->mSelectionLength,
       GetEscapedUTF8String(compositionEnd->mData).get(),
       compositionEnd->mData.Length()));
  }

  contentForTSF.ReplaceSelectedTextWith(aInsertStr);

  if (aTextChange) {
    aTextChange->acpStart = oldSelection.acpStart;
    aTextChange->acpOldEnd = oldSelection.acpEnd;
    aTextChange->acpNewEnd = contentForTSF.Selection().EndOffset();
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::InsertTextAtSelectionInternal() "
     "succeeded: mWidget=0x%p, mWidget->Destroyed()=%s, aTextChange={ "
     "acpStart=%ld, acpOldEnd=%ld, acpNewEnd=%ld }",
     this, mWidget.get(),
     GetBoolName(mWidget ? mWidget->Destroyed() : true),
     aTextChange ? aTextChange->acpStart : 0,
     aTextChange ? aTextChange->acpOldEnd : 0,
     aTextChange ? aTextChange->acpNewEnd : 0));
  return true;
}

STDMETHODIMP
TSFTextStore::InsertEmbeddedAtSelection(DWORD dwFlags,
                                        IDataObject* pDataObject,
                                        LONG* pacpStart,
                                        LONG* pacpEnd,
                                        TS_TEXTCHANGE* pChange)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::InsertEmbeddedAtSelection() called "
     "but not supported (E_NOTIMPL)", this));

  // embedded objects are not supported
  return E_NOTIMPL;
}

HRESULT
TSFTextStore::RecordCompositionStartAction(ITfCompositionView* aComposition,
                                           ITfRange* aRange,
                                           bool aPreserveSelection)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::RecordCompositionStartAction("
     "aComposition=0x%p, aRange=0x%p, aPreserveSelection=%s), "
     "mComposition.mView=0x%p",
     this, aComposition, aRange, GetBoolName(aPreserveSelection),
     mComposition.mView.get()));

  LONG start = 0, length = 0;
  HRESULT hr = GetRangeExtent(aRange, &start, &length);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RecordCompositionStartAction() FAILED "
       "due to GetRangeExtent() failure", this));
    return hr;
  }

  return RecordCompositionStartAction(aComposition, start, length,
                                      aPreserveSelection);
}

HRESULT
TSFTextStore::RecordCompositionStartAction(ITfCompositionView* aComposition,
                                           LONG aStart,
                                           LONG aLength,
                                           bool aPreserveSelection)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::RecordCompositionStartAction("
     "aComposition=0x%p, aStart=%d, aLength=%d, aPreserveSelection=%s), "
     "mComposition.mView=0x%p",
     this, aComposition, aStart, aLength, GetBoolName(aPreserveSelection),
     mComposition.mView.get()));

  Content& contentForTSF = ContentForTSFRef();
  if (!contentForTSF.IsInitialized()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RecordCompositionStartAction() FAILED "
       "due to ContentForTSFRef() failure", this));
    return E_FAIL;
  }

  CompleteLastActionIfStillIncomplete();

  // TIP may have inserted text at selection before calling
  // OnStartComposition().  In this case, we've already created a pair of
  // pending compositionstart and pending compositionend.  If the pending
  // compositionstart occurred same range as this composition, it was the
  // start of this composition.  In such case, we should cancel the pending
  // compositionend and start composition normally.
  if (!aPreserveSelection &&
      WasTextInsertedWithoutCompositionAt(aStart, aLength)) {
    const PendingAction& pendingCompositionEnd = mPendingActions.LastElement();
    const PendingAction& pendingCompositionStart =
      mPendingActions[mPendingActions.Length() - 2];
    contentForTSF.RestoreCommittedComposition(
      aComposition, pendingCompositionStart, pendingCompositionEnd);
    mPendingActions.RemoveElementAt(mPendingActions.Length() - 1);
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::RecordCompositionStartAction() "
       "succeeded: restoring the committed string as composing string, "
       "mComposition={ mStart=%ld, mString.Length()=%ld, "
       "mSelectionForTSF={ acpStart=%ld, acpEnd=%ld, style.ase=%s, "
       "style.fInterimChar=%s } }",
       this, mComposition.mStart, mComposition.mString.Length(),
       mSelectionForTSF.StartOffset(), mSelectionForTSF.EndOffset(),
       GetActiveSelEndName(mSelectionForTSF.ActiveSelEnd()),
       GetBoolName(mSelectionForTSF.IsInterimChar())));
    return S_OK;
  }

  PendingAction* action = mPendingActions.AppendElement();
  action->mType = PendingAction::COMPOSITION_START;
  action->mSelectionStart = aStart;
  action->mSelectionLength = aLength;

  Selection& selectionForTSF = SelectionForTSFRef();
  if (selectionForTSF.IsDirty()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RecordCompositionStartAction() FAILED "
       "due to SelectionForTSFRef() failure", this));
    action->mAdjustSelection = true;
  } else if (selectionForTSF.MinOffset() != aStart ||
             selectionForTSF.MaxOffset() != aStart + aLength) {
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

  contentForTSF.StartComposition(aComposition, *action, aPreserveSelection);
  action->mData = mComposition.mString;

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::RecordCompositionStartAction() succeeded: "
     "mComposition={ mStart=%ld, mString.Length()=%ld, "
     "mSelectionForTSF={ acpStart=%ld, acpEnd=%ld, style.ase=%s, "
     "style.fInterimChar=%s } }",
     this, mComposition.mStart, mComposition.mString.Length(),
     mSelectionForTSF.StartOffset(), mSelectionForTSF.EndOffset(),
     GetActiveSelEndName(mSelectionForTSF.ActiveSelEnd()),
     GetBoolName(mSelectionForTSF.IsInterimChar())));
  return S_OK;
}

HRESULT
TSFTextStore::RecordCompositionEndAction()
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::RecordCompositionEndAction(), "
     "mComposition={ mView=0x%p, mString=\"%s\" }",
     this, mComposition.mView.get(),
     GetEscapedUTF8String(mComposition.mString).get()));

  MOZ_ASSERT(mComposition.IsComposing());

  CompleteLastActionIfStillIncomplete();
  PendingAction* action = mPendingActions.AppendElement();
  action->mType = PendingAction::COMPOSITION_END;
  action->mData = mComposition.mString;

  Content& contentForTSF = ContentForTSFRef();
  if (!contentForTSF.IsInitialized()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::RecordCompositionEndAction() FAILED due "
       "to ContentForTSFRef() failure", this));
    return E_FAIL;
  }
  contentForTSF.EndComposition(*action);

  // If this composition was restart but the composition doesn't modify
  // anything, we should remove the pending composition for preventing to
  // dispatch redundant composition events.
  for (size_t i = mPendingActions.Length(), j = 1; i > 0; --i, ++j) {
    PendingAction& pendingAction = mPendingActions[i - 1];
    if (pendingAction.mType == PendingAction::COMPOSITION_START) {
      if (pendingAction.mData != action->mData) {
        break;
      }
      // When only setting selection is necessary, we should append it.
      if (pendingAction.mAdjustSelection) {
        PendingAction* setSelection = mPendingActions.AppendElement();
        setSelection->mType = PendingAction::SET_SELECTION;
        setSelection->mSelectionStart = pendingAction.mSelectionStart;
        setSelection->mSelectionLength = pendingAction.mSelectionLength;
        setSelection->mSelectionReversed = false;
      }
      // Remove the redundant pending composition.
      mPendingActions.RemoveElementsAt(i - 1, j);
      MOZ_LOG(sTextStoreLog, LogLevel::Info,
        ("0x%p   TSFTextStore::RecordCompositionEndAction(), "
         "succeeded, but the composition was canceled due to redundant",
         this));
      return S_OK;
    }
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::RecordCompositionEndAction(), succeeded",
     this));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::OnStartComposition(ITfCompositionView* pComposition,
                                 BOOL* pfOk)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::OnStartComposition(pComposition=0x%p, "
     "pfOk=0x%p), mComposition.mView=0x%p",
     this, pComposition, pfOk, mComposition.mView.get()));

  AutoPendingActionAndContentFlusher flusher(this);

  *pfOk = FALSE;

  // Only one composition at a time
  if (mComposition.IsComposing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::OnStartComposition() FAILED due to "
       "there is another composition already (but returns S_OK)", this));
    return S_OK;
  }

  RefPtr<ITfRange> range;
  HRESULT hr = pComposition->GetRange(getter_AddRefs(range));
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::OnStartComposition() FAILED due to "
       "pComposition->GetRange() failure", this));
    return hr;
  }
  hr = RecordCompositionStartAction(pComposition, range, false);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::OnStartComposition() FAILED due to "
       "RecordCompositionStartAction() failure", this));
    return hr;
  }

  *pfOk = TRUE;
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::OnStartComposition() succeeded", this));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::OnUpdateComposition(ITfCompositionView* pComposition,
                                  ITfRange* pRangeNew)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::OnUpdateComposition(pComposition=0x%p, "
     "pRangeNew=0x%p), mComposition.mView=0x%p",
     this, pComposition, pRangeNew, mComposition.mView.get()));

  AutoPendingActionAndContentFlusher flusher(this);

  if (!mDocumentMgr || !mContext) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::OnUpdateComposition() FAILED due to "
       "not ready for the composition", this));
    return E_UNEXPECTED;
  }
  if (!mComposition.IsComposing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::OnUpdateComposition() FAILED due to "
       "no active composition", this));
    return E_UNEXPECTED;
  }
  if (mComposition.mView != pComposition) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::OnUpdateComposition() FAILED due to "
       "different composition view specified", this));
    return E_UNEXPECTED;
  }

  // pRangeNew is null when the update is not complete
  if (!pRangeNew) {
    PendingAction* action = LastOrNewPendingCompositionUpdate();
    action->mIncomplete = true;
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::OnUpdateComposition() succeeded but "
       "not complete", this));
    return S_OK;
  }

  HRESULT hr = RestartCompositionIfNecessary(pRangeNew);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::OnUpdateComposition() FAILED due to "
       "RestartCompositionIfNecessary() failure", this));
    return hr;
  }

  hr = RecordCompositionUpdateAction();
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::OnUpdateComposition() FAILED due to "
       "RecordCompositionUpdateAction() failure", this));
    return hr;
  }

  if (MOZ_LOG_TEST(sTextStoreLog, LogLevel::Info)) {
    Selection& selectionForTSF = SelectionForTSFRef();
    if (selectionForTSF.IsDirty()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::OnUpdateComposition() FAILED due to "
         "SelectionForTSFRef() failure", this));
      return E_FAIL;
    }
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::OnUpdateComposition() succeeded: "
       "mComposition={ mStart=%ld, mString=\"%s\" }, "
       "SelectionForTSFRef()={ acpStart=%ld, acpEnd=%ld, style.ase=%s }",
       this, mComposition.mStart,
       GetEscapedUTF8String(mComposition.mString).get(),
       selectionForTSF.StartOffset(), selectionForTSF.EndOffset(),
       GetActiveSelEndName(selectionForTSF.ActiveSelEnd())));
  }
  return S_OK;
}

STDMETHODIMP
TSFTextStore::OnEndComposition(ITfCompositionView* pComposition)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::OnEndComposition(pComposition=0x%p), "
     "mComposition={ mView=0x%p, mString=\"%s\" }",
     this, pComposition, mComposition.mView.get(),
     GetEscapedUTF8String(mComposition.mString).get()));

  AutoPendingActionAndContentFlusher flusher(this);

  if (!mComposition.IsComposing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::OnEndComposition() FAILED due to "
       "no active composition", this));
    return E_UNEXPECTED;
  }

  if (mComposition.mView != pComposition) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::OnEndComposition() FAILED due to "
       "different composition view specified", this));
    return E_UNEXPECTED;
  }

  HRESULT hr = RecordCompositionEndAction();
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::OnEndComposition() FAILED due to "
       "RecordCompositionEndAction() failure", this));
    return hr;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::OnEndComposition(), succeeded", this));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::AdviseMouseSink(ITfRangeACP* range,
                              ITfMouseSink* pSink,
                              DWORD* pdwCookie)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::AdviseMouseSink(range=0x%p, pSink=0x%p, "
     "pdwCookie=0x%p)", this, range, pSink, pdwCookie));

  if (!pdwCookie) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::AdviseMouseSink() FAILED due to the "
       "pdwCookie is null", this));
    return E_INVALIDARG;
  }
  // Initialize the result with invalid cookie for safety.
  *pdwCookie = MouseTracker::kInvalidCookie;

  if (!range) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::AdviseMouseSink() FAILED due to the "
       "range is null", this));
    return E_INVALIDARG;
  }
  if (!pSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::AdviseMouseSink() FAILED due to the "
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
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::AdviseMouseSink() FAILED due to "
         "failure of MouseTracker::Init()", this));
      return hr;
    }
  }
  HRESULT hr = tracker->AdviseSink(this, range, pSink);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::AdviseMouseSink() FAILED due to failure "
       "of MouseTracker::Init()", this));
    return hr;
  }
  *pdwCookie = tracker->Cookie();
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::AdviseMouseSink(), succeeded, "
     "*pdwCookie=%d", this, *pdwCookie));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::UnadviseMouseSink(DWORD dwCookie)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p TSFTextStore::UnadviseMouseSink(dwCookie=%d)",
     this, dwCookie));
  if (dwCookie == MouseTracker::kInvalidCookie) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::UnadviseMouseSink() FAILED due to "
       "the cookie is invalid value", this));
    return E_INVALIDARG;
  }
  // The cookie value must be an index of mMouseTrackers.
  // We can use this shortcut for now.
  if (static_cast<size_t>(dwCookie) >= mMouseTrackers.Length()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::UnadviseMouseSink() FAILED due to "
       "the cookie is too large value", this));
    return E_INVALIDARG;
  }
  MouseTracker& tracker = mMouseTrackers[dwCookie];
  if (!tracker.IsUsing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::UnadviseMouseSink() FAILED due to "
       "the found tracker uninstalled already", this));
    return E_INVALIDARG;
  }
  tracker.UnadviseSink();
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::UnadviseMouseSink(), succeeded", this));
  return S_OK;
}

// static
nsresult
TSFTextStore::OnFocusChange(bool aGotFocus,
                            nsWindowBase* aFocusedWidget,
                            const InputContext& aContext)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("  TSFTextStore::OnFocusChange(aGotFocus=%s, "
     "aFocusedWidget=0x%p, aContext=%s), "
     "sThreadMgr=0x%p, sEnabledTextStore=0x%p",
     GetBoolName(aGotFocus), aFocusedWidget,
     GetInputContextString(aContext).get(),
     sThreadMgr.get(), sEnabledTextStore.get()));

  if (NS_WARN_IF(!IsInTSFMode())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<ITfDocumentMgr> prevFocusedDocumentMgr;

  // If currently sEnableTextStore has focus, notifies TSF of losing focus.
  if (ThinksHavingFocus()) {
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
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
        ("  TSFTextStore::OnFocusChange() FAILED due to "
         "ITfThreadMgr::SetFocus() failure"));
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

  // If an editor is getting focus, create new TextStore and set focus.
  if (NS_WARN_IF(!CreateAndSetFocus(aFocusedWidget, aContext))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::OnFocusChange() FAILED due to "
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
TSFTextStore::CreateAndSetFocus(nsWindowBase* aFocusedWidget,
                                const InputContext& aContext)
{
  // TSF might do something which causes that we need to access static methods
  // of TSFTextStore.  At that time, sEnabledTextStore may be necessary.
  // So, we should set sEnabledTextStore directly.
  RefPtr<TSFTextStore> textStore = new TSFTextStore();
  sEnabledTextStore = textStore;
  if (NS_WARN_IF(!textStore->Init(aFocusedWidget, aContext))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::CreateAndSetFocus() FAILED due to "
       "TSFTextStore::Init() failure"));
    return false;
  }
  if (NS_WARN_IF(!textStore->mDocumentMgr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::CreateAndSetFocus() FAILED due to "
       "invalid TSFTextStore::mDocumentMgr"));
    return false;
  }
  if (aContext.mIMEState.mEnabled == IMEState::PASSWORD) {
    MarkContextAsKeyboardDisabled(textStore->mContext);
    RefPtr<ITfContext> topContext;
    textStore->mDocumentMgr->GetTop(getter_AddRefs(topContext));
    if (topContext && topContext != textStore->mContext) {
      MarkContextAsKeyboardDisabled(topContext);
    }
  }

  HRESULT hr;
  {
    // Windows 10's softwware keyboard requires that SetSelection must be
    // always successful into SetFocus.  If returning error, it might crash
    // into TextInputFramework.dll.
    AutoSetTemporarySelection setSelection(textStore->SelectionForTSFRef());

    hr = sThreadMgr->SetFocus(textStore->mDocumentMgr);
  }

  if (NS_WARN_IF(FAILED(hr))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::CreateAndSetFocus() FAILED due to "
       "ITfTheadMgr::SetFocus() failure"));
    return false;
  }
  // Use AssociateFocus() for ensuring that any native focus event
  // never steal focus from our documentMgr.
  RefPtr<ITfDocumentMgr> prevFocusedDocumentMgr;
  hr = sThreadMgr->AssociateFocus(aFocusedWidget->GetWindowHandle(),
                                  textStore->mDocumentMgr,
                                  getter_AddRefs(prevFocusedDocumentMgr));
  if (NS_WARN_IF(FAILED(hr))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::CreateAndSetFocus() FAILED due to "
       "ITfTheadMgr::AssociateFocus() failure"));
    return false;
  }

  if (textStore->mSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("  TSFTextStore::CreateAndSetFocus(), calling "
       "ITextStoreACPSink::OnLayoutChange(TS_LC_CREATE) for 0x%p...",
       textStore.get()));
    textStore->mSink->OnLayoutChange(TS_LC_CREATE, TEXTSTORE_DEFAULT_VIEW);
  }
  return true;
}

// static
IMENotificationRequests
TSFTextStore::GetIMENotificationRequests()
{
  if (sThreadMgr && sEnabledTextStore && sEnabledTextStore->mDocumentMgr) {
    RefPtr<ITfDocumentMgr> docMgr;
    sThreadMgr->GetFocus(getter_AddRefs(docMgr));
    if (docMgr == sEnabledTextStore->mDocumentMgr) {
      return IMENotificationRequests(
               IMENotificationRequests::NOTIFY_TEXT_CHANGE |
               IMENotificationRequests::NOTIFY_POSITION_CHANGE |
               IMENotificationRequests::NOTIFY_MOUSE_BUTTON_EVENT_ON_CHAR |
               IMENotificationRequests::NOTIFY_DURING_DEACTIVE);
    }
  }
  return IMENotificationRequests();
}

nsresult
TSFTextStore::OnTextChangeInternal(const IMENotification& aIMENotification)
{
  const TextChangeDataBase& textChangeData = aIMENotification.mTextChangeData;

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::OnTextChangeInternal(aIMENotification={ "
     "mMessage=0x%08X, mTextChangeData={ mStartOffset=%lu, "
     "mRemovedEndOffset=%lu, mAddedEndOffset=%lu, "
     "mCausedOnlyByComposition=%s, "
     "mIncludingChangesDuringComposition=%s, "
     "mIncludingChangesWithoutComposition=%s }), "
     "mDestroyed=%s, mSink=0x%p, mSinkMask=%s, "
     "mComposition.IsComposing()=%s",
     this, aIMENotification.mMessage,
     textChangeData.mStartOffset,
     textChangeData.mRemovedEndOffset,
     textChangeData.mAddedEndOffset,
     GetBoolName(textChangeData.mCausedOnlyByComposition),
     GetBoolName(textChangeData.mIncludingChangesDuringComposition),
     GetBoolName(textChangeData.mIncludingChangesWithoutComposition),
     GetBoolName(mDestroyed),
     mSink.get(),
     GetSinkMaskNameStr(mSinkMask).get(),
     GetBoolName(mComposition.IsComposing())));

  if (mDestroyed) {
    // If this instance is already destroyed, we shouldn't notify TSF of any
    // changes.
    return NS_OK;
  }

  mDeferNotifyingTSF = false;

  // Different from selection change, we don't modify anything with text
  // change data.  Therefore, if neither TSF not TIP wants text change
  // notifications, we don't need to store the changes.
  if (!mSink || !(mSinkMask & TS_AS_TEXT_CHANGE)) {
    return NS_OK;
  }

  // Merge any text change data even if it's caused by composition.
  mPendingTextChangeData.MergeWith(textChangeData);

  MaybeFlushPendingNotifications();

  return NS_OK;
}

void
TSFTextStore::NotifyTSFOfTextChange()
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(!IsReadLocked());
  MOZ_ASSERT(!mComposition.IsComposing());
  MOZ_ASSERT(mPendingTextChangeData.IsValid());

  // If the text changes are caused only by composition, we don't need to
  // notify TSF of the text changes.
  if (mPendingTextChangeData.mCausedOnlyByComposition) {
    mPendingTextChangeData.Clear();
    return;
  }

  // First, forget cached selection.
  mSelectionForTSF.MarkDirty();

  // For making it safer, we should check if there is a valid sink to receive
  // text change notification.
  if (NS_WARN_IF(!mSink) || NS_WARN_IF(!(mSinkMask & TS_AS_TEXT_CHANGE))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::NotifyTSFOfTextChange() FAILED due to "
       "mSink is not ready to call ITextStoreACPSink::OnTextChange()...",
       this));
    mPendingTextChangeData.Clear();
    return;
  }

  if (NS_WARN_IF(!mPendingTextChangeData.IsInInt32Range())) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::NotifyTSFOfTextChange() FAILED due to "
       "offset is too big for calling "
       "ITextStoreACPSink::OnTextChange()...",
       this));
    mPendingTextChangeData.Clear();
    return;
   }

  TS_TEXTCHANGE textChange;
  textChange.acpStart =
    static_cast<LONG>(mPendingTextChangeData.mStartOffset);
  textChange.acpOldEnd =
    static_cast<LONG>(mPendingTextChangeData.mRemovedEndOffset);
  textChange.acpNewEnd =
    static_cast<LONG>(mPendingTextChangeData.mAddedEndOffset);
  mPendingTextChangeData.Clear();

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::NotifyTSFOfTextChange(), calling "
     "ITextStoreACPSink::OnTextChange(0, { acpStart=%ld, acpOldEnd=%ld, "
     "acpNewEnd=%ld })...", this, textChange.acpStart,
     textChange.acpOldEnd, textChange.acpNewEnd));
  mSink->OnTextChange(0, &textChange);
}

nsresult
TSFTextStore::OnSelectionChangeInternal(const IMENotification& aIMENotification)
{
  const SelectionChangeDataBase& selectionChangeData =
    aIMENotification.mSelectionChangeData;
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::OnSelectionChangeInternal("
     "aIMENotification={ mSelectionChangeData={ mOffset=%lu, "
     "Length()=%lu, mReversed=%s, mWritingMode=%s, "
     "mCausedByComposition=%s, mCausedBySelectionEvent=%s, "
     "mOccurredDuringComposition=%s } }), mDestroyed=%s, "
     "mSink=0x%p, mSinkMask=%s, mIsRecordingActionsWithoutLock=%s, "
     "mComposition.IsComposing()=%s",
     this, selectionChangeData.mOffset, selectionChangeData.Length(),
     GetBoolName(selectionChangeData.mReversed),
     GetWritingModeName(selectionChangeData.GetWritingMode()).get(),
     GetBoolName(selectionChangeData.mCausedByComposition),
     GetBoolName(selectionChangeData.mCausedBySelectionEvent),
     GetBoolName(selectionChangeData.mOccurredDuringComposition),
     GetBoolName(mDestroyed),
     mSink.get(), GetSinkMaskNameStr(mSinkMask).get(),
     GetBoolName(mIsRecordingActionsWithoutLock),
     GetBoolName(mComposition.IsComposing())));

  if (mDestroyed) {
    // If this instance is already destroyed, we shouldn't notify TSF of any
    // changes.
    return NS_OK;
  }

  mDeferNotifyingTSF = false;

  // Assign the new selection change data to the pending selection change data
  // because only the latest selection data is necessary.
  // Note that this is necessary to update mSelectionForTSF.  Therefore, even if
  // neither TSF nor TIP wants selection change notifications, we need to
  // store the selection information.
  mPendingSelectionChangeData.Assign(selectionChangeData);

  // Flush remaining pending notifications here if it's possible.
  MaybeFlushPendingNotifications();

  return NS_OK;
}

void
TSFTextStore::NotifyTSFOfSelectionChange()
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(!IsReadLocked());
  MOZ_ASSERT(!mComposition.IsComposing());
  MOZ_ASSERT(mPendingSelectionChangeData.IsValid());

  // If selection range isn't actually changed, we don't need to notify TSF
  // of this selection change.
  if (!mSelectionForTSF.SetSelection(
                          mPendingSelectionChangeData.mOffset,
                          mPendingSelectionChangeData.Length(),
                          mPendingSelectionChangeData.mReversed,
                          mPendingSelectionChangeData.GetWritingMode())) {
    mPendingSelectionChangeData.Clear();
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::NotifyTSFOfSelectionChange(), "
       "selection isn't actually changed.", this));
    return;
  }

  mPendingSelectionChangeData.Clear();

  if (!mSink || !(mSinkMask & TS_AS_SEL_CHANGE)) {
    return;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::NotifyTSFOfSelectionChange(), calling "
     "ITextStoreACPSink::OnSelectionChange()...", this));
  mSink->OnSelectionChange();
}

nsresult
TSFTextStore::OnLayoutChangeInternal()
{
  if (mDestroyed) {
    // If this instance is already destroyed, we shouldn't notify TSF of any
    // changes.
    return NS_OK;
  }

  NS_ENSURE_TRUE(mContext, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSink, NS_ERROR_FAILURE);

  mDeferNotifyingTSF = false;

  nsresult rv = NS_OK;

  // We need to notify TSF of layout change even if the document is locked.
  // So, don't use MaybeFlushPendingNotifications() for flushing pending
  // layout change.
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::OnLayoutChangeInternal(), calling "
     "NotifyTSFOfLayoutChange()...", this));
  if (NS_WARN_IF(!NotifyTSFOfLayoutChange())) {
    rv = NS_ERROR_FAILURE;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::OnLayoutChangeInternal(), calling "
     "MaybeFlushPendingNotifications()...", this));
  MaybeFlushPendingNotifications();

  return rv;
}

bool
TSFTextStore::NotifyTSFOfLayoutChange()
{
  MOZ_ASSERT(!mDestroyed);

  // If we're waiting a query of layout information from TIP, it means that
  // we've returned TS_E_NOLAYOUT error.
  bool returnedNoLayoutError =
    mHasReturnedNoLayoutError || mWaitingQueryLayout;

  // If we returned TS_E_NOLAYOUT, TIP should query the computed layout again.
  mWaitingQueryLayout = returnedNoLayoutError;

  // For avoiding to call this method again at unlocking the document during
  // calls of OnLayoutChange(), reset mHasReturnedNoLayoutError.
  mHasReturnedNoLayoutError = false;

  // Now, layout has been computed.  We should notify mContentForTSF for
  // making GetTextExt() and GetACPFromPoint() not return TS_E_NOLAYOUT.
  if (mContentForTSF.IsInitialized()) {
    mContentForTSF.OnLayoutChanged();
  }

  // Now, the caret position is different from ours.  Destroy the native caret
  // if there is.
  MaybeDestroyNativeCaret();

  // This method should return true if either way succeeds.
  bool ret = true;

  if (mSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::NotifyTSFOfLayoutChange(), "
       "calling ITextStoreACPSink::OnLayoutChange()...",
       this));
    HRESULT hr = mSink->OnLayoutChange(TS_LC_CHANGE, TEXTSTORE_DEFAULT_VIEW);
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::NotifyTSFOfLayoutChange(), "
       "called ITextStoreACPSink::OnLayoutChange()",
       this));
    ret = SUCCEEDED(hr);
  }

  // The layout change caused by composition string change should cause
  // calling ITfContextOwnerServices::OnLayoutChange() too.
  if (returnedNoLayoutError && mContext) {
    RefPtr<ITfContextOwnerServices> service;
    mContext->QueryInterface(IID_ITfContextOwnerServices,
                             getter_AddRefs(service));
    if (service) {
      MOZ_LOG(sTextStoreLog, LogLevel::Info,
        ("0x%p   TSFTextStore::NotifyTSFOfLayoutChange(), "
         "calling ITfContextOwnerServices::OnLayoutChange()...",
         this));
      HRESULT hr = service->OnLayoutChange();
      ret = ret && SUCCEEDED(hr);
      MOZ_LOG(sTextStoreLog, LogLevel::Info,
        ("0x%p   TSFTextStore::NotifyTSFOfLayoutChange(), "
         "called ITfContextOwnerServices::OnLayoutChange()",
         this));
    }
  }

  if (!mWidget || mWidget->Destroyed()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::NotifyTSFOfLayoutChange(), "
       "the widget is destroyed during calling OnLayoutChange()",
       this));
    return ret;
  }

  if (mDestroyed) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::NotifyTSFOfLayoutChange(), "
       "the TSFTextStore instance is destroyed during calling "
       "OnLayoutChange()",
       this));
    return ret;
  }

  // If we returned TS_E_NOLAYOUT again, we need another call of
  // OnLayoutChange() later.  So, let's wait a query from TIP.
  if (mHasReturnedNoLayoutError) {
    mWaitingQueryLayout = true;
  }

  if (!mWaitingQueryLayout) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::NotifyTSFOfLayoutChange(), "
       "succeeded notifying TIP of our layout change",
       this));
    return ret;
  }

  // If we believe that TIP needs to retry to retrieve our layout information
  // later, we should call it with ::PostMessage() hack.
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::NotifyTSFOfLayoutChange(), "
     "posing  MOZ_WM_NOTIY_TSF_OF_LAYOUT_CHANGE for calling "
     "OnLayoutChange() again...", this));
  ::PostMessage(mWidget->GetWindowHandle(),
                MOZ_WM_NOTIY_TSF_OF_LAYOUT_CHANGE,
                reinterpret_cast<WPARAM>(this), 0);

  return true;
}

void
TSFTextStore::NotifyTSFOfLayoutChangeAgain()
{
  // Don't notify TSF of layout change after destroyed.
  if (mDestroyed) {
    mWaitingQueryLayout = false;
    return;
  }

  // Before preforming this method, TIP has accessed our layout information by
  // itself.  In such case, we don't need to call OnLayoutChange() anymore.
  if (!mWaitingQueryLayout) {
    return;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("0x%p   TSFTextStore::NotifyTSFOfLayoutChangeAgain(), "
     "calling NotifyTSFOfLayoutChange()...", this));
  NotifyTSFOfLayoutChange();

  // If TIP didn't retrieved our layout information during a call of
  // NotifyTSFOfLayoutChange(), it means that the TIP already gave up to
  // retry to retrieve layout information or doesn't necessary it anymore.
  // But don't forget that the call may have caused returning TS_E_NOLAYOUT
  // error again.  In such case we still need to call OnLayoutChange() later.
  if (!mHasReturnedNoLayoutError && mWaitingQueryLayout) {
    mWaitingQueryLayout = false;
    MOZ_LOG(sTextStoreLog, LogLevel::Warning,
      ("0x%p   TSFTextStore::NotifyTSFOfLayoutChangeAgain(), "
       "called NotifyTSFOfLayoutChange() but TIP didn't retry to "
       "retrieve the layout information", this));
  } else {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::NotifyTSFOfLayoutChangeAgain(), "
       "called NotifyTSFOfLayoutChange()", this));
  }
}

nsresult
TSFTextStore::OnUpdateCompositionInternal()
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::OnUpdateCompositionInternal(), "
     "mDestroyed=%s, mDeferNotifyingTSF=%s",
     this, GetBoolName(mDestroyed), GetBoolName(mDeferNotifyingTSF)));

  // There are nothing to do after destroyed.
  if (mDestroyed) {
    return NS_OK;
  }

  // If composition is completely finished both in TSF/TIP and the focused
  // editor which may be in a remote process, we can clear the cache until
  // starting next composition.
  if (!mComposition.IsComposing() && !IsComposingInContent()) {
    mDeferClearingContentForTSF = false;
  }
  mDeferNotifyingTSF = false;
  MaybeFlushPendingNotifications();
  return NS_OK;
}

nsresult
TSFTextStore::OnMouseButtonEventInternal(
                const IMENotification& aIMENotification)
{
  if (mDestroyed) {
    // If this instance is already destroyed, we shouldn't notify TSF of any
    // events.
    return NS_OK;
  }

  if (mMouseTrackers.IsEmpty()) {
    return NS_OK;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::OnMouseButtonEventInternal("
     "aIMENotification={ mEventMessage=%s, mOffset=%u, mCursorPos={ "
     "mX=%d, mY=%d }, mCharRect={ mX=%d, mY=%d, mWidth=%d, mHeight=%d }, "
     "mButton=%s, mButtons=%s, mModifiers=%s })",
     this, ToChar(aIMENotification.mMouseButtonEventData.mEventMessage),
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
    aIMENotification.mMouseButtonEventData.mEventMessage == eMouseUp;
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
TSFTextStore::CreateNativeCaret()
{
  MaybeDestroyNativeCaret();

  // Don't create native caret after destroyed.
  if (mDestroyed) {
    return;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::CreateNativeCaret(), "
     "mComposition.IsComposing()=%s",
     this, GetBoolName(mComposition.IsComposing())));

  Selection& selectionForTSF = SelectionForTSFRef();
  if (selectionForTSF.IsDirty()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::CreateNativeCaret() FAILED due to "
       "SelectionForTSFRef() failure", this));
    return;
  }

  WidgetQueryContentEvent queryCaretRect(true, eQueryCaretRect, mWidget);
  mWidget->InitEvent(queryCaretRect);

  WidgetQueryContentEvent::Options options;
  // XXX If this is called without composition and the selection isn't
  //     collapsed, is it OK?
  int64_t caretOffset = selectionForTSF.MaxOffset();
  if (mComposition.IsComposing()) {
    // If there is a composition, use insertion point relative query for
    // deciding caret position because composition might be at different
    // position where TSFTextStore believes it at.
    options.mRelativeToInsertionPoint = true;
    caretOffset -= mComposition.mStart;
  } else if (!CanAccessActualContentDirectly()) {
    // If TSF/TIP cannot access actual content directly, there may be pending
    // text and/or selection changes which have not been notified TSF yet.
    // Therefore, we should use relative to insertion point query since
    // TSF/TIP computes the offset from the cached selection.
    options.mRelativeToInsertionPoint = true;
    caretOffset -= mSelectionForTSF.StartOffset();
  }
  queryCaretRect.InitForQueryCaretRect(caretOffset, options);

  DispatchEvent(queryCaretRect);
  if (NS_WARN_IF(!queryCaretRect.mSucceeded)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::CreateNativeCaret() FAILED due to "
       "eQueryCaretRect failure (offset=%d)", this, caretOffset));
    return;
  }

  LayoutDeviceIntRect& caretRect = queryCaretRect.mReply.mRect;
  mNativeCaretIsCreated = ::CreateCaret(mWidget->GetWindowHandle(), nullptr,
                                        caretRect.width, caretRect.height);
  if (!mNativeCaretIsCreated) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::CreateNativeCaret() FAILED due to "
       "CreateCaret() failure", this));
    return;
  }

  nsWindow* window = static_cast<nsWindow*>(mWidget.get());
  nsWindow* toplevelWindow = window->GetTopLevelWindow(false);
  if (!toplevelWindow) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::CreateNativeCaret() FAILED due to "
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
TSFTextStore::MaybeDestroyNativeCaret()
{
  if (!mNativeCaretIsCreated) {
    return;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::MaybeDestroyNativeCaret(), "
     "destroying native caret", this));

  ::DestroyCaret();
  mNativeCaretIsCreated = false;
}

void
TSFTextStore::CommitCompositionInternal(bool aDiscard)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::CommitCompositionInternal(aDiscard=%s), "
     "mSink=0x%p, mContext=0x%p, mComposition.mView=0x%p, "
     "mComposition.mString=\"%s\"",
     this, GetBoolName(aDiscard), mSink.get(), mContext.get(),
     mComposition.mView.get(),
     GetEscapedUTF8String(mComposition.mString).get()));

  // If the document is locked, TSF will fail to commit composition since
  // TSF needs another document lock.  So, let's put off the request.
  // Note that TextComposition will commit composition in the focused editor
  // with the latest composition string for web apps and waits asynchronous
  // committing messages.  Therefore, we can and need to perform this
  // asynchronously.
  if (IsReadLocked()) {
    if (mDeferCommittingComposition || mDeferCancellingComposition) {
      MOZ_LOG(sTextStoreLog, LogLevel::Debug,
        ("0x%p   TSFTextStore::CommitCompositionInternal(), "
         "does nothing because already called and waiting unlock...", this));
      return;
    }
    if (aDiscard) {
      mDeferCancellingComposition = true;
    } else {
      mDeferCommittingComposition = true;
    }
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::CommitCompositionInternal(), "
       "putting off to request to %s composition after unlocking the document",
       this, aDiscard ? "cancel" : "commit"));
    return;
  }

  if (mComposition.IsComposing() && aDiscard) {
    LONG endOffset = mComposition.EndOffset();
    mComposition.mString.Truncate(0);
    // Note that don't notify TSF of text change after this is destroyed.
    if (mSink && !mDestroyed) {
      TS_TEXTCHANGE textChange;
      textChange.acpStart = mComposition.mStart;
      textChange.acpOldEnd = endOffset;
      textChange.acpNewEnd = mComposition.mStart;
      MOZ_LOG(sTextStoreLog, LogLevel::Info,
        ("0x%p   TSFTextStore::CommitCompositionInternal(), calling"
         "mSink->OnTextChange(0, { acpStart=%ld, acpOldEnd=%ld, "
         "acpNewEnd=%ld })...", this, textChange.acpStart,
         textChange.acpOldEnd, textChange.acpNewEnd));
      mSink->OnTextChange(0, &textChange);
    }
  }
  // Terminate two contexts, the base context (mContext) and the top
  // if the top context is not the same as the base context
  RefPtr<ITfContext> context = mContext;
  do {
    if (context) {
      RefPtr<ITfContextOwnerCompositionServices> services;
      context->QueryInterface(IID_ITfContextOwnerCompositionServices,
                              getter_AddRefs(services));
      if (services) {
        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::CommitCompositionInternal(), "
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

  RefPtr<ITfCompartmentMgr> compMgr;
  pUnk->QueryInterface(IID_ITfCompartmentMgr, getter_AddRefs(compMgr));
  if (!compMgr) return false;

  return SUCCEEDED(compMgr->GetCompartment(aID, aCompartment)) &&
         (*aCompartment) != nullptr;
}

// static
void
TSFTextStore::SetIMEOpenState(bool aState)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("TSFTextStore::SetIMEOpenState(aState=%s)",
     GetBoolName(aState)));

  RefPtr<ITfCompartment> comp;
  if (!GetCompartment(sThreadMgr,
                      GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                      getter_AddRefs(comp))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
      ("  TSFTextStore::SetIMEOpenState() FAILED due to"
       "no compartment available"));
    return;
  }

  VARIANT variant;
  variant.vt = VT_I4;
  variant.lVal = aState;
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("  TSFTextStore::SetIMEOpenState(), setting "
     "0x%04X to GUID_COMPARTMENT_KEYBOARD_OPENCLOSE...",
     variant.lVal));
  comp->SetValue(sClientId, &variant);
}

// static
bool
TSFTextStore::GetIMEOpenState()
{
  RefPtr<ITfCompartment> comp;
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
TSFTextStore::SetInputContext(nsWindowBase* aWidget,
                             const InputContext& aContext,
                             const InputContextAction& aAction)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("TSFTextStore::SetInputContext(aWidget=%p, "
     "aContext=%s, aAction.mFocusChange=%s), "
     "sEnabledTextStore=0x%p, ThinksHavingFocus()=%s",
     aWidget, GetInputContextString(aContext).get(),
     GetFocusChangeName(aAction.mFocusChange), sEnabledTextStore.get(),
     GetBoolName(ThinksHavingFocus())));

  NS_ENSURE_TRUE_VOID(IsInTSFMode());

  if (aAction.mFocusChange != InputContextAction::FOCUS_NOT_CHANGED) {
    if (sEnabledTextStore) {
      RefPtr<TSFTextStore> textStore(sEnabledTextStore);
      textStore->SetInputScope(aContext.mHTMLInputType,
                               aContext.mHTMLInputInputmode);
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
TSFTextStore::MarkContextAsKeyboardDisabled(ITfContext* aContext)
{
  VARIANT variant_int4_value1;
  variant_int4_value1.vt = VT_I4;
  variant_int4_value1.lVal = 1;

  RefPtr<ITfCompartment> comp;
  if (!GetCompartment(aContext,
                      GUID_COMPARTMENT_KEYBOARD_DISABLED,
                      getter_AddRefs(comp))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("TSFTextStore::MarkContextAsKeyboardDisabled() failed"
       "aContext=0x%p...", aContext));
    return;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("TSFTextStore::MarkContextAsKeyboardDisabled(), setting "
     "to disable context 0x%p...",
     aContext));
  comp->SetValue(sClientId, &variant_int4_value1);
}

// static
void
TSFTextStore::MarkContextAsEmpty(ITfContext* aContext)
{
  VARIANT variant_int4_value1;
  variant_int4_value1.vt = VT_I4;
  variant_int4_value1.lVal = 1;

  RefPtr<ITfCompartment> comp;
  if (!GetCompartment(aContext,
                      GUID_COMPARTMENT_EMPTYCONTEXT,
                      getter_AddRefs(comp))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("TSFTextStore::MarkContextAsEmpty() failed"
       "aContext=0x%p...", aContext));
    return;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("TSFTextStore::MarkContextAsEmpty(), setting "
     "to mark empty context 0x%p...", aContext));
  comp->SetValue(sClientId, &variant_int4_value1);
}

// static
void
TSFTextStore::Initialize()
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("TSFTextStore::Initialize() is called..."));

  if (sThreadMgr) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::Initialize() FAILED due to already initialized"));
    return;
  }

  bool enableTsf = Preferences::GetBool(kPrefNameEnableTSF, false);
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("  TSFTextStore::Initialize(), TSF is %s",
     enableTsf ? "enabled" : "disabled"));
  if (!enableTsf) {
    return;
  }

  // XXX MSDN documents that ITfInputProcessorProfiles is available only on
  //     desktop apps.  However, there is no known way to obtain
  //     ITfInputProcessorProfileMgr instance without ITfInputProcessorProfiles
  //     instance.
  RefPtr<ITfInputProcessorProfiles> inputProcessorProfiles;
  HRESULT hr =
    ::CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                       CLSCTX_INPROC_SERVER,
                       IID_ITfInputProcessorProfiles,
                       getter_AddRefs(inputProcessorProfiles));
  if (FAILED(hr) || !inputProcessorProfiles) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::Initialize() FAILED to create input processor "
       "profiles, hr=0x%08X", hr));
    return;
  }

  RefPtr<ITfThreadMgr> threadMgr;
  hr = ::CoCreateInstance(CLSID_TF_ThreadMgr, nullptr,
                          CLSCTX_INPROC_SERVER, IID_ITfThreadMgr,
                          getter_AddRefs(threadMgr));
  if (FAILED(hr) || !threadMgr) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::Initialize() FAILED to "
       "create the thread manager, hr=0x%08X", hr));
    return;
  }

  RefPtr<ITfMessagePump> messagePump;
  hr = threadMgr->QueryInterface(IID_ITfMessagePump,
                                 getter_AddRefs(messagePump));
  if (FAILED(hr) || !messagePump) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::Initialize() FAILED to "
       "QI message pump from the thread manager, hr=0x%08X", hr));
    return;
  }

  RefPtr<ITfKeystrokeMgr> keystrokeMgr;
  hr = threadMgr->QueryInterface(IID_ITfKeystrokeMgr,
                                 getter_AddRefs(keystrokeMgr));
  if (FAILED(hr) || !keystrokeMgr) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::Initialize() FAILED to "
       "QI keystroke manager from the thread manager, hr=0x%08X", hr));
    return;
  }

  hr = threadMgr->Activate(&sClientId);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::Initialize() FAILED to activate, hr=0x%08X", hr));
    return;
  }

  RefPtr<ITfDisplayAttributeMgr> displayAttributeMgr;
  hr = ::CoCreateInstance(CLSID_TF_DisplayAttributeMgr, nullptr,
                          CLSCTX_INPROC_SERVER, IID_ITfDisplayAttributeMgr,
                          getter_AddRefs(displayAttributeMgr));
  if (FAILED(hr) || !displayAttributeMgr) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::Initialize() FAILED to create "
       "a display attribute manager instance, hr=0x%08X", hr));
    return;
  }

  RefPtr<ITfCategoryMgr> categoryMgr;
  hr = ::CoCreateInstance(CLSID_TF_CategoryMgr, nullptr,
                          CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                          getter_AddRefs(categoryMgr));
  if (FAILED(hr) || !categoryMgr) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::Initialize() FAILED to create "
       "a category manager instance, hr=0x%08X", hr));
    return;
  }

  RefPtr<ITfDocumentMgr> disabledDocumentMgr;
  hr = threadMgr->CreateDocumentMgr(getter_AddRefs(disabledDocumentMgr));
  if (FAILED(hr) || !disabledDocumentMgr) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::Initialize() FAILED to create "
       "a document manager for disabled mode, hr=0x%08X", hr));
    return;
  }

  RefPtr<ITfContext> disabledContext;
  DWORD editCookie = 0;
  hr = disabledDocumentMgr->CreateContext(sClientId, 0, nullptr,
                                          getter_AddRefs(disabledContext),
                                          &editCookie);
  if (FAILED(hr) || !disabledContext) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::Initialize() FAILED to create "
       "a context for disabled mode, hr=0x%08X", hr));
    return;
  }

  MarkContextAsKeyboardDisabled(disabledContext);
  MarkContextAsEmpty(disabledContext);

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("  TSFTextStore::Initialize() is creating "
     "a TSFStaticSink instance..."));
  TSFStaticSink* staticSink = TSFStaticSink::GetInstance();
  if (!staticSink->Init(threadMgr, inputProcessorProfiles)) {
    TSFStaticSink::Shutdown();
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::Initialize() FAILED to initialize TSFStaticSink "
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

  sCreateNativeCaretForLegacyATOK =
    Preferences::GetBool("intl.tsf.hack.atok.create_native_caret", true);
  sDoNotReturnNoLayoutErrorToATOKOfCompositionString =
    Preferences::GetBool(
      "intl.tsf.hack.atok.do_not_return_no_layout_error_of_composition_string",
      true);
  sDoNotReturnNoLayoutErrorToMSSimplifiedTIP =
    Preferences::GetBool(
      "intl.tsf.hack.ms_simplified_chinese.do_not_return_no_layout_error",
      true);
  sDoNotReturnNoLayoutErrorToMSTraditionalTIP =
    Preferences::GetBool(
      "intl.tsf.hack.ms_traditional_chinese.do_not_return_no_layout_error",
      true);
  sDoNotReturnNoLayoutErrorToFreeChangJie =
    Preferences::GetBool(
      "intl.tsf.hack.free_chang_jie.do_not_return_no_layout_error", true);
  sDoNotReturnNoLayoutErrorToEasyChangjei =
    Preferences::GetBool(
      "intl.tsf.hack.easy_changjei.do_not_return_no_layout_error", true);
  sDoNotReturnNoLayoutErrorToMSJapaneseIMEAtFirstChar =
    Preferences::GetBool(
      "intl.tsf.hack.ms_japanese_ime."
      "do_not_return_no_layout_error_at_first_char", true);
  sDoNotReturnNoLayoutErrorToMSJapaneseIMEAtCaret =
    Preferences::GetBool(
      "intl.tsf.hack.ms_japanese_ime.do_not_return_no_layout_error_at_caret",
      true);
  sHackQueryInsertForMSSimplifiedTIP =
    Preferences::GetBool(
      "intl.tsf.hack.ms_simplified_chinese.query_insert_result", true);
  sHackQueryInsertForMSTraditionalTIP =
    Preferences::GetBool(
      "intl.tsf.hack.ms_traditional_chinese.query_insert_result", true);

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
    ("  TSFTextStore::Initialize(), sThreadMgr=0x%p, "
     "sClientId=0x%08X, sDisplayAttrMgr=0x%p, "
     "sCategoryMgr=0x%p, sDisabledDocumentMgr=0x%p, sDisabledContext=%p, "
     "sCreateNativeCaretForLegacyATOK=%s, "
     "sDoNotReturnNoLayoutErrorToATOKOfCompositionString=%s, "
     "sDoNotReturnNoLayoutErrorToFreeChangJie=%s, "
     "sDoNotReturnNoLayoutErrorToEasyChangjei=%s, "
     "sDoNotReturnNoLayoutErrorToMSJapaneseIMEAtFirstChar=%s, "
     "sDoNotReturnNoLayoutErrorToMSJapaneseIMEAtCaret=%s",
     sThreadMgr.get(), sClientId, sDisplayAttrMgr.get(),
     sCategoryMgr.get(), sDisabledDocumentMgr.get(), sDisabledContext.get(),
     GetBoolName(sCreateNativeCaretForLegacyATOK),
     GetBoolName(sDoNotReturnNoLayoutErrorToATOKOfCompositionString),
     GetBoolName(sDoNotReturnNoLayoutErrorToFreeChangJie),
     GetBoolName(sDoNotReturnNoLayoutErrorToEasyChangjei),
     GetBoolName(sDoNotReturnNoLayoutErrorToMSJapaneseIMEAtFirstChar),
     GetBoolName(sDoNotReturnNoLayoutErrorToMSJapaneseIMEAtCaret)));
}

// static
void
TSFTextStore::Terminate()
{
  MOZ_LOG(sTextStoreLog, LogLevel::Info, ("TSFTextStore::Terminate()"));

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
TSFTextStore::ProcessRawKeyMessage(const MSG& aMsg)
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
    RefPtr<TSFTextStore> textStore(sEnabledTextStore);
    if (textStore) {
      textStore->OnStartToHandleKeyMessage();
    }
    hr = sKeystrokeMgr->KeyDown(aMsg.wParam, aMsg.lParam, &eaten);
    if (textStore) {
      textStore->OnEndHandlingKeyMessage();
    }
    return SUCCEEDED(hr) && eaten;
  }
  if (aMsg.message == WM_KEYUP) {
    BOOL eaten;
    HRESULT hr = sKeystrokeMgr->TestKeyUp(aMsg.wParam, aMsg.lParam, &eaten);
    if (FAILED(hr) || !eaten) {
      return false;
    }
    RefPtr<TSFTextStore> textStore(sEnabledTextStore);
    if (textStore) {
      textStore->OnStartToHandleKeyMessage();
    }
    hr = sKeystrokeMgr->KeyUp(aMsg.wParam, aMsg.lParam, &eaten);
    if (textStore) {
      textStore->OnEndHandlingKeyMessage();
    }
    return SUCCEEDED(hr) && eaten;
  }
  return false;
}

// static
void
TSFTextStore::ProcessMessage(nsWindowBase* aWindow,
                             UINT aMessage,
                             WPARAM& aWParam,
                             LPARAM& aLParam,
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
    case MOZ_WM_NOTIY_TSF_OF_LAYOUT_CHANGE: {
      TSFTextStore* maybeTextStore = reinterpret_cast<TSFTextStore*>(aWParam);
      if (maybeTextStore == sEnabledTextStore) {
        RefPtr<TSFTextStore> textStore(maybeTextStore);
        textStore->NotifyTSFOfLayoutChangeAgain();
      }
      break;
    }
  }
}

// static
bool
TSFTextStore::IsIMM_IME()
{
  return TSFStaticSink::IsIMM_IME();
}

/******************************************************************/
/* TSFTextStore::Composition                                       */
/******************************************************************/

void
TSFTextStore::Composition::Start(ITfCompositionView* aCompositionView,
                                 LONG aCompositionStartOffset,
                                 const nsAString& aCompositionString)
{
  mView = aCompositionView;
  mString = aCompositionString;
  mStart = aCompositionStartOffset;
}

void
TSFTextStore::Composition::End()
{
  mView = nullptr;
  mString.Truncate();
}

/******************************************************************************
 *  TSFTextStore::Content
 *****************************************************************************/

const nsDependentSubstring
TSFTextStore::Content::GetSelectedText() const
{
  MOZ_ASSERT(mInitialized);
  return GetSubstring(static_cast<uint32_t>(mSelection.StartOffset()),
                      static_cast<uint32_t>(mSelection.Length()));
}

const nsDependentSubstring
TSFTextStore::Content::GetSubstring(uint32_t aStart, uint32_t aLength) const
{
  MOZ_ASSERT(mInitialized);
  return nsDependentSubstring(mText, aStart, aLength);
}

void
TSFTextStore::Content::ReplaceSelectedTextWith(const nsAString& aString)
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
TSFTextStore::Content::ReplaceTextWith(LONG aStart,
                                       LONG aLength,
                                       const nsAString& aReplaceString)
{
  MOZ_ASSERT(mInitialized);
  const nsDependentSubstring replacedString =
    GetSubstring(static_cast<uint32_t>(aStart),
                 static_cast<uint32_t>(aLength));
  if (aReplaceString != replacedString) {
    uint32_t firstDifferentOffset = mMinTextModifiedOffset;
    if (mComposition.IsComposing()) {
      // Emulate text insertion during compositions, because during a
      // composition, editor expects the whole composition string to
      // be sent in eCompositionChange, not just the inserted part.
      // The actual eCompositionChange will be sent in SetSelection
      // or OnUpdateComposition.
      MOZ_ASSERT(aStart >= mComposition.mStart);
      MOZ_ASSERT(aStart + aLength <= mComposition.EndOffset());
      mComposition.mString.Replace(
        static_cast<uint32_t>(aStart - mComposition.mStart),
        static_cast<uint32_t>(aLength), aReplaceString);
      // TIP may set composition string twice or more times during a document
      // lock.  Therefore, we should compute the first difference offset with
      // mLastCompositionString.
      if (mComposition.mString != mLastCompositionString) {
        firstDifferentOffset =
          mComposition.mStart +
            FirstDifferentCharOffset(mComposition.mString,
                                     mLastCompositionString);
        // The previous change to the composition string is canceled.
        if (mMinTextModifiedOffset >=
              static_cast<uint32_t>(mComposition.mStart) &&
            mMinTextModifiedOffset < firstDifferentOffset) {
          mMinTextModifiedOffset = firstDifferentOffset;
        }
      } else if (mMinTextModifiedOffset >=
                   static_cast<uint32_t>(mComposition.mStart) &&
                 mMinTextModifiedOffset <
                   static_cast<uint32_t>(mComposition.EndOffset())) {
        // The previous change to the composition string is canceled.
        mMinTextModifiedOffset = firstDifferentOffset =
          mComposition.EndOffset();
      }
      MOZ_LOG(sTextStoreLog, LogLevel::Debug,
        ("0x%p   TSFTextStore::Content::ReplaceTextWith(aStart=%d, "
         "aLength=%d, aReplaceString=\"%s\"), mComposition={ mStart=%d, "
         "mString=\"%s\" }, mLastCompositionString=\"%s\", "
         "mMinTextModifiedOffset=%u, firstDifferentOffset=%u",
         this, aStart, aLength, GetEscapedUTF8String(aReplaceString).get(),
         mComposition.mStart, GetEscapedUTF8String(mComposition.mString).get(),
         GetEscapedUTF8String(mLastCompositionString).get(),
         mMinTextModifiedOffset, firstDifferentOffset));
    } else {
      firstDifferentOffset =
        static_cast<uint32_t>(aStart) +
          FirstDifferentCharOffset(aReplaceString, replacedString);
    }
    mMinTextModifiedOffset =
      std::min(mMinTextModifiedOffset, firstDifferentOffset);
    mText.Replace(static_cast<uint32_t>(aStart),
                  static_cast<uint32_t>(aLength), aReplaceString);
  }
  // Selection should be collapsed at the end of the inserted string.
  mSelection.CollapseAt(
    static_cast<uint32_t>(aStart) + aReplaceString.Length());
}

void
TSFTextStore::Content::StartComposition(ITfCompositionView* aCompositionView,
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
    // XXX Do we need to set a new writing-mode here when setting a new
    // selection? Currently, we just preserve the existing value.
    mSelection.SetSelection(mComposition.mStart, mComposition.mString.Length(),
                            false, mSelection.GetWritingMode());
  }
}

void
TSFTextStore::Content::RestoreCommittedComposition(
                         ITfCompositionView* aCompositionView,
                         const PendingAction& aPendingCompositionStart,
                         const PendingAction& aCanceledCompositionEnd)
{
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(aCompositionView);
  MOZ_ASSERT(!mComposition.mView);
  MOZ_ASSERT(aPendingCompositionStart.mType ==
               PendingAction::COMPOSITION_START);
  MOZ_ASSERT(aCanceledCompositionEnd.mType ==
               PendingAction::COMPOSITION_END);
  MOZ_ASSERT(GetSubstring(
               static_cast<uint32_t>(aPendingCompositionStart.mSelectionStart),
               static_cast<uint32_t>(aCanceledCompositionEnd.mData.Length())) ==
               aCanceledCompositionEnd.mData);

  // Restore the committed string as composing string.
  mComposition.Start(aCompositionView,
                     aPendingCompositionStart.mSelectionStart,
                     aCanceledCompositionEnd.mData);
}

void
TSFTextStore::Content::EndComposition(const PendingAction& aCompEnd)
{
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(mComposition.mView);
  MOZ_ASSERT(aCompEnd.mType == PendingAction::COMPOSITION_END);

  mSelection.CollapseAt(mComposition.mStart + aCompEnd.mData.Length());
  mComposition.End();
}

/******************************************************************************
 *  TSFTextStore::MouseTracker
 *****************************************************************************/

TSFTextStore::MouseTracker::MouseTracker()
  : mStart(-1)
  , mLength(-1)
  , mCookie(kInvalidCookie)
{
}

HRESULT
TSFTextStore::MouseTracker::Init(TSFTextStore* aTextStore)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::MouseTracker::Init(aTextStore=0x%p), "
     "aTextStore->mMouseTrackers.Length()=%d",
     this, aTextStore->mMouseTrackers.Length()));

  if (&aTextStore->mMouseTrackers.LastElement() != this) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::MouseTracker::Init() FAILED due to "
       "this is not the last element of mMouseTrackers", this));
    return E_FAIL;
  }
  if (aTextStore->mMouseTrackers.Length() > kInvalidCookie) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::MouseTracker::Init() FAILED due to "
       "no new cookie available", this));
    return E_FAIL;
  }
  MOZ_ASSERT(!aTextStore->mMouseTrackers.IsEmpty(),
             "This instance must be in TSFTextStore::mMouseTrackers");
  mCookie = static_cast<DWORD>(aTextStore->mMouseTrackers.Length() - 1);
  return S_OK;
}

HRESULT
TSFTextStore::MouseTracker::AdviseSink(TSFTextStore* aTextStore,
                                       ITfRangeACP* aTextRange,
                                       ITfMouseSink* aMouseSink)
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::MouseTracker::AdviseSink(aTextStore=0x%p, "
     "aTextRange=0x%p, aMouseSink=0x%p), mCookie=%d, mSink=0x%p",
     this, aTextStore, aTextRange, aMouseSink, mCookie, mSink.get()));
  MOZ_ASSERT(mCookie != kInvalidCookie, "This hasn't been initalized?");

  if (mSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::MouseTracker::AdviseMouseSink() FAILED "
       "due to already being used", this));
    return E_FAIL;
  }

  HRESULT hr = aTextRange->GetExtent(&mStart, &mLength);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::MouseTracker::AdviseMouseSink() FAILED "
       "due to failure of ITfRangeACP::GetExtent()", this));
    return hr;
  }

  if (mStart < 0 || mLength <= 0) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::MouseTracker::AdviseMouseSink() FAILED "
       "due to odd result of ITfRangeACP::GetExtent(), "
       "mStart=%d, mLength=%d", this, mStart, mLength));
    return E_INVALIDARG;
  }

  nsAutoString textContent;
  if (NS_WARN_IF(!aTextStore->GetCurrentText(textContent))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::MouseTracker::AdviseMouseSink() FAILED "
       "due to failure of TSFTextStore::GetCurrentText()", this));
    return E_FAIL;
  }

  if (textContent.Length() <= static_cast<uint32_t>(mStart) ||
      textContent.Length() < static_cast<uint32_t>(mStart + mLength)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("0x%p   TSFTextStore::MouseTracker::AdviseMouseSink() FAILED "
       "due to out of range, mStart=%d, mLength=%d, "
       "textContent.Length()=%d",
       this, mStart, mLength, textContent.Length()));
    return E_INVALIDARG;
  }

  mSink = aMouseSink;

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::MouseTracker::AdviseMouseSink(), "
     "succeeded, mStart=%d, mLength=%d, textContent.Length()=%d",
     this, mStart, mLength, textContent.Length()));
  return S_OK;
}

void
TSFTextStore::MouseTracker::UnadviseSink()
{
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::MouseTracker::UnadviseSink(), "
     "mCookie=%d, mSink=0x%p, mStart=%d, mLength=%d",
     this, mCookie, mSink.get(), mStart, mLength));
  mSink = nullptr;
  mStart = mLength = -1;
}

bool
TSFTextStore::MouseTracker::OnMouseButtonEvent(ULONG aEdge,
                                               ULONG aQuadrant,
                                               DWORD aButtonStatus)
{
  MOZ_ASSERT(IsUsing(), "The caller must check before calling OnMouseEvent()");

  BOOL eaten = FALSE;
  HRESULT hr = mSink->OnMouseEvent(aEdge, aQuadrant, aButtonStatus, &eaten);

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
    ("0x%p   TSFTextStore::MouseTracker::OnMouseEvent(aEdge=%d, "
     "aQuadrant=%d, aButtonStatus=0x%08X), hr=0x%08X, eaten=%s",
     this, aEdge, aQuadrant, aButtonStatus, hr, GetBoolName(!!eaten)));

  return SUCCEEDED(hr) && eaten;
}

#ifdef DEBUG
// static
bool
TSFTextStore::CurrentKeyboardLayoutHasIME()
{
  if (!sInputProcessorProfiles) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("TSFTextStore::CurrentKeyboardLayoutHasIME() FAILED due to "
       "there is no input processor profiles instance"));
    return false;
  }
  RefPtr<ITfInputProcessorProfileMgr> profileMgr;
  HRESULT hr =
    sInputProcessorProfiles->QueryInterface(IID_ITfInputProcessorProfileMgr,
                                            getter_AddRefs(profileMgr));
  if (FAILED(hr) || !profileMgr) {
    // On Windows Vista or later, ImmIsIME() API always returns true.
    // If we failed to obtain the profile manager, we cannot know if current
    // keyboard layout has IME.
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::CurrentKeyboardLayoutHasIME() FAILED to query "
       "ITfInputProcessorProfileMgr"));
    return false;
  }

  TF_INPUTPROCESSORPROFILE profile;
  hr = profileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &profile);
  if (hr == S_FALSE) {
    return false; // not found or not active
  }
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
      ("  TSFTextStore::CurrentKeyboardLayoutHasIME() FAILED to retreive "
       "active profile"));
    return false;
  }
  return (profile.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR);
}
#endif // #ifdef DEBUG

} // name widget
} // name mozilla
