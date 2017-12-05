/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TextInputHandler_h_
#define TextInputHandler_h_

#include "nsCocoaUtils.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#include "mozView.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "mozilla/EventForwards.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "WritingModes.h"

class nsChildView;

namespace mozilla {
namespace widget {

// Key code constants
enum
{
#if !defined(MAC_OS_X_VERSION_10_12) || \
  MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12
  kVK_RightCommand    = 0x36, // right command key
#endif

  kVK_PC_PrintScreen     = kVK_F13,
  kVK_PC_ScrollLock      = kVK_F14,
  kVK_PC_Pause           = kVK_F15,

  kVK_PC_Insert          = kVK_Help,
  kVK_PC_Backspace       = kVK_Delete,
  kVK_PC_Delete          = kVK_ForwardDelete,

  kVK_PC_ContextMenu     = 0x6E,

  kVK_Powerbook_KeypadEnter = 0x34  // Enter on Powerbook's keyboard is different
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
  static TISInputSourceWrapper& CurrentInputSource();
  /**
   * Shutdown() should be called when nobody doesn't need to use this class.
   */
  static void Shutdown();

  TISInputSourceWrapper()
    : mInputSource{nullptr}
    , mKeyboardLayout{nullptr}
    , mUCKeyboardLayout{nullptr}
    , mIsRTL{0}
    , mOverrideKeyboard{false}
  {
    mInputSourceList = nullptr;
    Clear();
  }

  explicit TISInputSourceWrapper(const char* aID)
    : mInputSource{nullptr}
    , mKeyboardLayout{nullptr}
    , mUCKeyboardLayout{nullptr}
    , mIsRTL{0}
    , mOverrideKeyboard{false}
  {
    mInputSourceList = nullptr;
    InitByInputSourceID(aID);
  }

  explicit TISInputSourceWrapper(SInt32 aLayoutID)
    : mInputSource{nullptr}
    , mKeyboardLayout{nullptr}
    , mUCKeyboardLayout{nullptr}
    , mIsRTL{0}
    , mOverrideKeyboard{false}
  {
    mInputSourceList = nullptr;
    InitByLayoutID(aLayoutID);
  }

  explicit TISInputSourceWrapper(TISInputSourceRef aInputSource)
    : mInputSource{nullptr}
    , mKeyboardLayout{nullptr}
    , mUCKeyboardLayout{nullptr}
    , mIsRTL{0}
    , mOverrideKeyboard{false}
  {
    mInputSourceList = nullptr;
    InitByTISInputSourceRef(aInputSource);
  }

  ~TISInputSourceWrapper() { Clear(); }

  void InitByInputSourceID(const char* aID);
  void InitByInputSourceID(const nsString& aID);
  void InitByInputSourceID(const CFStringRef aID);
  /**
   * InitByLayoutID() initializes the keyboard layout by the layout ID.
   *
   * @param aLayoutID             An ID of keyboard layout.
   *                                0: US
   *                                1: Greek
   *                                2: German
   *                                3: Swedish-Pro
   *                                4: Dvorak-Qwerty Cmd
   *                                5: Thai
   *                                6: Arabic
   *                                7: French
   *                                8: Hebrew
   *                                9: Lithuanian
   *                               10: Norwegian
   *                               11: Spanish
   * @param aOverrideKeyboard     When testing set to TRUE, otherwise, set to
   *                              FALSE.  When TRUE, we use an ANSI keyboard
   *                              instead of the actual keyboard.
   */
  void InitByLayoutID(SInt32 aLayoutID, bool aOverrideKeyboard = false);
  void InitByCurrentInputSource();
  void InitByCurrentKeyboardLayout();
  void InitByCurrentASCIICapableInputSource();
  void InitByCurrentASCIICapableKeyboardLayout();
  void InitByCurrentInputMethodKeyboardLayoutOverride();
  void InitByTISInputSourceRef(TISInputSourceRef aInputSource);
  void InitByLanguage(CFStringRef aLanguage);

  /**
   * If the instance is initialized with a keyboard layout input source,
   * returns it.
   * If the instance is initialized with an IME mode input source, the result
   * references the keyboard layout for the IME mode.  However, this can be
   * initialized only when the IME mode is actually selected.  I.e, if IME mode
   * input source is initialized with LayoutID or SourceID, this returns null.
   */
  TISInputSourceRef GetKeyboardLayoutInputSource() const
  {
    return mKeyboardLayout;
  }
  const UCKeyboardLayout* GetUCKeyboardLayout();

  bool IsOpenedIMEMode();
  bool IsIMEMode();
  bool IsKeyboardLayout();

  bool IsASCIICapable()
  {
    NS_ENSURE_TRUE(mInputSource, false);
    return GetBoolProperty(kTISPropertyInputSourceIsASCIICapable);
  }

  bool IsEnabled()
  {
    NS_ENSURE_TRUE(mInputSource, false);
    return GetBoolProperty(kTISPropertyInputSourceIsEnabled);
  }

  bool GetLanguageList(CFArrayRef &aLanguageList);
  bool GetPrimaryLanguage(CFStringRef &aPrimaryLanguage);
  bool GetPrimaryLanguage(nsAString &aPrimaryLanguage);

  bool GetLocalizedName(CFStringRef &aName)
  {
    NS_ENSURE_TRUE(mInputSource, false);
    return GetStringProperty(kTISPropertyLocalizedName, aName);
  }

  bool GetLocalizedName(nsAString &aName)
  {
    NS_ENSURE_TRUE(mInputSource, false);
    return GetStringProperty(kTISPropertyLocalizedName, aName);
  }

  bool GetInputSourceID(CFStringRef &aID)
  {
    NS_ENSURE_TRUE(mInputSource, false);
    return GetStringProperty(kTISPropertyInputSourceID, aID);
  }

  bool GetInputSourceID(nsAString &aID)
  {
    NS_ENSURE_TRUE(mInputSource, false);
    return GetStringProperty(kTISPropertyInputSourceID, aID);
  }

  bool GetBundleID(CFStringRef &aBundleID)
  {
    NS_ENSURE_TRUE(mInputSource, false);
    return GetStringProperty(kTISPropertyBundleID, aBundleID);
  }

  bool GetBundleID(nsAString &aBundleID)
  {
    NS_ENSURE_TRUE(mInputSource, false);
    return GetStringProperty(kTISPropertyBundleID, aBundleID);
  }

  bool GetInputSourceType(CFStringRef &aType)
  {
    NS_ENSURE_TRUE(mInputSource, false);
    return GetStringProperty(kTISPropertyInputSourceType, aType);
  }

  bool GetInputSourceType(nsAString &aType)
  {
    NS_ENSURE_TRUE(mInputSource, false);
    return GetStringProperty(kTISPropertyInputSourceType, aType);
  }

  bool IsForRTLLanguage();
  bool IsInitializedByCurrentInputSource();

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
   * @param aInsertString         If caller expects that the event will cause
   *                              a character to be input (say in an editor),
   *                              the caller should set this.  Otherwise,
   *                              if caller sets null to this, this method will
   *                              compute the character to be input from
   *                              characters of aNativeKeyEvent.
   */
  void InitKeyEvent(NSEvent *aNativeKeyEvent, WidgetKeyboardEvent& aKeyEvent,
                    const nsAString *aInsertString = nullptr);

  /**
   * WillDispatchKeyboardEvent() computes aKeyEvent.mAlternativeCharCodes and
   * recompute aKeyEvent.mCharCode if it's necessary.
   *
   * @param aNativeKeyEvent       A native key event for which you want to
   *                              dispatch a Gecko key event.
   * @param aInsertString         If caller expects that the event will cause
   *                              a character to be input (say in an editor),
   *                              the caller should set this.  Otherwise,
   *                              if caller sets null to this, this method will
   *                              compute the character to be input from
   *                              characters of aNativeKeyEvent.
   * @param aKeyEvent             The result -- a Gecko key event initialized
   *                              from the native key event.  This must be
   *                              eKeyPress event.
   */
  void WillDispatchKeyboardEvent(NSEvent* aNativeKeyEvent,
                                 const nsAString* aInsertString,
                                 WidgetKeyboardEvent& aKeyEvent);

  /**
   * ComputeGeckoKeyCode() returns Gecko keycode for aNativeKeyCode on current
   * keyboard layout.
   *
   * @param aNativeKeyCode        A native keycode.
   * @param aKbType               A native Keyboard Type value.  Typically,
   *                              this is a result of ::LMGetKbdType().
   * @param aCmdIsPressed         TRUE if Cmd key is pressed.  Otherwise, FALSE.
   * @return                      The computed Gecko keycode.
   */
  uint32_t ComputeGeckoKeyCode(UInt32 aNativeKeyCode, UInt32 aKbType,
                               bool aCmdIsPressed);

  /**
   * ComputeGeckoKeyNameIndex() returns Gecko key name index for the key.
   *
   * @param aNativeKeyCode        A native keycode.
   */
  static KeyNameIndex ComputeGeckoKeyNameIndex(UInt32 aNativeKeyCode);

  /**
   * ComputeGeckoCodeNameIndex() returns Gecko code name index for the key.
   *
   * @param aNativeKeyCode        A native keycode.
   * @param aKbType               A native Keyboard Type value.  Typically,
   *                              this is a result of ::LMGetKbdType().
   */
  static CodeNameIndex ComputeGeckoCodeNameIndex(UInt32 aNativeKeyCode,
                                                 UInt32 aKbType);

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
  uint32_t TranslateToChar(UInt32 aKeyCode, UInt32 aModifiers, UInt32 aKbType);

  /**
   * TranslateToChar() checks if aKeyCode with aModifiers is a dead key.
   *
   * @param aKeyCode              A native keyCode.
   * @param aModifiers            Combination of native modifier flags.
   * @param aKbType               A native Keyboard Type value.  Typically,
   *                              this is a result of ::LMGetKbdType().
   * @return                      Returns true if the key with specified
   *                              modifier state isa dead key.  Otherwise,
   *                              false.
   */
  bool IsDeadKey(UInt32 aKeyCode, UInt32 aModifiers, UInt32 aKbType);

  /**
   * ComputeInsertString() computes string to be inserted with the key event.
   *
   * @param aNativeKeyEvent     The native key event which causes our keyboard
   *                            event(s).
   * @param aKeyEvent           A Gecko key event which was partially
   *                            initialized with aNativeKeyEvent.
   * @param aInsertString       The string to be inputting by aNativeKeyEvent.
   *                            This should be specified by InsertText().
   *                            In other words, if the key event doesn't cause
   *                            a call of InsertText(), this can be nullptr.
   * @param aResult             The string which should be set to charCode of
   *                            keypress event(s).
   */
  void ComputeInsertStringForCharCode(NSEvent* aNativeKeyEvent,
                                      const WidgetKeyboardEvent& aKeyEvent,
                                      const nsAString* aInsertString,
                                      nsAString& aResult);

  /**
   * IsPrintableKeyEvent() returns true if aNativeKeyEvent is caused by
   * a printable key.  Otherwise, returns false.
   */
  bool IsPrintableKeyEvent(NSEvent* aNativeKeyEvent) const;

  /**
   * GetKbdType() returns physical keyboard type.
   */
  UInt32 GetKbdType() const;

  bool GetBoolProperty(const CFStringRef aKey);
  bool GetStringProperty(const CFStringRef aKey, CFStringRef &aStr);
  bool GetStringProperty(const CFStringRef aKey, nsAString &aStr);

  TISInputSourceRef mInputSource;
  TISInputSourceRef mKeyboardLayout;
  CFArrayRef mInputSourceList;
  const UCKeyboardLayout* mUCKeyboardLayout;
  int8_t mIsRTL;

  bool mOverrideKeyboard;

  static TISInputSourceWrapper* sCurrentInputSource;
};

/**
 * TextInputHandlerBase is a base class of IMEInputHandler and TextInputHandler.
 * Utility methods should be implemented this level.
 */

class TextInputHandlerBase : public TextEventDispatcherListener
{
public:
  /**
   * Other TextEventDispatcherListener methods should be implemented in
   * IMEInputHandler.
   */
  NS_DECL_ISUPPORTS

  /**
   * DispatchEvent() dispatches aEvent on mWidget.
   *
   * @param aEvent                An event which you want to dispatch.
   * @return                      TRUE if the event is consumed by web contents
   *                              or chrome contents.  Otherwise, FALSE.
   */
  bool DispatchEvent(WidgetGUIEvent& aEvent);

  /**
   * SetSelection() dispatches eSetSelection event for the aRange.
   *
   * @param aRange                The range which will be selected.
   * @return                      TRUE if setting selection is succeeded and
   *                              the widget hasn't been destroyed.
   *                              Otherwise, FALSE.
   */
  bool SetSelection(NSRange& aRange);

  /**
   * InitKeyEvent() initializes aKeyEvent for aNativeKeyEvent.
   *
   * @param aNativeKeyEvent       A native key event for which you want to
   *                              dispatch a Gecko key event.
   * @param aKeyEvent             The result -- a Gecko key event initialized
   *                              from the native key event.
   * @param aInsertString         If caller expects that the event will cause
   *                              a character to be input (say in an editor),
   *                              the caller should set this.  Otherwise,
   *                              if caller sets null to this, this method will
   *                              compute the character to be input from
   *                              characters of aNativeKeyEvent.
   */
  void InitKeyEvent(NSEvent *aNativeKeyEvent, WidgetKeyboardEvent& aKeyEvent,
                    const nsAString *aInsertString = nullptr);

  /**
   * SynthesizeNativeKeyEvent() is an implementation of
   * nsIWidget::SynthesizeNativeKeyEvent().  See the document in nsIWidget.h
   * for the detail.
   */
  nsresult SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                    int32_t aNativeKeyCode,
                                    uint32_t aModifierFlags,
                                    const nsAString& aCharacters,
                                    const nsAString& aUnmodifiedCharacters);

  /**
   * Utility method intended for testing. Attempts to construct a native key
   * event that would have been generated during an actual key press. This
   * *does not dispatch* the native event. Instead, it is attached to the
   * |mNativeKeyEvent| field of the Gecko event that is passed in.
   * @param aKeyEvent  Gecko key event to attach the native event to
   */
  NS_IMETHOD AttachNativeKeyEvent(WidgetKeyboardEvent& aKeyEvent);

  /**
   * GetWindowLevel() returns the window level of current focused (in Gecko)
   * window.  E.g., if an <input> element in XUL panel has focus, this returns
   * the XUL panel's window level.
   */
  NSInteger GetWindowLevel();

  /**
   * IsSpecialGeckoKey() checks whether aNativeKeyCode is mapped to a special
   * Gecko keyCode.  A key is "special" if it isn't used for text input.
   *
   * @param aNativeKeyCode        A native keycode.
   * @return                      If the keycode is mapped to a special key,
   *                              TRUE.  Otherwise, FALSE.
   */
  static bool IsSpecialGeckoKey(UInt32 aNativeKeyCode);


  /**
   * EnableSecureEventInput() and DisableSecureEventInput() wrap the Carbon
   * Event Manager APIs with the same names.  In addition they keep track of
   * how many times we've called them (in the same process) -- unlike the
   * Carbon Event Manager APIs, which only keep track of how many times they've
   * been called from any and all processes.
   *
   * The Carbon Event Manager's IsSecureEventInputEnabled() returns whether
   * secure event input mode is enabled (in any process).  This class's
   * IsSecureEventInputEnabled() returns whether we've made any calls to
   * EnableSecureEventInput() that are not (yet) offset by the calls we've
   * made to DisableSecureEventInput().
   */
  static void EnableSecureEventInput();
  static void DisableSecureEventInput();
  static bool IsSecureEventInputEnabled();

  /**
   * EnsureSecureEventInputDisabled() calls DisableSecureEventInput() until
   * our call count becomes 0.
   */
  static void EnsureSecureEventInputDisabled();

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
  // The creator of this instance, client and its text event dispatcher.
  // These members must not be nullptr after initialized until
  // OnDestroyWidget() is called.
  nsChildView* mWidget; // [WEAK]
  RefPtr<TextEventDispatcher> mDispatcher;

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
    // String specified by InsertText().  This is not null only during a
    // call of InsertText().
    nsAString* mInsertString;
    // String which are included in [mKeyEvent characters] and already handled
    // by InsertText() call(s).
    nsString mInsertedString;
    // Unique id associated with a keydown / keypress event. It's ok if this
    // wraps over long periods.
    uint32_t mUniqueId;
    // Whether keydown event was consumed by web contents or chrome contents.
    bool mKeyDownHandled;
    // Whether keypress event was dispatched for mKeyEvent.
    bool mKeyPressDispatched;
    // Whether keypress event was consumed by web contents or chrome contents.
    bool mKeyPressHandled;
    // Whether the key event causes other key events via IME or something.
    bool mCausedOtherKeyEvents;
    // Whether the key event causes composition change or committing
    // composition.  So, even if InsertText() is called, this may be false
    // if it dispatches keypress event.
    bool mCompositionDispatched;

    KeyEventState()
      : mKeyEvent(nullptr)
      , mUniqueId(0)
    {
      Clear();
    }

    explicit KeyEventState(NSEvent* aNativeKeyEvent, uint32_t aUniqueId = 0)
      : mKeyEvent(nullptr)
      , mUniqueId(0)
    {
      Clear();
      Set(aNativeKeyEvent, aUniqueId);
    }

    KeyEventState(const KeyEventState &aOther) = delete;

    ~KeyEventState()
    {
      Clear();
    }

    void Set(NSEvent* aNativeKeyEvent, uint32_t aUniqueId = 0)
    {
      NS_PRECONDITION(aNativeKeyEvent, "aNativeKeyEvent must not be NULL");
      Clear();
      mKeyEvent = [aNativeKeyEvent retain];
      mUniqueId = aUniqueId;
    }

    void Clear()
    {
      if (mKeyEvent) {
        [mKeyEvent release];
        mKeyEvent = nullptr;
        mUniqueId = 0;
      }
      mInsertString = nullptr;
      mInsertedString.Truncate();
      mKeyDownHandled = false;
      mKeyPressDispatched = false;
      mKeyPressHandled = false;
      mCausedOtherKeyEvents = false;
      mCompositionDispatched = false;
    }

    bool IsDefaultPrevented() const
    {
      return mKeyDownHandled || mKeyPressHandled || mCausedOtherKeyEvents ||
             mCompositionDispatched;
    }

    bool CanDispatchKeyPressEvent() const
    {
      return !mKeyPressDispatched && !IsDefaultPrevented();
    }

    bool CanHandleCommand() const
    {
      return !mKeyDownHandled && !mKeyPressHandled;
    }

    bool IsProperKeyEvent(Command aCommand) const
    {
      if (NS_WARN_IF(!mKeyEvent)) {
        return false;
      }
      KeyNameIndex keyNameIndex =
        TISInputSourceWrapper::ComputeGeckoKeyNameIndex([mKeyEvent keyCode]);
      Modifiers modifiers =
        nsCocoaUtils::ModifiersForEvent(mKeyEvent) & (MODIFIER_SHIFT |
                                                      MODIFIER_CONTROL |
                                                      MODIFIER_ALT |
                                                      MODIFIER_META);
      switch (aCommand) {
        case CommandInsertLineBreak:
          return keyNameIndex == KEY_NAME_INDEX_Enter &&
                 modifiers == MODIFIER_CONTROL;
        case CommandInsertParagraph:
          return keyNameIndex == KEY_NAME_INDEX_Enter &&
                 modifiers == MODIFIER_NONE;
        case CommandDeleteCharBackward:
          return keyNameIndex == KEY_NAME_INDEX_Backspace &&
                 modifiers == MODIFIER_NONE;
        case CommandDeleteToBeginningOfLine:
          return keyNameIndex == KEY_NAME_INDEX_Backspace &&
                 modifiers == MODIFIER_META;
        case CommandDeleteWordBackward:
          return keyNameIndex == KEY_NAME_INDEX_Backspace &&
                 modifiers == MODIFIER_ALT;
        case CommandDeleteCharForward:
          return keyNameIndex == KEY_NAME_INDEX_Delete &&
                 modifiers == MODIFIER_NONE;
        case CommandDeleteWordForward:
          return keyNameIndex == KEY_NAME_INDEX_Delete &&
                 modifiers == MODIFIER_ALT;
        case CommandInsertTab:
          return keyNameIndex == KEY_NAME_INDEX_Tab &&
                 modifiers == MODIFIER_NONE;
        case CommandInsertBacktab:
          return keyNameIndex == KEY_NAME_INDEX_Tab &&
                 modifiers == MODIFIER_SHIFT;
        case CommandCharNext:
          return keyNameIndex == KEY_NAME_INDEX_ArrowRight &&
                 modifiers == MODIFIER_NONE;
        case CommandSelectCharNext:
          return keyNameIndex == KEY_NAME_INDEX_ArrowRight &&
                 modifiers == MODIFIER_SHIFT;
        case CommandWordNext:
          return keyNameIndex == KEY_NAME_INDEX_ArrowRight &&
                 modifiers == MODIFIER_ALT;
        case CommandSelectWordNext:
          return keyNameIndex == KEY_NAME_INDEX_ArrowRight &&
                 modifiers == (MODIFIER_ALT | MODIFIER_SHIFT);
        case CommandEndLine:
          return keyNameIndex == KEY_NAME_INDEX_ArrowRight &&
                 modifiers == MODIFIER_META;
        case CommandSelectEndLine:
          return keyNameIndex == KEY_NAME_INDEX_ArrowRight &&
                 modifiers == (MODIFIER_META | MODIFIER_SHIFT);
        case CommandCharPrevious:
          return keyNameIndex == KEY_NAME_INDEX_ArrowLeft &&
                 modifiers == MODIFIER_NONE;
        case CommandSelectCharPrevious:
          return keyNameIndex == KEY_NAME_INDEX_ArrowLeft &&
                 modifiers == MODIFIER_SHIFT;
        case CommandWordPrevious:
          return keyNameIndex == KEY_NAME_INDEX_ArrowLeft &&
                 modifiers == MODIFIER_ALT;
        case CommandSelectWordPrevious:
          return keyNameIndex == KEY_NAME_INDEX_ArrowLeft &&
                 modifiers == (MODIFIER_ALT | MODIFIER_SHIFT);
        case CommandBeginLine:
          return keyNameIndex == KEY_NAME_INDEX_ArrowLeft &&
                 modifiers == MODIFIER_META;
        case CommandSelectBeginLine:
          return keyNameIndex == KEY_NAME_INDEX_ArrowLeft &&
                 modifiers == (MODIFIER_META | MODIFIER_SHIFT);
        case CommandLinePrevious:
          return keyNameIndex == KEY_NAME_INDEX_ArrowUp &&
                 modifiers == MODIFIER_NONE;
        case CommandSelectLinePrevious:
          return keyNameIndex == KEY_NAME_INDEX_ArrowUp &&
                 modifiers == MODIFIER_SHIFT;
        case CommandMoveTop:
          return keyNameIndex == KEY_NAME_INDEX_ArrowUp &&
                 modifiers == MODIFIER_META;
        case CommandSelectTop:
          return (keyNameIndex == KEY_NAME_INDEX_ArrowUp &&
                  modifiers == (MODIFIER_META | MODIFIER_SHIFT)) ||
                 (keyNameIndex == KEY_NAME_INDEX_Home &&
                  modifiers == MODIFIER_SHIFT);
        case CommandLineNext:
          return keyNameIndex == KEY_NAME_INDEX_ArrowDown &&
                 modifiers == MODIFIER_NONE;
        case CommandSelectLineNext:
          return keyNameIndex == KEY_NAME_INDEX_ArrowDown &&
                 modifiers == MODIFIER_SHIFT;
        case CommandMoveBottom:
          return keyNameIndex == KEY_NAME_INDEX_ArrowDown &&
                 modifiers == MODIFIER_META;
        case CommandSelectBottom:
          return (keyNameIndex == KEY_NAME_INDEX_ArrowDown &&
                  modifiers == (MODIFIER_META | MODIFIER_SHIFT)) ||
                 (keyNameIndex == KEY_NAME_INDEX_End &&
                  modifiers == MODIFIER_SHIFT);
        case CommandScrollPageUp:
          return keyNameIndex == KEY_NAME_INDEX_PageUp &&
                 modifiers == MODIFIER_NONE;
        case CommandSelectPageUp:
          return keyNameIndex == KEY_NAME_INDEX_PageUp &&
                 modifiers == MODIFIER_SHIFT;
        case CommandScrollPageDown:
          return keyNameIndex == KEY_NAME_INDEX_PageDown &&
                 modifiers == MODIFIER_NONE;
        case CommandSelectPageDown:
          return keyNameIndex == KEY_NAME_INDEX_PageDown &&
                 modifiers == MODIFIER_SHIFT;
        case CommandScrollBottom:
          return keyNameIndex == KEY_NAME_INDEX_End &&
                 modifiers == MODIFIER_NONE;
        case CommandScrollTop:
          return keyNameIndex == KEY_NAME_INDEX_Home &&
                 modifiers == MODIFIER_NONE;
        case CommandCancelOperation:
          return (keyNameIndex == KEY_NAME_INDEX_Escape &&
                  (modifiers == MODIFIER_NONE ||
                   modifiers == MODIFIER_SHIFT)) ||
                 ([mKeyEvent keyCode] == kVK_ANSI_Period &&
                  modifiers == MODIFIER_META);
        case CommandComplete:
          return keyNameIndex == KEY_NAME_INDEX_Escape &&
                 (modifiers == MODIFIER_ALT ||
                  modifiers == (MODIFIER_ALT | MODIFIER_SHIFT));
        default:
          return false;
      }
    }

    void InitKeyEvent(TextInputHandlerBase* aHandler,
                      WidgetKeyboardEvent& aKeyEvent);

    /**
     * GetUnhandledString() returns characters of the event which have not been
     * handled with InsertText() yet. For example, if there is a composition
     * caused by a dead key press like '`' and it's committed by some key
     * combinations like |Cmd+v|, then, the |v|'s KeyDown event's |characters|
     * is |`v|.  Then, after |`| is committed with a call of InsertString(),
     * this returns only 'v'.
     */
    void GetUnhandledString(nsAString& aUnhandledString) const;
  };

  /**
   * Helper classes for guaranteeing cleaning mCurrentKeyEvent
   */
  class AutoKeyEventStateCleaner
  {
  public:
    explicit AutoKeyEventStateCleaner(TextInputHandlerBase* aHandler) :
      mHandler(aHandler)
    {
    }

    ~AutoKeyEventStateCleaner()
    {
      mHandler->RemoveCurrentKeyEvent();
    }
  private:
    RefPtr<TextInputHandlerBase> mHandler;
  };

  class MOZ_STACK_CLASS AutoInsertStringClearer
  {
  public:
    explicit AutoInsertStringClearer(KeyEventState* aState)
      : mState(aState)
    {
    }
    ~AutoInsertStringClearer();

  private:
    KeyEventState* mState;
  };

  /**
   * mCurrentKeyEvents stores all key events which are being processed.
   * When we call interpretKeyEvents, IME may generate other key events.
   * mCurrentKeyEvents[0] is the latest key event.
   */
  nsTArray<KeyEventState*> mCurrentKeyEvents;

  /**
   * mFirstKeyEvent must be used for first key event.  This member prevents
   * memory fragmentation for most key events.
   */
  KeyEventState mFirstKeyEvent;

  /**
   * PushKeyEvent() adds the current key event to mCurrentKeyEvents.
   */
  KeyEventState* PushKeyEvent(NSEvent* aNativeKeyEvent, uint32_t aUniqueId = 0)
  {
    uint32_t nestCount = mCurrentKeyEvents.Length();
    for (uint32_t i = 0; i < nestCount; i++) {
      // When the key event is caused by another key event, all key events
      // which are being handled should be marked as "consumed".
      mCurrentKeyEvents[i]->mCausedOtherKeyEvents = true;
    }

    KeyEventState* keyEvent = nullptr;
    if (nestCount == 0) {
      mFirstKeyEvent.Set(aNativeKeyEvent, aUniqueId);
      keyEvent = &mFirstKeyEvent;
    } else {
      keyEvent = new KeyEventState(aNativeKeyEvent, aUniqueId);
    }
    return *mCurrentKeyEvents.AppendElement(keyEvent);
  }

  /**
   * RemoveCurrentKeyEvent() removes the current key event from
   * mCurrentKeyEvents.
   */
  void RemoveCurrentKeyEvent()
  {
    NS_ASSERTION(mCurrentKeyEvents.Length() > 0,
                 "RemoveCurrentKeyEvent() is called unexpectedly");
    KeyEventState* keyEvent = GetCurrentKeyEvent();
    mCurrentKeyEvents.RemoveElementAt(mCurrentKeyEvents.Length() - 1);
    if (keyEvent == &mFirstKeyEvent) {
      keyEvent->Clear();
    } else {
      delete keyEvent;
    }
  }

  /**
   * GetCurrentKeyEvent() returns current processing key event.
   */
  KeyEventState* GetCurrentKeyEvent()
  {
    if (mCurrentKeyEvents.Length() == 0) {
      return nullptr;
    }
    return mCurrentKeyEvents[mCurrentKeyEvents.Length() - 1];
  }

  struct KeyboardLayoutOverride final
  {
    int32_t mKeyboardLayout;
    bool mOverrideEnabled;

    KeyboardLayoutOverride() :
      mKeyboardLayout(0), mOverrideEnabled(false)
    {
    }
  };

  const KeyboardLayoutOverride& KeyboardLayoutOverrideRef() const
  {
    return mKeyboardOverride;
  }

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
  static bool IsPrintableChar(char16_t aChar);

  /**
   * IsNormalCharInputtingEvent() checks whether aKeyEvent causes text input.
   *
   * @param aKeyEvent             A key event.
   * @return                      TRUE if the key event causes text input.
   *                              Otherwise, FALSE.
   */
  static bool IsNormalCharInputtingEvent(const WidgetKeyboardEvent& aKeyEvent);

  /**
   * IsModifierKey() checks whether the native keyCode is for a modifier key.
   *
   * @param aNativeKeyCode        A native keyCode.
   * @return                      TRUE if aNativeKeyCode is for a modifier key.
   *                              Otherwise, FALSE.
   */
  static bool IsModifierKey(UInt32 aNativeKeyCode);

private:
  KeyboardLayoutOverride mKeyboardOverride;

  static int32_t sSecureEventInputCount;
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

class IMEInputHandler : public TextInputHandlerBase
{
public:
  // TextEventDispatcherListener methods
  NS_IMETHOD NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                       const IMENotification& aNotification) override;
  NS_IMETHOD_(IMENotificationRequests) GetIMENotificationRequests() override;
  NS_IMETHOD_(void) OnRemovedFrom(
                      TextEventDispatcher* aTextEventDispatcher) override;
  NS_IMETHOD_(void) WillDispatchKeyboardEvent(
                      TextEventDispatcher* aTextEventDispatcher,
                      WidgetKeyboardEvent& aKeyboardEvent,
                      uint32_t aIndexOfKeypress,
                      void* aData) override;

public:
  virtual bool OnDestroyWidget(nsChildView* aDestroyingWidget) override;

  virtual void OnFocusChangeInGecko(bool aFocus);

  void OnSelectionChange(const IMENotification& aIMENotification);
  void OnLayoutChange();

  /**
   * Call [NSTextInputContext handleEvent] for mouse event support of IME
   */
  bool OnHandleEvent(NSEvent* aEvent);

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
   * @param aReplacementRange     The range which will be replaced with the
   *                              aAttrString instead of current marked range.
   */
  void SetMarkedText(NSAttributedString* aAttrString,
                     NSRange& aSelectedRange,
                     NSRange* aReplacementRange = nullptr);

  /**
   * GetAttributedSubstringFromRange() returns an NSAttributedString instance
   * which is allocated as autorelease for aRange.
   *
   * @param aRange                The range of string which you want.
   * @param aActualRange          The actual range of the result.
   * @return                      The string in aRange.  If the string is empty,
   *                              this returns nil.  If succeeded, this returns
   *                              an instance which is allocated as autorelease.
   *                              If this has some troubles, returns nil.
   */
  NSAttributedString* GetAttributedSubstringFromRange(
                        NSRange& aRange,
                        NSRange* aActualRange = nullptr);

  /**
   * SelectedRange() returns current selected range.
   *
   * @return                      If an editor has focus, this returns selection
   *                              range in the editor.  Otherwise, this returns
   *                              selection range  in the focused document.
   */
  NSRange SelectedRange();

  /**
   * DrawsVerticallyForCharacterAtIndex() returns whether the character at
   * the given index is being rendered vertically.
   *
   * @param aCharIndex            The character offset to query.
   *
   * @return                      True if writing-mode is vertical at the given
   *                              character offset; otherwise false.
   */
  bool DrawsVerticallyForCharacterAtIndex(uint32_t aCharIndex);

  /**
   * FirstRectForCharacterRange() returns first *character* rect in the range.
   * Cocoa needs the first line rect in the range, but we cannot compute it
   * on current implementation.
   *
   * @param aRange                A range of text to examine.  Its position is
   *                              an offset from the beginning of the focused
   *                              editor or document.
   * @param aActualRange          If this is not null, this returns the actual
   *                              range used for computing the result.
   * @return                      An NSRect containing the first character in
   *                              aRange, in screen coordinates.
   *                              If the length of aRange is 0, the width will
   *                              be 0.
   */
  NSRect FirstRectForCharacterRange(NSRange& aRange,
                                    NSRange* aActualRange = nullptr);

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

  void CommitIMEComposition();
  void CancelIMEComposition();

  void EnableIME(bool aEnableIME);
  void SetIMEOpenState(bool aOpen);
  void SetASCIICapableOnly(bool aASCIICapableOnly);

  /**
   * True if OSX believes that our view has keyboard focus.
   */
  bool IsFocused();

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
    kNotifyIMEOfFocusChangeInGecko = 1,
    kSyncASCIICapableOnly          = 2
  };
  uint32_t mPendingMethods;

  IMEInputHandler(nsChildView* aWidget, NSView<mozView> *aNativeView);
  virtual ~IMEInputHandler();

  void ResetTimer();

  virtual void ExecutePendingMethods();

  /**
   * InsertTextAsCommittingComposition() commits current composition.  If there
   * is no composition, this starts a composition and commits it immediately.
   *
   * @param aAttrString           A string which is committed.
   * @param aReplacementRange     The range which will be replaced with the
   *                              aAttrString instead of current selection.
   */
  void InsertTextAsCommittingComposition(NSAttributedString* aAttrString,
                                         NSRange* aReplacementRange);

private:
  // If mIsIMEComposing is true, the composition string is stored here.
  NSString* mIMECompositionString;
  // If mIsIMEComposing is true, the start offset of the composition string.
  uint32_t mIMECompositionStart;

  NSRange mMarkedRange;
  NSRange mSelectedRange;

  NSRange mRangeForWritingMode; // range within which mWritingMode applies
  mozilla::WritingMode mWritingMode;

  bool mIsIMEComposing;
  bool mIsIMEEnabled;
  bool mIsASCIICapableOnly;
  bool mIgnoreIMECommit;
  bool mIMEHasFocus;

  void KillIMEComposition();
  void SendCommittedText(NSString *aString);
  void OpenSystemPreferredLanguageIME();

  // Pending methods
  void NotifyIMEOfFocusChangeInGecko();
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
  TextRangeType ConvertToTextRangeType(uint32_t aUnderlineStyle,
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
  uint32_t GetRangeCount(NSAttributedString *aString);

  /**
   * CreateTextRangeArray() returns text ranges for clauses and/or caret.
   *
   * @param aAttrString           An NSAttributedString instance which indicates
   *                              current composition string.
   * @param aSelectedRange        Current selected range (or caret position).
   * @return                      The result is set to the
   *                              NSUnderlineStyleAttributeName ranges in
   *                              aAttrString.
   */
  already_AddRefed<mozilla::TextRangeArray>
    CreateTextRangeArray(NSAttributedString *aAttrString,
                         NSRange& aSelectedRange);

  /**
   * DispatchCompositionStartEvent() dispatches a compositionstart event and
   * initializes the members indicating composition state.
   *
   * @return                      true if it can continues handling composition.
   *                              Otherwise, e.g., canceled by the web page,
   *                              this returns false.
   */
  bool DispatchCompositionStartEvent();

  /**
   * DispatchCompositionChangeEvent() dispatches a compositionchange event on
   * mWidget and modifies the members indicating composition state.
   *
   * @param aText                 User text input.
   * @param aAttrString           An NSAttributedString instance which indicates
   *                              current composition string.
   * @param aSelectedRange        Current selected range (or caret position).
   *
   * @return                      true if it can continues handling composition.
   *                              Otherwise, e.g., canceled by the web page,
   *                              this returns false.
   */
  bool DispatchCompositionChangeEvent(const nsString& aText,
                                      NSAttributedString* aAttrString,
                                      NSRange& aSelectedRange);

  /**
   * DispatchCompositionCommitEvent() dispatches a compositioncommit event or
   * compositioncommitasis event.  If aCommitString is null, dispatches
   * compositioncommitasis event.  I.e., if aCommitString is null, this
   * commits the composition with the last data.  Otherwise, commits the
   * composition with aCommitString value.
   *
   * @return                      true if the widget isn't destroyed.
   *                              Otherwise, false.
   */
  bool DispatchCompositionCommitEvent(const nsAString* aCommitString = nullptr);

  // The focused IME handler.  Please note that the handler might lost the
  // actual focus by deactivating the application.  If we are active, this
  // must have the actual focused handle.
  // We cannot access to the NSInputManager during we aren't active, so, the
  // focused handler can have an IME transaction even if we are deactive.
  static IMEInputHandler* sFocusedIMEHandler;

  static bool sCachedIsForRTLLangage;
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
   * @param aUniqueId             A unique ID for the event.
   * @return                      TRUE if the event is dispatched to web
   *                              contents or chrome contents. Otherwise, FALSE.
   */
  bool HandleKeyDownEvent(NSEvent* aNativeEvent, uint32_t aUniqueId);

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
   * eKeyPress event.  If this is called during composition, this commits
   * the composition by the aAttrString.
   *
   * @param aAttrString           An inserted string.
   * @param aReplacementRange     The range which will be replaced with the
   *                              aAttrString instead of current selection.
   */
  void InsertText(NSAttributedString *aAttrString,
                  NSRange* aReplacementRange = nullptr);

  /**
   * Handles aCommand.  This may cause dispatching an eKeyPress event.
   *
   * @param aCommand    The command which receives from Cocoa.
   * @return            true if this handles the command even if it does
   *                    nothing actually.  Otherwise, false.
   */
  bool HandleCommand(Command aCommand);

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
    KeyEventState* currentKeyEvent = GetCurrentKeyEvent();
    return currentKeyEvent && currentKeyEvent->mKeyPressHandled;
  }

protected:
  // Stores the association of device dependent modifier flags with a modifier
  // keyCode.  Being device dependent, this association may differ from one kind
  // of hardware to the next.
  struct ModifierKey
  {
    NSUInteger flags;
    unsigned short keyCode;

    ModifierKey(NSUInteger aFlags, unsigned short aKeyCode) :
      flags(aFlags), keyCode(aKeyCode)
    {
    }

    NSUInteger GetDeviceDependentFlags() const
    {
      return (flags & ~NSDeviceIndependentModifierFlagsMask);
    }

    NSUInteger GetDeviceIndependentFlags() const
    {
      return (flags & NSDeviceIndependentModifierFlagsMask);
    }
  };
  typedef nsTArray<ModifierKey> ModifierKeyArray;
  ModifierKeyArray mModifierKeys;

  /**
   * GetModifierKeyForNativeKeyCode() returns the stored ModifierKey for
   * the key.
   */
  const ModifierKey*
    GetModifierKeyForNativeKeyCode(unsigned short aKeyCode) const;

  /**
   * GetModifierKeyForDeviceDependentFlags() returns the stored ModifierKey for
   * the device dependent flags.
   */
  const ModifierKey*
    GetModifierKeyForDeviceDependentFlags(NSUInteger aFlags) const;

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
