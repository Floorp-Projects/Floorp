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

#ifndef TextInputHandler_h_
#define TextInputHandler_h_

#include "nsCocoaUtils.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#include "mozView.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "npapi.h"
#include "nsTArray.h"
#include "nsEvent.h"

class nsChildView;
struct nsTextRange;

namespace mozilla {
namespace widget {

// Key code constants
enum
{
  kEscapeKeyCode      = 0x35,
  kRCommandKeyCode    = 0x36, // right command key
  kCommandKeyCode     = 0x37,
  kShiftKeyCode       = 0x38,
  kCapsLockKeyCode    = 0x39,
  kOptionkeyCode      = 0x3A,
  kControlKeyCode     = 0x3B,
  kRShiftKeyCode      = 0x3C, // right shift key
  kROptionKeyCode     = 0x3D, // right option key
  kRControlKeyCode    = 0x3E, // right control key
  kClearKeyCode       = 0x47,

  // function keys
  kF1KeyCode          = 0x7A,
  kF2KeyCode          = 0x78,
  kF3KeyCode          = 0x63,
  kF4KeyCode          = 0x76,
  kF5KeyCode          = 0x60,
  kF6KeyCode          = 0x61,
  kF7KeyCode          = 0x62,
  kF8KeyCode          = 0x64,
  kF9KeyCode          = 0x65,
  kF10KeyCode         = 0x6D,
  kF11KeyCode         = 0x67,
  kF12KeyCode         = 0x6F,
  kF13KeyCode         = 0x69,
  kF14KeyCode         = 0x6B,
  kF15KeyCode         = 0x71,

  kPrintScreenKeyCode = kF13KeyCode,
  kScrollLockKeyCode  = kF14KeyCode,
  kPauseKeyCode       = kF15KeyCode,

  // keypad
  kKeypad0KeyCode     = 0x52,
  kKeypad1KeyCode     = 0x53,
  kKeypad2KeyCode     = 0x54,
  kKeypad3KeyCode     = 0x55,
  kKeypad4KeyCode     = 0x56,
  kKeypad5KeyCode     = 0x57,
  kKeypad6KeyCode     = 0x58,
  kKeypad7KeyCode     = 0x59,
  kKeypad8KeyCode     = 0x5B,
  kKeypad9KeyCode     = 0x5C,

  kKeypadMultiplyKeyCode  = 0x43,
  kKeypadAddKeyCode       = 0x45,
  kKeypadSubtractKeyCode  = 0x4E,
  kKeypadDecimalKeyCode   = 0x41,
  kKeypadDivideKeyCode    = 0x4B,
  kKeypadEqualsKeyCode    = 0x51, // no correpsonding gecko key code
  kEnterKeyCode           = 0x4C,
  kReturnKeyCode          = 0x24,
  kPowerbookEnterKeyCode  = 0x34, // Enter on Powerbook's keyboard is different

  kInsertKeyCode          = 0x72, // also help key
  kDeleteKeyCode          = 0x75, // also forward delete key
  kTabKeyCode             = 0x30,
  kTildeKeyCode           = 0x32,
  kBackspaceKeyCode       = 0x33,
  kHomeKeyCode            = 0x73, 
  kEndKeyCode             = 0x77,
  kPageUpKeyCode          = 0x74,
  kPageDownKeyCode        = 0x79,
  kLeftArrowKeyCode       = 0x7B,
  kRightArrowKeyCode      = 0x7C,
  kUpArrowKeyCode         = 0x7E,
  kDownArrowKeyCode       = 0x7D
};

/**
 * TISInputSourceWrapper is a wrapper for the TISInputSourceRef.  If we get the
 * TISInputSourceRef from InputSourceID, we need to release the CFArray instance
 * which is returned by TISCreateInputSourceList.  However, when we release the
 * list, we cannot access the TISInputSourceRef.  So, it's not usable, and it
 * may cause the memory leak bugs.  nsTISInputSource automatically releases the
 * list when the instance is destroyed.
 */
class TISInputSourceWrapper
{
public:
  static TISInputSourceWrapper& CurrentKeyboardLayout();

  TISInputSourceWrapper()
  {
    mInputSourceList = nsnull;
    Clear();
  }

  TISInputSourceWrapper(const char* aID)
  {
    mInputSourceList = nsnull;
    InitByInputSourceID(aID);
  }

  TISInputSourceWrapper(SInt32 aLayoutID)
  {
    mInputSourceList = nsnull;
    InitByLayoutID(aLayoutID);
  }

  TISInputSourceWrapper(TISInputSourceRef aInputSource)
  {
    mInputSourceList = nsnull;
    InitByTISInputSourceRef(aInputSource);
  }

  ~TISInputSourceWrapper() { Clear(); }

  void InitByInputSourceID(const char* aID);
  void InitByInputSourceID(const nsAFlatString &aID);
  void InitByInputSourceID(const CFStringRef aID);
  /**
   * InitByLayoutID() initializes the keyboard layout by the layout ID.
   * The KeyboardLayoutIdentifier (SInt32), used by Apple's now-deprecated
   * Keyboard Layout Services, is no longer used by its replacement --
   * Apple's Text Input Services (TIS).  All the layout IDs currently
   * supported by InitByLayoutID() are backwards-compatible with the layout
   * IDs used by Keyboard Layout Services.  But there's no need to contine
   * maintaining backwards compatibility as support for new IDs is added.
   *
   * @param aLayoutID             An ID of keyboard layout.
   *                                     0: US
   *                                -18944: Greek
   *                                     3: German
   *                                   224: Swedish-Pro
   * @param aOverrideKeyboard     When testing set to TRUE, otherwise, set to
   *                              FALSE.  When TRUE, we use an ANSI keyboard
   *                              instead of the actual keyboard.
   */
  void InitByLayoutID(SInt32 aLayoutID, bool aOverrideKeyboard = false);
  void InitByCurrentInputSource();
  void InitByCurrentKeyboardLayout();
  void InitByCurrentASCIICapableInputSource();
  void InitByCurrentASCIICapableKeyboardLayout();
  void InitByTISInputSourceRef(TISInputSourceRef aInputSource);
  void InitByLanguage(CFStringRef aLanguage);

  const UCKeyboardLayout* GetUCKeyboardLayout();

  bool IsOpenedIMEMode();
  bool IsIMEMode();

  bool IsASCIICapable()
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetBoolProperty(kTISPropertyInputSourceIsASCIICapable);
  }

  bool IsEnabled()
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetBoolProperty(kTISPropertyInputSourceIsEnabled);
  }

  bool GetLanguageList(CFArrayRef &aLanguageList);
  bool GetPrimaryLanguage(CFStringRef &aPrimaryLanguage);
  bool GetPrimaryLanguage(nsAString &aPrimaryLanguage);

  bool GetLocalizedName(CFStringRef &aName)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyLocalizedName, aName);
  }

  bool GetLocalizedName(nsAString &aName)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyLocalizedName, aName);
  }

  bool GetInputSourceID(CFStringRef &aID)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyInputSourceID, aID);
  }

  bool GetInputSourceID(nsAString &aID)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyInputSourceID, aID);
  }

  bool GetBundleID(CFStringRef &aBundleID)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyBundleID, aBundleID);
  }

  bool GetBundleID(nsAString &aBundleID)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyBundleID, aBundleID);
  }

  bool GetInputSourceType(CFStringRef &aType)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyInputSourceType, aType);
  }

  bool GetInputSourceType(nsAString &aType)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyInputSourceType, aType);
  }

  bool IsForRTLLanguage();
  bool IsInitializedByCurrentKeyboardLayout();

  enum {
    // 40 is an actual result of the ::LMGetKbdType() when we connect an
    // unknown keyboard and set the keyboard type to ANSI manually on the
    // set up dialog.
    eKbdType_ANSI = 40
  };

  void Select();
  void Clear();

  /**
   * InitKeyEvent() initializes aKeyEvent for aNativeKeyEvent.
   *
   * @param aNativeKeyEvent       A native key event for which you want to
   *                              dispatch a Gecko key event.
   * @param aKeyEvent             The result -- a Gecko key event initialized
   *                              from the native key event.
   */
  void InitKeyEvent(NSEvent *aNativeKeyEvent, nsKeyEvent& aKeyEvent);

protected:
  /**
   * TranslateToString() computes the inputted text from the native keyCode,
   * modifier flags and keyboard type.
   *
   * @param aKeyCode              A native keyCode.
   * @param aModifiers            Combination of native modifier flags.
   * @param aKbType               A native Keyboard Type value.  Typically,
   *                              this is a result of ::LMGetKbdType().
   * @param aStr                  Result, i.e., inputted text.
   *                              The result can be two or more characters.
   * @return                      If succeeded, TRUE.  Otherwise, FALSE.
   *                              Even if TRUE, aStr can be empty string.
   */
  bool TranslateToString(UInt32 aKeyCode, UInt32 aModifiers,
                           UInt32 aKbType, nsAString &aStr);

  /**
   * TranslateToChar() computes the inputted character from the native keyCode,
   * modifier flags and keyboard type.  If two or more characters would be
   * input, this returns 0.
   *
   * @param aKeyCode              A native keyCode.
   * @param aModifiers            Combination of native modifier flags.
   * @param aKbType               A native Keyboard Type value.  Typically,
   *                              this is a result of ::LMGetKbdType().
   * @return                      If succeeded and the result is one character,
   *                              returns the charCode of it.  Otherwise,
   *                              returns 0.
   */
  PRUint32 TranslateToChar(UInt32 aKeyCode, UInt32 aModifiers, UInt32 aKbdType);

  /**
   * InitKeyPressEvent() initializes aKeyEvent for aNativeKeyEvent.
   * Don't call this method when aKeyEvent isn't NS_KEY_PRESS.
   *
   * @param aNativeKeyEvent       A native key event for which you want to
   *                              dispatch a Gecko key event.
   * @param aKeyEvent             The result -- a Gecko key event initialized
   *                              from the native key event.  This must be
   *                              NS_KEY_PRESS event.
   */
  void InitKeyPressEvent(NSEvent *aNativeKeyEvent, nsKeyEvent& aKeyEvent);

  bool GetBoolProperty(const CFStringRef aKey);
  bool GetStringProperty(const CFStringRef aKey, CFStringRef &aStr);
  bool GetStringProperty(const CFStringRef aKey, nsAString &aStr);

  TISInputSourceRef mInputSource;
  CFArrayRef mInputSourceList;
  const UCKeyboardLayout* mUCKeyboardLayout;
  PRInt8 mIsRTL;

  bool mOverrideKeyboard;
};

/**
 * TextInputHandlerBase is a base class of PluginTextInputHandler,
 * IMEInputHandler and TextInputHandler.  Utility methods should be implemented
 * this level.
 */

class TextInputHandlerBase
{
public:
  nsrefcnt AddRef()
  {
    NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "mRefCnt is negative");
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "TextInputHandlerBase", sizeof(*this));
    return mRefCnt;
  }
  nsrefcnt Release()
  {
    NS_PRECONDITION(mRefCnt != 0, "mRefCnt is alrady zero");
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "TextInputHandlerBase");
    if (mRefCnt == 0) {
        mRefCnt = 1; /* stabilize */
        delete this;
        return 0;
    }
    return mRefCnt;
  }

  /**
   * DispatchEvent() dispatches aEvent on mWidget.
   *
   * @param aEvent                An event which you want to dispatch.
   * @return                      TRUE if the event is consumed by web contents
   *                              or chrome contents.  Otherwise, FALSE.
   */
  bool DispatchEvent(nsGUIEvent& aEvent);

  /**
   * InitKeyEvent() initializes aKeyEvent for aNativeKeyEvent.
   *
   * @param aNativeKeyEvent       A native key event for which you want to
   *                              dispatch a Gecko key event.
   * @param aKeyEvent             The result -- a Gecko key event initialized
   *                              from the native key event.
   */
  void InitKeyEvent(NSEvent *aNativeKeyEvent, nsKeyEvent& aKeyEvent);

  /**
   * SynthesizeNativeKeyEvent() is an implementation of
   * nsIWidget::SynthesizeNativeKeyEvent().  See the document in nsIWidget.h
   * for the detail.
   */
  nsresult SynthesizeNativeKeyEvent(PRInt32 aNativeKeyboardLayout,
                                    PRInt32 aNativeKeyCode,
                                    PRUint32 aModifierFlags,
                                    const nsAString& aCharacters,
                                    const nsAString& aUnmodifiedCharacters);

  /**
   * ComputeGeckoKeyCode() computes Gecko defined keyCode from the native
   * keyCode or the characters.
   *
   * @param aNativeKeyCode        A native keyCode.
   * @param aCharacters           Characters from the native key event (obtained
   *                              using charactersIgnoringModifiers).  If the
   *                              native event contains one or more characters,
   *                              the result is computed from this.
   * @return                      Gecko keyCode value for aNativeKeyCode (if
   *                              aCharacters is empty), otherwise for
   *                              aCharacters (if aCharacters is non-empty).
   *                              Or zero if the aCharacters contains one or
   *                              more Unicode characters, or if aNativeKeyCode
   *                              cannot be mapped to a Gecko keyCode.
   */
  static PRUint32 ComputeGeckoKeyCode(UInt32 aNativeKeyCode,
                                      NSString *aCharacters);

  /**
   * IsSpecialGeckoKey() checks whether aNativeKeyCode is mapped to a special
   * Gecko keyCode.  A key is "special" if it isn't used for text input.
   *
   * @param aNativeKeyCode        A native keycode.
   * @return                      If the keycode is mapped to a special key,
   *                              TRUE.  Otherwise, FALSE.
   */
  static bool IsSpecialGeckoKey(UInt32 aNativeKeyCode);

protected:
  nsAutoRefCnt mRefCnt;

public:
   /**
   * mWidget must not be destroyed without OnDestroyWidget being called.
   *
   * @param aDestroyingWidget     Destroying widget.  This might not be mWidget.
   * @return                      This result doesn't have any meaning for
   *                              callers.  When aDstroyingWidget isn't the same
   *                              as mWidget, FALSE.  Then, inherited methods in
   *                              sub classes should return from this method
   *                              without cleaning up.
   */
  virtual bool OnDestroyWidget(nsChildView* aDestroyingWidget);

protected:
  // The creater of this instance and client.
  // This must not be null after initialized until OnDestroyWidget() is called.
  nsChildView* mWidget; // [WEAK]

  // The native view for mWidget.
  // This view handles the actual text inputting.
  NSView<mozView>* mView; // [STRONG]

  TextInputHandlerBase(nsChildView* aWidget, NSView<mozView> *aNativeView);
  virtual ~TextInputHandlerBase();

  bool Destroyed() { return !mWidget; }

  /**
   * mCurrentKeyEvent indicates what key event we are handling.  While
   * handling a native keydown event, we need to store the event for insertText,
   * doCommandBySelector and various action message handlers of NSResponder
   * such as [NSResponder insertNewline:sender].
   */
  struct KeyEventState
  {
    // Handling native key event
    NSEvent* mKeyEvent;
    // Whether keydown event was consumed by web contents or chrome contents.
    bool mKeyDownHandled;
    // Whether keypress event was dispatched for mKeyEvent.
    bool mKeyPressDispatched;
    // Whether keypress event was consumed by web contents or chrome contents.
    bool mKeyPressHandled;

    KeyEventState() : mKeyEvent(nsnull)
    {
      Clear();
    }

    ~KeyEventState()
    {
      Clear();
    }

    void Set(NSEvent* aNativeKeyEvent)
    {
      NS_PRECONDITION(aNativeKeyEvent, "aNativeKeyEvent must not be NULL");
      Clear();
      mKeyEvent = [aNativeKeyEvent retain];
    }

    void Clear()
    {
      if (mKeyEvent) {
        [mKeyEvent release];
        mKeyEvent = nsnull;
      }
      mKeyDownHandled = PR_FALSE;
      mKeyPressDispatched = PR_FALSE;
      mKeyPressHandled = PR_FALSE;
    }

    bool KeyDownOrPressHandled()
    {
      return mKeyDownHandled || mKeyPressHandled;
    }
  };

  /**
   * Helper class for guaranteeing cleaning mCurrentKeyEvent
   */
  class AutoKeyEventStateCleaner
  {
  public:
    AutoKeyEventStateCleaner(TextInputHandlerBase* aHandler) :
      mHandler(aHandler)
    {
    }

    ~AutoKeyEventStateCleaner()
    {
      mHandler->mCurrentKeyEvent.Clear();
    }
  private:
    TextInputHandlerBase* mHandler;
  };

  // XXX If keydown event was nested, the key event is overwritten by newer
  //     event.  This is wrong behavior.  Some IMEs are making such situation.
  KeyEventState mCurrentKeyEvent;

  /**
   * IsPrintableChar() checks whether the unicode character is
   * a non-printable ASCII character or not.  Note that this returns
   * TRUE even if aChar is a non-printable UNICODE character.
   *
   * @param aChar                 A unicode character.
   * @return                      TRUE if aChar is a printable ASCII character
   *                              or a unicode character.  Otherwise, i.e,
   *                              if aChar is a non-printable ASCII character,
   *                              FALSE.
   */
  static bool IsPrintableChar(PRUnichar aChar);

  /**
   * ComputeGeckoKeyCodeFromChar() computes Gecko defined keyCode value from
   * aChar.  If aChar is not an ASCII character, this always returns FALSE.
   *
   * @param aChar                 A unicode character.
   * @return                      A Gecko defined keyCode.  Or zero if aChar
   *                              is a unicode character.
   */
  static PRUint32 ComputeGeckoKeyCodeFromChar(PRUnichar aChar);

  /**
   * IsNormalCharInputtingEvent() checks whether aKeyEvent causes text input.
   *
   * @param aKeyEvent             A key event.
   * @return                      TRUE if the key event causes text input.
   *                              Otherwise, FALSE.
   */
  static bool IsNormalCharInputtingEvent(const nsKeyEvent& aKeyEvent);

  /**
   * IsModifierKey() checks whether the native keyCode is for a modifier key.
   *
   * @param aNativeKeyCode        A native keyCode.
   * @return                      TRUE if aNativeKeyCode is for a modifier key.
   *                              Otherwise, FALSE.
   */
  static bool IsModifierKey(UInt32 aNativeKeyCode);

private:
  struct KeyboardLayoutOverride {
    PRInt32 mKeyboardLayout;
    bool mOverrideEnabled;

    KeyboardLayoutOverride() :
      mKeyboardLayout(0), mOverrideEnabled(PR_FALSE)
    {
    }
  };

  KeyboardLayoutOverride mKeyboardOverride;
};

/**
 * PluginTextInputHandler handles text input events for plugins.
 */

class PluginTextInputHandler : public TextInputHandlerBase
{
public:

  /**
   * When starting complex text input for current event on plugin, this is
   * called.  See also the comment of StartComplexTextInputForCurrentEvent() of
   * nsIPluginWidget.
   */
  nsresult StartComplexTextInputForCurrentEvent()
  {
    mPluginComplexTextInputRequested = PR_TRUE;
    return NS_OK;
  }

  /**
   * HandleKeyDownEventForPlugin() handles aNativeKeyEvent.
   *
   * @param aNativeKeyEvent       A native NSKeyDown event.
   */
  void HandleKeyDownEventForPlugin(NSEvent* aNativeKeyEvent);

  /**
   * HandleKeyUpEventForPlugin() handles aNativeKeyEvent.
   *
   * @param aNativeKeyEvent       A native NSKeyUp event.
   */
  void HandleKeyUpEventForPlugin(NSEvent* aNativeKeyEvent);

  /**
   * ConvertCocoaKeyEventToNPCocoaEvent() converts aCocoaEvent to NPCocoaEvent.
   *
   * @param aCocoaEvent           A native key event.
   * @param aPluginEvent          The result.
   */
  static void ConvertCocoaKeyEventToNPCocoaEvent(NSEvent* aCocoaEvent,
                                                 NPCocoaEvent& aPluginEvent);

#ifndef NP_NO_CARBON

  /**
   * InstallPluginKeyEventsHandler() is called when initializing process.
   * RemovePluginKeyEventsHandler() is called when finalizing process.
   * These methods initialize/finalize global resource for handling events for
   * plugins.
   */
  static void InstallPluginKeyEventsHandler();
  static void RemovePluginKeyEventsHandler();

  /**
   * This must be called before first key/IME event for plugins.
   * This method initializes IMKInputSession methods swizzling.
   */
  static void SwizzleMethods();

  /**
   * When a composition starts or finishes, this is called.
   */
  void SetPluginTSMInComposition(bool aInComposition)
  {
    mPluginTSMInComposition = aInComposition;
  }

#endif // #ifndef NP_NO_CARBON

protected:
  bool mIgnoreNextKeyUpEvent;

  PluginTextInputHandler(nsChildView* aWidget, NSView<mozView> *aNativeView);
  ~PluginTextInputHandler();

#ifndef NP_NO_CARBON

  /**
   * ConvertCocoaKeyEventToCarbonEvent() converts aCocoaKeyEvent to
   * aCarbonKeyEvent.
   *
   * @param aCocoaKeyEvent        A Cocoa key event.
   * @param aCarbonKeyEvent       Converted Carbon event from aCocoaEvent.
   * @param aMakeKeyDownEventIfNSFlagsChanged
   *                              If aCocoaKeyEvent isn't NSFlagsChanged event,
   *                              this is ignored.  Otherwise, i.e., if
   *                              aCocoaKeyEvent is NSFlagsChanged event,
   *                              set TRUE if you need a keydown event.
   *                              Otherwise, Set FALSE for a keyup event.
   */
  static void ConvertCocoaKeyEventToCarbonEvent(
                NSEvent* aCocoaKeyEvent,
                EventRecord& aCarbonKeyEvent,
                bool aMakeKeyDownEventIfNSFlagsChanged = false);

#endif // #ifndef NP_NO_CARBON

private:

#ifndef NP_NO_CARBON
  TSMDocumentID mPluginTSMDoc;

  bool mPluginTSMInComposition;
#endif // #ifndef NP_NO_CARBON

  bool mPluginComplexTextInputRequested;

  /**
   * DispatchCocoaNPAPITextEvent() dispatches a text event for Cocoa plugin.
   *
   * @param aString               A string inputted by the dispatching event.
   * @return                      TRUE if the dispatched event was consumed.
   *                              Otherwise, FALSE.
   */
  bool DispatchCocoaNPAPITextEvent(NSString* aString);

  /**
   * Whether the plugin is in composition or not.
   * On 32bit build, this returns the state of mPluginTSMInComposition.
   * On 64bit build, this returns ComplexTextInputPanel's state.
   *
   * @return                      TRUE if plugin is in composition.  Otherwise,
   *                              FALSE.
   */
  bool IsInPluginComposition();

#ifndef NP_NO_CARBON

  /**
   * Create a TSM document for use with plugins, so that we can support IME in
   * them.  Once it's created, if need be (re)activate it.  Some plugins (e.g.
   * the Flash plugin running in Camino) don't create their own TSM document --
   * without which IME can't work.  Others (e.g. the Flash plugin running in
   * Firefox) create a TSM document that (somehow) makes the input window behave
   * badly when it contains more than one kind of input (say Hiragana and
   * Romaji).  (We can't just use the per-NSView TSM documents that Cocoa
   * provides (those created and managed by the NSTSMInputContext class) -- for
   * some reason TSMProcessRawKeyEvent() doesn't work with them.)
   */
  void ActivatePluginTSMDocument();

  /**
   * HandleCarbonPluginKeyEvent() handles the aKeyEvent.  This is called by
   * PluginKeyEventsHandler().
   *
   * @param aKeyEvent             A native Carbon event.
   */
  void HandleCarbonPluginKeyEvent(EventRef aKeyEvent);

  /**
   * ConvertUnicodeToCharCode() converts aUnichar to native encoded string.
   *
   * @param aUniChar              A unicode character.
   * @param aOutChar              Native encoded string for aUniChar.
   * @return                      TRUE if the converting succeeded.
   *                              Otherwise, FALSE.
   */
  static bool ConvertUnicodeToCharCode(PRUnichar aUniChar,
                                         unsigned char* aOutChar);

  /**
   * Target for text services events sent as the result of calls made to
   * TSMProcessRawKeyEvent() in HandleKeyDownEventForPlugin() when a plugin has
   * the focus.  The calls to TSMProcessRawKeyEvent() short-circuit Cocoa-based
   * IME (which would otherwise interfere with our efforts) and allow Carbon-
   * based IME to work in plugins (via the NPAPI).  This strategy doesn't cause
   * trouble for plugins that (like the Java Embedding Plugin) bypass the NPAPI
   * to get their keyboard events and do their own Cocoa-based IME.
   */
  static OSStatus PluginKeyEventsHandler(EventHandlerCallRef aHandlerRef,
                                         EventRef aEvent,
                                         void *aUserData);

  static EventHandlerRef sPluginKeyEventsHandler;

#endif // #ifndef NP_NO_CARBON
};

/**
 * IMEInputHandler manages:
 *   1. The IME/keyboard layout statement of nsChildView.
 *   2. The IME composition statement of nsChildView.
 * And also provides the methods which controls the current IME transaction of
 * the instance.
 *
 * Note that an nsChildView handles one or more NSView's events.  E.g., even if
 * a text editor on XUL panel element, the input events handled on the parent
 * (or its ancestor) widget handles it (the native focus is set to it).  The
 * actual focused view is notified by OnFocusChangeInGecko.
 */

class IMEInputHandler : public PluginTextInputHandler
{
public:
  virtual bool OnDestroyWidget(nsChildView* aDestroyingWidget);

  virtual void OnFocusChangeInGecko(bool aFocus);

  /**
   * DispatchTextEvent() dispatches a text event on mWidget.
   *
   * @param aText                 User text input.
   * @param aAttrString           An NSAttributedString instance which indicates
   *                              current composition string.
   * @param aSelectedRange        Current selected range (or caret position).
   * @param aDoCommit             TRUE if the composition string should be
   *                              committed.  Otherwise, FALSE.
   */
  bool DispatchTextEvent(const nsString& aText,
                           NSAttributedString* aAttrString,
                           NSRange& aSelectedRange,
                           bool aDoCommit);

  /**
   * SetMarkedText() is a handler of setMarkedText of NSTextInput.
   *
   * @param aAttrString           This mut be an instance of NSAttributedString.
   *                              If the aString parameter to
   *                              [ChildView setMarkedText:setSelectedRange:]
   *                              isn't an instance of NSAttributedString,
   *                              create an NSAttributedString from it and pass
   *                              that instead.
   * @param aSelectedRange        Current selected range (or caret position).
   */
  void SetMarkedText(NSAttributedString* aAttrString,
                     NSRange& aSelectedRange);

  /**
   * ConversationIdentifier() returns an ID for the current editor.  The ID is
   * guaranteed to be unique among currently existing editors.  But it might be
   * the same as the ID of an editor that has already been destroyed.
   *
   * @return                      An identifier of current focused editor.
   */
  NSInteger ConversationIdentifier();

  /**
   * GetAttributedSubstringFromRange() returns an NSAttributedString instance
   * which is allocated as autorelease for aRange.
   *
   * @param aRange                The range of string which you want.
   * @return                      The string in aRange.  If the string is empty,
   *                              this returns nil.  If succeeded, this returns
   *                              an instance which is allocated as autorelease.
   *                              If this has some troubles, returns nil.
   */
  NSAttributedString* GetAttributedSubstringFromRange(NSRange& aRange);

  /**
   * SelectedRange() returns current selected range.
   *
   * @return                      If an editor has focus, this returns selection
   *                              range in the editor.  Otherwise, this returns
   *                              selection range  in the focused document.
   */
  NSRange SelectedRange();

  /**
   * FirstRectForCharacterRange() returns first *character* rect in the range.
   * Cocoa needs the first line rect in the range, but we cannot compute it
   * on current implementation.
   *
   * @param aRange                A range of text to examine.  Its position is
   *                              an offset from the beginning of the focused
   *                              editor or document.
   * @return                      An NSRect containing the first character in
   *                              aRange, in screen coordinates.
   *                              If the length of aRange is 0, the width will
   *                              be 0.
   */
  NSRect FirstRectForCharacterRange(NSRange& aRange);

  /**
   * CharacterIndexForPoint() returns an offset of a character at aPoint.
   * XXX This isn't implemented, always returns 0.
   *
   * @param                       The point in screen coordinates.
   * @return                      The offset of the character at aPoint from
   *                              the beginning of the focused editor or
   *                              document.
   */
  NSUInteger CharacterIndexForPoint(NSPoint& aPoint);

  /**
   * GetValidAttributesForMarkedText() returns attributes which we support.
   *
   * @return                      Always empty array for now.
   */
  NSArray* GetValidAttributesForMarkedText();

  bool HasMarkedText();
  NSRange MarkedRange();

  bool IsIMEComposing() { return mIsIMEComposing; }
  bool IsIMEOpened();
  bool IsIMEEnabled() { return mIsIMEEnabled; }
  bool IsASCIICapableOnly() { return mIsASCIICapableOnly; }
  bool IgnoreIMECommit() { return mIgnoreIMECommit; }

  bool IgnoreIMEComposition()
  {
    // Ignore the IME composition events when we're pending to discard the
    // composition and we are not to handle the IME composition now.
    return (mPendingMethods & kDiscardIMEComposition) &&
           (mIsInFocusProcessing || !IsFocused());
  }

  void CommitIMEComposition();
  void CancelIMEComposition();

  void EnableIME(bool aEnableIME);
  void SetIMEOpenState(bool aOpen);
  void SetASCIICapableOnly(bool aASCIICapableOnly);

  static CFArrayRef CreateAllIMEModeList();
  static void DebugPrintAllIMEModes();

  // Don't use ::TSMGetActiveDocument() API directly, the document may not
  // be what you want.
  static TSMDocumentID GetCurrentTSMDocumentID();

protected:
  // We cannot do some jobs in the given stack by some reasons.
  // Following flags and the timer provide the execution pending mechanism,
  // See the comment in nsCocoaTextInputHandler.mm.
  nsCOMPtr<nsITimer> mTimer;
  enum {
    kResetIMEWindowLevel     = 1,
    kDiscardIMEComposition   = 2,
    kSyncASCIICapableOnly    = 4
  };
  PRUint32 mPendingMethods;

  IMEInputHandler(nsChildView* aWidget, NSView<mozView> *aNativeView);
  virtual ~IMEInputHandler();

  bool IsFocused();
  void ResetTimer();

  virtual void ExecutePendingMethods();

  /**
   * InsertTextAsCommittingComposition() commits current composition.  If there
   * is no composition, this starts a composition and commits it immediately.
   *
   * @param aAttrString           A string which is committed.
   */
  void InsertTextAsCommittingComposition(NSAttributedString* aAttrString);

private:
  // If mIsIMEComposing is true, the composition string is stored here.
  NSString* mIMECompositionString;
  // mLastDispatchedCompositionString stores the lastest dispatched composition
  // string by compositionupdate event.
  nsString mLastDispatchedCompositionString;

  NSRange mMarkedRange;

  bool mIsIMEComposing;
  bool mIsIMEEnabled;
  bool mIsASCIICapableOnly;
  bool mIgnoreIMECommit;
  // This flag is enabled by OnFocusChangeInGecko, and will be cleared by
  // ExecutePendingMethods.  When this is true, IsFocus() returns TRUE.  At
  // that time, the focus processing in Gecko might not be finished yet.  So,
  // you cannot use nsQueryContentEvent or something.
  bool mIsInFocusProcessing;

  void KillIMEComposition();
  void SendCommittedText(NSString *aString);
  void OpenSystemPreferredLanguageIME();

  // Pending methods
  void ResetIMEWindowLevel();
  void DiscardIMEComposition();
  void SyncASCIICapableOnly();

  static bool sStaticMembersInitialized;
  static CFStringRef sLatestIMEOpenedModeInputSourceID;
  static void InitStaticMembers();
  static void OnCurrentTextInputSourceChange(CFNotificationCenterRef aCenter,
                                             void* aObserver,
                                             CFStringRef aName,
                                             const void* aObject,
                                             CFDictionaryRef aUserInfo);

  static void FlushPendingMethods(nsITimer* aTimer, void* aClosure);

  /**
   * ConvertToTextRangeStyle converts the given native underline style to
   * our defined text range type.
   *
   * @param aUnderlineStyle       NSUnderlineStyleSingle or
   *                              NSUnderlineStyleThick.
   * @param aSelectedRange        Current selected range (or caret position).
   * @return                      NS_TEXTRANGE_*.
   */
  PRUint32 ConvertToTextRangeType(PRUint32 aUnderlineStyle,
                                  NSRange& aSelectedRange);

  /**
   * GetRangeCount() computes the range count of aAttrString.
   *
   * @param aAttrString           An NSAttributedString instance whose number of
   *                              NSUnderlineStyleAttributeName ranges you with
   *                              to know.
   * @return                      The count of NSUnderlineStyleAttributeName
   *                              ranges in aAttrString.
   */
  PRUint32 GetRangeCount(NSAttributedString *aString);

  /**
   * SetTextRangeList() appends text ranges to aTextRangeList.
   *
   * @param aTextRangeList        When SetTextRangeList() returns, this will
   *                              be set to the NSUnderlineStyleAttributeName
   *                              ranges in aAttrString.  Note that if you pass
   *                              in a large enough auto-range instance for most
   *                              cases (e.g., nsAutoTArray<nsTextRange, 4>),
   *                              it prevents memory fragmentation.
   * @param aAttrString           An NSAttributedString instance which indicates
   *                              current composition string.
   * @param aSelectedRange        Current selected range (or caret position).
   */
  void SetTextRangeList(nsTArray<nsTextRange>& aTextRangeList,
                        NSAttributedString *aAttrString,
                        NSRange& aSelectedRange);

  /**
   * InitCompositionEvent() initializes aCompositionEvent.
   *
   * @param aCompositionEvent     A composition event which you want to
   *                              initialize.
   */
  void InitCompositionEvent(nsCompositionEvent& aCompositionEvent);

  /**
   * When a composition starts, OnStartIMEComposition() is called.
   */
  void OnStartIMEComposition();

  /**
   * When a composition is updated, OnUpdateIMEComposition() is called.
   */
  void OnUpdateIMEComposition(NSString* aIMECompositionString);

  /**
   * When a composition is finished, OnEndIMEComposition() is called.
   */
  void OnEndIMEComposition();

  // The focused IME handler.  Please note that the handler might lost the
  // actual focus by deactivating the application.  If we are active, this
  // must have the actual focused handle.
  // We cannot access to the NSInputManager during we aren't active, so, the
  // focused handler can have an IME transaction even if we are deactive.
  static IMEInputHandler* sFocusedIMEHandler;
};

/**
 * TextInputHandler implements the NSTextInput protocol.
 */
class TextInputHandler : public IMEInputHandler
{
public:
  static NSUInteger sLastModifierState;

  static CFArrayRef CreateAllKeyboardLayoutList();
  static void DebugPrintAllKeyboardLayouts();

  TextInputHandler(nsChildView* aWidget, NSView<mozView> *aNativeView);
  virtual ~TextInputHandler();

  /**
   * KeyDown event handler.
   *
   * @param aNativeEvent          A native NSKeyDown event.
   * @return                      TRUE if the event is consumed by web contents
   *                              or chrome contents.  Otherwise, FALSE.
   */
  bool HandleKeyDownEvent(NSEvent* aNativeEvent);

  /**
   * KeyUp event handler.
   *
   * @param aNativeEvent          A native NSKeyUp event.
   */
  void HandleKeyUpEvent(NSEvent* aNativeEvent);

  /**
   * FlagsChanged event handler.
   *
   * @param aNativeEvent          A native NSFlagsChanged event.
   */
  void HandleFlagsChanged(NSEvent* aNativeEvent);

  /**
   * Insert the string to content.  I.e., this is a text input event handler.
   * If this is called during keydown event handling, this may dispatch a
   * NS_KEY_PRESS event.  If this is called during composition, this commits
   * the composition by the aAttrString.
   *
   * @param aAttrString           An inserted string.
   */
  void InsertText(NSAttributedString *aAttrString);

  /**
   * doCommandBySelector event handler.
   *
   * @param aSelector             A selector of the command.
   * @return                      TRUE if the command is consumed.  Otherwise,
   *                              FALSE.
   */
  bool DoCommandBySelector(const char* aSelector);

  /**
   * KeyPressWasHandled() checks whether keypress event was handled or not.
   *
   * @return                      TRUE if keypress event for latest native key
   *                              event was handled.  Otherwise, FALSE.
   *                              If this handler isn't handling any key events,
   *                              always returns FALSE.
   */
  bool KeyPressWasHandled()
  {
    return mCurrentKeyEvent.mKeyPressHandled;
  }

protected:
  /**
   * DispatchKeyEventForFlagsChanged() dispatches keydown event or keyup event
   * for the aNativeEvent.
   *
   * @param aNativeEvent          A native flagschanged event which you want to
   *                              dispatch our key event for.
   * @param aDispatchKeyDown      TRUE if you want to dispatch a keydown event.
   *                              Otherwise, i.e., to dispatch keyup event,
   *                              FALSE.
   */
  void DispatchKeyEventForFlagsChanged(NSEvent* aNativeEvent,
                                       bool aDispatchKeyDown);
};

} // namespace widget
} // namespace mozilla

#endif // TextInputHandler_h_
