/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define INPUTSCOPE_INIT_GUID
#define TEXTATTRS_INIT_GUID
#include "TSFTextStore.h"

#include <olectl.h>
#include <algorithm>
#include "nscore.h"

#include "IMMHandler.h"
#include "KeyboardLayout.h"
#include "WinIMEHandler.h"
#include "WinUtils.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_intl.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"
#include "mozilla/ToString.h"
#include "mozilla/WindowsVersion.h"
#include "nsWindow.h"
#include "nsPrintfCString.h"

// Workaround for mingw32
#ifndef TS_SD_INPUTPANEMANUALDISPLAYENABLE
#  define TS_SD_INPUTPANEMANUALDISPLAYENABLE 0x40
#endif

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

enum class TextInputProcessorID {
  // Internal use only.  This won't be returned by TSFStaticSink::ActiveTIP().
  eNotComputed,

  // Not a TIP.  E.g., simple keyboard layout or IMM-IME.
  eNone,

  // Used for other TIPs, i.e., any TIPs which we don't support specifically.
  eUnknown,

  // TIP for Japanese.
  eMicrosoftIMEForJapanese,
  eMicrosoftOfficeIME2010ForJapanese,
  eGoogleJapaneseInput,
  eATOK2011,
  eATOK2012,
  eATOK2013,
  eATOK2014,
  eATOK2015,
  eATOK2016,
  eATOKUnknown,
  eJapanist10,

  // TIP for Traditional Chinese.
  eMicrosoftBopomofo,
  eMicrosoftChangJie,
  eMicrosoftPhonetic,
  eMicrosoftQuick,
  eMicrosoftNewChangJie,
  eMicrosoftNewPhonetic,
  eMicrosoftNewQuick,
  eFreeChangJie,

  // TIP for Simplified Chinese.
  eMicrosoftPinyin,
  eMicrosoftPinyinNewExperienceInputStyle,
  eMicrosoftWubi,

  // TIP for Korean.
  eMicrosoftIMEForKorean,
  eMicrosoftOldHangul,

  // Keyman Desktop, which can install various language keyboards.
  eKeymanDesktop,
};

static const char* GetBoolName(bool aBool) { return aBool ? "true" : "false"; }

static void HandleSeparator(nsCString& aDesc) {
  if (!aDesc.IsEmpty()) {
    aDesc.AppendLiteral(" | ");
  }
}

static const nsCString GetFindFlagName(DWORD aFindFlag) {
  nsCString description;
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

class GetACPFromPointFlagName : public nsAutoCString {
 public:
  explicit GetACPFromPointFlagName(DWORD aFlags) {
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

static const char* GetFocusChangeName(
    InputContextAction::FocusChange aFocusChange) {
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
    case InputContextAction::WIDGET_CREATED:
      return "WIDGET_CREATED";
    default:
      return "Unknown";
  }
}

static nsCString GetCLSIDNameStr(REFCLSID aCLSID) {
  LPOLESTR str = nullptr;
  HRESULT hr = ::StringFromCLSID(aCLSID, &str);
  if (FAILED(hr) || !str || !str[0]) {
    return ""_ns;
  }

  nsCString result;
  result = NS_ConvertUTF16toUTF8(str);
  ::CoTaskMemFree(str);
  return result;
}

static nsCString GetGUIDNameStr(REFGUID aGUID) {
  OLECHAR str[40];
  int len = ::StringFromGUID2(aGUID, str, ArrayLength(str));
  if (!len || !str[0]) {
    return ""_ns;
  }

  return NS_ConvertUTF16toUTF8(str);
}

static nsCString GetGUIDNameStrWithTable(REFGUID aGUID) {
#define RETURN_GUID_NAME(aNamedGUID)      \
  if (IsEqualGUID(aGUID, aNamedGUID)) {   \
    return nsLiteralCString(#aNamedGUID); \
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

static nsCString GetRIIDNameStr(REFIID aRIID) {
  LPOLESTR str = nullptr;
  HRESULT hr = ::StringFromIID(aRIID, &str);
  if (FAILED(hr) || !str || !str[0]) {
    return ""_ns;
  }

  nsAutoString key(L"Interface\\");
  key += str;

  nsCString result;
  wchar_t buf[256];
  if (WinUtils::GetRegistryKey(HKEY_CLASSES_ROOT, key.get(), nullptr, buf,
                               sizeof(buf))) {
    result = NS_ConvertUTF16toUTF8(buf);
  } else {
    result = NS_ConvertUTF16toUTF8(str);
  }

  ::CoTaskMemFree(str);
  return result;
}

static const char* GetCommonReturnValueName(HRESULT aResult) {
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

static const char* GetTextStoreReturnValueName(HRESULT aResult) {
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

static const nsCString GetSinkMaskNameStr(DWORD aSinkMask) {
  nsCString description;
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

static const nsCString GetLockFlagNameStr(DWORD aLockFlags) {
  nsCString description;
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

static const char* GetTextRunTypeName(TsRunType aRunType) {
  switch (aRunType) {
    case TS_RT_PLAIN:
      return "TS_RT_PLAIN";
    case TS_RT_HIDDEN:
      return "TS_RT_HIDDEN";
    case TS_RT_OPAQUE:
      return "TS_RT_OPAQUE";
    default:
      return "Unknown";
  }
}

static nsCString GetColorName(const TF_DA_COLOR& aColor) {
  switch (aColor.type) {
    case TF_CT_NONE:
      return "TF_CT_NONE"_ns;
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

static nsCString GetLineStyleName(TF_DA_LINESTYLE aLineStyle) {
  switch (aLineStyle) {
    case TF_LS_NONE:
      return "TF_LS_NONE"_ns;
    case TF_LS_SOLID:
      return "TF_LS_SOLID"_ns;
    case TF_LS_DOT:
      return "TF_LS_DOT"_ns;
    case TF_LS_DASH:
      return "TF_LS_DASH"_ns;
    case TF_LS_SQUIGGLE:
      return "TF_LS_SQUIGGLE"_ns;
    default: {
      return nsPrintfCString("Unknown(%08X)", static_cast<int32_t>(aLineStyle));
    }
  }
}

static nsCString GetClauseAttrName(TF_DA_ATTR_INFO aAttr) {
  switch (aAttr) {
    case TF_ATTR_INPUT:
      return "TF_ATTR_INPUT"_ns;
    case TF_ATTR_TARGET_CONVERTED:
      return "TF_ATTR_TARGET_CONVERTED"_ns;
    case TF_ATTR_CONVERTED:
      return "TF_ATTR_CONVERTED"_ns;
    case TF_ATTR_TARGET_NOTCONVERTED:
      return "TF_ATTR_TARGET_NOTCONVERTED"_ns;
    case TF_ATTR_INPUT_ERROR:
      return "TF_ATTR_INPUT_ERROR"_ns;
    case TF_ATTR_FIXEDCONVERTED:
      return "TF_ATTR_FIXEDCONVERTED"_ns;
    case TF_ATTR_OTHER:
      return "TF_ATTR_OTHER"_ns;
    default: {
      return nsPrintfCString("Unknown(%08X)", static_cast<int32_t>(aAttr));
    }
  }
}

static nsCString GetDisplayAttrStr(const TF_DISPLAYATTRIBUTE& aDispAttr) {
  nsCString str;
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

static const char* GetMouseButtonName(int16_t aButton) {
  switch (aButton) {
    case MouseButton::ePrimary:
      return "LeftButton";
    case MouseButton::eMiddle:
      return "MiddleButton";
    case MouseButton::eSecondary:
      return "RightButton";
    default:
      return "UnknownButton";
  }
}

#define ADD_SEPARATOR_IF_NECESSARY(aStr) \
  if (!aStr.IsEmpty()) {                 \
    aStr.AppendLiteral(", ");            \
  }

static nsCString GetMouseButtonsName(int16_t aButtons) {
  if (!aButtons) {
    return "no buttons"_ns;
  }
  nsCString names;
  if (aButtons & MouseButtonsFlag::ePrimaryFlag) {
    names = "LeftButton";
  }
  if (aButtons & MouseButtonsFlag::eSecondaryFlag) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += "RightButton";
  }
  if (aButtons & MouseButtonsFlag::eMiddleFlag) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += "MiddleButton";
  }
  if (aButtons & MouseButtonsFlag::e4thFlag) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += "4thButton";
  }
  if (aButtons & MouseButtonsFlag::e5thFlag) {
    ADD_SEPARATOR_IF_NECESSARY(names);
    names += "5thButton";
  }
  return names;
}

static nsCString GetModifiersName(Modifiers aModifiers) {
  if (aModifiers == MODIFIER_NONE) {
    return "no modifiers"_ns;
  }
  nsCString names;
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

class GetWritingModeName : public nsAutoCString {
 public:
  explicit GetWritingModeName(const WritingMode& aWritingMode) {
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

class GetEscapedUTF8String final : public NS_ConvertUTF16toUTF8 {
 public:
  explicit GetEscapedUTF8String(const nsAString& aString)
      : NS_ConvertUTF16toUTF8(aString) {
    Escape();
  }
  explicit GetEscapedUTF8String(const char16ptr_t aString)
      : NS_ConvertUTF16toUTF8(aString) {
    Escape();
  }
  GetEscapedUTF8String(const char16ptr_t aString, uint32_t aLength)
      : NS_ConvertUTF16toUTF8(aString, aLength) {
    Escape();
  }

 private:
  void Escape() {
    ReplaceSubstring("\r", "\\r");
    ReplaceSubstring("\n", "\\n");
    ReplaceSubstring("\t", "\\t");
  }
};

class GetInputScopeString : public nsAutoCString {
 public:
  explicit GetInputScopeString(const nsTArray<InputScope>& aList) {
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
        case IS_PRIVATE:
          AppendLiteral("IS_PRIVATE");
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

class InputScopeImpl final : public ITfInputScope {
  ~InputScopeImpl() {}

 public:
  explicit InputScopeImpl(const nsTArray<InputScope>& aList)
      : mInputScopes(aList.Clone()) {
    MOZ_LOG(
        sTextStoreLog, LogLevel::Info,
        ("0x%p InputScopeImpl(%s)", this, GetInputScopeString(aList).get()));
  }

  NS_INLINE_DECL_IUNKNOWN_REFCOUNTING(InputScopeImpl)

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
    *ppv = nullptr;
    if ((IID_IUnknown == riid) || (IID_ITfInputScope == riid)) {
      *ppv = static_cast<ITfInputScope*>(this);
    }
    if (*ppv) {
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHODIMP GetInputScopes(InputScope** pprgInputScopes, UINT* pcCount) {
    uint32_t count = (mInputScopes.IsEmpty() ? 1 : mInputScopes.Length());

    InputScope* pScope =
        (InputScope*)CoTaskMemAlloc(sizeof(InputScope) * count);
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

  STDMETHODIMP GetPhrase(BSTR** ppbstrPhrases, UINT* pcCount) {
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

class TSFStaticSink final : public ITfInputProcessorProfileActivationSink {
 public:
  static TSFStaticSink* GetInstance() {
    if (!sInstance) {
      RefPtr<ITfThreadMgr> threadMgr = TSFTextStore::GetThreadMgr();
      if (NS_WARN_IF(!threadMgr)) {
        MOZ_LOG(
            sTextStoreLog, LogLevel::Error,
            ("TSFStaticSink::GetInstance() FAILED to initialize TSFStaticSink "
             "instance due to no ThreadMgr instance"));
        return nullptr;
      }
      RefPtr<ITfInputProcessorProfiles> inputProcessorProfiles =
          TSFTextStore::GetInputProcessorProfiles();
      if (NS_WARN_IF(!inputProcessorProfiles)) {
        MOZ_LOG(
            sTextStoreLog, LogLevel::Error,
            ("TSFStaticSink::GetInstance() FAILED to initialize TSFStaticSink "
             "instance due to no InputProcessorProfiles instance"));
        return nullptr;
      }
      RefPtr<TSFStaticSink> staticSink = new TSFStaticSink();
      if (NS_WARN_IF(!staticSink->Init(threadMgr, inputProcessorProfiles))) {
        staticSink->Destroy();
        MOZ_LOG(
            sTextStoreLog, LogLevel::Error,
            ("TSFStaticSink::GetInstance() FAILED to initialize TSFStaticSink "
             "instance"));
        return nullptr;
      }
      sInstance = staticSink.forget();
    }
    return sInstance;
  }

  static void Shutdown() {
    if (sInstance) {
      sInstance->Destroy();
      sInstance = nullptr;
    }
  }

  bool Init(ITfThreadMgr* aThreadMgr,
            ITfInputProcessorProfiles* aInputProcessorProfiles);
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
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

  const nsString& GetActiveTIPKeyboardDescription() const {
    return mActiveTIPKeyboardDescription;
  }

  static bool IsIMM_IMEActive() {
    // Use IMM API until TSFStaticSink starts to work.
    if (!sInstance || !sInstance->EnsureInitActiveTIPKeyboard()) {
      return IsIMM_IME(::GetKeyboardLayout(0));
    }
    return sInstance->mIsIMM_IME;
  }

  static bool IsIMM_IME(HKL aHKL) {
    return (::ImmGetIMEFileNameW(aHKL, nullptr, 0) > 0);
  }

  static bool IsTraditionalChinese() {
    EnsureInstance();
    return sInstance && sInstance->IsTraditionalChineseInternal();
  }
  static bool IsSimplifiedChinese() {
    EnsureInstance();
    return sInstance && sInstance->IsSimplifiedChineseInternal();
  }
  static bool IsJapanese() {
    EnsureInstance();
    return sInstance && sInstance->IsJapaneseInternal();
  }
  static bool IsKorean() {
    EnsureInstance();
    return sInstance && sInstance->IsKoreanInternal();
  }

  /**
   * ActiveTIP() returns an ID for currently active TIP.
   * Please note that this method is expensive due to needs a lot of GUID
   * comparations if active language ID is one of CJKT.  If you need to
   * check TIPs for a specific language, you should check current language
   * first.
   */
  static TextInputProcessorID ActiveTIP() {
    EnsureInstance();
    if (!sInstance || !sInstance->EnsureInitActiveTIPKeyboard()) {
      return TextInputProcessorID::eUnknown;
    }
    sInstance->ComputeActiveTextInputProcessor();
    if (NS_WARN_IF(sInstance->mActiveTIP ==
                   TextInputProcessorID::eNotComputed)) {
      return TextInputProcessorID::eUnknown;
    }
    return sInstance->mActiveTIP;
  }

  static bool IsMSChangJieOrMSQuickActive() {
    // ActiveTIP() is expensive if it hasn't computed active TIP yet.
    // For avoiding unnecessary computation, we should check if the language
    // for current TIP is Traditional Chinese.
    if (!IsTraditionalChinese()) {
      return false;
    }
    switch (ActiveTIP()) {
      case TextInputProcessorID::eMicrosoftChangJie:
      case TextInputProcessorID::eMicrosoftQuick:
        return true;
      default:
        return false;
    }
  }

  static bool IsMSPinyinOrMSWubiActive() {
    // ActiveTIP() is expensive if it hasn't computed active TIP yet.
    // For avoiding unnecessary computation, we should check if the language
    // for current TIP is Simplified Chinese.
    if (!IsSimplifiedChinese()) {
      return false;
    }
    switch (ActiveTIP()) {
      case TextInputProcessorID::eMicrosoftPinyin:
      case TextInputProcessorID::eMicrosoftWubi:
        return true;
      default:
        return false;
    }
  }

  static bool IsMSJapaneseIMEActive() {
    // ActiveTIP() is expensive if it hasn't computed active TIP yet.
    // For avoiding unnecessary computation, we should check if the language
    // for current TIP is Japanese.
    if (!IsJapanese()) {
      return false;
    }
    return ActiveTIP() == TextInputProcessorID::eMicrosoftIMEForJapanese;
  }

  static bool IsGoogleJapaneseInputActive() {
    // ActiveTIP() is expensive if it hasn't computed active TIP yet.
    // For avoiding unnecessary computation, we should check if the language
    // for current TIP is Japanese.
    if (!IsJapanese()) {
      return false;
    }
    return ActiveTIP() == TextInputProcessorID::eGoogleJapaneseInput;
  }

  static bool IsATOKActive() {
    // ActiveTIP() is expensive if it hasn't computed active TIP yet.
    // For avoiding unnecessary computation, we should check if active TIP is
    // ATOK first since it's cheaper.
    return IsJapanese() && sInstance->IsATOKActiveInternal();
  }

  // Note that ATOK 2011 - 2016 refers native caret position for deciding its
  // popup window position.
  static bool IsATOKReferringNativeCaretActive() {
    // ActiveTIP() is expensive if it hasn't computed active TIP yet.
    // For avoiding unnecessary computation, we should check if active TIP is
    // ATOK first since it's cheaper.
    if (!IsJapanese() || !sInstance->IsATOKActiveInternal()) {
      return false;
    }
    switch (ActiveTIP()) {
      case TextInputProcessorID::eATOK2011:
      case TextInputProcessorID::eATOK2012:
      case TextInputProcessorID::eATOK2013:
      case TextInputProcessorID::eATOK2014:
      case TextInputProcessorID::eATOK2015:
        return true;
      default:
        return false;
    }
  }

 private:
  static void EnsureInstance() {
    if (!sInstance) {
      RefPtr<TSFStaticSink> staticSink = GetInstance();
      Unused << staticSink;
    }
  }

  bool IsTraditionalChineseInternal() const { return mLangID == 0x0404; }
  bool IsSimplifiedChineseInternal() const { return mLangID == 0x0804; }
  bool IsJapaneseInternal() const { return mLangID == 0x0411; }
  bool IsKoreanInternal() const { return mLangID == 0x0412; }

  bool IsATOKActiveInternal() {
    EnsureInitActiveTIPKeyboard();
    // FYI: Name of packaged ATOK includes the release year like "ATOK 2015".
    //      Name of ATOK Passport (subscription) equals "ATOK".
    return StringBeginsWith(mActiveTIPKeyboardDescription, u"ATOK "_ns) ||
           mActiveTIPKeyboardDescription.EqualsLiteral("ATOK");
  }

  void ComputeActiveTextInputProcessor() {
    if (mActiveTIP != TextInputProcessorID::eNotComputed) {
      return;
    }

    if (mActiveTIPGUID == GUID_NULL) {
      mActiveTIP = TextInputProcessorID::eNone;
      return;
    }

    // Comparing GUID is slow. So, we should use language information to
    // reduce the comparing cost for TIP which is not we do not support
    // specifically since they are always compared with all supported TIPs.
    switch (mLangID) {
      case 0x0404:
        mActiveTIP = ComputeActiveTIPAsTraditionalChinese();
        break;
      case 0x0411:
        mActiveTIP = ComputeActiveTIPAsJapanese();
        break;
      case 0x0412:
        mActiveTIP = ComputeActiveTIPAsKorean();
        break;
      case 0x0804:
        mActiveTIP = ComputeActiveTIPAsSimplifiedChinese();
        break;
      default:
        mActiveTIP = TextInputProcessorID::eUnknown;
        break;
    }
    // Special case for Keyman Desktop, it is available for any languages.
    // Therefore, we need to check it only if we don't know the active TIP.
    if (mActiveTIP != TextInputProcessorID::eUnknown) {
      return;
    }

    // Note that keyboard layouts for Keyman assign its GUID on install
    // randomly, but CLSID is constant in any environments.
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1670834#c7
    // https://github.com/keymanapp/keyman/blob/318c73a9e1d571d942837ff9964590626e5bd5aa/windows/src/engine/kmtip/globals.cpp#L37
    // {FE0420F1-38D1-4B4C-96BF-E7E20A74CFB7}
    static constexpr CLSID kKeymanDesktop_CLSID = {
        0xFE0420F1,
        0x38D1,
        0x4B4C,
        {0x96, 0xBF, 0xE7, 0xE2, 0x0A, 0x74, 0xCF, 0xB7}};
    if (mActiveTIPCLSID == kKeymanDesktop_CLSID) {
      mActiveTIP = TextInputProcessorID::eKeymanDesktop;
    }
  }

  TextInputProcessorID ComputeActiveTIPAsJapanese() {
    // {A76C93D9-5523-4E90-AAFA-4DB112F9AC76} (Win7, Win8.1, Win10)
    static constexpr GUID kMicrosoftIMEForJapaneseGUID = {
        0xA76C93D9,
        0x5523,
        0x4E90,
        {0xAA, 0xFA, 0x4D, 0xB1, 0x12, 0xF9, 0xAC, 0x76}};
    if (mActiveTIPGUID == kMicrosoftIMEForJapaneseGUID) {
      return TextInputProcessorID::eMicrosoftIMEForJapanese;
    }
    // {54EDCC94-1524-4BB1-9FB7-7BABE4F4CA64}
    static constexpr GUID kMicrosoftOfficeIME2010ForJapaneseGUID = {
        0x54EDCC94,
        0x1524,
        0x4BB1,
        {0x9F, 0xB7, 0x7B, 0xAB, 0xE4, 0xF4, 0xCA, 0x64}};
    if (mActiveTIPGUID == kMicrosoftOfficeIME2010ForJapaneseGUID) {
      return TextInputProcessorID::eMicrosoftOfficeIME2010ForJapanese;
    }
    // {773EB24E-CA1D-4B1B-B420-FA985BB0B80D}
    static constexpr GUID kGoogleJapaneseInputGUID = {
        0x773EB24E,
        0xCA1D,
        0x4B1B,
        {0xB4, 0x20, 0xFA, 0x98, 0x5B, 0xB0, 0xB8, 0x0D}};
    if (mActiveTIPGUID == kGoogleJapaneseInputGUID) {
      return TextInputProcessorID::eGoogleJapaneseInput;
    }
    // {F9C24A5C-8A53-499D-9572-93B2FF582115}
    static const GUID kATOK2011GUID = {
        0xF9C24A5C,
        0x8A53,
        0x499D,
        {0x95, 0x72, 0x93, 0xB2, 0xFF, 0x58, 0x21, 0x15}};
    if (mActiveTIPGUID == kATOK2011GUID) {
      return TextInputProcessorID::eATOK2011;
    }
    // {1DE01562-F445-401B-B6C3-E5B18DB79461}
    static constexpr GUID kATOK2012GUID = {
        0x1DE01562,
        0xF445,
        0x401B,
        {0xB6, 0xC3, 0xE5, 0xB1, 0x8D, 0xB7, 0x94, 0x61}};
    if (mActiveTIPGUID == kATOK2012GUID) {
      return TextInputProcessorID::eATOK2012;
    }
    // {3C4DB511-189A-4168-B6EA-BFD0B4C85615}
    static constexpr GUID kATOK2013GUID = {
        0x3C4DB511,
        0x189A,
        0x4168,
        {0xB6, 0xEA, 0xBF, 0xD0, 0xB4, 0xC8, 0x56, 0x15}};
    if (mActiveTIPGUID == kATOK2013GUID) {
      return TextInputProcessorID::eATOK2013;
    }
    // {4EF33B79-6AA9-4271-B4BF-9321C279381B}
    static constexpr GUID kATOK2014GUID = {
        0x4EF33B79,
        0x6AA9,
        0x4271,
        {0xB4, 0xBF, 0x93, 0x21, 0xC2, 0x79, 0x38, 0x1B}};
    if (mActiveTIPGUID == kATOK2014GUID) {
      return TextInputProcessorID::eATOK2014;
    }
    // {EAB4DC00-CE2E-483D-A86A-E6B99DA9599A}
    static constexpr GUID kATOK2015GUID = {
        0xEAB4DC00,
        0xCE2E,
        0x483D,
        {0xA8, 0x6A, 0xE6, 0xB9, 0x9D, 0xA9, 0x59, 0x9A}};
    if (mActiveTIPGUID == kATOK2015GUID) {
      return TextInputProcessorID::eATOK2015;
    }
    // {0B557B4C-5740-4110-A60A-1493FA10BF2B}
    static constexpr GUID kATOK2016GUID = {
        0x0B557B4C,
        0x5740,
        0x4110,
        {0xA6, 0x0A, 0x14, 0x93, 0xFA, 0x10, 0xBF, 0x2B}};
    if (mActiveTIPGUID == kATOK2016GUID) {
      return TextInputProcessorID::eATOK2016;
    }

    // * ATOK 2017
    //   - {6DBFD8F5-701D-11E6-920F-782BCBA6348F}
    // * ATOK Passport (confirmed with version 31.1.2)
    //   - {A38F2FD9-7199-45E1-841C-BE0313D8052F}

    if (IsATOKActiveInternal()) {
      return TextInputProcessorID::eATOKUnknown;
    }

    // {E6D66705-1EDA-4373-8D01-1D0CB2D054C7}
    static constexpr GUID kJapanist10GUID = {
        0xE6D66705,
        0x1EDA,
        0x4373,
        {0x8D, 0x01, 0x1D, 0x0C, 0xB2, 0xD0, 0x54, 0xC7}};
    if (mActiveTIPGUID == kJapanist10GUID) {
      return TextInputProcessorID::eJapanist10;
    }

    return TextInputProcessorID::eUnknown;
  }

  TextInputProcessorID ComputeActiveTIPAsTraditionalChinese() {
    // {B2F9C502-1742-11D4-9790-0080C882687E} (Win8.1, Win10)
    static constexpr GUID kMicrosoftBopomofoGUID = {
        0xB2F9C502,
        0x1742,
        0x11D4,
        {0x97, 0x90, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
    if (mActiveTIPGUID == kMicrosoftBopomofoGUID) {
      return TextInputProcessorID::eMicrosoftBopomofo;
    }
    // {4BDF9F03-C7D3-11D4-B2AB-0080C882687E} (Win7, Win8.1, Win10)
    static const GUID kMicrosoftChangJieGUID = {
        0x4BDF9F03,
        0xC7D3,
        0x11D4,
        {0xB2, 0xAB, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
    if (mActiveTIPGUID == kMicrosoftChangJieGUID) {
      return TextInputProcessorID::eMicrosoftChangJie;
    }
    // {761309DE-317A-11D4-9B5D-0080C882687E} (Win7)
    static constexpr GUID kMicrosoftPhoneticGUID = {
        0x761309DE,
        0x317A,
        0x11D4,
        {0x9B, 0x5D, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
    if (mActiveTIPGUID == kMicrosoftPhoneticGUID) {
      return TextInputProcessorID::eMicrosoftPhonetic;
    }
    // {6024B45F-5C54-11D4-B921-0080C882687E} (Win7, Win8.1, Win10)
    static constexpr GUID kMicrosoftQuickGUID = {
        0x6024B45F,
        0x5C54,
        0x11D4,
        {0xB9, 0x21, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
    if (mActiveTIPGUID == kMicrosoftQuickGUID) {
      return TextInputProcessorID::eMicrosoftQuick;
    }
    // {F3BA907A-6C7E-11D4-97FA-0080C882687E} (Win7)
    static constexpr GUID kMicrosoftNewChangJieGUID = {
        0xF3BA907A,
        0x6C7E,
        0x11D4,
        {0x97, 0xFA, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
    if (mActiveTIPGUID == kMicrosoftNewChangJieGUID) {
      return TextInputProcessorID::eMicrosoftNewChangJie;
    }
    // {B2F9C502-1742-11D4-9790-0080C882687E} (Win7)
    static constexpr GUID kMicrosoftNewPhoneticGUID = {
        0xB2F9C502,
        0x1742,
        0x11D4,
        {0x97, 0x90, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
    if (mActiveTIPGUID == kMicrosoftNewPhoneticGUID) {
      return TextInputProcessorID::eMicrosoftNewPhonetic;
    }
    // {0B883BA0-C1C7-11D4-87F9-0080C882687E} (Win7)
    static constexpr GUID kMicrosoftNewQuickGUID = {
        0x0B883BA0,
        0xC1C7,
        0x11D4,
        {0x87, 0xF9, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
    if (mActiveTIPGUID == kMicrosoftNewQuickGUID) {
      return TextInputProcessorID::eMicrosoftNewQuick;
    }

    // NOTE: There are some other Traditional Chinese TIPs installed in Windows:
    // * Chinese Traditional Array (version 6.0)
    //   - {D38EFF65-AA46-4FD5-91A7-67845FB02F5B} (Win7, Win8.1)
    // * Chinese Traditional DaYi (version 6.0)
    //   - {037B2C25-480C-4D7F-B027-D6CA6B69788A} (Win7, Win8.1)

    // {B58630B5-0ED3-4335-BBC9-E77BBCB43CAD}
    static const GUID kFreeChangJieGUID = {
        0xB58630B5,
        0x0ED3,
        0x4335,
        {0xBB, 0xC9, 0xE7, 0x7B, 0xBC, 0xB4, 0x3C, 0xAD}};
    if (mActiveTIPGUID == kFreeChangJieGUID) {
      return TextInputProcessorID::eFreeChangJie;
    }

    return TextInputProcessorID::eUnknown;
  }

  TextInputProcessorID ComputeActiveTIPAsSimplifiedChinese() {
    // FYI: This matches with neither "Microsoft Pinyin ABC Input Style" nor
    //      "Microsoft Pinyin New Experience Input Style" on Win7.
    // {FA550B04-5AD7-411F-A5AC-CA038EC515D7} (Win8.1, Win10)
    static constexpr GUID kMicrosoftPinyinGUID = {
        0xFA550B04,
        0x5AD7,
        0x411F,
        {0xA5, 0xAC, 0xCA, 0x03, 0x8E, 0xC5, 0x15, 0xD7}};
    if (mActiveTIPGUID == kMicrosoftPinyinGUID) {
      return TextInputProcessorID::eMicrosoftPinyin;
    }

    // {F3BA9077-6C7E-11D4-97FA-0080C882687E} (Win7)
    static constexpr GUID kMicrosoftPinyinNewExperienceInputStyleGUID = {
        0xF3BA9077,
        0x6C7E,
        0x11D4,
        {0x97, 0xFA, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
    if (mActiveTIPGUID == kMicrosoftPinyinNewExperienceInputStyleGUID) {
      return TextInputProcessorID::eMicrosoftPinyinNewExperienceInputStyle;
    }
    // {82590C13-F4DD-44F4-BA1D-8667246FDF8E} (Win8.1, Win10)
    static constexpr GUID kMicrosoftWubiGUID = {
        0x82590C13,
        0xF4DD,
        0x44F4,
        {0xBA, 0x1D, 0x86, 0x67, 0x24, 0x6F, 0xDF, 0x8E}};
    if (mActiveTIPGUID == kMicrosoftWubiGUID) {
      return TextInputProcessorID::eMicrosoftWubi;
    }
    // NOTE: There are some other Simplified Chinese TIPs installed in Windows:
    // * Chinese Simplified QuanPin (version 6.0)
    //   - {54FC610E-6ABD-4685-9DDD-A130BDF1B170} (Win8.1)
    // * Chinese Simplified ZhengMa (version 6.0)
    //   - {733B4D81-3BC3-4132-B91A-E9CDD5E2BFC9} (Win8.1)
    // * Chinese Simplified ShuangPin (version 6.0)
    //   - {EF63706D-31C4-490E-9DBB-BD150ADC454B} (Win8.1)
    // * Microsoft Pinyin ABC Input Style
    //   - {FCA121D2-8C6D-41FB-B2DE-A2AD110D4820} (Win7)
    return TextInputProcessorID::eUnknown;
  }

  TextInputProcessorID ComputeActiveTIPAsKorean() {
    // {B5FE1F02-D5F2-4445-9C03-C568F23C99A1} (Win7, Win8.1, Win10)
    static constexpr GUID kMicrosoftIMEForKoreanGUID = {
        0xB5FE1F02,
        0xD5F2,
        0x4445,
        {0x9C, 0x03, 0xC5, 0x68, 0xF2, 0x3C, 0x99, 0xA1}};
    if (mActiveTIPGUID == kMicrosoftIMEForKoreanGUID) {
      return TextInputProcessorID::eMicrosoftIMEForKorean;
    }
    // {B60AF051-257A-46BC-B9D3-84DAD819BAFB} (Win8.1, Win10)
    static constexpr GUID kMicrosoftOldHangulGUID = {
        0xB60AF051,
        0x257A,
        0x46BC,
        {0xB9, 0xD3, 0x84, 0xDA, 0xD8, 0x19, 0xBA, 0xFB}};
    if (mActiveTIPGUID == kMicrosoftOldHangulGUID) {
      return TextInputProcessorID::eMicrosoftOldHangul;
    }

    // NOTE: There is the other Korean TIP installed in Windows:
    // * Microsoft IME 2010
    //   - {48878C45-93F9-4aaf-A6A1-272CD863C4F5} (Win7)

    return TextInputProcessorID::eUnknown;
  }

 public:  // ITfInputProcessorProfileActivationSink
  STDMETHODIMP OnActivated(DWORD, LANGID, REFCLSID, REFGUID, REFGUID, HKL,
                           DWORD);

 private:
  TSFStaticSink();
  virtual ~TSFStaticSink() {}

  bool EnsureInitActiveTIPKeyboard();

  void Destroy();

  void GetTIPDescription(REFCLSID aTextService, LANGID aLangID,
                         REFGUID aProfile, nsAString& aDescription);
  bool IsTIPCategoryKeyboard(REFCLSID aTextService, LANGID aLangID,
                             REFGUID aProfile);

  TextInputProcessorID mActiveTIP;

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

  // Active TIP's GUID and CLSID
  GUID mActiveTIPGUID;
  CLSID mActiveTIPCLSID;

  static StaticRefPtr<TSFStaticSink> sInstance;
};

StaticRefPtr<TSFStaticSink> TSFStaticSink::sInstance;

TSFStaticSink::TSFStaticSink()
    : mActiveTIP(TextInputProcessorID::eNotComputed),
      mIPProfileCookie(TF_INVALID_COOKIE),
      mLangID(0),
      mIsIMM_IME(false),
      mOnActivatedCalled(false),
      mActiveTIPGUID(GUID_NULL) {}

bool TSFStaticSink::Init(ITfThreadMgr* aThreadMgr,
                         ITfInputProcessorProfiles* aInputProcessorProfiles) {
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
             "instance (0x%08X)",
             this, hr));
    return false;
  }

  // NOTE: On Vista or later, Windows let us know activate IME changed only
  //       with ITfInputProcessorProfileActivationSink.
  hr = source->AdviseSink(
      IID_ITfInputProcessorProfileActivationSink,
      static_cast<ITfInputProcessorProfileActivationSink*>(this),
      &mIPProfileCookie);
  if (FAILED(hr) || mIPProfileCookie == TF_INVALID_COOKIE) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p TSFStaticSink::Init() FAILED to install "
             "ITfInputProcessorProfileActivationSink (0x%08X)",
             this, hr));
    return false;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFStaticSink::Init(), "
           "mIPProfileCookie=0x%08X",
           this, mIPProfileCookie));
  return true;
}

void TSFStaticSink::Destroy() {
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
               "ITfSource instance (0x%08X)",
               this, hr));
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
TSFStaticSink::OnActivated(DWORD dwProfileType, LANGID langid, REFCLSID rclsid,
                           REFGUID catid, REFGUID guidProfile, HKL hkl,
                           DWORD dwFlags) {
  if ((dwFlags & TF_IPSINK_FLAG_ACTIVE) &&
      (dwProfileType == TF_PROFILETYPE_KEYBOARDLAYOUT ||
       catid == GUID_TFCAT_TIP_KEYBOARD)) {
    mOnActivatedCalled = true;
    mActiveTIP = TextInputProcessorID::eNotComputed;
    mActiveTIPGUID = guidProfile;
    mActiveTIPCLSID = rclsid;
    mLangID = langid & 0xFFFF;
    mIsIMM_IME = IsIMM_IME(hkl);
    GetTIPDescription(rclsid, langid, guidProfile,
                      mActiveTIPKeyboardDescription);
    if (mActiveTIPGUID != GUID_NULL) {
      // key should be "LocaleID|Description".  Although GUID of the
      // profile is unique key since description may be localized for system
      // language, unfortunately, it's too long to record as key with its
      // description.  Therefore, we should record only the description with
      // LocaleID because Microsoft IME may not include language information.
      // 72 is kMaximumKeyStringLength in TelemetryScalar.cpp
      nsAutoString key;
      key.AppendPrintf("0x%04X|", mLangID);
      nsAutoString description(mActiveTIPKeyboardDescription);
      static const uint32_t kMaxDescriptionLength = 72 - key.Length();
      if (description.Length() > kMaxDescriptionLength) {
        if (NS_IS_LOW_SURROGATE(description[kMaxDescriptionLength - 1]) &&
            NS_IS_HIGH_SURROGATE(description[kMaxDescriptionLength - 2])) {
          description.Truncate(kMaxDescriptionLength - 2);
        } else {
          description.Truncate(kMaxDescriptionLength - 1);
        }
        // U+2026 is "..."
        description.Append(char16_t(0x2026));
      }
      key.Append(description);
      Telemetry::ScalarSet(Telemetry::ScalarID::WIDGET_IME_NAME_ON_WINDOWS, key,
                           true);
    }
    // Notify IMEHandler of changing active keyboard layout.
    IMEHandler::OnKeyboardLayoutChanged();
  }
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFStaticSink::OnActivated(dwProfileType=%s (0x%08X), "
           "langid=0x%08X, rclsid=%s, catid=%s, guidProfile=%s, hkl=0x%08X, "
           "dwFlags=0x%08X (TF_IPSINK_FLAG_ACTIVE: %s)), mIsIMM_IME=%s, "
           "mActiveTIPDescription=\"%s\"",
           this,
           dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR
               ? "TF_PROFILETYPE_INPUTPROCESSOR"
           : dwProfileType == TF_PROFILETYPE_KEYBOARDLAYOUT
               ? "TF_PROFILETYPE_KEYBOARDLAYOUT"
               : "Unknown",
           dwProfileType, langid, GetCLSIDNameStr(rclsid).get(),
           GetGUIDNameStr(catid).get(), GetGUIDNameStr(guidProfile).get(), hkl,
           dwFlags, GetBoolName(dwFlags & TF_IPSINK_FLAG_ACTIVE),
           GetBoolName(mIsIMM_IME),
           NS_ConvertUTF16toUTF8(mActiveTIPKeyboardDescription).get()));
  return S_OK;
}

bool TSFStaticSink::EnsureInitActiveTIPKeyboard() {
  if (mOnActivatedCalled) {
    return true;
  }

  RefPtr<ITfInputProcessorProfileMgr> profileMgr;
  HRESULT hr = mInputProcessorProfiles->QueryInterface(
      IID_ITfInputProcessorProfileMgr, getter_AddRefs(profileMgr));
  if (FAILED(hr) || !profileMgr) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
             "to get input processor profile manager, hr=0x%08X",
             this, hr));
    return false;
  }

  TF_INPUTPROCESSORPROFILE profile;
  hr = profileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &profile);
  if (hr == S_FALSE) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
            ("0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
             "to get active keyboard layout profile due to no active profile, "
             "hr=0x%08X",
             this, hr));
    // XXX Should we call OnActivated() with arguments like non-TIP in this
    //     case?
    return false;
  }
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
             "to get active TIP keyboard, hr=0x%08X",
             this, hr));
    return false;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), "
           "calling OnActivated() manually...",
           this));
  OnActivated(profile.dwProfileType, profile.langid, profile.clsid,
              profile.catid, profile.guidProfile, ::GetKeyboardLayout(0),
              TF_IPSINK_FLAG_ACTIVE);
  return true;
}

void TSFStaticSink::GetTIPDescription(REFCLSID aTextService, LANGID aLangID,
                                      REFGUID aProfile,
                                      nsAString& aDescription) {
  aDescription.Truncate();

  if (aTextService == CLSID_NULL || aProfile == GUID_NULL) {
    return;
  }

  BSTR description = nullptr;
  HRESULT hr = mInputProcessorProfiles->GetLanguageProfileDescription(
      aTextService, aLangID, aProfile, &description);
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

bool TSFStaticSink::IsTIPCategoryKeyboard(REFCLSID aTextService, LANGID aLangID,
                                          REFGUID aProfile) {
  if (aTextService == CLSID_NULL || aProfile == GUID_NULL) {
    return false;
  }

  RefPtr<IEnumTfLanguageProfiles> enumLangProfiles;
  HRESULT hr = mInputProcessorProfiles->EnumLanguageProfiles(
      aLangID, getter_AddRefs(enumLangProfiles));
  if (FAILED(hr) || !enumLangProfiles) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFStaticSink::IsTIPCategoryKeyboard(), FAILED "
             "to get language profiles enumerator, hr=0x%08X",
             this, hr));
    return false;
  }

  TF_LANGUAGEPROFILE profile;
  ULONG fetch = 0;
  while (SUCCEEDED(enumLangProfiles->Next(1, &profile, &fetch)) && fetch) {
    // XXX We're not sure a profile is registered with two or more categories.
    if (profile.clsid == aTextService && profile.guidProfile == aProfile &&
        profile.catid == GUID_TFCAT_TIP_KEYBOARD) {
      return true;
    }
  }
  return false;
}

/******************************************************************/
/* TSFPreference                                                  */
/******************************************************************/

class TSFPrefs final {
 public:
#define DECL_AND_IMPL_BOOL_PREF(aPref, aName, aDefaultValue)                  \
  static bool aName() {                                                       \
    static bool s##aName##Value = Preferences::GetBool(aPref, aDefaultValue); \
    return s##aName##Value;                                                   \
  }

  DECL_AND_IMPL_BOOL_PREF("intl.ime.hack.set_input_scope_of_url_bar_to_default",
                          ShouldSetInputScopeOfURLBarToDefault, true)
  DECL_AND_IMPL_BOOL_PREF(
      "intl.tsf.hack.allow_to_stop_hacking_on_build_17643_or_later",
      AllowToStopHackingOnBuild17643OrLater, false)
  DECL_AND_IMPL_BOOL_PREF("intl.tsf.hack.atok.create_native_caret",
                          NeedToCreateNativeCaretForLegacyATOK, true)
  DECL_AND_IMPL_BOOL_PREF(
      "intl.tsf.hack.atok.do_not_return_no_layout_error_of_composition_string",
      DoNotReturnNoLayoutErrorToATOKOfCompositionString, true)
  DECL_AND_IMPL_BOOL_PREF(
      "intl.tsf.hack.japanist10."
      "do_not_return_no_layout_error_of_composition_string",
      DoNotReturnNoLayoutErrorToJapanist10OfCompositionString, true)
  DECL_AND_IMPL_BOOL_PREF(
      "intl.tsf.hack.ms_simplified_chinese.do_not_return_no_layout_error",
      DoNotReturnNoLayoutErrorToMSSimplifiedTIP, true)
  DECL_AND_IMPL_BOOL_PREF(
      "intl.tsf.hack.ms_traditional_chinese.do_not_return_no_layout_error",
      DoNotReturnNoLayoutErrorToMSTraditionalTIP, true)
  DECL_AND_IMPL_BOOL_PREF(
      "intl.tsf.hack.free_chang_jie.do_not_return_no_layout_error",
      DoNotReturnNoLayoutErrorToFreeChangJie, true)
  DECL_AND_IMPL_BOOL_PREF(
      "intl.tsf.hack.ms_japanese_ime.do_not_return_no_layout_error_at_first_"
      "char",
      DoNotReturnNoLayoutErrorToMSJapaneseIMEAtFirstChar, true)
  DECL_AND_IMPL_BOOL_PREF(
      "intl.tsf.hack.ms_japanese_ime.do_not_return_no_layout_error_at_caret",
      DoNotReturnNoLayoutErrorToMSJapaneseIMEAtCaret, true)
  DECL_AND_IMPL_BOOL_PREF(
      "intl.tsf.hack.ms_simplified_chinese.query_insert_result",
      NeedToHackQueryInsertForMSSimplifiedTIP, true)
  DECL_AND_IMPL_BOOL_PREF(
      "intl.tsf.hack.ms_traditional_chinese.query_insert_result",
      NeedToHackQueryInsertForMSTraditionalTIP, true)

#undef DECL_AND_IMPL_BOOL_PREF
};

/******************************************************************/
/* TSFTextStore                                                   */
/******************************************************************/

StaticRefPtr<ITfThreadMgr> TSFTextStore::sThreadMgr;
StaticRefPtr<ITfMessagePump> TSFTextStore::sMessagePump;
StaticRefPtr<ITfKeystrokeMgr> TSFTextStore::sKeystrokeMgr;
StaticRefPtr<ITfDisplayAttributeMgr> TSFTextStore::sDisplayAttrMgr;
StaticRefPtr<ITfCategoryMgr> TSFTextStore::sCategoryMgr;
StaticRefPtr<ITfCompartment> TSFTextStore::sCompartmentForOpenClose;
StaticRefPtr<ITfDocumentMgr> TSFTextStore::sDisabledDocumentMgr;
StaticRefPtr<ITfContext> TSFTextStore::sDisabledContext;
StaticRefPtr<ITfInputProcessorProfiles> TSFTextStore::sInputProcessorProfiles;
StaticRefPtr<TSFTextStore> TSFTextStore::sEnabledTextStore;
const MSG* TSFTextStore::sHandlingKeyMsg = nullptr;
DWORD TSFTextStore::sClientId = 0;
bool TSFTextStore::sIsKeyboardEventDispatched = false;

#define TEXTSTORE_DEFAULT_VIEW (1)

TSFTextStore::TSFTextStore()
    : mEditCookie(0),
      mSinkMask(0),
      mLock(0),
      mLockQueued(0),
      mHandlingKeyMessage(0),
      mRequestedAttrValues(false),
      mIsRecordingActionsWithoutLock(false),
      mHasReturnedNoLayoutError(false),
      mWaitingQueryLayout(false),
      mPendingDestroy(false),
      mDeferClearingContentForTSF(false),
      mDeferNotifyingTSF(false),
      mDeferCommittingComposition(false),
      mDeferCancellingComposition(false),
      mDestroyed(false),
      mBeingDestroyed(false) {
  for (int32_t i = 0; i < NUM_OF_SUPPORTED_ATTRS; i++) {
    mRequestedAttrs[i] = false;
  }

  // We hope that 5 or more actions don't occur at once.
  mPendingActions.SetCapacity(5);

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::TSFTextStore() SUCCEEDED", this));
}

TSFTextStore::~TSFTextStore() {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore instance is destroyed", this));
}

bool TSFTextStore::Init(nsWindowBase* aWidget, const InputContext& aContext) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::Init(aWidget=0x%p)", this, aWidget));

  if (NS_WARN_IF(!aWidget) || NS_WARN_IF(aWidget->Destroyed())) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::Init() FAILED due to being initialized with "
             "destroyed widget",
             this));
    return false;
  }

  if (mDocumentMgr) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::Init() FAILED due to already initialized",
             this));
    return false;
  }

  mWidget = aWidget;
  if (NS_WARN_IF(!mWidget)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::Init() FAILED "
             "due to aWidget is nullptr ",
             this));
    return false;
  }
  mDispatcher = mWidget->GetTextEventDispatcher();
  if (NS_WARN_IF(!mDispatcher)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::Init() FAILED "
             "due to aWidget->GetTextEventDispatcher() failure",
             this));
    return false;
  }

  SetInputScope(aContext.mHTMLInputType, aContext.mHTMLInputInputmode,
                aContext.mInPrivateBrowsing);

  // Create document manager
  RefPtr<ITfThreadMgr> threadMgr = sThreadMgr;
  RefPtr<ITfDocumentMgr> documentMgr;
  HRESULT hr = threadMgr->CreateDocumentMgr(getter_AddRefs(documentMgr));
  if (NS_WARN_IF(FAILED(hr))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::Init() FAILED to create ITfDocumentMgr "
             "(0x%08X)",
             this, hr));
    return false;
  }
  if (NS_WARN_IF(mDestroyed)) {
    MOZ_LOG(
        sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::Init() FAILED to create ITfDocumentMgr due to "
         "TextStore being destroyed during calling "
         "ITfThreadMgr::CreateDocumentMgr()",
         this));
    return false;
  }
  // Create context and add it to document manager
  RefPtr<ITfContext> context;
  hr = documentMgr->CreateContext(sClientId, 0,
                                  static_cast<ITextStoreACP*>(this),
                                  getter_AddRefs(context), &mEditCookie);
  if (NS_WARN_IF(FAILED(hr))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::Init() FAILED to create the context "
             "(0x%08X)",
             this, hr));
    return false;
  }
  if (NS_WARN_IF(mDestroyed)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::Init() FAILED to create ITfContext due to "
             "TextStore being destroyed during calling "
             "ITfDocumentMgr::CreateContext()",
             this));
    return false;
  }

  hr = documentMgr->Push(context);
  if (NS_WARN_IF(FAILED(hr))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::Init() FAILED to push the context (0x%08X)",
             this, hr));
    return false;
  }
  if (NS_WARN_IF(mDestroyed)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::Init() FAILED to create ITfContext due to "
             "TextStore being destroyed during calling ITfDocumentMgr::Push()",
             this));
    documentMgr->Pop(TF_POPF_ALL);
    return false;
  }

  mDocumentMgr = documentMgr;
  mContext = context;

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::Init() succeeded: "
           "mDocumentMgr=0x%p, mContext=0x%p, mEditCookie=0x%08X",
           this, mDocumentMgr.get(), mContext.get(), mEditCookie));

  return true;
}

void TSFTextStore::Destroy() {
  if (mBeingDestroyed) {
    return;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::Destroy(), mLock=%s, "
           "mComposition=%s, mHandlingKeyMessage=%u",
           this, GetLockFlagNameStr(mLock).get(),
           ToString(mComposition).c_str(), mHandlingKeyMessage));

  mDestroyed = true;

  // Destroy native caret first because it's not directly related to TSF and
  // there may be another textstore which gets focus.  So, we should avoid
  // to destroy caret after the new one recreates caret.
  IMEHandler::MaybeDestroyNativeCaret();

  if (mLock) {
    mPendingDestroy = true;
    return;
  }

  AutoRestore<bool> savedBeingDestroyed(mBeingDestroyed);
  mBeingDestroyed = true;

  // If there is composition, TSF keeps the composition even after the text
  // store destroyed.  So, we should clear the composition here.
  if (mComposition.isSome()) {
    CommitCompositionInternal(false);
  }

  if (mSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
            ("0x%p   TSFTextStore::Destroy(), calling "
             "ITextStoreACPSink::OnLayoutChange(TS_LC_DESTROY)...",
             this));
    RefPtr<ITextStoreACPSink> sink = mSink;
    sink->OnLayoutChange(TS_LC_DESTROY, TEXTSTORE_DEFAULT_VIEW);
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

void TSFTextStore::ReleaseTSFObjects() {
  MOZ_ASSERT(!mHandlingKeyMessage);

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::ReleaseTSFObjects()", this));

  mContext = nullptr;
  if (mDocumentMgr) {
    RefPtr<ITfDocumentMgr> documentMgr = mDocumentMgr.forget();
    documentMgr->Pop(TF_POPF_ALL);
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
TSFTextStore::QueryInterface(REFIID riid, void** ppv) {
  *ppv = nullptr;
  if ((IID_IUnknown == riid) || (IID_ITextStoreACP == riid)) {
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
          ("0x%p TSFTextStore::QueryInterface() FAILED, riid=%s", this,
           GetRIIDNameStr(riid).get()));
  return E_NOINTERFACE;
}

STDMETHODIMP
TSFTextStore::AdviseSink(REFIID riid, IUnknown* punk, DWORD dwMask) {
  MOZ_LOG(
      sTextStoreLog, LogLevel::Info,
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
             "unsupported interface",
             this));
    return E_INVALIDARG;  // means unsupported interface.
  }

  if (!mSink) {
    // Install sink
    punk->QueryInterface(IID_ITextStoreACPSink, getter_AddRefs(mSink));
    if (!mSink) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::AdviseSink() FAILED due to "
               "punk not having the interface",
               this));
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
               "the sink being different from the stored sink",
               this));
      return CONNECT_E_ADVISELIMIT;
    }
  }
  // Update mask either for a new sink or an existing sink
  mSinkMask = dwMask;
  return S_OK;
}

STDMETHODIMP
TSFTextStore::UnadviseSink(IUnknown* punk) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::UnadviseSink(punk=0x%p), mSink=0x%p", this, punk,
           mSink.get()));

  if (!punk) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::UnadviseSink() FAILED due to the null punk",
             this));
    return E_INVALIDARG;
  }
  if (!mSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::UnadviseSink() FAILED due to "
             "any sink not stored",
             this));
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
             "the sink being different from the stored sink",
             this));
    return CONNECT_E_NOCONNECTION;
  }
  mSink = nullptr;
  mSinkMask = 0;
  return S_OK;
}

STDMETHODIMP
TSFTextStore::RequestLock(DWORD dwLockFlags, HRESULT* phrSession) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::RequestLock(dwLockFlags=%s, phrSession=0x%p), "
           "mLock=%s, mDestroyed=%s",
           this, GetLockFlagNameStr(dwLockFlags).get(), phrSession,
           GetLockFlagNameStr(mLock).get(), GetBoolName(mDestroyed)));

  if (!mSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RequestLock() FAILED due to "
             "any sink not stored",
             this));
    return E_FAIL;
  }
  if (mDestroyed &&
      (mContentForTSF.isNothing() || mSelectionForTSF.isNothing())) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RequestLock() FAILED due to "
             "being destroyed and no information of the contents",
             this));
    return E_FAIL;
  }
  if (!phrSession) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RequestLock() FAILED due to "
             "null phrSession",
             this));
    return E_INVALIDARG;
  }

  if (!mLock) {
    // put on lock
    mLock = dwLockFlags & (~TS_LF_SYNC);
    MOZ_LOG(
        sTextStoreLog, LogLevel::Info,
        ("0x%p   Locking (%s) >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>",
         this, GetLockFlagNameStr(mLock).get()));
    // Don't release this instance during this lock because this is called by
    // TSF but they don't grab us during this call.
    RefPtr<TSFTextStore> kungFuDeathGrip(this);
    RefPtr<ITextStoreACPSink> sink = mSink;
    *phrSession = sink->OnLockGranted(mLock);
    MOZ_LOG(
        sTextStoreLog, LogLevel::Info,
        ("0x%p   Unlocked (%s) <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
         "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<",
         this, GetLockFlagNameStr(mLock).get()));
    DidLockGranted();
    while (mLockQueued) {
      mLock = mLockQueued;
      mLockQueued = 0;
      MOZ_LOG(sTextStoreLog, LogLevel::Info,
              ("0x%p   Locking for the request in the queue (%s) >>>>>>>>>>>>>>"
               ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
               ">>>>>",
               this, GetLockFlagNameStr(mLock).get()));
      sink->OnLockGranted(mLock);
      MOZ_LOG(sTextStoreLog, LogLevel::Info,
              ("0x%p   Unlocked (%s) <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
               "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
               "<<<<<",
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
             "queue, *phrSession=TS_S_ASYNC",
             this));
    return S_OK;
  }

  // no more locks allowed
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::RequestLock() didn't allow to lock, "
           "*phrSession=TS_E_SYNCHRONOUS",
           this));
  *phrSession = TS_E_SYNCHRONOUS;
  return E_FAIL;
}

void TSFTextStore::DidLockGranted() {
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

void TSFTextStore::DispatchEvent(WidgetGUIEvent& aEvent) {
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

void TSFTextStore::FlushPendingActions() {
  if (!mWidget || mWidget->Destroyed()) {
    // Note that don't clear mContentForTSF because TIP may try to commit
    // composition with a document lock.  In such case, TSFTextStore needs to
    // behave as expected by TIP.
    mPendingActions.Clear();
    mPendingSelectionChangeData.Clear();
    mHasReturnedNoLayoutError = false;
    return;
  }

  // Some TIP may request lock but does nothing during the lock.  In such case,
  // this should do nothing.  For example, when MS-IME for Japanese is active
  // and we're inactivating, this case occurs and causes different behavior
  // from the other TIPs.
  if (mPendingActions.IsEmpty()) {
    return;
  }

  RefPtr<nsWindowBase> widget(mWidget);
  nsresult rv = mDispatcher->BeginNativeInputTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::FlushPendingActions() "
             "FAILED due to BeginNativeInputTransaction() failure",
             this));
    return;
  }
  for (uint32_t i = 0; i < mPendingActions.Length(); i++) {
    PendingAction& action = mPendingActions[i];
    switch (action.mType) {
      case PendingAction::Type::eKeyboardEvent:
        if (mDestroyed) {
          MOZ_LOG(sTextStoreLog, LogLevel::Warning,
                  ("0x%p   TSFTextStore::FlushPendingActions() "
                   "IGNORED pending KeyboardEvent(%s) due to already destroyed",
                   action.mKeyMsg.message == WM_KEYDOWN ? "eKeyDown" : "eKeyUp",
                   this));
        }
        MOZ_DIAGNOSTIC_ASSERT(action.mKeyMsg.message == WM_KEYDOWN ||
                              action.mKeyMsg.message == WM_KEYUP);
        DispatchKeyboardEventAsProcessedByIME(action.mKeyMsg);
        if (!widget || widget->Destroyed()) {
          break;
        }
        break;
      case PendingAction::Type::eCompositionStart: {
        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
                ("0x%p   TSFTextStore::FlushPendingActions() "
                 "flushing Type::eCompositionStart={ mSelectionStart=%d, "
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
          selectionSet.mExpandToClusterBoundary =
              TSFStaticSink::ActiveTIP() !=
                  TextInputProcessorID::eKeymanDesktop &&
              StaticPrefs::
                  intl_tsf_hack_extend_setting_selection_range_to_cluster_boundaries();
          DispatchEvent(selectionSet);
          if (!selectionSet.mSucceeded) {
            MOZ_LOG(sTextStoreLog, LogLevel::Error,
                    ("0x%p   TSFTextStore::FlushPendingActions() "
                     "FAILED due to eSetSelection failure",
                     this));
            break;
          }
        }

        // eCompositionStart always causes
        // NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED.  Therefore, we should
        // wait to clear mContentForTSF until it's notified.
        mDeferClearingContentForTSF = true;

        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
                ("0x%p   TSFTextStore::FlushPendingActions() "
                 "dispatching compositionstart event...",
                 this));
        WidgetEventTime eventTime = widget->CurrentMessageWidgetEventTime();
        nsEventStatus status;
        rv = mDispatcher->StartComposition(status, &eventTime);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          MOZ_LOG(sTextStoreLog, LogLevel::Error,
                  ("0x%p   TSFTextStore::FlushPendingActions() "
                   "FAILED to dispatch compositionstart event, "
                   "IsHandlingCompositionInContent()=%s",
                   this, GetBoolName(IsHandlingCompositionInContent())));
          // XXX Is this right? If there is a composition in content,
          //     shouldn't we wait NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED?
          mDeferClearingContentForTSF = !IsHandlingCompositionInContent();
        }
        if (!widget || widget->Destroyed()) {
          break;
        }
        break;
      }
      case PendingAction::Type::eCompositionUpdate: {
        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
                ("0x%p   TSFTextStore::FlushPendingActions() "
                 "flushing Type::eCompositionUpdate={ mData=\"%s\", "
                 "mRanges=0x%p, mRanges->Length()=%d }",
                 this, GetEscapedUTF8String(action.mData).get(),
                 action.mRanges.get(),
                 action.mRanges ? action.mRanges->Length() : 0));

        // eCompositionChange causes a DOM text event, the IME will be notified
        // of NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED.  In this case, we
        // should not clear mContentForTSF until we notify the IME of the
        // composition update.
        mDeferClearingContentForTSF = true;

        rv = mDispatcher->SetPendingComposition(action.mData, action.mRanges);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          MOZ_LOG(sTextStoreLog, LogLevel::Error,
                  ("0x%p   TSFTextStore::FlushPendingActions() "
                   "FAILED to setting pending composition... "
                   "IsHandlingCompositionInContent()=%s",
                   this, GetBoolName(IsHandlingCompositionInContent())));
          // XXX Is this right? If there is a composition in content,
          //     shouldn't we wait NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED?
          mDeferClearingContentForTSF = !IsHandlingCompositionInContent();
        } else {
          MOZ_LOG(sTextStoreLog, LogLevel::Debug,
                  ("0x%p   TSFTextStore::FlushPendingActions() "
                   "dispatching compositionchange event...",
                   this));
          WidgetEventTime eventTime = widget->CurrentMessageWidgetEventTime();
          nsEventStatus status;
          rv = mDispatcher->FlushPendingComposition(status, &eventTime);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            MOZ_LOG(sTextStoreLog, LogLevel::Error,
                    ("0x%p   TSFTextStore::FlushPendingActions() "
                     "FAILED to dispatch compositionchange event, "
                     "IsHandlingCompositionInContent()=%s",
                     this, GetBoolName(IsHandlingCompositionInContent())));
            // XXX Is this right? If there is a composition in content,
            //     shouldn't we wait NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED?
            mDeferClearingContentForTSF = !IsHandlingCompositionInContent();
          }
          // Be aware, the mWidget might already have been destroyed.
        }
        break;
      }
      case PendingAction::Type::eCompositionEnd: {
        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
                ("0x%p   TSFTextStore::FlushPendingActions() "
                 "flushing Type::eCompositionEnd={ mData=\"%s\" }",
                 this, GetEscapedUTF8String(action.mData).get()));

        // Dispatching eCompositionCommit causes a DOM text event, then,
        // the IME will be notified of NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED
        // when focused content actually handles the event.  For example,
        // when focused content is in a remote process, it's sent when
        // all dispatched composition events have been handled in the remote
        // process.  So, until then, we don't have newer content information.
        // Therefore, we need to put off to clear mContentForTSF.
        mDeferClearingContentForTSF = true;

        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
                ("0x%p   TSFTextStore::FlushPendingActions(), "
                 "dispatching compositioncommit event...",
                 this));
        WidgetEventTime eventTime = widget->CurrentMessageWidgetEventTime();
        nsEventStatus status;
        rv = mDispatcher->CommitComposition(status, &action.mData, &eventTime);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          MOZ_LOG(sTextStoreLog, LogLevel::Error,
                  ("0x%p   TSFTextStore::FlushPendingActions() "
                   "FAILED to dispatch compositioncommit event, "
                   "IsHandlingCompositionInContent()=%s",
                   this, GetBoolName(IsHandlingCompositionInContent())));
          // XXX Is this right? If there is a composition in content,
          //     shouldn't we wait NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED?
          mDeferClearingContentForTSF = !IsHandlingCompositionInContent();
        }
        break;
      }
      case PendingAction::Type::eSetSelection: {
        MOZ_LOG(
            sTextStoreLog, LogLevel::Debug,
            ("0x%p   TSFTextStore::FlushPendingActions() "
             "flushing Type::eSetSelection={ mSelectionStart=%d, "
             "mSelectionLength=%d, mSelectionReversed=%s }, "
             "mDestroyed=%s",
             this, action.mSelectionStart, action.mSelectionLength,
             GetBoolName(action.mSelectionReversed), GetBoolName(mDestroyed)));

        if (mDestroyed) {
          MOZ_LOG(sTextStoreLog, LogLevel::Warning,
                  ("0x%p   TSFTextStore::FlushPendingActions() "
                   "IGNORED pending selectionset due to already destroyed",
                   this));
          break;
        }

        WidgetSelectionEvent selectionSet(true, eSetSelection, widget);
        selectionSet.mOffset = static_cast<uint32_t>(action.mSelectionStart);
        selectionSet.mLength = static_cast<uint32_t>(action.mSelectionLength);
        selectionSet.mReversed = action.mSelectionReversed;
        selectionSet.mExpandToClusterBoundary =
            TSFStaticSink::ActiveTIP() !=
                TextInputProcessorID::eKeymanDesktop &&
            StaticPrefs::
                intl_tsf_hack_extend_setting_selection_range_to_cluster_boundaries();
        DispatchEvent(selectionSet);
        if (!selectionSet.mSucceeded) {
          MOZ_LOG(sTextStoreLog, LogLevel::Error,
                  ("0x%p   TSFTextStore::FlushPendingActions() "
                   "FAILED due to eSetSelection failure",
                   this));
          break;
        }
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
             "qutting since the mWidget has gone",
             this));
    break;
  }
  mPendingActions.Clear();
}

void TSFTextStore::MaybeFlushPendingNotifications() {
  if (IsReadLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
            ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
             "putting off flushing pending notifications due to being the "
             "document locked...",
             this));
    return;
  }

  if (mDeferCommittingComposition) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
            ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
             "calling TSFTextStore::CommitCompositionInternal(false)...",
             this));
    mDeferCommittingComposition = mDeferCancellingComposition = false;
    CommitCompositionInternal(false);
  } else if (mDeferCancellingComposition) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
            ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
             "calling TSFTextStore::CommitCompositionInternal(true)...",
             this));
    mDeferCommittingComposition = mDeferCancellingComposition = false;
    CommitCompositionInternal(true);
  }

  if (mDeferNotifyingTSF) {
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
            ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
             "putting off flushing pending notifications due to being "
             "dispatching events...",
             this));
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
             "does nothing because this has already destroyed completely...",
             this));
    return;
  }

  if (!mDeferClearingContentForTSF && mContentForTSF.isSome()) {
    mContentForTSF.reset();
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
            ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
             "mContentForTSF is set to `Nothing`",
             this));
  }

  // When there is no cached content, we can sync actual contents and TSF/TIP
  // expecting contents.
  RefPtr<TSFTextStore> kungFuDeathGrip = this;
  Unused << kungFuDeathGrip;
  if (mContentForTSF.isNothing()) {
    if (mPendingTextChangeData.IsValid()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Info,
              ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
               "calling TSFTextStore::NotifyTSFOfTextChange()...",
               this));
      NotifyTSFOfTextChange();
    }
    if (mPendingSelectionChangeData.IsValid()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Info,
              ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
               "calling TSFTextStore::NotifyTSFOfSelectionChange()...",
               this));
      NotifyTSFOfSelectionChange();
    }
  }

  if (mHasReturnedNoLayoutError) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
            ("0x%p   TSFTextStore::MaybeFlushPendingNotifications(), "
             "calling TSFTextStore::NotifyTSFOfLayoutChange()...",
             this));
    NotifyTSFOfLayoutChange();
  }
}

void TSFTextStore::MaybeDispatchKeyboardEventAsProcessedByIME() {
  // If we've already been destroyed, we cannot do anything.
  if (mDestroyed) {
    MOZ_LOG(
        sTextStoreLog, LogLevel::Debug,
        ("0x%p   TSFTextStore::MaybeDispatchKeyboardEventAsProcessedByIME(), "
         "does nothing because it's already been destroyed",
         this));
    return;
  }

  // If we're not handling key message or we've already dispatched a keyboard
  // event for the handling key message, we should do nothing anymore.
  if (!sHandlingKeyMsg || sIsKeyboardEventDispatched) {
    MOZ_LOG(
        sTextStoreLog, LogLevel::Debug,
        ("0x%p   TSFTextStore::MaybeDispatchKeyboardEventAsProcessedByIME(), "
         "does nothing because not necessary to dispatch keyboard event",
         this));
    return;
  }

  sIsKeyboardEventDispatched = true;
  // If the document is locked, just adding the task to dispatching an event
  // to the queue.
  if (IsReadLocked()) {
    MOZ_LOG(
        sTextStoreLog, LogLevel::Debug,
        ("0x%p   TSFTextStore::MaybeDispatchKeyboardEventAsProcessedByIME(), "
         "adding to dispatch a keyboard event into the queue...",
         this));
    PendingAction* action = mPendingActions.AppendElement();
    action->mType = PendingAction::Type::eKeyboardEvent;
    memcpy(&action->mKeyMsg, sHandlingKeyMsg, sizeof(MSG));
    return;
  }

  // Otherwise, dispatch a keyboard event.
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::MaybeDispatchKeyboardEventAsProcessedByIME(), "
           "trying to dispatch a keyboard event...",
           this));
  DispatchKeyboardEventAsProcessedByIME(*sHandlingKeyMsg);
}

void TSFTextStore::DispatchKeyboardEventAsProcessedByIME(const MSG& aMsg) {
  MOZ_ASSERT(mWidget);
  MOZ_ASSERT(!mWidget->Destroyed());
  MOZ_ASSERT(!mDestroyed);

  ModifierKeyState modKeyState;
  MSG msg(aMsg);
  msg.wParam = VK_PROCESSKEY;
  NativeKey nativeKey(mWidget, msg, modKeyState);
  switch (aMsg.message) {
    case WM_KEYDOWN:
      MOZ_LOG(sTextStoreLog, LogLevel::Debug,
              ("0x%p   TSFTextStore::DispatchKeyboardEventAsProcessedByIME(), "
               "dispatching an eKeyDown event...",
               this));
      nativeKey.HandleKeyDownMessage();
      break;
    case WM_KEYUP:
      MOZ_LOG(sTextStoreLog, LogLevel::Debug,
              ("0x%p   TSFTextStore::DispatchKeyboardEventAsProcessedByIME(), "
               "dispatching an eKeyUp event...",
               this));
      nativeKey.HandleKeyUpMessage();
      break;
    default:
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::DispatchKeyboardEventAsProcessedByIME(), "
               "ERROR, it doesn't handle the message",
               this));
      break;
  }
}

STDMETHODIMP
TSFTextStore::GetStatus(TS_STATUS* pdcs) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::GetStatus(pdcs=0x%p)", this, pdcs));

  if (!pdcs) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetStatus() FAILED due to null pdcs", this));
    return E_INVALIDARG;
  }
  // We manage on-screen keyboard by own.
  pdcs->dwDynamicFlags = TS_SD_INPUTPANEMANUALDISPLAYENABLE;
  // we use a "flat" text model for TSF support so no hidden text
  pdcs->dwStaticFlags = TS_SS_NOHIDDENTEXT;
  return S_OK;
}

STDMETHODIMP
TSFTextStore::QueryInsert(LONG acpTestStart, LONG acpTestEnd, ULONG cch,
                          LONG* pacpResultStart, LONG* pacpResultEnd) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::QueryInsert(acpTestStart=%ld, "
           "acpTestEnd=%ld, cch=%lu, pacpResultStart=0x%p, pacpResultEnd=0x%p)",
           this, acpTestStart, acpTestEnd, cch, acpTestStart, acpTestEnd));

  if (!pacpResultStart || !pacpResultEnd) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::QueryInsert() FAILED due to "
             "the null argument",
             this));
    return E_INVALIDARG;
  }

  if (acpTestStart < 0 || acpTestStart > acpTestEnd) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::QueryInsert() FAILED due to "
             "wrong argument",
             this));
    return E_INVALIDARG;
  }

  // XXX need to adjust to cluster boundary
  // Assume we are given good offsets for now
  if (IsWin8OrLater() && mComposition.isNothing() &&
      ((TSFPrefs::NeedToHackQueryInsertForMSTraditionalTIP() &&
        TSFStaticSink::IsMSChangJieOrMSQuickActive()) ||
       (TSFPrefs::NeedToHackQueryInsertForMSSimplifiedTIP() &&
        TSFStaticSink::IsMSPinyinOrMSWubiActive()))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Warning,
            ("0x%p   TSFTextStore::QueryInsert() WARNING using different "
             "result for the TIP",
             this));
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
TSFTextStore::GetSelection(ULONG ulIndex, ULONG ulCount,
                           TS_SELECTION_ACP* pSelection, ULONG* pcFetched) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::GetSelection(ulIndex=%lu, ulCount=%lu, "
           "pSelection=0x%p, pcFetched=0x%p)",
           this, ulIndex, ulCount, pSelection, pcFetched));

  if (!IsReadLocked()) {
    MOZ_LOG(
        sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::GetSelection() FAILED due to not locked", this));
    return TS_E_NOLOCK;
  }
  if (!ulCount || !pSelection || !pcFetched) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetSelection() FAILED due to "
             "null argument",
             this));
    return E_INVALIDARG;
  }

  *pcFetched = 0;

  if (ulIndex != static_cast<ULONG>(TS_DEFAULT_SELECTION) && ulIndex != 0) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetSelection() FAILED due to "
             "unsupported selection",
             this));
    return TS_E_NOSELECTION;
  }

  Maybe<Selection>& selectionForTSF = SelectionForTSF();
  if (selectionForTSF.isNothing()) {
    if (DoNotReturnErrorFromGetSelection()) {
      TS_SELECTION_ACP acp;
      acp.acpStart = acp.acpEnd = 0;
      acp.style.ase = TS_AE_START;
      acp.style.fInterimChar = FALSE;
      *pSelection = acp;
      *pcFetched = 1;
      MOZ_LOG(
          sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::GetSelection() returns fake selection range "
           "for avoiding a crash in TSF, *pSelection=%s",
           this, mozilla::ToString(*pSelection).c_str()));
      return S_OK;
    }
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetSelection() FAILED due to "
             "SelectionForTSF() failure",
             this));
    return E_FAIL;
  }
  *pSelection = selectionForTSF->ACPRef();
  *pcFetched = 1;
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::GetSelection() succeeded, *pSelection=%s",
           this, mozilla::ToString(*pSelection).c_str()));
  return S_OK;
}

// static
bool TSFTextStore::DoNotReturnErrorFromGetSelection() {
  // There is a crash bug of TSF if we return error from GetSelection().
  // That was introduced in Anniversary Update (build 14393, see bug 1312302)
  // TODO: We should avoid to run this hack on fixed builds.  When we get
  //       exact build number, we should get back here.
  static bool sTSFMayCrashIfGetSelectionReturnsError =
      IsWindows10BuildOrLater(14393);
  return sTSFMayCrashIfGetSelectionReturnsError;
}

Maybe<TSFTextStore::Content>& TSFTextStore::ContentForTSF() {
  // This should be called when the document is locked or the content hasn't
  // been abandoned yet.
  if (NS_WARN_IF(!IsReadLocked() && mContentForTSF.isNothing())) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::ContentForTSF(), FAILED, due to "
             "called wrong timing, IsReadLocked()=%s, mContentForTSF=Nothing",
             this, GetBoolName(IsReadLocked())));
    return mContentForTSF;
  }

  Maybe<Selection>& selectionForTSF = SelectionForTSF();
  if (selectionForTSF.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::ContentForTSF(), FAILED, due to "
             "SelectionForTSF() failure",
             this));
    mContentForTSF.reset();
    return mContentForTSF;
  }

  if (mContentForTSF.isNothing()) {
    nsString text;  // Don't use auto string for avoiding to copy long string.
    if (NS_WARN_IF(!GetCurrentText(text))) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::ContentForTSF(), FAILED, due to "
               "GetCurrentText() failure",
               this));
      return mContentForTSF;
    }

    mContentForTSF.emplace(*this, text);
    // Basically, the cached content which is expected by TSF/TIP should be
    // cleared after active composition is committed or the document lock is
    // unlocked.  However, in e10s mode, content will be modified
    // asynchronously.  In such case, mDeferClearingContentForTSF may be
    // true until whole dispatched events are handled by the focused editor.
    mDeferClearingContentForTSF = false;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::ContentForTSF(): mContentForTSF=%s", this,
           mozilla::ToString(mContentForTSF).c_str()));

  return mContentForTSF;
}

bool TSFTextStore::CanAccessActualContentDirectly() const {
  if (mContentForTSF.isNothing() || mSelectionForTSF.isNothing()) {
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

  return mSelectionForTSF->EqualsExceptDirection(mPendingSelectionChangeData);
}

bool TSFTextStore::GetCurrentText(nsAString& aTextContent) {
  if (mContentForTSF.isSome()) {
    aTextContent = mContentForTSF->TextRef();
    return true;
  }

  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(mWidget && !mWidget->Destroyed());

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::GetCurrentText(): "
           "retrieving text from the content...",
           this));

  WidgetQueryContentEvent queryTextContentEvent(true, eQueryTextContent,
                                                mWidget);
  queryTextContentEvent.InitForQueryTextContent(0, UINT32_MAX);
  mWidget->InitEvent(queryTextContentEvent);
  DispatchEvent(queryTextContentEvent);
  if (NS_WARN_IF(queryTextContentEvent.Failed())) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetCurrentText(), FAILED, due to "
             "eQueryTextContent failure",
             this));
    aTextContent.Truncate();
    return false;
  }

  aTextContent = queryTextContentEvent.mReply->DataRef();
  return true;
}

Maybe<TSFTextStore::Selection>& TSFTextStore::SelectionForTSF() {
  if (mSelectionForTSF.isNothing()) {
    MOZ_ASSERT(!mDestroyed);
    // If the window has never been available, we should crash since working
    // with broken values may make TIP confused.
    if (!mWidget || mWidget->Destroyed()) {
      MOZ_ASSERT_UNREACHABLE("There should be non-destroyed widget");
    }

    WidgetQueryContentEvent querySelectedTextEvent(true, eQuerySelectedText,
                                                   mWidget);
    mWidget->InitEvent(querySelectedTextEvent);
    DispatchEvent(querySelectedTextEvent);
    if (NS_WARN_IF(querySelectedTextEvent.DidNotFindSelection())) {
      return mSelectionForTSF;
    }
    MOZ_ASSERT(querySelectedTextEvent.mReply->mOffsetAndData.isSome());
    mSelectionForTSF =
        Some(Selection(querySelectedTextEvent.mReply->StartOffset(),
                       querySelectedTextEvent.mReply->DataLength(),
                       querySelectedTextEvent.mReply->mReversed,
                       querySelectedTextEvent.mReply->WritingModeRef()));
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::SelectionForTSF() succeeded, "
           "mSelectionForTSF=%s",
           this, ToString(mSelectionForTSF).c_str()));

  return mSelectionForTSF;
}

static HRESULT GetRangeExtent(ITfRange* aRange, LONG* aStart, LONG* aLength) {
  RefPtr<ITfRangeACP> rangeACP;
  aRange->QueryInterface(IID_ITfRangeACP, getter_AddRefs(rangeACP));
  NS_ENSURE_TRUE(rangeACP, E_FAIL);
  return rangeACP->GetExtent(aStart, aLength);
}

static TextRangeType GetGeckoSelectionValue(TF_DISPLAYATTRIBUTE& aDisplayAttr) {
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
TSFTextStore::GetDisplayAttribute(ITfProperty* aAttrProperty, ITfRange* aRange,
                                  TF_DISPLAYATTRIBUTE* aResult) {
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
             this, start - mComposition->StartOffset(),
             start - mComposition->StartOffset() + length,
             GetCommonReturnValueName(hr)));
  }

  VARIANT propValue;
  ::VariantInit(&propValue);
  hr = aAttrProperty->GetValue(TfEditCookie(mEditCookie), aRange, &propValue);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetDisplayAttribute() FAILED due to "
             "ITfProperty::GetValue() failed",
             this));
    return hr;
  }
  if (VT_I4 != propValue.vt) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetDisplayAttribute() FAILED due to "
             "ITfProperty::GetValue() returns non-VT_I4 value",
             this));
    ::VariantClear(&propValue);
    return E_FAIL;
  }

  RefPtr<ITfCategoryMgr> categoryMgr = GetCategoryMgr();
  if (NS_WARN_IF(!categoryMgr)) {
    return E_FAIL;
  }
  GUID guid;
  hr = categoryMgr->GetGUID(DWORD(propValue.lVal), &guid);
  ::VariantClear(&propValue);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetDisplayAttribute() FAILED due to "
             "ITfCategoryMgr::GetGUID() failed",
             this));
    return hr;
  }

  RefPtr<ITfDisplayAttributeMgr> displayAttrMgr = GetDisplayAttributeMgr();
  if (NS_WARN_IF(!displayAttrMgr)) {
    return E_FAIL;
  }
  RefPtr<ITfDisplayAttributeInfo> info;
  hr = displayAttrMgr->GetDisplayAttributeInfo(guid, getter_AddRefs(info),
                                               nullptr);
  if (FAILED(hr) || !info) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetDisplayAttribute() FAILED due to "
             "ITfDisplayAttributeMgr::GetDisplayAttributeInfo() failed",
             this));
    return hr;
  }

  hr = info->GetAttributeInfo(aResult);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetDisplayAttribute() FAILED due to "
             "ITfDisplayAttributeInfo::GetAttributeInfo() failed",
             this));
    return hr;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::GetDisplayAttribute() succeeded: "
           "Result={ %s }",
           this, GetDisplayAttrStr(*aResult).get()));
  return S_OK;
}

HRESULT
TSFTextStore::RestartCompositionIfNecessary(ITfRange* aRangeNew) {
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::RestartCompositionIfNecessary("
           "aRangeNew=0x%p), mComposition=%s",
           this, aRangeNew, ToString(mComposition).c_str()));

  if (mComposition.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RestartCompositionIfNecessary() FAILED "
             "due to no composition view",
             this));
    return E_FAIL;
  }

  HRESULT hr;
  RefPtr<ITfCompositionView> pComposition(mComposition->GetView());
  RefPtr<ITfRange> composingRange(aRangeNew);
  if (!composingRange) {
    hr = pComposition->GetRange(getter_AddRefs(composingRange));
    if (FAILED(hr)) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::RestartCompositionIfNecessary() "
               "FAILED due to pComposition->GetRange() failure",
               this));
      return hr;
    }
  }

  // Get starting offset of the composition
  LONG compStart = 0, compLength = 0;
  hr = GetRangeExtent(composingRange, &compStart, &compLength);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RestartCompositionIfNecessary() FAILED "
             "due to GetRangeExtent() failure",
             this));
    return hr;
  }

  if (mComposition->StartOffset() == compStart &&
      mComposition->Length() == compLength) {
    return S_OK;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::RestartCompositionIfNecessary(), "
           "restaring composition because of compostion range is changed "
           "(range=%ld-%ld, mComposition=%s)",
           this, compStart, compStart + compLength,
           ToString(mComposition).c_str()));

  // If the queried composition length is different from the length
  // of our composition string, OnUpdateComposition is being called
  // because a part of the original composition was committed.
  hr = RestartComposition(*mComposition, pComposition, composingRange);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RestartCompositionIfNecessary() "
             "FAILED due to RestartComposition() failure",
             this));
    return hr;
  }

  MOZ_LOG(
      sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::RestartCompositionIfNecessary() succeeded", this));
  return S_OK;
}

HRESULT TSFTextStore::RestartComposition(Composition& aCurrentComposition,
                                         ITfCompositionView* aCompositionView,
                                         ITfRange* aNewRange) {
  Maybe<Selection>& selectionForTSF = SelectionForTSF();

  LONG newStart, newLength;
  HRESULT hr = GetRangeExtent(aNewRange, &newStart, &newLength);
  LONG newEnd = newStart + newLength;

  if (selectionForTSF.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RestartComposition() FAILED "
             "due to SelectionForTSF() failure",
             this));
    return E_FAIL;
  }

  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RestartComposition() FAILED "
             "due to GetRangeExtent() failure",
             this));
    return hr;
  }

  // If the new range has no overlap with the crrent range, we just commit
  // the composition and restart new composition with the new range but
  // current selection range should be preserved.
  if (newStart >= aCurrentComposition.EndOffset() ||
      newEnd <= aCurrentComposition.StartOffset()) {
    RecordCompositionEndAction();
    RecordCompositionStartAction(aCompositionView, newStart, newLength, true);
    return S_OK;
  }

  MOZ_LOG(
      sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::RestartComposition(aCompositionView=0x%p, "
       "aNewRange=0x%p { newStart=%d, newLength=%d }), aCurrentComposition=%s, "
       "selectionForTSF=%s",
       this, aCompositionView, aNewRange, newStart, newLength,
       ToString(aCurrentComposition).c_str(),
       ToString(selectionForTSF).c_str()));

  // If the new range has an overlap with the current one, we should not commit
  // the whole current range to avoid creating an odd undo transaction.
  // I.e., the overlapped range which is being composed should not appear in
  // undo transaction.

  // Backup current composition data and selection data.
  Composition oldComposition = aCurrentComposition;
  Selection oldSelection = *selectionForTSF;

  // Commit only the part of composition.
  LONG keepComposingStartOffset =
      std::max(oldComposition.StartOffset(), newStart);
  LONG keepComposingEndOffset = std::min(oldComposition.EndOffset(), newEnd);
  MOZ_ASSERT(
      keepComposingStartOffset <= keepComposingEndOffset,
      "Why keepComposingEndOffset is smaller than keepComposingStartOffset?");
  LONG keepComposingLength = keepComposingEndOffset - keepComposingStartOffset;
  // Remove the overlapped part from the commit string.
  nsAutoString commitString(oldComposition.DataRef());
  commitString.Cut(keepComposingStartOffset - oldComposition.StartOffset(),
                   keepComposingLength);
  // Update the composition string.
  Maybe<Content>& contentForTSF = ContentForTSF();
  if (contentForTSF.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RestartComposition() FAILED "
             "due to ContentForTSF() failure",
             this));
    return E_FAIL;
  }
  contentForTSF->ReplaceTextWith(oldComposition.StartOffset(),
                                 oldComposition.Length(), commitString);
  MOZ_ASSERT(mComposition.isSome());
  // Record a compositionupdate action for commit the part of composing string.
  PendingAction* action = LastOrNewPendingCompositionUpdate();
  if (mComposition.isSome()) {
    action->mData = mComposition->DataRef();
  }
  action->mRanges->Clear();
  // Note that we shouldn't append ranges when composition string
  // is empty because it may cause TextComposition confused.
  if (!action->mData.IsEmpty()) {
    TextRange caretRange;
    caretRange.mStartOffset = caretRange.mEndOffset = static_cast<uint32_t>(
        oldComposition.StartOffset() + commitString.Length());
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
  contentForTSF->ReplaceSelectedTextWith(nsDependentSubstring(
      oldComposition.DataRef(),
      keepComposingStartOffset - oldComposition.StartOffset(),
      keepComposingLength));
  selectionForTSF = Some(oldSelection);

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::RestartComposition() succeeded, "
           "mComposition=%s, selectionForTSF=%s",
           this, ToString(mComposition).c_str(),
           ToString(selectionForTSF).c_str()));

  return S_OK;
}

static bool GetColor(const TF_DA_COLOR& aTSFColor, nscolor& aResult) {
  switch (aTSFColor.type) {
    case TF_CT_SYSCOLOR: {
      DWORD sysColor = ::GetSysColor(aTSFColor.nIndex);
      aResult =
          NS_RGB(GetRValue(sysColor), GetGValue(sysColor), GetBValue(sysColor));
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

static bool GetLineStyle(TF_DA_LINESTYLE aTSFLineStyle,
                         TextRangeStyle::LineStyle& aTextRangeLineStyle) {
  switch (aTSFLineStyle) {
    case TF_LS_NONE:
      aTextRangeLineStyle = TextRangeStyle::LineStyle::None;
      return true;
    case TF_LS_SOLID:
      aTextRangeLineStyle = TextRangeStyle::LineStyle::Solid;
      return true;
    case TF_LS_DOT:
      aTextRangeLineStyle = TextRangeStyle::LineStyle::Dotted;
      return true;
    case TF_LS_DASH:
      aTextRangeLineStyle = TextRangeStyle::LineStyle::Dashed;
      return true;
    case TF_LS_SQUIGGLE:
      aTextRangeLineStyle = TextRangeStyle::LineStyle::Wavy;
      return true;
    default:
      return false;
  }
}

HRESULT
TSFTextStore::RecordCompositionUpdateAction() {
  MOZ_LOG(
      sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::RecordCompositionUpdateAction(), mComposition=%s",
       this, ToString(mComposition).c_str()));

  if (mComposition.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RecordCompositionUpdateAction() FAILED "
             "due to no composition view",
             this));
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
  HRESULT hr =
      mContext->GetProperty(GUID_PROP_ATTRIBUTE, getter_AddRefs(attrPropetry));
  if (FAILED(hr) || !attrPropetry) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RecordCompositionUpdateAction() FAILED "
             "due to mContext->GetProperty() failure",
             this));
    return FAILED(hr) ? hr : E_FAIL;
  }

  RefPtr<ITfRange> composingRange;
  hr = mComposition->GetView()->GetRange(getter_AddRefs(composingRange));
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RecordCompositionUpdateAction() "
             "FAILED due to mComposition->GetView()->GetRange() failure",
             this));
    return hr;
  }

  RefPtr<IEnumTfRanges> enumRanges;
  hr = attrPropetry->EnumRanges(TfEditCookie(mEditCookie),
                                getter_AddRefs(enumRanges), composingRange);
  if (FAILED(hr) || !enumRanges) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RecordCompositionUpdateAction() FAILED "
             "due to attrPropetry->EnumRanges() failure",
             this));
    return FAILED(hr) ? hr : E_FAIL;
  }

  // First, put the log of content and selection here.
  Maybe<Selection>& selectionForTSF = SelectionForTSF();
  if (selectionForTSF.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RecordCompositionUpdateAction() FAILED "
             "due to SelectionForTSF() failure",
             this));
    return E_FAIL;
  }

  PendingAction* action = LastOrNewPendingCompositionUpdate();
  action->mData = mComposition->DataRef();
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
      LONG start = std::min(std::max(rangeStart, mComposition->StartOffset()),
                            mComposition->EndOffset());
      LONG end = std::max(
          std::min(rangeStart + rangeLength, mComposition->EndOffset()),
          mComposition->StartOffset());
      LONG length = end - start;
      if (length < 0) {
        MOZ_LOG(sTextStoreLog, LogLevel::Error,
                ("0x%p   TSFTextStore::RecordCompositionUpdateAction() "
                 "ignores invalid range (%d-%d)",
                 this, rangeStart - mComposition->StartOffset(),
                 rangeStart - mComposition->StartOffset() + rangeLength));
        continue;
      }
      if (!length) {
        MOZ_LOG(sTextStoreLog, LogLevel::Debug,
                ("0x%p   TSFTextStore::RecordCompositionUpdateAction() "
                 "ignores a range due to outside of the composition or empty "
                 "(%d-%d)",
                 this, rangeStart - mComposition->StartOffset(),
                 rangeStart - mComposition->StartOffset() + rangeLength));
        continue;
      }

      TextRange newRange;
      newRange.mStartOffset =
          static_cast<uint32_t>(start - mComposition->StartOffset());
      // The end of the last range in the array is
      // always kept at the end of composition
      newRange.mEndOffset = mComposition->Length();

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
    if (!selectionForTSF->Collapsed() && action->mRanges->Length() == 1) {
      TextRange& range = action->mRanges->ElementAt(0);
      LONG start = selectionForTSF->MinOffset();
      LONG end = selectionForTSF->MaxOffset();
      if (range.mStartOffset == start - mComposition->StartOffset() &&
          range.mEndOffset == end - mComposition->StartOffset() &&
          range.mRangeStyle.IsNoChangeStyle()) {
        range.mRangeStyle.Clear();
        // The looks of selected type is better than others.
        range.mRangeType = TextRangeType::eSelectedRawClause;
      }
    }

    // The caret position has to be collapsed.
    uint32_t caretPosition = static_cast<uint32_t>(
        selectionForTSF->MaxOffset() - mComposition->StartOffset());

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
           "succeeded",
           this));

  return S_OK;
}

HRESULT
TSFTextStore::SetSelectionInternal(const TS_SELECTION_ACP* pSelection,
                                   bool aDispatchCompositionChangeEvent) {
  MOZ_LOG(
      sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::SetSelectionInternal(pSelection=%s, "
       "aDispatchCompositionChangeEvent=%s), mComposition=%s",
       this, pSelection ? mozilla::ToString(*pSelection).c_str() : "nullptr",
       GetBoolName(aDispatchCompositionChangeEvent),
       ToString(mComposition).c_str()));

  MOZ_ASSERT(IsReadWriteLocked());

  Maybe<Selection>& selectionForTSF = SelectionForTSF();
  if (selectionForTSF.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::SetSelectionInternal() FAILED due to "
             "SelectionForTSF() failure",
             this));
    return E_FAIL;
  }

  MaybeDispatchKeyboardEventAsProcessedByIME();
  if (mDestroyed) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::SetSelectionInternal() FAILED due to "
             "destroyed during dispatching a keyboard event",
             this));
    return E_FAIL;
  }

  // If actually the range is not changing, we should do nothing.
  // Perhaps, we can ignore the difference change because it must not be
  // important for following edit.
  if (selectionForTSF->EqualsExceptDirection(*pSelection)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Warning,
            ("0x%p   TSFTextStore::SetSelectionInternal() Succeeded but "
             "did nothing because the selection range isn't changing",
             this));
    selectionForTSF->SetSelection(*pSelection);
    return S_OK;
  }

  if (mComposition.isSome()) {
    if (aDispatchCompositionChangeEvent) {
      HRESULT hr = RestartCompositionIfNecessary();
      if (FAILED(hr)) {
        MOZ_LOG(sTextStoreLog, LogLevel::Error,
                ("0x%p   TSFTextStore::SetSelectionInternal() FAILED due to "
                 "RestartCompositionIfNecessary() failure",
                 this));
        return hr;
      }
    }
    if (pSelection->acpStart < mComposition->StartOffset() ||
        pSelection->acpEnd > mComposition->EndOffset()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::SetSelectionInternal() FAILED due to "
               "the selection being out of the composition string",
               this));
      return TS_E_INVALIDPOS;
    }
    // Emulate selection during compositions
    selectionForTSF->SetSelection(*pSelection);
    if (aDispatchCompositionChangeEvent) {
      HRESULT hr = RecordCompositionUpdateAction();
      if (FAILED(hr)) {
        MOZ_LOG(sTextStoreLog, LogLevel::Error,
                ("0x%p   TSFTextStore::SetSelectionInternal() FAILED due to "
                 "RecordCompositionUpdateAction() failure",
                 this));
        return hr;
      }
    }
    return S_OK;
  }

  TS_SELECTION_ACP selectionInContent(*pSelection);

  // If mContentForTSF caches old contents which is now different from
  // actual contents, we need some complicated hack here...
  // Note that this hack assumes that this is used for reconversion.
  if (mContentForTSF.isSome() && mPendingTextChangeData.IsValid() &&
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
               "there is unknown content change",
               this));
      return E_FAIL;
    }
  }

  CompleteLastActionIfStillIncomplete();
  PendingAction* action = mPendingActions.AppendElement();
  action->mType = PendingAction::Type::eSetSelection;
  action->mSelectionStart = selectionInContent.acpStart;
  action->mSelectionLength =
      selectionInContent.acpEnd - selectionInContent.acpStart;
  action->mSelectionReversed = (selectionInContent.style.ase == TS_AE_START);

  // Use TSF specified selection for updating mSelectionForTSF.
  selectionForTSF->SetSelection(*pSelection);

  return S_OK;
}

STDMETHODIMP
TSFTextStore::SetSelection(ULONG ulCount, const TS_SELECTION_ACP* pSelection) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::SetSelection(ulCount=%lu, pSelection=%s }), "
           "mComposition=%s",
           this, ulCount,
           pSelection ? mozilla::ToString(pSelection).c_str() : "nullptr",
           ToString(mComposition).c_str()));

  if (!IsReadWriteLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::SetSelection() FAILED due to "
             "not locked (read-write)",
             this));
    return TS_E_NOLOCK;
  }
  if (ulCount != 1) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::SetSelection() FAILED due to "
             "trying setting multiple selection",
             this));
    return E_INVALIDARG;
  }
  if (!pSelection) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::SetSelection() FAILED due to "
             "null argument",
             this));
    return E_INVALIDARG;
  }

  HRESULT hr = SetSelectionInternal(pSelection, true);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::SetSelection() FAILED due to "
             "SetSelectionInternal() failure",
             this));
  } else {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
            ("0x%p   TSFTextStore::SetSelection() succeeded", this));
  }
  return hr;
}

STDMETHODIMP
TSFTextStore::GetText(LONG acpStart, LONG acpEnd, WCHAR* pchPlain,
                      ULONG cchPlainReq, ULONG* pcchPlainOut,
                      TS_RUNINFO* prgRunInfo, ULONG ulRunInfoReq,
                      ULONG* pulRunInfoOut, LONG* pacpNext) {
  MOZ_LOG(
      sTextStoreLog, LogLevel::Info,
      ("0x%p TSFTextStore::GetText(acpStart=%ld, acpEnd=%ld, pchPlain=0x%p, "
       "cchPlainReq=%lu, pcchPlainOut=0x%p, prgRunInfo=0x%p, ulRunInfoReq=%lu, "
       "pulRunInfoOut=0x%p, pacpNext=0x%p), mComposition=%s",
       this, acpStart, acpEnd, pchPlain, cchPlainReq, pcchPlainOut, prgRunInfo,
       ulRunInfoReq, pulRunInfoOut, pacpNext, ToString(mComposition).c_str()));

  if (!IsReadLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetText() FAILED due to "
             "not locked (read)",
             this));
    return TS_E_NOLOCK;
  }

  if (!pcchPlainOut || (!pchPlain && !prgRunInfo) ||
      !cchPlainReq != !pchPlain || !ulRunInfoReq != !prgRunInfo) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetText() FAILED due to "
             "invalid argument",
             this));
    return E_INVALIDARG;
  }

  if (acpStart < 0 || acpEnd < -1 || (acpEnd != -1 && acpStart > acpEnd)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetText() FAILED due to "
             "invalid position",
             this));
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

  Maybe<Content>& contentForTSF = ContentForTSF();
  if (contentForTSF.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetText() FAILED due to "
             "ContentForTSF() failure",
             this));
    return E_FAIL;
  }
  if (contentForTSF->TextRef().Length() < static_cast<uint32_t>(acpStart)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetText() FAILED due to "
             "acpStart is larger offset than the actual text length",
             this));
    return TS_E_INVALIDPOS;
  }
  if (acpEnd != -1 &&
      contentForTSF->TextRef().Length() < static_cast<uint32_t>(acpEnd)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetText() FAILED due to "
             "acpEnd is larger offset than the actual text length",
             this));
    return TS_E_INVALIDPOS;
  }
  uint32_t length = (acpEnd == -1) ? contentForTSF->TextRef().Length() -
                                         static_cast<uint32_t>(acpStart)
                                   : static_cast<uint32_t>(acpEnd - acpStart);
  if (cchPlainReq && cchPlainReq - 1 < length) {
    length = cchPlainReq - 1;
  }
  if (length) {
    if (pchPlain && cchPlainReq) {
      const char16_t* startChar =
          contentForTSF->TextRef().BeginReading() + acpStart;
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
TSFTextStore::SetText(DWORD dwFlags, LONG acpStart, LONG acpEnd,
                      const WCHAR* pchText, ULONG cch, TS_TEXTCHANGE* pChange) {
  MOZ_LOG(
      sTextStoreLog, LogLevel::Info,
      ("0x%p TSFTextStore::SetText(dwFlags=%s, acpStart=%ld, acpEnd=%ld, "
       "pchText=0x%p \"%s\", cch=%lu, pChange=0x%p), mComposition=%s",
       this, dwFlags == TS_ST_CORRECTION ? "TS_ST_CORRECTION" : "not-specified",
       acpStart, acpEnd, pchText,
       pchText && cch ? GetEscapedUTF8String(pchText, cch).get() : "", cch,
       pChange, ToString(mComposition).c_str()));

  // Per SDK documentation, and since we don't have better
  // ways to do this, this method acts as a helper to
  // call SetSelection followed by InsertTextAtSelection
  if (!IsReadWriteLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::SetText() FAILED due to "
             "not locked (read)",
             this));
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
             "SetSelectionInternal() failure",
             this));
    return hr;
  }
  // Replace just selected text
  if (!InsertTextAtSelectionInternal(nsDependentSubstring(pchText, cch),
                                     pChange)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::SetText() FAILED due to "
             "InsertTextAtSelectionInternal() failure",
             this));
    return E_FAIL;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::SetText() succeeded: pChange={ "
           "acpStart=%ld, acpOldEnd=%ld, acpNewEnd=%ld }",
           this, pChange ? pChange->acpStart : 0,
           pChange ? pChange->acpOldEnd : 0, pChange ? pChange->acpNewEnd : 0));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::GetFormattedText(LONG acpStart, LONG acpEnd,
                               IDataObject** ppDataObject) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::GetFormattedText() called "
           "but not supported (E_NOTIMPL)",
           this));

  // no support for formatted text
  return E_NOTIMPL;
}

STDMETHODIMP
TSFTextStore::GetEmbedded(LONG acpPos, REFGUID rguidService, REFIID riid,
                          IUnknown** ppunk) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::GetEmbedded() called "
           "but not supported (E_NOTIMPL)",
           this));

  // embedded objects are not supported
  return E_NOTIMPL;
}

STDMETHODIMP
TSFTextStore::QueryInsertEmbedded(const GUID* pguidService,
                                  const FORMATETC* pFormatEtc,
                                  BOOL* pfInsertable) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::QueryInsertEmbedded() called "
           "but not supported, *pfInsertable=FALSE (S_OK)",
           this));

  // embedded objects are not supported
  *pfInsertable = FALSE;
  return S_OK;
}

STDMETHODIMP
TSFTextStore::InsertEmbedded(DWORD dwFlags, LONG acpStart, LONG acpEnd,
                             IDataObject* pDataObject, TS_TEXTCHANGE* pChange) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::InsertEmbedded() called "
           "but not supported (E_NOTIMPL)",
           this));

  // embedded objects are not supported
  return E_NOTIMPL;
}

// static
bool TSFTextStore::ShouldSetInputScopeOfURLBarToDefault() {
  // FYI: Google Japanese Input may be an IMM-IME.  If it's installed on
  //      Win7, it's always IMM-IME.  Otherwise, basically, it's a TIP.
  //      However, if it's installed on Win7 and has not been updated yet
  //      after the OS is upgraded to Win8 or later, it's still an IMM-IME.
  //      Therefore, we also need to check with IMMHandler here.
  if (!TSFPrefs::ShouldSetInputScopeOfURLBarToDefault()) {
    return false;
  }

  if (IMMHandler::IsGoogleJapaneseInputActive()) {
    return true;
  }

  switch (TSFStaticSink::ActiveTIP()) {
    case TextInputProcessorID::eMicrosoftIMEForJapanese:
    case TextInputProcessorID::eGoogleJapaneseInput:
    case TextInputProcessorID::eMicrosoftBopomofo:
    case TextInputProcessorID::eMicrosoftChangJie:
    case TextInputProcessorID::eMicrosoftPhonetic:
    case TextInputProcessorID::eMicrosoftQuick:
    case TextInputProcessorID::eMicrosoftNewChangJie:
    case TextInputProcessorID::eMicrosoftNewPhonetic:
    case TextInputProcessorID::eMicrosoftNewQuick:
    case TextInputProcessorID::eMicrosoftPinyin:
    case TextInputProcessorID::eMicrosoftPinyinNewExperienceInputStyle:
    case TextInputProcessorID::eMicrosoftOldHangul:
    case TextInputProcessorID::eMicrosoftWubi:
      return true;
    case TextInputProcessorID::eMicrosoftIMEForKorean:
      return IsWin8OrLater();
    default:
      return false;
  }
}

void TSFTextStore::SetInputScope(const nsString& aHTMLInputType,
                                 const nsString& aHTMLInputInputMode,
                                 bool aInPrivateBrowsing) {
  mInputScopes.Clear();

  // IME may refer only first input scope, but we will append inputmode's
  // input scopes too like Chrome since IME may refer it.
  IMEHandler::AppendInputScopeFromType(aHTMLInputType, mInputScopes);
  IMEHandler::AppendInputScopeFromInputmode(aHTMLInputInputMode, mInputScopes);

  if (aInPrivateBrowsing) {
    mInputScopes.AppendElement(IS_PRIVATE);
  }
}

int32_t TSFTextStore::GetRequestedAttrIndex(const TS_ATTRID& aAttrID) {
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
TSFTextStore::GetAttrID(int32_t aIndex) {
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
TSFTextStore::HandleRequestAttrs(DWORD aFlags, ULONG aFilterCount,
                                 const TS_ATTRID* aFilterAttrs) {
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
TSFTextStore::RequestSupportedAttrs(DWORD dwFlags, ULONG cFilterAttrs,
                                    const TS_ATTRID* paFilterAttrs) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::RequestSupportedAttrs(dwFlags=%s, "
           "cFilterAttrs=%lu)",
           this, GetFindFlagName(dwFlags).get(), cFilterAttrs));

  return HandleRequestAttrs(dwFlags, cFilterAttrs, paFilterAttrs);
}

STDMETHODIMP
TSFTextStore::RequestAttrsAtPosition(LONG acpPos, ULONG cFilterAttrs,
                                     const TS_ATTRID* paFilterAttrs,
                                     DWORD dwFlags) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::RequestAttrsAtPosition(acpPos=%ld, "
           "cFilterAttrs=%lu, dwFlags=%s)",
           this, acpPos, cFilterAttrs, GetFindFlagName(dwFlags).get()));

  return HandleRequestAttrs(dwFlags | TS_ATTR_FIND_WANT_VALUE, cFilterAttrs,
                            paFilterAttrs);
}

STDMETHODIMP
TSFTextStore::RequestAttrsTransitioningAtPosition(LONG acpPos,
                                                  ULONG cFilterAttrs,
                                                  const TS_ATTRID* paFilterAttr,
                                                  DWORD dwFlags) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::RequestAttrsTransitioningAtPosition("
           "acpPos=%ld, cFilterAttrs=%lu, dwFlags=%s) called but not supported "
           "(S_OK)",
           this, acpPos, cFilterAttrs, GetFindFlagName(dwFlags).get()));

  // no per character attributes defined
  return S_OK;
}

STDMETHODIMP
TSFTextStore::FindNextAttrTransition(LONG acpStart, LONG acpHalt,
                                     ULONG cFilterAttrs,
                                     const TS_ATTRID* paFilterAttrs,
                                     DWORD dwFlags, LONG* pacpNext,
                                     BOOL* pfFound, LONG* plFoundOffset) {
  if (!pacpNext || !pfFound || !plFoundOffset) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  0x%p TSFTextStore::FindNextAttrTransition() FAILED due to "
             "null argument",
             this));
    return E_INVALIDARG;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::FindNextAttrTransition() called "
           "but not supported (S_OK)",
           this));

  // no per character attributes defined
  *pacpNext = *plFoundOffset = acpHalt;
  *pfFound = FALSE;
  return S_OK;
}

STDMETHODIMP
TSFTextStore::RetrieveRequestedAttrs(ULONG ulCount, TS_ATTRVAL* paAttrVals,
                                     ULONG* pcFetched) {
  if (!pcFetched || !paAttrVals) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p TSFTextStore::RetrieveRequestedAttrs() FAILED due to "
             "null argument",
             this));
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
            ("0x%p   TSFTextStore::RetrieveRequestedAttrs() for %s", this,
             GetGUIDNameStrWithTable(attrID).get()));

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
          Maybe<Selection>& selectionForTSF = SelectionForTSF();
          paAttrVals[count].varValue.vt = VT_BOOL;
          paAttrVals[count].varValue.boolVal =
              selectionForTSF.isSome() &&
                      selectionForTSF->GetWritingMode().IsVertical()
                  ? VARIANT_TRUE
                  : VARIANT_FALSE;
          break;
        }
        case eTextOrientation: {
          Maybe<Selection>& selectionForTSF = SelectionForTSF();
          paAttrVals[count].varValue.vt = VT_I4;
          paAttrVals[count].varValue.lVal =
              selectionForTSF.isSome() &&
                      selectionForTSF->GetWritingMode().IsVertical()
                  ? 2700
                  : 0;
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
           "for unknown TS_ATTRVAL, *pcFetched=0 (S_OK)",
           this));

  paAttrVals->dwOverlapId = 0;
  paAttrVals->varValue.vt = VT_EMPTY;
  *pcFetched = 0;
  return S_OK;
}

STDMETHODIMP
TSFTextStore::GetEndACP(LONG* pacp) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::GetEndACP(pacp=0x%p)", this, pacp));

  if (!IsReadLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetEndACP() FAILED due to "
             "not locked (read)",
             this));
    return TS_E_NOLOCK;
  }

  if (!pacp) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetEndACP() FAILED due to "
             "null argument",
             this));
    return E_INVALIDARG;
  }

  Maybe<Content>& contentForTSF = ContentForTSF();
  if (contentForTSF.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetEndACP() FAILED due to "
             "ContentForTSF() failure",
             this));
    return E_FAIL;
  }
  *pacp = static_cast<LONG>(contentForTSF->TextRef().Length());
  return S_OK;
}

STDMETHODIMP
TSFTextStore::GetActiveView(TsViewCookie* pvcView) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::GetActiveView(pvcView=0x%p)", this, pvcView));

  if (!pvcView) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetActiveView() FAILED due to "
             "null argument",
             this));
    return E_INVALIDARG;
  }

  *pvcView = TEXTSTORE_DEFAULT_VIEW;

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::GetActiveView() succeeded: *pvcView=%ld", this,
           *pvcView));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::GetACPFromPoint(TsViewCookie vcView, const POINT* pt,
                              DWORD dwFlags, LONG* pacp) {
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
             "not locked (read)",
             this));
    return TS_E_NOLOCK;
  }

  if (vcView != TEXTSTORE_DEFAULT_VIEW) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to "
             "called with invalid view",
             this));
    return E_INVALIDARG;
  }

  if (!pt) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to "
             "null pt",
             this));
    return E_INVALIDARG;
  }

  if (!pacp) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to "
             "null pacp",
             this));
    return E_INVALIDARG;
  }

  mWaitingQueryLayout = false;

  if (mDestroyed ||
      (mContentForTSF.isSome() && mContentForTSF->IsLayoutChanged())) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetACPFromPoint() returned "
             "TS_E_NOLAYOUT",
             this));
    mHasReturnedNoLayoutError = true;
    return TS_E_NOLAYOUT;
  }

  LayoutDeviceIntPoint ourPt(pt->x, pt->y);
  // Convert to widget relative coordinates from screen's.
  ourPt -= mWidget->WidgetToScreenOffset();

  // NOTE: Don't check if the point is in the widget since the point can be
  //       outside of the widget if focused editor is in a XUL <panel>.

  WidgetQueryContentEvent queryCharAtPointEvent(true, eQueryCharacterAtPoint,
                                                mWidget);
  mWidget->InitEvent(queryCharAtPointEvent, &ourPt);

  // FYI: WidgetQueryContentEvent may cause flushing pending layout and it
  //      may cause focus change or something.
  RefPtr<TSFTextStore> kungFuDeathGrip(this);
  DispatchEvent(queryCharAtPointEvent);
  if (!mWidget || mWidget->Destroyed()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to "
             "mWidget was destroyed during eQueryCharacterAtPoint",
             this));
    return E_FAIL;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::GetACPFromPoint(), queryCharAtPointEvent={ "
           "mReply=%s }",
           this, ToString(queryCharAtPointEvent.mReply).c_str()));

  if (NS_WARN_IF(queryCharAtPointEvent.Failed())) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to "
             "eQueryCharacterAtPoint failure",
             this));
    return E_FAIL;
  }

  // If dwFlags isn't set and the point isn't in any character's bounding box,
  // we should return TS_E_INVALIDPOINT.
  if (!(dwFlags & GXFPF_NEAREST) && queryCharAtPointEvent.DidNotFindChar()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to the "
             "point contained by no bounding box",
             this));
    return TS_E_INVALIDPOINT;
  }

  // Although, we're not sure if mTentativeCaretOffset becomes NOT_FOUND,
  // let's assume that there is no content in such case.
  NS_WARNING_ASSERTION(queryCharAtPointEvent.DidNotFindTentativeCaretOffset(),
                       "Tentative caret offset was not found");

  uint32_t offset;

  // If dwFlags includes GXFPF_ROUND_NEAREST, we should return tentative
  // caret offset (MSDN calls it "range position").
  if (dwFlags & GXFPF_ROUND_NEAREST) {
    offset = queryCharAtPointEvent.mReply->mTentativeCaretOffset.valueOr(0);
  } else if (queryCharAtPointEvent.FoundChar()) {
    // Otherwise, we should return character offset whose bounding box contains
    // the point.
    offset = queryCharAtPointEvent.mReply->StartOffset();
  } else {
    // If the point isn't in any character's bounding box but we need to return
    // the nearest character from the point, we should *guess* the character
    // offset since there is no inexpensive API to check it strictly.
    // XXX If we retrieve 2 bounding boxes, one is before the offset and
    //     the other is after the offset, we could resolve the offset.
    //     However, dispatching 2 eQueryTextRect may be expensive.

    // So, use tentative offset for now.
    offset = queryCharAtPointEvent.mReply->mTentativeCaretOffset.valueOr(0);

    // However, if it's after the last character, we need to decrement the
    // offset.
    Maybe<Content>& contentForTSF = ContentForTSF();
    if (contentForTSF.isNothing()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to "
               "ContentForTSF() failure",
               this));
      return E_FAIL;
    }
    if (contentForTSF->TextRef().Length() <= offset) {
      // If the tentative caret is after the last character, let's return
      // the last character's offset.
      offset = contentForTSF->TextRef().Length() - 1;
    }
  }

  if (NS_WARN_IF(offset > LONG_MAX)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetACPFromPoint() FAILED due to out of "
             "range of the result",
             this));
    return TS_E_INVALIDPOINT;
  }

  *pacp = static_cast<LONG>(offset);
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::GetACPFromPoint() succeeded: *pacp=%d", this,
           *pacp));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::GetTextExt(TsViewCookie vcView, LONG acpStart, LONG acpEnd,
                         RECT* prc, BOOL* pfClipped) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::GetTextExt(vcView=%ld, "
           "acpStart=%ld, acpEnd=%ld, prc=0x%p, pfClipped=0x%p), "
           "IsHandlingCompositionInParent()=%s, "
           "IsHandlingCompositionInContent()=%s, mContentForTSF=%s, "
           "mSelectionForTSF=%s, mComposition=%s, mDeferNotifyingTSF=%s, "
           "mWaitingQueryLayout=%s, IMEHandler::IsA11yHandlingNativeCaret()=%s",
           this, vcView, acpStart, acpEnd, prc, pfClipped,
           GetBoolName(IsHandlingCompositionInParent()),
           GetBoolName(IsHandlingCompositionInContent()),
           mozilla::ToString(mContentForTSF).c_str(),
           ToString(mSelectionForTSF).c_str(), ToString(mComposition).c_str(),
           GetBoolName(mDeferNotifyingTSF), GetBoolName(mWaitingQueryLayout),
           GetBoolName(IMEHandler::IsA11yHandlingNativeCaret())));

  if (!IsReadLocked()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
             "not locked (read)",
             this));
    return TS_E_NOLOCK;
  }

  if (vcView != TEXTSTORE_DEFAULT_VIEW) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
             "called with invalid view",
             this));
    return E_INVALIDARG;
  }

  if (!prc || !pfClipped) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
             "null argument",
             this));
    return E_INVALIDARG;
  }

  // According to MSDN, ITextStoreACP::GetTextExt() should return
  // TS_E_INVALIDARG when acpStart and acpEnd are same (i.e., collapsed range).
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms538435(v=vs.85).aspx
  // > TS_E_INVALIDARG: The specified start and end character positions are
  // >                  equal.
  // However, some TIPs (including Microsoft's Chinese TIPs!) call this with
  // collapsed range and if we return TS_E_INVALIDARG, they stops showing their
  // owning window or shows it but odd position.  So, we should just return
  // error only when acpStart and/or acpEnd are really odd.

  if (acpStart < 0 || acpEnd < acpStart) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
             "invalid position",
             this));
    return TS_E_INVALIDPOS;
  }

  mWaitingQueryLayout = false;

  if (IsHandlingCompositionInContent() && mContentForTSF.isSome() &&
      mContentForTSF->HasOrHadComposition() &&
      mContentForTSF->IsLayoutChanged() &&
      mContentForTSF->MinModifiedOffset().value() >
          static_cast<uint32_t>(LONG_MAX)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetTextExt(), FAILED due to the text "
             "is too big for TSF (cannot treat modified offset as LONG), "
             "mContentForTSF=%s",
             this, ToString(mContentForTSF).c_str()));
    return E_FAIL;
  }

  // At Windows 10 build 17643 (an insider preview for RS5), Microsoft fixed
  // the bug of TS_E_NOLAYOUT (even when we returned TS_E_NOLAYOUT, TSF
  // returned E_FAIL to TIP).  However, until we drop to support older Windows
  // and all TIPs are aware of TS_E_NOLAYOUT result, we need to keep returning
  // S_OK and available rectangle only for them.
  if (!MaybeHackNoErrorLayoutBugs(acpStart, acpEnd) &&
      mContentForTSF.isSome() && mContentForTSF->IsLayoutChangedAt(acpEnd)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetTextExt() returned TS_E_NOLAYOUT "
             "(acpEnd=%d)",
             this, acpEnd));
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
  WidgetQueryContentEvent queryTextRectEvent(true, eQueryTextRect, mWidget);
  mWidget->InitEvent(queryTextRectEvent);

  WidgetQueryContentEvent::Options options;
  int64_t startOffset = acpStart;
  if (mComposition.isSome()) {
    // If there is a composition, TSF must want character rects related to
    // the composition.  Therefore, we should use insertion point relative
    // query because the composition might be at different position from
    // the position where TSFTextStore believes it at.
    options.mRelativeToInsertionPoint = true;
    startOffset -= mComposition->StartOffset();
  } else if (IsHandlingCompositionInParent() && mContentForTSF.isSome() &&
             mContentForTSF->HasOrHadComposition()) {
    // If there was a composition and its commit event hasn't been dispatched
    // yet, ContentCacheInParent is still open for relative offset query from
    // the latest composition.
    options.mRelativeToInsertionPoint = true;
    startOffset -= mContentForTSF->LatestCompositionRange()->StartOffset();
  } else if (!CanAccessActualContentDirectly()) {
    // If TSF/TIP cannot access actual content directly, there may be pending
    // text and/or selection changes which have not been notified TSF yet.
    // Therefore, we should use relative to insertion point query since
    // TSF/TIP computes the offset from the cached selection.
    options.mRelativeToInsertionPoint = true;
    startOffset -= mSelectionForTSF->StartOffset();
  }
  // ContentEventHandler and ContentCache return actual caret rect when
  // the queried range is collapsed and selection is collapsed at the
  // queried range.  Then, its height (in horizontal layout, width in vertical
  // layout) may be different from actual font height of the line.  In such
  // case, users see "dancing" of candidate or suggest window of TIP.
  // For preventing it, we should query text rect with at least 1 length.
  uint32_t length = std::max(static_cast<int32_t>(acpEnd - acpStart), 1);
  queryTextRectEvent.InitForQueryTextRect(startOffset, length, options);

  DispatchEvent(queryTextRectEvent);
  if (NS_WARN_IF(queryTextRectEvent.Failed())) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
             "eQueryTextRect failure",
             this));
    return TS_E_INVALIDPOS;  // but unexpected failure, maybe.
  }

  // IMEs don't like empty rects, fix here
  if (queryTextRectEvent.mReply->mRect.Width() <= 0) {
    queryTextRectEvent.mReply->mRect.SetWidth(1);
  }
  if (queryTextRectEvent.mReply->mRect.Height() <= 0) {
    queryTextRectEvent.mReply->mRect.SetHeight(1);
  }

  // convert to unclipped screen rect
  nsWindow* refWindow =
      static_cast<nsWindow*>(!!queryTextRectEvent.mReply->mFocusedWidget
                                 ? queryTextRectEvent.mReply->mFocusedWidget
                                 : static_cast<nsIWidget*>(mWidget.get()));
  // Result rect is in top level widget coordinates
  refWindow = refWindow->GetTopLevelWindow(false);
  if (!refWindow) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
             "no top level window",
             this));
    return E_FAIL;
  }

  queryTextRectEvent.mReply->mRect.MoveBy(refWindow->WidgetToScreenOffset());

  // get bounding screen rect to test for clipping
  if (!GetScreenExtInternal(*prc)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetTextExt() FAILED due to "
             "GetScreenExtInternal() failure",
             this));
    return E_FAIL;
  }

  // clip text rect to bounding rect
  RECT textRect;
  ::SetRect(&textRect, queryTextRectEvent.mReply->mRect.X(),
            queryTextRectEvent.mReply->mRect.Y(),
            queryTextRectEvent.mReply->mRect.XMost(),
            queryTextRectEvent.mReply->mRect.YMost());
  if (!::IntersectRect(prc, prc, &textRect))
    // Text is not visible
    ::SetRectEmpty(prc);

  // not equal if text rect was clipped
  *pfClipped = !::EqualRect(prc, &textRect);

  // ATOK 2011 - 2016 refers native caret position and size on windows whose
  // class name is one of Mozilla's windows for deciding candidate window
  // position.  Additionally, ATOK 2015 and earlier behaves really odd when
  // we don't create native caret.  Therefore, we need to create native caret
  // only when ATOK 2011 - 2015 is active (i.e., not necessary for ATOK 2016).
  // However, if a11y module is handling native caret, we shouldn't touch it.
  // Note that ATOK must require the latest information of the caret.  So,
  // even if we'll create native caret later, we need to creat it here with
  // current information.
  if (!IMEHandler::IsA11yHandlingNativeCaret() &&
      TSFPrefs::NeedToCreateNativeCaretForLegacyATOK() &&
      TSFStaticSink::IsATOKReferringNativeCaretActive() &&
      mComposition.isSome() &&
      mComposition->IsOffsetInRangeOrEndOffset(acpStart) &&
      mComposition->IsOffsetInRangeOrEndOffset(acpEnd)) {
    CreateNativeCaret();
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::GetTextExt() succeeded: "
           "*prc={ left=%ld, top=%ld, right=%ld, bottom=%ld }, *pfClipped=%s",
           this, prc->left, prc->top, prc->right, prc->bottom,
           GetBoolName(*pfClipped)));

  return S_OK;
}

bool TSFTextStore::MaybeHackNoErrorLayoutBugs(LONG& aACPStart, LONG& aACPEnd) {
  // When ITextStoreACP::GetTextExt() returns TS_E_NOLAYOUT, TSF returns E_FAIL
  // to its caller (typically, active TIP).  Then, most TIPs abort current job
  // or treat such application as non-GUI apps.  E.g., some of them give up
  // showing candidate window, some others show candidate window at top-left of
  // the screen.  For avoiding this issue, when there is composition (until
  // composition is actually committed in remote content), we should not
  // return TS_E_NOLAYOUT error for TIPs whose some features are broken by
  // this issue.
  // Note that ideally, this issue should be avoided by each TIP since this
  // won't be fixed at least on non-latest Windows.  Actually, Google Japanese
  // Input (based on Mozc) does it.  When GetTextExt() returns E_FAIL, TIPs
  // should try to check result of GetRangeFromPoint() because TSF returns
  // TS_E_NOLAYOUT correctly in this case. See:
  // https://github.com/google/mozc/blob/6b878e31fb6ac4347dc9dfd8ccc1080fe718479f/src/win32/tip/tip_range_util.cc#L237-L257

  if (!IsHandlingCompositionInContent() || mContentForTSF.isNothing() ||
      !mContentForTSF->HasOrHadComposition() ||
      !mContentForTSF->IsLayoutChangedAt(aACPEnd)) {
    return false;
  }

  MOZ_ASSERT(mComposition.isNothing() ||
             mComposition->StartOffset() ==
                 mContentForTSF->LatestCompositionRange()->StartOffset());
  MOZ_ASSERT(mComposition.isNothing() ||
             mComposition->EndOffset() ==
                 mContentForTSF->LatestCompositionRange()->EndOffset());

  // If TSF does not have the bug, we need to hack only with a few TIPs.
  static const bool sAlllowToStopHackingIfFine =
      IsWindows10BuildOrLater(17643) &&
      TSFPrefs::AllowToStopHackingOnBuild17643OrLater();

  // We need to compute active TIP now.  This may take a couple of milliseconds,
  // however, it'll be cached, so, must be faster than check active TIP every
  // GetTextExt() calls.
  const Maybe<Selection>& selectionForTSF = SelectionForTSF();
  switch (TSFStaticSink::ActiveTIP()) {
    // MS IME for Japanese doesn't support asynchronous handling at deciding
    // its suggest list window position.  The feature was implemented
    // starting from Windows 8.  And also we may meet same trouble in e10s
    // mode on Win7.  So, we should never return TS_E_NOLAYOUT to MS IME for
    // Japanese.
    case TextInputProcessorID::eMicrosoftIMEForJapanese:
      // Basically, MS-IME tries to retrieve whole composition string rect
      // at deciding suggest window immediately after unlocking the document.
      // However, in e10s mode, the content hasn't updated yet in most cases.
      // Therefore, if the first character at the retrieving range rect is
      // available, we should use it as the result.
      // Note that according to bug 1609675, MS-IME for Japanese itself does
      // not handle TS_E_NOLAYOUT correctly at least on Build 18363.657 (1909).
      if (TSFPrefs::DoNotReturnNoLayoutErrorToMSJapaneseIMEAtFirstChar() &&
          aACPStart < aACPEnd) {
        aACPEnd = aACPStart;
        break;
      }
      if (sAlllowToStopHackingIfFine) {
        return false;
      }
      // Although, the condition is not clear, MS-IME sometimes retrieves the
      // caret rect immediately after modifying the composition string but
      // before unlocking the document.  In such case, we should return the
      // nearest character rect.
      if (TSFPrefs::DoNotReturnNoLayoutErrorToMSJapaneseIMEAtCaret() &&
          selectionForTSF.isSome() && aACPStart == aACPEnd &&
          selectionForTSF->Collapsed() &&
          selectionForTSF->EndOffset() == aACPEnd) {
        int32_t minOffsetOfLayoutChanged =
            static_cast<int32_t>(mContentForTSF->MinModifiedOffset().value());
        aACPEnd = aACPStart = std::max(minOffsetOfLayoutChanged - 1, 0);
      } else {
        return false;
      }
      break;
    // The bug of Microsoft Office IME 2010 for Japanese is similar to
    // MS-IME for Win 8.1 and Win 10.  Newer version of MS Office IME is not
    // released yet.  So, we can hack it without prefs  because there must be
    // no developers who want to disable this hack for tests.
    // XXX We have not tested with Microsoft Office IME 2010 since it's
    //     installable only with Win7 and Win8 (i.e., cannot install Win8.1
    //     and Win10), and requires upgrade to Win10.
    case TextInputProcessorID::eMicrosoftOfficeIME2010ForJapanese:
      // Basically, MS-IME tries to retrieve whole composition string rect
      // at deciding suggest window immediately after unlocking the document.
      // However, in e10s mode, the content hasn't updated yet in most cases.
      // Therefore, if the first character at the retrieving range rect is
      // available, we should use it as the result.
      if (aACPStart < aACPEnd) {
        aACPEnd = aACPStart;
      }
      // Although, the condition is not clear, MS-IME sometimes retrieves the
      // caret rect immediately after modifying the composition string but
      // before unlocking the document.  In such case, we should return the
      // nearest character rect.
      else if (aACPStart == aACPEnd && selectionForTSF.isSome() &&
               selectionForTSF->Collapsed() &&
               selectionForTSF->EndOffset() == aACPEnd) {
        int32_t minOffsetOfLayoutChanged =
            static_cast<int32_t>(mContentForTSF->MinModifiedOffset().value());
        aACPEnd = aACPStart = std::max(minOffsetOfLayoutChanged - 1, 0);
      } else {
        return false;
      }
      break;
    // ATOK fails to handle TS_E_NOLAYOUT only when it decides the position of
    // suggest window.  In such case, ATOK tries to query rect of whole or a
    // part of composition string.
    // FYI: ATOK changes their implementation around candidate window and
    //      suggest widget at ATOK 2016.  Therefore, there are some differences
    //      ATOK 2015 (or older) and ATOK 2016 (or newer).
    // FYI: ATOK 2017 stops referring our window class name.  I.e., ATOK 2016
    //      and older may behave differently only on Gecko but this must be
    //      finished from ATOK 2017.
    // FYI: For testing with legacy ATOK, we should hack it even if current ATOK
    //      refers native caret rect on windows whose window class is one of
    //      Mozilla window classes and we stop creating native caret for ATOK
    //      because creating native caret causes ATOK refers caret position
    //      when GetTextExt() returns TS_E_NOLAYOUT.
    case TextInputProcessorID::eATOK2011:
    case TextInputProcessorID::eATOK2012:
    case TextInputProcessorID::eATOK2013:
    case TextInputProcessorID::eATOK2014:
    case TextInputProcessorID::eATOK2015:
      // ATOK 2016 and later may temporarily show candidate window at odd
      // position when you convert a word quickly (e.g., keep pressing
      // space bar).  So, on ATOK 2016 or later, we need to keep hacking the
      // result of GetTextExt().
      if (sAlllowToStopHackingIfFine) {
        return false;
      }
      // If we'll create native caret where we paint our caret.  Then, ATOK
      // will refer native caret.  So, we don't need to hack anything in
      // this case.
      if (TSFPrefs::NeedToCreateNativeCaretForLegacyATOK()) {
        MOZ_ASSERT(TSFStaticSink::IsATOKReferringNativeCaretActive());
        return false;
      }
      [[fallthrough]];
    case TextInputProcessorID::eATOK2016:
    case TextInputProcessorID::eATOKUnknown:
      if (!TSFPrefs::DoNotReturnNoLayoutErrorToATOKOfCompositionString()) {
        return false;
      }
      // If the range is in the composition string, we should return rectangle
      // in it as far as possible.
      if (!mContentForTSF->LatestCompositionRange()->IsOffsetInRangeOrEndOffset(
              aACPStart) ||
          !mContentForTSF->LatestCompositionRange()->IsOffsetInRangeOrEndOffset(
              aACPEnd)) {
        return false;
      }
      break;
    // Japanist 10 fails to handle TS_E_NOLAYOUT when it decides the position
    // of candidate window.  In such case, Japanist shows candidate window at
    // top-left of the screen.  So, we should return the nearest caret rect
    // where we know.  This is Japanist's bug.  So, even after build 17643,
    // we need this hack.
    case TextInputProcessorID::eJapanist10:
      if (!TSFPrefs::
              DoNotReturnNoLayoutErrorToJapanist10OfCompositionString()) {
        return false;
      }
      if (!mContentForTSF->LatestCompositionRange()->IsOffsetInRangeOrEndOffset(
              aACPStart) ||
          !mContentForTSF->LatestCompositionRange()->IsOffsetInRangeOrEndOffset(
              aACPEnd)) {
        return false;
      }
      break;
    // Free ChangJie 2010 doesn't handle ITfContextView::GetTextExt() properly.
    // This must be caused by the bug of TSF since Free ChangJie works fine on
    // build 17643 and later.
    case TextInputProcessorID::eFreeChangJie:
      if (sAlllowToStopHackingIfFine) {
        return false;
      }
      if (!TSFPrefs::DoNotReturnNoLayoutErrorToFreeChangJie()) {
        return false;
      }
      aACPEnd = mContentForTSF->LatestCompositionRange()->StartOffset();
      aACPStart = std::min(aACPStart, aACPEnd);
      break;
    // Some Traditional Chinese TIPs of Microsoft don't show candidate window
    // in e10s mode on Win8 or later.
    case TextInputProcessorID::eMicrosoftQuick:
      if (sAlllowToStopHackingIfFine) {
        return false;  // MS Quick works fine with Win10 build 17643.
      }
      [[fallthrough]];
    case TextInputProcessorID::eMicrosoftChangJie:
      if (!IsWin8OrLater() ||
          !TSFPrefs::DoNotReturnNoLayoutErrorToMSTraditionalTIP()) {
        return false;
      }
      aACPEnd = mContentForTSF->LatestCompositionRange()->StartOffset();
      aACPStart = std::min(aACPStart, aACPEnd);
      break;
    // Some Simplified Chinese TIPs of Microsoft don't show candidate window
    // in e10s mode on Win8 or later.
    // FYI: Only Simplified Chinese TIPs of Microsoft still require this hack
    //      because they sometimes do not show candidate window when we return
    //      TS_E_NOLAYOUT for first query.  Note that even when they show
    //      candidate window properly, we return TS_E_NOLAYOUT and following
    //      log looks same as when they don't show candidate window.  Perhaps,
    //      there is stateful cause or race in them.
    case TextInputProcessorID::eMicrosoftPinyin:
    case TextInputProcessorID::eMicrosoftWubi:
      if (!IsWin8OrLater() ||
          !TSFPrefs::DoNotReturnNoLayoutErrorToMSSimplifiedTIP()) {
        return false;
      }
      aACPEnd = mContentForTSF->LatestCompositionRange()->StartOffset();
      aACPStart = std::min(aACPStart, aACPEnd);
      break;
    default:
      return false;
  }

  // If we hack the queried range for active TIP, that means we should not
  // return TS_E_NOLAYOUT even if hacked offset is still modified.  So, as
  // far as possible, we should adjust the offset.
  MOZ_ASSERT(mContentForTSF->IsLayoutChanged());
  bool collapsed = aACPStart == aACPEnd;
  // Note that even if all characters in the editor or the composition
  // string was modified, 0 or start offset of the composition string is
  // useful because it may return caret rect or old character's rect which
  // the user still see.  That must be useful information for TIP.
  int32_t firstModifiedOffset =
      static_cast<int32_t>(mContentForTSF->MinModifiedOffset().value());
  LONG lastUnmodifiedOffset = std::max(firstModifiedOffset - 1, 0);
  if (mContentForTSF->IsLayoutChangedAt(aACPStart)) {
    if (aACPStart >= mContentForTSF->LatestCompositionRange()->StartOffset()) {
      // If mContentForTSF has last composition string and current
      // composition string, we can assume that ContentCacheInParent has
      // cached rects of composition string at least length of current
      // composition string.  Otherwise, we can assume that rect for
      // first character of composition string is stored since it was
      // selection start or caret position.
      LONG maxCachedOffset =
          mContentForTSF->LatestCompositionRange()->EndOffset();
      if (mContentForTSF->LastComposition().isSome()) {
        maxCachedOffset = std::min(
            maxCachedOffset, mContentForTSF->LastComposition()->EndOffset());
      }
      aACPStart = std::min(aACPStart, maxCachedOffset);
    }
    // Otherwise, we don't know which character rects are cached.  So, we
    // need to use first unmodified character's rect in this case.  Even
    // if there is no character, the query event will return caret rect
    // instead.
    else {
      aACPStart = lastUnmodifiedOffset;
    }
    MOZ_ASSERT(aACPStart <= aACPEnd);
  }

  // If TIP requests caret rect with collapsed range, we should keep
  // collapsing the range.
  if (collapsed) {
    aACPEnd = aACPStart;
  }
  // Let's set aACPEnd to larger offset of last unmodified offset or
  // aACPStart which may be the first character offset of the composition
  // string.  However, some TIPs may want to know the right edge of the
  // range.  Therefore, if aACPEnd is in composition string and active TIP
  // doesn't retrieve caret rect (i.e., the range isn't collapsed), we
  // should keep using the original aACPEnd.  Otherwise, we should set
  // aACPEnd to larger value of aACPStart and lastUnmodifiedOffset.
  else if (mContentForTSF->IsLayoutChangedAt(aACPEnd) &&
           !mContentForTSF->LatestCompositionRange()
                ->IsOffsetInRangeOrEndOffset(aACPEnd)) {
    aACPEnd = std::max(aACPStart, lastUnmodifiedOffset);
  }

  MOZ_LOG(
      sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::HackNoErrorLayoutBugs() hacked the queried range "
       "for not returning TS_E_NOLAYOUT, new values are: "
       "aACPStart=%d, aACPEnd=%d",
       this, aACPStart, aACPEnd));

  return true;
}

STDMETHODIMP
TSFTextStore::GetScreenExt(TsViewCookie vcView, RECT* prc) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::GetScreenExt(vcView=%ld, prc=0x%p)", this,
           vcView, prc));

  if (vcView != TEXTSTORE_DEFAULT_VIEW) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetScreenExt() FAILED due to "
             "called with invalid view",
             this));
    return E_INVALIDARG;
  }

  if (!prc) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetScreenExt() FAILED due to "
             "null argument",
             this));
    return E_INVALIDARG;
  }

  if (mDestroyed) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetScreenExt() returns empty rect "
             "due to already destroyed",
             this));
    prc->left = prc->top = prc->right = prc->bottom = 0;
    return S_OK;
  }

  if (!GetScreenExtInternal(*prc)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetScreenExt() FAILED due to "
             "GetScreenExtInternal() failure",
             this));
    return E_FAIL;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::GetScreenExt() succeeded: "
           "*prc={ left=%ld, top=%ld, right=%ld, bottom=%ld }",
           this, prc->left, prc->top, prc->right, prc->bottom));
  return S_OK;
}

bool TSFTextStore::GetScreenExtInternal(RECT& aScreenExt) {
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::GetScreenExtInternal()", this));

  MOZ_ASSERT(!mDestroyed);

  // use NS_QUERY_EDITOR_RECT to get rect in system, screen coordinates
  WidgetQueryContentEvent queryEditorRectEvent(true, eQueryEditorRect, mWidget);
  mWidget->InitEvent(queryEditorRectEvent);
  DispatchEvent(queryEditorRectEvent);
  if (queryEditorRectEvent.Failed()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetScreenExtInternal() FAILED due to "
             "eQueryEditorRect failure",
             this));
    return false;
  }

  nsWindow* refWindow =
      static_cast<nsWindow*>(!!queryEditorRectEvent.mReply->mFocusedWidget
                                 ? queryEditorRectEvent.mReply->mFocusedWidget
                                 : static_cast<nsIWidget*>(mWidget.get()));
  // Result rect is in top level widget coordinates
  refWindow = refWindow->GetTopLevelWindow(false);
  if (!refWindow) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetScreenExtInternal() FAILED due to "
             "no top level window",
             this));
    return false;
  }

  LayoutDeviceIntRect boundRect = refWindow->GetClientBounds();
  boundRect.MoveTo(0, 0);

  // Clip frame rect to window rect
  boundRect.IntersectRect(queryEditorRectEvent.mReply->mRect, boundRect);
  if (!boundRect.IsEmpty()) {
    boundRect.MoveBy(refWindow->WidgetToScreenOffset());
    ::SetRect(&aScreenExt, boundRect.X(), boundRect.Y(), boundRect.XMost(),
              boundRect.YMost());
  } else {
    ::SetRectEmpty(&aScreenExt);
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::GetScreenExtInternal() succeeded: "
           "aScreenExt={ left=%ld, top=%ld, right=%ld, bottom=%ld }",
           this, aScreenExt.left, aScreenExt.top, aScreenExt.right,
           aScreenExt.bottom));
  return true;
}

STDMETHODIMP
TSFTextStore::GetWnd(TsViewCookie vcView, HWND* phwnd) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::GetWnd(vcView=%ld, phwnd=0x%p), "
           "mWidget=0x%p",
           this, vcView, phwnd, mWidget.get()));

  if (vcView != TEXTSTORE_DEFAULT_VIEW) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetWnd() FAILED due to "
             "called with invalid view",
             this));
    return E_INVALIDARG;
  }

  if (!phwnd) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::GetScreenExt() FAILED due to "
             "null argument",
             this));
    return E_INVALIDARG;
  }

  *phwnd = mWidget ? mWidget->GetWindowHandle() : nullptr;

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::GetWnd() succeeded: *phwnd=0x%p", this,
           static_cast<void*>(*phwnd)));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::InsertTextAtSelection(DWORD dwFlags, const WCHAR* pchText,
                                    ULONG cch, LONG* pacpStart, LONG* pacpEnd,
                                    TS_TEXTCHANGE* pChange) {
  MOZ_LOG(
      sTextStoreLog, LogLevel::Info,
      ("0x%p TSFTextStore::InsertTextAtSelection(dwFlags=%s, "
       "pchText=0x%p \"%s\", cch=%lu, pacpStart=0x%p, pacpEnd=0x%p, "
       "pChange=0x%p), mComposition=%s",
       this,
       dwFlags == 0                  ? "0"
       : dwFlags == TF_IAS_NOQUERY   ? "TF_IAS_NOQUERY"
       : dwFlags == TF_IAS_QUERYONLY ? "TF_IAS_QUERYONLY"
                                     : "Unknown",
       pchText, pchText && cch ? GetEscapedUTF8String(pchText, cch).get() : "",
       cch, pacpStart, pacpEnd, pChange, ToString(mComposition).c_str()));

  if (cch && !pchText) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
             "null pchText",
             this));
    return E_INVALIDARG;
  }

  if (TS_IAS_QUERYONLY == dwFlags) {
    if (!IsReadLocked()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
               "not locked (read)",
               this));
      return TS_E_NOLOCK;
    }

    if (!pacpStart || !pacpEnd) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
               "null argument",
               this));
      return E_INVALIDARG;
    }

    // Get selection first
    Maybe<Selection>& selectionForTSF = SelectionForTSF();
    if (selectionForTSF.isNothing()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
               "SelectionForTSF() failure",
               this));
      return E_FAIL;
    }

    // Simulate text insertion
    *pacpStart = selectionForTSF->StartOffset();
    *pacpEnd = selectionForTSF->EndOffset();
    if (pChange) {
      pChange->acpStart = selectionForTSF->StartOffset();
      pChange->acpOldEnd = selectionForTSF->EndOffset();
      pChange->acpNewEnd =
          selectionForTSF->StartOffset() + static_cast<LONG>(cch);
    }
  } else {
    if (!IsReadWriteLocked()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
               "not locked (read-write)",
               this));
      return TS_E_NOLOCK;
    }

    if (!pChange) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
               "null pChange",
               this));
      return E_INVALIDARG;
    }

    if (TS_IAS_NOQUERY != dwFlags && (!pacpStart || !pacpEnd)) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
               "null argument",
               this));
      return E_INVALIDARG;
    }

    if (!InsertTextAtSelectionInternal(nsDependentSubstring(pchText, cch),
                                       pChange)) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::InsertTextAtSelection() FAILED due to "
               "InsertTextAtSelectionInternal() failure",
               this));
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
           pChange ? pChange->acpStart : 0, pChange ? pChange->acpOldEnd : 0,
           pChange ? pChange->acpNewEnd : 0));
  return S_OK;
}

bool TSFTextStore::InsertTextAtSelectionInternal(const nsAString& aInsertStr,
                                                 TS_TEXTCHANGE* aTextChange) {
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::InsertTextAtSelectionInternal("
           "aInsertStr=\"%s\", aTextChange=0x%p), mComposition=%s",
           this, GetEscapedUTF8String(aInsertStr).get(), aTextChange,
           ToString(mComposition).c_str()));

  Maybe<Content>& contentForTSF = ContentForTSF();
  if (contentForTSF.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::InsertTextAtSelectionInternal() failed "
             "due to ContentForTSF() failure()",
             this));
    return false;
  }

  MaybeDispatchKeyboardEventAsProcessedByIME();
  if (mDestroyed) {
    MOZ_LOG(
        sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::InsertTextAtSelectionInternal() FAILED due to "
         "destroyed during dispatching a keyboard event",
         this));
    return false;
  }

  TS_SELECTION_ACP oldSelection = contentForTSF->Selection()->ACPRef();
  if (mComposition.isNothing()) {
    // Use a temporary composition to contain the text
    PendingAction* compositionStart = mPendingActions.AppendElements(2);
    PendingAction* compositionEnd = compositionStart + 1;

    compositionStart->mType = PendingAction::Type::eCompositionStart;
    compositionStart->mSelectionStart = oldSelection.acpStart;
    compositionStart->mSelectionLength =
        oldSelection.acpEnd - oldSelection.acpStart;
    compositionStart->mAdjustSelection = false;

    compositionEnd->mType = PendingAction::Type::eCompositionEnd;
    compositionEnd->mData = aInsertStr;
    compositionEnd->mSelectionStart = compositionStart->mSelectionStart;

    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
            ("0x%p   TSFTextStore::InsertTextAtSelectionInternal() "
             "appending pending compositionstart and compositionend... "
             "PendingCompositionStart={ mSelectionStart=%d, "
             "mSelectionLength=%d }, PendingCompositionEnd={ mData=\"%s\" "
             "(Length()=%u), mSelectionStart=%d }",
             this, compositionStart->mSelectionStart,
             compositionStart->mSelectionLength,
             GetEscapedUTF8String(compositionEnd->mData).get(),
             compositionEnd->mData.Length(), compositionEnd->mSelectionStart));
  }

  contentForTSF->ReplaceSelectedTextWith(aInsertStr);

  if (aTextChange) {
    aTextChange->acpStart = oldSelection.acpStart;
    aTextChange->acpOldEnd = oldSelection.acpEnd;
    aTextChange->acpNewEnd = contentForTSF->Selection()->EndOffset();
  }

  MOZ_LOG(
      sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::InsertTextAtSelectionInternal() "
       "succeeded: mWidget=0x%p, mWidget->Destroyed()=%s, aTextChange={ "
       "acpStart=%ld, acpOldEnd=%ld, acpNewEnd=%ld }",
       this, mWidget.get(), GetBoolName(mWidget ? mWidget->Destroyed() : true),
       aTextChange ? aTextChange->acpStart : 0,
       aTextChange ? aTextChange->acpOldEnd : 0,
       aTextChange ? aTextChange->acpNewEnd : 0));
  return true;
}

STDMETHODIMP
TSFTextStore::InsertEmbeddedAtSelection(DWORD dwFlags, IDataObject* pDataObject,
                                        LONG* pacpStart, LONG* pacpEnd,
                                        TS_TEXTCHANGE* pChange) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::InsertEmbeddedAtSelection() called "
           "but not supported (E_NOTIMPL)",
           this));

  // embedded objects are not supported
  return E_NOTIMPL;
}

HRESULT TSFTextStore::RecordCompositionStartAction(
    ITfCompositionView* aCompositionView, ITfRange* aRange,
    bool aPreserveSelection) {
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::RecordCompositionStartAction("
           "aCompositionView=0x%p, aRange=0x%p, aPreserveSelection=%s), "
           "mComposition=%s",
           this, aCompositionView, aRange, GetBoolName(aPreserveSelection),
           ToString(mComposition).c_str()));

  LONG start = 0, length = 0;
  HRESULT hr = GetRangeExtent(aRange, &start, &length);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RecordCompositionStartAction() FAILED "
             "due to GetRangeExtent() failure",
             this));
    return hr;
  }

  return RecordCompositionStartAction(aCompositionView, start, length,
                                      aPreserveSelection);
}

HRESULT TSFTextStore::RecordCompositionStartAction(
    ITfCompositionView* aCompositionView, LONG aStart, LONG aLength,
    bool aPreserveSelection) {
  MOZ_LOG(
      sTextStoreLog, LogLevel::Debug,
      ("0x%p   TSFTextStore::RecordCompositionStartAction("
       "aCompositionView=0x%p, aStart=%d, aLength=%d, aPreserveSelection=%s), "
       "mComposition=%s",
       this, aCompositionView, aStart, aLength, GetBoolName(aPreserveSelection),
       ToString(mComposition).c_str()));

  Maybe<Content>& contentForTSF = ContentForTSF();
  if (contentForTSF.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RecordCompositionStartAction() FAILED "
             "due to ContentForTSF() failure",
             this));
    return E_FAIL;
  }

  MaybeDispatchKeyboardEventAsProcessedByIME();
  if (mDestroyed) {
    MOZ_LOG(
        sTextStoreLog, LogLevel::Error,
        ("0x%p   TSFTextStore::RecordCompositionStartAction() FAILED due to "
         "destroyed during dispatching a keyboard event",
         this));
    return false;
  }

  CompleteLastActionIfStillIncomplete();

  // TIP may have inserted text at selection before calling
  // OnStartComposition().  In this case, we've already created a pending
  // compositionend.  If new composition replaces all commit string of the
  // pending compositionend, we should cancel the pending compositionend and
  // keep the previous composition normally.
  // On Windows 7, MS-IME for Korean, MS-IME 2010 for Korean and MS Old Hangul
  // may start composition with calling InsertTextAtSelection() and
  // OnStartComposition() with this order (bug 1208043).
  // On Windows 10, MS Pinyin, MS Wubi, MS ChangJie and MS Quick commits
  // last character and replace it with empty string with new composition
  // when user removes last character of composition string with Backspace
  // key (bug 1462257).
  if (!aPreserveSelection &&
      IsLastPendingActionCompositionEndAt(aStart, aLength)) {
    const PendingAction& pendingCompositionEnd = mPendingActions.LastElement();
    contentForTSF->RestoreCommittedComposition(aCompositionView,
                                               pendingCompositionEnd);
    mPendingActions.RemoveLastElement();
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
            ("0x%p   TSFTextStore::RecordCompositionStartAction() "
             "succeeded: restoring the committed string as composing string, "
             "mComposition=%s, mSelectionForTSF=%s",
             this, ToString(mComposition).c_str(),
             ToString(mSelectionForTSF).c_str()));
    return S_OK;
  }

  PendingAction* action = mPendingActions.AppendElement();
  action->mType = PendingAction::Type::eCompositionStart;
  action->mSelectionStart = aStart;
  action->mSelectionLength = aLength;

  Maybe<Selection>& selectionForTSF = SelectionForTSF();
  if (selectionForTSF.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RecordCompositionStartAction() FAILED "
             "due to SelectionForTSF() failure",
             this));
    action->mAdjustSelection = true;
  } else if (selectionForTSF->MinOffset() != aStart ||
             selectionForTSF->MaxOffset() != aStart + aLength) {
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

  contentForTSF->StartComposition(aCompositionView, *action,
                                  aPreserveSelection);
  MOZ_ASSERT(mComposition.isSome());
  action->mData = mComposition->DataRef();

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::RecordCompositionStartAction() succeeded: "
           "mComposition=%s, mSelectionForTSF=%s }",
           this, ToString(mComposition).c_str(),
           ToString(mSelectionForTSF).c_str()));
  return S_OK;
}

HRESULT
TSFTextStore::RecordCompositionEndAction() {
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::RecordCompositionEndAction(), "
           "mComposition=%s",
           this, ToString(mComposition).c_str()));

  MOZ_ASSERT(mComposition.isSome());

  if (mComposition.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RecordCompositionEndAction() FAILED due to "
             "no composition",
             this));
    return false;
  }

  MaybeDispatchKeyboardEventAsProcessedByIME();
  if (mDestroyed) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RecordCompositionEndAction() FAILED due to "
             "destroyed during dispatching a keyboard event",
             this));
    return false;
  }

  // If we're handling incomplete composition update or already handled
  // composition update, we can forget them since composition end will send
  // the latest composition string and it overwrites the composition string
  // even if we dispatch eCompositionChange event before that.  So, let's
  // forget all composition updates now.
  RemoveLastCompositionUpdateActions();
  PendingAction* action = mPendingActions.AppendElement();
  action->mType = PendingAction::Type::eCompositionEnd;
  action->mData = mComposition->DataRef();
  action->mSelectionStart = mComposition->StartOffset();

  Maybe<Content>& contentForTSF = ContentForTSF();
  if (contentForTSF.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::RecordCompositionEndAction() FAILED due "
             "to ContentForTSF() failure",
             this));
    return E_FAIL;
  }
  contentForTSF->EndComposition(*action);

  // If this composition was restart but the composition doesn't modify
  // anything, we should remove the pending composition for preventing to
  // dispatch redundant composition events.
  for (size_t i = mPendingActions.Length(), j = 1; i > 0; --i, ++j) {
    PendingAction& pendingAction = mPendingActions[i - 1];
    if (pendingAction.mType == PendingAction::Type::eCompositionStart) {
      if (pendingAction.mData != action->mData) {
        break;
      }
      // When only setting selection is necessary, we should append it.
      if (pendingAction.mAdjustSelection) {
        LONG selectionStart = pendingAction.mSelectionStart;
        LONG selectionLength = pendingAction.mSelectionLength;

        PendingAction* setSelection = mPendingActions.AppendElement();
        setSelection->mType = PendingAction::Type::eSetSelection;
        setSelection->mSelectionStart = selectionStart;
        setSelection->mSelectionLength = selectionLength;
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

  MOZ_LOG(
      sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::RecordCompositionEndAction(), succeeded", this));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::OnStartComposition(ITfCompositionView* pComposition, BOOL* pfOk) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::OnStartComposition(pComposition=0x%p, "
           "pfOk=0x%p), mComposition=%s",
           this, pComposition, pfOk, ToString(mComposition).c_str()));

  AutoPendingActionAndContentFlusher flusher(this);

  *pfOk = FALSE;

  // Only one composition at a time
  if (mComposition.isSome()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::OnStartComposition() FAILED due to "
             "there is another composition already (but returns S_OK)",
             this));
    return S_OK;
  }

  RefPtr<ITfRange> range;
  HRESULT hr = pComposition->GetRange(getter_AddRefs(range));
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::OnStartComposition() FAILED due to "
             "pComposition->GetRange() failure",
             this));
    return hr;
  }
  hr = RecordCompositionStartAction(pComposition, range, false);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::OnStartComposition() FAILED due to "
             "RecordCompositionStartAction() failure",
             this));
    return hr;
  }

  *pfOk = TRUE;
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::OnStartComposition() succeeded", this));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::OnUpdateComposition(ITfCompositionView* pComposition,
                                  ITfRange* pRangeNew) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::OnUpdateComposition(pComposition=0x%p, "
           "pRangeNew=0x%p), mComposition=%s",
           this, pComposition, pRangeNew, ToString(mComposition).c_str()));

  AutoPendingActionAndContentFlusher flusher(this);

  if (!mDocumentMgr || !mContext) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::OnUpdateComposition() FAILED due to "
             "not ready for the composition",
             this));
    return E_UNEXPECTED;
  }
  if (mComposition.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::OnUpdateComposition() FAILED due to "
             "no active composition",
             this));
    return E_UNEXPECTED;
  }
  if (mComposition->GetView() != pComposition) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::OnUpdateComposition() FAILED due to "
             "different composition view specified",
             this));
    return E_UNEXPECTED;
  }

  // pRangeNew is null when the update is not complete
  if (!pRangeNew) {
    MaybeDispatchKeyboardEventAsProcessedByIME();
    if (mDestroyed) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::OnUpdateComposition() FAILED due to "
               "destroyed during dispatching a keyboard event",
               this));
      return E_FAIL;
    }
    PendingAction* action = LastOrNewPendingCompositionUpdate();
    action->mIncomplete = true;
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
            ("0x%p   TSFTextStore::OnUpdateComposition() succeeded but "
             "not complete",
             this));
    return S_OK;
  }

  HRESULT hr = RestartCompositionIfNecessary(pRangeNew);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::OnUpdateComposition() FAILED due to "
             "RestartCompositionIfNecessary() failure",
             this));
    return hr;
  }

  hr = RecordCompositionUpdateAction();
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::OnUpdateComposition() FAILED due to "
             "RecordCompositionUpdateAction() failure",
             this));
    return hr;
  }

  if (MOZ_LOG_TEST(sTextStoreLog, LogLevel::Info)) {
    Maybe<Selection>& selectionForTSF = SelectionForTSF();
    if (selectionForTSF.isNothing()) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("0x%p   TSFTextStore::OnUpdateComposition() FAILED due to "
               "SelectionForTSF() failure",
               this));
      return E_FAIL;
    }
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
            ("0x%p   TSFTextStore::OnUpdateComposition() succeeded: "
             "mComposition=%s, SelectionForTSF()=%s",
             this, ToString(mComposition).c_str(),
             ToString(selectionForTSF).c_str()));
  }
  return S_OK;
}

STDMETHODIMP
TSFTextStore::OnEndComposition(ITfCompositionView* pComposition) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::OnEndComposition(pComposition=0x%p), "
           "mComposition=%s",
           this, pComposition, ToString(mComposition).c_str()));

  AutoPendingActionAndContentFlusher flusher(this);

  if (mComposition.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::OnEndComposition() FAILED due to "
             "no active composition",
             this));
    return E_UNEXPECTED;
  }

  if (mComposition->GetView() != pComposition) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::OnEndComposition() FAILED due to "
             "different composition view specified",
             this));
    return E_UNEXPECTED;
  }

  HRESULT hr = RecordCompositionEndAction();
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::OnEndComposition() FAILED due to "
             "RecordCompositionEndAction() failure",
             this));
    return hr;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::OnEndComposition(), succeeded", this));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::AdviseMouseSink(ITfRangeACP* range, ITfMouseSink* pSink,
                              DWORD* pdwCookie) {
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p TSFTextStore::AdviseMouseSink(range=0x%p, pSink=0x%p, "
           "pdwCookie=0x%p)",
           this, range, pSink, pdwCookie));

  if (!pdwCookie) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::AdviseMouseSink() FAILED due to the "
             "pdwCookie is null",
             this));
    return E_INVALIDARG;
  }
  // Initialize the result with invalid cookie for safety.
  *pdwCookie = MouseTracker::kInvalidCookie;

  if (!range) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::AdviseMouseSink() FAILED due to the "
             "range is null",
             this));
    return E_INVALIDARG;
  }
  if (!pSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::AdviseMouseSink() FAILED due to the "
             "pSink is null",
             this));
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
               "failure of MouseTracker::Init()",
               this));
      return hr;
    }
  }
  HRESULT hr = tracker->AdviseSink(this, range, pSink);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::AdviseMouseSink() FAILED due to failure "
             "of MouseTracker::Init()",
             this));
    return hr;
  }
  *pdwCookie = tracker->Cookie();
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::AdviseMouseSink(), succeeded, "
           "*pdwCookie=%d",
           this, *pdwCookie));
  return S_OK;
}

STDMETHODIMP
TSFTextStore::UnadviseMouseSink(DWORD dwCookie) {
  MOZ_LOG(
      sTextStoreLog, LogLevel::Info,
      ("0x%p TSFTextStore::UnadviseMouseSink(dwCookie=%d)", this, dwCookie));
  if (dwCookie == MouseTracker::kInvalidCookie) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::UnadviseMouseSink() FAILED due to "
             "the cookie is invalid value",
             this));
    return E_INVALIDARG;
  }
  // The cookie value must be an index of mMouseTrackers.
  // We can use this shortcut for now.
  if (static_cast<size_t>(dwCookie) >= mMouseTrackers.Length()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::UnadviseMouseSink() FAILED due to "
             "the cookie is too large value",
             this));
    return E_INVALIDARG;
  }
  MouseTracker& tracker = mMouseTrackers[dwCookie];
  if (!tracker.IsUsing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::UnadviseMouseSink() FAILED due to "
             "the found tracker uninstalled already",
             this));
    return E_INVALIDARG;
  }
  tracker.UnadviseSink();
  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::UnadviseMouseSink(), succeeded", this));
  return S_OK;
}

// static
nsresult TSFTextStore::OnFocusChange(bool aGotFocus,
                                     nsWindowBase* aFocusedWidget,
                                     const InputContext& aContext) {
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("  TSFTextStore::OnFocusChange(aGotFocus=%s, "
           "aFocusedWidget=0x%p, aContext=%s), "
           "sThreadMgr=0x%p, sEnabledTextStore=0x%p",
           GetBoolName(aGotFocus), aFocusedWidget,
           mozilla::ToString(aContext).c_str(), sThreadMgr.get(),
           sEnabledTextStore.get()));

  if (NS_WARN_IF(!IsInTSFMode())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<ITfDocumentMgr> prevFocusedDocumentMgr;
  bool hasFocus = ThinksHavingFocus();
  RefPtr<TSFTextStore> oldTextStore = sEnabledTextStore.forget();

  // If currently oldTextStore still has focus, notifies TSF of losing focus.
  if (hasFocus) {
    RefPtr<ITfThreadMgr> threadMgr = sThreadMgr;
    DebugOnly<HRESULT> hr = threadMgr->AssociateFocus(
        oldTextStore->mWidget->GetWindowHandle(), nullptr,
        getter_AddRefs(prevFocusedDocumentMgr));
    NS_ASSERTION(SUCCEEDED(hr), "Disassociating focus failed");
    NS_ASSERTION(prevFocusedDocumentMgr == oldTextStore->mDocumentMgr,
                 "different documentMgr has been associated with the window");
  }

  // Even if there was a focused TextStore, we won't use it with new focused
  // editor.  So, release it now.
  if (oldTextStore) {
    oldTextStore->Destroy();
  }

  if (NS_WARN_IF(!sThreadMgr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::OnFocusChange() FAILED, due to "
             "sThreadMgr being destroyed during calling "
             "ITfThreadMgr::AssociateFocus()"));
    return NS_ERROR_FAILURE;
  }
  if (NS_WARN_IF(sEnabledTextStore)) {
    MOZ_LOG(
        sTextStoreLog, LogLevel::Error,
        ("  TSFTextStore::OnFocusChange() FAILED, due to "
         "nested event handling has created another focused TextStore during "
         "calling ITfThreadMgr::AssociateFocus()"));
    return NS_ERROR_FAILURE;
  }

  // If this is a notification of blur, move focus to the dummy document
  // manager.
  if (!aGotFocus || !aContext.mIMEState.IsEditable()) {
    RefPtr<ITfThreadMgr> threadMgr = sThreadMgr;
    RefPtr<ITfDocumentMgr> disabledDocumentMgr = sDisabledDocumentMgr;
    HRESULT hr = threadMgr->SetFocus(disabledDocumentMgr);
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
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// static
void TSFTextStore::EnsureToDestroyAndReleaseEnabledTextStoreIf(
    RefPtr<TSFTextStore>& aTextStore) {
  aTextStore->Destroy();
  if (sEnabledTextStore == aTextStore) {
    sEnabledTextStore = nullptr;
  }
  aTextStore = nullptr;
}

// static
bool TSFTextStore::CreateAndSetFocus(nsWindowBase* aFocusedWidget,
                                     const InputContext& aContext) {
  // TSF might do something which causes that we need to access static methods
  // of TSFTextStore.  At that time, sEnabledTextStore may be necessary.
  // So, we should set sEnabledTextStore directly.
  RefPtr<TSFTextStore> textStore = new TSFTextStore();
  sEnabledTextStore = textStore;
  if (NS_WARN_IF(!textStore->Init(aFocusedWidget, aContext))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::CreateAndSetFocus() FAILED due to "
             "TSFTextStore::Init() failure"));
    EnsureToDestroyAndReleaseEnabledTextStoreIf(textStore);
    return false;
  }
  RefPtr<ITfDocumentMgr> newDocMgr = textStore->mDocumentMgr;
  if (NS_WARN_IF(!newDocMgr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::CreateAndSetFocus() FAILED due to "
             "invalid TSFTextStore::mDocumentMgr"));
    EnsureToDestroyAndReleaseEnabledTextStoreIf(textStore);
    return false;
  }
  if (aContext.mIMEState.mEnabled == IMEEnabled::Password) {
    MarkContextAsKeyboardDisabled(textStore->mContext);
    RefPtr<ITfContext> topContext;
    newDocMgr->GetTop(getter_AddRefs(topContext));
    if (topContext && topContext != textStore->mContext) {
      MarkContextAsKeyboardDisabled(topContext);
    }
  }

  HRESULT hr;
  RefPtr<ITfThreadMgr> threadMgr = sThreadMgr;
  hr = threadMgr->SetFocus(newDocMgr);

  if (NS_WARN_IF(FAILED(hr))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::CreateAndSetFocus() FAILED due to "
             "ITfTheadMgr::SetFocus() failure"));
    EnsureToDestroyAndReleaseEnabledTextStoreIf(textStore);
    return false;
  }
  if (NS_WARN_IF(!sThreadMgr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::CreateAndSetFocus() FAILED due to "
             "sThreadMgr being destroyed during calling "
             "ITfTheadMgr::SetFocus()"));
    EnsureToDestroyAndReleaseEnabledTextStoreIf(textStore);
    return false;
  }
  if (NS_WARN_IF(sEnabledTextStore != textStore)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::CreateAndSetFocus() FAILED due to "
             "creating TextStore has lost focus during calling "
             "ITfThreadMgr::SetFocus()"));
    EnsureToDestroyAndReleaseEnabledTextStoreIf(textStore);
    return false;
  }

  // Use AssociateFocus() for ensuring that any native focus event
  // never steal focus from our documentMgr.
  RefPtr<ITfDocumentMgr> prevFocusedDocumentMgr;
  hr = threadMgr->AssociateFocus(aFocusedWidget->GetWindowHandle(), newDocMgr,
                                 getter_AddRefs(prevFocusedDocumentMgr));
  if (NS_WARN_IF(FAILED(hr))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::CreateAndSetFocus() FAILED due to "
             "ITfTheadMgr::AssociateFocus() failure"));
    EnsureToDestroyAndReleaseEnabledTextStoreIf(textStore);
    return false;
  }
  if (NS_WARN_IF(!sThreadMgr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::CreateAndSetFocus() FAILED due to "
             "sThreadMgr being destroyed during calling "
             "ITfTheadMgr::AssociateFocus()"));
    EnsureToDestroyAndReleaseEnabledTextStoreIf(textStore);
    return false;
  }
  if (NS_WARN_IF(sEnabledTextStore != textStore)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::CreateAndSetFocus() FAILED due to "
             "creating TextStore has lost focus during calling "
             "ITfTheadMgr::AssociateFocus()"));
    EnsureToDestroyAndReleaseEnabledTextStoreIf(textStore);
    return false;
  }

  if (textStore->mSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
            ("  TSFTextStore::CreateAndSetFocus(), calling "
             "ITextStoreACPSink::OnLayoutChange(TS_LC_CREATE) for 0x%p...",
             textStore.get()));
    RefPtr<ITextStoreACPSink> sink = textStore->mSink;
    sink->OnLayoutChange(TS_LC_CREATE, TEXTSTORE_DEFAULT_VIEW);
    if (NS_WARN_IF(sEnabledTextStore != textStore)) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("  TSFTextStore::CreateAndSetFocus() FAILED due to "
               "creating TextStore has lost focus during calling "
               "ITextStoreACPSink::OnLayoutChange(TS_LC_CREATE)"));
      EnsureToDestroyAndReleaseEnabledTextStoreIf(textStore);
      return false;
    }
  }
  return true;
}

// static
IMENotificationRequests TSFTextStore::GetIMENotificationRequests() {
  if (!sEnabledTextStore || NS_WARN_IF(!sEnabledTextStore->mDocumentMgr)) {
    // If there is no active text store, we don't need any notifications
    // since there is no sink which needs notifications.
    return IMENotificationRequests();
  }

  // Otherwise, requests all notifications since even if some of them may not
  // be required by the sink of active TIP, active TIP may be changed and
  // other TIPs may need all notifications.
  // Note that Windows temporarily steal focus from active window if the main
  // process which created the window becomes busy.  In this case, we shouldn't
  // commit composition since user may want to continue to compose the
  // composition after becoming not busy.  Therefore, we need notifications
  // even during deactive.
  // Be aware, we don't need to check actual focused text store.  For example,
  // MS-IME for Japanese handles focus messages by themselves and sets focused
  // text store to nullptr when the process is being inactivated.  However,
  // we still need to reuse sEnabledTextStore if the process is activated and
  // focused element isn't changed.  Therefore, if sEnabledTextStore isn't
  // nullptr, we need to keep notifying the sink even when it is not focused
  // text store for the thread manager.
  return IMENotificationRequests(
      IMENotificationRequests::NOTIFY_TEXT_CHANGE |
      IMENotificationRequests::NOTIFY_POSITION_CHANGE |
      IMENotificationRequests::NOTIFY_MOUSE_BUTTON_EVENT_ON_CHAR |
      IMENotificationRequests::NOTIFY_DURING_DEACTIVE);
}

nsresult TSFTextStore::OnTextChangeInternal(
    const IMENotification& aIMENotification) {
  const TextChangeDataBase& textChangeData = aIMENotification.mTextChangeData;

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::OnTextChangeInternal(aIMENotification={ "
           "mMessage=0x%08X, mTextChangeData=%s }), "
           "mDestroyed=%s, mSink=0x%p, mSinkMask=%s, "
           "mComposition=%s",
           this, aIMENotification.mMessage,
           mozilla::ToString(textChangeData).c_str(), GetBoolName(mDestroyed),
           mSink.get(), GetSinkMaskNameStr(mSinkMask).get(),
           ToString(mComposition).c_str()));

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

void TSFTextStore::NotifyTSFOfTextChange() {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(!IsReadLocked());
  MOZ_ASSERT(mComposition.isNothing());
  MOZ_ASSERT(mPendingTextChangeData.IsValid());

  // If the text changes are caused only by composition, we don't need to
  // notify TSF of the text changes.
  if (mPendingTextChangeData.mCausedOnlyByComposition) {
    mPendingTextChangeData.Clear();
    return;
  }

  // First, forget cached selection.
  mSelectionForTSF.reset();

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
  textChange.acpStart = static_cast<LONG>(mPendingTextChangeData.mStartOffset);
  textChange.acpOldEnd =
      static_cast<LONG>(mPendingTextChangeData.mRemovedEndOffset);
  textChange.acpNewEnd =
      static_cast<LONG>(mPendingTextChangeData.mAddedEndOffset);
  mPendingTextChangeData.Clear();

  MOZ_LOG(
      sTextStoreLog, LogLevel::Info,
      ("0x%p   TSFTextStore::NotifyTSFOfTextChange(), calling "
       "ITextStoreACPSink::OnTextChange(0, { acpStart=%ld, acpOldEnd=%ld, "
       "acpNewEnd=%ld })...",
       this, textChange.acpStart, textChange.acpOldEnd, textChange.acpNewEnd));
  RefPtr<ITextStoreACPSink> sink = mSink;
  sink->OnTextChange(0, &textChange);
}

nsresult TSFTextStore::OnSelectionChangeInternal(
    const IMENotification& aIMENotification) {
  const SelectionChangeDataBase& selectionChangeData =
      aIMENotification.mSelectionChangeData;
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::OnSelectionChangeInternal("
           "aIMENotification={ mSelectionChangeData=%s }), mDestroyed=%s, "
           "mSink=0x%p, mSinkMask=%s, mIsRecordingActionsWithoutLock=%s, "
           "mComposition=%s",
           this, mozilla::ToString(selectionChangeData).c_str(),
           GetBoolName(mDestroyed), mSink.get(),
           GetSinkMaskNameStr(mSinkMask).get(),
           GetBoolName(mIsRecordingActionsWithoutLock),
           ToString(mComposition).c_str()));

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

  // If we're available, we should create native caret instead of IMEHandler
  // because we may have some cache to do it.
  // Note that if we have composition, we'll notified composition-updated
  // later so that we don't need to create native caret in such case.
  if (!IsHandlingCompositionInContent() &&
      IMEHandler::NeedsToCreateNativeCaret()) {
    CreateNativeCaret();
  }

  return NS_OK;
}

void TSFTextStore::NotifyTSFOfSelectionChange() {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(!IsReadLocked());
  MOZ_ASSERT(mComposition.isNothing());
  MOZ_ASSERT(mPendingSelectionChangeData.IsValid());

  // If selection range isn't actually changed, we don't need to notify TSF
  // of this selection change.
  if (mSelectionForTSF.isNothing()) {
    mSelectionForTSF.emplace(mPendingSelectionChangeData.mOffset,
                             mPendingSelectionChangeData.Length(),
                             mPendingSelectionChangeData.mReversed,
                             mPendingSelectionChangeData.GetWritingMode());
  } else if (!mSelectionForTSF->SetSelection(
                 mPendingSelectionChangeData.mOffset,
                 mPendingSelectionChangeData.Length(),
                 mPendingSelectionChangeData.mReversed,
                 mPendingSelectionChangeData.GetWritingMode())) {
    mPendingSelectionChangeData.Clear();
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
            ("0x%p   TSFTextStore::NotifyTSFOfSelectionChange(), "
             "selection isn't actually changed.",
             this));
    return;
  }

  mPendingSelectionChangeData.Clear();

  if (!mSink || !(mSinkMask & TS_AS_SEL_CHANGE)) {
    return;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("0x%p   TSFTextStore::NotifyTSFOfSelectionChange(), calling "
           "ITextStoreACPSink::OnSelectionChange()...",
           this));
  RefPtr<ITextStoreACPSink> sink = mSink;
  sink->OnSelectionChange();
}

nsresult TSFTextStore::OnLayoutChangeInternal() {
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
           "NotifyTSFOfLayoutChange()...",
           this));
  if (NS_WARN_IF(!NotifyTSFOfLayoutChange())) {
    rv = NS_ERROR_FAILURE;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::OnLayoutChangeInternal(), calling "
           "MaybeFlushPendingNotifications()...",
           this));
  MaybeFlushPendingNotifications();

  return rv;
}

bool TSFTextStore::NotifyTSFOfLayoutChange() {
  MOZ_ASSERT(!mDestroyed);

  // If we're waiting a query of layout information from TIP, it means that
  // we've returned TS_E_NOLAYOUT error.
  bool returnedNoLayoutError = mHasReturnedNoLayoutError || mWaitingQueryLayout;

  // If we returned TS_E_NOLAYOUT, TIP should query the computed layout again.
  mWaitingQueryLayout = returnedNoLayoutError;

  // For avoiding to call this method again at unlocking the document during
  // calls of OnLayoutChange(), reset mHasReturnedNoLayoutError.
  mHasReturnedNoLayoutError = false;

  // Now, layout has been computed.  We should notify mContentForTSF for
  // making GetTextExt() and GetACPFromPoint() not return TS_E_NOLAYOUT.
  if (mContentForTSF.isSome()) {
    mContentForTSF->OnLayoutChanged();
  }

  if (IMEHandler::NeedsToCreateNativeCaret()) {
    // If we're available, we should create native caret instead of IMEHandler
    // because we may have some cache to do it.
    CreateNativeCaret();
  } else {
    // Now, the caret position is different from ours.  Destroy the native caret
    // if we've create it only for GetTextExt().
    IMEHandler::MaybeDestroyNativeCaret();
  }

  // This method should return true if either way succeeds.
  bool ret = true;

  if (mSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
            ("0x%p   TSFTextStore::NotifyTSFOfLayoutChange(), "
             "calling ITextStoreACPSink::OnLayoutChange()...",
             this));
    RefPtr<ITextStoreACPSink> sink = mSink;
    HRESULT hr = sink->OnLayoutChange(TS_LC_CHANGE, TEXTSTORE_DEFAULT_VIEW);
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
           "OnLayoutChange() again...",
           this));
  ::PostMessage(mWidget->GetWindowHandle(), MOZ_WM_NOTIY_TSF_OF_LAYOUT_CHANGE,
                reinterpret_cast<WPARAM>(this), 0);

  return true;
}

void TSFTextStore::NotifyTSFOfLayoutChangeAgain() {
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
           "calling NotifyTSFOfLayoutChange()...",
           this));
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
             "retrieve the layout information",
             this));
  } else {
    MOZ_LOG(sTextStoreLog, LogLevel::Info,
            ("0x%p   TSFTextStore::NotifyTSFOfLayoutChangeAgain(), "
             "called NotifyTSFOfLayoutChange()",
             this));
  }
}

nsresult TSFTextStore::OnUpdateCompositionInternal() {
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::OnUpdateCompositionInternal(), "
           "mDestroyed=%s, mDeferNotifyingTSF=%s",
           this, GetBoolName(mDestroyed), GetBoolName(mDeferNotifyingTSF)));

  // There are nothing to do after destroyed.
  if (mDestroyed) {
    return NS_OK;
  }

  // Update cached data now because all pending events have been handled now.
  if (mContentForTSF.isSome()) {
    mContentForTSF->OnCompositionEventsHandled();
  }

  // If composition is completely finished both in TSF/TIP and the focused
  // editor which may be in a remote process, we can clear the cache and don't
  // have it until starting next composition.
  if (mComposition.isNothing() && !IsHandlingCompositionInContent()) {
    mDeferClearingContentForTSF = false;
  }
  mDeferNotifyingTSF = false;
  MaybeFlushPendingNotifications();

  // If we're available, we should create native caret instead of IMEHandler
  // because we may have some cache to do it.
  if (IMEHandler::NeedsToCreateNativeCaret()) {
    CreateNativeCaret();
  }

  return NS_OK;
}

nsresult TSFTextStore::OnMouseButtonEventInternal(
    const IMENotification& aIMENotification) {
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
           "aIMENotification={ mEventMessage=%s, mOffset=%u, mCursorPos=%s, "
           "mCharRect=%s, mButton=%s, mButtons=%s, mModifiers=%s })",
           this, ToChar(aIMENotification.mMouseButtonEventData.mEventMessage),
           aIMENotification.mMouseButtonEventData.mOffset,
           ToString(aIMENotification.mMouseButtonEventData.mCursorPos).c_str(),
           ToString(aIMENotification.mMouseButtonEventData.mCharRect).c_str(),
           GetMouseButtonName(aIMENotification.mMouseButtonEventData.mButton),
           GetMouseButtonsName(aIMENotification.mMouseButtonEventData.mButtons)
               .get(),
           GetModifiersName(aIMENotification.mMouseButtonEventData.mModifiers)
               .get()));

  uint32_t offset = aIMENotification.mMouseButtonEventData.mOffset;
  if (offset > static_cast<uint32_t>(LONG_MAX)) {
    return NS_OK;
  }
  LayoutDeviceIntRect charRect =
      aIMENotification.mMouseButtonEventData.mCharRect;
  LayoutDeviceIntPoint cursorPos =
      aIMENotification.mMouseButtonEventData.mCursorPos;
  ULONG quadrant = 1;
  if (charRect.Width() > 0) {
    int32_t cursorXInChar = cursorPos.x - charRect.X();
    quadrant = cursorXInChar * 4 / charRect.Width();
    quadrant = (quadrant + 2) % 4;
  }
  ULONG edge = quadrant < 2 ? offset + 1 : offset;
  DWORD buttonStatus = 0;
  bool isMouseUp =
      aIMENotification.mMouseButtonEventData.mEventMessage == eMouseUp;
  if (!isMouseUp) {
    switch (aIMENotification.mMouseButtonEventData.mButton) {
      case MouseButton::ePrimary:
        buttonStatus = MK_LBUTTON;
        break;
      case MouseButton::eMiddle:
        buttonStatus = MK_MBUTTON;
        break;
      case MouseButton::eSecondary:
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
    if (!tracker.IsUsing() || tracker.Range().isNothing() ||
        !tracker.Range()->IsOffsetInRange(offset)) {
      continue;
    }
    if (tracker.OnMouseButtonEvent(edge - tracker.Range()->StartOffset(),
                                   quadrant, buttonStatus)) {
      return NS_SUCCESS_EVENT_CONSUMED;
    }
  }
  return NS_OK;
}

void TSFTextStore::CreateNativeCaret() {
  MOZ_ASSERT(!IMEHandler::IsA11yHandlingNativeCaret());

  IMEHandler::MaybeDestroyNativeCaret();

  // Don't create native caret after destroyed.
  if (mDestroyed) {
    return;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::CreateNativeCaret(), mComposition=%s", this,
           ToString(mComposition).c_str()));

  Maybe<Selection>& selectionForTSF = SelectionForTSF();
  if (selectionForTSF.isNothing()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::CreateNativeCaret() FAILED due to "
             "SelectionForTSF() failure",
             this));
    return;
  }

  WidgetQueryContentEvent queryCaretRectEvent(true, eQueryCaretRect, mWidget);
  mWidget->InitEvent(queryCaretRectEvent);

  WidgetQueryContentEvent::Options options;
  // XXX If this is called without composition and the selection isn't
  //     collapsed, is it OK?
  int64_t caretOffset = selectionForTSF->MaxOffset();
  if (mComposition.isSome()) {
    // If there is a composition, use insertion point relative query for
    // deciding caret position because composition might be at different
    // position where TSFTextStore believes it at.
    options.mRelativeToInsertionPoint = true;
    caretOffset -= mComposition->StartOffset();
  } else if (!CanAccessActualContentDirectly()) {
    // If TSF/TIP cannot access actual content directly, there may be pending
    // text and/or selection changes which have not been notified TSF yet.
    // Therefore, we should use relative to insertion point query since
    // TSF/TIP computes the offset from the cached selection.
    options.mRelativeToInsertionPoint = true;
    caretOffset -= selectionForTSF->StartOffset();
  }
  queryCaretRectEvent.InitForQueryCaretRect(caretOffset, options);

  DispatchEvent(queryCaretRectEvent);
  if (NS_WARN_IF(queryCaretRectEvent.Failed())) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::CreateNativeCaret() FAILED due to "
             "eQueryCaretRect failure (offset=%d)",
             this, caretOffset));
    return;
  }

  if (!IMEHandler::CreateNativeCaret(static_cast<nsWindow*>(mWidget.get()),
                                     queryCaretRectEvent.mReply->mRect)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::CreateNativeCaret() FAILED due to "
             "IMEHandler::CreateNativeCaret() failure",
             this));
    return;
  }
}

void TSFTextStore::CommitCompositionInternal(bool aDiscard) {
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::CommitCompositionInternal(aDiscard=%s), "
           "mSink=0x%p, mContext=0x%p, mComposition=%s",
           this, GetBoolName(aDiscard), mSink.get(), mContext.get(),
           ToString(mComposition).c_str()));

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
               "does nothing because already called and waiting unlock...",
               this));
      return;
    }
    if (aDiscard) {
      mDeferCancellingComposition = true;
    } else {
      mDeferCommittingComposition = true;
    }
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
            ("0x%p   TSFTextStore::CommitCompositionInternal(), "
             "putting off to request to %s composition after unlocking the "
             "document",
             this, aDiscard ? "cancel" : "commit"));
    return;
  }

  if (mComposition.isSome() && aDiscard) {
    LONG endOffset = mComposition->EndOffset();
    mComposition->SetData(EmptyString());
    // Note that don't notify TSF of text change after this is destroyed.
    if (mSink && !mDestroyed) {
      TS_TEXTCHANGE textChange;
      textChange.acpStart = mComposition->StartOffset();
      textChange.acpOldEnd = endOffset;
      textChange.acpNewEnd = mComposition->StartOffset();
      MOZ_LOG(sTextStoreLog, LogLevel::Info,
              ("0x%p   TSFTextStore::CommitCompositionInternal(), calling"
               "mSink->OnTextChange(0, { acpStart=%ld, acpOldEnd=%ld, "
               "acpNewEnd=%ld })...",
               this, textChange.acpStart, textChange.acpOldEnd,
               textChange.acpNewEnd));
      RefPtr<ITextStoreACPSink> sink = mSink;
      sink->OnTextChange(0, &textChange);
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
    if (context != mContext) break;
    if (mDocumentMgr) mDocumentMgr->GetTop(getter_AddRefs(context));
  } while (context != mContext);
}

static bool GetCompartment(IUnknown* pUnk, const GUID& aID,
                           ITfCompartment** aCompartment) {
  if (!pUnk) return false;

  RefPtr<ITfCompartmentMgr> compMgr;
  pUnk->QueryInterface(IID_ITfCompartmentMgr, getter_AddRefs(compMgr));
  if (!compMgr) return false;

  return SUCCEEDED(compMgr->GetCompartment(aID, aCompartment)) &&
         (*aCompartment) != nullptr;
}

// static
void TSFTextStore::SetIMEOpenState(bool aState) {
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("TSFTextStore::SetIMEOpenState(aState=%s)", GetBoolName(aState)));

  if (!sThreadMgr) {
    return;
  }

  RefPtr<ITfCompartment> comp = GetCompartmentForOpenClose();
  if (NS_WARN_IF(!comp)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
            ("  TSFTextStore::SetIMEOpenState() FAILED due to"
             "no compartment available"));
    return;
  }

  VARIANT variant;
  variant.vt = VT_I4;
  variant.lVal = aState;
  HRESULT hr = comp->SetValue(sClientId, &variant);
  if (NS_WARN_IF(FAILED(hr))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::SetIMEOpenState() FAILED due to "
             "ITfCompartment::SetValue() failure, hr=0x%08X",
             hr));
    return;
  }
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("  TSFTextStore::SetIMEOpenState(), setting "
           "0x%04X to GUID_COMPARTMENT_KEYBOARD_OPENCLOSE...",
           variant.lVal));
}

// static
bool TSFTextStore::GetIMEOpenState() {
  if (!sThreadMgr) {
    return false;
  }

  RefPtr<ITfCompartment> comp = GetCompartmentForOpenClose();
  if (NS_WARN_IF(!comp)) {
    return false;
  }

  VARIANT variant;
  ::VariantInit(&variant);
  HRESULT hr = comp->GetValue(&variant);
  if (NS_WARN_IF(FAILED(hr))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("TSFTextStore::GetIMEOpenState() FAILED due to "
             "ITfCompartment::GetValue() failure, hr=0x%08X",
             hr));
    return false;
  }
  // Until IME is open in this process, the result may be empty.
  if (variant.vt == VT_EMPTY) {
    return false;
  }
  if (NS_WARN_IF(variant.vt != VT_I4)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("TSFTextStore::GetIMEOpenState() FAILED due to "
             "invalid result of ITfCompartment::GetValue()"));
    ::VariantClear(&variant);
    return false;
  }

  return variant.lVal != 0;
}

// static
void TSFTextStore::SetInputContext(nsWindowBase* aWidget,
                                   const InputContext& aContext,
                                   const InputContextAction& aAction) {
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("TSFTextStore::SetInputContext(aWidget=%p, "
           "aContext=%s, aAction.mFocusChange=%s), "
           "sEnabledTextStore(0x%p)={ mWidget=0x%p }, ThinksHavingFocus()=%s",
           aWidget, mozilla::ToString(aContext).c_str(),
           GetFocusChangeName(aAction.mFocusChange), sEnabledTextStore.get(),
           sEnabledTextStore ? sEnabledTextStore->mWidget.get() : nullptr,
           GetBoolName(ThinksHavingFocus())));

  switch (aAction.mFocusChange) {
    case InputContextAction::WIDGET_CREATED:
      // If this is called when the widget is created, there is nothing to do.
      return;
    case InputContextAction::FOCUS_NOT_CHANGED:
    case InputContextAction::MENU_LOST_PSEUDO_FOCUS:
      if (NS_WARN_IF(!IsInTSFMode())) {
        return;
      }
      // In these cases, `NOTIFY_IME_OF_FOCUS` won't be sent.  Therefore,
      // we need to reset text store for new state right now.
      break;
    default:
      NS_WARNING_ASSERTION(IsInTSFMode(),
                           "Why is this called when TSF is disabled?");
      if (sEnabledTextStore) {
        RefPtr<TSFTextStore> textStore(sEnabledTextStore);
        textStore->SetInputScope(aContext.mHTMLInputType,
                                 aContext.mHTMLInputInputmode,
                                 aContext.mInPrivateBrowsing);
      }
      return;
  }

  // If focus isn't actually changed but the enabled state is changed,
  // emulate the focus move.
  if (!ThinksHavingFocus() && aContext.mIMEState.IsEditable()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
            ("  TSFTextStore::SetInputContent() emulates focus for IME "
             "state change"));
    OnFocusChange(true, aWidget, aContext);
  } else if (ThinksHavingFocus() && !aContext.mIMEState.IsEditable()) {
    MOZ_LOG(sTextStoreLog, LogLevel::Debug,
            ("  TSFTextStore::SetInputContent() emulates blur for IME "
             "state change"));
    OnFocusChange(false, aWidget, aContext);
  }
}

// static
void TSFTextStore::MarkContextAsKeyboardDisabled(ITfContext* aContext) {
  VARIANT variant_int4_value1;
  variant_int4_value1.vt = VT_I4;
  variant_int4_value1.lVal = 1;

  RefPtr<ITfCompartment> comp;
  if (!GetCompartment(aContext, GUID_COMPARTMENT_KEYBOARD_DISABLED,
                      getter_AddRefs(comp))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("TSFTextStore::MarkContextAsKeyboardDisabled() failed"
             "aContext=0x%p...",
             aContext));
    return;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("TSFTextStore::MarkContextAsKeyboardDisabled(), setting "
           "to disable context 0x%p...",
           aContext));
  comp->SetValue(sClientId, &variant_int4_value1);
}

// static
void TSFTextStore::MarkContextAsEmpty(ITfContext* aContext) {
  VARIANT variant_int4_value1;
  variant_int4_value1.vt = VT_I4;
  variant_int4_value1.lVal = 1;

  RefPtr<ITfCompartment> comp;
  if (!GetCompartment(aContext, GUID_COMPARTMENT_EMPTYCONTEXT,
                      getter_AddRefs(comp))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("TSFTextStore::MarkContextAsEmpty() failed"
             "aContext=0x%p...",
             aContext));
    return;
  }

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("TSFTextStore::MarkContextAsEmpty(), setting "
           "to mark empty context 0x%p...",
           aContext));
  comp->SetValue(sClientId, &variant_int4_value1);
}

// static
void TSFTextStore::Initialize() {
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

  RefPtr<ITfThreadMgr> threadMgr;
  HRESULT hr =
      ::CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER,
                         IID_ITfThreadMgr, getter_AddRefs(threadMgr));
  if (FAILED(hr) || !threadMgr) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::Initialize() FAILED to "
             "create the thread manager, hr=0x%08X",
             hr));
    return;
  }

  hr = threadMgr->Activate(&sClientId);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::Initialize() FAILED to activate, hr=0x%08X", hr));
    return;
  }

  RefPtr<ITfDocumentMgr> disabledDocumentMgr;
  hr = threadMgr->CreateDocumentMgr(getter_AddRefs(disabledDocumentMgr));
  if (FAILED(hr) || !disabledDocumentMgr) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::Initialize() FAILED to create "
             "a document manager for disabled mode, hr=0x%08X",
             hr));
    return;
  }

  RefPtr<ITfContext> disabledContext;
  DWORD editCookie = 0;
  hr = disabledDocumentMgr->CreateContext(
      sClientId, 0, nullptr, getter_AddRefs(disabledContext), &editCookie);
  if (FAILED(hr) || !disabledContext) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::Initialize() FAILED to create "
             "a context for disabled mode, hr=0x%08X",
             hr));
    return;
  }

  MarkContextAsKeyboardDisabled(disabledContext);
  MarkContextAsEmpty(disabledContext);

  sThreadMgr = threadMgr;
  sDisabledDocumentMgr = disabledDocumentMgr;
  sDisabledContext = disabledContext;

  MOZ_LOG(sTextStoreLog, LogLevel::Info,
          ("  TSFTextStore::Initialize(), sThreadMgr=0x%p, "
           "sClientId=0x%08X, sDisabledDocumentMgr=0x%p, sDisabledContext=%p",
           sThreadMgr.get(), sClientId, sDisabledDocumentMgr.get(),
           sDisabledContext.get()));
}

// static
already_AddRefed<ITfThreadMgr> TSFTextStore::GetThreadMgr() {
  RefPtr<ITfThreadMgr> threadMgr = sThreadMgr;
  return threadMgr.forget();
}

// static
already_AddRefed<ITfMessagePump> TSFTextStore::GetMessagePump() {
  static bool sInitialized = false;
  if (!sThreadMgr) {
    return nullptr;
  }
  if (sMessagePump) {
    RefPtr<ITfMessagePump> messagePump = sMessagePump;
    return messagePump.forget();
  }
  // If it tried to retrieve ITfMessagePump from sThreadMgr but it failed,
  // we shouldn't retry it at every message due to performance reason.
  // Although this shouldn't occur actually.
  if (sInitialized) {
    return nullptr;
  }
  sInitialized = true;

  RefPtr<ITfMessagePump> messagePump;
  HRESULT hr = sThreadMgr->QueryInterface(IID_ITfMessagePump,
                                          getter_AddRefs(messagePump));
  if (NS_WARN_IF(FAILED(hr)) || NS_WARN_IF(!messagePump)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("TSFTextStore::GetMessagePump() FAILED to "
             "QI message pump from the thread manager, hr=0x%08X",
             hr));
    return nullptr;
  }
  sMessagePump = messagePump;
  return messagePump.forget();
}

// static
already_AddRefed<ITfDisplayAttributeMgr>
TSFTextStore::GetDisplayAttributeMgr() {
  RefPtr<ITfDisplayAttributeMgr> displayAttributeMgr;
  if (sDisplayAttrMgr) {
    displayAttributeMgr = sDisplayAttrMgr;
    return displayAttributeMgr.forget();
  }

  HRESULT hr = ::CoCreateInstance(
      CLSID_TF_DisplayAttributeMgr, nullptr, CLSCTX_INPROC_SERVER,
      IID_ITfDisplayAttributeMgr, getter_AddRefs(displayAttributeMgr));
  if (NS_WARN_IF(FAILED(hr)) || NS_WARN_IF(!displayAttributeMgr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("TSFTextStore::GetDisplayAttributeMgr() FAILED to create "
             "a display attribute manager instance, hr=0x%08X",
             hr));
    return nullptr;
  }
  sDisplayAttrMgr = displayAttributeMgr;
  return displayAttributeMgr.forget();
}

// static
already_AddRefed<ITfCategoryMgr> TSFTextStore::GetCategoryMgr() {
  RefPtr<ITfCategoryMgr> categoryMgr;
  if (sCategoryMgr) {
    categoryMgr = sCategoryMgr;
    return categoryMgr.forget();
  }
  HRESULT hr =
      ::CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                         IID_ITfCategoryMgr, getter_AddRefs(categoryMgr));
  if (NS_WARN_IF(FAILED(hr)) || NS_WARN_IF(!categoryMgr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("TSFTextStore::GetCategoryMgr() FAILED to create "
             "a category manager instance, hr=0x%08X",
             hr));
    return nullptr;
  }
  sCategoryMgr = categoryMgr;
  return categoryMgr.forget();
}

// static
already_AddRefed<ITfCompartment> TSFTextStore::GetCompartmentForOpenClose() {
  if (sCompartmentForOpenClose) {
    RefPtr<ITfCompartment> compartment = sCompartmentForOpenClose;
    return compartment.forget();
  }

  if (!sThreadMgr) {
    return nullptr;
  }

  RefPtr<ITfCompartmentMgr> compartmentMgr;
  HRESULT hr = sThreadMgr->QueryInterface(IID_ITfCompartmentMgr,
                                          getter_AddRefs(compartmentMgr));
  if (NS_WARN_IF(FAILED(hr)) || NS_WARN_IF(!compartmentMgr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("TSFTextStore::GetCompartmentForOpenClose() FAILED due to"
             "sThreadMgr not having ITfCompartmentMgr, hr=0x%08X",
             hr));
    return nullptr;
  }

  RefPtr<ITfCompartment> compartment;
  hr = compartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                                      getter_AddRefs(compartment));
  if (NS_WARN_IF(FAILED(hr)) || NS_WARN_IF(!compartment)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("TSFTextStore::GetCompartmentForOpenClose() FAILED due to"
             "ITfCompartmentMgr::GetCompartment() failuere, hr=0x%08X",
             hr));
    return nullptr;
  }

  sCompartmentForOpenClose = compartment;
  return compartment.forget();
}

// static
already_AddRefed<ITfInputProcessorProfiles>
TSFTextStore::GetInputProcessorProfiles() {
  RefPtr<ITfInputProcessorProfiles> inputProcessorProfiles;
  if (sInputProcessorProfiles) {
    inputProcessorProfiles = sInputProcessorProfiles;
    return inputProcessorProfiles.forget();
  }
  // XXX MSDN documents that ITfInputProcessorProfiles is available only on
  //     desktop apps.  However, there is no known way to obtain
  //     ITfInputProcessorProfileMgr instance without ITfInputProcessorProfiles
  //     instance.
  HRESULT hr = ::CoCreateInstance(
      CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
      IID_ITfInputProcessorProfiles, getter_AddRefs(inputProcessorProfiles));
  if (NS_WARN_IF(FAILED(hr)) || NS_WARN_IF(!inputProcessorProfiles)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("TSFTextStore::GetInputProcessorProfiles() FAILED to create input "
             "processor profiles, hr=0x%08X",
             hr));
    return nullptr;
  }
  sInputProcessorProfiles = inputProcessorProfiles;
  return inputProcessorProfiles.forget();
}

// static
void TSFTextStore::Terminate() {
  MOZ_LOG(sTextStoreLog, LogLevel::Info, ("TSFTextStore::Terminate()"));

  TSFStaticSink::Shutdown();

  sDisplayAttrMgr = nullptr;
  sCategoryMgr = nullptr;
  sEnabledTextStore = nullptr;
  sDisabledDocumentMgr = nullptr;
  sDisabledContext = nullptr;
  sCompartmentForOpenClose = nullptr;
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
bool TSFTextStore::ProcessRawKeyMessage(const MSG& aMsg) {
  if (!sThreadMgr) {
    return false;  // not in TSF mode
  }
  static bool sInitialized = false;
  if (!sKeystrokeMgr) {
    // If it tried to retrieve ITfKeystrokeMgr from sThreadMgr but it failed,
    // we shouldn't retry it at every keydown nor keyup due to performance
    // reason.  Although this shouldn't occur actually.
    if (sInitialized) {
      return false;
    }
    sInitialized = true;
    RefPtr<ITfKeystrokeMgr> keystrokeMgr;
    HRESULT hr = sThreadMgr->QueryInterface(IID_ITfKeystrokeMgr,
                                            getter_AddRefs(keystrokeMgr));
    if (NS_WARN_IF(FAILED(hr)) || NS_WARN_IF(!keystrokeMgr)) {
      MOZ_LOG(sTextStoreLog, LogLevel::Error,
              ("TSFTextStore::ProcessRawKeyMessage() FAILED to "
               "QI keystroke manager from the thread manager, hr=0x%08X",
               hr));
      return false;
    }
    sKeystrokeMgr = keystrokeMgr.forget();
  }

  if (aMsg.message == WM_KEYDOWN) {
    RefPtr<TSFTextStore> textStore(sEnabledTextStore);
    if (textStore) {
      textStore->OnStartToHandleKeyMessage();
      if (NS_WARN_IF(textStore != sEnabledTextStore)) {
        // Let's handle the key message with new focused TSFTextStore.
        textStore = sEnabledTextStore;
      }
    }
    AutoRestore<const MSG*> savePreviousKeyMsg(sHandlingKeyMsg);
    AutoRestore<bool> saveKeyEventDispatched(sIsKeyboardEventDispatched);
    sHandlingKeyMsg = &aMsg;
    sIsKeyboardEventDispatched = false;
    BOOL eaten;
    RefPtr<ITfKeystrokeMgr> keystrokeMgr = sKeystrokeMgr;
    HRESULT hr = keystrokeMgr->TestKeyDown(aMsg.wParam, aMsg.lParam, &eaten);
    if (FAILED(hr) || !sKeystrokeMgr || !eaten) {
      return false;
    }
    hr = keystrokeMgr->KeyDown(aMsg.wParam, aMsg.lParam, &eaten);
    if (textStore) {
      textStore->OnEndHandlingKeyMessage(!!eaten);
    }
    return SUCCEEDED(hr) &&
           (eaten || !sKeystrokeMgr || sIsKeyboardEventDispatched);
  }
  if (aMsg.message == WM_KEYUP) {
    RefPtr<TSFTextStore> textStore(sEnabledTextStore);
    if (textStore) {
      textStore->OnStartToHandleKeyMessage();
      if (NS_WARN_IF(textStore != sEnabledTextStore)) {
        // Let's handle the key message with new focused TSFTextStore.
        textStore = sEnabledTextStore;
      }
    }
    AutoRestore<const MSG*> savePreviousKeyMsg(sHandlingKeyMsg);
    AutoRestore<bool> saveKeyEventDispatched(sIsKeyboardEventDispatched);
    sHandlingKeyMsg = &aMsg;
    sIsKeyboardEventDispatched = false;
    BOOL eaten;
    RefPtr<ITfKeystrokeMgr> keystrokeMgr = sKeystrokeMgr;
    HRESULT hr = keystrokeMgr->TestKeyUp(aMsg.wParam, aMsg.lParam, &eaten);
    if (FAILED(hr) || !sKeystrokeMgr || !eaten) {
      return false;
    }
    hr = keystrokeMgr->KeyUp(aMsg.wParam, aMsg.lParam, &eaten);
    if (textStore) {
      textStore->OnEndHandlingKeyMessage(!!eaten);
    }
    return SUCCEEDED(hr) &&
           (eaten || !sKeystrokeMgr || sIsKeyboardEventDispatched);
  }
  return false;
}

// static
void TSFTextStore::ProcessMessage(nsWindowBase* aWindow, UINT aMessage,
                                  WPARAM& aWParam, LPARAM& aLParam,
                                  MSGResult& aResult) {
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
bool TSFTextStore::IsIMM_IMEActive() {
  return TSFStaticSink::IsIMM_IMEActive();
}

// static
bool TSFTextStore::IsMSJapaneseIMEActive() {
  return TSFStaticSink::IsMSJapaneseIMEActive();
}

// static
bool TSFTextStore::IsGoogleJapaneseInputActive() {
  return TSFStaticSink::IsGoogleJapaneseInputActive();
}

/******************************************************************************
 *  TSFTextStore::Content
 *****************************************************************************/

const nsDependentSubstring TSFTextStore::Content::GetSelectedText() const {
  if (NS_WARN_IF(mSelection.isNothing())) {
    return nsDependentSubstring();
  }
  return GetSubstring(static_cast<uint32_t>(mSelection->StartOffset()),
                      static_cast<uint32_t>(mSelection->Length()));
}

const nsDependentSubstring TSFTextStore::Content::GetSubstring(
    uint32_t aStart, uint32_t aLength) const {
  return nsDependentSubstring(mText, aStart, aLength);
}

void TSFTextStore::Content::ReplaceSelectedTextWith(const nsAString& aString) {
  if (NS_WARN_IF(mSelection.isNothing())) {
    return;
  }
  ReplaceTextWith(mSelection->StartOffset(), mSelection->Length(), aString);
}

inline uint32_t FirstDifferentCharOffset(const nsAString& aStr1,
                                         const nsAString& aStr2) {
  MOZ_ASSERT(aStr1 != aStr2);
  uint32_t i = 0;
  uint32_t minLength = std::min(aStr1.Length(), aStr2.Length());
  for (; i < minLength && aStr1[i] == aStr2[i]; i++) {
    /* nothing to do */
  }
  return i;
}

void TSFTextStore::Content::ReplaceTextWith(LONG aStart, LONG aLength,
                                            const nsAString& aReplaceString) {
  MOZ_ASSERT(aStart >= 0);
  MOZ_ASSERT(aLength >= 0);
  const nsDependentSubstring replacedString = GetSubstring(
      static_cast<uint32_t>(aStart), static_cast<uint32_t>(aLength));
  if (aReplaceString != replacedString) {
    uint32_t firstDifferentOffset = mMinModifiedOffset.valueOr(UINT32_MAX);
    if (mComposition.isSome()) {
      // Emulate text insertion during compositions, because during a
      // composition, editor expects the whole composition string to
      // be sent in eCompositionChange, not just the inserted part.
      // The actual eCompositionChange will be sent in SetSelection
      // or OnUpdateComposition.
      MOZ_ASSERT(aStart >= mComposition->StartOffset());
      MOZ_ASSERT(aStart + aLength <= mComposition->EndOffset());
      mComposition->ReplaceData(
          static_cast<uint32_t>(aStart - mComposition->StartOffset()),
          static_cast<uint32_t>(aLength), aReplaceString);
      // TIP may set composition string twice or more times during a document
      // lock.  Therefore, we should compute the first difference offset with
      // mLastComposition.
      if (mLastComposition.isNothing()) {
        firstDifferentOffset = mComposition->StartOffset();
      } else if (mComposition->DataRef() != mLastComposition->DataRef()) {
        firstDifferentOffset =
            mComposition->StartOffset() +
            FirstDifferentCharOffset(mComposition->DataRef(),
                                     mLastComposition->DataRef());
        // The previous change to the composition string is canceled.
        if (mMinModifiedOffset.isSome() &&
            mMinModifiedOffset.value() >=
                static_cast<uint32_t>(mComposition->StartOffset()) &&
            mMinModifiedOffset.value() < firstDifferentOffset) {
          mMinModifiedOffset = Some(firstDifferentOffset);
        }
      } else if (mMinModifiedOffset.isSome() &&
                 mMinModifiedOffset.value() < static_cast<uint32_t>(LONG_MAX) &&
                 mComposition->IsOffsetInRange(
                     static_cast<long>(mMinModifiedOffset.value()))) {
        // The previous change to the composition string is canceled.
        firstDifferentOffset = mComposition->EndOffset();
        mMinModifiedOffset = Some(firstDifferentOffset);
      }
      mLatestCompositionRange = Some(mComposition->CreateStartAndEndOffsets());
      MOZ_LOG(
          sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::Content::ReplaceTextWith(aStart=%d, "
           "aLength=%d, aReplaceString=\"%s\"), mComposition=%s, "
           "mLastComposition=%s, mMinModifiedOffset=%s, "
           "firstDifferentOffset=%u",
           this, aStart, aLength, GetEscapedUTF8String(aReplaceString).get(),
           ToString(mComposition).c_str(), ToString(mLastComposition).c_str(),
           ToString(mMinModifiedOffset).c_str(), firstDifferentOffset));
    } else {
      firstDifferentOffset =
          static_cast<uint32_t>(aStart) +
          FirstDifferentCharOffset(aReplaceString, replacedString);
    }
    mMinModifiedOffset =
        mMinModifiedOffset.isNothing()
            ? Some(firstDifferentOffset)
            : Some(std::min(mMinModifiedOffset.value(), firstDifferentOffset));
    mText.Replace(static_cast<uint32_t>(aStart), static_cast<uint32_t>(aLength),
                  aReplaceString);
  }
  // Selection should be collapsed at the end of the inserted string.
  mSelection = Some(TSFTextStore::Selection(static_cast<uint32_t>(aStart) +
                                            aReplaceString.Length()));
}

void TSFTextStore::Content::StartComposition(
    ITfCompositionView* aCompositionView, const PendingAction& aCompStart,
    bool aPreserveSelection) {
  MOZ_ASSERT(aCompositionView);
  MOZ_ASSERT(mComposition.isNothing());
  MOZ_ASSERT(aCompStart.mType == PendingAction::Type::eCompositionStart);

  mComposition.reset();  // Avoid new crash in the beta and nightly channels.
  mComposition.emplace(
      aCompositionView, aCompStart.mSelectionStart,
      GetSubstring(static_cast<uint32_t>(aCompStart.mSelectionStart),
                   static_cast<uint32_t>(aCompStart.mSelectionLength)));
  mLatestCompositionRange = Some(mComposition->CreateStartAndEndOffsets());
  if (!aPreserveSelection) {
    // XXX Do we need to set a new writing-mode here when setting a new
    // selection? Currently, we just preserve the existing value.
    WritingMode writingMode =
        mSelection.isNothing() ? WritingMode() : mSelection->GetWritingMode();
    mSelection = Some(TSFTextStore::Selection(mComposition->StartOffset(),
                                              mComposition->Length(), false,
                                              writingMode));
  }
}

void TSFTextStore::Content::RestoreCommittedComposition(
    ITfCompositionView* aCompositionView,
    const PendingAction& aCanceledCompositionEnd) {
  MOZ_ASSERT(aCompositionView);
  MOZ_ASSERT(mComposition.isNothing());
  MOZ_ASSERT(aCanceledCompositionEnd.mType ==
             PendingAction::Type::eCompositionEnd);
  MOZ_ASSERT(
      GetSubstring(
          static_cast<uint32_t>(aCanceledCompositionEnd.mSelectionStart),
          static_cast<uint32_t>(aCanceledCompositionEnd.mData.Length())) ==
      aCanceledCompositionEnd.mData);

  // Restore the committed string as composing string.
  mComposition.reset();  // Avoid new crash in the beta and nightly channels.
  mComposition.emplace(aCompositionView,
                       aCanceledCompositionEnd.mSelectionStart,
                       aCanceledCompositionEnd.mData);
  mLatestCompositionRange = Some(mComposition->CreateStartAndEndOffsets());
}

void TSFTextStore::Content::EndComposition(const PendingAction& aCompEnd) {
  MOZ_ASSERT(mComposition.isSome());
  MOZ_ASSERT(aCompEnd.mType == PendingAction::Type::eCompositionEnd);

  if (mComposition.isNothing()) {
    return;  // Avoid new crash in the beta and nightly channels.
  }

  mSelection = Some(TSFTextStore::Selection(mComposition->StartOffset() +
                                            aCompEnd.mData.Length()));
  mComposition.reset();
}

/******************************************************************************
 *  TSFTextStore::MouseTracker
 *****************************************************************************/

TSFTextStore::MouseTracker::MouseTracker() : mCookie(kInvalidCookie) {}

HRESULT
TSFTextStore::MouseTracker::Init(TSFTextStore* aTextStore) {
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::MouseTracker::Init(aTextStore=0x%p), "
           "aTextStore->mMouseTrackers.Length()=%d",
           this, aTextStore->mMouseTrackers.Length()));

  if (&aTextStore->mMouseTrackers.LastElement() != this) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::MouseTracker::Init() FAILED due to "
             "this is not the last element of mMouseTrackers",
             this));
    return E_FAIL;
  }
  if (aTextStore->mMouseTrackers.Length() > kInvalidCookie) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::MouseTracker::Init() FAILED due to "
             "no new cookie available",
             this));
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
                                       ITfMouseSink* aMouseSink) {
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::MouseTracker::AdviseSink(aTextStore=0x%p, "
           "aTextRange=0x%p, aMouseSink=0x%p), mCookie=%d, mSink=0x%p",
           this, aTextStore, aTextRange, aMouseSink, mCookie, mSink.get()));
  MOZ_ASSERT(mCookie != kInvalidCookie, "This hasn't been initalized?");

  if (mSink) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::MouseTracker::AdviseMouseSink() FAILED "
             "due to already being used",
             this));
    return E_FAIL;
  }

  MOZ_ASSERT(mRange.isNothing());

  LONG start = 0, length = 0;
  HRESULT hr = aTextRange->GetExtent(&start, &length);
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::MouseTracker::AdviseMouseSink() FAILED "
             "due to failure of ITfRangeACP::GetExtent()",
             this));
    return hr;
  }

  if (start < 0 || length <= 0 || start + length > LONG_MAX) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::MouseTracker::AdviseMouseSink() FAILED "
             "due to odd result of ITfRangeACP::GetExtent(), "
             "start=%d, length=%d",
             this, start, length));
    return E_INVALIDARG;
  }

  nsAutoString textContent;
  if (NS_WARN_IF(!aTextStore->GetCurrentText(textContent))) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::MouseTracker::AdviseMouseSink() FAILED "
             "due to failure of TSFTextStore::GetCurrentText()",
             this));
    return E_FAIL;
  }

  if (textContent.Length() <= static_cast<uint32_t>(start) ||
      textContent.Length() < static_cast<uint32_t>(start + length)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("0x%p   TSFTextStore::MouseTracker::AdviseMouseSink() FAILED "
             "due to out of range, start=%d, length=%d, "
             "textContent.Length()=%d",
             this, start, length, textContent.Length()));
    return E_INVALIDARG;
  }

  mRange.emplace(start, start + length);

  mSink = aMouseSink;

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::MouseTracker::AdviseMouseSink(), "
           "succeeded, mRange=%s, textContent.Length()=%d",
           this, ToString(mRange).c_str(), textContent.Length()));
  return S_OK;
}

void TSFTextStore::MouseTracker::UnadviseSink() {
  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::MouseTracker::UnadviseSink(), "
           "mCookie=%d, mSink=0x%p, mRange=%s",
           this, mCookie, mSink.get(), ToString(mRange).c_str()));
  mSink = nullptr;
  mRange.reset();
}

bool TSFTextStore::MouseTracker::OnMouseButtonEvent(ULONG aEdge,
                                                    ULONG aQuadrant,
                                                    DWORD aButtonStatus) {
  MOZ_ASSERT(IsUsing(), "The caller must check before calling OnMouseEvent()");

  BOOL eaten = FALSE;
  RefPtr<ITfMouseSink> sink = mSink;
  HRESULT hr = sink->OnMouseEvent(aEdge, aQuadrant, aButtonStatus, &eaten);

  MOZ_LOG(sTextStoreLog, LogLevel::Debug,
          ("0x%p   TSFTextStore::MouseTracker::OnMouseEvent(aEdge=%d, "
           "aQuadrant=%d, aButtonStatus=0x%08X), hr=0x%08X, eaten=%s",
           this, aEdge, aQuadrant, aButtonStatus, hr, GetBoolName(!!eaten)));

  return SUCCEEDED(hr) && eaten;
}

#ifdef DEBUG
// static
bool TSFTextStore::CurrentKeyboardLayoutHasIME() {
  RefPtr<ITfInputProcessorProfiles> inputProcessorProfiles =
      TSFTextStore::GetInputProcessorProfiles();
  if (!inputProcessorProfiles) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("TSFTextStore::CurrentKeyboardLayoutHasIME() FAILED due to "
             "there is no input processor profiles instance"));
    return false;
  }
  RefPtr<ITfInputProcessorProfileMgr> profileMgr;
  HRESULT hr = inputProcessorProfiles->QueryInterface(
      IID_ITfInputProcessorProfileMgr, getter_AddRefs(profileMgr));
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
    return false;  // not found or not active
  }
  if (FAILED(hr)) {
    MOZ_LOG(sTextStoreLog, LogLevel::Error,
            ("  TSFTextStore::CurrentKeyboardLayoutHasIME() FAILED to retreive "
             "active profile"));
    return false;
  }
  return (profile.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR);
}
#endif  // #ifdef DEBUG

}  // namespace widget
}  // namespace mozilla
