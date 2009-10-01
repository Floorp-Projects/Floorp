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

#include "nsCocoaTextInputHandler.h"

#ifdef NS_LEOPARD_AND_LATER

#include "nsChildView.h"
#include "nsObjCExceptions.h"

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif
#include "prlog.h"

#undef DEBUG_IME_HANDLER
#undef DEBUG_TEXT_INPUT_HANDLER
//#define DEBUG_IME_HANDLER 1
//#define DEBUG_TEXT_INPUT_HANDLER 1

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

#ifdef DEBUG_IME_HANDLER

static const char*
TrueOrFalse(PRBool aBool)
{
  return aBool ? "TRUE" : "FALSE";
}

static void
DebugPrintPointer(nsCocoaIMEHandler* aHandler)
{
  static nsCocoaIMEHandler* sLastHandler = nsnull;
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


#pragma mark -


/******************************************************************************
 *
 *  nsTISInputSource implementation
 *
 ******************************************************************************/

void
nsTISInputSource::InitByInputSourceID(const char* aID)
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
nsTISInputSource::InitByInputSourceID(const nsAFlatString &aID)
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
nsTISInputSource::InitByInputSourceID(const CFStringRef aID)
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
nsTISInputSource::InitByLayoutID(SInt32 aLayoutID)
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
nsTISInputSource::InitByCurrentInputSource()
{
  Clear();
  mInputSource = ::TISCopyCurrentKeyboardInputSource();
}

void
nsTISInputSource::InitByCurrentKeyboardLayout()
{
  Clear();
  mInputSource = ::TISCopyCurrentKeyboardLayoutInputSource();
}

void
nsTISInputSource::InitByCurrentASCIICapableInputSource()
{
  Clear();
  mInputSource = ::TISCopyCurrentASCIICapableKeyboardInputSource();
}

void
nsTISInputSource::InitByCurrentASCIICapableKeyboardLayout()
{
  Clear();
  mInputSource = ::TISCopyCurrentASCIICapableKeyboardLayoutInputSource();
}

void
nsTISInputSource::InitByTISInputSourceRef(TISInputSourceRef aInputSource)
{
  Clear();
  mInputSource = aInputSource;
}

void
nsTISInputSource::InitByLanguage(CFStringRef aLanguage)
{
  Clear();
  mInputSource = ::TISCopyInputSourceForLanguage(aLanguage);
}

const UCKeyboardLayout*
nsTISInputSource::GetUCKeyboardLayout()
{
  NS_ENSURE_TRUE(mInputSource, nsnull);
  CFDataRef uchr = static_cast<CFDataRef>(
    ::TISGetInputSourceProperty(mInputSource,
                                kTISPropertyUnicodeKeyLayoutData));

  // We should be always able to get the layout here.
  NS_ENSURE_TRUE(uchr, nsnull);
  return reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(uchr));
}

PRBool
nsTISInputSource::GetBoolProperty(const CFStringRef aKey)
{
  CFBooleanRef ret = static_cast<CFBooleanRef>(
    ::TISGetInputSourceProperty(mInputSource, aKey));
  return ::CFBooleanGetValue(ret);
}

PRBool
nsTISInputSource::GetStringProperty(const CFStringRef aKey, CFStringRef &aStr)
{
  aStr = static_cast<CFStringRef>(
    ::TISGetInputSourceProperty(mInputSource, aKey));
  return aStr != nsnull;
}

PRBool
nsTISInputSource::GetStringProperty(const CFStringRef aKey, nsAString &aStr)
{
  CFStringRef str;
  GetStringProperty(aKey, str);
  GetStringForNSString((const NSString*)str, aStr);
  return !aStr.IsEmpty();
}

PRBool
nsTISInputSource::IsOpenedIMEMode()
{
  NS_ENSURE_TRUE(mInputSource, PR_FALSE);
  if (!IsIMEMode())
    return PR_FALSE;
  return !IsASCIICapable();
}

PRBool
nsTISInputSource::IsIMEMode()
{
  NS_ENSURE_TRUE(mInputSource, PR_FALSE);
  CFStringRef str;
  GetInputSourceType(str);
  NS_ENSURE_TRUE(str, PR_FALSE);
  return ::CFStringCompare(kTISTypeKeyboardInputMode,
                           str, 0) == kCFCompareEqualTo;
}

PRBool
nsTISInputSource::GetLanguageList(CFArrayRef &aLanguageList)
{
  NS_ENSURE_TRUE(mInputSource, PR_FALSE);
  aLanguageList = static_cast<CFArrayRef>(
    ::TISGetInputSourceProperty(mInputSource,
                                kTISPropertyInputSourceLanguages));
  return aLanguageList != nsnull;
}

PRBool
nsTISInputSource::GetPrimaryLanguage(CFStringRef &aPrimaryLanguage)
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
nsTISInputSource::GetPrimaryLanguage(nsAString &aPrimaryLanguage)
{
  NS_ENSURE_TRUE(mInputSource, PR_FALSE);
  CFStringRef primaryLanguage;
  NS_ENSURE_TRUE(GetPrimaryLanguage(primaryLanguage), PR_FALSE);
  GetStringForNSString((const NSString*)primaryLanguage, aPrimaryLanguage);
  return !aPrimaryLanguage.IsEmpty();
}

void
nsTISInputSource::Select()
{
  if (!mInputSource)
    return;
  ::TISSelectInputSource(mInputSource);
}

void
nsTISInputSource::Clear()
{
  if (mInputSourceList) {
    ::CFRelease(mInputSourceList);
  }
  mInputSourceList = nsnull;
  mInputSource = nsnull;
}


#pragma mark -


/******************************************************************************
 *
 *  nsCocoaTextInputHandler implementation (static methods)
 *
 ******************************************************************************/

// static
CFArrayRef
nsCocoaTextInputHandler::CreateAllKeyboardLayoutList()
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
nsCocoaTextInputHandler::DebugPrintAllKeyboardLayouts(
                           PRLogModuleInfo* aLogModuleInfo)
{
  CFArrayRef list = CreateAllKeyboardLayoutList();
  PR_LOG(aLogModuleInfo, PR_LOG_ALWAYS, ("Keyboard layout configuration:"));
  CFIndex idx = ::CFArrayGetCount(list);
  nsTISInputSource tis;
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
 *  nsCocoaTextInputHandler implementation
 *
 ******************************************************************************/

nsCocoaTextInputHandler::nsCocoaTextInputHandler() :
  nsCocoaIMEHandler()
{
}

nsCocoaTextInputHandler::~nsCocoaTextInputHandler()
{
}


#pragma mark -


/******************************************************************************
 *
 *  nsCocoaIMEHandler implementation (static methods)
 *
 ******************************************************************************/

PRBool nsCocoaIMEHandler::sStaticMembersInitialized = PR_FALSE;
CFStringRef nsCocoaIMEHandler::sLatestIMEOpenedModeInputSourceID = nsnull;
nsCocoaIMEHandler* nsCocoaIMEHandler::sFocusedIMEHandler = nsnull;

// static
void
nsCocoaIMEHandler::InitStaticMembers()
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
nsCocoaIMEHandler::OnCurrentTextInputSourceChange(
                     CFNotificationCenterRef aCenter,
                     void* aObserver,
                     CFStringRef aName,
                     const void* aObject,
                     CFDictionaryRef aUserInfo)
{
  // Cache the latest IME opened mode to sLatestIMEOpenedModeInputSourceID.
  nsTISInputSource tis;
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
    nsTISInputSource tis1, tis2, tis3;
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
nsCocoaIMEHandler::FlushPendingMethods(nsITimer* aTimer, void* aClosure)
{
  NS_ASSERTION(aClosure, "aClosure is null");
  static_cast<nsCocoaIMEHandler*>(aClosure)->ExecutePendingMethods();
}

// static
CFArrayRef
nsCocoaIMEHandler::CreateAllIMEModeList()
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
nsCocoaIMEHandler::DebugPrintAllIMEModes(PRLogModuleInfo* aLogModuleInfo)
{
  CFArrayRef list = CreateAllIMEModeList();
  PR_LOG(aLogModuleInfo, PR_LOG_ALWAYS, ("IME mode configuration:"));
  CFIndex idx = ::CFArrayGetCount(list);
  nsTISInputSource tis;
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


#pragma mark -


/******************************************************************************
 *
 *  nsCocoaIMEHandler implementation #1
 *    The methods are releated to the pending methods.  Some jobs should be
 *    run after the stack is finished, e.g, some methods cannot run the jobs
 *    during processing the focus event.  And also some other jobs should be
 *    run at the next focus event is processed.
 *    The pending methods are recorded in mPendingMethods.  They are executed
 *    by ExecutePendingMethods via FlushPendingMethods.
 *
 ******************************************************************************/

void
nsCocoaIMEHandler::ResetIMEWindowLevel()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"nsCocoaIMEHandler::ResetIMEWindowLevel");
  NSLog(@"  IsFocused:%s ::TSMGetActiveDocument():%p",
        TrueOrFalse(IsFocused()), ::TSMGetActiveDocument());
#endif // DEBUG_IME_HANDLER

  if (!mView)
    return;

  if (!IsFocused()) {
    // retry at next focus event
    mPendingMethods |= kResetIMEWindowLevel;
    return;
  }

  TSMDocumentID doc = ::TSMGetActiveDocument();
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

  ::TSMSetDocumentProperty(::TSMGetActiveDocument(),
                           kTSMDocumentWindowLevelPropertyTag,
                           sizeof(windowLevel), &windowLevel);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsCocoaIMEHandler::DiscardIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"nsCocoaIMEHandler::DiscardIMEComposition");
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
nsCocoaIMEHandler::SyncASCIICapableOnly()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"nsCocoaIMEHandler::SyncASCIICapableOnly");
  NSLog(@"  IsFocused:%s ::TSMGetActiveDocument():%p",
        TrueOrFalse(IsFocused()), ::TSMGetActiveDocument());
#endif

  if (!mView)
    return;

  if (!IsFocused()) {
    // retry at next focus event
    mPendingMethods |= kSyncASCIICapableOnly;
    return;
  }

  TSMDocumentID doc = ::TSMGetActiveDocument();
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
nsCocoaIMEHandler::ResetTimer()
{
  NS_ASSERTION(mPendingMethods != 0,
               "There are not pending methods, why this is called?");
  if (mTimer) {
    mTimer->Cancel();
    return;
  }
  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (!mTimer) {
    NS_ERROR("mTimer is null");
    return;
  }
  mTimer->InitWithFuncCallback(FlushPendingMethods, this, 0,
                               nsITimer::TYPE_ONE_SHOT);
}

void
nsCocoaIMEHandler::ExecutePendingMethods()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (![[NSApplication sharedApplication] isActive]) {
    mIsInFocusProcessing = PR_FALSE;
    // If we're not active, we should retry at focus event
    return;
  }

  PRUint32 pendingMethods = mPendingMethods;
  // First, reset the pending method flags because if each methods cannot
  // run now, they can reentry to the pending flags by theirselves.
  mPendingMethods = 0;
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }

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
 *  nsCocoaIMEHandler implementation #2
 *
 ******************************************************************************/

nsCocoaIMEHandler::nsCocoaIMEHandler() :
  mOwnerWidget(nsnull), mView(nsnull),
  mPendingMethods(0), mIMECompositionString(nsnull),
  mIsIMEComposing(PR_FALSE), mIsIMEEnabled(PR_TRUE),
  mIsASCIICapableOnly(PR_FALSE), mIgnoreIMECommit(PR_FALSE),
  mIsInFocusProcessing(PR_FALSE)
{
  InitStaticMembers();
}

nsCocoaIMEHandler::~nsCocoaIMEHandler()
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
nsCocoaIMEHandler::Init(nsChildView* aOwner)
{
  mOwnerWidget = aOwner;
  mView =
    static_cast<NSView<mozView>*>(aOwner->GetNativeData(NS_NATIVE_WIDGET));
}

void
nsCocoaIMEHandler::OnFocusChangeInGecko(PRBool aFocus)
{
#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"nsCocoaIMEHandler::OnFocusChangeInGecko");
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

void
nsCocoaIMEHandler::OnDestroyView(NSView<mozView> *aDestroyingView)
{
  // If we're not focused, the destroying view might be composing with it in
  // another instance.
  if (sFocusedIMEHandler && sFocusedIMEHandler != this) {
    sFocusedIMEHandler->OnDestroyView(aDestroyingView);
  }

  if (aDestroyingView != mView) {
    return;
  }

  if (IsIMEComposing()) {
    // If our view is in the composition, we should clean up it.
    // XXX Might CancelIMEComposition() fail because mView is being destroyed?
    CancelIMEComposition();
    OnEndIMEComposition();
  }

  mView = nsnull;
}

void
nsCocoaIMEHandler::OnStartIMEComposition(NSView<mozView> *aView)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"nsCocoaIMEHandler::OnStartIMEComposition");
  NSLog(@"  aView:%p mView:%p currentInputManager:%p",
        aView, mView, [NSInputManager currentInputManager]);
#endif // DEBUG_IME_HANDLER

  NS_ASSERTION(!mIsIMEComposing, "There is a composition already");
  NS_ASSERTION(aView == mView, "The composition is started on another view");
  mIsIMEComposing = PR_TRUE;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsCocoaIMEHandler::OnUpdateIMEComposition(NSString* aIMECompositionString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"nsCocoaIMEHandler::OnUpdateIMEComposition");
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
nsCocoaIMEHandler::OnEndIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"nsCocoaIMEHandler::OnEndIMEComposition");
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
nsCocoaIMEHandler::SendCommittedText(NSString *aString)
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
nsCocoaIMEHandler::KillIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"nsCocoaIMEHandler::KillIMEComposition");
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
  NSLog(@"Pending nsCocoaIMEHandler::KillIMEComposition...");
#endif // DEBUG_IME_HANDLER

  // Commit the composition internally.
  SendCommittedText(mIMECompositionString);
  NS_ASSERTION(!mIsIMEComposing, "We're still in a composition");
  // The pending method will be fired by the next focus event.
  mPendingMethods |= kDiscardIMEComposition;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsCocoaIMEHandler::CommitIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!IsIMEComposing())
    return;

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"nsCocoaIMEHandler::CommitIMEComposition");
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
nsCocoaIMEHandler::CancelIMEComposition()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!IsIMEComposing())
    return;

#ifdef DEBUG_IME_HANDLER
  DebugPrintPointer(this);
  NSLog(@"nsCocoaIMEHandler::CancelIMEComposition");
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
nsCocoaIMEHandler::IsFocused()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ENSURE_TRUE(mView, PR_FALSE);
  NSWindow* window = [mView window];
  NS_ENSURE_TRUE(window, PR_FALSE);
  return [window firstResponder] == mView && [window isMainWindow] &&
         [[NSApplication sharedApplication] isActive];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(PR_FALSE);
}

PRBool
nsCocoaIMEHandler::IsIMEOpened()
{
  nsTISInputSource tis;
  tis.InitByCurrentInputSource();
  return tis.IsOpenedIMEMode();
}

void
nsCocoaIMEHandler::SetASCIICapableOnly(PRBool aASCIICapableOnly)
{
  if (aASCIICapableOnly == mIsASCIICapableOnly)
    return;

  CommitIMEComposition();
  mIsASCIICapableOnly = aASCIICapableOnly;
  SyncASCIICapableOnly();
}

void
nsCocoaIMEHandler::EnableIME(PRBool aEnableIME)
{
  if (aEnableIME == mIsIMEEnabled)
    return;

  CommitIMEComposition();
  mIsIMEEnabled = aEnableIME;
}

void
nsCocoaIMEHandler::SetIMEOpenState(PRBool aOpenIME)
{
  if (!IsFocused() || IsIMEOpened() == aOpenIME)
    return;

  if (!aOpenIME) {
    nsTISInputSource tis;
    tis.InitByCurrentASCIICapableInputSource();
    tis.Select();
    return;
  }

  // If we know the latest IME opened mode, we should select it.
  if (sLatestIMEOpenedModeInputSourceID) {
    nsTISInputSource tis;
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
nsCocoaIMEHandler::OpenSystemPreferredLanguageIME()
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
      nsTISInputSource tis;
      tis.InitByLanguage(lang);
      if (tis.IsOpenedIMEMode()) {
#ifdef DEBUG_IME_HANDLER
        CFStringRef foundTIS;
        tis.GetInputSourceID(foundTIS);
        NSLog(@"nsCocoaIMEHandler::OpenSystemPreferredLanguageIME");
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

#endif // NS_LEOPARD_AND_LATER
