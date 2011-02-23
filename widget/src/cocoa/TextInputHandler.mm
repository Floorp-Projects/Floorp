/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TextInputHandler.h"

#include "nsChildView.h"
#include "nsObjCExceptions.h"
#include "nsBidiUtils.h"

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif
#include "prlog.h"

using namespace mozilla::widget;

#undef DEBUG_IME_HANDLER
#undef DEBUG_TEXT_INPUT_HANDLER
//#define DEBUG_IME_HANDLER 1
//#define DEBUG_TEXT_INPUT_HANDLER 1

// TODO: static methods should be moved to nsCocoaUtils

static void
GetStringForNSString(const NSString *aSrc, nsAString& aDist)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!aSrc) {
    aDist.Truncate();
    return;
  }

  aDist.SetLength([aSrc length]);
  [aSrc getCharacters: aDist.BeginWriting()];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static NSString* ToNSString(const nsAString& aString)
{
  return [NSString stringWithCharacters:aString.BeginReading()
                                 length:aString.Length()];
}

static inline void
GeckoRectToNSRect(const nsIntRect& inGeckoRect, NSRect& outCocoaRect)
{
  outCocoaRect.origin.x = inGeckoRect.x;
  outCocoaRect.origin.y = inGeckoRect.y;
  outCocoaRect.size.width = inGeckoRect.width;
  outCocoaRect.size.height = inGeckoRect.height;
}

static NSEvent*
MakeNewCocoaEventWithType(NSEventType aEventType, NSEvent *aEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSEvent* newEvent =
    [NSEvent     keyEventWithType:aEventType
                         location:[aEvent locationInWindow] 
                    modifierFlags:[aEvent modifierFlags]
                        timestamp:[aEvent timestamp]
                     windowNumber:[aEvent windowNumber]
                          context:[aEvent context]
                       characters:[aEvent characters]
      charactersIgnoringModifiers:[aEvent charactersIgnoringModifiers]
                        isARepeat:[aEvent isARepeat]
                          keyCode:[aEvent keyCode]];
  return newEvent;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

#ifdef DEBUG_IME_HANDLER

static const char*
TrueOrFalse(PRBool aBool)
{
  return aBool ? "TRUE" : "FALSE";
}

static void
DebugPrintPointer(IMEInputHandler* aHandler)
{
  static IMEInputHandler* sLastHandler = nsnull;
  if (aHandler == sLastHandler)
    return;
  sLastHandler = aHandler;
  NSLog(@"%8p ************************************************", aHandler);
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

#endif // DEBUG_IME_HANDLER

static PRUint32 gHandlerInstanceCount = 0;
static TISInputSourceWrapper gCurrentKeyboardLayout;

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

PRBool
TISInputSourceWrapper::TranslateToString(UInt32 aKeyCode, UInt32 aModifiers,
                                         UInt32 aKbType, nsAString &aStr)
{
  aStr.Truncate();

  const UCKeyboardLayout* UCKey = GetUCKeyboardLayout();
  NS_ENSURE_TRUE(UCKey, PR_FALSE);

#ifdef DEBUG_TEXT_INPUT_HANDLER
  NSLog(@"**** TISInputSourceWrapper::TranslateToString: UCKey: %p, aKeyCode: %X, aModifiers: %X, aKbType: %X",
        UCKey, aKeyCode, aModifiers, aKbType);
  PRBool isShift = aModifiers & shiftKey;
  PRBool isCtrl = aModifiers & controlKey;
  PRBool isOpt = aModifiers & optionKey;
  PRBool isCmd = aModifiers & cmdKey;
  PRBool isCL = aModifiers & alphaLock;
  PRBool isNL = aModifiers & kEventKeyModifierNumLockMask;
  NSLog(@"        Shift: %s, Ctrl: %s, Opt: %s, Cmd: %s, CapsLock: %s, NumLock: %s",
        isShift ? "ON" : "off", isCtrl ? "ON" : "off", isOpt ? "ON" : "off",
        isCmd ? "ON" : "off", isCL ? "ON" : "off", isNL ? "ON" : "off");
#endif
  UInt32 deadKeyState = 0;
  UniCharCount len;
  UniChar chars[5];
  OSStatus err = ::UCKeyTranslate(UCKey, aKeyCode,
                                  kUCKeyActionDown, aModifiers >> 8,
                                  aKbType, kUCKeyTranslateNoDeadKeysMask,
                                  &deadKeyState, 5, &len, chars);
  NS_ENSURE_TRUE(err == noErr, PR_FALSE);
  if (len == 0) {
    return PR_TRUE;
  }
  NS_ENSURE_TRUE(EnsureStringLength(aStr, len), PR_FALSE);
  NS_ASSERTION(sizeof(PRUnichar) == sizeof(UniChar),
               "size of PRUnichar and size of UniChar are different");
  memcpy(aStr.BeginWriting(), chars, len * sizeof(PRUnichar));
#ifdef DEBUG_TEXT_INPUT_HANDLER
  for (PRUint32 i = 0; i < PRUint32(len); i++) {
    NSLog(@"       result: %X(%C)", chars[i], chars[i] > ' ' ? chars[i] : ' ');
  }
#endif
  return PR_TRUE;
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
                                      PRBool aOverrideKeyboard)
{
  // NOTE: Doument new layout IDs in TextInputHandler.h when you add ones.
  switch (aLayoutID) {
    case 0:
      InitByInputSourceID("com.apple.keylayout.US");
      break;
    case -18944:
      InitByInputSourceID("com.apple.keylayout.Greek");
      break;
    case 3:
      InitByInputSourceID("com.apple.keylayout.German");
      break;
    case 224:
      InitByInputSourceID("com.apple.keylayout.Swedish-Pro");
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

PRBool
TISInputSourceWrapper::GetBoolProperty(const CFStringRef aKey)
{
  CFBooleanRef ret = static_cast<CFBooleanRef>(
    ::TISGetInputSourceProperty(mInputSource, aKey));
  return ::CFBooleanGetValue(ret);
}

PRBool
TISInputSourceWrapper::GetStringProperty(const CFStringRef aKey,
                                         CFStringRef &aStr)
{
  aStr = static_cast<CFStringRef>(
    ::TISGetInputSourceProperty(mInputSource, aKey));
  return aStr != nsnull;
}

PRBool
TISInputSourceWrapper::GetStringProperty(const CFStringRef aKey,
                                         nsAString &aStr)
{
  CFStringRef str;
  GetStringProperty(aKey, str);
  GetStringForNSString((const NSString*)str, aStr);
  return !aStr.IsEmpty();
}

PRBool
TISInputSourceWrapper::IsOpenedIMEMode()
{
  NS_ENSURE_TRUE(mInputSource, PR_FALSE);
  if (!IsIMEMode())
    return PR_FALSE;
  return !IsASCIICapable();
}

PRBool
TISInputSourceWrapper::IsIMEMode()
{
  NS_ENSURE_TRUE(mInputSource, PR_FALSE);
  CFStringRef str;
  GetInputSourceType(str);
  NS_ENSURE_TRUE(str, PR_FALSE);
  return ::CFStringCompare(kTISTypeKeyboardInputMode,
                           str, 0) == kCFCompareEqualTo;
}

PRBool
TISInputSourceWrapper::GetLanguageList(CFArrayRef &aLanguageList)
{
  NS_ENSURE_TRUE(mInputSource, PR_FALSE);
  aLanguageList = static_cast<CFArrayRef>(
    ::TISGetInputSourceProperty(mInputSource,
                                kTISPropertyInputSourceLanguages));
  return aLanguageList != nsnull;
}

PRBool
TISInputSourceWrapper::GetPrimaryLanguage(CFStringRef &aPrimaryLanguage)
{
  NS_ENSURE_TRUE(mInputSource, PR_FALSE);
  CFArrayRef langList;
  NS_ENSURE_TRUE(GetLanguageList(langList), PR_FALSE);
  if (::CFArrayGetCount(langList) == 0)
    return PR_FALSE;
  aPrimaryLanguage =
    static_cast<CFStringRef>(::CFArrayGetValueAtIndex(langList, 0));
  return aPrimaryLanguage != nsnull;
}

PRBool
TISInputSourceWrapper::GetPrimaryLanguage(nsAString &aPrimaryLanguage)
{
  NS_ENSURE_TRUE(mInputSource, PR_FALSE);
  CFStringRef primaryLanguage;
  NS_ENSURE_TRUE(GetPrimaryLanguage(primaryLanguage), PR_FALSE);
  GetStringForNSString((const NSString*)primaryLanguage, aPrimaryLanguage);
  return !aPrimaryLanguage.IsEmpty();
}

PRBool
TISInputSourceWrapper::IsForRTLLanguage()
{
  if (mIsRTL < 0) {
    // Get the input character of the 'A' key of ANSI keyboard layout.
    nsAutoString str;
    PRBool ret = TranslateToString(kVK_ANSI_A, 0, eKbdType_ANSI, str);
    NS_ENSURE_TRUE(ret, ret);
    PRUnichar ch = str.IsEmpty() ? PRUnichar(0) : str.CharAt(0);
    mIsRTL = UCS2_CHAR_IS_BIDI(ch) || ch == 0xD802 || ch == 0xD803;
  }
  return mIsRTL != 0;
}

PRBool
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
  if (mInputSourceList) {
    ::CFRelease(mInputSourceList);
  }
  mInputSourceList = nsnull;
  mInputSource = nsnull;
  mIsRTL = -1;
  mUCKeyboardLayout = nsnull;
  mOverrideKeyboard = PR_FALSE;
}

void
TISInputSourceWrapper::InitKeyEvent(NSEvent *aNativeKeyEvent,
                                    nsKeyEvent& aKeyEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ENSURE_TRUE(aNativeKeyEvent, );

  aKeyEvent.time = PR_IntervalNow();

  NSUInteger modifiers = [aNativeKeyEvent modifierFlags];
  aKeyEvent.isShift   = ((modifiers & NSShiftKeyMask) != 0);
  aKeyEvent.isControl = ((modifiers & NSControlKeyMask) != 0);
  aKeyEvent.isAlt     = ((modifiers & NSAlternateKeyMask) != 0);
  aKeyEvent.isMeta    = ((modifiers & NSCommandKeyMask) != 0);

  aKeyEvent.refPoint = nsIntPoint(0, 0);
  aKeyEvent.isChar = PR_FALSE; // XXX not used in XP level

  NSString* str = nil;
  if ([aNativeKeyEvent type] != NSFlagsChanged) {
    str = [aNativeKeyEvent charactersIgnoringModifiers];
  }
  aKeyEvent.keyCode =
    TextInputHandler::ComputeGeckoKeyCode([aNativeKeyEvent keyCode], str);

  if (aKeyEvent.message == NS_KEY_PRESS &&
      !TextInputHandler::IsSpecialGeckoKey([aNativeKeyEvent keyCode])) {
    InitKeyPressEvent(aNativeKeyEvent, aKeyEvent);
    return;
  }

  aKeyEvent.charCode = 0;


  NS_OBJC_END_TRY_ABORT_BLOCK
}

void
TISInputSourceWrapper::InitKeyPressEvent(NSEvent *aNativeKeyEvent,
                                         nsKeyEvent& aKeyEvent)
{
  NS_ASSERTION(aKeyEvent.message == NS_KEY_PRESS,
               "aKeyEvent must be NS_KEY_PRESS event");

  aKeyEvent.isChar = PR_TRUE; // this is not a special key  XXX not used in XP

  aKeyEvent.charCode = 0;
  NSString* chars = [aNativeKeyEvent characters];
  if ([chars length] > 0) {
    aKeyEvent.charCode = [chars characterAtIndex:0];
  }

  // convert control-modified charCode to raw charCode (with appropriate case)
  if (aKeyEvent.isControl && aKeyEvent.charCode <= 26) {
    aKeyEvent.charCode += (aKeyEvent.isShift) ? ('A' - 1) : ('a' - 1);
  }

  if (aKeyEvent.charCode != 0) {
    aKeyEvent.keyCode = 0;
  }

  if (!aKeyEvent.isControl && !aKeyEvent.isMeta && !aKeyEvent.isAlt) {
    return;
  }

  TISInputSourceWrapper USLayout("com.apple.keylayout.US");
  PRBool isRomanKeyboardLayout = IsASCIICapable();

  // If a keyboard layout override is set, we also need to force the
  // keyboard type to something ANSI to avoid test failures on machines
  // with JIS keyboards (since the pair of keyboard layout and physical
  // keyboard type form the actual key layout).  This assumes that the
  // test setting the override was written assuming an ANSI keyboard.
  UInt32 kbType = mOverrideKeyboard ? eKbdType_ANSI : ::LMGetKbdType();

  UInt32 key = [aNativeKeyEvent keyCode];

  // Caps lock and num lock modifier state:
  UInt32 lockState = 0;
  if ([aNativeKeyEvent modifierFlags] & NSAlphaShiftKeyMask) {
    lockState |= alphaLock;
  }
  if ([aNativeKeyEvent modifierFlags] & NSNumericPadKeyMask) {
    lockState |= kEventKeyModifierNumLockMask;
  }

  nsString str;

  // normal chars
  PRUint32 unshiftedChar = TranslateToChar(key, lockState, kbType);
  UInt32 shiftLockMod = shiftKey | lockState;
  PRUint32 shiftedChar = TranslateToChar(key, shiftLockMod, kbType);

  // characters generated with Cmd key
  // XXX we should remove CapsLock state, which changes characters from
  //     Latin to Cyrillic with Russian layout on 10.4 only when Cmd key
  //     is pressed.
  UInt32 numState = (lockState & ~alphaLock); // only num lock state
  PRUint32 uncmdedChar = TranslateToChar(key, numState, kbType);
  UInt32 shiftNumMod = numState | shiftKey;
  PRUint32 uncmdedShiftChar = TranslateToChar(key, shiftNumMod, kbType);
  PRUint32 uncmdedUSChar = USLayout.TranslateToChar(key, numState, kbType);
  UInt32 cmdNumMod = cmdKey | numState;
  PRUint32 cmdedChar = TranslateToChar(key, cmdNumMod, kbType);
  UInt32 cmdShiftNumMod = shiftKey | cmdNumMod;
  PRUint32 cmdedShiftChar = TranslateToChar(key, cmdShiftNumMod, kbType);

  // Is the keyboard layout changed by Cmd key?
  // E.g., Arabic, Russian, Hebrew, Greek and Dvorak-QWERTY.
  PRBool isCmdSwitchLayout = uncmdedChar != cmdedChar;
  // Is the keyboard layout for Latin, but Cmd key switches the layout?
  // I.e., Dvorak-QWERTY
  PRBool isDvorakQWERTY = isCmdSwitchLayout && isRomanKeyboardLayout;

  // If the current keyboard is not Dvorak-QWERTY or Cmd is not pressed,
  // we should append unshiftedChar and shiftedChar for handling the
  // normal characters.  These are the characters that the user is most
  // likely to associate with this key.
  if ((unshiftedChar || shiftedChar) &&
      (!aKeyEvent.isMeta || !isDvorakQWERTY)) {
    nsAlternativeCharCode altCharCodes(unshiftedChar, shiftedChar);
    aKeyEvent.alternativeCharCodes.AppendElement(altCharCodes);
  }

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
  PRBool hasCmdShiftOnlyChar =
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
    PRUint32 ch = USLayout.TranslateToChar(key, lockState, kbType);
    if (ch) {
      cmdedChar = ch;
    }
    ch = USLayout.TranslateToChar(key, shiftLockMod, kbType);
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
  if (aKeyEvent.isMeta && !(aKeyEvent.isControl || aKeyEvent.isAlt)) {
    // The character to use for charCode.
    PRUint32 preferredCharCode = 0;
    preferredCharCode = aKeyEvent.isShift ? cmdedShiftChar : cmdedChar;

    if (preferredCharCode) {
#ifdef DEBUG_TEXT_INPUT_HANDLER
      if (aKeyEvent.charCode != preferredCharCode) {
        NSLog(@"      charCode replaced: %X(%C) to %X(%C)",
              aKeyEvent.charCode,
              aKeyEvent.charCode > ' ' ? aKeyEvent.charCode : ' ',
              preferredCharCode,
              preferredCharCode > ' ' ? preferredCharCode : ' ');
      }
#endif
      aKeyEvent.charCode = preferredCharCode;
    }
  }

  // If the current keyboard layout is switched by the Cmd key,
  // we should append cmdedChar and shiftedCmdChar that are
  // Latin char for the key. But don't append at Dvorak-QWERTY.
  if ((cmdedChar || cmdedShiftChar) && isCmdSwitchLayout && !isDvorakQWERTY) {
    nsAlternativeCharCode altCharCodes(cmdedChar, cmdedShiftChar);
    aKeyEvent.alternativeCharCodes.AppendElement(altCharCodes);
  }
  // Special case for 'SS' key of German layout. See the comment of
  // hasCmdShiftOnlyChar definition for the detail.
  if (hasCmdShiftOnlyChar && originalCmdedShiftChar) {
    nsAlternativeCharCode altCharCodes(0, originalCmdedShiftChar);
    aKeyEvent.alternativeCharCodes.AppendElement(altCharCodes);
  }
}


#pragma mark -


/******************************************************************************
 *
 *  TextInputHandler implementation (static methods)
 *
 ******************************************************************************/

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
TextInputHandler::DebugPrintAllKeyboardLayouts(PRLogModuleInfo* aLogModuleInfo)
{
  CFArrayRef list = CreateAllKeyboardLayoutList();
  PR_LOG(aLogModuleInfo, PR_LOG_ALWAYS, ("Keyboard layout configuration:"));
  CFIndex idx = ::CFArrayGetCount(list);
  TISInputSourceWrapper tis;
  for (CFIndex i = 0; i < idx; ++i) {
    TISInputSourceRef inputSource = static_cast<TISInputSourceRef>(
      const_cast<void *>(::CFArrayGetValueAtIndex(list, i)));
    tis.InitByTISInputSourceRef(inputSource);
    nsAutoString name, isid;
    tis.GetLocalizedName(name);
    tis.GetInputSourceID(isid);
    PR_LOG(aLogModuleInfo, PR_LOG_ALWAYS,
           ("  %s\t<%s>%s%s\n",
            NS_ConvertUTF16toUTF8(name).get(),
            NS_ConvertUTF16toUTF8(isid).get(),
            tis.IsASCIICapable() ? "" : "\t(Isn't ASCII capable)",
            tis.GetUCKeyboardLayout() ? "" : "\t(uchr is NOT AVAILABLE)"));
  }
  ::CFRelease(list);
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
  [mView installTextInputHandler:this];
}

TextInputHandler::~TextInputHandler()
{
  [mView uninstallTextInputHandler];
}

PRBool
TextInputHandler::HandleKeyDownEvent(NSEvent* aNativeEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (Destroyed()) {
    return PR_FALSE;
  }

  nsRefPtr<nsChildView> kungFuDeathGrip(mWidget);

  mCurrentKeyEvent.Set(aNativeEvent);
  AutoKeyEventStateCleaner remover(this);

  BOOL nonDeadKeyPress = [[aNativeEvent characters] length] > 0;
  if (nonDeadKeyPress && !IsIMEComposing()) {
    NSResponder* firstResponder = [[mView window] firstResponder];

    nsKeyEvent keydownEvent(PR_TRUE, NS_KEY_DOWN, mWidget);
    InitKeyEvent(aNativeEvent, keydownEvent);

#ifndef NP_NO_CARBON
    EventRecord carbonEvent;
    if ([mView pluginEventModel] == NPEventModelCarbon) {
      ConvertCocoaKeyEventToCarbonEvent(aNativeEvent, carbonEvent, PR_TRUE);
      keydownEvent.pluginEvent = &carbonEvent;
    }
#endif // #ifndef NP_NO_CARBON

    mCurrentKeyEvent.mKeyDownHandled = DispatchEvent(keydownEvent);
    if (Destroyed()) {
      return mCurrentKeyEvent.KeyDownOrPressHandled();
    }

    // The key down event may have shifted the focus, in which
    // case we should not fire the key press.
    // XXX This is a special code only on Cocoa widget, why is this needed?
    if (firstResponder != [[mView window] firstResponder]) {
      return mCurrentKeyEvent.KeyDownOrPressHandled();
    }

    // If this is the context menu key command, send a context menu key event.
    NSUInteger modifierFlags =
      [aNativeEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask;
    if (modifierFlags == NSControlKeyMask &&
        [[aNativeEvent charactersIgnoringModifiers] isEqualToString:@" "]) {
      nsMouseEvent contextMenuEvent(PR_TRUE, NS_CONTEXTMENU,
                                    [mView widget], nsMouseEvent::eReal,
                                    nsMouseEvent::eContextMenuKey);
      contextMenuEvent.isShift = contextMenuEvent.isControl =
        contextMenuEvent.isAlt = contextMenuEvent.isMeta = PR_FALSE;

      PRBool cmEventHandled = DispatchEvent(contextMenuEvent);
      [mView maybeInitContextMenuTracking];
      // Bail, there is nothing else to do here.
      return (cmEventHandled || mCurrentKeyEvent.KeyDownOrPressHandled());
    }

    nsKeyEvent keypressEvent(PR_TRUE, NS_KEY_PRESS, mWidget);
    InitKeyEvent(aNativeEvent, keypressEvent);

    // if this is a non-letter keypress, or the control key is down,
    // dispatch the keydown to gecko, so that we trap delete,
    // control-letter combinations etc before Cocoa tries to use
    // them for keybindings.
    // XXX This is wrong. IME may be handle the non-letter keypress event as
    //     its owning shortcut key.  See bug 477291.
    if ((!keypressEvent.isChar || keypressEvent.isControl) &&
        !IsIMEComposing()) {
      if (mCurrentKeyEvent.mKeyDownHandled) {
        keypressEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;
      }
      mCurrentKeyEvent.mKeyPressHandled = DispatchEvent(keypressEvent);
      mCurrentKeyEvent.mKeyPressDispatched = PR_TRUE;
      if (Destroyed()) {
        return mCurrentKeyEvent.KeyDownOrPressHandled();
      }
    }
  }

  // Let Cocoa interpret the key events, caching IsIMEComposing first.
  PRBool wasComposing = IsIMEComposing();
  PRBool interpretKeyEventsCalled = PR_FALSE;
  if (IsIMEEnabled() || IsASCIICapableOnly()) {
    [mView interpretKeyEvents:[NSArray arrayWithObject:aNativeEvent]];
    interpretKeyEventsCalled = PR_TRUE;
  }

  if (Destroyed()) {
    return mCurrentKeyEvent.KeyDownOrPressHandled();
  }

  if (!mCurrentKeyEvent.mKeyPressDispatched && nonDeadKeyPress &&
      !wasComposing && !IsIMEComposing()) {
    nsKeyEvent keypressEvent(PR_TRUE, NS_KEY_PRESS, mWidget);
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
      if (mCurrentKeyEvent.mKeyDownHandled) {
        keypressEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;
      }
      mCurrentKeyEvent.mKeyPressHandled = DispatchEvent(keypressEvent);
    }
  }

  // Note: mWidget might have become null here. Don't count on it from here on.

  return mCurrentKeyEvent.KeyDownOrPressHandled();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(PR_FALSE);
}

void
TextInputHandler::InsertText(NSAttributedString *aAttrString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (Destroyed()) {
    return;
  }

  if (IgnoreIMEComposition()) {
    return;
  }

  nsString str;
  GetStringForNSString([aAttrString string], str);
  if (!IsIMEComposing() && str.IsEmpty()) {
    return; // nothing to do
  }

  if (str.Length() != 1 || IsIMEComposing()) {
    InsertTextAsCommittingComposition(aAttrString);
    return;
  }

  // Don't let the same event be fired twice when hitting
  // enter/return! (Bug 420502)
  if (mCurrentKeyEvent.mKeyPressDispatched) {
    return;
  }

  nsRefPtr<nsChildView> kungFuDeathGrip(mWidget);

  // Dispatch keypress event with char instead of textEvent
  nsKeyEvent keypressEvent(PR_TRUE, NS_KEY_PRESS, mWidget);
  keypressEvent.time      = PR_IntervalNow();
  keypressEvent.charCode  = str.CharAt(0);
  keypressEvent.keyCode   = 0;
  keypressEvent.isChar    = PR_TRUE;

  // Don't set other modifiers from the current event, because here in
  // -insertText: they've already been taken into account in creating
  // the input string.

  // create event for use by plugins
#ifndef NP_NO_CARBON
  EventRecord carbonEvent;
#endif // #ifndef NP_NO_CARBON

  if (mCurrentKeyEvent.mKeyEvent) {
    NSEvent* keyEvent = mCurrentKeyEvent.mKeyEvent;

    // XXX The ASCII characters inputting mode of egbridge (Japanese IME)
    // might send the keyDown event with wrong keyboard layout if other
    // keyboard layouts are already loaded. In that case, the native event
    // doesn't match to this gecko event...
#ifndef NP_NO_CARBON
    if ([mView pluginEventModel] == NPEventModelCarbon) {
      ConvertCocoaKeyEventToCarbonEvent(keyEvent, carbonEvent, PR_TRUE);
      keypressEvent.pluginEvent = &carbonEvent;
    }
#endif // #ifndef NP_NO_CARBON

    if (mCurrentKeyEvent.mKeyDownHandled) {
      keypressEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;
    }

    keypressEvent.isShift = ([keyEvent modifierFlags] & NSShiftKeyMask) != 0;
    if (!IsPrintableChar(keypressEvent.charCode)) {
      keypressEvent.keyCode =
        ComputeGeckoKeyCode([keyEvent keyCode],
                            [keyEvent charactersIgnoringModifiers]);
      keypressEvent.charCode = 0;
    }
  } else {
    // Note that insertText is not called only at key pressing.
    if (!IsPrintableChar(keypressEvent.charCode)) {
      keypressEvent.keyCode =
        ComputeGeckoKeyCodeFromChar(keypressEvent.charCode);
      keypressEvent.charCode = 0;
    }
  }

  // TODO:
  // If mCurrentKeyEvent.mKeyEvent is null and when we implement textInput
  // event of DOM3 Events, we should dispatch it instead of keypress event.
  PRBool keyPressHandled = DispatchEvent(keypressEvent);

  // Note: mWidget might have become null here. Don't count on it from here on.

  if (mCurrentKeyEvent.mKeyEvent) {
    mCurrentKeyEvent.mKeyPressHandled = keyPressHandled;
    mCurrentKeyEvent.mKeyPressDispatched = PR_TRUE;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


#pragma mark -


/******************************************************************************
 *
 *  IMEInputHandler implementation (static methods)
 *
 ******************************************************************************/

PRBool IMEInputHandler::sStaticMembersInitialized = PR_FALSE;
CFStringRef IMEInputHandler::sLatestIMEOpenedModeInputSourceID = nsnull;
IMEInputHandler* IMEInputHandler::sFocusedIMEHandler = nsnull;

// static
void
IMEInputHandler::InitStaticMembers()
{
  if (sStaticMembersInitialized)
    return;
  sStaticMembersInitialized = PR_TRUE;
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

#ifdef DEBUG_IME_HANDLER
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
    NSLog(@"********************************************");
    NSLog(@"Current Input Source is changed to:");
    NSLog(@"  current InputManager: %p", [NSInputManager currentInputManager]);
    NSLog(@"  %@", is0);
    NSLog(@"    type: %@ %s", type0,
          tis.IsASCIICapable() ? "- ASCII capable " : "");
    NSLog(@"    primary language: %@", lang0);
    NSLog(@"    bundle ID: %@", bundleID0);
    NSLog(@"    current ASCII capable Input Source: %@", is2);
    NSLog(@"    current Keyboard Layout: %@", is1);
    NSLog(@"    current ASCII capable Keyboard Layout: %@", is3);
    NSLog(@"********************************************");
  }
  sLastTIS = newTIS;
#endif // DEBUG_IME_HANDLER
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
IMEInputHandler::DebugPrintAllIMEModes(PRLogModuleInfo* aLogModuleInfo)
{
  CFArrayRef list = CreateAllIMEModeList();
  PR_LOG(aLogModuleInfo, PR_LOG_ALWAYS, ("IME mode configuration:"));
  CFIndex idx = ::CFArrayGetCount(list);
  TISInputSourceWrapper tis;
  for (CFIndex i = 0; i < idx; ++i) {
    TISInputSourceRef inputSource = static_cast<TISInputSourceRef>(
      const_cast<void *>(::CFArrayGetValueAtIndex(list, i)));
    tis.InitByTISInputSourceRef(inputSource);
    nsAutoString name, isid;
    tis.GetLocalizedName(name);
    tis.GetInputSourceID(isid);
    PR_LOG(aLogModuleInfo, PR_LOG_ALWAYS,
           ("  %s\t<%s>%s%s\n",
            NS_ConvertUTF16toUTF8(name).get(),
            NS_ConvertUTF16toUTF8(isid).get(),
            tis.IsASCIICapable() ? "" : "\t(Isn't ASCII capable)",
            tis.IsEnabled() ? "" : "\t(Isn't Enabled)"));
  }
  ::CFRelease(list);
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

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"IMEInputHandler::ResetIMEWindowLevel");
  NSLog(@"  IsFocused:%s GetCurrentTSMDocumentID():%p",
        TrueOrFalse(IsFocused()), GetCurrentTSMDocumentID());
#endif // DEBUG_IME_HANDLER

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
#ifdef DEBUG_IME_HANDLER
  NSLog(@"  windowLevel:%s (%x)",
        GetWindowLevelName(windowLevel), windowLevel);
#endif // DEBUG_IME_HANDLER

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

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"IMEInputHandler::DiscardIMEComposition");
  NSLog(@"  currentInputManager:%p", [NSInputManager currentInputManager]);
#endif // DEBUG_IME_HANDLER

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

  mIgnoreIMECommit = PR_TRUE;
  [im markedTextAbandoned: mView];
  mIgnoreIMECommit = PR_FALSE;

  NS_OBJC_END_TRY_ABORT_BLOCK
}

void
IMEInputHandler::SyncASCIICapableOnly()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"IMEInputHandler::SyncASCIICapableOnly");
  NSLog(@"  IsFocused:%s GetCurrentTSMDocumentID():%p",
        TrueOrFalse(IsFocused()), GetCurrentTSMDocumentID());
#endif

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
    mIsInFocusProcessing = PR_FALSE;
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

  mIsInFocusProcessing = PR_FALSE;

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
#ifdef DEBUG_IME_HANDLER
  NSLog(@"****in ConvertToTextRangeType = %d", aUnderlineStyle);
#endif
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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

PRBool
IMEInputHandler::DispatchTextEvent(const nsString& aText,
                                   NSAttributedString* aAttrString,
                                   NSRange& aSelectedRange,
                                   PRBool aDoCommit)
{
#ifdef DEBUG_IME_HANDLER
  NSLog(@"****in DispatchTextEvent; string = '%@'", aAttrString);
  NSLog(@" aSelectedRange = %d, %d",
        aSelectedRange.location, aSelectedRange.length);
#endif

  NS_ENSURE_TRUE(!Destroyed(), PR_FALSE);

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsTextEvent textEvent(PR_TRUE, NS_TEXT_TEXT, mWidget);
  textEvent.time = PR_IntervalNow();
  textEvent.theText = aText;
  nsAutoTArray<nsTextRange, 4> textRanges;
  if (!aDoCommit) {
    SetTextRangeList(textRanges, aAttrString, aSelectedRange);
  }
  textEvent.rangeArray = textRanges.Elements();
  textEvent.rangeCount = textRanges.Length();

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

#if DEBUG_IME
  NSLog(@"****in InsertTextAsCommittingComposition: '%@'", aAttrString);
  NSLog(@" mMarkedRange = %d, %d", mMarkedRange.location, mMarkedRange.length);
#endif

  if (Destroyed()) {
    return;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsString str;
  GetStringForNSString([aAttrString string], str);

  if (!IsIMEComposing()) {
    // XXXmnakano Probably, we shouldn't emulate composition in this case.
    // I think that we should just fire DOM3 textInput event if we implement it.
    nsCompositionEvent compStart(PR_TRUE, NS_COMPOSITION_START, mWidget);
    InitCompositionEvent(compStart);

    DispatchEvent(compStart);
    if (Destroyed()) {
      return;
    }

    OnStartIMEComposition();
  }

  if (IgnoreIMECommit()) {
    str.Truncate();
  }

  NSRange range = NSMakeRange(0, str.Length());
  DispatchTextEvent(str, aAttrString, range, PR_TRUE);
  if (Destroyed()) {
    return;
  }

  OnUpdateIMEComposition([aAttrString string]);

  nsCompositionEvent compEnd(PR_TRUE, NS_COMPOSITION_END, mWidget);
  InitCompositionEvent(compEnd);
  DispatchEvent(compEnd);
  if (Destroyed()) {
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

#if DEBUG_IME_HANDLER 
  NSLog(@"****in SetMarkedText location: %d, length: %d",
        aSelectedRange.location, aSelectedRange.length);
  NSLog(@" mMarkedRange = %d, %d", mMarkedRange.location, mMarkedRange.length);
  NSLog(@" aAttrString = '%@'", aAttrString);
#endif

  if (Destroyed() || IgnoreIMEComposition()) {
    return;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsString str;
  GetStringForNSString([aAttrString string], str);

  mMarkedRange.length = str.Length();

  if (!IsIMEComposing() && !str.IsEmpty()) {
    nsQueryContentEvent selection(PR_TRUE, NS_QUERY_SELECTED_TEXT,
                                  mWidget);
    DispatchEvent(selection);
    mMarkedRange.location = selection.mSucceeded ? selection.mReply.mOffset : 0;

    nsCompositionEvent compStart(PR_TRUE, NS_COMPOSITION_START, mWidget);
    InitCompositionEvent(compStart);

    DispatchEvent(compStart);
    if (Destroyed()) {
      return;
    }

    OnStartIMEComposition();
  }

  if (IsIMEComposing()) {
    OnUpdateIMEComposition([aAttrString string]);

    PRBool doCommit = str.IsEmpty();
    DispatchTextEvent(str, aAttrString, aSelectedRange, doCommit);
    if (Destroyed()) {
      return;
    }

    if (doCommit) {
      nsCompositionEvent compEnd(PR_TRUE, NS_COMPOSITION_END, mWidget);
      InitCompositionEvent(compEnd);
      DispatchEvent(compEnd);
      if (Destroyed()) {
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
  if (Destroyed()) {
    return reinterpret_cast<NSInteger>(mView);
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  // NOTE: The size of NSInteger is same as pointer size.
  nsQueryContentEvent textContent(PR_TRUE, NS_QUERY_TEXT_CONTENT, mWidget);
  textContent.InitForQueryTextContent(0, 0);
  DispatchEvent(textContent);
  if (!textContent.mSucceeded) {
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

#if DEBUG_IME_HANDLER
  NSLog(@"****in GetAttributedSubstringFromRange");
  NSLog(@" aRange      = %d, %d", aRange.location, aRange.length);
#endif

  if (Destroyed() || aRange.location == NSNotFound || aRange.length == 0) {
    return nil;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsAutoString str;
  nsQueryContentEvent textContent(PR_TRUE, NS_QUERY_TEXT_CONTENT, mWidget);
  textContent.InitForQueryTextContent(aRange.location, aRange.length);
  DispatchEvent(textContent);

  if (!textContent.mSucceeded || textContent.mReply.mString.IsEmpty()) {
    return nil;
  }

  NSString* nsstr = ToNSString(textContent.mReply.mString);
  NSAttributedString* result =
    [[[NSAttributedString alloc] initWithString:nsstr
                                     attributes:nil] autorelease];
  return result;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

NSRange
IMEInputHandler::SelectedRange()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

#if DEBUG_IME_HANDLER
  NSLog(@"****in SelectedRange");
#endif

  NSRange range = NSMakeRange(NSNotFound, 0);
  if (Destroyed()) {
    return range;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsQueryContentEvent selection(PR_TRUE, NS_QUERY_SELECTED_TEXT, mWidget);
  DispatchEvent(selection);
  if (!selection.mSucceeded) {
    return range;
  }

#if DEBUG_IME_HANDLER
  NSLog(@" result of SelectedRange = %d, %d",
        selection.mReply.mOffset, selection.mReply.mString.Length());
#endif
  return NSMakeRange(selection.mReply.mOffset,
                     selection.mReply.mString.Length());

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRange(0, 0));
}

NSRect
IMEInputHandler::FirstRectForCharacterRange(NSRange& aRange)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

#if DEBUG_IME_HANDLER
  NSLog(@"****in FirstRectForCharacterRange");
  NSLog(@" aRange      = %d, %d", aRange.location, aRange.length);
#endif

  // XXX this returns first character rect or caret rect, it is limitation of
  // now. We need more work for returns first line rect. But current
  // implementation is enough for IMEs.

  NSRect rect;
  if (Destroyed() || aRange.location == NSNotFound) {
    return rect;
  }

  nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  nsIntRect r;
  PRBool useCaretRect = (aRange.length == 0);
  if (!useCaretRect) {
    nsQueryContentEvent charRect(PR_TRUE, NS_QUERY_TEXT_RECT, mWidget);
    charRect.InitForQueryTextRect(aRange.location, 1);
    DispatchEvent(charRect);
    if (charRect.mSucceeded) {
      r = charRect.mReply.mRect;
    } else {
      useCaretRect = PR_TRUE;
    }
  }

  if (useCaretRect) {
    nsQueryContentEvent caretRect(PR_TRUE, NS_QUERY_CARET_RECT, mWidget);
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
  GeckoRectToNSRect(r, rect);
  rect = [rootView convertRect:rect toView:nil];
  rect.origin = [rootWindow convertBaseToScreen:rect.origin];
#if DEBUG_IME_HANDLER
  NSLog(@" result rect (x,y,w,h) = %f, %f, %f, %f",
        rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
#endif
  return rect;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRect(0.0, 0.0, 0.0, 0.0));
}

NSUInteger
IMEInputHandler::CharacterIndexForPoint(NSPoint& aPoint)
{
#if DEBUG_IME
  NSLog(@"****in CharacterIndexForPoint");
#endif

  //nsRefPtr<IMEInputHandler> kungFuDeathGrip(this);

  // To implement this, we'd have to grovel in text frames looking at text
  // offsets.
  return 0;
}

NSArray*
IMEInputHandler::GetValidAttributesForMarkedText()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

#if DEBUG_IME
  NSLog(@"****in GetValidAttributesForMarkedText");
#endif

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
  mIsIMEComposing(PR_FALSE), mIsIMEEnabled(PR_TRUE),
  mIsASCIICapableOnly(PR_FALSE), mIgnoreIMECommit(PR_FALSE),
  mIsInFocusProcessing(PR_FALSE)
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
IMEInputHandler::OnFocusChangeInGecko(PRBool aFocus)
{
#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"IMEInputHandler::OnFocusChangeInGecko");
  NSLog(@"  aFocus:%s sFocusedIMEHandler:%p",
        TrueOrFalse(aFocus), sFocusedIMEHandler);
#endif // DEBUG_IME_HANDLER
  // This is called when the native focus is changed and when the native focus
  // isn't changed but the focus is changed in Gecko.
  // XXX currently, we're not called this method with PR_FALSE, we need to
  // improve the nsIMEStateManager implementation.
  if (!aFocus) {
    if (sFocusedIMEHandler == this)
      sFocusedIMEHandler = nsnull;
    return;
  }

  sFocusedIMEHandler = this;
  mIsInFocusProcessing = PR_TRUE;

  // We need to reset the IME's window level by the current focused view of
  // Gecko.  It may be different from mView.  However, we cannot get the
  // new focused view here because the focus change process in Gecko hasn't
  // been finished yet.  So, we should post the job to the todo list.
  mPendingMethods |= kResetIMEWindowLevel;
  ResetTimer();
}

PRBool
IMEInputHandler::OnDestroyWidget(nsChildView* aDestroyingWidget)
{
  // If we're not focused, the focused IMEInputHandler may have been
  // created by another widget/nsChildView.
  if (sFocusedIMEHandler && sFocusedIMEHandler != this) {
    sFocusedIMEHandler->OnDestroyWidget(aDestroyingWidget);
  }

  if (!PluginTextInputHandler::OnDestroyWidget(aDestroyingWidget)) {
    return PR_FALSE;
  }

  if (IsIMEComposing()) {
    // If our view is in the composition, we should clean up it.
    CancelIMEComposition();
    OnEndIMEComposition();
  }

  return PR_TRUE;
}

void
IMEInputHandler::OnStartIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"IMEInputHandler::OnStartIMEComposition");
  NSLog(@"  mView=%p, currentInputManager:%p",
        mView, [NSInputManager currentInputManager]);
#endif // DEBUG_IME_HANDLER

  NS_ASSERTION(!mIsIMEComposing, "There is a composition already");
  mIsIMEComposing = PR_TRUE;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::OnUpdateIMEComposition(NSString* aIMECompositionString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"IMEInputHandler::OnUpdateIMEComposition");
  NSLog(@"  aIMECompositionString:%@ currentInputManager:%p",
        aIMECompositionString, [NSInputManager currentInputManager]);
#endif // DEBUG_IME_HANDLER

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

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"IMEInputHandler::OnEndIMEComposition");
  NSLog(@"  mIMECompositionString:%@ currentInputManager:%p",
        mIMECompositionString, [NSInputManager currentInputManager]);
#endif // DEBUG_IME_HANDLER

  NS_ASSERTION(mIsIMEComposing, "We're not in composition");

  mIsIMEComposing = PR_FALSE;

  if (mIMECompositionString) {
    [mIMECompositionString release];
    mIMECompositionString = nsnull;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
IMEInputHandler::SendCommittedText(NSString *aString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"IMEInputHandler::KillIMEComposition");
  NSLog(@"  currentInputManager:%p", [NSInputManager currentInputManager]);
#endif // DEBUG_IME_HANDLER

  if (Destroyed()) {
    return;
  }

  if (IsFocused()) {
    [[NSInputManager currentInputManager] markedTextAbandoned: mView];
    return;
  }

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"Pending IMEInputHandler::KillIMEComposition...");
#endif // DEBUG_IME_HANDLER

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

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"IMEInputHandler::CommitIMEComposition");
  NSLog(@"  mIMECompositionString:%@ currentInputManager:%p",
        mIMECompositionString, [NSInputManager currentInputManager]);
#endif // DEBUG_IME_HANDLER

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

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"IMEInputHandler::CancelIMEComposition");
  NSLog(@"  mIMECompositionString:%@ currentInputManager:%p",
        mIMECompositionString, [NSInputManager currentInputManager]);
#endif // DEBUG_IME_HANDLER

  // For canceling the current composing, we need to ignore the param of
  // insertText.  But this code is ugly...
  mIgnoreIMECommit = PR_TRUE;
  KillIMEComposition();
  mIgnoreIMECommit = PR_FALSE;

  if (!IsIMEComposing())
    return;

  // If the composition is still there, KillIMEComposition only kills the
  // composition in TSM.  We also need to kill the our composition too.
  SendCommittedText(@"");

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

PRBool
IMEInputHandler::IsFocused()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ENSURE_TRUE(!Destroyed(), PR_FALSE);
  NSWindow* window = [mView window];
  NS_ENSURE_TRUE(window, PR_FALSE);
  return [window firstResponder] == mView &&
         ([window isMainWindow] || [window isSheet]) &&
         [[NSApplication sharedApplication] isActive];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(PR_FALSE);
}

PRBool
IMEInputHandler::IsIMEOpened()
{
  TISInputSourceWrapper tis;
  tis.InitByCurrentInputSource();
  return tis.IsOpenedIMEMode();
}

void
IMEInputHandler::SetASCIICapableOnly(PRBool aASCIICapableOnly)
{
  if (aASCIICapableOnly == mIsASCIICapableOnly)
    return;

  CommitIMEComposition();
  mIsASCIICapableOnly = aASCIICapableOnly;
  SyncASCIICapableOnly();
}

void
IMEInputHandler::EnableIME(PRBool aEnableIME)
{
  if (aEnableIME == mIsIMEEnabled)
    return;

  CommitIMEComposition();
  mIsIMEEnabled = aEnableIME;
}

void
IMEInputHandler::SetIMEOpenState(PRBool aOpenIME)
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
  static PRBool sIsPrefferredIMESearched = PR_FALSE;
  if (sIsPrefferredIMESearched)
    return;
  sIsPrefferredIMESearched = PR_TRUE;
  OpenSystemPreferredLanguageIME();
}

void
IMEInputHandler::OpenSystemPreferredLanguageIME()
{
  CFArrayRef langList = ::CFLocaleCopyPreferredLanguages();
  if (!langList) {
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

    PRBool changed = PR_FALSE;
    CFStringRef lang = static_cast<CFStringRef>(
      ::CFLocaleGetValue(locale, kCFLocaleLanguageCode));
    NS_ASSERTION(lang, "lang is null");
    if (lang) {
      TISInputSourceWrapper tis;
      tis.InitByLanguage(lang);
      if (tis.IsOpenedIMEMode()) {
#ifdef DEBUG_IME_HANDLER
        CFStringRef foundTIS;
        tis.GetInputSourceID(foundTIS);
        NSLog(@"IMEInputHandler::OpenSystemPreferredLanguageIME");
        NSLog(@"  found Input Source: %@ by %@",
              (const NSString*)foundTIS, lang);
#endif // DEBUG_IME_HANDLER
        tis.Select();
        changed = PR_TRUE;
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
  TextInputHandlerBase(aWidget, aNativeView)
{
}

PluginTextInputHandler::~PluginTextInputHandler()
{
}

#ifndef NP_NO_CARBON

/* static */ PRBool
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
  NS_ENSURE_TRUE(err == noErr, PR_FALSE);

  err = ::CreateUnicodeToTextInfoByEncoding(systemEncoding, &converterInfo);
  NS_ENSURE_TRUE(err == noErr, PR_FALSE);

  err = ::ConvertFromUnicodeToPString(converterInfo, sizeof(PRUnichar),
                                      &aUniChar, convertedString);
  NS_ENSURE_TRUE(err == noErr, PR_FALSE);

  *aOutChar = convertedString[1];
  ::DisposeUnicodeToTextInfo(&converterInfo);
  return PR_TRUE;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(PR_FALSE);
}

/* static */ void
PluginTextInputHandler::ConvertCocoaKeyEventToCarbonEvent(
                          NSEvent* aCocoaKeyEvent,
                          EventRecord& aCarbonKeyEvent,
                          PRBool aMakeKeyDownEventIfNSFlagsChanged)
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

#endif // NP_NO_CARBON


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

PRBool
TextInputHandlerBase::OnDestroyWidget(nsChildView* aDestroyingWidget)
{
  if (aDestroyingWidget != mWidget) {
    return PR_FALSE;
  }

  mWidget = nsnull;
  return PR_TRUE;
}

PRBool
TextInputHandlerBase::DispatchEvent(nsGUIEvent& aEvent)
{
  if (aEvent.message == NS_KEY_PRESS) {
    nsInputEvent& inputEvent = static_cast<nsInputEvent&>(aEvent);
    if (!inputEvent.isMeta) {
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
    tis.InitByLayoutID(mKeyboardOverride.mKeyboardLayout, PR_TRUE);
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
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(sModifierFlagMap); ++i) {
    if (aModifierFlags & sModifierFlagMap[i][0]) {
      modifierFlags |= sModifierFlagMap[i][1];
    }
  }

  NSInteger windowNumber = [[mView window] windowNumber];
  PRBool sendFlagsChangedEvent = IsModifierKey(aNativeKeyCode);
  NSEventType eventType = sendFlagsChangedEvent ? NSFlagsChanged : NSKeyDown;
  NSEvent* downEvent =
    [NSEvent     keyEventWithType:eventType
                         location:NSMakePoint(0,0)
                    modifierFlags:modifierFlags
                        timestamp:0
                     windowNumber:windowNumber
                          context:[NSGraphicsContext currentContext]
                       characters:ToNSString(aCharacters)
      charactersIgnoringModifiers:ToNSString(aUnmodifiedCharacters)
                        isARepeat:NO
                          keyCode:aNativeKeyCode];

  NSEvent* upEvent =
    sendFlagsChangedEvent ? nil : MakeNewCocoaEventWithType(NSKeyUp, downEvent);

  if (downEvent && (sendFlagsChangedEvent || upEvent)) {
    KeyboardLayoutOverride currentLayout = mKeyboardOverride;
    mKeyboardOverride.mKeyboardLayout = aNativeKeyboardLayout;
    mKeyboardOverride.mOverrideEnabled = PR_TRUE;
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

/* static */ PRBool
TextInputHandlerBase::IsPrintableChar(PRUnichar aChar)
{
  return (aChar >= 0x20 && aChar <= 0x7E) || aChar >= 0xA0;
}

/* static */ PRUint32
TextInputHandlerBase::ComputeGeckoKeyCodeFromChar(PRUnichar aChar)
{
  // We don't support the key code for non-ASCII characters
  if (aChar > 0x7E) {
    return 0;
  }

  // lowercase
  if (aChar >= 'a' && aChar <= 'z') {
    return PRUint32(toupper(aChar));
  }
  // uppercase
  if (aChar >= 'A' && aChar <= 'Z') {
    return PRUint32(aChar);
  }
  // numeric
  if (aChar >= '0' && aChar <= '9') {
    return PRUint32(aChar - '0' + NS_VK_0);
  }

  switch (aChar) {
    case kReturnCharCode:
    case kEnterCharCode:
    case '\n':
      return NS_VK_RETURN;
    case '{':
    case '[':
      return NS_VK_OPEN_BRACKET;
    case '}':
    case ']':
      return NS_VK_CLOSE_BRACKET;
    case '\'':
    case '"':
      return NS_VK_QUOTE;

    case '\\':                  return NS_VK_BACK_SLASH;
    case ' ':                   return NS_VK_SPACE;
    case ';':                   return NS_VK_SEMICOLON;
    case '=':                   return NS_VK_EQUALS;
    case ',':                   return NS_VK_COMMA;
    case '.':                   return NS_VK_PERIOD;
    case '/':                   return NS_VK_SLASH;
    case '`':                   return NS_VK_BACK_QUOTE;
    case '\t':                  return NS_VK_TAB;
    case '-':                   return NS_VK_SUBTRACT;
    case '+':                   return NS_VK_ADD;

    default:
      if (!IsPrintableChar(aChar)) {
        NS_WARNING("ComputeGeckoKeyCodeFromChar() has failed.");
      }
      return 0;
  }
}

/* static */ PRUint32
TextInputHandlerBase::ComputeGeckoKeyCode(UInt32 aNativeKeyCode,
                                          NSString *aCharacters)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  switch (aNativeKeyCode) {
    // modifiers. We don't get separate events for these
    case kEscapeKeyCode:        return NS_VK_ESCAPE;
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

    default:
      // if we haven't gotten the key code already, look at the char code
      if ([aCharacters length] > 0) {
        return ComputeGeckoKeyCodeFromChar([aCharacters characterAtIndex:0]);
      }
  }

  return 0;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

/* static */ PRBool
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

    case kInsertKeyCode:
    case kDeleteKeyCode:
    case kTabKeyCode:
    case kBackspaceKeyCode:

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
      return PR_TRUE;
  }
  return PR_FALSE;
}

/* static */ PRBool
TextInputHandlerBase::IsNormalCharInputtingEvent(const nsKeyEvent& aKeyEvent)
{
  // this is not character inputting event, simply.
  if (!aKeyEvent.isChar || !aKeyEvent.charCode || aKeyEvent.isMeta) {
    return PR_FALSE;
  }
  // if this is unicode char inputting event, we don't need to check
  // ctrl/alt/command keys
  if (aKeyEvent.charCode > 0x7F) {
    return PR_TRUE;
  }
  // ASCII chars should be inputted without ctrl/alt/command keys
  return !aKeyEvent.isControl && !aKeyEvent.isAlt;
}

/* static */ PRBool
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
      return PR_TRUE;
  }
  return PR_FALSE;
}
