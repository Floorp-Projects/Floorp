/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "TextInputHandler.h"

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif // MOZ_LOGGING
#include "prlog.h"

#include "nsChildView.h"
#include "nsObjCExceptions.h"
#include "nsBidiUtils.h"
#include "nsToolkit.h"
#include "nsCocoaUtils.h"
#include "WidgetUtils.h"
#include "nsPrintfCString.h"

#ifdef __LP64__
#include "ComplexTextInputPanel.h"
#endif // __LP64__

#ifndef NP_NO_CARBON
#include <objc/runtime.h>
#endif // NP_NO_CARBON

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif
#include "prlog.h"

using namespace mozilla;
using namespace mozilla::widget;

#ifdef PR_LOGGING

PRLogModuleInfo* gLog = nsnull;

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
    case kEscapeKeyCode:          return "Escape";
    case kRCommandKeyCode:        return "Right-Command";
    case kCommandKeyCode:         return "Command";
    case kShiftKeyCode:           return "Shift";
    case kCapsLockKeyCode:        return "CapsLock";
    case kOptionkeyCode:          return "Option";
    case kControlKeyCode:         return "Control";
    case kRShiftKeyCode:          return "Right-Shift";
    case kROptionKeyCode:         return "Right-Option";
    case kRControlKeyCode:        return "Right-Control";
    case kClearKeyCode:           return "Clear";

    case kF1KeyCode:              return "F1";
    case kF2KeyCode:              return "F2";
    case kF3KeyCode:              return "F3";
    case kF4KeyCode:              return "F4";
    case kF5KeyCode:              return "F5";
    case kF6KeyCode:              return "F6";
    case kF7KeyCode:              return "F7";
    case kF8KeyCode:              return "F8";
    case kF9KeyCode:              return "F9";
    case kF10KeyCode:             return "F10";
    case kF11KeyCode:             return "F11";
    case kF12KeyCode:             return "F12";
    case kF13KeyCode:             return "F13/PrintScreen";
    case kF14KeyCode:             return "F14/ScrollLock";
    case kF15KeyCode:             return "F15/Pause";

    case kKeypad0KeyCode:         return "NumPad-0";
    case kKeypad1KeyCode:         return "NumPad-1";
    case kKeypad2KeyCode:         return "NumPad-2";
    case kKeypad3KeyCode:         return "NumPad-3";
    case kKeypad4KeyCode:         return "NumPad-4";
    case kKeypad5KeyCode:         return "NumPad-5";
    case kKeypad6KeyCode:         return "NumPad-6";
    case kKeypad7KeyCode:         return "NumPad-7";
    case kKeypad8KeyCode:         return "NumPad-8";
    case kKeypad9KeyCode:         return "NumPad-9";

    case kKeypadMultiplyKeyCode:  return "NumPad-*";
    case kKeypadAddKeyCode:       return "NumPad-+";
    case kKeypadSubtractKeyCode:  return "NumPad--";
    case kKeypadDecimalKeyCode:   return "NumPad-.";
    case kKeypadDivideKeyCode:    return "NumPad-/";
    case kKeypadEqualsKeyCode:    return "NumPad-=";
    case kEnterKeyCode:           return "NumPad-Enter";
    case kReturnKeyCode:          return "NumPad-Return";
    case kPowerbookEnterKeyCode:  return "NumPad-EnterOnPowerBook";

    case kInsertKeyCode:          return "Insert/Help";
    case kDeleteKeyCode:          return "Delete";
    case kTabKeyCode:             return "Tab";
    case kTildeKeyCode:           return "Tilde";
    case kBackspaceKeyCode:       return "Backspace";
    case kHomeKeyCode:            return "Home";
    case kEndKeyCode:             return "End";
    case kPageUpKeyCode:          return "PageUp";
    case kPageDownKeyCode:        return "PageDown";
    case kLeftArrowKeyCode:       return "LeftArrow";
    case kRightArrowKeyCode:      return "RightArrow";
    case kUpArrowKeyCode:         return "UpArrow";
    case kDownArrowKeyCode:       return "DownArrow";

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
  for (PRUint32 i = 0; i < str.Length(); i++) {
    PRUnichar ch = str[i];
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
GetGeckoKeyEventType(const nsEvent &aEvent)
{
  switch (aEvent.message) {
    case NS_KEY_DOWN:    return "NS_KEY_DOWN";
    case NS_KEY_UP:      return "NS_KEY_UP";
    case NS_KEY_PRESS:   return "NS_KEY_PRESS";
    default:             return "not key event";
  }
}

static const char*
GetRangeTypeName(PRUint32 aRangeType)
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

#endif // #ifdef PR_LOGGING

static PRUint32 gHandlerInstanceCount = 0;
static TISInputSourceWrapper gCurrentKeyboardLayout;

static void
InitLogModule()
{
#ifdef PR_LOGGING
  // Clear() is always called when TISInputSourceWrappper is created.
  if (!gLog) {
    gLog = PR_NewLogModule("TextInputHandlerWidgets");
    TextInputHandler::DebugPrintAllKeyboardLayouts();
    IMEInputHandler::DebugPrintAllIMEModes();
  }
#endif
}

static void
InitCurrentKeyboardLayout()
{
  if (gHandlerInstanceCount > 0 &&
      !gCurrentKeyboardLayout.IsInitializedByCurrentKeyboardLayout()) {
    gCurrentKeyboardLayout.InitByCurrentKeyboardLayout();
  }
}

static void
FinalizeCurrentKeyboardLayout()
{
  gCurrentKeyboardLayout.Clear();
}


#pragma mark -


/******************************************************************************
 *
 *  TISInputSourceWrapper implementation
 *
 ******************************************************************************/

// static
TISInputSourceWrapper&
TISInputSourceWrapper::CurrentKeyboardLayout()
{
  InitCurrentKeyboardLayout();
  return gCurrentKeyboardLayout;
}

bool
TISInputSourceWrapper::TranslateToString(UInt32 aKeyCode, UInt32 aModifiers,
                                         UInt32 aKbType, nsAString &aStr)
{
  aStr.Truncate();

  const UCKeyboardLayout* UCKey = GetUCKeyboardLayout();

  PR_LOG(gLog, PR_LOG_ALWAYS,
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

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TISInputSourceWrapper::TranslateToString, err=0x%X, len=%llu",
     this, err, len));

  NS_ENSURE_TRUE(err == noErr, false);
  if (len == 0) {
    return true;
  }
  NS_ENSURE_TRUE(EnsureStringLength(aStr, len), false);
  NS_ASSERTION(sizeof(PRUnichar) == sizeof(UniChar),
               "size of PRUnichar and size of UniChar are different");
  memcpy(aStr.BeginWriting(), chars, len * sizeof(PRUnichar));

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TISInputSourceWrapper::TranslateToString, aStr=\"%s\"",
     this, NS_ConvertUTF16toUTF8(aStr).get()));

  return true;
}

PRUint32
TISInputSourceWrapper::TranslateToChar(UInt32 aKeyCode, UInt32 aModifiers,
                                       UInt32 aKbType)
{
  nsAutoString str;
  if (!TranslateToString(aKeyCode, aModifiers, aKbType, str) ||
      str.Length() != 1) {
    return 0;
  }
  return static_cast<PRUint32>(str.CharAt(0));
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
                                                     aID.get(), aID.Length());
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
}

void
TISInputSourceWrapper::InitByCurrentKeyboardLayout()
{
  Clear();
  mInputSource = ::TISCopyCurrentKeyboardLayoutInputSource();
}

void
TISInputSourceWrapper::InitByCurrentASCIICapableInputSource()
{
  Clear();
  mInputSource = ::TISCopyCurrentASCIICapableKeyboardInputSource();
}

void
TISInputSourceWrapper::InitByCurrentASCIICapableKeyboardLayout()
{
  Clear();
  mInputSource = ::TISCopyCurrentASCIICapableKeyboardLayoutInputSource();
}

void
TISInputSourceWrapper::InitByTISInputSourceRef(TISInputSourceRef aInputSource)
{
  Clear();
  mInputSource = aInputSource;
}

void
TISInputSourceWrapper::InitByLanguage(CFStringRef aLanguage)
{
  Clear();
  mInputSource = ::TISCopyInputSourceForLanguage(aLanguage);
}

const UCKeyboardLayout*
TISInputSourceWrapper::GetUCKeyboardLayout()
{
  NS_ENSURE_TRUE(mInputSource, nsnull);
  if (mUCKeyboardLayout) {
    return mUCKeyboardLayout;
  }
  CFDataRef uchr = static_cast<CFDataRef>(
    ::TISGetInputSourceProperty(mInputSource,
                                kTISPropertyUnicodeKeyLayoutData));

  // We should be always able to get the layout here.
  NS_ENSURE_TRUE(uchr, nsnull);
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
  return aStr != nsnull;
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
TISInputSourceWrapper::GetLanguageList(CFArrayRef &aLanguageList)
{
  NS_ENSURE_TRUE(mInputSource, false);
  aLanguageList = static_cast<CFArrayRef>(
    ::TISGetInputSourceProperty(mInputSource,
                                kTISPropertyInputSourceLanguages));
  return aLanguageList != nsnull;
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
  return aPrimaryLanguage != nsnull;
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
    PRUnichar ch = str.IsEmpty() ? PRUnichar(0) : str.CharAt(0);
    mIsRTL = UCS2_CHAR_IS_BIDI(ch) || ch == 0xD802 || ch == 0xD803;
  }
  return mIsRTL != 0;
}

bool
TISInputSourceWrapper::IsInitializedByCurrentKeyboardLayout()
{
  return mInputSource == ::TISCopyCurrentKeyboardLayoutInputSource();
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
  mInputSourceList = nsnull;
  mInputSource = nsnull;
  mIsRTL = -1;
  mUCKeyboardLayout = nsnull;
  mOverrideKeyboard = false;
}

void
TISInputSourceWrapper::InitKeyEvent(NSEvent *aNativeKeyEvent,
                                    nsKeyEvent& aKeyEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TISInputSourceWrapper::InitKeyEvent, aNativeKeyEvent=%p, "
     "aKeyEvent.message=%s",
     this, aNativeKeyEvent, GetGeckoKeyEventType(aKeyEvent)));

  NS_ENSURE_TRUE(aNativeKeyEvent, );

  aKeyEvent.time = PR_IntervalNow();

  nsCocoaUtils::InitInputEvent(aKeyEvent, aNativeKeyEvent);

  aKeyEvent.refPoint = nsIntPoint(0, 0);
  aKeyEvent.isChar = false; // XXX not used in XP level

  // If a keyboard layout override is set, we also need to force the keyboard
  // type to something ANSI to avoid test failures on machines with JIS
  // keyboards (since the pair of keyboard layout and physical keyboard type
  // form the actual key layout).  This assumes that the test setting the
  // override was written assuming an ANSI keyboard.
  UInt32 kbType = mOverrideKeyboard ? eKbdType_ANSI : ::LMGetKbdType();

  aKeyEvent.keyCode =
    ComputeGeckoKeyCode([aNativeKeyEvent keyCode], kbType, aKeyEvent.IsMeta());

  switch ([aNativeKeyEvent keyCode]) {
    case kCommandKeyCode:
    case kShiftKeyCode:
    case kOptionkeyCode:
    case kControlKeyCode:
      aKeyEvent.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_LEFT;
      break;

    case kRCommandKeyCode:
    case kRShiftKeyCode:
    case kROptionKeyCode:
    case kRControlKeyCode:
      aKeyEvent.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_RIGHT;
      break;

    case kKeypad0KeyCode:
    case kKeypad1KeyCode:
    case kKeypad2KeyCode:
    case kKeypad3KeyCode:
    case kKeypad4KeyCode:
    case kKeypad5KeyCode:
    case kKeypad6KeyCode:
    case kKeypad7KeyCode:
    case kKeypad8KeyCode:
    case kKeypad9KeyCode:
    case kKeypadMultiplyKeyCode:
    case kKeypadAddKeyCode:
    case kKeypadSubtractKeyCode:
    case kKeypadDecimalKeyCode:
    case kKeypadDivideKeyCode:
    case kKeypadEqualsKeyCode:
    case kEnterKeyCode:
    case kPowerbookEnterKeyCode:
      aKeyEvent.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_NUMPAD;
      break;

    default:
      aKeyEvent.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD;
      break;
  }

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TISInputSourceWrapper::InitKeyEvent, "
     "shift=%s, ctrl=%s, alt=%s, meta=%s",
     this, OnOrOff(aKeyEvent.IsShift()), OnOrOff(aKeyEvent.IsControl()),
     OnOrOff(aKeyEvent.IsAlt()), OnOrOff(aKeyEvent.IsMeta())));

  if (aKeyEvent.message == NS_KEY_PRESS &&
      !TextInputHandler::IsSpecialGeckoKey([aNativeKeyEvent keyCode])) {
    InitKeyPressEvent(aNativeKeyEvent, aKeyEvent, kbType);
    return;
  }

  aKeyEvent.charCode = 0;

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TISInputSourceWrapper::InitKeyEvent, keyCode=0x%X charCode=0x0",
     this, aKeyEvent.keyCode));

  NS_OBJC_END_TRY_ABORT_BLOCK
}

void
TISInputSourceWrapper::InitKeyPressEvent(NSEvent *aNativeKeyEvent,
                                         nsKeyEvent& aKeyEvent,
                                         UInt32 aKbType)
{
  NS_ASSERTION(aKeyEvent.message == NS_KEY_PRESS,
               "aKeyEvent must be NS_KEY_PRESS event");

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TISInputSourceWrapper::InitKeyPressEvent, aNativeKeyEvent=%p"
     "aKeyEvent.message=%s, aKbType=0x%X, IsOpenedIMEMode()=%s",
     this, aNativeKeyEvent, GetGeckoKeyEventType(aKeyEvent), aKbType,
     TrueOrFalse(IsOpenedIMEMode())));

  aKeyEvent.isChar = true; // this is not a special key  XXX not used in XP

  aKeyEvent.charCode = 0;
  NSString* chars = [aNativeKeyEvent characters];
  if ([chars length] > 0) {
    // XXX This is wrong at Hiragana or Katakana with Kana-Nyuryoku mode or
    //     Chinese or Koran IME modes.  We should use ASCII characters for the
    //     charCode.
    aKeyEvent.charCode = [chars characterAtIndex:0];
  }

  // convert control-modified charCode to raw charCode (with appropriate case)
  if (aKeyEvent.IsControl() && aKeyEvent.charCode <= 26) {
    aKeyEvent.charCode += (aKeyEvent.IsShift()) ? ('A' - 1) : ('a' - 1);
  }

  if (aKeyEvent.charCode != 0) {
    aKeyEvent.keyCode = 0;
  }

  PR_LOG(gLog, PR_LOG_ALWAYS,
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

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TISInputSourceWrapper::InitKeyPressEvent, "
     "isRomanKeyboardLayout=%s, key=0x%X",
     this, TrueOrFalse(isRomanKeyboardLayout), aKbType, key));

  nsString str;

  // normal chars
  PRUint32 unshiftedChar = TranslateToChar(key, lockState, aKbType);
  UInt32 shiftLockMod = shiftKey | lockState;
  PRUint32 shiftedChar = TranslateToChar(key, shiftLockMod, aKbType);

  // characters generated with Cmd key
  // XXX we should remove CapsLock state, which changes characters from
  //     Latin to Cyrillic with Russian layout on 10.4 only when Cmd key
  //     is pressed.
  UInt32 numState = (lockState & ~alphaLock); // only num lock state
  PRUint32 uncmdedChar = TranslateToChar(key, numState, aKbType);
  UInt32 shiftNumMod = numState | shiftKey;
  PRUint32 uncmdedShiftChar = TranslateToChar(key, shiftNumMod, aKbType);
  PRUint32 uncmdedUSChar = USLayout.TranslateToChar(key, numState, aKbType);
  UInt32 cmdNumMod = cmdKey | numState;
  PRUint32 cmdedChar = TranslateToChar(key, cmdNumMod, aKbType);
  UInt32 cmdShiftNumMod = shiftKey | cmdNumMod;
  PRUint32 cmdedShiftChar = TranslateToChar(key, cmdShiftNumMod, aKbType);

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
    nsAlternativeCharCode altCharCodes(unshiftedChar, shiftedChar);
    aKeyEvent.alternativeCharCodes.AppendElement(altCharCodes);
  }
  PR_LOG(gLog, PR_LOG_ALWAYS,
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
  PRUint32 originalCmdedShiftChar = cmdedShiftChar;

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
    PRUint32 ch = USLayout.TranslateToChar(key, lockState, aKbType);
    if (ch) {
      cmdedChar = ch;
    }
    ch = USLayout.TranslateToChar(key, shiftLockMod, aKbType);
    if (ch) {
      cmdedShiftChar = ch;
    }
  }

  // Only charCode (not alternativeCharCodes) is available to javascript,
  // so attempt to set this to the most likely intended (or most useful)
  // character.  Note that cmdedChar and cmdedShiftChar are usually
  // Latin/ASCII characters and that is what is wanted here as accel
  // keys are expected to be Latin characters.
  //
  // XXX We should do something similar when Control is down (bug 429510).
  if (aKeyEvent.IsMeta() && !(aKeyEvent.IsControl() || aKeyEvent.IsAlt())) {
    // The character to use for charCode.
    PRUint32 preferredCharCode = 0;
    preferredCharCode = aKeyEvent.IsShift() ? cmdedShiftChar : cmdedChar;

    if (preferredCharCode) {
      aKeyEvent.charCode = preferredCharCode;
      PR_LOG(gLog, PR_LOG_ALWAYS,
        ("%p TISInputSourceWrapper::InitKeyPressEvent, "
         "aKeyEvent.charCode=U+%X",
         this, aKeyEvent.charCode));
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
    nsAlternativeCharCode altCharCodes(cmdedChar, cmdedShiftChar);
    aKeyEvent.alternativeCharCodes.AppendElement(altCharCodes);
  }
  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TISInputSourceWrapper::InitKeyPressEvent, "
     "hasCmdShiftOnlyChar=%s, isCmdSwitchLayout=%s, isDvorakQWERTY=%s, "
     "cmdedChar=U+%X, cmdedShiftChar=U+%X",
     this, TrueOrFalse(hasCmdShiftOnlyChar), TrueOrFalse(isDvorakQWERTY),
     TrueOrFalse(isDvorakQWERTY), cmdedChar, cmdedShiftChar));
  // Special case for 'SS' key of German layout. See the comment of
  // hasCmdShiftOnlyChar definition for the detail.
  if (hasCmdShiftOnlyChar && originalCmdedShiftChar) {
    nsAlternativeCharCode altCharCodes(0, originalCmdedShiftChar);
    aKeyEvent.alternativeCharCodes.AppendElement(altCharCodes);
  }
  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TISInputSourceWrapper::InitKeyPressEvent, "
     "hasCmdShiftOnlyChar=%s, originalCmdedShiftChar=U+%X",
     this, TrueOrFalse(hasCmdShiftOnlyChar), originalCmdedShiftChar));
}

PRUint32
TISInputSourceWrapper::ComputeGeckoKeyCode(UInt32 aNativeKeyCode,
                                           UInt32 aKbType,
                                           bool aCmdIsPressed)
{
  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TISInputSourceWrapper::ComputeGeckoKeyCode, aNativeKeyCode=0x%X, "
     "aKbType=0x%X, aCmdIsPressed=%s, IsOpenedIMEMode()=%s, "
     "IsASCIICapable()=%s",
     this, aNativeKeyCode, aKbType, TrueOrFalse(aCmdIsPressed),
     TrueOrFalse(IsOpenedIMEMode()), TrueOrFalse(IsASCIICapable())));

  switch (aNativeKeyCode) {
    case kSpaceKeyCode:         return NS_VK_SPACE;
    case kEscapeKeyCode:        return NS_VK_ESCAPE;

    // modifiers
    case kRCommandKeyCode:
    case kCommandKeyCode:       return NS_VK_META;
    case kRShiftKeyCode:
    case kShiftKeyCode:         return NS_VK_SHIFT;
    case kCapsLockKeyCode:      return NS_VK_CAPS_LOCK;
    case kRControlKeyCode:
    case kControlKeyCode:       return NS_VK_CONTROL;
    case kROptionKeyCode:
    case kOptionkeyCode:        return NS_VK_ALT;

    case kClearKeyCode:         return NS_VK_CLEAR;

    // function keys
    case kF1KeyCode:            return NS_VK_F1;
    case kF2KeyCode:            return NS_VK_F2;
    case kF3KeyCode:            return NS_VK_F3;
    case kF4KeyCode:            return NS_VK_F4;
    case kF5KeyCode:            return NS_VK_F5;
    case kF6KeyCode:            return NS_VK_F6;
    case kF7KeyCode:            return NS_VK_F7;
    case kF8KeyCode:            return NS_VK_F8;
    case kF9KeyCode:            return NS_VK_F9;
    case kF10KeyCode:           return NS_VK_F10;
    case kF11KeyCode:           return NS_VK_F11;
    case kF12KeyCode:           return NS_VK_F12;
    // case kF13KeyCode:           return NS_VK_F13;  // clash with the 3 below
    // case kF14KeyCode:           return NS_VK_F14;
    // case kF15KeyCode:           return NS_VK_F15;
    case kF16KeyCode:           return NS_VK_F16;
    case kF17KeyCode:           return NS_VK_F17;
    case kF18KeyCode:           return NS_VK_F18;
    case kF19KeyCode:           return NS_VK_F19;

    case kPauseKeyCode:         return NS_VK_PAUSE;
    case kScrollLockKeyCode:    return NS_VK_SCROLL_LOCK;
    case kPrintScreenKeyCode:   return NS_VK_PRINTSCREEN;

    // keypad
    case kKeypad0KeyCode:       return NS_VK_NUMPAD0;
    case kKeypad1KeyCode:       return NS_VK_NUMPAD1;
    case kKeypad2KeyCode:       return NS_VK_NUMPAD2;
    case kKeypad3KeyCode:       return NS_VK_NUMPAD3;
    case kKeypad4KeyCode:       return NS_VK_NUMPAD4;
    case kKeypad5KeyCode:       return NS_VK_NUMPAD5;
    case kKeypad6KeyCode:       return NS_VK_NUMPAD6;
    case kKeypad7KeyCode:       return NS_VK_NUMPAD7;
    case kKeypad8KeyCode:       return NS_VK_NUMPAD8;
    case kKeypad9KeyCode:       return NS_VK_NUMPAD9;

    case kKeypadMultiplyKeyCode:  return NS_VK_MULTIPLY;
    case kKeypadAddKeyCode:       return NS_VK_ADD;
    case kKeypadSubtractKeyCode:  return NS_VK_SUBTRACT;
    case kKeypadDecimalKeyCode:   return NS_VK_DECIMAL;
    case kKeypadDivideKeyCode:    return NS_VK_DIVIDE;

    // IME keys
    case kJapanese_Eisu:        return NS_VK_EISU;
    case kJapanese_Kana:        return NS_VK_KANA;

    // these may clash with forward delete and help
    case kInsertKeyCode:        return NS_VK_INSERT;
    case kDeleteKeyCode:        return NS_VK_DELETE;

    case kBackspaceKeyCode:     return NS_VK_BACK;
    case kTabKeyCode:           return NS_VK_TAB;

    case kHomeKeyCode:          return NS_VK_HOME;
    case kEndKeyCode:           return NS_VK_END;

    case kPageUpKeyCode:        return NS_VK_PAGE_UP;
    case kPageDownKeyCode:      return NS_VK_PAGE_DOWN;

    case kLeftArrowKeyCode:     return NS_VK_LEFT;
    case kRightArrowKeyCode:    return NS_VK_RIGHT;
    case kUpArrowKeyCode:       return NS_VK_UP;
    case kDownArrowKeyCode:     return NS_VK_DOWN;

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

    case kEnterKeyCode:
    case kReturnKeyCode:
    case kPowerbookEnterKeyCode: return NS_VK_RETURN;
  }

  // If Cmd key is pressed, that causes switching keyboard layout temporarily.
  // E.g., Dvorak-QWERTY.  Therefore, if Cmd key is pressed, we should honor it.
  UInt32 modifiers = aCmdIsPressed ? cmdKey : 0;

  PRUint32 charCode = TranslateToChar(aNativeKeyCode, modifiers, aKbType);

  // Special case for Mac.  Mac inputs Yen sign (U+00A5) directly instead of
  // Back slash (U+005C).  We should return NS_VK_BACK_SLASH for compatibility
  // with other platforms.
  // XXX How about Won sign (U+20A9) which has same problem as Yen sign?
  if (charCode == 0x00A5) {
    return NS_VK_BACK_SLASH;
  }

  PRUint32 keyCode = WidgetUtils::ComputeKeyCodeFromChar(charCode);
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
#ifdef PR_LOGGING
  if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
    CFArrayRef list = CreateAllKeyboardLayoutList();
    PR_LOG(gLog, PR_LOG_ALWAYS, ("Keyboard layout configuration:"));
    CFIndex idx = ::CFArrayGetCount(list);
    TISInputSourceWrapper tis;
    for (CFIndex i = 0; i < idx; ++i) {
      TISInputSourceRef inputSource = static_cast<TISInputSourceRef>(
        const_cast<void *>(::CFArrayGetValueAtIndex(list, i)));
      tis.InitByTISInputSourceRef(inputSource);
      nsAutoString name, isid;
      tis.GetLocalizedName(name);
      tis.GetInputSourceID(isid);
      PR_LOG(gLog, PR_LOG_ALWAYS,
             ("  %s\t<%s>%s%s\n",
              NS_ConvertUTF16toUTF8(name).get(),
              NS_ConvertUTF16toUTF8(isid).get(),
              tis.IsASCIICapable() ? "" : "\t(Isn't ASCII capable)",
              tis.GetUCKeyboardLayout() ? "" : "\t(uchr is NOT AVAILABLE)"));
    }
    ::CFRelease(list);
  }
#endif // #ifdef PR_LOGGING
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
    PR_LOG(gLog, PR_LOG_ALWAYS,
      ("%p TextInputHandler::HandleKeyDownEvent, "
       "widget has been already destroyed", this));
    return false;
  }

  PR_LOG(gLog, PR_LOG_ALWAYS,
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

  BOOL nonDeadKeyPress = [[aNativeEvent characters] length] > 0;
  if (nonDeadKeyPress && !IsIMEComposing()) {
    NSResponder* firstResponder = [[mView window] firstResponder];

    nsKeyEvent keydownEvent(true, NS_KEY_DOWN, mWidget);
    InitKeyEvent(aNativeEvent, keydownEvent);

#ifndef NP_NO_CARBON
    EventRecord carbonEvent;
    if ([mView pluginEventModel] == NPEventModelCarbon) {
      ConvertCocoaKeyEventToCarbonEvent(aNativeEvent, carbonEvent, true);
      keydownEvent.pluginEvent = &carbonEvent;
    }
#endif // #ifndef NP_NO_CARBON

    currentKeyEvent->mKeyDownHandled = DispatchEvent(keydownEvent);
    if (Destroyed()) {
      PR_LOG(gLog, PR_LOG_ALWAYS,
        ("%p TextInputHandler::HandleKeyDownEvent, "
         "widget was destroyed by keydown event", this));
      return currentKeyEvent->KeyDownOrPressHandled();
    }

    // The key down event may have shifted the focus, in which
    // case we should not fire the key press.
    // XXX This is a special code only on Cocoa widget, why is this needed?
    if (firstResponder != [[mView window] firstResponder]) {
      PR_LOG(gLog, PR_LOG_ALWAYS,
        ("%p TextInputHandler::HandleKeyDownEvent, "
         "view lost focus by keydown event", this));
      return currentKeyEvent->KeyDownOrPressHandled();
    }

    // If this is the context menu key command, send a context menu key event.
    NSUInteger modifierFlags =
      [aNativeEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask;
    if (modifierFlags == NSControlKeyMask &&
        [[aNativeEvent charactersIgnoringModifiers] isEqualToString:@" "]) {
      nsMouseEvent contextMenuEvent(true, NS_CONTEXTMENU,
                                    [mView widget], nsMouseEvent::eReal,
                                    nsMouseEvent::eContextMenuKey);
      contextMenuEvent.modifiers = 0;

      bool cmEventHandled = DispatchEvent(contextMenuEvent);
      PR_LOG(gLog, PR_LOG_ALWAYS,
        ("%p TextInputHandler::HandleKeyDownEvent, "
         "context menu event dispatched, handled=%s%s",
         this, TrueOrFalse(cmEventHandled),
         Destroyed() ? " and widget was destroyed" : ""));
      [mView maybeInitContextMenuTracking];
      // Bail, there is nothing else to do here.
      return (cmEventHandled || currentKeyEvent->KeyDownOrPressHandled());
    }
  }

  // Let Cocoa interpret the key events, caching IsIMEComposing first.
  bool wasComposing = IsIMEComposing();
  bool interpretKeyEventsCalled = false;
  if (IsIMEEnabled() || IsASCIICapableOnly()) {
    PR_LOG(gLog, PR_LOG_ALWAYS,
      ("%p TextInputHandler::HandleKeyDownEvent, calling interpretKeyEvents",
       this));
    [mView interpretKeyEvents:[NSArray arrayWithObject:aNativeEvent]];
    interpretKeyEventsCalled = true;
    PR_LOG(gLog, PR_LOG_ALWAYS,
      ("%p TextInputHandler::HandleKeyDownEvent, called interpretKeyEvents",
       this));
  }

  if (Destroyed()) {
    PR_LOG(gLog, PR_LOG_ALWAYS,
      ("%p TextInputHandler::HandleKeyDownEvent, widget was destroyed",
       this));
    return currentKeyEvent->KeyDownOrPressHandled();
  }

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TextInputHandler::HandleKeyDownEvent, wasComposing=%s, "
     "IsIMEComposing()=%s",
     this, TrueOrFalse(wasComposing), TrueOrFalse(IsIMEComposing())));

  if (!currentKeyEvent->mKeyPressDispatched && nonDeadKeyPress &&
      !wasComposing && !IsIMEComposing()) {
    nsKeyEvent keypressEvent(true, NS_KEY_PRESS, mWidget);
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
      if (currentKeyEvent->mKeyDownHandled ||
          currentKeyEvent->mCausedOtherKeyEvents) {
        keypressEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;
      }
      currentKeyEvent->mKeyPressHandled = DispatchEvent(keypressEvent);
      PR_LOG(gLog, PR_LOG_ALWAYS,
        ("%p TextInputHandler::HandleKeyDownEvent, keypress event dispatched",
         this));
    }
  }

  // Note: mWidget might have become null here. Don't count on it from here on.

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TextInputHandler::HandleKeyDownEvent, "
     "keydown handled=%s, keypress handled=%s",
     this, TrueOrFalse(currentKeyEvent->mKeyDownHandled),
     TrueOrFalse(currentKeyEvent->mKeyPressHandled)));
  return currentKeyEvent->KeyDownOrPressHandled();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

void
TextInputHandler::HandleKeyUpEvent(NSEvent* aNativeEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TextInputHandler::HandleKeyUpEvent, aNativeEvent=%p, "
     "type=%s, keyCode=%lld (0x%X), modifierFlags=0x%X, characters=\"%s\", "
     "charactersIgnoringModifiers=\"%s\", "
     "mIgnoreNextKeyUpEvent=%s, IsIMEComposing()=%s",
     this, aNativeEvent, GetNativeKeyEventType(aNativeEvent),
     [aNativeEvent keyCode], [aNativeEvent keyCode],
     [aNativeEvent modifierFlags], GetCharacters([aNativeEvent characters]),
     GetCharacters([aNativeEvent charactersIgnoringModifiers]),
     TrueOrFalse(mIgnoreNextKeyUpEvent), TrueOrFalse(IsIMEComposing())));

  if (mIgnoreNextKeyUpEvent) {
    mIgnoreNextKeyUpEvent = false;
    return;
  }

  if (Destroyed()) {
    PR_LOG(gLog, PR_LOG_ALWAYS,
      ("%p TextInputHandler::HandleKeyUpEvent, "
       "widget has been already destroyed", this));
    return;
  }

  // if we don't have any characters we can't generate a keyUp event
  if ([[aNativeEvent characters] length] == 0 || IsIMEComposing()) {
    return;
  }

  nsKeyEvent keyupEvent(true, NS_KEY_UP, mWidget);
  InitKeyEvent(aNativeEvent, keyupEvent);

  DispatchEvent(keyupEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
TextInputHandler::HandleFlagsChanged(NSEvent* aNativeEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (Destroyed()) {
    PR_LOG(gLog, PR_LOG_ALWAYS,
      ("%p TextInputHandler::HandleFlagsChanged, "
       "widget has been already destroyed", this));
    return;
  }

  nsRefPtr<nsChildView> kungFuDeathGrip(mWidget);

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TextInputHandler::HandleFlagsChanged, aNativeEvent=%p, "
     "type=%s, keyCode=%s (0x%X), sLastModifierState=0x%X, IsIMEComposing()=%s",
     this, aNativeEvent, GetNativeKeyEventType(aNativeEvent),
     GetKeyNameForNativeKeyCode([aNativeEvent keyCode]), [aNativeEvent keyCode],
     sLastModifierState, TrueOrFalse(IsIMEComposing())));

  // CapsLock state and other modifier states are different:
  // CapsLock state does not revert when the CapsLock key goes up, as the
  // modifier state does for other modifier keys on key up.
  if ([aNativeEvent keyCode] == kCapsLockKeyCode) {
    // Fire key down event for caps lock.
    DispatchKeyEventForFlagsChanged(aNativeEvent, true);
    if (Destroyed()) {
      return;
    }
    // XXX should we fire keyup event too? The keyup event for CapsLock key
    // is never dispatched on Gecko.
  } else if ([aNativeEvent type] == NSFlagsChanged) {
    // Fire key up/down events for the modifier keys (shift, alt, ctrl, command)
    NSUInteger modifiers =
      [aNativeEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask;
    const NSUInteger kModifierMaskTable[] =
      { NSShiftKeyMask, NSControlKeyMask,
        NSAlternateKeyMask, NSCommandKeyMask };
    const PRUint32 kModifierCount = ArrayLength(kModifierMaskTable);

    for (PRUint32 i = 0; i < kModifierCount; i++) {
      NSUInteger modifierBit = kModifierMaskTable[i];
      if ((modifiers & modifierBit) != (sLastModifierState & modifierBit)) {
        bool isKeyDown = ((modifiers & modifierBit) != 0);
        DispatchKeyEventForFlagsChanged(aNativeEvent, isKeyDown);
        if (Destroyed()) {
          return;
        }

        // Stop if focus has changed.
        // Check to see if mView is still the first responder.
        if (![mView isFirstResponder]) {
          break;
        }
      }
    }

    sLastModifierState = modifiers;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
TextInputHandler::DispatchKeyEventForFlagsChanged(NSEvent* aNativeEvent,
                                                  bool aDispatchKeyDown)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (Destroyed()) {
    return;
  }

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TextInputHandler::DispatchKeyEventForFlagsChanged, aNativeEvent=%p, "
     "type=%s, keyCode=%s (0x%X), aDispatchKeyDown=%s, IsIMEComposing()=%s",
     this, aNativeEvent, GetNativeKeyEventType(aNativeEvent),
     GetKeyNameForNativeKeyCode([aNativeEvent keyCode]), [aNativeEvent keyCode],
     TrueOrFalse(aDispatchKeyDown), TrueOrFalse(IsIMEComposing())));

  if ([aNativeEvent type] != NSFlagsChanged || IsIMEComposing()) {
    return;
  }

  PRUint32 message = aDispatchKeyDown ? NS_KEY_DOWN : NS_KEY_UP;

#ifndef NP_NO_CARBON
  EventRecord carbonEvent;
#endif // ifndef NP_NO_CARBON
  NPCocoaEvent cocoaEvent;

  // Fire a key event.
  nsKeyEvent keyEvent(true, message, mWidget);
  InitKeyEvent(aNativeEvent, keyEvent);

  // create event for use by plugins
  if ([mView isPluginView]) {
#ifndef NP_NO_CARBON
    if ([mView pluginEventModel] == NPEventModelCarbon) {
      ConvertCocoaKeyEventToCarbonEvent(aNativeEvent, carbonEvent,
                                        aDispatchKeyDown);
      keyEvent.pluginEvent = &carbonEvent;
    }
#endif // ifndef NP_NO_CARBON
    if ([mView pluginEventModel] == NPEventModelCocoa) {
      ConvertCocoaKeyEventToNPCocoaEvent(aNativeEvent, cocoaEvent);
      keyEvent.pluginEvent = &cocoaEvent;
    }
  }

  DispatchEvent(keyEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
TextInputHandler::InsertText(NSAttributedString *aAttrString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (Destroyed()) {
    return;
  }

  KeyEventState* currentKeyEvent = GetCurrentKeyEvent();

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TextInputHandler::InsertText, aAttrString=\"%s\", "
     "IsIMEComposing()=%s, IgnoreIMEComposition()=%s, "
     "keyevent=%p, keypressDispatched=%s",
     this, GetCharacters([aAttrString string]), TrueOrFalse(IsIMEComposing()),
     TrueOrFalse(IgnoreIMEComposition()),
     currentKeyEvent ? currentKeyEvent->mKeyEvent : nsnull,
     currentKeyEvent ?
       TrueOrFalse(currentKeyEvent->mKeyPressDispatched) : "N/A"));

  if (IgnoreIMEComposition()) {
    return;
  }

  nsString str;
  nsCocoaUtils::GetStringForNSString([aAttrString string], str);
  if (!IsIMEComposing() && str.IsEmpty()) {
    return; // nothing to do
  }

  if (str.Length() != 1 || IsIMEComposing()) {
    InsertTextAsCommittingComposition(aAttrString);
    return;
  }

  // Don't let the same event be fired twice when hitting
  // enter/return! (Bug 420502)
  if (currentKeyEvent && currentKeyEvent->mKeyPressDispatched) {
    return;
  }

  nsRefPtr<nsChildView> kungFuDeathGrip(mWidget);

  // Dispatch keypress event with char instead of textEvent
  nsKeyEvent keypressEvent(true, NS_KEY_PRESS, mWidget);

  // Don't set other modifiers from the current event, because here in
  // -insertText: they've already been taken into account in creating
  // the input string.

  // create event for use by plugins
#ifndef NP_NO_CARBON
  EventRecord carbonEvent;
#endif // #ifndef NP_NO_CARBON

  if (currentKeyEvent) {
    NSEvent* keyEvent = currentKeyEvent->mKeyEvent;
    InitKeyEvent(keyEvent, keypressEvent);

    // XXX The ASCII characters inputting mode of egbridge (Japanese IME)
    // might send the keyDown event with wrong keyboard layout if other
    // keyboard layouts are already loaded. In that case, the native event
    // doesn't match to this gecko event...
#ifndef NP_NO_CARBON
    if ([mView pluginEventModel] == NPEventModelCarbon) {
      ConvertCocoaKeyEventToCarbonEvent(keyEvent, carbonEvent, true);
      keypressEvent.pluginEvent = &carbonEvent;
    }
#endif // #ifndef NP_NO_CARBON

    if (currentKeyEvent->mKeyDownHandled) {
      keypressEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;
    }
  } else {
    nsCocoaUtils::InitInputEvent(keypressEvent, static_cast<NSEvent*>(nsnull));
    keypressEvent.charCode = str.CharAt(0);
    keypressEvent.keyCode = 0;
    keypressEvent.isChar = true;
    // Note that insertText is not called only at key pressing.
    if (!IsPrintableChar(keypressEvent.charCode)) {
      keypressEvent.keyCode =
        WidgetUtils::ComputeKeyCodeFromChar(keypressEvent.charCode);
      keypressEvent.charCode = 0;
    }
  }

  // Remove basic modifiers from keypress event because if they are included,
  // nsPlaintextEditor ignores the event.
  keypressEvent.modifiers &= ~(widget::MODIFIER_CONTROL |
                               widget::MODIFIER_ALT |
                               widget::MODIFIER_META);

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

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TextInputHandler::DoCommandBySelector, aSelector=\"%s\", "
     "Destroyed()=%s, keypressHandled=%s, causedOtherKeyEvents=%s",
     this, aSelector ? aSelector : "", TrueOrFalse(Destroyed()),
     currentKeyEvent ?
       TrueOrFalse(currentKeyEvent->mKeyPressHandled) : "N/A",
     currentKeyEvent ?
       TrueOrFalse(currentKeyEvent->mCausedOtherKeyEvents) : "N/A"));

  if (currentKeyEvent && !currentKeyEvent->mKeyPressDispatched) {
    nsKeyEvent keypressEvent(true, NS_KEY_PRESS, mWidget);
    InitKeyEvent(currentKeyEvent->mKeyEvent, keypressEvent);
    if (currentKeyEvent->mKeyDownHandled ||
        currentKeyEvent->mCausedOtherKeyEvents) {
      keypressEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;
    }
    currentKeyEvent->mKeyPressHandled = DispatchEvent(keypressEvent);
    currentKeyEvent->mKeyPressDispatched = true;
    PR_LOG(gLog, PR_LOG_ALWAYS,
      ("%p TextInputHandler::DoCommandBySelector, keypress event "
       "dispatched, Destroyed()=%s, keypressHandled=%s",
       this, TrueOrFalse(Destroyed()),
       TrueOrFalse(currentKeyEvent->mKeyPressHandled)));
  }

  return !Destroyed() && currentKeyEvent &&
         (currentKeyEvent->mKeyPressHandled ||
          currentKeyEvent->mCausedOtherKeyEvents);
}


#pragma mark -


/******************************************************************************
 *
 *  IMEInputHandler implementation (static methods)
 *
 ******************************************************************************/

bool IMEInputHandler::sStaticMembersInitialized = false;
CFStringRef IMEInputHandler::sLatestIMEOpenedModeInputSourceID = nsnull;
IMEInputHandler* IMEInputHandler::sFocusedIMEHandler = nsnull;

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

#ifdef PR_LOGGING
  if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
    static CFStringRef sLastTIS = nsnull;
    CFStringRef newTIS;
    tis.GetInputSourceID(newTIS);
    if (!sLastTIS ||
        ::CFStringCompare(sLastTIS, newTIS, 0) != kCFCompareEqualTo) {
      TISInputSourceWrapper tis1, tis2, tis3;
      tis1.InitByCurrentKeyboardLayout();
      tis2.InitByCurrentASCIICapableInputSource();
      tis3.InitByCurrentASCIICapableKeyboardLayout();
      CFStringRef is0, is1, is2, is3, type0, lang0, bundleID0;
      tis.GetInputSourceID(is0);
      tis1.GetInputSourceID(is1);
      tis2.GetInputSourceID(is2);
      tis3.GetInputSourceID(is3);
      tis.GetInputSourceType(type0);
      tis.GetPrimaryLanguage(lang0);
      tis.GetBundleID(bundleID0);

      PR_LOG(gLog, PR_LOG_ALWAYS,
        ("IMEInputHandler::OnCurrentTextInputSourceChange,\n"
         "  Current Input Source is changed to:\n"
         "    currentInputManager=%p\n"
         "    %s\n"
         "      type=%s %s\n"
         "    primary language=%s\n"
         "    bundle ID=%s\n"
         "    current ASCII capable Input Source=%s\n"
         "    current Keyboard Layout=%s\n"
         "    current ASCII capable Keyboard Layout=%s",
         [NSInputManager currentInputManager], GetCharacters(is0),
         GetCharacters(type0), tis.IsASCIICapable() ? "- ASCII capable " : "",
         GetCharacters(lang0), GetCharacters(bundleID0),
         GetCharacters(is2), GetCharacters(is1), GetCharacters(is3)));
    }
    sLastTIS = newTIS;
  }
#endif // #ifdef PR_LOGGING
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
#ifdef PR_LOGGING
  if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
    CFArrayRef list = CreateAllIMEModeList();
    PR_LOG(gLog, PR_LOG_ALWAYS, ("IME mode configuration:"));
    CFIndex idx = ::CFArrayGetCount(list);
    TISInputSourceWrapper tis;
    for (CFIndex i = 0; i < idx; ++i) {
      TISInputSourceRef inputSource = static_cast<TISInputSourceRef>(
        const_cast<void *>(::CFArrayGetValueAtIndex(list, i)));
      tis.InitByTISInputSourceRef(inputSource);
      nsAutoString name, isid;
      tis.GetLocalizedName(name);
      tis.GetInputSourceID(isid);
      PR_LOG(gLog, PR_LOG_ALWAYS,
             ("  %s\t<%s>%s%s\n",
              NS_ConvertUTF16toUTF8(name).get(),
              NS_ConvertUTF16toUTF8(isid).get(),
              tis.IsASCIICapable() ? "" : "\t(Isn't ASCII capable)",
              tis.IsEnabled() ? "" : "\t(Isn't Enabled)"));
    }
    ::CFRelease(list);
  }
#endif // #ifdef PR_LOGGING
}

//static
TSMDocumentID
IMEInputHandler::GetCurrentTSMDocumentID()
{
  // On OS X 10.6.x at least, ::TSMGetActiveDocument() has a bug that prevents
  // it from returning accurate results unless
  // [NSInputManager currentInputManager] is called first.
  // So, we need to call [NSInputManager currentInputManager] first here.
  [NSInputManager currentInputManager];
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
IMEInputHandler::ResetIMEWindowLevel()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::ResetIMEWindowLevel, "
     "Destroyed()=%s, IsFocused()=%s, GetCurrentTSMDocumentID()=%p",
     this, TrueOrFalse(Destroyed()), TrueOrFalse(IsFocused()),
     GetCurrentTSMDocumentID()));

  if (Destroyed()) {
    return;
  }

  if (!IsFocused()) {
    // retry at next focus event
    mPendingMethods |= kResetIMEWindowLevel;
    return;
  }

  TSMDocumentID doc = GetCurrentTSMDocumentID();
  if (!doc) {
    // retry
    mPendingMethods |= kResetIMEWindowLevel;
    NS_WARNING("Application is active but there is no active document");
    ResetTimer();
    return;
  }

  // When the focus of Gecko is on a text box of a popup panel, the actual
  // focused view is the panel's parent view (mView). But the editor is
  // displayed on the popuped widget's view (editorView).  So, their window
  // level may be different.
  NSView<mozView>* editorView = mWidget->GetEditorView();
  if (!editorView) {
    NS_ERROR("editorView is null");
    return;
  }

  // We need to set the focused window level to TSMDocument. Then, the popup
  // windows of IME (E.g., a candidate list window) will be over the focused
  // view. See http://developer.apple.com/technotes/tn2005/tn2128.html#TNTAG1
  NSInteger windowLevel = [[editorView window] level];
  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::ResetIMEWindowLevel, windowLevel=%s (%X)",
     this, GetWindowLevelName(windowLevel), windowLevel));

  // Chinese IMEs on 10.5 don't work fine if the level is NSNormalWindowLevel,
  // then, we need to increment the value.
  if (windowLevel == NSNormalWindowLevel)
    windowLevel++;

  ::TSMSetDocumentProperty(GetCurrentTSMDocumentID(),
                           kTSMDocumentWindowLevelPropertyTag,
                           sizeof(windowLevel), &windowLevel);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::DiscardIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::DiscardIMEComposition, "
     "Destroyed()=%s, IsFocused()=%s, currentInputManager=%p",
     this, TrueOrFalse(Destroyed()), TrueOrFalse(IsFocused()),
     [NSInputManager currentInputManager]));

  if (Destroyed()) {
    return;
  }

  if (!IsFocused()) {
    // retry at next focus event
    mPendingMethods |= kDiscardIMEComposition;
    return;
  }

  NSInputManager* im = [NSInputManager currentInputManager];
  if (!im) {
    // retry
    mPendingMethods |= kDiscardIMEComposition;
    NS_WARNING("Application is active but there is no currentInputManager");
    ResetTimer();
    return;
  }

  mIgnoreIMECommit = true;
  [im markedTextAbandoned: mView];
  mIgnoreIMECommit = false;

  NS_OBJC_END_TRY_ABORT_BLOCK
}

void
IMEInputHandler::SyncASCIICapableOnly()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PR_LOG(gLog, PR_LOG_ALWAYS,
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
    mTimer = nsnull;
  }

  if (![[NSApplication sharedApplication] isActive]) {
    mIsInFocusProcessing = false;
    // If we're not active, we should retry at focus event
    return;
  }

  PRUint32 pendingMethods = mPendingMethods;
  // First, reset the pending method flags because if each methods cannot
  // run now, they can reentry to the pending flags by theirselves.
  mPendingMethods = 0;

  if (pendingMethods & kDiscardIMEComposition)
    DiscardIMEComposition();
  if (pendingMethods & kSyncASCIICapableOnly)
    SyncASCIICapableOnly();
  if (pendingMethods & kResetIMEWindowLevel)
    ResetIMEWindowLevel();

  mIsInFocusProcessing = false;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#pragma mark -


/******************************************************************************
 *
 * IMEInputHandler implementation (native event handlers)
 *
 ******************************************************************************/

PRUint32
IMEInputHandler::ConvertToTextRangeType(PRUint32 aUnderlineStyle,
                                        NSRange& aSelectedRange)
{
  PR_LOG(gLog, PR_LOG_ALWAYS,
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

PRUint32
IMEInputHandler::GetRangeCount(NSAttributedString *aAttrString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // Iterate through aAttrString for the NSUnderlineStyleAttributeName and
  // count the different segments adjusting limitRange as we go.
  PRUint32 count = 0;
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

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::GetRangeCount, aAttrString=\"%s\", count=%llu",
     this, GetCharacters([aAttrString string]), count));

  return count;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

void
IMEInputHandler::SetTextRangeList(nsTArray<nsTextRange>& aTextRangeList,
                                  NSAttributedString *aAttrString,
                                  NSRange& aSelectedRange)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Convert the Cocoa range into the nsTextRange Array used in Gecko.
  // Iterate through the attributed string and map the underline attribute to
  // Gecko IME textrange attributes.  We may need to change the code here if
  // we change the implementation of validAttributesForMarkedText.
  NSRange limitRange = NSMakeRange(0, [aAttrString length]);
  PRUint32 rangeCount = GetRangeCount(aAttrString);
  for (PRUint32 i = 0; i < rangeCount && limitRange.length > 0; i++) {
    NSRange effectiveRange;
    id attributeValue = [aAttrString attribute:NSUnderlineStyleAttributeName
                                       atIndex:limitRange.location
                         longestEffectiveRange:&effectiveRange
                                       inRange:limitRange];

    nsTextRange range;
    range.mStartOffset = effectiveRange.location;
    range.mEndOffset = NSMaxRange(effectiveRange);
    range.mRangeType =
      ConvertToTextRangeType([attributeValue intValue], aSelectedRange);
    aTextRangeList.AppendElement(range);

    PR_LOG(gLog, PR_LOG_ALWAYS,
      ("%p IMEInputHandler::SetTextRangeList, "
       "range={ mStartOffset=%llu, mEndOffset=%llu, mRangeType=%s }",
       this, range.mStartOffset, range.mEndOffset,
       GetRangeTypeName(range.mRangeType)));

    limitRange =
      NSMakeRange(NSMaxRange(effectiveRange), 
                  NSMaxRange(limitRange) - NSMaxRange(effectiveRange));
  }

  // Get current caret position.
  nsTextRange range;
  range.mStartOffset = aSelectedRange.location + aSelectedRange.length;
  range.mEndOffset = range.mStartOffset;
  range.mRangeType = NS_TEXTRANGE_CARETPOSITION;
  aTextRangeList.AppendElement(range);

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::SetTextRangeList, "
     "range={ mStartOffset=%llu, mEndOffset=%llu, mRangeType=%s }",
     this, range.mStartOffset, range.mEndOffset,
     GetRangeTypeName(range.mRangeType)));

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

bool
IMEInputHandler::DispatchTextEvent(const nsString& aText,
                                   NSAttributedString* aAttrString,
                                   NSRange& aSelectedRange,
                                   bool aDoCommit)
{
  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::DispatchTextEvent, "
     "aText=\"%s\", aAttrString=\"%s\", "
     "aSelectedRange={ location=%llu, length=%llu }, "
     "aDoCommit=%s, Destroyed()=%s",
     this, NS_ConvertUTF16toUTF8(aText).get(),
     GetCharacters([aAttrString string]),
     aSelectedRange.location, aSelectedRange.length,
     TrueOrFalse(aDoCommit), TrueOrFalse(Destroyed())));

  NS_ENSURE_TRUE(!Destroyed(), false);

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsTextEvent textEvent(true, NS_TEXT_TEXT, mWidget);
  textEvent.time = PR_IntervalNow();
  textEvent.theText = aText;
  nsAutoTArray<nsTextRange, 4> textRanges;
  if (!aDoCommit) {
    SetTextRangeList(textRanges, aAttrString, aSelectedRange);
  }
  textEvent.rangeArray = textRanges.Elements();
  textEvent.rangeCount = textRanges.Length();

  if (textEvent.theText != mLastDispatchedCompositionString) {
    nsCompositionEvent compositionUpdate(true, NS_COMPOSITION_UPDATE,
                                         mWidget);
    compositionUpdate.time = textEvent.time;
    compositionUpdate.data = textEvent.theText;
    mLastDispatchedCompositionString = textEvent.theText;
    DispatchEvent(compositionUpdate);
    if (mIsInFocusProcessing || Destroyed()) {
      PR_LOG(gLog, PR_LOG_ALWAYS,
        ("%p IMEInputHandler::DispatchTextEvent, compositionupdate causes "
         "aborting the composition, mIsInFocusProcessing=%s, Destryoed()=%s",
         this, TrueOrFalse(mIsInFocusProcessing), TrueOrFalse(Destroyed())));
      if (Destroyed()) {
        return true;
      }
    }
  }

  return DispatchEvent(textEvent);
}

void
IMEInputHandler::InitCompositionEvent(nsCompositionEvent& aCompositionEvent)
{
  aCompositionEvent.time = PR_IntervalNow();
}

void
IMEInputHandler::InsertTextAsCommittingComposition(
                   NSAttributedString* aAttrString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::InsertTextAsCommittingComposition, "
     "aAttrString=\"%s\", Destroyed()=%s, IsIMEComposing()=%s, "
     "mMarkedRange={ location=%llu, length=%llu }",
     this, GetCharacters([aAttrString string]), TrueOrFalse(Destroyed()),
     TrueOrFalse(IsIMEComposing()),
     mMarkedRange.location, mMarkedRange.length));

  if (Destroyed()) {
    return;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsString str;
  nsCocoaUtils::GetStringForNSString([aAttrString string], str);

  if (!IsIMEComposing()) {
    // XXXmnakano Probably, we shouldn't emulate composition in this case.
    // I think that we should just fire DOM3 textInput event if we implement it.
    nsCompositionEvent compStart(true, NS_COMPOSITION_START, mWidget);
    InitCompositionEvent(compStart);

    DispatchEvent(compStart);
    if (Destroyed()) {
      PR_LOG(gLog, PR_LOG_ALWAYS,
        ("%p IMEInputHandler::InsertTextAsCommittingComposition, "
         "destroyed by compositionstart event", this));
      return;
    }

    OnStartIMEComposition();
  }

  if (IgnoreIMECommit()) {
    PR_LOG(gLog, PR_LOG_ALWAYS,
      ("%p IMEInputHandler::InsertTextAsCommittingComposition, "
       "IgnoreIMECommit()=%s", this, TrueOrFalse(IgnoreIMECommit())));
    str.Truncate();
  }

  NSRange range = NSMakeRange(0, str.Length());
  DispatchTextEvent(str, aAttrString, range, true);
  if (Destroyed()) {
    PR_LOG(gLog, PR_LOG_ALWAYS,
      ("%p IMEInputHandler::InsertTextAsCommittingComposition, "
       "destroyed by text event", this));
    return;
  }

  OnUpdateIMEComposition([aAttrString string]);

  nsCompositionEvent compEnd(true, NS_COMPOSITION_END, mWidget);
  InitCompositionEvent(compEnd);
  compEnd.data = mLastDispatchedCompositionString;
  DispatchEvent(compEnd);
  if (Destroyed()) {
    PR_LOG(gLog, PR_LOG_ALWAYS,
      ("%p IMEInputHandler::InsertTextAsCommittingComposition, "
       "destroyed by compositionend event", this));
    return;
  }

  OnEndIMEComposition();

  mMarkedRange = NSMakeRange(NSNotFound, 0);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::SetMarkedText(NSAttributedString* aAttrString,
                               NSRange& aSelectedRange)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::SetMarkedText, "
     "aAttrString=\"%s\", aSelectedRange={ location=%llu, length=%llu }, "
     "Destroyed()=%s, IgnoreIMEComposition()=%s, IsIMEComposing()=%s, "
     "mMarkedRange={ location=%llu, length=%llu }",
     this, GetCharacters([aAttrString string]),
     aSelectedRange.location, aSelectedRange.length, TrueOrFalse(Destroyed()),
     TrueOrFalse(IgnoreIMEComposition()), TrueOrFalse(IsIMEComposing()),
     mMarkedRange.location, mMarkedRange.length));

  if (Destroyed() || IgnoreIMEComposition()) {
    return;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsString str;
  nsCocoaUtils::GetStringForNSString([aAttrString string], str);

  mMarkedRange.length = str.Length();

  if (!IsIMEComposing() && !str.IsEmpty()) {
    nsQueryContentEvent selection(true, NS_QUERY_SELECTED_TEXT,
                                  mWidget);
    DispatchEvent(selection);
    mMarkedRange.location = selection.mSucceeded ? selection.mReply.mOffset : 0;

    nsCompositionEvent compStart(true, NS_COMPOSITION_START, mWidget);
    InitCompositionEvent(compStart);

    DispatchEvent(compStart);
    if (Destroyed()) {
      PR_LOG(gLog, PR_LOG_ALWAYS,
        ("%p IMEInputHandler::SetMarkedText, "
         "destroyed by compositionstart event", this));
      return;
    }

    OnStartIMEComposition();
  }

  if (IsIMEComposing()) {
    OnUpdateIMEComposition([aAttrString string]);

    bool doCommit = str.IsEmpty();
    DispatchTextEvent(str, aAttrString, aSelectedRange, doCommit);
    if (Destroyed()) {
      PR_LOG(gLog, PR_LOG_ALWAYS,
        ("%p IMEInputHandler::SetMarkedText, "
         "destroyed by text event", this));
      return;
    }

    if (doCommit) {
      nsCompositionEvent compEnd(true, NS_COMPOSITION_END, mWidget);
      InitCompositionEvent(compEnd);
      compEnd.data = mLastDispatchedCompositionString;
      DispatchEvent(compEnd);
      if (Destroyed()) {
        PR_LOG(gLog, PR_LOG_ALWAYS,
          ("%p IMEInputHandler::SetMarkedText, "
           "destroyed by compositionend event", this));
        return;
      }
      OnEndIMEComposition();
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NSInteger
IMEInputHandler::ConversationIdentifier()
{
  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::ConversationIdentifier, Destroyed()=%s",
     this, TrueOrFalse(Destroyed())));

  if (Destroyed()) {
    return reinterpret_cast<NSInteger>(mView);
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  // NOTE: The size of NSInteger is same as pointer size.
  nsQueryContentEvent textContent(true, NS_QUERY_TEXT_CONTENT, mWidget);
  textContent.InitForQueryTextContent(0, 0);
  DispatchEvent(textContent);
  if (!textContent.mSucceeded) {
    PR_LOG(gLog, PR_LOG_ALWAYS,
      ("%p IMEInputHandler::ConversationIdentifier, Failed", this));
    return reinterpret_cast<NSInteger>(mView);
  }
  // XXX This might return same ID as a previously existing editor if the
  //     deleted editor was created at the same address.  Is there a better way?
  return reinterpret_cast<NSInteger>(textContent.mReply.mContentsRoot);
}

NSAttributedString*
IMEInputHandler::GetAttributedSubstringFromRange(NSRange& aRange)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::GetAttributedSubstringFromRange, "
     "aRange={ location=%llu, length=%llu }, Destroyed()=%s",
     this, aRange.location, aRange.length, TrueOrFalse(Destroyed())));

  if (Destroyed() || aRange.location == NSNotFound || aRange.length == 0) {
    return nil;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsAutoString str;
  nsQueryContentEvent textContent(true, NS_QUERY_TEXT_CONTENT, mWidget);
  textContent.InitForQueryTextContent(aRange.location, aRange.length);
  DispatchEvent(textContent);

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::GetAttributedSubstringFromRange, "
     "textContent={ mSucceeded=%s, mReply.mString=\"%s\"",
     this, TrueOrFalse(textContent.mSucceeded),
     NS_ConvertUTF16toUTF8(textContent.mReply.mString).get()));

  if (!textContent.mSucceeded || textContent.mReply.mString.IsEmpty()) {
    return nil;
  }

  NSString* nsstr = nsCocoaUtils::ToNSString(textContent.mReply.mString);
  NSAttributedString* result =
    [[[NSAttributedString alloc] initWithString:nsstr
                                     attributes:nil] autorelease];
  return result;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

bool
IMEInputHandler::HasMarkedText()
{
  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::HasMarkedText, "
     "mMarkedRange={ location=%llu, length=%llu }",
     this, mMarkedRange.location, mMarkedRange.length));

  return (mMarkedRange.location != NSNotFound) && (mMarkedRange.length != 0);
}

NSRange
IMEInputHandler::MarkedRange()
{
  PR_LOG(gLog, PR_LOG_ALWAYS,
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

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::SelectedRange, Destroyed()=%s",
     this, TrueOrFalse(Destroyed())));

  NSRange range = NSMakeRange(NSNotFound, 0);
  if (Destroyed()) {
    return range;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsQueryContentEvent selection(true, NS_QUERY_SELECTED_TEXT, mWidget);
  DispatchEvent(selection);

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::SelectedRange, selection={ mSucceeded=%s, "
     "mReply={ mOffset=%llu, mString.Length()=%llu } }",
     this, TrueOrFalse(selection.mSucceeded), selection.mReply.mOffset,
     selection.mReply.mString.Length()));

  if (!selection.mSucceeded) {
    return range;
  }

  return NSMakeRange(selection.mReply.mOffset,
                     selection.mReply.mString.Length());

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRange(0, 0));
}

NSRect
IMEInputHandler::FirstRectForCharacterRange(NSRange& aRange)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::FirstRectForCharacterRange, Destroyed()=%s"
     "aRange={ location=%llu, length=%llu }",
     this, TrueOrFalse(Destroyed()), aRange.location, aRange.length));

  // XXX this returns first character rect or caret rect, it is limitation of
  // now. We need more work for returns first line rect. But current
  // implementation is enough for IMEs.

  NSRect rect;
  if (Destroyed() || aRange.location == NSNotFound) {
    return rect;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsIntRect r;
  bool useCaretRect = (aRange.length == 0);
  if (!useCaretRect) {
    nsQueryContentEvent charRect(true, NS_QUERY_TEXT_RECT, mWidget);
    charRect.InitForQueryTextRect(aRange.location, 1);
    DispatchEvent(charRect);
    if (charRect.mSucceeded) {
      r = charRect.mReply.mRect;
    } else {
      useCaretRect = true;
    }
  }

  if (useCaretRect) {
    nsQueryContentEvent caretRect(true, NS_QUERY_CARET_RECT, mWidget);
    caretRect.InitForQueryCaretRect(aRange.location);
    DispatchEvent(caretRect);
    if (!caretRect.mSucceeded) {
      return rect;
    }
    r = caretRect.mReply.mRect;
    r.width = 0;
  }

  nsIWidget* rootWidget = mWidget->GetTopLevelWidget();
  NSWindow* rootWindow =
    static_cast<NSWindow*>(rootWidget->GetNativeData(NS_NATIVE_WINDOW));
  NSView* rootView =
    static_cast<NSView*>(rootWidget->GetNativeData(NS_NATIVE_WIDGET));
  if (!rootWindow || !rootView) {
    return rect;
  }
  nsCocoaUtils::GeckoRectToNSRect(r, rect);
  rect = [rootView convertRect:rect toView:nil];
  rect.origin = [rootWindow convertBaseToScreen:rect.origin];

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::FirstRectForCharacterRange, "
     "useCaretRect=%s rect={ x=%f, y=%f, width=%f, height=%f }",
     this, TrueOrFalse(useCaretRect), rect.origin.x, rect.origin.y,
     rect.size.width, rect.size.height));

  return rect;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRect(0.0, 0.0, 0.0, 0.0));
}

NSUInteger
IMEInputHandler::CharacterIndexForPoint(NSPoint& aPoint)
{
  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::CharacterIndexForPoint, aPoint={ x=%f, y=%f }",
     this, aPoint.x, aPoint.y));

  //nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  // To implement this, we'd have to grovel in text frames looking at text
  // offsets.
  return 0;
}

NSArray*
IMEInputHandler::GetValidAttributesForMarkedText()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  PR_LOG(gLog, PR_LOG_ALWAYS,
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
  PluginTextInputHandler(aWidget, aNativeView),
  mPendingMethods(0), mIMECompositionString(nsnull),
  mIsIMEComposing(false), mIsIMEEnabled(true),
  mIsASCIICapableOnly(false), mIgnoreIMECommit(false),
  mIsInFocusProcessing(false)
{
  InitStaticMembers();

  mMarkedRange.location = NSNotFound;
  mMarkedRange.length = 0;
}

IMEInputHandler::~IMEInputHandler()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }
  if (sFocusedIMEHandler == this) {
    sFocusedIMEHandler = nsnull;
  }
}

void
IMEInputHandler::OnFocusChangeInGecko(bool aFocus)
{
  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::OnFocusChangeInGecko, aFocus=%s, Destroyed()=%s, "
     "sFocusedIMEHandler=%p",
     this, TrueOrFalse(aFocus), TrueOrFalse(Destroyed()), sFocusedIMEHandler));

  // This is called when the native focus is changed and when the native focus
  // isn't changed but the focus is changed in Gecko.
  // XXX currently, we're not called this method with false, we need to
  // improve the nsIMEStateManager implementation.
  if (!aFocus) {
    if (sFocusedIMEHandler == this)
      sFocusedIMEHandler = nsnull;
    return;
  }

  sFocusedIMEHandler = this;
  mIsInFocusProcessing = true;

  // We need to reset the IME's window level by the current focused view of
  // Gecko.  It may be different from mView.  However, we cannot get the
  // new focused view here because the focus change process in Gecko hasn't
  // been finished yet.  So, we should post the job to the todo list.
  mPendingMethods |= kResetIMEWindowLevel;
  ResetTimer();
}

bool
IMEInputHandler::OnDestroyWidget(nsChildView* aDestroyingWidget)
{
  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::OnDestroyWidget, aDestroyingWidget=%p, "
     "sFocusedIMEHandler=%p, IsIMEComposing()=%s",
     this, aDestroyingWidget, sFocusedIMEHandler,
     TrueOrFalse(IsIMEComposing())));

  // If we're not focused, the focused IMEInputHandler may have been
  // created by another widget/nsChildView.
  if (sFocusedIMEHandler && sFocusedIMEHandler != this) {
    sFocusedIMEHandler->OnDestroyWidget(aDestroyingWidget);
  }

  if (!PluginTextInputHandler::OnDestroyWidget(aDestroyingWidget)) {
    return false;
  }

  if (IsIMEComposing()) {
    // If our view is in the composition, we should clean up it.
    CancelIMEComposition();
    OnEndIMEComposition();
  }

  return true;
}

void
IMEInputHandler::OnStartIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::OnStartIMEComposition, mView=%p, mWidget=%p"
     "currentInputManager=%p, mIsIMEComposing=%s",
     this, mView, mWidget, [NSInputManager currentInputManager],
     TrueOrFalse(mIsIMEComposing)));

  NS_ASSERTION(!mIsIMEComposing, "There is a composition already");
  mIsIMEComposing = true;

  mLastDispatchedCompositionString.Truncate();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::OnUpdateIMEComposition(NSString* aIMECompositionString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::OnUpdateIMEComposition, mView=%p, mWidget=%p, "
     "currentInputManager=%p, mIsIMEComposing=%s, aIMECompositionString=\"%s\"",
     this, mView, mWidget, [NSInputManager currentInputManager],
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

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::OnEndIMEComposition, mView=%p, mWidget=%p, "
     "currentInputManager=%p, mIsIMEComposing=%s",
     this, mView, mWidget, [NSInputManager currentInputManager],
     TrueOrFalse(mIsIMEComposing)));

  NS_ASSERTION(mIsIMEComposing, "We're not in composition");

  mIsIMEComposing = false;

  if (mIMECompositionString) {
    [mIMECompositionString release];
    mIMECompositionString = nsnull;
  }

  mLastDispatchedCompositionString.Truncate();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::SendCommittedText(NSString *aString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::SendCommittedText, mView=%p, mWidget=%p, "
     "currentInputManager=%p, mIsIMEComposing=%s",
     this, mView, mWidget, [NSInputManager currentInputManager],
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

  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::KillIMEComposition, mView=%p, mWidget=%p, "
     "currentInputManager=%p, mIsIMEComposing=%s, "
     "Destroyed()=%s, IsFocused()=%s",
     this, mView, mWidget, [NSInputManager currentInputManager],
     TrueOrFalse(mIsIMEComposing), TrueOrFalse(Destroyed()),
     TrueOrFalse(IsFocused())));

  if (Destroyed()) {
    return;
  }

  if (IsFocused()) {
    [[NSInputManager currentInputManager] markedTextAbandoned: mView];
    return;
  }

  PR_LOG(gLog, PR_LOG_ALWAYS,
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

  PR_LOG(gLog, PR_LOG_ALWAYS,
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

  PR_LOG(gLog, PR_LOG_ALWAYS,
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
         ([window isMainWindow] || [window isSheet]) &&
         [[NSApplication sharedApplication] isActive];

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
  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p IMEInputHandler::OpenSystemPreferredLanguageIME", this));

  CFArrayRef langList = ::CFLocaleCopyPreferredLanguages();
  if (!langList) {
    PR_LOG(gLog, PR_LOG_ALWAYS,
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
#ifdef PR_LOGGING
        if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
          CFStringRef foundTIS;
          tis.GetInputSourceID(foundTIS);
          PR_LOG(gLog, PR_LOG_ALWAYS,
            ("%p IMEInputHandler::OpenSystemPreferredLanguageIME, "
             "foundTIS=%s, lang=%s",
             this, GetCharacters(foundTIS), GetCharacters(lang)));
        }
#endif // #ifdef PR_LOGGING
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


#pragma mark -


/******************************************************************************
 *
 *  PluginTextInputHandler implementation
 *
 ******************************************************************************/

PluginTextInputHandler::PluginTextInputHandler(nsChildView* aWidget,
                                               NSView<mozView> *aNativeView) :
  TextInputHandlerBase(aWidget, aNativeView),
  mIgnoreNextKeyUpEvent(false),
#ifndef NP_NO_CARBON
  mPluginTSMDoc(0), mPluginTSMInComposition(false),
#endif // #ifndef NP_NO_CARBON
  mPluginComplexTextInputRequested(false)
{
}

PluginTextInputHandler::~PluginTextInputHandler()
{
#ifndef NP_NO_CARBON
  if (mPluginTSMDoc) {
    ::DeleteTSMDocument(mPluginTSMDoc);
  }
#endif // #ifndef NP_NO_CARBON
}

/* static */ void
PluginTextInputHandler::ConvertCocoaKeyEventToNPCocoaEvent(
                          NSEvent* aCocoaEvent,
                          NPCocoaEvent& aPluginEvent)
{
  nsCocoaUtils::InitNPCocoaEvent(&aPluginEvent);
  NSEventType nativeType = [aCocoaEvent type];
  switch (nativeType) {
    case NSKeyDown:
      aPluginEvent.type = NPCocoaEventKeyDown;
      break;
    case NSKeyUp:
      aPluginEvent.type = NPCocoaEventKeyUp;
      break;
    case NSFlagsChanged:
      aPluginEvent.type = NPCocoaEventFlagsChanged;
      break;
    default:
      NS_WARNING("Asked to convert key event of unknown type to Cocoa plugin event!");
  }
  aPluginEvent.data.key.modifierFlags = [aCocoaEvent modifierFlags];
  aPluginEvent.data.key.keyCode = [aCocoaEvent keyCode];
  // don't try to access character data for flags changed events,
  // it will raise an exception
  if (nativeType != NSFlagsChanged) {
    aPluginEvent.data.key.characters = (NPNSString*)[aCocoaEvent characters];
    aPluginEvent.data.key.charactersIgnoringModifiers =
      (NPNSString*)[aCocoaEvent charactersIgnoringModifiers];
    aPluginEvent.data.key.isARepeat = [aCocoaEvent isARepeat];
  }
}

#ifndef NP_NO_CARBON

/* static */ bool
PluginTextInputHandler::ConvertUnicodeToCharCode(PRUnichar aUniChar,
                                                 unsigned char* aOutChar)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  UnicodeToTextInfo converterInfo;
  TextEncoding      systemEncoding;
  Str255            convertedString;

  *aOutChar = nsnull;

  OSStatus err =
    ::UpgradeScriptInfoToTextEncoding(smSystemScript,
                                      kTextLanguageDontCare,
                                      kTextRegionDontCare,
                                      NULL,
                                      &systemEncoding);
  NS_ENSURE_TRUE(err == noErr, false);

  err = ::CreateUnicodeToTextInfoByEncoding(systemEncoding, &converterInfo);
  NS_ENSURE_TRUE(err == noErr, false);

  err = ::ConvertFromUnicodeToPString(converterInfo, sizeof(PRUnichar),
                                      &aUniChar, convertedString);
  NS_ENSURE_TRUE(err == noErr, false);

  *aOutChar = convertedString[1];
  ::DisposeUnicodeToTextInfo(&converterInfo);
  return true;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

/* static */ void
PluginTextInputHandler::ConvertCocoaKeyEventToCarbonEvent(
                          NSEvent* aCocoaKeyEvent,
                          EventRecord& aCarbonKeyEvent,
                          bool aMakeKeyDownEventIfNSFlagsChanged)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  UInt32 charCode = 0;
  if ([aCocoaKeyEvent type] == NSFlagsChanged) {
    aCarbonKeyEvent.what = aMakeKeyDownEventIfNSFlagsChanged ? keyDown : keyUp;
  } else {
    if ([[aCocoaKeyEvent characters] length] > 0) {
      charCode = [[aCocoaKeyEvent characters] characterAtIndex:0];
    }
    if ([aCocoaKeyEvent type] == NSKeyDown) {
      aCarbonKeyEvent.what = [aCocoaKeyEvent isARepeat] ? autoKey : keyDown;
    } else {
      aCarbonKeyEvent.what = keyUp;
    }
  }

  if (charCode >= 0x0080) {
    switch (charCode) {
      case NSUpArrowFunctionKey:
        charCode = kUpArrowCharCode;
        break;
      case NSDownArrowFunctionKey:
        charCode = kDownArrowCharCode;
        break;
      case NSLeftArrowFunctionKey:
        charCode = kLeftArrowCharCode;
        break;
      case NSRightArrowFunctionKey:
        charCode = kRightArrowCharCode;
        break;
      default:
        unsigned char convertedCharCode;
        if (ConvertUnicodeToCharCode(charCode, &convertedCharCode)) {
          charCode = convertedCharCode;
        }
        //NSLog(@"charcode is %d, converted to %c, char is %@",
        //      charCode, convertedCharCode, [aCocoaKeyEvent characters]);
        break;
    }
  }

  aCarbonKeyEvent.message =
    (charCode & 0x00FF) | ([aCocoaKeyEvent keyCode] << 8);
  aCarbonKeyEvent.when = ::TickCount();
  ::GetGlobalMouse(&aCarbonKeyEvent.where);
  // XXX Is this correct? If ::GetCurrentKeyModifiers() returns "current"
  //     state and there is one or more pending modifier key events,
  //     the result is mismatch with the state at current key event.
  aCarbonKeyEvent.modifiers = ::GetCurrentKeyModifiers();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

EventHandlerRef PluginTextInputHandler::sPluginKeyEventsHandler = NULL;

/* static */ void
PluginTextInputHandler::InstallPluginKeyEventsHandler()
{
  if (sPluginKeyEventsHandler) {
    return;
  }
  static const EventTypeSpec sTSMEvents[] =
    { { kEventClassTextInput, kEventTextInputUnicodeForKeyEvent } };
  ::InstallEventHandler(::GetEventDispatcherTarget(),
                        ::NewEventHandlerUPP(PluginKeyEventsHandler),
                        GetEventTypeCount(sTSMEvents),
                        sTSMEvents,
                        NULL,
                        &sPluginKeyEventsHandler);
}

/* static */ void
PluginTextInputHandler::RemovePluginKeyEventsHandler()
{
  if (!sPluginKeyEventsHandler) {
    return;
  }
  ::RemoveEventHandler(sPluginKeyEventsHandler);
  sPluginKeyEventsHandler = NULL;
}

/* static */ void
PluginTextInputHandler::SwizzleMethods()
{
  Class IMKInputSessionClass = ::NSClassFromString(@"IMKInputSession");
  nsToolkit::SwizzleMethods(IMKInputSessionClass, @selector(handleEvent:),
    @selector(PluginTextInputHandler_IMKInputSession_handleEvent:));
  nsToolkit::SwizzleMethods(IMKInputSessionClass, @selector(commitComposition),
    @selector(PluginTextInputHandler_IMKInputSession_commitComposition));
  nsToolkit::SwizzleMethods(IMKInputSessionClass, @selector(finishSession),
    @selector(PluginTextInputHandler_IMKInputSession_finishSession));
}

/* static */ OSStatus
PluginTextInputHandler::PluginKeyEventsHandler(EventHandlerCallRef aHandlerRef,
                                               EventRef aEvent,
                                               void *aUserData)
{
  nsAutoreleasePool localPool;

  TSMDocumentID activeDoc = ::TSMGetActiveDocument();
  NS_ENSURE_TRUE(activeDoc, eventNotHandledErr);

  ChildView *target = nil;
  OSStatus status = ::TSMGetDocumentProperty(activeDoc,
                                             kFocusedChildViewTSMDocPropertyTag,
                                             sizeof(ChildView *), nil, &target);
  NS_ENSURE_TRUE(status == noErr, eventNotHandledErr);
  NS_ENSURE_TRUE(target, eventNotHandledErr);

  EventRef keyEvent = NULL;
  status = ::GetEventParameter(aEvent, kEventParamTextInputSendKeyboardEvent,
                               typeEventRef, NULL, sizeof(EventRef), NULL,
                               &keyEvent);
  NS_ENSURE_TRUE(status == noErr, eventNotHandledErr);
  NS_ENSURE_TRUE(keyEvent, eventNotHandledErr);

  nsIWidget* widget = [target widget];
  NS_ENSURE_TRUE(widget, eventNotHandledErr);
  TextInputHandler*  handler =
    static_cast<nsChildView*>(widget)->GetTextInputHandler();
  NS_ENSURE_TRUE(handler, eventNotHandledErr);
  handler->HandleCarbonPluginKeyEvent(keyEvent);

  return noErr;
}

// Called from PluginKeyEventsHandler() (a handler for Carbon TSM events) to
// process a Carbon key event for the currently focused plugin.  Both Unicode
// characters and "Mac encoding characters" (in the MBCS or "multibyte
// character system") are (or should be) available from aKeyEvent, but here we
// use the MCBS characters.  This is how the WebKit does things, and seems to
// be what plugins expect.
void
PluginTextInputHandler::HandleCarbonPluginKeyEvent(EventRef aKeyEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (Destroyed()) {
    return;
  }

  NS_ASSERTION(mView, "mView must not be NULL");

  nsRefPtr<nsChildView> kungFuDeathGrip(mWidget);

  if ([mView pluginEventModel] == NPEventModelCocoa) {
    UInt32 size;
    OSStatus status =
      ::GetEventParameter(aKeyEvent, kEventParamKeyUnicodes, typeUnicodeText,
                          NULL, 0, &size, NULL);
    NS_ENSURE_TRUE(status == noErr, );

    UniChar* chars = (UniChar*)malloc(size);
    NS_ENSURE_TRUE(chars, );

    status = ::GetEventParameter(aKeyEvent, kEventParamKeyUnicodes,
                                 typeUnicodeText, NULL, size, NULL, chars);
    if (status != noErr) {
      free(chars);
      return;
    }

    CFStringRef text =
      ::CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, chars,
                                           (size / sizeof(UniChar)),
                                           kCFAllocatorNull);
    if (!text) {
      free(chars);
      return;
    }

    NPCocoaEvent cocoaTextEvent;
    nsCocoaUtils::InitNPCocoaEvent(&cocoaTextEvent);
    cocoaTextEvent.type = NPCocoaEventTextInput;
    cocoaTextEvent.data.text.text = (NPNSString*)text;

    nsPluginEvent pluginEvent(true, NS_PLUGIN_INPUT_EVENT, mWidget);
    nsCocoaUtils::InitPluginEvent(pluginEvent, cocoaTextEvent);
    DispatchEvent(pluginEvent);

    ::CFRelease(text);
    free(chars);

    return;
  }

  UInt32 numCharCodes;
  OSStatus status = ::GetEventParameter(aKeyEvent, kEventParamKeyMacCharCodes,
                                        typeChar, NULL, 0, &numCharCodes, NULL);
  NS_ENSURE_TRUE(status == noErr, );

  nsAutoTArray<unsigned char, 3> charCodes;
  charCodes.SetLength(numCharCodes);
  status = ::GetEventParameter(aKeyEvent, kEventParamKeyMacCharCodes,
                               typeChar, NULL, numCharCodes, NULL,
                               charCodes.Elements());
  NS_ENSURE_TRUE(status == noErr, );

  UInt32 modifiers;
  status = ::GetEventParameter(aKeyEvent, kEventParamKeyModifiers,
                               typeUInt32, NULL, sizeof(modifiers), NULL,
                               &modifiers);
  NS_ENSURE_TRUE(status == noErr, );

  NSUInteger cocoaModifiers = 0;
  if (modifiers & shiftKey) {
    cocoaModifiers |= NSShiftKeyMask;
  }
  if (modifiers & controlKey) {
    cocoaModifiers |= NSControlKeyMask;
  }
  if (modifiers & optionKey) {
    cocoaModifiers |= NSAlternateKeyMask;
  }
  if (modifiers & cmdKey) { // Should never happen
    cocoaModifiers |= NSCommandKeyMask;
  }

  UInt32 macKeyCode;
  status = ::GetEventParameter(aKeyEvent, kEventParamKeyCode,
                               typeUInt32, NULL, sizeof(macKeyCode), NULL,
                               &macKeyCode);
  NS_ENSURE_TRUE(status == noErr, );

  TISInputSourceWrapper currentKeyboardLayout;
  currentKeyboardLayout.InitByCurrentKeyboardLayout();

  EventRef cloneEvent = ::CopyEvent(aKeyEvent);
  for (PRUint32 i = 0; i < numCharCodes; ++i) {
    status = ::SetEventParameter(cloneEvent, kEventParamKeyMacCharCodes,
                                 typeChar, 1, charCodes.Elements() + i);
    NS_ENSURE_TRUE(status == noErr, );

    EventRecord eventRec;
    if (::ConvertEventRefToEventRecord(cloneEvent, &eventRec)) {
      nsKeyEvent keydownEvent(true, NS_KEY_DOWN, mWidget);
      nsCocoaUtils::InitInputEvent(keydownEvent, cocoaModifiers);

      PRUint32 keyCode =
        currentKeyboardLayout.ComputeGeckoKeyCode(macKeyCode, ::LMGetKbdType(),
                                                  keydownEvent.IsMeta());
      PRUint32 charCode(charCodes.ElementAt(i));

      keydownEvent.time = PR_IntervalNow();
      keydownEvent.pluginEvent = &eventRec;
      if (IsSpecialGeckoKey(macKeyCode)) {
        keydownEvent.keyCode = keyCode;
      } else {
        // XXX This is wrong. charCode must be 0 for keydown event.
        keydownEvent.charCode = charCode;
        keydownEvent.isChar   = true;
      }
      DispatchEvent(keydownEvent);
      if (Destroyed()) {
        break;
      }
    }
  }

  ::ReleaseEvent(cloneEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
PluginTextInputHandler::ActivatePluginTSMDocument()
{
  if (!mPluginTSMDoc) {
    // Create a TSM document that supports both non-Unicode and Unicode input.
    // Though ProcessPluginKeyEvent() only sends Mac char codes to
    // the plugin, this makes the input window behave better when it contains
    // more than one kind of input (say Hiragana and Romaji).  This is what
    // the OS does when it creates a TSM document for use by an
    // NSTSMInputContext class.
    InterfaceTypeList supportedServices;
    supportedServices[0] = kTextServiceDocumentInterfaceType;
    supportedServices[1] = kUnicodeDocumentInterfaceType;
    ::NewTSMDocument(2, supportedServices, &mPluginTSMDoc, 0);
    // We'll need to use the "input window".
    ::UseInputWindow(mPluginTSMDoc, YES);
    ::ActivateTSMDocument(mPluginTSMDoc);
  } else if (::TSMGetActiveDocument() != mPluginTSMDoc) {
    ::ActivateTSMDocument(mPluginTSMDoc);
  }
}

#endif // #ifndef NP_NO_CARBON

void
PluginTextInputHandler::HandleKeyDownEventForPlugin(NSEvent* aNativeKeyEvent)
{
  if (Destroyed()) {
    return;
  }

  NS_ASSERTION(mView, "mView must not be NULL");

#ifdef NP_NO_CARBON

  if ([mView pluginEventModel] != NPEventModelCocoa) {
    return;
  }

  ComplexTextInputPanel* ctiPanel =
    [ComplexTextInputPanel sharedComplexTextInputPanel];

  // If a composition is in progress then simply let the input panel continue
  // it.
  if (IsInPluginComposition()) {
    // Don't send key up events for key downs associated with compositions.
    mIgnoreNextKeyUpEvent = true;

    NSString* textString = nil;
    [ctiPanel interpretKeyEvent:aNativeKeyEvent string:&textString];
    if (textString) {
      DispatchCocoaNPAPITextEvent(textString);
    }

    return;
  }

  // Reset complex text input request flag.
  mPluginComplexTextInputRequested = false;

  // Send key down event to the plugin.
  nsPluginEvent pluginEvent(true, NS_PLUGIN_INPUT_EVENT, mWidget);
  NPCocoaEvent cocoaEvent;
  ConvertCocoaKeyEventToNPCocoaEvent(aNativeKeyEvent, cocoaEvent);
  nsCocoaUtils::InitPluginEvent(pluginEvent, cocoaEvent);
  DispatchEvent(pluginEvent);
  if (Destroyed()) {
    return;
  }

  // Start complex text composition if requested.
  if (mPluginComplexTextInputRequested) {
    // Don't send key up events for key downs associated with compositions.
    mIgnoreNextKeyUpEvent = true;

    NSString* textString = nil;
    [ctiPanel interpretKeyEvent:aNativeKeyEvent string:&textString];
    if (textString) {
      DispatchCocoaNPAPITextEvent(textString);
    }
  }

#else // #ifdef NP_NO_CARBON

  bool wasInComposition = false;
  if ([mView pluginEventModel] == NPEventModelCocoa) {
    if (IsInPluginComposition()) {
      wasInComposition = true;

      // Don't send key up events for key downs associated with compositions.
      mIgnoreNextKeyUpEvent = true;
    } else {
      // Reset complex text input request flag.
      mPluginComplexTextInputRequested = false;

      // Send key down event to the plugin.
      nsPluginEvent pluginEvent(true, NS_PLUGIN_INPUT_EVENT, mWidget);
      NPCocoaEvent cocoaEvent;
      ConvertCocoaKeyEventToNPCocoaEvent(aNativeKeyEvent, cocoaEvent);
      nsCocoaUtils::InitPluginEvent(pluginEvent, cocoaEvent);
      DispatchEvent(pluginEvent);
      if (Destroyed()) {
        return;
      }

      // Only continue if plugin wants complex text input.
      if (!mPluginComplexTextInputRequested) {
        return;
      }

      // Don't send key up events for key downs associated with compositions.
      mIgnoreNextKeyUpEvent = true;
    }

    // Don't send complex text input to a plugin in Cocoa event mode if
    // either the Control key or the Command key is pressed -- even if the
    // plugin has requested it, or we are already in IME composition.  This
    // conforms to our behavior in 64-bit mode and fixes bug 619217.
    NSUInteger modifierFlags = [aNativeKeyEvent modifierFlags];
    if ((modifierFlags & NSControlKeyMask) ||
        (modifierFlags & NSCommandKeyMask)) {
      return;
    }
  }

  // This will take care of all Carbon plugin events and also send Cocoa plugin
  // text events when NSInputContext is not available (ifndef NP_NO_CARBON).
  ActivatePluginTSMDocument();

  // We use the active TSM document to pass a pointer to ourselves (the
  // currently focused ChildView) to PluginKeyEventsHandler().  Because this
  // pointer is weak, we should retain and release ourselves around the call
  // to TSMProcessRawKeyEvent().
  nsAutoRetainCocoaObject kungFuDeathGrip(mView);
  ::TSMSetDocumentProperty(mPluginTSMDoc,
                           kFocusedChildViewTSMDocPropertyTag,
                           sizeof(ChildView *), &mView);
  ::TSMProcessRawKeyEvent([aNativeKeyEvent _eventRef]);
  ::TSMRemoveDocumentProperty(mPluginTSMDoc,
                              kFocusedChildViewTSMDocPropertyTag);

#endif // #ifdef NP_NO_CARBON else
}

void
PluginTextInputHandler::HandleKeyUpEventForPlugin(NSEvent* aNativeKeyEvent)
{
  if (mIgnoreNextKeyUpEvent) {
    mIgnoreNextKeyUpEvent = false;
    return;
  }

  if (Destroyed()) {
    return;
  }

  NS_ASSERTION(mView, "mView must not be NULL");

  NPEventModel eventModel = [mView pluginEventModel];
  if (eventModel == NPEventModelCocoa) {
    // Don't send key up events to Cocoa plugins during composition.
    if (IsInPluginComposition()) {
      return;
    }

    nsKeyEvent keyupEvent(true, NS_KEY_UP, mWidget);
    InitKeyEvent(aNativeKeyEvent, keyupEvent);
    NPCocoaEvent pluginEvent;
    ConvertCocoaKeyEventToNPCocoaEvent(aNativeKeyEvent, pluginEvent);
    keyupEvent.pluginEvent = &pluginEvent;
    DispatchEvent(keyupEvent);
    return;
  }

#ifndef NP_NO_CARBON
  if (eventModel == NPEventModelCarbon) {
    // I'm not sure the call to TSMProcessRawKeyEvent() is needed here (though
    // WebKit makes one).
    ::TSMProcessRawKeyEvent([aNativeKeyEvent _eventRef]);

    // Don't send a keyUp event if the corresponding keyDown event(s) is/are
    // still being processed (idea borrowed from WebKit).
    ChildView* keydownTarget = nil;
    OSStatus status =
      ::TSMGetDocumentProperty(mPluginTSMDoc,
                               kFocusedChildViewTSMDocPropertyTag,
                               sizeof(ChildView *), nil, &keydownTarget);
    NS_ENSURE_TRUE(status == noErr, );
    if (keydownTarget == mView) {
      return;
    }

    // PluginKeyEventsHandler() never sends keyUp events to
    // HandleCarbonPluginKeyEvent(), so we need to send them to Gecko here.
    // (This means that when commiting text from IME, several keyDown events
    // may be sent to Gecko (in processPluginKeyEvent) for one keyUp event here.
    // But this is how the WebKit does it, and games expect a keyUp event to
    // be sent when it actually happens (they need to be able to detect how
    // long a key has been held down) -- which wouldn't be possible if we sent
    // them from processPluginKeyEvent.)
    nsKeyEvent keyupEvent(true, NS_KEY_UP, mWidget);
    InitKeyEvent(aNativeKeyEvent, keyupEvent);
    EventRecord pluginEvent;
    ConvertCocoaKeyEventToCarbonEvent(aNativeKeyEvent, pluginEvent, false);
    keyupEvent.pluginEvent = &pluginEvent;
    DispatchEvent(keyupEvent);
    return;
  }
#endif // #ifndef NP_NO_CARBON

}

bool
PluginTextInputHandler::IsInPluginComposition()
{
  return
#ifdef NP_NO_CARBON
    [[ComplexTextInputPanel sharedComplexTextInputPanel] inComposition] != NO;
#else // #ifdef NP_NO_CARBON
    mPluginTSMInComposition;
#endif // #ifdef NP_NO_CARBON else
}

bool
PluginTextInputHandler::DispatchCocoaNPAPITextEvent(NSString* aString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NPCocoaEvent cocoaTextEvent;
  nsCocoaUtils::InitNPCocoaEvent(&cocoaTextEvent);
  cocoaTextEvent.type = NPCocoaEventTextInput;
  cocoaTextEvent.data.text.text = (NPNSString*)aString;

  nsPluginEvent pluginEvent(true, NS_PLUGIN_INPUT_EVENT, mWidget);
  nsCocoaUtils::InitPluginEvent(pluginEvent, cocoaTextEvent);
  return DispatchEvent(pluginEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}


#pragma mark -


#ifndef NP_NO_CARBON

/******************************************************************************
 *
 *  PluginTextInputHandler_IMKInputSession_* implementation
 *
 ******************************************************************************/

// IMKInputSession is an undocumented class in the HIToolbox framework.  It's
// present on both Leopard and SnowLeopard, and is used at a low level to
// process IME input regardless of which high-level API is used (Text Services
// Manager or Cocoa).  It works the same way in both 32-bit and 64-bit code.
@interface NSObject (IMKInputSessionMethodSwizzling)
- (BOOL)PluginTextInputHandler_IMKInputSession_handleEvent:(EventRef)theEvent;
- (void)PluginTextInputHandler_IMKInputSession_commitComposition;
- (void)PluginTextInputHandler_IMKInputSession_finishSession;
@end

@implementation NSObject (IMKInputSessionMethodSwizzling)

- (BOOL)PluginTextInputHandler_IMKInputSession_handleEvent:(EventRef)theEvent
{
  [self retain];
  BOOL retval =
    [self PluginTextInputHandler_IMKInputSession_handleEvent:theEvent];
  NSUInteger retainCount = [self retainCount];
  [self release];
  // Return without doing anything if we've been deleted.
  if (retainCount == 1) {
    return retval;
  }

  NSWindow *mainWindow = [NSApp mainWindow];
  NSResponder *firstResponder = [mainWindow firstResponder];
  if (![firstResponder isKindOfClass:[ChildView class]]) {
    return retval;
  }

  // 'charactersEntered' is the length (in bytes) of currently "marked text"
  // -- text that's been entered in IME but not yet committed.  If it's
  // non-zero we're composing text in an IME session; if it's zero we're
  // not in an IME session.
  NSInteger entered = 0;
  object_getInstanceVariable(self, "charactersEntered",
                             (void **) &entered);
  nsIWidget* widget = [(ChildView*)firstResponder widget];
  NS_ENSURE_TRUE(widget, retval);
  TextInputHandler* handler =
    static_cast<nsChildView*>(widget)->GetTextInputHandler();
  NS_ENSURE_TRUE(handler, retval);
  handler->SetPluginTSMInComposition(entered != 0);

  return retval;
}

// This method is called whenever IME input is committed as a result of an
// "abnormal" termination -- for example when changing the keyboard focus from
// one input field to another.
- (void)PluginTextInputHandler_IMKInputSession_commitComposition
{
  NSWindow *mainWindow = [NSApp mainWindow];
  NSResponder *firstResponder = [mainWindow firstResponder];
  if ([firstResponder isKindOfClass:[ChildView class]]) {
    nsIWidget* widget = [(ChildView*)firstResponder widget];
    if (widget) {
      TextInputHandler* handler =
        static_cast<nsChildView*>(widget)->GetTextInputHandler();
      if (handler) {
        handler->SetPluginTSMInComposition(false);
      }
    }
  }
  [self PluginTextInputHandler_IMKInputSession_commitComposition];
}

// This method is called just before we're deallocated.
- (void)PluginTextInputHandler_IMKInputSession_finishSession
{
  NSWindow *mainWindow = [NSApp mainWindow];
  NSResponder *firstResponder = [mainWindow firstResponder];
  if ([firstResponder isKindOfClass:[ChildView class]]) {
    nsIWidget* widget = [(ChildView*)firstResponder widget];
    if (widget) {
      TextInputHandler* handler =
        static_cast<nsChildView*>(widget)->GetTextInputHandler();
      if (handler) {
        handler->SetPluginTSMInComposition(false);
      }
    }
  }
  [self PluginTextInputHandler_IMKInputSession_finishSession];
}

@end

#endif // #ifndef NP_NO_CARBON


#pragma mark -


/******************************************************************************
 *
 *  TextInputHandlerBase implementation
 *
 ******************************************************************************/

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
    FinalizeCurrentKeyboardLayout();
  }
}

bool
TextInputHandlerBase::OnDestroyWidget(nsChildView* aDestroyingWidget)
{
  PR_LOG(gLog, PR_LOG_ALWAYS,
    ("%p TextInputHandlerBase::OnDestroyWidget, "
     "aDestroyingWidget=%p, mWidget=%p",
     this, aDestroyingWidget, mWidget));

  if (aDestroyingWidget != mWidget) {
    return false;
  }

  mWidget = nsnull;
  return true;
}

bool
TextInputHandlerBase::DispatchEvent(nsGUIEvent& aEvent)
{
  if (aEvent.message == NS_KEY_PRESS) {
    nsInputEvent& inputEvent = static_cast<nsInputEvent&>(aEvent);
    if (!inputEvent.IsMeta()) {
      PR_LOG(gLog, PR_LOG_ALWAYS,
        ("%p TextInputHandlerBase::DispatchEvent, hiding mouse cursor", this));
      [NSCursor setHiddenUntilMouseMoves:YES];
    }
  }
  return mWidget->DispatchWindowEvent(aEvent);
}

void
TextInputHandlerBase::InitKeyEvent(NSEvent *aNativeKeyEvent,
                                   nsKeyEvent& aKeyEvent)
{
  NS_ASSERTION(aNativeKeyEvent, "aNativeKeyEvent must not be NULL");

  TISInputSourceWrapper tis;
  if (mKeyboardOverride.mOverrideEnabled) {
    tis.InitByLayoutID(mKeyboardOverride.mKeyboardLayout, true);
  } else {
    tis.InitByCurrentKeyboardLayout();
  }
  tis.InitKeyEvent(aNativeKeyEvent, aKeyEvent);
}

nsresult
TextInputHandlerBase::SynthesizeNativeKeyEvent(
                        PRInt32 aNativeKeyboardLayout,
                        PRInt32 aNativeKeyCode,
                        PRUint32 aModifierFlags,
                        const nsAString& aCharacters,
                        const nsAString& aUnmodifiedCharacters)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  static const PRUint32 sModifierFlagMap[][2] = {
    { nsIWidget::CAPS_LOCK,       NSAlphaShiftKeyMask },
    { nsIWidget::SHIFT_L,         NSShiftKeyMask },
    { nsIWidget::SHIFT_R,         NSShiftKeyMask },
    { nsIWidget::CTRL_L,          NSControlKeyMask },
    { nsIWidget::CTRL_R,          NSControlKeyMask },
    { nsIWidget::ALT_L,           NSAlternateKeyMask },
    { nsIWidget::ALT_R,           NSAlternateKeyMask },
    { nsIWidget::COMMAND_L,       NSCommandKeyMask },
    { nsIWidget::COMMAND_R,       NSCommandKeyMask },
    { nsIWidget::NUMERIC_KEY_PAD, NSNumericPadKeyMask },
    { nsIWidget::HELP,            NSHelpKeyMask },
    { nsIWidget::FUNCTION,        NSFunctionKeyMask }
  };

  PRUint32 modifierFlags = 0;
  for (PRUint32 i = 0; i < ArrayLength(sModifierFlagMap); ++i) {
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

/* static */ bool
TextInputHandlerBase::IsPrintableChar(PRUnichar aChar)
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
    case kEscapeKeyCode:
    case kShiftKeyCode:
    case kRShiftKeyCode:
    case kCommandKeyCode:
    case kRCommandKeyCode:
    case kCapsLockKeyCode:
    case kControlKeyCode:
    case kRControlKeyCode:
    case kOptionkeyCode:
    case kROptionKeyCode:
    case kClearKeyCode:

    // function keys
    case kF1KeyCode:
    case kF2KeyCode:
    case kF3KeyCode:
    case kF4KeyCode:
    case kF5KeyCode:
    case kF6KeyCode:
    case kF7KeyCode:
    case kF8KeyCode:
    case kF9KeyCode:
    case kF10KeyCode:
    case kF11KeyCode:
    case kF12KeyCode:
    case kPauseKeyCode:
    case kScrollLockKeyCode:
    case kPrintScreenKeyCode:
    case kF16KeyCode:
    case kF17KeyCode:
    case kF18KeyCode:
    case kF19KeyCode:

    case kInsertKeyCode:
    case kDeleteKeyCode:
    case kTabKeyCode:
    case kBackspaceKeyCode:

    case kJapanese_Eisu:
    case kJapanese_Kana:

    case kHomeKeyCode:
    case kEndKeyCode:
    case kPageUpKeyCode:
    case kPageDownKeyCode:
    case kLeftArrowKeyCode:
    case kRightArrowKeyCode:
    case kUpArrowKeyCode:
    case kDownArrowKeyCode:
    case kReturnKeyCode:
    case kEnterKeyCode:
    case kPowerbookEnterKeyCode:
      return true;
  }
  return false;
}

/* static */ bool
TextInputHandlerBase::IsNormalCharInputtingEvent(const nsKeyEvent& aKeyEvent)
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
    case kCapsLockKeyCode:
    case kRCommandKeyCode:
    case kCommandKeyCode:
    case kShiftKeyCode:
    case kOptionkeyCode:
    case kControlKeyCode:
    case kRShiftKeyCode:
    case kROptionKeyCode:
    case kRControlKeyCode:
      return true;
  }
  return false;
}
