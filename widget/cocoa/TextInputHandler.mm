/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "TextInputHandler.h"

#include "mozilla/Logging.h"

#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"

#include "nsChildView.h"
#include "nsObjCExceptions.h"
#include "nsBidiUtils.h"
#include "nsToolkit.h"
#include "nsCocoaUtils.h"
#include "WidgetUtils.h"
#include "nsPrintfCString.h"
#include "mozilla/unused.h"
#include "mozilla/dom/ContentParent.h"
#include "ComplexTextInputPanel.h"

using namespace mozilla;
using namespace mozilla::widget;

PRLogModuleInfo* gLog = nullptr;

static const char*
OnOrOff(bool aBool)
{
  return aBool ? "ON" : "off";
}

static const char*
TrueOrFalse(bool aBool)
{
  return aBool ? "TRUE" : "FALSE";
}

static const char*
GetKeyNameForNativeKeyCode(unsigned short aNativeKeyCode)
{
  switch (aNativeKeyCode) {
    case kVK_Escape:              return "Escape";
    case kVK_RightCommand:        return "Right-Command";
    case kVK_Command:             return "Command";
    case kVK_Shift:               return "Shift";
    case kVK_CapsLock:            return "CapsLock";
    case kVK_Option:              return "Option";
    case kVK_Control:             return "Control";
    case kVK_RightShift:          return "Right-Shift";
    case kVK_RightOption:         return "Right-Option";
    case kVK_RightControl:        return "Right-Control";
    case kVK_ANSI_KeypadClear:    return "Clear";

    case kVK_F1:                  return "F1";
    case kVK_F2:                  return "F2";
    case kVK_F3:                  return "F3";
    case kVK_F4:                  return "F4";
    case kVK_F5:                  return "F5";
    case kVK_F6:                  return "F6";
    case kVK_F7:                  return "F7";
    case kVK_F8:                  return "F8";
    case kVK_F9:                  return "F9";
    case kVK_F10:                 return "F10";
    case kVK_F11:                 return "F11";
    case kVK_F12:                 return "F12";
    case kVK_F13:                 return "F13/PrintScreen";
    case kVK_F14:                 return "F14/ScrollLock";
    case kVK_F15:                 return "F15/Pause";

    case kVK_ANSI_Keypad0:        return "NumPad-0";
    case kVK_ANSI_Keypad1:        return "NumPad-1";
    case kVK_ANSI_Keypad2:        return "NumPad-2";
    case kVK_ANSI_Keypad3:        return "NumPad-3";
    case kVK_ANSI_Keypad4:        return "NumPad-4";
    case kVK_ANSI_Keypad5:        return "NumPad-5";
    case kVK_ANSI_Keypad6:        return "NumPad-6";
    case kVK_ANSI_Keypad7:        return "NumPad-7";
    case kVK_ANSI_Keypad8:        return "NumPad-8";
    case kVK_ANSI_Keypad9:        return "NumPad-9";

    case kVK_ANSI_KeypadMultiply: return "NumPad-*";
    case kVK_ANSI_KeypadPlus:     return "NumPad-+";
    case kVK_ANSI_KeypadMinus:    return "NumPad--";
    case kVK_ANSI_KeypadDecimal:  return "NumPad-.";
    case kVK_ANSI_KeypadDivide:   return "NumPad-/";
    case kVK_ANSI_KeypadEquals:   return "NumPad-=";
    case kVK_ANSI_KeypadEnter:    return "NumPad-Enter";
    case kVK_Return:              return "Return";
    case kVK_Powerbook_KeypadEnter: return "NumPad-EnterOnPowerBook";

    case kVK_PC_Insert:           return "Insert/Help";
    case kVK_PC_Delete:           return "Delete";
    case kVK_Tab:                 return "Tab";
    case kVK_PC_Backspace:        return "Backspace";
    case kVK_Home:                return "Home";
    case kVK_End:                 return "End";
    case kVK_PageUp:              return "PageUp";
    case kVK_PageDown:            return "PageDown";
    case kVK_LeftArrow:           return "LeftArrow";
    case kVK_RightArrow:          return "RightArrow";
    case kVK_UpArrow:             return "UpArrow";
    case kVK_DownArrow:           return "DownArrow";
    case kVK_PC_ContextMenu:      return "ContextMenu";

    case kVK_Function:            return "Function";
    case kVK_VolumeUp:            return "VolumeUp";
    case kVK_VolumeDown:          return "VolumeDown";
    case kVK_Mute:                return "Mute";

    case kVK_ISO_Section:         return "ISO_Section";

    case kVK_JIS_Yen:             return "JIS_Yen";
    case kVK_JIS_Underscore:      return "JIS_Underscore";
    case kVK_JIS_KeypadComma:     return "JIS_KeypadComma";
    case kVK_JIS_Eisu:            return "JIS_Eisu";
    case kVK_JIS_Kana:            return "JIS_Kana";

    case kVK_ANSI_A:              return "A";
    case kVK_ANSI_B:              return "B";
    case kVK_ANSI_C:              return "C";
    case kVK_ANSI_D:              return "D";
    case kVK_ANSI_E:              return "E";
    case kVK_ANSI_F:              return "F";
    case kVK_ANSI_G:              return "G";
    case kVK_ANSI_H:              return "H";
    case kVK_ANSI_I:              return "I";
    case kVK_ANSI_J:              return "J";
    case kVK_ANSI_K:              return "K";
    case kVK_ANSI_L:              return "L";
    case kVK_ANSI_M:              return "M";
    case kVK_ANSI_N:              return "N";
    case kVK_ANSI_O:              return "O";
    case kVK_ANSI_P:              return "P";
    case kVK_ANSI_Q:              return "Q";
    case kVK_ANSI_R:              return "R";
    case kVK_ANSI_S:              return "S";
    case kVK_ANSI_T:              return "T";
    case kVK_ANSI_U:              return "U";
    case kVK_ANSI_V:              return "V";
    case kVK_ANSI_W:              return "W";
    case kVK_ANSI_X:              return "X";
    case kVK_ANSI_Y:              return "Y";
    case kVK_ANSI_Z:              return "Z";

    case kVK_ANSI_1:              return "1";
    case kVK_ANSI_2:              return "2";
    case kVK_ANSI_3:              return "3";
    case kVK_ANSI_4:              return "4";
    case kVK_ANSI_5:              return "5";
    case kVK_ANSI_6:              return "6";
    case kVK_ANSI_7:              return "7";
    case kVK_ANSI_8:              return "8";
    case kVK_ANSI_9:              return "9";
    case kVK_ANSI_0:              return "0";
    case kVK_ANSI_Equal:          return "Equal";
    case kVK_ANSI_Minus:          return "Minus";
    case kVK_ANSI_RightBracket:   return "RightBracket";
    case kVK_ANSI_LeftBracket:    return "LeftBracket";
    case kVK_ANSI_Quote:          return "Quote";
    case kVK_ANSI_Semicolon:      return "Semicolon";
    case kVK_ANSI_Backslash:      return "Backslash";
    case kVK_ANSI_Comma:          return "Comma";
    case kVK_ANSI_Slash:          return "Slash";
    case kVK_ANSI_Period:         return "Period";
    case kVK_ANSI_Grave:          return "Grave";

    default:                      return "undefined";
  }
}

static const char*
GetCharacters(const NSString* aString)
{
  nsAutoString str;
  nsCocoaUtils::GetStringForNSString(aString, str);
  if (str.IsEmpty()) {
    return "";
  }

  nsAutoString escapedStr;
  for (uint32_t i = 0; i < str.Length(); i++) {
    char16_t ch = str[i];
    if (ch < 0x20) {
      nsPrintfCString utf8str("(U+%04X)", ch);
      escapedStr += NS_ConvertUTF8toUTF16(utf8str);
    } else if (ch <= 0x7E) {
      escapedStr += ch;
    } else {
      nsPrintfCString utf8str("(U+%04X)", ch);
      escapedStr += ch;
      escapedStr += NS_ConvertUTF8toUTF16(utf8str);
    }
  }

  // the result will be freed automatically by cocoa.
  NSString* result = nsCocoaUtils::ToNSString(escapedStr);
  return [result UTF8String];
}

static const char*
GetCharacters(const CFStringRef aString)
{
  const NSString* str = reinterpret_cast<const NSString*>(aString);
  return GetCharacters(str);
}

static const char*
GetNativeKeyEventType(NSEvent* aNativeEvent)
{
  switch ([aNativeEvent type]) {
    case NSKeyDown:      return "NSKeyDown";
    case NSKeyUp:        return "NSKeyUp";
    case NSFlagsChanged: return "NSFlagsChanged";
    default:             return "not key event";
  }
}

static const char*
GetGeckoKeyEventType(const WidgetEvent& aEvent)
{
  switch (aEvent.mMessage) {
    case NS_KEY_DOWN:    return "NS_KEY_DOWN";
    case NS_KEY_UP:      return "NS_KEY_UP";
    case NS_KEY_PRESS:   return "NS_KEY_PRESS";
    default:             return "not key event";
  }
}

static const char*
GetRangeTypeName(uint32_t aRangeType)
{
  switch (aRangeType) {
    case NS_TEXTRANGE_RAWINPUT:
      return "NS_TEXTRANGE_RAWINPUT";
    case NS_TEXTRANGE_CONVERTEDTEXT:
      return "NS_TEXTRANGE_CONVERTEDTEXT";
    case NS_TEXTRANGE_SELECTEDRAWTEXT:
      return "NS_TEXTRANGE_SELECTEDRAWTEXT";
    case NS_TEXTRANGE_SELECTEDCONVERTEDTEXT:
      return "NS_TEXTRANGE_SELECTEDCONVERTEDTEXT";
    case NS_TEXTRANGE_CARETPOSITION:
      return "NS_TEXTRANGE_CARETPOSITION";
    default:
      return "invalid range type";
  }
}

static const char*
GetWindowLevelName(NSInteger aWindowLevel)
{
  switch (aWindowLevel) {
    case kCGBaseWindowLevelKey:
      return "kCGBaseWindowLevelKey (NSNormalWindowLevel)";
    case kCGMinimumWindowLevelKey:
      return "kCGMinimumWindowLevelKey";
    case kCGDesktopWindowLevelKey:
      return "kCGDesktopWindowLevelKey";
    case kCGBackstopMenuLevelKey:
      return "kCGBackstopMenuLevelKey";
    case kCGNormalWindowLevelKey:
      return "kCGNormalWindowLevelKey";
    case kCGFloatingWindowLevelKey:
      return "kCGFloatingWindowLevelKey (NSFloatingWindowLevel)";
    case kCGTornOffMenuWindowLevelKey:
      return "kCGTornOffMenuWindowLevelKey (NSSubmenuWindowLevel, NSTornOffMenuWindowLevel)";
    case kCGDockWindowLevelKey:
      return "kCGDockWindowLevelKey (NSDockWindowLevel)";
    case kCGMainMenuWindowLevelKey:
      return "kCGMainMenuWindowLevelKey (NSMainMenuWindowLevel)";
    case kCGStatusWindowLevelKey:
      return "kCGStatusWindowLevelKey (NSStatusWindowLevel)";
    case kCGModalPanelWindowLevelKey:
      return "kCGModalPanelWindowLevelKey (NSModalPanelWindowLevel)";
    case kCGPopUpMenuWindowLevelKey:
      return "kCGPopUpMenuWindowLevelKey (NSPopUpMenuWindowLevel)";
    case kCGDraggingWindowLevelKey:
      return "kCGDraggingWindowLevelKey";
    case kCGScreenSaverWindowLevelKey:
      return "kCGScreenSaverWindowLevelKey (NSScreenSaverWindowLevel)";
    case kCGMaximumWindowLevelKey:
      return "kCGMaximumWindowLevelKey";
    case kCGOverlayWindowLevelKey:
      return "kCGOverlayWindowLevelKey";
    case kCGHelpWindowLevelKey:
      return "kCGHelpWindowLevelKey";
    case kCGUtilityWindowLevelKey:
      return "kCGUtilityWindowLevelKey";
    case kCGDesktopIconWindowLevelKey:
      return "kCGDesktopIconWindowLevelKey";
    case kCGCursorWindowLevelKey:
      return "kCGCursorWindowLevelKey";
    case kCGNumberOfWindowLevelKeys:
      return "kCGNumberOfWindowLevelKeys";
    default:
      return "unknown window level";
  }
}

static bool
IsControlChar(uint32_t aCharCode)
{
  return aCharCode < ' ' || aCharCode == 0x7F;
}

static uint32_t gHandlerInstanceCount = 0;
static TISInputSourceWrapper gCurrentInputSource;

static void
InitLogModule()
{
  // Clear() is always called when TISInputSourceWrappper is created.
  if (!gLog) {
    gLog = PR_NewLogModule("TextInputHandlerWidgets");
    TextInputHandler::DebugPrintAllKeyboardLayouts();
    IMEInputHandler::DebugPrintAllIMEModes();
  }
}

static void
InitCurrentInputSource()
{
  if (gHandlerInstanceCount > 0 &&
      !gCurrentInputSource.IsInitializedByCurrentInputSource()) {
    gCurrentInputSource.InitByCurrentInputSource();
  }
}

static void
FinalizeCurrentInputSource()
{
  gCurrentInputSource.Clear();
}


#pragma mark -


/******************************************************************************
 *
 *  TISInputSourceWrapper implementation
 *
 ******************************************************************************/

// static
TISInputSourceWrapper&
TISInputSourceWrapper::CurrentInputSource()
{
  InitCurrentInputSource();
  return gCurrentInputSource;
}

bool
TISInputSourceWrapper::TranslateToString(UInt32 aKeyCode, UInt32 aModifiers,
                                         UInt32 aKbType, nsAString &aStr)
{
  aStr.Truncate();

  const UCKeyboardLayout* UCKey = GetUCKeyboardLayout();

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TISInputSourceWrapper::TranslateToString, aKeyCode=0x%X, "
     "aModifiers=0x%X, aKbType=0x%X UCKey=%p\n    "
     "Shift: %s, Ctrl: %s, Opt: %s, Cmd: %s, CapsLock: %s, NumLock: %s",
     this, aKeyCode, aModifiers, aKbType, UCKey,
     OnOrOff(aModifiers & shiftKey), OnOrOff(aModifiers & controlKey),
     OnOrOff(aModifiers & optionKey), OnOrOff(aModifiers & cmdKey),
     OnOrOff(aModifiers & alphaLock),
     OnOrOff(aModifiers & kEventKeyModifierNumLockMask)));

  NS_ENSURE_TRUE(UCKey, false);

  UInt32 deadKeyState = 0;
  UniCharCount len;
  UniChar chars[5];
  OSStatus err = ::UCKeyTranslate(UCKey, aKeyCode,
                                  kUCKeyActionDown, aModifiers >> 8,
                                  aKbType, kUCKeyTranslateNoDeadKeysMask,
                                  &deadKeyState, 5, &len, chars);

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TISInputSourceWrapper::TranslateToString, err=0x%X, len=%llu",
     this, err, len));

  NS_ENSURE_TRUE(err == noErr, false);
  if (len == 0) {
    return true;
  }
  NS_ENSURE_TRUE(EnsureStringLength(aStr, len), false);
  NS_ASSERTION(sizeof(char16_t) == sizeof(UniChar),
               "size of char16_t and size of UniChar are different");
  memcpy(aStr.BeginWriting(), chars, len * sizeof(char16_t));

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TISInputSourceWrapper::TranslateToString, aStr=\"%s\"",
     this, NS_ConvertUTF16toUTF8(aStr).get()));

  return true;
}

uint32_t
TISInputSourceWrapper::TranslateToChar(UInt32 aKeyCode, UInt32 aModifiers,
                                       UInt32 aKbType)
{
  nsAutoString str;
  if (!TranslateToString(aKeyCode, aModifiers, aKbType, str) ||
      str.Length() != 1) {
    return 0;
  }
  return static_cast<uint32_t>(str.CharAt(0));
}

void
TISInputSourceWrapper::InitByInputSourceID(const char* aID)
{
  Clear();
  if (!aID)
    return;

  CFStringRef idstr = ::CFStringCreateWithCString(kCFAllocatorDefault, aID,
                                                  kCFStringEncodingASCII);
  InitByInputSourceID(idstr);
  ::CFRelease(idstr);
}

void
TISInputSourceWrapper::InitByInputSourceID(const nsAFlatString &aID)
{
  Clear();
  if (aID.IsEmpty())
    return;
  CFStringRef idstr = ::CFStringCreateWithCharacters(kCFAllocatorDefault,
                                                     reinterpret_cast<const UniChar*>(aID.get()),
                                                     aID.Length());
  InitByInputSourceID(idstr);
  ::CFRelease(idstr);
}

void
TISInputSourceWrapper::InitByInputSourceID(const CFStringRef aID)
{
  Clear();
  if (!aID)
    return;
  const void* keys[] = { kTISPropertyInputSourceID };
  const void* values[] = { aID };
  CFDictionaryRef filter =
  ::CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, NULL, NULL);
  NS_ASSERTION(filter, "failed to create the filter");
  mInputSourceList = ::TISCreateInputSourceList(filter, true);
  ::CFRelease(filter);
  if (::CFArrayGetCount(mInputSourceList) > 0) {
    mInputSource = static_cast<TISInputSourceRef>(
      const_cast<void *>(::CFArrayGetValueAtIndex(mInputSourceList, 0)));
    if (IsKeyboardLayout()) {
      mKeyboardLayout = mInputSource;
    }
  }
}

void
TISInputSourceWrapper::InitByLayoutID(SInt32 aLayoutID,
                                      bool aOverrideKeyboard)
{
  // NOTE: Doument new layout IDs in TextInputHandler.h when you add ones.
  switch (aLayoutID) {
    case 0:
      InitByInputSourceID("com.apple.keylayout.US");
      break;
    case 1:
      InitByInputSourceID("com.apple.keylayout.Greek");
      break;
    case 2:
      InitByInputSourceID("com.apple.keylayout.German");
      break;
    case 3:
      InitByInputSourceID("com.apple.keylayout.Swedish-Pro");
      break;
    case 4:
      InitByInputSourceID("com.apple.keylayout.DVORAK-QWERTYCMD");
      break;
    case 5:
      InitByInputSourceID("com.apple.keylayout.Thai");
      break;
    case 6:
      InitByInputSourceID("com.apple.keylayout.Arabic");
      break;
    case 7:
      InitByInputSourceID("com.apple.keylayout.French");
      break;
    case 8:
      InitByInputSourceID("com.apple.keylayout.Hebrew");
      break;
    case 9:
      InitByInputSourceID("com.apple.keylayout.Lithuanian");
      break;
    case 10:
      InitByInputSourceID("com.apple.keylayout.Norwegian");
      break;
    case 11:
      InitByInputSourceID("com.apple.keylayout.Spanish");
      break;
    default:
      Clear();
      break;
  }
  mOverrideKeyboard = aOverrideKeyboard;
}

void
TISInputSourceWrapper::InitByCurrentInputSource()
{
  Clear();
  mInputSource = ::TISCopyCurrentKeyboardInputSource();
  mKeyboardLayout = ::TISCopyInputMethodKeyboardLayoutOverride();
  if (!mKeyboardLayout) {
    mKeyboardLayout = ::TISCopyCurrentKeyboardLayoutInputSource();
  }
  // If this causes composition, the current keyboard layout may input non-ASCII
  // characters such as Japanese Kana characters or Hangul characters.
  // However, we need to set ASCII characters to DOM key events for consistency
  // with other platforms.
  if (IsOpenedIMEMode()) {
    TISInputSourceWrapper tis(mKeyboardLayout);
    if (!tis.IsASCIICapable()) {
      mKeyboardLayout =
        ::TISCopyCurrentASCIICapableKeyboardLayoutInputSource();
    }
  }
}

void
TISInputSourceWrapper::InitByCurrentKeyboardLayout()
{
  Clear();
  mInputSource = ::TISCopyCurrentKeyboardLayoutInputSource();
  mKeyboardLayout = mInputSource;
}

void
TISInputSourceWrapper::InitByCurrentASCIICapableInputSource()
{
  Clear();
  mInputSource = ::TISCopyCurrentASCIICapableKeyboardInputSource();
  mKeyboardLayout = ::TISCopyInputMethodKeyboardLayoutOverride();
  if (mKeyboardLayout) {
    TISInputSourceWrapper tis(mKeyboardLayout);
    if (!tis.IsASCIICapable()) {
      mKeyboardLayout = nullptr;
    }
  }
  if (!mKeyboardLayout) {
    mKeyboardLayout =
      ::TISCopyCurrentASCIICapableKeyboardLayoutInputSource();
  }
}

void
TISInputSourceWrapper::InitByCurrentASCIICapableKeyboardLayout()
{
  Clear();
  mInputSource = ::TISCopyCurrentASCIICapableKeyboardLayoutInputSource();
  mKeyboardLayout = mInputSource;
}

void
TISInputSourceWrapper::InitByCurrentInputMethodKeyboardLayoutOverride()
{
  Clear();
  mInputSource = ::TISCopyInputMethodKeyboardLayoutOverride();
  mKeyboardLayout = mInputSource;
}

void
TISInputSourceWrapper::InitByTISInputSourceRef(TISInputSourceRef aInputSource)
{
  Clear();
  mInputSource = aInputSource;
  if (IsKeyboardLayout()) {
    mKeyboardLayout = mInputSource;
  }
}

void
TISInputSourceWrapper::InitByLanguage(CFStringRef aLanguage)
{
  Clear();
  mInputSource = ::TISCopyInputSourceForLanguage(aLanguage);
  if (IsKeyboardLayout()) {
    mKeyboardLayout = mInputSource;
  }
}

const UCKeyboardLayout*
TISInputSourceWrapper::GetUCKeyboardLayout()
{
  NS_ENSURE_TRUE(mKeyboardLayout, nullptr);
  if (mUCKeyboardLayout) {
    return mUCKeyboardLayout;
  }
  CFDataRef uchr = static_cast<CFDataRef>(
    ::TISGetInputSourceProperty(mKeyboardLayout,
                                kTISPropertyUnicodeKeyLayoutData));

  // We should be always able to get the layout here.
  NS_ENSURE_TRUE(uchr, nullptr);
  mUCKeyboardLayout =
    reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(uchr));
  return mUCKeyboardLayout;
}

bool
TISInputSourceWrapper::GetBoolProperty(const CFStringRef aKey)
{
  CFBooleanRef ret = static_cast<CFBooleanRef>(
    ::TISGetInputSourceProperty(mInputSource, aKey));
  return ::CFBooleanGetValue(ret);
}

bool
TISInputSourceWrapper::GetStringProperty(const CFStringRef aKey,
                                         CFStringRef &aStr)
{
  aStr = static_cast<CFStringRef>(
    ::TISGetInputSourceProperty(mInputSource, aKey));
  return aStr != nullptr;
}

bool
TISInputSourceWrapper::GetStringProperty(const CFStringRef aKey,
                                         nsAString &aStr)
{
  CFStringRef str;
  GetStringProperty(aKey, str);
  nsCocoaUtils::GetStringForNSString((const NSString*)str, aStr);
  return !aStr.IsEmpty();
}

bool
TISInputSourceWrapper::IsOpenedIMEMode()
{
  NS_ENSURE_TRUE(mInputSource, false);
  if (!IsIMEMode())
    return false;
  return !IsASCIICapable();
}

bool
TISInputSourceWrapper::IsIMEMode()
{
  NS_ENSURE_TRUE(mInputSource, false);
  CFStringRef str;
  GetInputSourceType(str);
  NS_ENSURE_TRUE(str, false);
  return ::CFStringCompare(kTISTypeKeyboardInputMode,
                           str, 0) == kCFCompareEqualTo;
}

bool
TISInputSourceWrapper::IsKeyboardLayout()
{
  NS_ENSURE_TRUE(mInputSource, false);
  CFStringRef str;
  GetInputSourceType(str);
  NS_ENSURE_TRUE(str, false);
  return ::CFStringCompare(kTISTypeKeyboardLayout,
                           str, 0) == kCFCompareEqualTo;
}

bool
TISInputSourceWrapper::GetLanguageList(CFArrayRef &aLanguageList)
{
  NS_ENSURE_TRUE(mInputSource, false);
  aLanguageList = static_cast<CFArrayRef>(
    ::TISGetInputSourceProperty(mInputSource,
                                kTISPropertyInputSourceLanguages));
  return aLanguageList != nullptr;
}

bool
TISInputSourceWrapper::GetPrimaryLanguage(CFStringRef &aPrimaryLanguage)
{
  NS_ENSURE_TRUE(mInputSource, false);
  CFArrayRef langList;
  NS_ENSURE_TRUE(GetLanguageList(langList), false);
  if (::CFArrayGetCount(langList) == 0)
    return false;
  aPrimaryLanguage =
    static_cast<CFStringRef>(::CFArrayGetValueAtIndex(langList, 0));
  return aPrimaryLanguage != nullptr;
}

bool
TISInputSourceWrapper::GetPrimaryLanguage(nsAString &aPrimaryLanguage)
{
  NS_ENSURE_TRUE(mInputSource, false);
  CFStringRef primaryLanguage;
  NS_ENSURE_TRUE(GetPrimaryLanguage(primaryLanguage), false);
  nsCocoaUtils::GetStringForNSString((const NSString*)primaryLanguage,
                                     aPrimaryLanguage);
  return !aPrimaryLanguage.IsEmpty();
}

bool
TISInputSourceWrapper::IsForRTLLanguage()
{
  if (mIsRTL < 0) {
    // Get the input character of the 'A' key of ANSI keyboard layout.
    nsAutoString str;
    bool ret = TranslateToString(kVK_ANSI_A, 0, eKbdType_ANSI, str);
    NS_ENSURE_TRUE(ret, ret);
    char16_t ch = str.IsEmpty() ? char16_t(0) : str.CharAt(0);
    mIsRTL = UCS2_CHAR_IS_BIDI(ch) || ch == 0xD802 || ch == 0xD803;
  }
  return mIsRTL != 0;
}

bool
TISInputSourceWrapper::IsInitializedByCurrentInputSource()
{
  return mInputSource == ::TISCopyCurrentKeyboardInputSource();
}

void
TISInputSourceWrapper::Select()
{
  if (!mInputSource)
    return;
  ::TISSelectInputSource(mInputSource);
}

void
TISInputSourceWrapper::Clear()
{
  // Clear() is always called when TISInputSourceWrappper is created.
  InitLogModule();

  if (mInputSourceList) {
    ::CFRelease(mInputSourceList);
  }
  mInputSourceList = nullptr;
  mInputSource = nullptr;
  mKeyboardLayout = nullptr;
  mIsRTL = -1;
  mUCKeyboardLayout = nullptr;
  mOverrideKeyboard = false;
}

void
TISInputSourceWrapper::InitKeyEvent(NSEvent *aNativeKeyEvent,
                                    WidgetKeyboardEvent& aKeyEvent,
                                    const nsAString *aInsertString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TISInputSourceWrapper::InitKeyEvent, aNativeKeyEvent=%p, "
     "aKeyEvent.mMessage=%s, aInsertString=%p, IsOpenedIMEMode()=%s",
     this, aNativeKeyEvent, GetGeckoKeyEventType(aKeyEvent), aInsertString,
     TrueOrFalse(IsOpenedIMEMode())));

  NS_ENSURE_TRUE(aNativeKeyEvent, );

  nsCocoaUtils::InitInputEvent(aKeyEvent, aNativeKeyEvent);

  // This is used only while dispatching the event (which is a synchronous
  // call), so there is no need to retain and release this data.
  aKeyEvent.mNativeKeyEvent = aNativeKeyEvent;

  // Fill in fields used for Cocoa NPAPI plugins
  if ([aNativeKeyEvent type] == NSKeyDown ||
      [aNativeKeyEvent type] == NSKeyUp) {
    aKeyEvent.mNativeKeyCode = [aNativeKeyEvent keyCode];
    aKeyEvent.mNativeModifierFlags = [aNativeKeyEvent modifierFlags];
    nsAutoString nativeChars;
    nsCocoaUtils::GetStringForNSString([aNativeKeyEvent characters], nativeChars);
    aKeyEvent.mNativeCharacters.Assign(nativeChars);
    nsAutoString nativeCharsIgnoringModifiers;
    nsCocoaUtils::GetStringForNSString([aNativeKeyEvent charactersIgnoringModifiers], nativeCharsIgnoringModifiers);
    aKeyEvent.mNativeCharactersIgnoringModifiers.Assign(nativeCharsIgnoringModifiers);
  } else if ([aNativeKeyEvent type] == NSFlagsChanged) {
    aKeyEvent.mNativeKeyCode = [aNativeKeyEvent keyCode];
    aKeyEvent.mNativeModifierFlags = [aNativeKeyEvent modifierFlags];
  }

  aKeyEvent.refPoint = LayoutDeviceIntPoint(0, 0);

  // If a keyboard layout override is set, we also need to force the keyboard
  // type to something ANSI to avoid test failures on machines with JIS
  // keyboards (since the pair of keyboard layout and physical keyboard type
  // form the actual key layout).  This assumes that the test setting the
  // override was written assuming an ANSI keyboard.
  UInt32 kbType = mOverrideKeyboard ? eKbdType_ANSI : ::LMGetKbdType();

  UInt32 nativeKeyCode = [aNativeKeyEvent keyCode];

  bool isPrintableKey = !TextInputHandler::IsSpecialGeckoKey(nativeKeyCode);
  if (isPrintableKey &&
      [aNativeKeyEvent type] != NSKeyDown &&
      [aNativeKeyEvent type] != NSKeyUp) {
    NS_WARNING("Why the printable key doesn't cause NSKeyDown or NSKeyUp?");
    isPrintableKey = false;
  }

  // Decide what string will be input.
  nsAutoString insertString;
  if (aInsertString) {
    // If the caller expects that the aInsertString will be input, we shouldn't
    // change it.
    insertString = *aInsertString;
  } else if (isPrintableKey) {
    // If IME is open, [aNativeKeyEvent characters] may be a character
    // which will be appended to the composition string.  However, especially,
    // while IME is disabled, most users and developers expect the key event
    // works as IME closed.  So, we should compute the insertString with
    // the ASCII capable keyboard layout.
    // NOTE: Such keyboard layouts typically change the layout to its ASCII
    //       capable layout when Command key is pressed.  And we don't worry
    //       when Control key is pressed too because it causes inputting
    //       control characters.
    if (!aKeyEvent.IsMeta() && !aKeyEvent.IsControl() && IsOpenedIMEMode()) {
      UInt32 state =
        nsCocoaUtils::ConvertToCarbonModifier([aNativeKeyEvent modifierFlags]);
      uint32_t ch = TranslateToChar(nativeKeyCode, state, kbType);
      if (ch) {
        insertString = ch;
      }
    } else {
      // If the caller isn't sure what string will be input, let's use
      // characters of NSEvent.
      nsCocoaUtils::GetStringForNSString([aNativeKeyEvent characters],
                                         insertString);
    }

    // If control key is pressed and the eventChars is a non-printable control
    // character, we should convert it to ASCII alphabet.
    if (aKeyEvent.IsControl() &&
        !insertString.IsEmpty() && insertString[0] <= char16_t(26)) {
      insertString = (aKeyEvent.IsShift() ^ aKeyEvent.IsCapsLocked()) ?
        static_cast<char16_t>(insertString[0] + ('A' - 1)) :
        static_cast<char16_t>(insertString[0] + ('a' - 1));
    }
    // If Meta key is pressed, it may cause to switch the keyboard layout like
    // Arabic, Russian, Hebrew, Greek and Dvorak-QWERTY.
    else if (aKeyEvent.IsMeta() &&
             !(aKeyEvent.IsControl() || aKeyEvent.IsAlt())) {
      UInt32 numLockState =
        aKeyEvent.IsNumLocked() ? kEventKeyModifierNumLockMask : 0;
      UInt32 capsLockState = aKeyEvent.IsCapsLocked() ? alphaLock : 0;
      UInt32 shiftState = aKeyEvent.IsShift() ? shiftKey : 0;
      uint32_t uncmdedChar =
        TranslateToChar(nativeKeyCode, numLockState, kbType);
      uint32_t cmdedChar =
        TranslateToChar(nativeKeyCode, cmdKey | numLockState, kbType);
      // If we can make a good guess at the characters that the user would
      // expect this key combination to produce (with and without Shift) then
      // use those characters.  This also corrects for CapsLock.
      uint32_t ch = 0;
      if (uncmdedChar == cmdedChar) {
        // The characters produced with Command seem similar to those without
        // Command.
        ch = TranslateToChar(nativeKeyCode,
                             shiftState | capsLockState | numLockState, kbType);
      } else {
        TISInputSourceWrapper USLayout("com.apple.keylayout.US");
        uint32_t uncmdedUSChar =
          USLayout.TranslateToChar(nativeKeyCode, numLockState, kbType);
        // If it looks like characters from US keyboard layout when Command key
        // is pressed, we should compute a character in the layout.
        if (uncmdedUSChar == cmdedChar) {
          ch = USLayout.TranslateToChar(nativeKeyCode,
                          shiftState | capsLockState | numLockState, kbType);
        }
      }

      // If there is a more preferred character for the commanded key event,
      // we should use it.
      if (ch) {
        insertString = ch;
      }
    }
  }

  // Remove control characters which shouldn't be inputted on editor.
  // XXX Currently, we don't find any cases inserting control characters with
  //     printable character.  So, just checking first character is enough.
  if (!insertString.IsEmpty() && IsControlChar(insertString[0])) {
    insertString.Truncate();
  }

  aKeyEvent.keyCode =
    ComputeGeckoKeyCode(nativeKeyCode, kbType, aKeyEvent.IsMeta());

  switch (nativeKeyCode) {
    case kVK_Command:
    case kVK_Shift:
    case kVK_Option:
    case kVK_Control:
      aKeyEvent.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_LEFT;
      break;

    case kVK_RightCommand:
    case kVK_RightShift:
    case kVK_RightOption:
    case kVK_RightControl:
      aKeyEvent.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_RIGHT;
      break;

    case kVK_ANSI_Keypad0:
    case kVK_ANSI_Keypad1:
    case kVK_ANSI_Keypad2:
    case kVK_ANSI_Keypad3:
    case kVK_ANSI_Keypad4:
    case kVK_ANSI_Keypad5:
    case kVK_ANSI_Keypad6:
    case kVK_ANSI_Keypad7:
    case kVK_ANSI_Keypad8:
    case kVK_ANSI_Keypad9:
    case kVK_ANSI_KeypadMultiply:
    case kVK_ANSI_KeypadPlus:
    case kVK_ANSI_KeypadMinus:
    case kVK_ANSI_KeypadDecimal:
    case kVK_ANSI_KeypadDivide:
    case kVK_ANSI_KeypadEquals:
    case kVK_ANSI_KeypadEnter:
    case kVK_JIS_KeypadComma:
    case kVK_Powerbook_KeypadEnter:
      aKeyEvent.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_NUMPAD;
      break;

    default:
      aKeyEvent.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD;
      break;
  }

  aKeyEvent.mIsRepeat =
    ([aNativeKeyEvent type] == NSKeyDown) ? [aNativeKeyEvent isARepeat] : false;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TISInputSourceWrapper::InitKeyEvent, "
     "shift=%s, ctrl=%s, alt=%s, meta=%s",
     this, OnOrOff(aKeyEvent.IsShift()), OnOrOff(aKeyEvent.IsControl()),
     OnOrOff(aKeyEvent.IsAlt()), OnOrOff(aKeyEvent.IsMeta())));

  if (aKeyEvent.mMessage == NS_KEY_PRESS &&
      (isPrintableKey || !insertString.IsEmpty())) {
    InitKeyPressEvent(aNativeKeyEvent,
                      insertString.IsEmpty() ? 0 : insertString[0],
                      aKeyEvent, kbType);
    MOZ_ASSERT(!aKeyEvent.charCode || !IsControlChar(aKeyEvent.charCode),
               "charCode must not be a control character");
  } else {
    aKeyEvent.charCode = 0;
    aKeyEvent.isChar = false; // XXX not used in XP level

    MOZ_LOG(gLog, LogLevel::Info,
      ("%p TISInputSourceWrapper::InitKeyEvent, keyCode=0x%X charCode=0x0",
       this, aKeyEvent.keyCode));
  }

  if (isPrintableKey) {
    aKeyEvent.mKeyNameIndex = KEY_NAME_INDEX_USE_STRING;
    // If insertText calls this method, let's use the string.
    if (aInsertString && !aInsertString->IsEmpty() &&
        !IsControlChar((*aInsertString)[0])) {
      aKeyEvent.mKeyValue = *aInsertString;
    }
    // If meta key is pressed, the printable key layout may be switched from
    // non-ASCII capable layout to ASCII capable, or from Dvorak to QWERTY.
    // KeyboardEvent.key value should be the switched layout's character.
    else if (aKeyEvent.IsMeta()) {
      nsCocoaUtils::GetStringForNSString([aNativeKeyEvent characters],
                                         aKeyEvent.mKeyValue);
    }
    // If control key is pressed, some keys may produce printable character via
    // [aNativeKeyEvent characters].  Otherwise, translate input character of
    // the key without control key.
    else if (aKeyEvent.IsControl()) {
      nsCocoaUtils::GetStringForNSString([aNativeKeyEvent characters],
                                         aKeyEvent.mKeyValue);
      if (aKeyEvent.mKeyValue.IsEmpty() ||
          IsControlChar(aKeyEvent.mKeyValue[0])) {
        NSUInteger cocoaState =
          [aNativeKeyEvent modifierFlags] & ~NSControlKeyMask;
        UInt32 carbonState = nsCocoaUtils::ConvertToCarbonModifier(cocoaState);
        aKeyEvent.mKeyValue =
          TranslateToChar(nativeKeyCode, carbonState, kbType);
      }
    }
    // Otherwise, KeyboardEvent.key expose
    // [aNativeKeyEvent characters] value.  However, if IME is open and the
    // keyboard layout isn't ASCII capable, exposing the non-ASCII character
    // doesn't match with other platform's behavior.  For the compatibility
    // with other platform's Gecko, we need to set a translated character.
    else if (IsOpenedIMEMode()) {
      UInt32 state =
        nsCocoaUtils::ConvertToCarbonModifier([aNativeKeyEvent modifierFlags]);
      aKeyEvent.mKeyValue = TranslateToChar(nativeKeyCode, state, kbType);
    } else {
      nsCocoaUtils::GetStringForNSString([aNativeKeyEvent characters],
                                         aKeyEvent.mKeyValue);
    }

    // Last resort.  If .key value becomes empty string, we should use
    // charactersIgnoringModifiers, if it's available.
    if (aKeyEvent.mKeyValue.IsEmpty() ||
        IsControlChar(aKeyEvent.mKeyValue[0])) {
      nsCocoaUtils::GetStringForNSString(
        [aNativeKeyEvent charactersIgnoringModifiers], aKeyEvent.mKeyValue);
      // But don't expose it if it's a control character.
      if (!aKeyEvent.mKeyValue.IsEmpty() &&
          IsControlChar(aKeyEvent.mKeyValue[0])) {
        aKeyEvent.mKeyValue.Truncate();
      }
    }
  } else {
    // Compute the key for non-printable keys and some special printable keys.
    aKeyEvent.mKeyNameIndex = ComputeGeckoKeyNameIndex(nativeKeyCode);
  }

  aKeyEvent.mCodeNameIndex = ComputeGeckoCodeNameIndex(nativeKeyCode);
  MOZ_ASSERT(aKeyEvent.mCodeNameIndex != CODE_NAME_INDEX_USE_STRING);

  NS_OBJC_END_TRY_ABORT_BLOCK
}

void
TISInputSourceWrapper::InitKeyPressEvent(NSEvent *aNativeKeyEvent,
                                         char16_t aInsertChar,
                                         WidgetKeyboardEvent& aKeyEvent,
                                         UInt32 aKbType)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION(aKeyEvent.mMessage == NS_KEY_PRESS,
               "aKeyEvent must be NS_KEY_PRESS event");

  if (MOZ_LOG_TEST(gLog, LogLevel::Info)) {
    nsAutoString chars;
    nsCocoaUtils::GetStringForNSString([aNativeKeyEvent characters], chars);
    NS_ConvertUTF16toUTF8 utf8Chars(chars);
    char16_t expectedChar = static_cast<char16_t>(aInsertChar);
    NS_ConvertUTF16toUTF8 utf8ExpectedChar(&expectedChar, 1);
    MOZ_LOG(gLog, LogLevel::Info,
      ("%p TISInputSourceWrapper::InitKeyPressEvent, aNativeKeyEvent=%p, "
       "[aNativeKeyEvent characters]=\"%s\", aInsertChar=0x%X(%s), "
       "aKeyEvent.mMessage=%s, aKbType=0x%X, IsOpenedIMEMode()=%s",
       this, aNativeKeyEvent, utf8Chars.get(), aInsertChar,
       utf8ExpectedChar.get(), GetGeckoKeyEventType(aKeyEvent), aKbType,
       TrueOrFalse(IsOpenedIMEMode())));
  }

  aKeyEvent.isChar = true; // this is not a special key  XXX not used in XP
  aKeyEvent.charCode = aInsertChar;
  if (aKeyEvent.charCode != 0) {
    aKeyEvent.keyCode = 0;
  }

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TISInputSourceWrapper::InitKeyPressEvent, "
     "aKeyEvent.keyCode=0x%X, aKeyEvent.charCode=0x%X",
     this, aKeyEvent.keyCode, aKeyEvent.charCode));

  if (!aKeyEvent.IsControl() && !aKeyEvent.IsMeta() && !aKeyEvent.IsAlt()) {
    return;
  }

  TISInputSourceWrapper USLayout("com.apple.keylayout.US");
  bool isRomanKeyboardLayout = IsASCIICapable();

  UInt32 key = [aNativeKeyEvent keyCode];

  // Caps lock and num lock modifier state:
  UInt32 lockState = 0;
  if ([aNativeKeyEvent modifierFlags] & NSAlphaShiftKeyMask) {
    lockState |= alphaLock;
  }
  if ([aNativeKeyEvent modifierFlags] & NSNumericPadKeyMask) {
    lockState |= kEventKeyModifierNumLockMask;
  }

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TISInputSourceWrapper::InitKeyPressEvent, "
     "isRomanKeyboardLayout=%s, key=0x%X",
     this, TrueOrFalse(isRomanKeyboardLayout), aKbType, key));

  nsString str;

  // normal chars
  uint32_t unshiftedChar = TranslateToChar(key, lockState, aKbType);
  UInt32 shiftLockMod = shiftKey | lockState;
  uint32_t shiftedChar = TranslateToChar(key, shiftLockMod, aKbType);

  // characters generated with Cmd key
  // XXX we should remove CapsLock state, which changes characters from
  //     Latin to Cyrillic with Russian layout on 10.4 only when Cmd key
  //     is pressed.
  UInt32 numState = (lockState & ~alphaLock); // only num lock state
  uint32_t uncmdedChar = TranslateToChar(key, numState, aKbType);
  UInt32 shiftNumMod = numState | shiftKey;
  uint32_t uncmdedShiftChar = TranslateToChar(key, shiftNumMod, aKbType);
  uint32_t uncmdedUSChar = USLayout.TranslateToChar(key, numState, aKbType);
  UInt32 cmdNumMod = cmdKey | numState;
  uint32_t cmdedChar = TranslateToChar(key, cmdNumMod, aKbType);
  UInt32 cmdShiftNumMod = shiftKey | cmdNumMod;
  uint32_t cmdedShiftChar = TranslateToChar(key, cmdShiftNumMod, aKbType);

  // Is the keyboard layout changed by Cmd key?
  // E.g., Arabic, Russian, Hebrew, Greek and Dvorak-QWERTY.
  bool isCmdSwitchLayout = uncmdedChar != cmdedChar;
  // Is the keyboard layout for Latin, but Cmd key switches the layout?
  // I.e., Dvorak-QWERTY
  bool isDvorakQWERTY = isCmdSwitchLayout && isRomanKeyboardLayout;

  // If the current keyboard is not Dvorak-QWERTY or Cmd is not pressed,
  // we should append unshiftedChar and shiftedChar for handling the
  // normal characters.  These are the characters that the user is most
  // likely to associate with this key.
  if ((unshiftedChar || shiftedChar) &&
      (!aKeyEvent.IsMeta() || !isDvorakQWERTY)) {
    AlternativeCharCode altCharCodes(unshiftedChar, shiftedChar);
    aKeyEvent.alternativeCharCodes.AppendElement(altCharCodes);
  }
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TISInputSourceWrapper::InitKeyPressEvent, "
     "aKeyEvent.isMeta=%s, isDvorakQWERTY=%s, "
     "unshiftedChar=U+%X, shiftedChar=U+%X",
     this, OnOrOff(aKeyEvent.IsMeta()), TrueOrFalse(isDvorakQWERTY),
     unshiftedChar, shiftedChar));

  // Most keyboard layouts provide the same characters in the NSEvents
  // with Command+Shift as with Command.  However, with Command+Shift we
  // want the character on the second level.  e.g. With a US QWERTY
  // layout, we want "?" when the "/","?" key is pressed with
  // Command+Shift.

  // On a German layout, the OS gives us '/' with Cmd+Shift+SS(eszett)
  // even though Cmd+SS is 'SS' and Shift+'SS' is '?'.  This '/' seems
  // like a hack to make the Cmd+"?" event look the same as the Cmd+"?"
  // event on a US keyboard.  The user thinks they are typing Cmd+"?", so
  // we'll prefer the "?" character, replacing charCode with shiftedChar
  // when Shift is pressed.  However, in case there is a layout where the
  // character unique to Cmd+Shift is the character that the user expects,
  // we'll send it as an alternative char.
  bool hasCmdShiftOnlyChar =
    cmdedChar != cmdedShiftChar && uncmdedShiftChar != cmdedShiftChar;
  uint32_t originalCmdedShiftChar = cmdedShiftChar;

  // If we can make a good guess at the characters that the user would
  // expect this key combination to produce (with and without Shift) then
  // use those characters.  This also corrects for CapsLock, which was
  // ignored above.
  if (!isCmdSwitchLayout) {
    // The characters produced with Command seem similar to those without
    // Command.
    if (unshiftedChar) {
      cmdedChar = unshiftedChar;
    }
    if (shiftedChar) {
      cmdedShiftChar = shiftedChar;
    }
  } else if (uncmdedUSChar == cmdedChar) {
    // It looks like characters from a US layout are provided when Command
    // is down.
    uint32_t ch = USLayout.TranslateToChar(key, lockState, aKbType);
    if (ch) {
      cmdedChar = ch;
    }
    ch = USLayout.TranslateToChar(key, shiftLockMod, aKbType);
    if (ch) {
      cmdedShiftChar = ch;
    }
  }

  // If the current keyboard layout is switched by the Cmd key,
  // we should append cmdedChar and shiftedCmdChar that are
  // Latin char for the key.
  // If the keyboard layout is Dvorak-QWERTY, we should append them only when
  // command key is pressed because when command key isn't pressed, uncmded
  // chars have been appended already.
  if ((cmdedChar || cmdedShiftChar) && isCmdSwitchLayout &&
      (aKeyEvent.IsMeta() || !isDvorakQWERTY)) {
    AlternativeCharCode altCharCodes(cmdedChar, cmdedShiftChar);
    aKeyEvent.alternativeCharCodes.AppendElement(altCharCodes);
  }
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TISInputSourceWrapper::InitKeyPressEvent, "
     "hasCmdShiftOnlyChar=%s, isCmdSwitchLayout=%s, isDvorakQWERTY=%s, "
     "cmdedChar=U+%X, cmdedShiftChar=U+%X",
     this, TrueOrFalse(hasCmdShiftOnlyChar), TrueOrFalse(isDvorakQWERTY),
     TrueOrFalse(isDvorakQWERTY), cmdedChar, cmdedShiftChar));
  // Special case for 'SS' key of German layout. See the comment of
  // hasCmdShiftOnlyChar definition for the detail.
  if (hasCmdShiftOnlyChar && originalCmdedShiftChar) {
    AlternativeCharCode altCharCodes(0, originalCmdedShiftChar);
    aKeyEvent.alternativeCharCodes.AppendElement(altCharCodes);
  }
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TISInputSourceWrapper::InitKeyPressEvent, "
     "hasCmdShiftOnlyChar=%s, originalCmdedShiftChar=U+%X",
     this, TrueOrFalse(hasCmdShiftOnlyChar), originalCmdedShiftChar));

  NS_OBJC_END_TRY_ABORT_BLOCK
}

uint32_t
TISInputSourceWrapper::ComputeGeckoKeyCode(UInt32 aNativeKeyCode,
                                           UInt32 aKbType,
                                           bool aCmdIsPressed)
{
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TISInputSourceWrapper::ComputeGeckoKeyCode, aNativeKeyCode=0x%X, "
     "aKbType=0x%X, aCmdIsPressed=%s, IsOpenedIMEMode()=%s, "
     "IsASCIICapable()=%s",
     this, aNativeKeyCode, aKbType, TrueOrFalse(aCmdIsPressed),
     TrueOrFalse(IsOpenedIMEMode()), TrueOrFalse(IsASCIICapable())));

  switch (aNativeKeyCode) {
    case kVK_Space:             return NS_VK_SPACE;
    case kVK_Escape:            return NS_VK_ESCAPE;

    // modifiers
    case kVK_RightCommand:
    case kVK_Command:           return NS_VK_META;
    case kVK_RightShift:
    case kVK_Shift:             return NS_VK_SHIFT;
    case kVK_CapsLock:          return NS_VK_CAPS_LOCK;
    case kVK_RightControl:
    case kVK_Control:           return NS_VK_CONTROL;
    case kVK_RightOption:
    case kVK_Option:            return NS_VK_ALT;

    case kVK_ANSI_KeypadClear:  return NS_VK_CLEAR;

    // function keys
    case kVK_F1:                return NS_VK_F1;
    case kVK_F2:                return NS_VK_F2;
    case kVK_F3:                return NS_VK_F3;
    case kVK_F4:                return NS_VK_F4;
    case kVK_F5:                return NS_VK_F5;
    case kVK_F6:                return NS_VK_F6;
    case kVK_F7:                return NS_VK_F7;
    case kVK_F8:                return NS_VK_F8;
    case kVK_F9:                return NS_VK_F9;
    case kVK_F10:               return NS_VK_F10;
    case kVK_F11:               return NS_VK_F11;
    case kVK_F12:               return NS_VK_F12;
    // case kVK_F13:               return NS_VK_F13;  // clash with the 3 below
    // case kVK_F14:               return NS_VK_F14;
    // case kVK_F15:               return NS_VK_F15;
    case kVK_F16:               return NS_VK_F16;
    case kVK_F17:               return NS_VK_F17;
    case kVK_F18:               return NS_VK_F18;
    case kVK_F19:               return NS_VK_F19;

    case kVK_PC_Pause:          return NS_VK_PAUSE;
    case kVK_PC_ScrollLock:     return NS_VK_SCROLL_LOCK;
    case kVK_PC_PrintScreen:    return NS_VK_PRINTSCREEN;

    // keypad
    case kVK_ANSI_Keypad0:      return NS_VK_NUMPAD0;
    case kVK_ANSI_Keypad1:      return NS_VK_NUMPAD1;
    case kVK_ANSI_Keypad2:      return NS_VK_NUMPAD2;
    case kVK_ANSI_Keypad3:      return NS_VK_NUMPAD3;
    case kVK_ANSI_Keypad4:      return NS_VK_NUMPAD4;
    case kVK_ANSI_Keypad5:      return NS_VK_NUMPAD5;
    case kVK_ANSI_Keypad6:      return NS_VK_NUMPAD6;
    case kVK_ANSI_Keypad7:      return NS_VK_NUMPAD7;
    case kVK_ANSI_Keypad8:      return NS_VK_NUMPAD8;
    case kVK_ANSI_Keypad9:      return NS_VK_NUMPAD9;

    case kVK_ANSI_KeypadMultiply: return NS_VK_MULTIPLY;
    case kVK_ANSI_KeypadPlus:     return NS_VK_ADD;
    case kVK_ANSI_KeypadMinus:    return NS_VK_SUBTRACT;
    case kVK_ANSI_KeypadDecimal:  return NS_VK_DECIMAL;
    case kVK_ANSI_KeypadDivide:   return NS_VK_DIVIDE;

    case kVK_JIS_KeypadComma:   return NS_VK_SEPARATOR;

    // IME keys
    case kVK_JIS_Eisu:          return NS_VK_EISU;
    case kVK_JIS_Kana:          return NS_VK_KANA;

    // these may clash with forward delete and help
    case kVK_PC_Insert:         return NS_VK_INSERT;
    case kVK_PC_Delete:         return NS_VK_DELETE;

    case kVK_PC_Backspace:      return NS_VK_BACK;
    case kVK_Tab:               return NS_VK_TAB;

    case kVK_Home:              return NS_VK_HOME;
    case kVK_End:               return NS_VK_END;

    case kVK_PageUp:            return NS_VK_PAGE_UP;
    case kVK_PageDown:          return NS_VK_PAGE_DOWN;

    case kVK_LeftArrow:         return NS_VK_LEFT;
    case kVK_RightArrow:        return NS_VK_RIGHT;
    case kVK_UpArrow:           return NS_VK_UP;
    case kVK_DownArrow:         return NS_VK_DOWN;

    case kVK_PC_ContextMenu:    return NS_VK_CONTEXT_MENU;

    case kVK_ANSI_1:            return NS_VK_1;
    case kVK_ANSI_2:            return NS_VK_2;
    case kVK_ANSI_3:            return NS_VK_3;
    case kVK_ANSI_4:            return NS_VK_4;
    case kVK_ANSI_5:            return NS_VK_5;
    case kVK_ANSI_6:            return NS_VK_6;
    case kVK_ANSI_7:            return NS_VK_7;
    case kVK_ANSI_8:            return NS_VK_8;
    case kVK_ANSI_9:            return NS_VK_9;
    case kVK_ANSI_0:            return NS_VK_0;

    case kVK_ANSI_KeypadEnter:
    case kVK_Return:
    case kVK_Powerbook_KeypadEnter: return NS_VK_RETURN;
  }

  // If Cmd key is pressed, that causes switching keyboard layout temporarily.
  // E.g., Dvorak-QWERTY.  Therefore, if Cmd key is pressed, we should honor it.
  UInt32 modifiers = aCmdIsPressed ? cmdKey : 0;

  uint32_t charCode = TranslateToChar(aNativeKeyCode, modifiers, aKbType);

  // Special case for Mac.  Mac inputs Yen sign (U+00A5) directly instead of
  // Back slash (U+005C).  We should return NS_VK_BACK_SLASH for compatibility
  // with other platforms.
  // XXX How about Won sign (U+20A9) which has same problem as Yen sign?
  if (charCode == 0x00A5) {
    return NS_VK_BACK_SLASH;
  }

  uint32_t keyCode = WidgetUtils::ComputeKeyCodeFromChar(charCode);
  if (keyCode) {
    return keyCode;
  }

  // If the unshifed char isn't an ASCII character, use shifted char.
  charCode = TranslateToChar(aNativeKeyCode, modifiers | shiftKey, aKbType);
  keyCode = WidgetUtils::ComputeKeyCodeFromChar(charCode);
  if (keyCode) {
    return keyCode;
  }

  // If this is ASCII capable, give up to compute it.
  if (IsASCIICapable()) {
    return 0;
  }

  // Retry with ASCII capable keyboard layout.
  TISInputSourceWrapper currentKeyboardLayout;
  currentKeyboardLayout.InitByCurrentASCIICapableKeyboardLayout();
  NS_ENSURE_TRUE(mInputSource != currentKeyboardLayout.mInputSource, 0);
  keyCode = currentKeyboardLayout.ComputeGeckoKeyCode(aNativeKeyCode, aKbType,
                                                      aCmdIsPressed);

  // However, if keyCode isn't for an alphabet keys or a numeric key, we should
  // ignore it.  For example, comma key of Thai layout is same as close-square-
  // bracket key of US layout and an unicode character key of Thai layout is
  // same as comma key of US layout.  If we return NS_VK_COMMA for latter key,
  // web application developers cannot distinguish with the former key.
  return ((keyCode >= NS_VK_A && keyCode <= NS_VK_Z) ||
          (keyCode >= NS_VK_0 && keyCode <= NS_VK_9)) ? keyCode : 0;
}

// static
KeyNameIndex
TISInputSourceWrapper::ComputeGeckoKeyNameIndex(UInt32 aNativeKeyCode)
{
  // NOTE:
  //   When unsupported keys like Convert, Nonconvert of Japanese keyboard is
  //   pressed:
  //     on 10.6.x, 'A' key event is fired (and also actually 'a' is inserted).
  //     on 10.7.x, Nothing happens.
  //     on 10.8.x, Nothing happens.
  //     on 10.9.x, FlagsChanged event is fired with keyCode 0xFF.
  switch (aNativeKeyCode) {

#define NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, aKeyNameIndex) \
    case aNativeKey: return aKeyNameIndex;

#include "NativeKeyToDOMKeyName.h"

#undef NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX

    default:
      return KEY_NAME_INDEX_Unidentified;
  }
}

// static
CodeNameIndex
TISInputSourceWrapper::ComputeGeckoCodeNameIndex(UInt32 aNativeKeyCode)
{
  switch (aNativeKeyCode) {

#define NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, aCodeNameIndex) \
    case aNativeKey: return aCodeNameIndex;

#include "NativeKeyToDOMCodeName.h"

#undef NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX

    default:
      return CODE_NAME_INDEX_UNKNOWN;
  }
}


#pragma mark -


/******************************************************************************
 *
 *  TextInputHandler implementation (static methods)
 *
 ******************************************************************************/

NSUInteger TextInputHandler::sLastModifierState = 0;

// static
CFArrayRef
TextInputHandler::CreateAllKeyboardLayoutList()
{
  const void* keys[] = { kTISPropertyInputSourceType };
  const void* values[] = { kTISTypeKeyboardLayout };
  CFDictionaryRef filter =
    ::CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, NULL, NULL);
  NS_ASSERTION(filter, "failed to create the filter");
  CFArrayRef list = ::TISCreateInputSourceList(filter, true);
  ::CFRelease(filter);
  return list;
}

// static
void
TextInputHandler::DebugPrintAllKeyboardLayouts()
{
  if (MOZ_LOG_TEST(gLog, LogLevel::Info)) {
    CFArrayRef list = CreateAllKeyboardLayoutList();
    MOZ_LOG(gLog, LogLevel::Info, ("Keyboard layout configuration:"));
    CFIndex idx = ::CFArrayGetCount(list);
    TISInputSourceWrapper tis;
    for (CFIndex i = 0; i < idx; ++i) {
      TISInputSourceRef inputSource = static_cast<TISInputSourceRef>(
        const_cast<void *>(::CFArrayGetValueAtIndex(list, i)));
      tis.InitByTISInputSourceRef(inputSource);
      nsAutoString name, isid;
      tis.GetLocalizedName(name);
      tis.GetInputSourceID(isid);
      MOZ_LOG(gLog, LogLevel::Info,
             ("  %s\t<%s>%s%s\n",
              NS_ConvertUTF16toUTF8(name).get(),
              NS_ConvertUTF16toUTF8(isid).get(),
              tis.IsASCIICapable() ? "" : "\t(Isn't ASCII capable)",
              tis.IsKeyboardLayout() && tis.GetUCKeyboardLayout() ?
                "" : "\t(uchr is NOT AVAILABLE)"));
    }
    ::CFRelease(list);
  }
}


#pragma mark -


/******************************************************************************
 *
 *  TextInputHandler implementation
 *
 ******************************************************************************/

TextInputHandler::TextInputHandler(nsChildView* aWidget,
                                   NSView<mozView> *aNativeView) :
  IMEInputHandler(aWidget, aNativeView)
{
  InitLogModule();
  [mView installTextInputHandler:this];
}

TextInputHandler::~TextInputHandler()
{
  [mView uninstallTextInputHandler];
}

bool
TextInputHandler::HandleKeyDownEvent(NSEvent* aNativeEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (Destroyed()) {
    MOZ_LOG(gLog, LogLevel::Info,
      ("%p TextInputHandler::HandleKeyDownEvent, "
       "widget has been already destroyed", this));
    return false;
  }

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TextInputHandler::HandleKeyDownEvent, aNativeEvent=%p, "
     "type=%s, keyCode=%lld (0x%X), modifierFlags=0x%X, characters=\"%s\", "
     "charactersIgnoringModifiers=\"%s\"",
     this, aNativeEvent, GetNativeKeyEventType(aNativeEvent),
     [aNativeEvent keyCode], [aNativeEvent keyCode],
     [aNativeEvent modifierFlags], GetCharacters([aNativeEvent characters]),
     GetCharacters([aNativeEvent charactersIgnoringModifiers])));

  nsRefPtr<nsChildView> kungFuDeathGrip(mWidget);

  KeyEventState* currentKeyEvent = PushKeyEvent(aNativeEvent);
  AutoKeyEventStateCleaner remover(this);

  ComplexTextInputPanel* ctiPanel = ComplexTextInputPanel::GetSharedComplexTextInputPanel();
  if (ctiPanel && ctiPanel->IsInComposition()) {
    nsAutoString committed;
    ctiPanel->InterpretKeyEvent(aNativeEvent, committed);
    if (!committed.IsEmpty()) {
      WidgetKeyboardEvent imeEvent(true, NS_KEY_DOWN, mWidget);
      InitKeyEvent(aNativeEvent, imeEvent);
      imeEvent.mPluginTextEventString.Assign(committed);
      DispatchEvent(imeEvent);
    }
    return true;
  }

  if (mWidget->IsPluginFocused() || !IsIMEComposing()) {
    NSResponder* firstResponder = [[mView window] firstResponder];

    WidgetKeyboardEvent keydownEvent(true, NS_KEY_DOWN, mWidget);
    InitKeyEvent(aNativeEvent, keydownEvent);

    currentKeyEvent->mKeyDownHandled = DispatchEvent(keydownEvent);
    if (Destroyed()) {
      MOZ_LOG(gLog, LogLevel::Info,
        ("%p TextInputHandler::HandleKeyDownEvent, "
         "widget was destroyed by keydown event", this));
      return currentKeyEvent->IsDefaultPrevented();
    }

    // The key down event may have shifted the focus, in which
    // case we should not fire the key press.
    // XXX This is a special code only on Cocoa widget, why is this needed?
    if (firstResponder != [[mView window] firstResponder]) {
      MOZ_LOG(gLog, LogLevel::Info,
        ("%p TextInputHandler::HandleKeyDownEvent, "
         "view lost focus by keydown event", this));
      return currentKeyEvent->IsDefaultPrevented();
    }

    if (currentKeyEvent->IsDefaultPrevented()) {
      MOZ_LOG(gLog, LogLevel::Info,
        ("%p TextInputHandler::HandleKeyDownEvent, "
         "keydown event's default is prevented", this));
      return true;
    }
  }

  // None of what follows is needed for plugin keyboard input.  In fact it
  // may cause trouble -- for example the call to [mView interpretKeyEvents:]
  // can, in e10s mode, cause each key typed to appear twice in an IME
  // composition.
  if (mWidget->IsPluginFocused()) {
    return true;
  }

  // Let Cocoa interpret the key events, caching IsIMEComposing first.
  bool wasComposing = IsIMEComposing();
  bool interpretKeyEventsCalled = false;
  if (IsIMEEnabled() || IsASCIICapableOnly()) {
    MOZ_LOG(gLog, LogLevel::Info,
      ("%p TextInputHandler::HandleKeyDownEvent, calling interpretKeyEvents",
       this));
    [mView interpretKeyEvents:[NSArray arrayWithObject:aNativeEvent]];
    interpretKeyEventsCalled = true;
    MOZ_LOG(gLog, LogLevel::Info,
      ("%p TextInputHandler::HandleKeyDownEvent, called interpretKeyEvents",
       this));
  }

  if (Destroyed()) {
    MOZ_LOG(gLog, LogLevel::Info,
      ("%p TextInputHandler::HandleKeyDownEvent, widget was destroyed",
       this));
    return currentKeyEvent->IsDefaultPrevented();
  }

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TextInputHandler::HandleKeyDownEvent, wasComposing=%s, "
     "IsIMEComposing()=%s",
     this, TrueOrFalse(wasComposing), TrueOrFalse(IsIMEComposing())));

  if (currentKeyEvent->CanDispatchKeyPressEvent() &&
      !wasComposing && !IsIMEComposing()) {
    WidgetKeyboardEvent keypressEvent(true, NS_KEY_PRESS, mWidget);
    InitKeyEvent(aNativeEvent, keypressEvent);

    // If we called interpretKeyEvents and this isn't normal character input
    // then IME probably ate the event for some reason. We do not want to
    // send a key press event in that case.
    // TODO:
    // There are some other cases which IME eats the current event.
    // 1. If key events were nested during calling interpretKeyEvents, it means
    //    that IME did something.  Then, we should do nothing.
    // 2. If one or more commands are called like "deleteBackward", we should
    //    dispatch keypress event at that time.  Note that the command may have
    //    been a converted or generated action by IME.  Then, we shouldn't do
    //    our default action for this key.
    if (!(interpretKeyEventsCalled &&
          IsNormalCharInputtingEvent(keypressEvent))) {
      currentKeyEvent->mKeyPressHandled = DispatchEvent(keypressEvent);
      currentKeyEvent->mKeyPressDispatched = true;
      MOZ_LOG(gLog, LogLevel::Info,
        ("%p TextInputHandler::HandleKeyDownEvent, keypress event dispatched",
         this));
    }
  }

  // Note: mWidget might have become null here. Don't count on it from here on.

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TextInputHandler::HandleKeyDownEvent, "
     "keydown handled=%s, keypress handled=%s, causedOtherKeyEvents=%s",
     this, TrueOrFalse(currentKeyEvent->mKeyDownHandled),
     TrueOrFalse(currentKeyEvent->mKeyPressHandled),
     TrueOrFalse(currentKeyEvent->mCausedOtherKeyEvents)));
  return currentKeyEvent->IsDefaultPrevented();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

void
TextInputHandler::HandleKeyUpEvent(NSEvent* aNativeEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TextInputHandler::HandleKeyUpEvent, aNativeEvent=%p, "
     "type=%s, keyCode=%lld (0x%X), modifierFlags=0x%X, characters=\"%s\", "
     "charactersIgnoringModifiers=\"%s\", "
     "IsIMEComposing()=%s",
     this, aNativeEvent, GetNativeKeyEventType(aNativeEvent),
     [aNativeEvent keyCode], [aNativeEvent keyCode],
     [aNativeEvent modifierFlags], GetCharacters([aNativeEvent characters]),
     GetCharacters([aNativeEvent charactersIgnoringModifiers]),
     TrueOrFalse(IsIMEComposing())));

  if (Destroyed()) {
    MOZ_LOG(gLog, LogLevel::Info,
      ("%p TextInputHandler::HandleKeyUpEvent, "
       "widget has been already destroyed", this));
    return;
  }

  // if we don't have any characters we can't generate a keyUp event
  if (IsIMEComposing()) {
    return;
  }

  WidgetKeyboardEvent keyupEvent(true, NS_KEY_UP, mWidget);
  InitKeyEvent(aNativeEvent, keyupEvent);

  DispatchEvent(keyupEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
TextInputHandler::HandleFlagsChanged(NSEvent* aNativeEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (Destroyed()) {
    MOZ_LOG(gLog, LogLevel::Info,
      ("%p TextInputHandler::HandleFlagsChanged, "
       "widget has been already destroyed", this));
    return;
  }

  nsRefPtr<nsChildView> kungFuDeathGrip(mWidget);

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TextInputHandler::HandleFlagsChanged, aNativeEvent=%p, "
     "type=%s, keyCode=%s (0x%X), modifierFlags=0x%08X, "
     "sLastModifierState=0x%08X, IsIMEComposing()=%s",
     this, aNativeEvent, GetNativeKeyEventType(aNativeEvent),
     GetKeyNameForNativeKeyCode([aNativeEvent keyCode]), [aNativeEvent keyCode],
     [aNativeEvent modifierFlags], sLastModifierState,
     TrueOrFalse(IsIMEComposing())));

  MOZ_ASSERT([aNativeEvent type] == NSFlagsChanged);

  NSUInteger diff = [aNativeEvent modifierFlags] ^ sLastModifierState;
  // Device dependent flags for left-control key, both shift keys, both command
  // keys and both option keys have been defined in Next's SDK.  But we
  // shouldn't use it directly as far as possible since Cocoa SDK doesn't
  // define them.  Fortunately, we need them only when we dispatch keyup
  // events.  So, we can usually know the actual relation between keyCode and
  // device dependent flags.  However, we need to remove following flags first
  // since the differences don't indicate modifier key state.
  // NX_STYLUSPROXIMITYMASK: Probably used for pen like device.
  // kCGEventFlagMaskNonCoalesced (= NX_NONCOALSESCEDMASK): See the document for
  // Quartz Event Services.
  diff &= ~(NX_STYLUSPROXIMITYMASK | kCGEventFlagMaskNonCoalesced);

  switch ([aNativeEvent keyCode]) {
    // CapsLock state and other modifier states are different:
    // CapsLock state does not revert when the CapsLock key goes up, as the
    // modifier state does for other modifier keys on key up.
    case kVK_CapsLock: {
      // Fire key down event for caps lock.
      DispatchKeyEventForFlagsChanged(aNativeEvent, true);
      // XXX should we fire keyup event too? The keyup event for CapsLock key
      // is never dispatched on Gecko.
      // XXX WebKit dispatches keydown event when CapsLock is locked, otherwise,
      // keyup event.  If we do so, we cannot keep the consistency with other
      // platform's behavior...
      break;
    }

    // If the event is caused by pressing or releasing a modifier key, just
    // dispatch the key's event.
    case kVK_Shift:
    case kVK_RightShift:
    case kVK_Command:
    case kVK_RightCommand:
    case kVK_Control:
    case kVK_RightControl:
    case kVK_Option:
    case kVK_RightOption:
    case kVK_Help: {
      // We assume that at most one modifier is changed per event if the event
      // is caused by pressing or releasing a modifier key.
      bool isKeyDown = ([aNativeEvent modifierFlags] & diff) != 0;
      DispatchKeyEventForFlagsChanged(aNativeEvent, isKeyDown);
      // XXX Some applications might send the event with incorrect device-
      //     dependent flags.
      if (isKeyDown && ((diff & ~NSDeviceIndependentModifierFlagsMask) != 0)) {
        unsigned short keyCode = [aNativeEvent keyCode];
        const ModifierKey* modifierKey =
          GetModifierKeyForDeviceDependentFlags(diff);
        if (modifierKey && modifierKey->keyCode != keyCode) {
          // Although, we're not sure the actual cause of this case, the stored
          // modifier information and the latest key event information may be
          // mismatched. Then, let's reset the stored information.
          // NOTE: If this happens, it may fail to handle NSFlagsChanged event
          // in the default case (below). However, it's the rare case handler
          // and this case occurs rarely. So, we can ignore the edge case bug.
          NS_WARNING("Resetting stored modifier key information");
          mModifierKeys.Clear();
          modifierKey = nullptr;
        }
        if (!modifierKey) {
          mModifierKeys.AppendElement(ModifierKey(diff, keyCode));
        }
      }
      break;
    }

    // Currently we don't support Fn key since other browsers don't dispatch
    // events for it and we don't have keyCode for this key.
    // It should be supported when we implement .key and .char.
    case kVK_Function:
      break;

    // If the event is caused by something else than pressing or releasing a
    // single modifier key (for example by the app having been deactivated
    // using command-tab), use the modifiers themselves to determine which
    // key's event to dispatch, and whether it's a keyup or keydown event.
    // In all cases we assume one or more modifiers are being deactivated
    // (never activated) -- otherwise we'd have received one or more events
    // corresponding to a single modifier key being pressed.
    default: {
      NSUInteger modifiers = sLastModifierState;
      for (int32_t bit = 0; bit < 32; ++bit) {
        NSUInteger flag = 1 << bit;
        if (!(diff & flag)) {
          continue;
        }

        // Given correct information from the application, a flag change here
        // will normally be a deactivation (except for some lockable modifiers
        // such as CapsLock).  But some applications (like VNC) can send an
        // activating event with a zero keyCode.  So we need to check for that
        // here.
        bool dispatchKeyDown = ((flag & [aNativeEvent modifierFlags]) != 0);

        unsigned short keyCode = 0;
        if (flag & NSDeviceIndependentModifierFlagsMask) {
          switch (flag) {
            case NSAlphaShiftKeyMask:
              keyCode = kVK_CapsLock;
              dispatchKeyDown = true;
              break;

            case NSNumericPadKeyMask:
              // NSNumericPadKeyMask is fired by VNC a lot. But not all of
              // these events can really be Clear key events, so we just ignore
              // them.
              continue;

            case NSHelpKeyMask:
              keyCode = kVK_Help;
              break;

            case NSFunctionKeyMask:
              // An NSFunctionKeyMask change here will normally be a
              // deactivation.  But sometimes it will be an activation send (by
              // VNC for example) with a zero keyCode.
              continue;

            // These cases (NSShiftKeyMask, NSControlKeyMask, NSAlternateKeyMask
            // and NSCommandKeyMask) should be handled by the other branch of
            // the if statement, below (which handles device dependent flags).
            // However, some applications (like VNC) can send key events without
            // any device dependent flags, so we handle them here instead.
            case NSShiftKeyMask:
              keyCode = (modifiers & 0x0004) ? kVK_RightShift : kVK_Shift;
              break;
            case NSControlKeyMask:
              keyCode = (modifiers & 0x2000) ? kVK_RightControl : kVK_Control;
              break;
            case NSAlternateKeyMask:
              keyCode = (modifiers & 0x0040) ? kVK_RightOption : kVK_Option;
              break;
            case NSCommandKeyMask:
              keyCode = (modifiers & 0x0010) ? kVK_RightCommand : kVK_Command;
              break;

            default:
              continue;
          }
        } else {
          const ModifierKey* modifierKey =
            GetModifierKeyForDeviceDependentFlags(flag);
          if (!modifierKey) {
            // See the note above (in the other branch of the if statement)
            // about the NSShiftKeyMask, NSControlKeyMask, NSAlternateKeyMask
            // and NSCommandKeyMask cases.
            continue;
          }
          keyCode = modifierKey->keyCode;
        }

        // Remove flags
        modifiers &= ~flag;
        switch (keyCode) {
          case kVK_Shift: {
            const ModifierKey* modifierKey =
              GetModifierKeyForNativeKeyCode(kVK_RightShift);
            if (!modifierKey ||
                !(modifiers & modifierKey->GetDeviceDependentFlags())) {
              modifiers &= ~NSShiftKeyMask;
            }
            break;
          }
          case kVK_RightShift: {
            const ModifierKey* modifierKey =
              GetModifierKeyForNativeKeyCode(kVK_Shift);
            if (!modifierKey ||
                !(modifiers & modifierKey->GetDeviceDependentFlags())) {
              modifiers &= ~NSShiftKeyMask;
            }
            break;
          }
          case kVK_Command: {
            const ModifierKey* modifierKey =
              GetModifierKeyForNativeKeyCode(kVK_RightCommand);
            if (!modifierKey ||
                !(modifiers & modifierKey->GetDeviceDependentFlags())) {
              modifiers &= ~NSCommandKeyMask;
            }
            break;
          }
          case kVK_RightCommand: {
            const ModifierKey* modifierKey =
              GetModifierKeyForNativeKeyCode(kVK_Command);
            if (!modifierKey ||
                !(modifiers & modifierKey->GetDeviceDependentFlags())) {
              modifiers &= ~NSCommandKeyMask;
            }
            break;
          }
          case kVK_Control: {
            const ModifierKey* modifierKey =
              GetModifierKeyForNativeKeyCode(kVK_RightControl);
            if (!modifierKey ||
                !(modifiers & modifierKey->GetDeviceDependentFlags())) {
              modifiers &= ~NSControlKeyMask;
            }
            break;
          }
          case kVK_RightControl: {
            const ModifierKey* modifierKey =
              GetModifierKeyForNativeKeyCode(kVK_Control);
            if (!modifierKey ||
                !(modifiers & modifierKey->GetDeviceDependentFlags())) {
              modifiers &= ~NSControlKeyMask;
            }
            break;
          }
          case kVK_Option: {
            const ModifierKey* modifierKey =
              GetModifierKeyForNativeKeyCode(kVK_RightOption);
            if (!modifierKey ||
                !(modifiers & modifierKey->GetDeviceDependentFlags())) {
              modifiers &= ~NSAlternateKeyMask;
            }
            break;
          }
          case kVK_RightOption: {
            const ModifierKey* modifierKey =
              GetModifierKeyForNativeKeyCode(kVK_Option);
            if (!modifierKey ||
                !(modifiers & modifierKey->GetDeviceDependentFlags())) {
              modifiers &= ~NSAlternateKeyMask;
            }
            break;
          }
          case kVK_Help:
            modifiers &= ~NSHelpKeyMask;
            break;
          default:
            break;
        }

        NSEvent* event =
          [NSEvent keyEventWithType:NSFlagsChanged
                           location:[aNativeEvent locationInWindow]
                      modifierFlags:modifiers
                          timestamp:[aNativeEvent timestamp]
                       windowNumber:[aNativeEvent windowNumber]
                            context:[aNativeEvent context]
                         characters:nil
        charactersIgnoringModifiers:nil
                          isARepeat:NO
                            keyCode:keyCode];
        DispatchKeyEventForFlagsChanged(event, dispatchKeyDown);
        if (Destroyed()) {
          break;
        }

        // Stop if focus has changed.
        // Check to see if mView is still the first responder.
        if (![mView isFirstResponder]) {
          break;
        }

      }
      break;
    }
  }

  // Be aware, the widget may have been destroyed.
  sLastModifierState = [aNativeEvent modifierFlags];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

const TextInputHandler::ModifierKey*
TextInputHandler::GetModifierKeyForNativeKeyCode(unsigned short aKeyCode) const
{
  for (ModifierKeyArray::index_type i = 0; i < mModifierKeys.Length(); ++i) {
    if (mModifierKeys[i].keyCode == aKeyCode) {
      return &((ModifierKey&)mModifierKeys[i]);
    }
  }
  return nullptr;
}

const TextInputHandler::ModifierKey*
TextInputHandler::GetModifierKeyForDeviceDependentFlags(NSUInteger aFlags) const
{
  for (ModifierKeyArray::index_type i = 0; i < mModifierKeys.Length(); ++i) {
    if (mModifierKeys[i].GetDeviceDependentFlags() ==
          (aFlags & ~NSDeviceIndependentModifierFlagsMask)) {
      return &((ModifierKey&)mModifierKeys[i]);
    }
  }
  return nullptr;
}

void
TextInputHandler::DispatchKeyEventForFlagsChanged(NSEvent* aNativeEvent,
                                                  bool aDispatchKeyDown)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (Destroyed()) {
    return;
  }

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TextInputHandler::DispatchKeyEventForFlagsChanged, aNativeEvent=%p, "
     "type=%s, keyCode=%s (0x%X), aDispatchKeyDown=%s, IsIMEComposing()=%s",
     this, aNativeEvent, GetNativeKeyEventType(aNativeEvent),
     GetKeyNameForNativeKeyCode([aNativeEvent keyCode]), [aNativeEvent keyCode],
     TrueOrFalse(aDispatchKeyDown), TrueOrFalse(IsIMEComposing())));

  if ([aNativeEvent type] != NSFlagsChanged || IsIMEComposing()) {
    return;
  }

  EventMessage message = aDispatchKeyDown ? NS_KEY_DOWN : NS_KEY_UP;

  // Fire a key event.
  WidgetKeyboardEvent keyEvent(true, message, mWidget);
  InitKeyEvent(aNativeEvent, keyEvent);

  // Attach a plugin event, in case keyEvent gets dispatched to a plugin.  Only
  // one field is needed -- the type.  The other fields can be constructed as
  // the need arises.  But Gecko doesn't have anything equivalent to the
  // NPCocoaEventFlagsChanged type, and this needs to be passed accurately to
  // any plugin to which this event is sent.
  NPCocoaEvent cocoaEvent;
  nsCocoaUtils::InitNPCocoaEvent(&cocoaEvent);
  cocoaEvent.type = NPCocoaEventFlagsChanged;
  keyEvent.mPluginEvent.Copy(cocoaEvent);

  DispatchEvent(keyEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
TextInputHandler::InsertText(NSAttributedString* aAttrString,
                             NSRange* aReplacementRange)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (Destroyed()) {
    return;
  }

  KeyEventState* currentKeyEvent = GetCurrentKeyEvent();

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TextInputHandler::InsertText, aAttrString=\"%s\", "
     "aReplacementRange=%p { location=%llu, length=%llu }, "
     "IsIMEComposing()=%s, IgnoreIMEComposition()=%s, "
     "keyevent=%p, keydownHandled=%s, keypressDispatched=%s, "
     "causedOtherKeyEvents=%s",
     this, GetCharacters([aAttrString string]), aReplacementRange,
     aReplacementRange ? aReplacementRange->location : 0,
     aReplacementRange ? aReplacementRange->length : 0,
     TrueOrFalse(IsIMEComposing()), TrueOrFalse(IgnoreIMEComposition()),
     currentKeyEvent ? currentKeyEvent->mKeyEvent : nullptr,
     currentKeyEvent ?
       TrueOrFalse(currentKeyEvent->mKeyDownHandled) : "N/A",
     currentKeyEvent ?
       TrueOrFalse(currentKeyEvent->mKeyPressDispatched) : "N/A",
     currentKeyEvent ?
       TrueOrFalse(currentKeyEvent->mCausedOtherKeyEvents) : "N/A"));

  if (IgnoreIMEComposition()) {
    return;
  }

  InputContext context = mWidget->GetInputContext();
  bool isEditable = (context.mIMEState.mEnabled == IMEState::ENABLED ||
                     context.mIMEState.mEnabled == IMEState::PASSWORD);
  NSRange selectedRange = SelectedRange();

  nsAutoString str;
  nsCocoaUtils::GetStringForNSString([aAttrString string], str);
  if (!IsIMEComposing() && str.IsEmpty()) {
    // nothing to do if there is no content which can be removed.
    if (!isEditable) {
      return;
    }
    // If replacement range is specified, we need to remove the range.
    // Otherwise, we need to remove the selected range if it's not collapsed.
    if (aReplacementRange && aReplacementRange->location != NSNotFound) {
      // nothing to do since the range is collapsed.
      if (aReplacementRange->length == 0) {
        return;
      }
      // If the replacement range is different from current selected range,
      // select the range.
      if (!NSEqualRanges(selectedRange, *aReplacementRange)) {
        NS_ENSURE_TRUE_VOID(SetSelection(*aReplacementRange));
      }
      selectedRange = SelectedRange();
    }
    NS_ENSURE_TRUE_VOID(selectedRange.location != NSNotFound);
    if (selectedRange.length == 0) {
      return; // nothing to do
    }
    // If this is caused by a key input, the keypress event which will be
    // dispatched later should cause the delete.  Therefore, nothing to do here.
    // Although, we're not sure if such case is actually possible.
    if (!currentKeyEvent) {
      return;
    }
    // Delete the selected range.
    nsRefPtr<TextInputHandler> kungFuDeathGrip(this);
    WidgetContentCommandEvent deleteCommandEvent(true,
                                                 NS_CONTENT_COMMAND_DELETE,
                                                 mWidget);
    DispatchEvent(deleteCommandEvent);
    NS_ENSURE_TRUE_VOID(deleteCommandEvent.mSucceeded);
    // Be aware! The widget might be destroyed here.
    return;
  }

  if (str.Length() != 1 || IsIMEComposing()) {
    InsertTextAsCommittingComposition(aAttrString, aReplacementRange);
    return;
  }

  // Don't let the same event be fired twice when hitting
  // enter/return! (Bug 420502)
  if (currentKeyEvent && !currentKeyEvent->CanDispatchKeyPressEvent()) {
    return;
  }

  nsRefPtr<nsChildView> kungFuDeathGrip(mWidget);

  // If the replacement range is specified, select the range.  Then, the
  // selection will be replaced by the later keypress event.
  if (isEditable &&
      aReplacementRange && aReplacementRange->location != NSNotFound &&
      !NSEqualRanges(selectedRange, *aReplacementRange)) {
    NS_ENSURE_TRUE_VOID(SetSelection(*aReplacementRange));
  }

  // Dispatch keypress event with char instead of compositionchange event
  WidgetKeyboardEvent keypressEvent(true, NS_KEY_PRESS, mWidget);
  keypressEvent.isChar = IsPrintableChar(str.CharAt(0));

  // Don't set other modifiers from the current event, because here in
  // -insertText: they've already been taken into account in creating
  // the input string.

  if (currentKeyEvent) {
    NSEvent* keyEvent = currentKeyEvent->mKeyEvent;
    InitKeyEvent(keyEvent, keypressEvent, &str);
  } else {
    nsCocoaUtils::InitInputEvent(keypressEvent, static_cast<NSEvent*>(nullptr));
    if (keypressEvent.isChar) {
      keypressEvent.charCode = str.CharAt(0);
    }
    // Note that insertText is not called only at key pressing.
    if (!keypressEvent.charCode) {
      keypressEvent.keyCode =
        WidgetUtils::ComputeKeyCodeFromChar(keypressEvent.charCode);
    }
  }

  // Remove basic modifiers from keypress event because if they are included,
  // nsPlaintextEditor ignores the event.
  keypressEvent.modifiers &= ~(MODIFIER_CONTROL |
                               MODIFIER_ALT |
                               MODIFIER_META);

  // TODO:
  // If mCurrentKeyEvent.mKeyEvent is null and when we implement textInput
  // event of DOM3 Events, we should dispatch it instead of keypress event.
  bool keyPressHandled = DispatchEvent(keypressEvent);

  // Note: mWidget might have become null here. Don't count on it from here on.

  if (currentKeyEvent) {
    currentKeyEvent->mKeyPressHandled = keyPressHandled;
    currentKeyEvent->mKeyPressDispatched = true;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

bool
TextInputHandler::DoCommandBySelector(const char* aSelector)
{
  nsRefPtr<nsChildView> kungFuDeathGrip(mWidget);

  KeyEventState* currentKeyEvent = GetCurrentKeyEvent();

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TextInputHandler::DoCommandBySelector, aSelector=\"%s\", "
     "Destroyed()=%s, keydownHandled=%s, keypressHandled=%s, "
     "causedOtherKeyEvents=%s",
     this, aSelector ? aSelector : "", TrueOrFalse(Destroyed()),
     currentKeyEvent ?
       TrueOrFalse(currentKeyEvent->mKeyDownHandled) : "N/A",
     currentKeyEvent ?
       TrueOrFalse(currentKeyEvent->mKeyPressHandled) : "N/A",
     currentKeyEvent ?
       TrueOrFalse(currentKeyEvent->mCausedOtherKeyEvents) : "N/A"));

  if (currentKeyEvent && currentKeyEvent->CanDispatchKeyPressEvent()) {
    WidgetKeyboardEvent keypressEvent(true, NS_KEY_PRESS, mWidget);
    InitKeyEvent(currentKeyEvent->mKeyEvent, keypressEvent);
    currentKeyEvent->mKeyPressHandled = DispatchEvent(keypressEvent);
    currentKeyEvent->mKeyPressDispatched = true;
    MOZ_LOG(gLog, LogLevel::Info,
      ("%p TextInputHandler::DoCommandBySelector, keypress event "
       "dispatched, Destroyed()=%s, keypressHandled=%s",
       this, TrueOrFalse(Destroyed()),
       TrueOrFalse(currentKeyEvent->mKeyPressHandled)));
  }

  return (!Destroyed() && currentKeyEvent &&
          currentKeyEvent->IsDefaultPrevented());
}


#pragma mark -


/******************************************************************************
 *
 *  IMEInputHandler implementation (static methods)
 *
 ******************************************************************************/

bool IMEInputHandler::sStaticMembersInitialized = false;
bool IMEInputHandler::sCachedIsForRTLLangage = false;
CFStringRef IMEInputHandler::sLatestIMEOpenedModeInputSourceID = nullptr;
IMEInputHandler* IMEInputHandler::sFocusedIMEHandler = nullptr;

// static
void
IMEInputHandler::InitStaticMembers()
{
  if (sStaticMembersInitialized)
    return;
  sStaticMembersInitialized = true;
  // We need to check the keyboard layout changes on all applications.
  CFNotificationCenterRef center = ::CFNotificationCenterGetDistributedCenter();
  // XXX Don't we need to remove the observer at shut down?
  // Mac Dev Center's document doesn't say how to remove the observer if
  // the second parameter is NULL.
  ::CFNotificationCenterAddObserver(center, NULL,
      OnCurrentTextInputSourceChange,
      kTISNotifySelectedKeyboardInputSourceChanged, NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);
  // Initiailize with the current keyboard layout
  OnCurrentTextInputSourceChange(NULL, NULL,
                                 kTISNotifySelectedKeyboardInputSourceChanged,
                                 NULL, NULL);
}

// static
void
IMEInputHandler::OnCurrentTextInputSourceChange(CFNotificationCenterRef aCenter,
                                                void* aObserver,
                                                CFStringRef aName,
                                                const void* aObject,
                                                CFDictionaryRef aUserInfo)
{
  // Cache the latest IME opened mode to sLatestIMEOpenedModeInputSourceID.
  TISInputSourceWrapper tis;
  tis.InitByCurrentInputSource();
  if (tis.IsOpenedIMEMode()) {
    tis.GetInputSourceID(sLatestIMEOpenedModeInputSourceID);
  }

  if (MOZ_LOG_TEST(gLog, LogLevel::Info)) {
    static CFStringRef sLastTIS = nullptr;
    CFStringRef newTIS;
    tis.GetInputSourceID(newTIS);
    if (!sLastTIS ||
        ::CFStringCompare(sLastTIS, newTIS, 0) != kCFCompareEqualTo) {
      TISInputSourceWrapper tis1, tis2, tis3, tis4, tis5;
      tis1.InitByCurrentKeyboardLayout();
      tis2.InitByCurrentASCIICapableInputSource();
      tis3.InitByCurrentASCIICapableKeyboardLayout();
      tis4.InitByCurrentInputMethodKeyboardLayoutOverride();
      tis5.InitByTISInputSourceRef(tis.GetKeyboardLayoutInputSource());
      CFStringRef is0 = nullptr, is1 = nullptr, is2 = nullptr, is3 = nullptr,
                  is4 = nullptr, is5 = nullptr, type0 = nullptr,
                  lang0 = nullptr, bundleID0 = nullptr;
      tis.GetInputSourceID(is0);
      tis1.GetInputSourceID(is1);
      tis2.GetInputSourceID(is2);
      tis3.GetInputSourceID(is3);
      tis4.GetInputSourceID(is4);
      tis5.GetInputSourceID(is5);
      tis.GetInputSourceType(type0);
      tis.GetPrimaryLanguage(lang0);
      tis.GetBundleID(bundleID0);

      MOZ_LOG(gLog, LogLevel::Info,
        ("IMEInputHandler::OnCurrentTextInputSourceChange,\n"
         "  Current Input Source is changed to:\n"
         "    currentInputContext=%p\n"
         "    %s\n"
         "      type=%s %s\n"
         "      overridden keyboard layout=%s\n"
         "      used keyboard layout for translation=%s\n"
         "    primary language=%s\n"
         "    bundle ID=%s\n"
         "    current ASCII capable Input Source=%s\n"
         "    current Keyboard Layout=%s\n"
         "    current ASCII capable Keyboard Layout=%s",
         [NSTextInputContext currentInputContext], GetCharacters(is0),
         GetCharacters(type0), tis.IsASCIICapable() ? "- ASCII capable " : "",
         GetCharacters(is4), GetCharacters(is5),
         GetCharacters(lang0), GetCharacters(bundleID0),
         GetCharacters(is2), GetCharacters(is1), GetCharacters(is3)));
    }
    sLastTIS = newTIS;
  }

  /**
   * When the direction is changed, all the children are notified.
   * No need to treat the initial case separately because it is covered
   * by the general case (sCachedIsForRTLLangage is initially false)
   */
  if (sCachedIsForRTLLangage != tis.IsForRTLLanguage()) {
    nsTArray<dom::ContentParent*> children;
    dom::ContentParent::GetAll(children);
    for (uint32_t i = 0; i < children.Length(); i++) {
      unused << children[i]->SendBidiKeyboardNotify(tis.IsForRTLLanguage());
    }
    sCachedIsForRTLLangage = tis.IsForRTLLanguage();
  }
}

// static
void
IMEInputHandler::FlushPendingMethods(nsITimer* aTimer, void* aClosure)
{
  NS_ASSERTION(aClosure, "aClosure is null");
  static_cast<IMEInputHandler*>(aClosure)->ExecutePendingMethods();
}

// static
CFArrayRef
IMEInputHandler::CreateAllIMEModeList()
{
  const void* keys[] = { kTISPropertyInputSourceType };
  const void* values[] = { kTISTypeKeyboardInputMode };
  CFDictionaryRef filter =
    ::CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, NULL, NULL);
  NS_ASSERTION(filter, "failed to create the filter");
  CFArrayRef list = ::TISCreateInputSourceList(filter, true);
  ::CFRelease(filter);
  return list;
}

// static
void
IMEInputHandler::DebugPrintAllIMEModes()
{
  if (MOZ_LOG_TEST(gLog, LogLevel::Info)) {
    CFArrayRef list = CreateAllIMEModeList();
    MOZ_LOG(gLog, LogLevel::Info, ("IME mode configuration:"));
    CFIndex idx = ::CFArrayGetCount(list);
    TISInputSourceWrapper tis;
    for (CFIndex i = 0; i < idx; ++i) {
      TISInputSourceRef inputSource = static_cast<TISInputSourceRef>(
        const_cast<void *>(::CFArrayGetValueAtIndex(list, i)));
      tis.InitByTISInputSourceRef(inputSource);
      nsAutoString name, isid;
      tis.GetLocalizedName(name);
      tis.GetInputSourceID(isid);
      MOZ_LOG(gLog, LogLevel::Info,
             ("  %s\t<%s>%s%s\n",
              NS_ConvertUTF16toUTF8(name).get(),
              NS_ConvertUTF16toUTF8(isid).get(),
              tis.IsASCIICapable() ? "" : "\t(Isn't ASCII capable)",
              tis.IsEnabled() ? "" : "\t(Isn't Enabled)"));
    }
    ::CFRelease(list);
  }
}

//static
TSMDocumentID
IMEInputHandler::GetCurrentTSMDocumentID()
{
  // At least on Mac OS X 10.6.x and 10.7.x, ::TSMGetActiveDocument() has a bug.
  // The result of ::TSMGetActiveDocument() isn't modified for new active text
  // input context until [NSTextInputContext currentInputContext] is called.
  // Therefore, we need to call it here.
  [NSTextInputContext currentInputContext];
  return ::TSMGetActiveDocument();
}


#pragma mark -


/******************************************************************************
 *
 *  IMEInputHandler implementation #1
 *    The methods are releated to the pending methods.  Some jobs should be
 *    run after the stack is finished, e.g, some methods cannot run the jobs
 *    during processing the focus event.  And also some other jobs should be
 *    run at the next focus event is processed.
 *    The pending methods are recorded in mPendingMethods.  They are executed
 *    by ExecutePendingMethods via FlushPendingMethods.
 *
 ******************************************************************************/

void
IMEInputHandler::NotifyIMEOfFocusChangeInGecko()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::NotifyIMEOfFocusChangeInGecko, "
     "Destroyed()=%s, IsFocused()=%s, inputContext=%p",
     this, TrueOrFalse(Destroyed()), TrueOrFalse(IsFocused()),
     mView ? [mView inputContext] : nullptr));

  if (Destroyed()) {
    return;
  }

  if (!IsFocused()) {
    // retry at next focus event
    mPendingMethods |= kNotifyIMEOfFocusChangeInGecko;
    return;
  }

  MOZ_ASSERT(mView);
  NSTextInputContext* inputContext = [mView inputContext];
  NS_ENSURE_TRUE_VOID(inputContext);

  // When an <input> element on a XUL <panel> element gets focus from an <input>
  // element on the opener window of the <panel> element, the owner window
  // still has native focus.  Therefore, IMEs may store the opener window's
  // level at this time because they don't know the actual focus is moved to
  // different window.  If IMEs try to get the newest window level after the
  // focus change, we return the window level of the XUL <panel>'s widget.
  // Therefore, let's emulate the native focus change.  Then, IMEs can refresh
  // the stored window level.
  [inputContext deactivate];
  [inputContext activate];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::DiscardIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::DiscardIMEComposition, "
     "Destroyed()=%s, IsFocused()=%s, mView=%p, inputContext=%p",
     this, TrueOrFalse(Destroyed()), TrueOrFalse(IsFocused()),
     mView, mView ? [mView inputContext] : nullptr));

  if (Destroyed()) {
    return;
  }

  if (!IsFocused()) {
    // retry at next focus event
    mPendingMethods |= kDiscardIMEComposition;
    return;
  }

  NS_ENSURE_TRUE_VOID(mView);
  NSTextInputContext* inputContext = [mView inputContext];
  NS_ENSURE_TRUE_VOID(inputContext);
  mIgnoreIMECommit = true;
  [inputContext discardMarkedText];
  mIgnoreIMECommit = false;

  NS_OBJC_END_TRY_ABORT_BLOCK
}

void
IMEInputHandler::SyncASCIICapableOnly()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::SyncASCIICapableOnly, "
     "Destroyed()=%s, IsFocused()=%s, mIsASCIICapableOnly=%s, "
     "GetCurrentTSMDocumentID()=%p",
     this, TrueOrFalse(Destroyed()), TrueOrFalse(IsFocused()),
     TrueOrFalse(mIsASCIICapableOnly), GetCurrentTSMDocumentID()));

  if (Destroyed()) {
    return;
  }

  if (!IsFocused()) {
    // retry at next focus event
    mPendingMethods |= kSyncASCIICapableOnly;
    return;
  }

  TSMDocumentID doc = GetCurrentTSMDocumentID();
  if (!doc) {
    // retry
    mPendingMethods |= kSyncASCIICapableOnly;
    NS_WARNING("Application is active but there is no active document");
    ResetTimer();
    return;
  }

  if (mIsASCIICapableOnly) {
    CFArrayRef ASCIICapableTISList = ::TISCreateASCIICapableInputSourceList();
    ::TSMSetDocumentProperty(doc,
                             kTSMDocumentEnabledInputSourcesPropertyTag,
                             sizeof(CFArrayRef),
                             &ASCIICapableTISList);
    ::CFRelease(ASCIICapableTISList);
  } else {
    ::TSMRemoveDocumentProperty(doc,
                                kTSMDocumentEnabledInputSourcesPropertyTag);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::ResetTimer()
{
  NS_ASSERTION(mPendingMethods != 0,
               "There are not pending methods, why this is called?");
  if (mTimer) {
    mTimer->Cancel();
  } else {
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    NS_ENSURE_TRUE(mTimer, );
  }
  mTimer->InitWithFuncCallback(FlushPendingMethods, this, 0,
                               nsITimer::TYPE_ONE_SHOT);
}

void
IMEInputHandler::ExecutePendingMethods()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  if (![[NSApplication sharedApplication] isActive]) {
    mIsInFocusProcessing = false;
    // If we're not active, we should retry at focus event
    return;
  }

  uint32_t pendingMethods = mPendingMethods;
  // First, reset the pending method flags because if each methods cannot
  // run now, they can reentry to the pending flags by theirselves.
  mPendingMethods = 0;

  if (pendingMethods & kDiscardIMEComposition)
    DiscardIMEComposition();
  if (pendingMethods & kSyncASCIICapableOnly)
    SyncASCIICapableOnly();
  if (pendingMethods & kNotifyIMEOfFocusChangeInGecko) {
    NotifyIMEOfFocusChangeInGecko();
  }

  mIsInFocusProcessing = false;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#pragma mark -


/******************************************************************************
 *
 * IMEInputHandler implementation (native event handlers)
 *
 ******************************************************************************/

uint32_t
IMEInputHandler::ConvertToTextRangeType(uint32_t aUnderlineStyle,
                                        NSRange& aSelectedRange)
{
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::ConvertToTextRangeType, "
     "aUnderlineStyle=%llu, aSelectedRange.length=%llu,",
     this, aUnderlineStyle, aSelectedRange.length));

  // We assume that aUnderlineStyle is NSUnderlineStyleSingle or
  // NSUnderlineStyleThick.  NSUnderlineStyleThick should indicate a selected
  // clause.  Otherwise, should indicate non-selected clause.

  if (aSelectedRange.length == 0) {
    switch (aUnderlineStyle) {
      case NSUnderlineStyleSingle:
        return NS_TEXTRANGE_RAWINPUT;
      case NSUnderlineStyleThick:
        return NS_TEXTRANGE_SELECTEDRAWTEXT;
      default:
        NS_WARNING("Unexpected line style");
        return NS_TEXTRANGE_SELECTEDRAWTEXT;
    }
  }

  switch (aUnderlineStyle) {
    case NSUnderlineStyleSingle:
      return NS_TEXTRANGE_CONVERTEDTEXT;
    case NSUnderlineStyleThick:
      return NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
    default:
      NS_WARNING("Unexpected line style");
      return NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
  }
}

uint32_t
IMEInputHandler::GetRangeCount(NSAttributedString *aAttrString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // Iterate through aAttrString for the NSUnderlineStyleAttributeName and
  // count the different segments adjusting limitRange as we go.
  uint32_t count = 0;
  NSRange effectiveRange;
  NSRange limitRange = NSMakeRange(0, [aAttrString length]);
  while (limitRange.length > 0) {
    [aAttrString  attribute:NSUnderlineStyleAttributeName 
                    atIndex:limitRange.location 
      longestEffectiveRange:&effectiveRange
                    inRange:limitRange];
    limitRange =
      NSMakeRange(NSMaxRange(effectiveRange), 
                  NSMaxRange(limitRange) - NSMaxRange(effectiveRange));
    count++;
  }

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::GetRangeCount, aAttrString=\"%s\", count=%llu",
     this, GetCharacters([aAttrString string]), count));

  return count;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

already_AddRefed<mozilla::TextRangeArray>
IMEInputHandler::CreateTextRangeArray(NSAttributedString *aAttrString,
                                      NSRange& aSelectedRange)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // Convert the Cocoa range into the TextRange Array used in Gecko.
  // Iterate through the attributed string and map the underline attribute to
  // Gecko IME textrange attributes.  We may need to change the code here if
  // we change the implementation of validAttributesForMarkedText.
  NSRange limitRange = NSMakeRange(0, [aAttrString length]);
  uint32_t rangeCount = GetRangeCount(aAttrString);
  nsRefPtr<mozilla::TextRangeArray> textRangeArray =
                                      new mozilla::TextRangeArray();
  for (uint32_t i = 0; i < rangeCount && limitRange.length > 0; i++) {
    NSRange effectiveRange;
    id attributeValue = [aAttrString attribute:NSUnderlineStyleAttributeName
                                       atIndex:limitRange.location
                         longestEffectiveRange:&effectiveRange
                                       inRange:limitRange];

    TextRange range;
    range.mStartOffset = effectiveRange.location;
    range.mEndOffset = NSMaxRange(effectiveRange);
    range.mRangeType =
      ConvertToTextRangeType([attributeValue intValue], aSelectedRange);
    textRangeArray->AppendElement(range);

    MOZ_LOG(gLog, LogLevel::Info,
      ("%p IMEInputHandler::CreateTextRangeArray, "
       "range={ mStartOffset=%llu, mEndOffset=%llu, mRangeType=%s }",
       this, range.mStartOffset, range.mEndOffset,
       GetRangeTypeName(range.mRangeType)));

    limitRange =
      NSMakeRange(NSMaxRange(effectiveRange), 
                  NSMaxRange(limitRange) - NSMaxRange(effectiveRange));
  }

  // Get current caret position.
  TextRange range;
  range.mStartOffset = aSelectedRange.location + aSelectedRange.length;
  range.mEndOffset = range.mStartOffset;
  range.mRangeType = NS_TEXTRANGE_CARETPOSITION;
  textRangeArray->AppendElement(range);

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::CreateTextRangeArray, "
     "range={ mStartOffset=%llu, mEndOffset=%llu, mRangeType=%s }",
     this, range.mStartOffset, range.mEndOffset,
     GetRangeTypeName(range.mRangeType)));

  return textRangeArray.forget();

  NS_OBJC_END_TRY_ABORT_BLOCK_NSNULL;
}

bool
IMEInputHandler::DispatchCompositionChangeEvent(const nsString& aText,
                                                NSAttributedString* aAttrString,
                                                NSRange& aSelectedRange)
{
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::DispatchCompositionChangeEvent, "
     "aText=\"%s\", aAttrString=\"%s\", "
     "aSelectedRange={ location=%llu, length=%llu }, "
     "Destroyed()=%s",
     this, NS_ConvertUTF16toUTF8(aText).get(),
     GetCharacters([aAttrString string]),
     aSelectedRange.location, aSelectedRange.length,
     TrueOrFalse(Destroyed())));

  NS_ENSURE_TRUE(!Destroyed(), false);

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  WidgetCompositionEvent compositionChangeEvent(true, NS_COMPOSITION_CHANGE,
                                                mWidget);
  compositionChangeEvent.time = PR_IntervalNow();
  compositionChangeEvent.mData = aText;
  compositionChangeEvent.mRanges =
    CreateTextRangeArray(aAttrString, aSelectedRange);
  return DispatchEvent(compositionChangeEvent);
}

bool
IMEInputHandler::DispatchCompositionCommitEvent(const nsAString* aCommitString)
{
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::DispatchCompositionCommitEvent, "
     "aCommitString=0x%p (\"%s\"), Destroyed()=%s",
     this, aCommitString,
     aCommitString ? NS_ConvertUTF16toUTF8(*aCommitString).get() : "",
     TrueOrFalse(Destroyed())));

  if (NS_WARN_IF(Destroyed())) {
    return false;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  EventMessage message =
    aCommitString ? NS_COMPOSITION_COMMIT : NS_COMPOSITION_COMMIT_AS_IS;
  WidgetCompositionEvent compositionCommitEvent(true, message, mWidget);
  compositionCommitEvent.time = PR_IntervalNow();
  if (aCommitString) {
    compositionCommitEvent.mData = *aCommitString;
  }
  return DispatchEvent(compositionCommitEvent);
}

void
IMEInputHandler::InitCompositionEvent(WidgetCompositionEvent& aCompositionEvent)
{
  aCompositionEvent.time = PR_IntervalNow();
}

void
IMEInputHandler::InsertTextAsCommittingComposition(
                   NSAttributedString* aAttrString,
                   NSRange* aReplacementRange)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::InsertTextAsCommittingComposition, "
     "aAttrString=\"%s\", aReplacementRange=%p { location=%llu, length=%llu }, "
     "Destroyed()=%s, IsIMEComposing()=%s, "
     "mMarkedRange={ location=%llu, length=%llu }",
     this, GetCharacters([aAttrString string]), aReplacementRange,
     aReplacementRange ? aReplacementRange->location : 0,
     aReplacementRange ? aReplacementRange->length : 0,
     TrueOrFalse(Destroyed()), TrueOrFalse(IsIMEComposing()),
     mMarkedRange.location, mMarkedRange.length));

  if (IgnoreIMECommit()) {
    MOZ_CRASH("IMEInputHandler::InsertTextAsCommittingComposition() must not"
              "be called while canceling the composition");
  }

  if (Destroyed()) {
    return;
  }

  // First, commit current composition with the latest composition string if the
  // replacement range is different from marked range.
  if (IsIMEComposing() && aReplacementRange &&
      aReplacementRange->location != NSNotFound &&
      !NSEqualRanges(MarkedRange(), *aReplacementRange)) {
    DispatchCompositionCommitEvent();
    if (Destroyed()) {
      MOZ_LOG(gLog, LogLevel::Info,
        ("%p IMEInputHandler::InsertTextAsCommittingComposition, "
         "destroyed by commiting composition for setting replacement range",
         this));
      return;
    }
    OnEndIMEComposition();
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsString str;
  nsCocoaUtils::GetStringForNSString([aAttrString string], str);

  if (!IsIMEComposing()) {
    // If there is no selection and replacement range is specified, set the
    // range as selection.
    if (aReplacementRange && aReplacementRange->location != NSNotFound &&
        !NSEqualRanges(SelectedRange(), *aReplacementRange)) {
      NS_ENSURE_TRUE_VOID(SetSelection(*aReplacementRange));
    }

    // XXXmnakano Probably, we shouldn't emulate composition in this case.
    // I think that we should just fire DOM3 textInput event if we implement it.
    WidgetCompositionEvent compStart(true, NS_COMPOSITION_START, mWidget);
    InitCompositionEvent(compStart);

    DispatchEvent(compStart);
    if (Destroyed()) {
      MOZ_LOG(gLog, LogLevel::Info,
        ("%p IMEInputHandler::InsertTextAsCommittingComposition, "
         "destroyed by compositionstart event", this));
      return;
    }

    OnStartIMEComposition();
  }

  DispatchCompositionCommitEvent(&str);
  if (Destroyed()) {
    MOZ_LOG(gLog, LogLevel::Info,
      ("%p IMEInputHandler::InsertTextAsCommittingComposition, "
       "destroyed by compositioncommit event", this));
    return;
  }

  OnEndIMEComposition();

  mMarkedRange = NSMakeRange(NSNotFound, 0);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::SetMarkedText(NSAttributedString* aAttrString,
                               NSRange& aSelectedRange,
                               NSRange* aReplacementRange)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::SetMarkedText, "
     "aAttrString=\"%s\", aSelectedRange={ location=%llu, length=%llu }, "
     "aReplacementRange=%p { location=%llu, length=%llu }, "
     "Destroyed()=%s, IgnoreIMEComposition()=%s, IsIMEComposing()=%s, "
     "mMarkedRange={ location=%llu, length=%llu }",
     this, GetCharacters([aAttrString string]),
     aSelectedRange.location, aSelectedRange.length, aReplacementRange,
     aReplacementRange ? aReplacementRange->location : 0,
     aReplacementRange ? aReplacementRange->length : 0,
     TrueOrFalse(Destroyed()), TrueOrFalse(IgnoreIMEComposition()),
     TrueOrFalse(IsIMEComposing()),
     mMarkedRange.location, mMarkedRange.length));

  if (Destroyed() || IgnoreIMEComposition()) {
    return;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  // First, commit current composition with the latest composition string if the
  // replacement range is different from marked range.
  if (IsIMEComposing() && aReplacementRange &&
      aReplacementRange->location != NSNotFound &&
      !NSEqualRanges(MarkedRange(), *aReplacementRange)) {
    bool ignoreIMECommit = mIgnoreIMECommit;
    mIgnoreIMECommit = false;
    DispatchCompositionCommitEvent();
    mIgnoreIMECommit = ignoreIMECommit;
    if (Destroyed()) {
      MOZ_LOG(gLog, LogLevel::Info,
        ("%p IMEInputHandler::SetMarkedText, "
         "destroyed by commiting composition for setting replacement range",
         this));
      return;
    }
    OnEndIMEComposition();
  }

  nsString str;
  nsCocoaUtils::GetStringForNSString([aAttrString string], str);

  mMarkedRange.length = str.Length();

  if (!IsIMEComposing() && !str.IsEmpty()) {
    // If there is no selection and replacement range is specified, set the
    // range as selection.
    if (aReplacementRange && aReplacementRange->location != NSNotFound &&
        !NSEqualRanges(SelectedRange(), *aReplacementRange)) {
      NS_ENSURE_TRUE_VOID(SetSelection(*aReplacementRange));
    }

    mMarkedRange.location = SelectedRange().location;

    WidgetCompositionEvent compStart(true, NS_COMPOSITION_START, mWidget);
    InitCompositionEvent(compStart);

    DispatchEvent(compStart);
    if (Destroyed()) {
      MOZ_LOG(gLog, LogLevel::Info,
        ("%p IMEInputHandler::SetMarkedText, "
         "destroyed by compositionstart event", this));
      return;
    }

    OnStartIMEComposition();
  }

  if (!IsIMEComposing()) {
    return;
  }

  if (!str.IsEmpty()) {
    OnUpdateIMEComposition([aAttrString string]);

    // Set temprary range for Apple Japanese IME with e10s because
    // SelectedRange may return invalid range until OnSelectionChange is
    // called from content process.
    // This value will be updated by OnSelectionChange soon.
    mSelectedRange.location = aSelectedRange.location + mMarkedRange.location;
    mSelectedRange.length = aSelectedRange.length;

    DispatchCompositionChangeEvent(str, aAttrString, aSelectedRange);
    if (Destroyed()) {
      MOZ_LOG(gLog, LogLevel::Info,
        ("%p IMEInputHandler::SetMarkedText, "
         "destroyed by compositionchange event", this));
    }
    return;
  }

  // If the composition string becomes empty string, we should commit
  // current composition.
  DispatchCompositionCommitEvent(&EmptyString());
  if (Destroyed()) {
    MOZ_LOG(gLog, LogLevel::Info,
      ("%p IMEInputHandler::SetMarkedText, "
       "destroyed by compositioncommit event", this));
    return;
  }
  OnEndIMEComposition();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NSInteger
IMEInputHandler::ConversationIdentifier()
{
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::ConversationIdentifier, Destroyed()=%s",
     this, TrueOrFalse(Destroyed())));

  if (Destroyed()) {
    return reinterpret_cast<NSInteger>(mView);
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  // NOTE: The size of NSInteger is same as pointer size.
  WidgetQueryContentEvent textContent(true, NS_QUERY_TEXT_CONTENT, mWidget);
  textContent.InitForQueryTextContent(0, 0);
  DispatchEvent(textContent);
  if (!textContent.mSucceeded) {
    MOZ_LOG(gLog, LogLevel::Info,
      ("%p IMEInputHandler::ConversationIdentifier, Failed", this));
    return reinterpret_cast<NSInteger>(mView);
  }
  // XXX This might return same ID as a previously existing editor if the
  //     deleted editor was created at the same address.  Is there a better way?
  return reinterpret_cast<NSInteger>(textContent.mReply.mContentsRoot);
}

NSAttributedString*
IMEInputHandler::GetAttributedSubstringFromRange(NSRange& aRange,
                                                 NSRange* aActualRange)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::GetAttributedSubstringFromRange, "
     "aRange={ location=%llu, length=%llu }, aActualRange=%p, Destroyed()=%s",
     this, aRange.location, aRange.length, aActualRange,
     TrueOrFalse(Destroyed())));

  if (aActualRange) {
    *aActualRange = NSMakeRange(NSNotFound, 0);
  }

  if (Destroyed() || aRange.location == NSNotFound || aRange.length == 0) {
    return nil;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsAutoString str;
  WidgetQueryContentEvent textContent(true, NS_QUERY_TEXT_CONTENT, mWidget);
  textContent.InitForQueryTextContent(aRange.location, aRange.length);
  textContent.RequestFontRanges();
  DispatchEvent(textContent);

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::GetAttributedSubstringFromRange, "
     "textContent={ mSucceeded=%s, mReply={ mString=\"%s\", mOffset=%llu } }",
     this, TrueOrFalse(textContent.mSucceeded),
     NS_ConvertUTF16toUTF8(textContent.mReply.mString).get(),
     textContent.mReply.mOffset));

  if (!textContent.mSucceeded) {
    return nil;
  }

  NSString* nsstr = nsCocoaUtils::ToNSString(textContent.mReply.mString);
  NSMutableAttributedString* result =
    [[[NSMutableAttributedString alloc] initWithString:nsstr
                                            attributes:nil] autorelease];
  const nsTArray<FontRange>& fontRanges = textContent.mReply.mFontRanges;
  int32_t lastOffset = textContent.mReply.mString.Length();
  for (auto i = fontRanges.Length(); i > 0; --i) {
    const FontRange& fontRange = fontRanges[i - 1];
    NSString* fontName = nsCocoaUtils::ToNSString(fontRange.mFontName);
    CGFloat fontSize = fontRange.mFontSize / mWidget->BackingScaleFactor();
    NSFont* font = [NSFont fontWithName:fontName size:fontSize];
    if (!font) {
      font = [NSFont systemFontOfSize:fontSize];
    }

    NSDictionary* attrs = @{ NSFontAttributeName: font };
    NSRange range = NSMakeRange(fontRange.mStartOffset,
                                lastOffset - fontRange.mStartOffset);
    [result setAttributes:attrs range:range];
    lastOffset = fontRange.mStartOffset;
  }
  if (aActualRange) {
    aActualRange->location = textContent.mReply.mOffset;
    aActualRange->length = textContent.mReply.mString.Length();
  }
  return result;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

bool
IMEInputHandler::HasMarkedText()
{
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::HasMarkedText, "
     "mMarkedRange={ location=%llu, length=%llu }",
     this, mMarkedRange.location, mMarkedRange.length));

  return (mMarkedRange.location != NSNotFound) && (mMarkedRange.length != 0);
}

NSRange
IMEInputHandler::MarkedRange()
{
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::MarkedRange, "
     "mMarkedRange={ location=%llu, length=%llu }",
     this, mMarkedRange.location, mMarkedRange.length));

  if (!HasMarkedText()) {
    return NSMakeRange(NSNotFound, 0);
  }
  return mMarkedRange;
}

NSRange
IMEInputHandler::SelectedRange()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::SelectedRange, Destroyed()=%s, mSelectedRange={ "
     "location=%llu, length=%llu }",
     this, TrueOrFalse(Destroyed()), mSelectedRange.location,
     mSelectedRange.length));

  if (Destroyed()) {
    return mSelectedRange;
  }

  if (mSelectedRange.location != NSNotFound) {
    MOZ_ASSERT(mIMEHasFocus);
    return mSelectedRange;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  WidgetQueryContentEvent selection(true, NS_QUERY_SELECTED_TEXT, mWidget);
  DispatchEvent(selection);

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::SelectedRange, selection={ mSucceeded=%s, "
     "mReply={ mOffset=%llu, mString.Length()=%llu } }",
     this, TrueOrFalse(selection.mSucceeded), selection.mReply.mOffset,
     selection.mReply.mString.Length()));

  if (!selection.mSucceeded) {
    return mSelectedRange;
  }

  mWritingMode = selection.GetWritingMode();
  mRangeForWritingMode = NSMakeRange(selection.mReply.mOffset,
                                     selection.mReply.mString.Length());

  if (mIMEHasFocus) {
    mSelectedRange = mRangeForWritingMode;
  }

  return mRangeForWritingMode;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(mSelectedRange);
}

bool
IMEInputHandler::DrawsVerticallyForCharacterAtIndex(uint32_t aCharIndex)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (Destroyed()) {
    return false;
  }

  if (mRangeForWritingMode.location == NSNotFound) {
    // Update cached writing-mode value for the current selection.
    SelectedRange();
  }

  if (aCharIndex < mRangeForWritingMode.location ||
      aCharIndex > mRangeForWritingMode.location + mRangeForWritingMode.length) {
    // It's not clear to me whether this ever happens in practice, but if an
    // IME ever wants to query writing mode at an offset outside the current
    // selection, the writing-mode value may not be correct for the index.
    // In that case, use FirstRectForCharacterRange to get a fresh value.
    // This does more work than strictly necessary (we don't need the rect here),
    // but should be a rare case.
    NS_WARNING("DrawsVerticallyForCharacterAtIndex not using cached writing mode");
    NSRange range = NSMakeRange(aCharIndex, 1);
    NSRange actualRange;
    FirstRectForCharacterRange(range, &actualRange);
  }

  return mWritingMode.IsVertical();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

NSRect
IMEInputHandler::FirstRectForCharacterRange(NSRange& aRange,
                                            NSRange* aActualRange)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::FirstRectForCharacterRange, Destroyed()=%s, "
     "aRange={ location=%llu, length=%llu }, aActualRange=%p }",
     this, TrueOrFalse(Destroyed()), aRange.location, aRange.length,
     aActualRange));

  // XXX this returns first character rect or caret rect, it is limitation of
  // now. We need more work for returns first line rect. But current
  // implementation is enough for IMEs.

  NSRect rect = NSMakeRect(0.0, 0.0, 0.0, 0.0);
  NSRange actualRange = NSMakeRange(NSNotFound, 0);
  if (aActualRange) {
    *aActualRange = actualRange;
  }
  if (Destroyed() || aRange.location == NSNotFound) {
    return rect;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  LayoutDeviceIntRect r;
  bool useCaretRect = (aRange.length == 0);
  if (!useCaretRect) {
    WidgetQueryContentEvent charRect(true, NS_QUERY_TEXT_RECT, mWidget);
    charRect.InitForQueryTextRect(aRange.location, 1);
    DispatchEvent(charRect);
    if (charRect.mSucceeded) {
      r = charRect.mReply.mRect;
      actualRange.location = charRect.mReply.mOffset;
      actualRange.length = charRect.mReply.mString.Length();
      mWritingMode = charRect.GetWritingMode();
      mRangeForWritingMode = actualRange;
    } else {
      useCaretRect = true;
    }
  }

  if (useCaretRect) {
    WidgetQueryContentEvent caretRect(true, NS_QUERY_CARET_RECT, mWidget);
    caretRect.InitForQueryCaretRect(aRange.location);
    DispatchEvent(caretRect);
    if (!caretRect.mSucceeded) {
      return rect;
    }
    r = caretRect.mReply.mRect;
    r.width = 0;
    actualRange.location = caretRect.mReply.mOffset;
    actualRange.length = 0;
  }

  nsIWidget* rootWidget = mWidget->GetTopLevelWidget();
  NSWindow* rootWindow =
    static_cast<NSWindow*>(rootWidget->GetNativeData(NS_NATIVE_WINDOW));
  NSView* rootView =
    static_cast<NSView*>(rootWidget->GetNativeData(NS_NATIVE_WIDGET));
  if (!rootWindow || !rootView) {
    return rect;
  }
  rect = nsCocoaUtils::DevPixelsToCocoaPoints(LayoutDevicePixel::ToUntyped(r),
                                              mWidget->BackingScaleFactor());
  rect = [rootView convertRect:rect toView:nil];
  rect.origin = [rootWindow convertBaseToScreen:rect.origin];

  if (aActualRange) {
    *aActualRange = actualRange;
  }

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::FirstRectForCharacterRange, "
     "useCaretRect=%s rect={ x=%f, y=%f, width=%f, height=%f }, "
     "actualRange={ location=%llu, length=%llu }",
     this, TrueOrFalse(useCaretRect), rect.origin.x, rect.origin.y,
     rect.size.width, rect.size.height, actualRange.location,
     actualRange.length));

  return rect;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRect(0.0, 0.0, 0.0, 0.0));
}

NSUInteger
IMEInputHandler::CharacterIndexForPoint(NSPoint& aPoint)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::CharacterIndexForPoint, aPoint={ x=%f, y=%f }",
     this, aPoint.x, aPoint.y));

  NSWindow* mainWindow = [NSApp mainWindow];
  if (!mWidget || !mainWindow) {
    return NSNotFound;
  }

  WidgetQueryContentEvent charAt(true, NS_QUERY_CHARACTER_AT_POINT, mWidget);
  NSPoint ptInWindow = [mainWindow convertScreenToBase:aPoint];
  NSPoint ptInView = [mView convertPoint:ptInWindow fromView:nil];
  charAt.refPoint.x =
    static_cast<int32_t>(ptInView.x) * mWidget->BackingScaleFactor();
  charAt.refPoint.y =
    static_cast<int32_t>(ptInView.y) * mWidget->BackingScaleFactor();
  mWidget->DispatchWindowEvent(charAt);
  if (!charAt.mSucceeded ||
      charAt.mReply.mOffset == WidgetQueryContentEvent::NOT_FOUND ||
      charAt.mReply.mOffset >= static_cast<uint32_t>(NSNotFound)) {
    return NSNotFound;
  }

  return charAt.mReply.mOffset;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSNotFound);
}

NSArray*
IMEInputHandler::GetValidAttributesForMarkedText()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::GetValidAttributesForMarkedText", this));

  //nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  //return [NSArray arrayWithObjects:NSUnderlineStyleAttributeName,
  //                                 NSMarkedClauseSegmentAttributeName,
  //                                 NSTextInputReplacementRangeAttributeName,
  //                                 nil];
  // empty array; we don't support any attributes right now
  return [NSArray array];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}


#pragma mark -


/******************************************************************************
 *
 *  IMEInputHandler implementation #2
 *
 ******************************************************************************/

IMEInputHandler::IMEInputHandler(nsChildView* aWidget,
                                 NSView<mozView> *aNativeView) :
  TextInputHandlerBase(aWidget, aNativeView),
  mPendingMethods(0), mIMECompositionString(nullptr),
  mIsIMEComposing(false), mIsIMEEnabled(true),
  mIsASCIICapableOnly(false), mIgnoreIMECommit(false),
  mIsInFocusProcessing(false), mIMEHasFocus(false)
{
  InitStaticMembers();

  mMarkedRange.location = NSNotFound;
  mMarkedRange.length = 0;
  mSelectedRange.location = NSNotFound;
  mSelectedRange.length = 0;
}

IMEInputHandler::~IMEInputHandler()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
  if (sFocusedIMEHandler == this) {
    sFocusedIMEHandler = nullptr;
  }
}

void
IMEInputHandler::OnFocusChangeInGecko(bool aFocus)
{
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::OnFocusChangeInGecko, aFocus=%s, Destroyed()=%s, "
     "sFocusedIMEHandler=%p",
     this, TrueOrFalse(aFocus), TrueOrFalse(Destroyed()), sFocusedIMEHandler));

  mSelectedRange.location = NSNotFound; // Marking dirty
  mIMEHasFocus = aFocus;

  // This is called when the native focus is changed and when the native focus
  // isn't changed but the focus is changed in Gecko.
  if (!aFocus) {
    if (sFocusedIMEHandler == this)
      sFocusedIMEHandler = nullptr;
    return;
  }

  sFocusedIMEHandler = this;
  mIsInFocusProcessing = true;

  // We need to notify IME of focus change in Gecko as native focus change
  // because the window level of the focused element in Gecko may be changed.
  mPendingMethods |= kNotifyIMEOfFocusChangeInGecko;
  ResetTimer();
}

bool
IMEInputHandler::OnDestroyWidget(nsChildView* aDestroyingWidget)
{
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::OnDestroyWidget, aDestroyingWidget=%p, "
     "sFocusedIMEHandler=%p, IsIMEComposing()=%s",
     this, aDestroyingWidget, sFocusedIMEHandler,
     TrueOrFalse(IsIMEComposing())));

  // If we're not focused, the focused IMEInputHandler may have been
  // created by another widget/nsChildView.
  if (sFocusedIMEHandler && sFocusedIMEHandler != this) {
    sFocusedIMEHandler->OnDestroyWidget(aDestroyingWidget);
  }

  if (!TextInputHandlerBase::OnDestroyWidget(aDestroyingWidget)) {
    return false;
  }

  if (IsIMEComposing()) {
    // If our view is in the composition, we should clean up it.
    CancelIMEComposition();
    OnEndIMEComposition();
  }

  mSelectedRange.location = NSNotFound; // Marking dirty
  mIMEHasFocus = false;

  return true;
}

void
IMEInputHandler::OnStartIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::OnStartIMEComposition, mView=%p, mWidget=%p"
     "inputContext=%p, mIsIMEComposing=%s",
     this, mView, mWidget, mView ? [mView inputContext] : nullptr,
     TrueOrFalse(mIsIMEComposing)));

  NS_ASSERTION(!mIsIMEComposing, "There is a composition already");
  mIsIMEComposing = true;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::OnUpdateIMEComposition(NSString* aIMECompositionString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::OnUpdateIMEComposition, mView=%p, mWidget=%p, "
     "inputContext=%p, mIsIMEComposing=%s, aIMECompositionString=\"%s\"",
     this, mView, mWidget, mView ? [mView inputContext] : nullptr,
     TrueOrFalse(mIsIMEComposing), GetCharacters(aIMECompositionString)));

  NS_ASSERTION(mIsIMEComposing, "We're not in composition");

  if (mIMECompositionString)
    [mIMECompositionString release];
  mIMECompositionString = [aIMECompositionString retain];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::OnEndIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::OnEndIMEComposition, mView=%p, mWidget=%p, "
     "inputContext=%p, mIsIMEComposing=%s",
     this, mView, mWidget, mView ? [mView inputContext] : nullptr,
     TrueOrFalse(mIsIMEComposing)));

  NS_ASSERTION(mIsIMEComposing, "We're not in composition");

  mIsIMEComposing = false;

  if (mIMECompositionString) {
    [mIMECompositionString release];
    mIMECompositionString = nullptr;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::SendCommittedText(NSString *aString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::SendCommittedText, mView=%p, mWidget=%p, "
     "inputContext=%p, mIsIMEComposing=%s",
     this, mView, mWidget, mView ? [mView inputContext] : nullptr,
     TrueOrFalse(mIsIMEComposing), mWidget));

  NS_ENSURE_TRUE(mWidget, );
  // XXX We should send the string without mView.
  if (!mView) {
    return;
  }

  NSAttributedString* attrStr =
    [[NSAttributedString alloc] initWithString:aString];
  [mView insertText:attrStr];
  [attrStr release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::KillIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::KillIMEComposition, mView=%p, mWidget=%p, "
     "inputContext=%p, mIsIMEComposing=%s, "
     "Destroyed()=%s, IsFocused()=%s",
     this, mView, mWidget, mView ? [mView inputContext] : nullptr,
     TrueOrFalse(mIsIMEComposing), TrueOrFalse(Destroyed()),
     TrueOrFalse(IsFocused())));

  if (Destroyed()) {
    return;
  }

  if (IsFocused()) {
    NS_ENSURE_TRUE_VOID(mView);
    NSTextInputContext* inputContext = [mView inputContext];
    NS_ENSURE_TRUE_VOID(inputContext);
    [inputContext discardMarkedText];
    return;
  }

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::KillIMEComposition, Pending...", this));

  // Commit the composition internally.
  SendCommittedText(mIMECompositionString);
  NS_ASSERTION(!mIsIMEComposing, "We're still in a composition");
  // The pending method will be fired by the next focus event.
  mPendingMethods |= kDiscardIMEComposition;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::CommitIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!IsIMEComposing())
    return;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::CommitIMEComposition, mIMECompositionString=%s",
     this, GetCharacters(mIMECompositionString)));

  KillIMEComposition();

  if (!IsIMEComposing())
    return;

  // If the composition is still there, KillIMEComposition only kills the
  // composition in TSM.  We also need to finish the our composition too.
  SendCommittedText(mIMECompositionString);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::CancelIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!IsIMEComposing())
    return;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::CancelIMEComposition, mIMECompositionString=%s",
     this, GetCharacters(mIMECompositionString)));

  // For canceling the current composing, we need to ignore the param of
  // insertText.  But this code is ugly...
  mIgnoreIMECommit = true;
  KillIMEComposition();
  mIgnoreIMECommit = false;

  if (!IsIMEComposing())
    return;

  // If the composition is still there, KillIMEComposition only kills the
  // composition in TSM.  We also need to kill the our composition too.
  SendCommittedText(@"");

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

bool
IMEInputHandler::IsFocused()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ENSURE_TRUE(!Destroyed(), false);
  NSWindow* window = [mView window];
  NS_ENSURE_TRUE(window, false);
  return [window firstResponder] == mView &&
         [window isKeyWindow] &&
         [[NSApplication sharedApplication] isActive];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

bool
IMEInputHandler::IsOrWouldBeFocused()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ENSURE_TRUE(!Destroyed(), false);
  NSWindow* window = [mView window];
  NS_ENSURE_TRUE(window, false);
  return [window firstResponder] == mView && ![window attachedSheet];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

bool
IMEInputHandler::IsIMEOpened()
{
  TISInputSourceWrapper tis;
  tis.InitByCurrentInputSource();
  return tis.IsOpenedIMEMode();
}

void
IMEInputHandler::SetASCIICapableOnly(bool aASCIICapableOnly)
{
  if (aASCIICapableOnly == mIsASCIICapableOnly)
    return;

  CommitIMEComposition();
  mIsASCIICapableOnly = aASCIICapableOnly;
  SyncASCIICapableOnly();
}

void
IMEInputHandler::EnableIME(bool aEnableIME)
{
  if (aEnableIME == mIsIMEEnabled)
    return;

  CommitIMEComposition();
  mIsIMEEnabled = aEnableIME;
}

void
IMEInputHandler::SetIMEOpenState(bool aOpenIME)
{
  if (!IsFocused() || IsIMEOpened() == aOpenIME)
    return;

  if (!aOpenIME) {
    TISInputSourceWrapper tis;
    tis.InitByCurrentASCIICapableInputSource();
    tis.Select();
    return;
  }

  // If we know the latest IME opened mode, we should select it.
  if (sLatestIMEOpenedModeInputSourceID) {
    TISInputSourceWrapper tis;
    tis.InitByInputSourceID(sLatestIMEOpenedModeInputSourceID);
    tis.Select();
    return;
  }

  // XXX If the current input source is a mode of IME, we should turn on it,
  // but we haven't found such way...

  // Finally, we should refer the system locale but this is a little expensive,
  // we shouldn't retry this (if it was succeeded, we already set
  // sLatestIMEOpenedModeInputSourceID at that time).
  static bool sIsPrefferredIMESearched = false;
  if (sIsPrefferredIMESearched)
    return;
  sIsPrefferredIMESearched = true;
  OpenSystemPreferredLanguageIME();
}

void
IMEInputHandler::OpenSystemPreferredLanguageIME()
{
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::OpenSystemPreferredLanguageIME", this));

  CFArrayRef langList = ::CFLocaleCopyPreferredLanguages();
  if (!langList) {
    MOZ_LOG(gLog, LogLevel::Info,
      ("%p IMEInputHandler::OpenSystemPreferredLanguageIME, langList is NULL",
       this));
    return;
  }
  CFIndex count = ::CFArrayGetCount(langList);
  for (CFIndex i = 0; i < count; i++) {
    CFLocaleRef locale =
      ::CFLocaleCreate(kCFAllocatorDefault,
          static_cast<CFStringRef>(::CFArrayGetValueAtIndex(langList, i)));
    if (!locale) {
      continue;
    }

    bool changed = false;
    CFStringRef lang = static_cast<CFStringRef>(
      ::CFLocaleGetValue(locale, kCFLocaleLanguageCode));
    NS_ASSERTION(lang, "lang is null");
    if (lang) {
      TISInputSourceWrapper tis;
      tis.InitByLanguage(lang);
      if (tis.IsOpenedIMEMode()) {
        if (MOZ_LOG_TEST(gLog, LogLevel::Info)) {
          CFStringRef foundTIS;
          tis.GetInputSourceID(foundTIS);
          MOZ_LOG(gLog, LogLevel::Info,
            ("%p IMEInputHandler::OpenSystemPreferredLanguageIME, "
             "foundTIS=%s, lang=%s",
             this, GetCharacters(foundTIS), GetCharacters(lang)));
        }
        tis.Select();
        changed = true;
      }
    }
    ::CFRelease(locale);
    if (changed) {
      break;
    }
  }
  ::CFRelease(langList);
}

void
IMEInputHandler::OnSelectionChange(const IMENotification& aIMENotification)
{
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p IMEInputHandler::OnSelectionChange", this));

  if (aIMENotification.mSelectionChangeData.mOffset == UINT32_MAX) {
    mSelectedRange.location = NSNotFound;
    mSelectedRange.length = 0;
    mRangeForWritingMode.location = NSNotFound;
    mRangeForWritingMode.length = 0;
    return;
  }

  mWritingMode = aIMENotification.mSelectionChangeData.GetWritingMode();
  mRangeForWritingMode =
    NSMakeRange(aIMENotification.mSelectionChangeData.mOffset,
                aIMENotification.mSelectionChangeData.Length());
  if (mIMEHasFocus) {
    mSelectedRange = mRangeForWritingMode;
  }
}

#pragma mark -


/******************************************************************************
 *
 *  TextInputHandlerBase implementation
 *
 ******************************************************************************/

int32_t TextInputHandlerBase::sSecureEventInputCount = 0;

TextInputHandlerBase::TextInputHandlerBase(nsChildView* aWidget,
                                           NSView<mozView> *aNativeView) :
  mWidget(aWidget)
{
  gHandlerInstanceCount++;
  mView = [aNativeView retain];
}

TextInputHandlerBase::~TextInputHandlerBase()
{
  [mView release];
  if (--gHandlerInstanceCount == 0) {
    FinalizeCurrentInputSource();
  }
}

bool
TextInputHandlerBase::OnDestroyWidget(nsChildView* aDestroyingWidget)
{
  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TextInputHandlerBase::OnDestroyWidget, "
     "aDestroyingWidget=%p, mWidget=%p",
     this, aDestroyingWidget, mWidget));

  if (aDestroyingWidget != mWidget) {
    return false;
  }

  mWidget = nullptr;
  return true;
}

bool
TextInputHandlerBase::DispatchEvent(WidgetGUIEvent& aEvent)
{
  if (aEvent.mMessage == NS_KEY_PRESS) {
    WidgetInputEvent& inputEvent = *aEvent.AsInputEvent();
    if (!inputEvent.IsMeta()) {
      MOZ_LOG(gLog, LogLevel::Info,
        ("%p TextInputHandlerBase::DispatchEvent, hiding mouse cursor", this));
      [NSCursor setHiddenUntilMouseMoves:YES];
    }
  }
  return mWidget->DispatchWindowEvent(aEvent);
}

void
TextInputHandlerBase::InitKeyEvent(NSEvent *aNativeKeyEvent,
                                   WidgetKeyboardEvent& aKeyEvent,
                                   const nsAString* aInsertString)
{
  NS_ASSERTION(aNativeKeyEvent, "aNativeKeyEvent must not be NULL");

  if (mKeyboardOverride.mOverrideEnabled) {
    TISInputSourceWrapper tis;
    tis.InitByLayoutID(mKeyboardOverride.mKeyboardLayout, true);
    tis.InitKeyEvent(aNativeKeyEvent, aKeyEvent, aInsertString);
    return;
  }
  TISInputSourceWrapper::CurrentInputSource().
    InitKeyEvent(aNativeKeyEvent, aKeyEvent, aInsertString);
}

nsresult
TextInputHandlerBase::SynthesizeNativeKeyEvent(
                        int32_t aNativeKeyboardLayout,
                        int32_t aNativeKeyCode,
                        uint32_t aModifierFlags,
                        const nsAString& aCharacters,
                        const nsAString& aUnmodifiedCharacters)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  static const uint32_t sModifierFlagMap[][2] = {
    { nsIWidget::CAPS_LOCK,       NSAlphaShiftKeyMask },
    { nsIWidget::SHIFT_L,         NSShiftKeyMask      | 0x0002 },
    { nsIWidget::SHIFT_R,         NSShiftKeyMask      | 0x0004 },
    { nsIWidget::CTRL_L,          NSControlKeyMask    | 0x0001 },
    { nsIWidget::CTRL_R,          NSControlKeyMask    | 0x2000 },
    { nsIWidget::ALT_L,           NSAlternateKeyMask  | 0x0020 },
    { nsIWidget::ALT_R,           NSAlternateKeyMask  | 0x0040 },
    { nsIWidget::COMMAND_L,       NSCommandKeyMask    | 0x0008 },
    { nsIWidget::COMMAND_R,       NSCommandKeyMask    | 0x0010 },
    { nsIWidget::NUMERIC_KEY_PAD, NSNumericPadKeyMask },
    { nsIWidget::HELP,            NSHelpKeyMask },
    { nsIWidget::FUNCTION,        NSFunctionKeyMask }
  };

  uint32_t modifierFlags = 0;
  for (uint32_t i = 0; i < ArrayLength(sModifierFlagMap); ++i) {
    if (aModifierFlags & sModifierFlagMap[i][0]) {
      modifierFlags |= sModifierFlagMap[i][1];
    }
  }

  NSInteger windowNumber = [[mView window] windowNumber];
  bool sendFlagsChangedEvent = IsModifierKey(aNativeKeyCode);
  NSEventType eventType = sendFlagsChangedEvent ? NSFlagsChanged : NSKeyDown;
  NSEvent* downEvent =
    [NSEvent     keyEventWithType:eventType
                         location:NSMakePoint(0,0)
                    modifierFlags:modifierFlags
                        timestamp:0
                     windowNumber:windowNumber
                          context:[NSGraphicsContext currentContext]
                       characters:nsCocoaUtils::ToNSString(aCharacters)
      charactersIgnoringModifiers:nsCocoaUtils::ToNSString(aUnmodifiedCharacters)
                        isARepeat:NO
                          keyCode:aNativeKeyCode];

  NSEvent* upEvent = sendFlagsChangedEvent ?
    nil : nsCocoaUtils::MakeNewCocoaEventWithType(NSKeyUp, downEvent);

  if (downEvent && (sendFlagsChangedEvent || upEvent)) {
    KeyboardLayoutOverride currentLayout = mKeyboardOverride;
    mKeyboardOverride.mKeyboardLayout = aNativeKeyboardLayout;
    mKeyboardOverride.mOverrideEnabled = true;
    [NSApp sendEvent:downEvent];
    if (upEvent) {
      [NSApp sendEvent:upEvent];
    }
    // processKeyDownEvent and keyUp block exceptions so we're sure to
    // reach here to restore mKeyboardOverride
    mKeyboardOverride = currentLayout;
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NSInteger
TextInputHandlerBase::GetWindowLevel()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TextInputHandlerBase::GetWindowLevel, Destryoed()=%s",
     this, TrueOrFalse(Destroyed())));

  if (Destroyed()) {
    return NSNormalWindowLevel;
  }

  // When an <input> element on a XUL <panel> is focused, the actual focused view
  // is the panel's parent view (mView). But the editor is displayed on the
  // popped-up widget's view (editorView).  We want the latter's window level.
  NSView<mozView>* editorView = mWidget->GetEditorView();
  NS_ENSURE_TRUE(editorView, NSNormalWindowLevel);
  NSInteger windowLevel = [[editorView window] level];

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TextInputHandlerBase::GetWindowLevel, windowLevel=%s (%X)",
     this, GetWindowLevelName(windowLevel), windowLevel));

  return windowLevel;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSNormalWindowLevel);
}

NS_IMETHODIMP
TextInputHandlerBase::AttachNativeKeyEvent(WidgetKeyboardEvent& aKeyEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Don't try to replace a native event if one already exists.
  // OS X doesn't have an OS modifier, can't make a native event.
  if (aKeyEvent.mNativeKeyEvent || aKeyEvent.modifiers & MODIFIER_OS) {
    return NS_OK;
  }

  MOZ_LOG(gLog, LogLevel::Info,
    ("%p TextInputHandlerBase::AttachNativeKeyEvent, key=0x%X, char=0x%X, "
     "mod=0x%X", this, aKeyEvent.keyCode, aKeyEvent.charCode,
     aKeyEvent.modifiers));

  NSEventType eventType;
  if (aKeyEvent.mMessage == NS_KEY_UP) {
    eventType = NSKeyUp;
  } else {
    eventType = NSKeyDown;
  }

  static const uint32_t sModifierFlagMap[][2] = {
    { MODIFIER_SHIFT,    NSShiftKeyMask },
    { MODIFIER_CONTROL,  NSControlKeyMask },
    { MODIFIER_ALT,      NSAlternateKeyMask },
    { MODIFIER_ALTGRAPH, NSAlternateKeyMask },
    { MODIFIER_META,     NSCommandKeyMask },
    { MODIFIER_CAPSLOCK, NSAlphaShiftKeyMask },
    { MODIFIER_NUMLOCK,  NSNumericPadKeyMask }
  };

  NSUInteger modifierFlags = 0;
  for (uint32_t i = 0; i < ArrayLength(sModifierFlagMap); ++i) {
    if (aKeyEvent.modifiers & sModifierFlagMap[i][0]) {
      modifierFlags |= sModifierFlagMap[i][1];
    }
  }

  NSInteger windowNumber = [[mView window] windowNumber];

  NSString* characters;
  if (aKeyEvent.charCode) {
    characters = [NSString stringWithCharacters:
      reinterpret_cast<const unichar*>(&(aKeyEvent.charCode)) length:1];
  } else {
    uint32_t cocoaCharCode =
      nsCocoaUtils::ConvertGeckoKeyCodeToMacCharCode(aKeyEvent.keyCode);
    characters = [NSString stringWithCharacters:
      reinterpret_cast<const unichar*>(&cocoaCharCode) length:1];
  }

  aKeyEvent.mNativeKeyEvent =
    [NSEvent     keyEventWithType:eventType
                         location:NSMakePoint(0,0)
                    modifierFlags:modifierFlags
                        timestamp:0
                     windowNumber:windowNumber
                          context:[NSGraphicsContext currentContext]
                       characters:characters
      charactersIgnoringModifiers:characters
                        isARepeat:NO
                          keyCode:0]; // Native key code not currently needed

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

bool
TextInputHandlerBase::SetSelection(NSRange& aRange)
{
  MOZ_ASSERT(!Destroyed());

  nsRefPtr<TextInputHandlerBase> kungFuDeathGrip(this);
  WidgetSelectionEvent selectionEvent(true, NS_SELECTION_SET, mWidget);
  selectionEvent.mOffset = aRange.location;
  selectionEvent.mLength = aRange.length;
  selectionEvent.mReversed = false;
  selectionEvent.mExpandToClusterBoundary = false;
  DispatchEvent(selectionEvent);
  NS_ENSURE_TRUE(selectionEvent.mSucceeded, false);
  return !Destroyed();
}

/* static */ bool
TextInputHandlerBase::IsPrintableChar(char16_t aChar)
{
  return (aChar >= 0x20 && aChar <= 0x7E) || aChar >= 0xA0;
}


/* static */ bool
TextInputHandlerBase::IsSpecialGeckoKey(UInt32 aNativeKeyCode)
{
  // this table is used to determine which keys are special and should not
  // generate a charCode
  switch (aNativeKeyCode) {
    // modifiers - we don't get separate events for these yet
    case kVK_Escape:
    case kVK_Shift:
    case kVK_RightShift:
    case kVK_Command:
    case kVK_RightCommand:
    case kVK_CapsLock:
    case kVK_Control:
    case kVK_RightControl:
    case kVK_Option:
    case kVK_RightOption:
    case kVK_ANSI_KeypadClear:
    case kVK_Function:

    // function keys
    case kVK_F1:
    case kVK_F2:
    case kVK_F3:
    case kVK_F4:
    case kVK_F5:
    case kVK_F6:
    case kVK_F7:
    case kVK_F8:
    case kVK_F9:
    case kVK_F10:
    case kVK_F11:
    case kVK_F12:
    case kVK_PC_Pause:
    case kVK_PC_ScrollLock:
    case kVK_PC_PrintScreen:
    case kVK_F16:
    case kVK_F17:
    case kVK_F18:
    case kVK_F19:

    case kVK_PC_Insert:
    case kVK_PC_Delete:
    case kVK_Tab:
    case kVK_PC_Backspace:
    case kVK_PC_ContextMenu:

    case kVK_JIS_Eisu:
    case kVK_JIS_Kana:

    case kVK_Home:
    case kVK_End:
    case kVK_PageUp:
    case kVK_PageDown:
    case kVK_LeftArrow:
    case kVK_RightArrow:
    case kVK_UpArrow:
    case kVK_DownArrow:
    case kVK_Return:
    case kVK_ANSI_KeypadEnter:
    case kVK_Powerbook_KeypadEnter:
      return true;
  }
  return false;
}

/* static */ bool
TextInputHandlerBase::IsNormalCharInputtingEvent(
                        const WidgetKeyboardEvent& aKeyEvent)
{
  // this is not character inputting event, simply.
  if (!aKeyEvent.isChar || !aKeyEvent.charCode || aKeyEvent.IsMeta()) {
    return false;
  }
  // if this is unicode char inputting event, we don't need to check
  // ctrl/alt/command keys
  if (aKeyEvent.charCode > 0x7F) {
    return true;
  }
  // ASCII chars should be inputted without ctrl/alt/command keys
  return !aKeyEvent.IsControl() && !aKeyEvent.IsAlt();
}

/* static */ bool
TextInputHandlerBase::IsModifierKey(UInt32 aNativeKeyCode)
{
  switch (aNativeKeyCode) {
    case kVK_CapsLock:
    case kVK_RightCommand:
    case kVK_Command:
    case kVK_Shift:
    case kVK_Option:
    case kVK_Control:
    case kVK_RightShift:
    case kVK_RightOption:
    case kVK_RightControl:
    case kVK_Function:
      return true;
  }
  return false;
}

/* static */ void
TextInputHandlerBase::EnableSecureEventInput()
{
  sSecureEventInputCount++;
  ::EnableSecureEventInput();
}

/* static */ void
TextInputHandlerBase::DisableSecureEventInput()
{
  if (!sSecureEventInputCount) {
    return;
  }
  sSecureEventInputCount--;
  ::DisableSecureEventInput();
}

/* static */ bool
TextInputHandlerBase::IsSecureEventInputEnabled()
{
  NS_ASSERTION(!!sSecureEventInputCount == !!::IsSecureEventInputEnabled(),
               "Some other process has enabled secure event input");
  return !!sSecureEventInputCount;
}

/* static */ void
TextInputHandlerBase::EnsureSecureEventInputDisabled()
{
  while (sSecureEventInputCount) {
    TextInputHandlerBase::DisableSecureEventInput();
  }
}
