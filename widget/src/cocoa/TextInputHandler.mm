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

// static
PRBool
TISInputSourceWrapper::UCKeyTranslateToString(
                         const UCKeyboardLayout* aHandle,
                         UInt32 aKeyCode, UInt32 aModifiers,
                         UInt32 aKbType, nsAString &aStr)
{
#ifdef DEBUG_TEXT_INPUT_HANDLER
  NSLog(@"**** TISInputSourceWrapper::UCKeyTranslateToString: aHandle: %p, aKeyCode: %X, aModifiers: %X, aKbType: %X",
        aHandle, aKeyCode, aModifiers, aKbType);
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
  NS_PRECONDITION(aStr.IsEmpty(), "aStr isn't empty");
  UInt32 deadKeyState = 0;
  UniCharCount len;
  UniChar chars[5];
  OSStatus err = ::UCKeyTranslate(aHandle, aKeyCode,
                                  kUCKeyActionDown, aModifiers >> 8,
                                  aKbType, kUCKeyTranslateNoDeadKeysMask,
                                  &deadKeyState, 5, &len, chars);
  if (err != noErr) {
    return PR_FALSE;
  }
  if (len == 0) {
    return PR_TRUE;
  }
  if (!EnsureStringLength(aStr, len)) {
    return PR_FALSE;
  }
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
TISInputSourceWrapper::InitByLayoutID(SInt32 aLayoutID)
{
  Clear();
  // XXX On TIS, the LayoutID is abolished completely.  Therefore, the numbers
  // have no meaning.  If you need to add other keyboard layouts for
  // nsIWidget::SynthesizeNativeKeyEvent, you don't need to check the old
  // resource, you can use any numbers.
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
  }
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

PRBool
TISInputSourceWrapper::TranslateToString(UInt32 aKeyCode, UInt32 aModifiers,
                                         UInt32 aKbdType, nsAString &aStr)
{
  aStr.Truncate();
  const UCKeyboardLayout* UCKey = GetUCKeyboardLayout();
  NS_ENSURE_TRUE(UCKey, PR_FALSE);
  return UCKeyTranslateToString(UCKey, aKeyCode, aModifiers, aKbdType, aStr);
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

TextInputHandler::TextInputHandler() :
  IMEInputHandler()
{
}

TextInputHandler::~TextInputHandler()
{
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

  if (!mView)
    return;

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
  NSView<mozView>* editorView = mOwnerWidget->GetEditorView();
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

  if (!mView)
    return;

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

  if (!mView)
    return;

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

  nsTextEvent textEvent(PR_TRUE, NS_TEXT_TEXT, mOwnerWidget);
  textEvent.time = PR_IntervalNow();
  textEvent.theText = aText;
  nsAutoTArray<nsTextRange, 4> textRanges;
  if (!aDoCommit) {
    SetTextRangeList(textRanges, aAttrString, aSelectedRange);
  }
  textEvent.rangeArray = textRanges.Elements();
  textEvent.rangeCount = textRanges.Length();

  return mOwnerWidget->DispatchWindowEvent(textEvent);
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

  nsRefPtr<nsChildView> kungFuDeathGrip(mOwnerWidget);

  nsString str;
  GetStringForNSString([aAttrString string], str);

  if (!IsIMEComposing()) {
    // XXXmnakano Probably, we shouldn't emulate composition in this case.
    // I think that we should just fire DOM3 textInput event if we implement it.
    nsCompositionEvent compStart(PR_TRUE, NS_COMPOSITION_START, mOwnerWidget);
    InitCompositionEvent(compStart);

    mOwnerWidget->DispatchWindowEvent(compStart);
    if (!mView) {
      return; // we're destroyed
    }

    OnStartIMEComposition();
  }

  if (IgnoreIMECommit()) {
    str.Truncate();
  }

  NSRange range = NSMakeRange(0, str.Length());
  DispatchTextEvent(str, aAttrString, range, PR_TRUE);
  if (!mView) {
    return; // we're destroyed
  }

  OnUpdateIMEComposition([aAttrString string]);

  nsCompositionEvent compEnd(PR_TRUE, NS_COMPOSITION_END, mOwnerWidget);
  InitCompositionEvent(compEnd);
  mOwnerWidget->DispatchWindowEvent(compEnd);
  if (!mView) {
    return; // we're destroyed
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

  if (IgnoreIMEComposition()) {
    return;
  }

  nsRefPtr<nsChildView> kungFuDeathGrip(mOwnerWidget);

  nsString str;
  GetStringForNSString([aAttrString string], str);

  mMarkedRange.length = str.Length();

  if (!IsIMEComposing() && !str.IsEmpty()) {
    nsQueryContentEvent selection(PR_TRUE, NS_QUERY_SELECTED_TEXT,
                                  mOwnerWidget);
    mOwnerWidget->DispatchWindowEvent(selection);
    mMarkedRange.location = selection.mSucceeded ? selection.mReply.mOffset : 0;

    nsCompositionEvent compStart(PR_TRUE, NS_COMPOSITION_START, mOwnerWidget);
    InitCompositionEvent(compStart);

    mOwnerWidget->DispatchWindowEvent(compStart);
    if (!mView) {
      return; // we're destroyed
    }

    OnStartIMEComposition();
  }

  if (IsIMEComposing()) {
    OnUpdateIMEComposition([aAttrString string]);

    PRBool doCommit = str.IsEmpty();
    DispatchTextEvent(str, aAttrString, aSelectedRange, doCommit);
    if (!mView) {
      return; // we're destroyed
    }

    if (doCommit) {
      nsCompositionEvent compEnd(PR_TRUE, NS_COMPOSITION_END, mOwnerWidget);
      InitCompositionEvent(compEnd);
      mOwnerWidget->DispatchWindowEvent(compEnd);
      if (!mView) {
        return; // we're destroyed
      }
      OnEndIMEComposition();
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NSInteger
IMEInputHandler::ConversationIdentifier()
{
  // NOTE: The size of NSInteger is same as pointer size.
  nsQueryContentEvent textContent(PR_TRUE, NS_QUERY_TEXT_CONTENT, mOwnerWidget);
  textContent.InitForQueryTextContent(0, 0);
  mOwnerWidget->DispatchWindowEvent(textContent);
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

  if (aRange.location == NSNotFound || aRange.length == 0) {
    return nil;
  }

  nsAutoString str;
  nsQueryContentEvent textContent(PR_TRUE, NS_QUERY_TEXT_CONTENT, mOwnerWidget);
  textContent.InitForQueryTextContent(aRange.location, aRange.length);
  mOwnerWidget->DispatchWindowEvent(textContent);

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
  nsQueryContentEvent selection(PR_TRUE, NS_QUERY_SELECTED_TEXT, mOwnerWidget);
  mOwnerWidget->DispatchWindowEvent(selection);
  if (!selection.mSucceeded) {
    return NSMakeRange(NSNotFound, 0);
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
  if (aRange.location == NSNotFound) {
    return rect;
  }

  nsIntRect r;
  PRBool useCaretRect = (aRange.length == 0);
  if (!useCaretRect) {
    nsQueryContentEvent charRect(PR_TRUE, NS_QUERY_TEXT_RECT, mOwnerWidget);
    charRect.InitForQueryTextRect(aRange.location, 1);
    mOwnerWidget->DispatchWindowEvent(charRect);
    if (charRect.mSucceeded) {
      r = charRect.mReply.mRect;
    } else {
      useCaretRect = PR_TRUE;
    }
  }

  if (useCaretRect) {
    nsQueryContentEvent caretRect(PR_TRUE, NS_QUERY_CARET_RECT, mOwnerWidget);
    caretRect.InitForQueryCaretRect(aRange.location);
    mOwnerWidget->DispatchWindowEvent(caretRect);
    if (!caretRect.mSucceeded) {
      return rect;
    }
    r = caretRect.mReply.mRect;
    r.width = 0;
  }

  nsIWidget* rootWidget = mOwnerWidget->GetTopLevelWidget();
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

IMEInputHandler::IMEInputHandler() :
  PluginTextInputHandler(),
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
IMEInputHandler::OnDestroyView(NSView<mozView> *aDestroyingView)
{
  // If we're not focused, the destroying view might be composing with it in
  // another instance.
  if (sFocusedIMEHandler && sFocusedIMEHandler != this) {
    sFocusedIMEHandler->OnDestroyView(aDestroyingView);
  }

  if (!PluginTextInputHandler::OnDestroyView(aDestroyingView)) {
    return PR_FALSE;
  }

  if (IsIMEComposing()) {
    // If our view is in the composition, we should clean up it.
    // XXX Might CancelIMEComposition() fail because mView is being destroyed?
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

  NS_ASSERTION(mView, "mView is null");

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

  if (!mView)
    return;

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

  NS_ENSURE_TRUE(mView, PR_FALSE);
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

PluginTextInputHandler::PluginTextInputHandler() :
  TextInputHandlerBase()
{
}

PluginTextInputHandler::~PluginTextInputHandler()
{
}


#pragma mark -


/******************************************************************************
 *
 *  TextInputHandlerBase implementation
 *
 ******************************************************************************/

TextInputHandlerBase::TextInputHandlerBase() :
  mOwnerWidget(nsnull), mView(nsnull)
{
  gHandlerInstanceCount++;
}

TextInputHandlerBase::~TextInputHandlerBase()
{
  if (--gHandlerInstanceCount == 0) {
    FinalizeCurrentKeyboardLayout();
  }
}

void
TextInputHandlerBase::Init(nsChildView* aOwner)
{
  mOwnerWidget = aOwner;
  mView =
    static_cast<NSView<mozView>*>(aOwner->GetNativeData(NS_NATIVE_WIDGET));
}

PRBool
TextInputHandlerBase::OnDestroyView(NSView<mozView> *aDestroyingView)
{
  if (aDestroyingView != mView) {
    return PR_FALSE;
  }

  mView = nsnull;

  return PR_TRUE;
}
