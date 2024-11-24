/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextEvents_h__
#define mozilla_TextEvents_h__

#include <stdint.h>

#include "mozilla/Assertions.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EventForwards.h"  // for KeyNameIndex, temporarily
#include "mozilla/FontRange.h"
#include "mozilla/Maybe.h"
#include "mozilla/NativeKeyBindingsType.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/TextRange.h"
#include "mozilla/WritingModes.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "mozilla/dom/StaticRange.h"
#include "mozilla/widget/IMEData.h"
#include "mozilla/ipc/IPCForwards.h"
#include "nsCOMPtr.h"
#include "nsHashtablesFwd.h"
#include "nsIFrame.h"
#include "nsISelectionListener.h"
#include "nsITransferable.h"
#include "nsRect.h"
#include "nsString.h"
#include "nsTArray.h"

class nsStringHashKey;

/******************************************************************************
 * virtual keycode values
 ******************************************************************************/

enum {
#define NS_DEFINE_VK(aDOMKeyName, aDOMKeyCode) NS_##aDOMKeyName = aDOMKeyCode,
#include "mozilla/VirtualKeyCodeList.h"
#undef NS_DEFINE_VK
  NS_VK_UNKNOWN = 0xFF
};

namespace mozilla {

enum : uint32_t {
  eKeyLocationStandard = dom::KeyboardEvent_Binding::DOM_KEY_LOCATION_STANDARD,
  eKeyLocationLeft = dom::KeyboardEvent_Binding::DOM_KEY_LOCATION_LEFT,
  eKeyLocationRight = dom::KeyboardEvent_Binding::DOM_KEY_LOCATION_RIGHT,
  eKeyLocationNumpad = dom::KeyboardEvent_Binding::DOM_KEY_LOCATION_NUMPAD
};

const nsCString GetDOMKeyCodeName(uint32_t aKeyCode);

namespace dom {
class PBrowserParent;
class PBrowserChild;
}  // namespace dom
namespace plugins {
class PPluginInstanceChild;
}  // namespace plugins

enum class AccessKeyType {
  // Handle access key for chrome.
  eChrome,
  // Handle access key for content.
  eContent,
  // Don't handle access key.
  eNone
};

/******************************************************************************
 * mozilla::AlternativeCharCode
 *
 * This stores alternative charCode values of a key event with some modifiers.
 * The stored values proper for testing shortcut key or access key.
 ******************************************************************************/

struct AlternativeCharCode {
  AlternativeCharCode() = default;
  AlternativeCharCode(uint32_t aUnshiftedCharCode, uint32_t aShiftedCharCode)
      : mUnshiftedCharCode(aUnshiftedCharCode),
        mShiftedCharCode(aShiftedCharCode) {}

  uint32_t mUnshiftedCharCode = 0u;
  uint32_t mShiftedCharCode = 0u;

  bool operator==(const AlternativeCharCode& aOther) const {
    return mUnshiftedCharCode == aOther.mUnshiftedCharCode &&
           mShiftedCharCode == aOther.mShiftedCharCode;
  }
  bool operator!=(const AlternativeCharCode& aOther) const {
    return !(*this == aOther);
  }
};

/******************************************************************************
 * mozilla::ShortcutKeyCandidate
 *
 * This stores a candidate of shortcut key combination.
 ******************************************************************************/

struct ShortcutKeyCandidate {
  enum class ShiftState : bool {
    // Can ignore `Shift` modifier state when comparing the key combination.
    // E.g., Ctrl + Shift + `:` in the US keyboard layout may match with
    // Ctrl + `:` shortcut.
    Ignorable,
    // `Shift` modifier state should be respected.  I.e., Ctrl + `;` in the US
    // keyboard layout never matches with Ctrl + Shift + `;` shortcut.
    MatchExactly,
  };

  enum class SkipIfEarlierHandlerDisabled : bool {
    // Even if an earlier handler is disabled, this may match with another
    // handler for avoiding inaccessible shortcut with the active keyboard
    // layout.
    No,
    // If an earlier handler (i.e., preferred handler) is disabled, this should
    // not try to match.  E.g., Ctrl + `-` in the French keyboard layout when
    // the zoom level is the minimum value, it should not match with Ctrl + `6`
    // shortcut (French keyboard layout introduces `-` when pressing Digit6 key
    // without Shift, and Shift + Digit6 introduces `6`).
    Yes,
  };

  ShortcutKeyCandidate() = default;
  ShortcutKeyCandidate(
      uint32_t aCharCode, ShiftState aShiftState,
      SkipIfEarlierHandlerDisabled aSkipIfEarlierHandlerDisabled)
      : mCharCode(aCharCode),
        mShiftState(aShiftState),
        mSkipIfEarlierHandlerDisabled(aSkipIfEarlierHandlerDisabled) {}

  // The mCharCode value which must match keyboard shortcut definition.
  uint32_t mCharCode = 0;

  ShiftState mShiftState = ShiftState::MatchExactly;
  SkipIfEarlierHandlerDisabled mSkipIfEarlierHandlerDisabled =
      SkipIfEarlierHandlerDisabled::No;
};

/******************************************************************************
 * mozilla::IgnoreModifierState
 *
 * This stores flags for modifiers that should be ignored when matching
 * XBL handlers.
 ******************************************************************************/

struct IgnoreModifierState {
  // When mShift is true, Shift key state will be ignored.
  bool mShift;
  // When mMeta is true, Meta key state will be ignored.
  bool mMeta;

  IgnoreModifierState() : mShift(false), mMeta(false) {}
};

/******************************************************************************
 * mozilla::WidgetKeyboardEvent
 ******************************************************************************/

class WidgetKeyboardEvent final : public WidgetInputEvent {
 private:
  friend class dom::PBrowserParent;
  friend class dom::PBrowserChild;
  friend struct IPC::ParamTraits<WidgetKeyboardEvent>;
  ALLOW_DEPRECATED_READPARAM

 protected:
  WidgetKeyboardEvent()
      : mNativeKeyEvent(nullptr),
        mKeyCode(0),
        mCharCode(0),
        mPseudoCharCode(0),
        mLocation(eKeyLocationStandard),
        mUniqueId(0),
        mKeyNameIndex(KEY_NAME_INDEX_Unidentified),
        mCodeNameIndex(CODE_NAME_INDEX_UNKNOWN),
        mIsRepeat(false),
        mIsComposing(false),
        mIsSynthesizedByTIP(false),
        mMaybeSkippableInRemoteProcess(true),
        mUseLegacyKeyCodeAndCharCodeValues(false),
        mEditCommandsForSingleLineEditorInitialized(false),
        mEditCommandsForMultiLineEditorInitialized(false),
        mEditCommandsForRichTextEditorInitialized(false) {}

 public:
  WidgetKeyboardEvent* AsKeyboardEvent() override { return this; }

  WidgetKeyboardEvent(bool aIsTrusted, EventMessage aMessage,
                      nsIWidget* aWidget,
                      EventClassID aEventClassID = eKeyboardEventClass,
                      const WidgetEventTime* aTime = nullptr)
      : WidgetInputEvent(aIsTrusted, aMessage, aWidget, aEventClassID, aTime),
        mNativeKeyEvent(nullptr),
        mKeyCode(0),
        mCharCode(0),
        mPseudoCharCode(0),
        mLocation(eKeyLocationStandard),
        mUniqueId(0),
        mKeyNameIndex(KEY_NAME_INDEX_Unidentified),
        mCodeNameIndex(CODE_NAME_INDEX_UNKNOWN),
        mIsRepeat(false),
        mIsComposing(false),
        mIsSynthesizedByTIP(false),
        mMaybeSkippableInRemoteProcess(true),
        mUseLegacyKeyCodeAndCharCodeValues(false),
        mEditCommandsForSingleLineEditorInitialized(false),
        mEditCommandsForMultiLineEditorInitialized(false),
        mEditCommandsForRichTextEditorInitialized(false) {}

  // IsInputtingText() and IsInputtingLineBreak() are used to check if
  // it should cause eKeyPress events even on web content.
  // UI Events defines that "keypress" event should be fired "if and only if
  // that key normally produces a character value".
  // <https://www.w3.org/TR/uievents/#event-type-keypress>
  // Additionally, for backward compatiblity with all existing browsers,
  // there is a spec issue for Enter key press.
  // <https://github.com/w3c/uievents/issues/183>
  bool IsInputtingText() const {
    // NOTE: On some keyboard layout, some characters are inputted with Control
    //       key or Alt key, but at that time, widget clears the modifier flag
    //       from eKeyPress event because our TextEditor won't handle eKeyPress
    //       events as inputting text (bug 1346832).
    // NOTE: There are some complicated issues of our traditional behavior.
    //       -- On Windows, KeyboardLayout::WillDispatchKeyboardEvent() clears
    //       MODIFIER_ALT and MODIFIER_CONTROL of eKeyPress event if it
    //       should be treated as inputting a character because AltGr is
    //       represented with both Alt key and Ctrl key are pressed, and
    //       some keyboard layouts may produces a character with Ctrl key.
    //       -- On Linux, KeymapWrapper doesn't have this hack since perhaps,
    //       we don't have any bug reports that user cannot input proper
    //       character with Alt and/or Ctrl key.
    //       -- On macOS, IMEInputHandler::WillDispatchKeyboardEvent() clears
    //       MODIFIER_ALT and MDOFIEIR_CONTROL of eKeyPress event only when
    //       TextInputHandler::InsertText() has been called for the event.
    //       I.e., they are cleared only when an editor has focus (even if IME
    //       is disabled in password field or by |ime-mode: disabled;|) because
    //       TextInputHandler::InsertText() is called while
    //       TextInputHandler::HandleKeyDownEvent() calls interpretKeyEvents:
    //       to notify text input processor of Cocoa (including IME).  In other
    //       words, when we need to disable IME completey when no editor has
    //       focus, we cannot call interpretKeyEvents:.  So,
    //       TextInputHandler::InsertText() won't be called when no editor has
    //       focus so that neither MODIFIER_ALT nor MODIFIER_CONTROL is
    //       cleared.  So, fortunately, altKey and ctrlKey values of "keypress"
    //       events are same as the other browsers only when no editor has
    //       focus.
    // NOTE: As mentioned above, for compatibility with the other browsers on
    //       macOS, we should keep MODIFIER_ALT and MODIFIER_CONTROL flags of
    //       eKeyPress events when no editor has focus.  However, Alt key,
    //       labeled "option" on keyboard for Mac, is AltGraph key on the other
    //       platforms.  So, even if MODIFIER_ALT is set, we need to dispatch
    //       eKeyPress event even on web content unless mCharCode is 0.
    //       Therefore, we need to ignore MODIFIER_ALT flag here only on macOS.
    return mMessage == eKeyPress && mCharCode &&
           !(mModifiers & (
#ifndef XP_MACOSX
                              // So, ignore MODIFIER_ALT only on macOS since
                              // option key is used as AltGraph key on macOS.
                              MODIFIER_ALT |
#endif  // #ifndef XP_MAXOSX
                              MODIFIER_CONTROL | MODIFIER_META));
  }

  bool IsInputtingLineBreak() const {
    return mMessage == eKeyPress && mKeyNameIndex == KEY_NAME_INDEX_Enter &&
           !(mModifiers & (MODIFIER_ALT | MODIFIER_CONTROL | MODIFIER_META));
  }

  /**
   * ShouldKeyPressEventBeFiredOnContent() should be called only when the
   * instance is eKeyPress event.  This returns true when the eKeyPress
   * event should be fired even on content in the default event group.
   */
  bool ShouldKeyPressEventBeFiredOnContent() const {
    MOZ_DIAGNOSTIC_ASSERT(mMessage == eKeyPress);
    if (IsInputtingText() || IsInputtingLineBreak()) {
      return true;
    }
    // Ctrl + Enter won't cause actual input in our editor.
    // However, the other browsers fire keypress event in any platforms.
    // So, for compatibility with them, we should fire keypress event for
    // Ctrl + Enter too.
    return mMessage == eKeyPress && mKeyNameIndex == KEY_NAME_INDEX_Enter &&
           !(mModifiers & (MODIFIER_ALT | MODIFIER_META | MODIFIER_SHIFT));
  }

  WidgetEvent* Duplicate() const override {
    MOZ_ASSERT(mClass == eKeyboardEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetKeyboardEvent* result = new WidgetKeyboardEvent(
        false, mMessage, nullptr, eKeyboardEventClass, this);
    result->AssignKeyEventData(*this, true);
    result->mEditCommandsForSingleLineEditor =
        mEditCommandsForSingleLineEditor.Clone();
    result->mEditCommandsForMultiLineEditor =
        mEditCommandsForMultiLineEditor.Clone();
    result->mEditCommandsForRichTextEditor =
        mEditCommandsForRichTextEditor.Clone();
    result->mFlags = mFlags;
    return result;
  }

  bool CanUserGestureActivateTarget() const {
    // Printable keys, 'carriage return' and 'space' are supported user gestures
    // for activating the document. However, if supported key is being pressed
    // combining with other operation keys, such like alt, control ..etc., we
    // won't activate the target for them because at that time user might
    // interact with browser or window manager which doesn't necessarily
    // demonstrate user's intent to play media.
    const bool isCombiningWithOperationKeys = (IsControl() && !IsAltGraph()) ||
                                              (IsAlt() && !IsAltGraph()) ||
                                              IsMeta();
    const bool isEnterOrSpaceKey =
        mKeyNameIndex == KEY_NAME_INDEX_Enter || mKeyCode == NS_VK_SPACE;
    return (PseudoCharCode() || isEnterOrSpaceKey) &&
           (!isCombiningWithOperationKeys ||
            // ctrl-c/ctrl-x/ctrl-v is quite common shortcut for clipboard
            // operation.
            // XXXedgar, we have to find a better way to handle browser keyboard
            // shortcut for user activation, instead of just ignoring all
            // combinations, see bug 1641171.
            ((mKeyCode == dom::KeyboardEvent_Binding::DOM_VK_C ||
              mKeyCode == dom::KeyboardEvent_Binding::DOM_VK_V ||
              mKeyCode == dom::KeyboardEvent_Binding::DOM_VK_X) &&
             IsAccel()));
  }

  // Returns true if this event is likely an user activation for a link or
  // a link-like button, where modifier keys are likely be used for controlling
  // where the link is opened.
  //
  // This returns false if the keyboard event is more likely an user-defined
  // shortcut key.
  bool CanReflectModifiersToUserActivation() const {
    MOZ_ASSERT(CanUserGestureActivateTarget(),
               "Consumer should check CanUserGestureActivateTarget first");
    // 'carriage return' and 'space' are supported user gestures for activating
    // a link or a button.
    // A button often behaves like a link, by calling window.open inside its
    // event handler.
    //
    // Access keys can also activate links/buttons, but access keys have their
    // own modifiers, and those modifiers are not appropriate for reflecting to
    // the user activation nor controlling where the link is opened.
    return mKeyNameIndex == KEY_NAME_INDEX_Enter || mKeyCode == NS_VK_SPACE;
  }

  [[nodiscard]] bool ShouldWorkAsSpaceKey() const {
    if (mKeyCode == NS_VK_SPACE) {
      return true;
    }
    // Additionally, if the code value is "Space" and the key is not mapped to
    // a function key (i.e., not a printable key), we should treat it as space
    // key because the active keyboard layout may input different character
    // from the ASCII white space (U+0020).  For example, NBSP (U+00A0).
    return mKeyNameIndex == KEY_NAME_INDEX_USE_STRING &&
           mCodeNameIndex == CODE_NAME_INDEX_Space;
  }

  /**
   * CanTreatAsUserInput() returns true if the key is pressed for perhaps
   * doing something on the web app or our UI.  This means that when this
   * returns false, e.g., when user presses a modifier key, user is probably
   * displeased by opening popup, entering fullscreen mode, etc.  Therefore,
   * only when this returns true, such reactions should be allowed.
   */
  bool CanTreatAsUserInput() const {
    if (!IsTrusted()) {
      return false;
    }
    switch (mKeyNameIndex) {
      case KEY_NAME_INDEX_Escape:
      // modifier keys:
      case KEY_NAME_INDEX_Alt:
      case KEY_NAME_INDEX_AltGraph:
      case KEY_NAME_INDEX_CapsLock:
      case KEY_NAME_INDEX_Control:
      case KEY_NAME_INDEX_Fn:
      case KEY_NAME_INDEX_FnLock:
      case KEY_NAME_INDEX_Meta:
      case KEY_NAME_INDEX_NumLock:
      case KEY_NAME_INDEX_ScrollLock:
      case KEY_NAME_INDEX_Shift:
      case KEY_NAME_INDEX_Symbol:
      case KEY_NAME_INDEX_SymbolLock:
      // legacy modifier keys:
      case KEY_NAME_INDEX_Hyper:
      case KEY_NAME_INDEX_Super:
        return false;
      default:
        return true;
    }
  }

  /**
   * ShouldInteractionTimeRecorded() returns true if the handling time of
   * the event should be recorded with the telemetry.
   */
  bool ShouldInteractionTimeRecorded() const {
    // Let's record only when we can treat the instance is a user input.
    return CanTreatAsUserInput();
  }

  // OS translated Unicode chars which are used for accesskey and accelkey
  // handling. The handlers will try from first character to last character.
  CopyableTArray<AlternativeCharCode> mAlternativeCharCodes;
  // DOM KeyboardEvent.key only when mKeyNameIndex is KEY_NAME_INDEX_USE_STRING.
  nsString mKeyValue;
  // DOM KeyboardEvent.code only when mCodeNameIndex is
  // CODE_NAME_INDEX_USE_STRING.
  nsString mCodeValue;

  // OS-specific native event can optionally be preserved.
  // This is used to retrieve editing shortcut keys in the environment.
  void* mNativeKeyEvent;
  // A DOM keyCode value or 0.  If a keypress event whose mCharCode is 0, this
  // should be 0.
  uint32_t mKeyCode;
  // If the instance is a keypress event of a printable key, this is a UTF-16
  // value of the key.  Otherwise, 0.  This value must not be a control
  // character when some modifiers are active.  Then, this value should be an
  // unmodified value except Shift and AltGr.
  uint32_t mCharCode;
  // mPseudoCharCode is valid only when mMessage is an eKeyDown event.
  // This stores mCharCode value of keypress event which is fired with same
  // key value and same modifier state.
  uint32_t mPseudoCharCode;
  // One of eKeyLocation*
  uint32_t mLocation;
  // Unique id associated with a keydown / keypress event. It's ok if this wraps
  // over long periods.
  uint32_t mUniqueId;

  // DOM KeyboardEvent.key
  KeyNameIndex mKeyNameIndex;
  // DOM KeyboardEvent.code
  CodeNameIndex mCodeNameIndex;

  // Indicates whether the event is generated by auto repeat or not.
  // if this is keyup event, always false.
  bool mIsRepeat;
  // Indicates whether the event is generated during IME (or deadkey)
  // composition.  This is initialized by EventStateManager.  So, key event
  // dispatchers don't need to initialize this.
  bool mIsComposing;
  // Indicates whether the event is synthesized from Text Input Processor
  // or an actual event from nsAppShell.
  bool mIsSynthesizedByTIP;
  // Indicates whether the event is skippable in remote process.
  // Don't refer this member directly when you need to check this.
  // Use CanSkipInRemoteProcess() instead.
  bool mMaybeSkippableInRemoteProcess;
  // Indicates whether the event should return legacy keyCode value and
  // charCode value to web apps (one of them is always 0) or not, when it's
  // an eKeyPress event.
  bool mUseLegacyKeyCodeAndCharCodeValues;

  bool CanSkipInRemoteProcess() const {
    // If this is a repeat event (i.e., generated by auto-repeat feature of
    // the platform), remove process may skip to handle it because of
    // performances reasons..  However, if it's caused by odd keyboard utils,
    // we should not ignore any key events even marked as repeated since
    // generated key sequence may be important to input proper text.  E.g.,
    // "SinhalaTamil IME" on Windows emulates dead key like input with
    // generating WM_KEYDOWN for VK_PACKET (inputting any Unicode characters
    // without keyboard layout information) and VK_BACK (Backspace) to remove
    // previous character(s) and those messages may be marked as "repeat" by
    // their bug.
    return mIsRepeat && mMaybeSkippableInRemoteProcess;
  }

  /**
   * If the key is an arrow key, and the current selection is in a vertical
   * content, the caret should be moved to physically.  However, arrow keys
   * are mapped to logical move commands in horizontal content.  Therefore,
   * we need to check writing mode if and only if the key is an arrow key, and
   * need to remap the command to logical command in vertical content if the
   * writing mode at selection is vertical.  These methods help to convert
   * arrow keys in horizontal content to correspnding direction arrow keys
   * in vertical content.
   */
  bool NeedsToRemapNavigationKey() const {
    // TODO: Use mKeyNameIndex instead.
    return mKeyCode >= NS_VK_LEFT && mKeyCode <= NS_VK_DOWN;
  }

  uint32_t GetRemappedKeyCode(const WritingMode& aWritingMode) const {
    if (!aWritingMode.IsVertical()) {
      return mKeyCode;
    }
    switch (mKeyCode) {
      case NS_VK_LEFT:
        return aWritingMode.IsVerticalLR() ? NS_VK_UP : NS_VK_DOWN;
      case NS_VK_RIGHT:
        return aWritingMode.IsVerticalLR() ? NS_VK_DOWN : NS_VK_UP;
      case NS_VK_UP:
        return NS_VK_LEFT;
      case NS_VK_DOWN:
        return NS_VK_RIGHT;
      default:
        return mKeyCode;
    }
  }

  KeyNameIndex GetRemappedKeyNameIndex(const WritingMode& aWritingMode) const {
    if (!aWritingMode.IsVertical()) {
      return mKeyNameIndex;
    }
    uint32_t remappedKeyCode = GetRemappedKeyCode(aWritingMode);
    if (remappedKeyCode == mKeyCode) {
      return mKeyNameIndex;
    }
    switch (remappedKeyCode) {
      case NS_VK_LEFT:
        return KEY_NAME_INDEX_ArrowLeft;
      case NS_VK_RIGHT:
        return KEY_NAME_INDEX_ArrowRight;
      case NS_VK_UP:
        return KEY_NAME_INDEX_ArrowUp;
      case NS_VK_DOWN:
        return KEY_NAME_INDEX_ArrowDown;
      default:
        MOZ_ASSERT_UNREACHABLE("Add a case for the new remapped key");
        return mKeyNameIndex;
    }
  }

  /**
   * Retrieves all edit commands from mWidget.  This shouldn't be called when
   * the instance is an untrusted event, doesn't have widget or in non-chrome
   * process.
   *
   * @param aWritingMode
   *                    When writing mode of focused element is vertical, this
   *                    will resolve some key's physical direction to logical
   *                    direction.  For doing it, this must be set to the
   *                    writing mode at current selection.  However, when there
   *                    is no focused element and no selection ranges, this
   *                    should be set to Nothing().  Using the result of
   *                    `TextEventDispatcher::MaybeQueryWritingModeAtSelection()`
   *                    is recommended.
   */
  MOZ_CAN_RUN_SCRIPT void InitAllEditCommands(
      const Maybe<WritingMode>& aWritingMode);

  /**
   * Retrieves edit commands from mWidget only for aType.  This shouldn't be
   * called when the instance is an untrusted event or doesn't have widget.
   *
   * @param aWritingMode
   *                    When writing mode of focused element is vertical, this
   *                    will resolve some key's physical direction to logical
   *                    direction.  For doing it, this must be set to the
   *                    writing mode at current selection.  However, when there
   *                    is no focused element and no selection ranges, this
   *                    should be set to Nothing().  Using the result of
   *                    `TextEventDispatcher::MaybeQueryWritingModeAtSelection()`
   *                    is recommended.
   * @return            false if some resource is not available to get
   *                    commands unexpectedly.  Otherwise, true even if
   *                    retrieved command is nothing.
   */
  MOZ_CAN_RUN_SCRIPT bool InitEditCommandsFor(
      NativeKeyBindingsType aType, const Maybe<WritingMode>& aWritingMode);

  /**
   * PreventNativeKeyBindings() makes the instance to not cause any edit
   * actions even if it matches with a native key binding.
   */
  void PreventNativeKeyBindings() {
    mEditCommandsForSingleLineEditor.Clear();
    mEditCommandsForMultiLineEditor.Clear();
    mEditCommandsForRichTextEditor.Clear();
    mEditCommandsForSingleLineEditorInitialized = true;
    mEditCommandsForMultiLineEditorInitialized = true;
    mEditCommandsForRichTextEditorInitialized = true;
  }

  /**
   * EditCommandsConstRef() returns reference to edit commands for aType.
   */
  const nsTArray<CommandInt>& EditCommandsConstRef(
      NativeKeyBindingsType aType) const {
    return const_cast<WidgetKeyboardEvent*>(this)->EditCommandsRef(aType);
  }

  /**
   * IsEditCommandsInitialized() returns true if edit commands for aType
   * was already initialized.  Otherwise, false.
   */
  bool IsEditCommandsInitialized(NativeKeyBindingsType aType) const {
    return const_cast<WidgetKeyboardEvent*>(this)->IsEditCommandsInitializedRef(
        aType);
  }

  /**
   * AreAllEditCommandsInitialized() returns true if edit commands for all
   * types were already initialized.  Otherwise, false.
   */
  bool AreAllEditCommandsInitialized() const {
    return mEditCommandsForSingleLineEditorInitialized &&
           mEditCommandsForMultiLineEditorInitialized &&
           mEditCommandsForRichTextEditorInitialized;
  }

  /**
   * Execute edit commands for aType.
   *
   * @return        true if the caller should do nothing anymore.
   *                false, otherwise.
   */
  typedef void (*DoCommandCallback)(Command, void*);
  MOZ_CAN_RUN_SCRIPT bool ExecuteEditCommands(NativeKeyBindingsType aType,
                                              DoCommandCallback aCallback,
                                              void* aCallbackData);

  // If the key should cause keypress events, this returns true.
  // Otherwise, false.
  bool ShouldCauseKeypressEvents() const;

  // mCharCode value of non-eKeyPress events is always 0.  However, if
  // non-eKeyPress event has one or more alternative char code values,
  // its first item should be the mCharCode value of following eKeyPress event.
  // PseudoCharCode() returns mCharCode value for eKeyPress event,
  // the first alternative char code value of non-eKeyPress event or 0.
  uint32_t PseudoCharCode() const {
    return mMessage == eKeyPress ? mCharCode : mPseudoCharCode;
  }
  void SetCharCode(uint32_t aCharCode) {
    if (mMessage == eKeyPress) {
      mCharCode = aCharCode;
    } else {
      mPseudoCharCode = aCharCode;
    }
  }

  void GetDOMKeyName(nsAString& aKeyName) {
    if (mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
      aKeyName = mKeyValue;
      return;
    }
    GetDOMKeyName(mKeyNameIndex, aKeyName);
  }
  void GetDOMCodeName(nsAString& aCodeName) {
    if (mCodeNameIndex == CODE_NAME_INDEX_USE_STRING) {
      aCodeName = mCodeValue;
      return;
    }
    GetDOMCodeName(mCodeNameIndex, aCodeName);
  }

  /**
   * GetFallbackKeyCodeOfPunctuationKey() returns a DOM keyCode value for
   * aCodeNameIndex.  This is keyCode value of the key when active keyboard
   * layout is ANSI (US), JIS or ABNT keyboard layout (the latter 2 layouts
   * are used only when ANSI doesn't have the key).  The result is useful
   * if the key doesn't produce ASCII character with active keyboard layout
   * nor with alternative ASCII capable keyboard layout.
   */
  static uint32_t GetFallbackKeyCodeOfPunctuationKey(
      CodeNameIndex aCodeNameIndex);

  bool IsModifierKeyEvent() const {
    return GetModifierForKeyName(mKeyNameIndex) != MODIFIER_NONE;
  }

  /**
   * Get the candidates for shortcut key.
   *
   * @param aCandidates [out] the candidate shortcut key combination list.
   *                          the first item is most preferred.
   */
  void GetShortcutKeyCandidates(ShortcutKeyCandidateArray& aCandidates) const;

  /**
   * Get the candidates for access key.
   *
   * @param aCandidates [out] the candidate access key list.
   *                          the first item is most preferred.
   */
  void GetAccessKeyCandidates(nsTArray<uint32_t>& aCandidates) const;

  /**
   * Check whether the modifiers match with chrome access key or
   * content access key.
   */
  bool ModifiersMatchWithAccessKey(AccessKeyType aType) const;

  /**
   * Return active modifiers which may match with access key.
   * For example, even if Alt is access key modifier, then, when Control,
   * CapseLock and NumLock are active, this returns only MODIFIER_CONTROL.
   */
  Modifiers ModifiersForAccessKeyMatching() const;

  /**
   * Return access key modifiers.
   */
  static Modifiers AccessKeyModifiers(AccessKeyType aType);

  static void Shutdown();

  /**
   * ComputeLocationFromCodeValue() returns one of .mLocation value
   * (eKeyLocation*) which is the most preferred value for the specified code
   * value.
   */
  static uint32_t ComputeLocationFromCodeValue(CodeNameIndex aCodeNameIndex);

  /**
   * ComputeKeyCodeFromKeyNameIndex() return a .mKeyCode value which can be
   * mapped from the specified key value.  Note that this returns 0 if the
   * key name index is KEY_NAME_INDEX_Unidentified or KEY_NAME_INDEX_USE_STRING.
   * This means that this method is useful only for non-printable keys.
   */
  static uint32_t ComputeKeyCodeFromKeyNameIndex(KeyNameIndex aKeyNameIndex);

  /**
   * ComputeCodeNameIndexFromKeyNameIndex() returns a code name index which
   * is typically mapped to given key name index on the platform.
   * Note that this returns CODE_NAME_INDEX_UNKNOWN if the key name index is
   * KEY_NAME_INDEX_Unidentified or KEY_NAME_INDEX_USE_STRING.
   * This means that this method is useful only for non-printable keys.
   *
   * @param aKeyNameIndex      A non-printable key name index.
   * @param aLocation          Should be one of location value.  This is
   *                           important when aKeyNameIndex may exist in
   *                           both Numpad or Standard, or in both Left or
   *                           Right.  If this is nothing, this method
   *                           returns Left or Standard position's code
   *                           value.
   */
  static CodeNameIndex ComputeCodeNameIndexFromKeyNameIndex(
      KeyNameIndex aKeyNameIndex, const Maybe<uint32_t>& aLocation);

  /**
   * GetModifierForKeyName() returns a value of Modifier which is activated
   * by the aKeyNameIndex.
   */
  static Modifier GetModifierForKeyName(KeyNameIndex aKeyNameIndex);

  /**
   * IsLeftOrRightModiferKeyNameIndex() returns true if aKeyNameIndex is a
   * modifier key which may be in Left and Right location.
   */
  static bool IsLeftOrRightModiferKeyNameIndex(KeyNameIndex aKeyNameIndex) {
    switch (aKeyNameIndex) {
      case KEY_NAME_INDEX_Alt:
      case KEY_NAME_INDEX_Control:
      case KEY_NAME_INDEX_Meta:
      case KEY_NAME_INDEX_Shift:
        return true;
      default:
        return false;
    }
  }

  /**
   * IsLockableModifier() returns true if aKeyNameIndex is a lockable modifier
   * key such as CapsLock and NumLock.
   */
  static bool IsLockableModifier(KeyNameIndex aKeyNameIndex);

  static void GetDOMKeyName(KeyNameIndex aKeyNameIndex, nsAString& aKeyName);
  static void GetDOMCodeName(CodeNameIndex aCodeNameIndex,
                             nsAString& aCodeName);

  static KeyNameIndex GetKeyNameIndex(const nsAString& aKeyValue);
  static CodeNameIndex GetCodeNameIndex(const nsAString& aCodeValue);

  static const char* GetCommandStr(Command aCommand);

  void AssignKeyEventData(const WidgetKeyboardEvent& aEvent,
                          bool aCopyTargets) {
    AssignInputEventData(aEvent, aCopyTargets);

    mKeyCode = aEvent.mKeyCode;
    mCharCode = aEvent.mCharCode;
    mPseudoCharCode = aEvent.mPseudoCharCode;
    mLocation = aEvent.mLocation;
    mAlternativeCharCodes = aEvent.mAlternativeCharCodes.Clone();
    mIsRepeat = aEvent.mIsRepeat;
    mIsComposing = aEvent.mIsComposing;
    mKeyNameIndex = aEvent.mKeyNameIndex;
    mCodeNameIndex = aEvent.mCodeNameIndex;
    mKeyValue = aEvent.mKeyValue;
    mCodeValue = aEvent.mCodeValue;
    // Don't copy mNativeKeyEvent because it may be referred after its instance
    // is destroyed.
    mNativeKeyEvent = nullptr;
    mUniqueId = aEvent.mUniqueId;
    mIsSynthesizedByTIP = aEvent.mIsSynthesizedByTIP;
    mMaybeSkippableInRemoteProcess = aEvent.mMaybeSkippableInRemoteProcess;
    mUseLegacyKeyCodeAndCharCodeValues =
        aEvent.mUseLegacyKeyCodeAndCharCodeValues;

    // Don't copy mEditCommandsFor*Editor because it may require a lot of
    // memory space.  For example, if the event is dispatched but grabbed by
    // a JS variable, they are not necessary anymore.

    mEditCommandsForSingleLineEditorInitialized =
        aEvent.mEditCommandsForSingleLineEditorInitialized;
    mEditCommandsForMultiLineEditorInitialized =
        aEvent.mEditCommandsForMultiLineEditorInitialized;
    mEditCommandsForRichTextEditorInitialized =
        aEvent.mEditCommandsForRichTextEditorInitialized;
  }

  void AssignCommands(const WidgetKeyboardEvent& aEvent) {
    mEditCommandsForSingleLineEditorInitialized =
        aEvent.mEditCommandsForSingleLineEditorInitialized;
    if (mEditCommandsForSingleLineEditorInitialized) {
      mEditCommandsForSingleLineEditor =
          aEvent.mEditCommandsForSingleLineEditor.Clone();
    } else {
      mEditCommandsForSingleLineEditor.Clear();
    }
    mEditCommandsForMultiLineEditorInitialized =
        aEvent.mEditCommandsForMultiLineEditorInitialized;
    if (mEditCommandsForMultiLineEditorInitialized) {
      mEditCommandsForMultiLineEditor =
          aEvent.mEditCommandsForMultiLineEditor.Clone();
    } else {
      mEditCommandsForMultiLineEditor.Clear();
    }
    mEditCommandsForRichTextEditorInitialized =
        aEvent.mEditCommandsForRichTextEditorInitialized;
    if (mEditCommandsForRichTextEditorInitialized) {
      mEditCommandsForRichTextEditor =
          aEvent.mEditCommandsForRichTextEditor.Clone();
    } else {
      mEditCommandsForRichTextEditor.Clear();
    }
  }

 private:
  static const char16_t* const kKeyNames[];
  static const char16_t* const kCodeNames[];
  typedef nsTHashMap<nsStringHashKey, KeyNameIndex> KeyNameIndexHashtable;
  typedef nsTHashMap<nsStringHashKey, CodeNameIndex> CodeNameIndexHashtable;
  static KeyNameIndexHashtable* sKeyNameIndexHashtable;
  static CodeNameIndexHashtable* sCodeNameIndexHashtable;

  // mEditCommandsFor*Editor store edit commands.  This should be initialized
  // with InitEditCommandsFor().
  // XXX Ideally, this should be array of Command rather than CommandInt.
  //     However, ParamTraits isn't aware of enum array.
  CopyableTArray<CommandInt> mEditCommandsForSingleLineEditor;
  CopyableTArray<CommandInt> mEditCommandsForMultiLineEditor;
  CopyableTArray<CommandInt> mEditCommandsForRichTextEditor;

  nsTArray<CommandInt>& EditCommandsRef(NativeKeyBindingsType aType) {
    switch (aType) {
      case NativeKeyBindingsType::SingleLineEditor:
        return mEditCommandsForSingleLineEditor;
      case NativeKeyBindingsType::MultiLineEditor:
        return mEditCommandsForMultiLineEditor;
      case NativeKeyBindingsType::RichTextEditor:
        return mEditCommandsForRichTextEditor;
      default:
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE(
            "Invalid native key binding type");
    }
  }

  // mEditCommandsFor*EditorInitialized are set to true when
  // InitEditCommandsFor() initializes edit commands for the type.
  bool mEditCommandsForSingleLineEditorInitialized;
  bool mEditCommandsForMultiLineEditorInitialized;
  bool mEditCommandsForRichTextEditorInitialized;

  bool& IsEditCommandsInitializedRef(NativeKeyBindingsType aType) {
    switch (aType) {
      case NativeKeyBindingsType::SingleLineEditor:
        return mEditCommandsForSingleLineEditorInitialized;
      case NativeKeyBindingsType::MultiLineEditor:
        return mEditCommandsForMultiLineEditorInitialized;
      case NativeKeyBindingsType::RichTextEditor:
        return mEditCommandsForRichTextEditorInitialized;
      default:
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE(
            "Invalid native key binding type");
    }
  }
};

/******************************************************************************
 * mozilla::WidgetCompositionEvent
 ******************************************************************************/

class WidgetCompositionEvent : public WidgetGUIEvent {
 private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;
  ALLOW_DEPRECATED_READPARAM

  WidgetCompositionEvent() : mOriginalMessage(eVoidEvent) {}

 public:
  virtual WidgetCompositionEvent* AsCompositionEvent() override { return this; }

  WidgetCompositionEvent(bool aIsTrusted, EventMessage aMessage,
                         nsIWidget* aWidget,
                         const WidgetEventTime* aTime = nullptr)
      : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eCompositionEventClass,
                       aTime),
        mNativeIMEContext(aWidget),
        mOriginalMessage(eVoidEvent) {}

  virtual WidgetEvent* Duplicate() const override {
    MOZ_ASSERT(mClass == eCompositionEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetCompositionEvent* result =
        new WidgetCompositionEvent(false, mMessage, nullptr, this);
    result->AssignCompositionEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  // The composition string or the commit string.  If the instance is a
  // compositionstart event, this is initialized with selected text by
  // TextComposition automatically.
  nsString mData;

  RefPtr<TextRangeArray> mRanges;

  // mNativeIMEContext stores the native IME context which causes the
  // composition event.
  widget::NativeIMEContext mNativeIMEContext;

  // If the instance is a clone of another event, mOriginalMessage stores
  // the another event's mMessage.
  EventMessage mOriginalMessage;

  // Composition ID considered by TextComposition.  If the event has not been
  // handled by TextComposition yet, this is 0.  And also if the event is for
  // a composition synthesized in a content process, this is always 0.
  uint32_t mCompositionId = 0;

  void AssignCompositionEventData(const WidgetCompositionEvent& aEvent,
                                  bool aCopyTargets) {
    AssignGUIEventData(aEvent, aCopyTargets);

    mData = aEvent.mData;
    mOriginalMessage = aEvent.mOriginalMessage;
    mRanges = aEvent.mRanges;

    // Currently, we don't need to copy the other members because they are
    // for internal use only (not available from JS).
  }

  bool IsComposing() const { return mRanges && mRanges->IsComposing(); }

  uint32_t TargetClauseOffset() const {
    return mRanges ? mRanges->TargetClauseOffset() : 0;
  }

  uint32_t TargetClauseLength() const {
    uint32_t length = UINT32_MAX;
    if (mRanges) {
      length = mRanges->TargetClauseLength();
    }
    return length == UINT32_MAX ? mData.Length() : length;
  }

  uint32_t RangeCount() const { return mRanges ? mRanges->Length() : 0; }

  bool CausesDOMTextEvent() const {
    return mMessage == eCompositionChange || mMessage == eCompositionCommit ||
           mMessage == eCompositionCommitAsIs;
  }

  bool CausesDOMCompositionEndEvent() const {
    return mMessage == eCompositionEnd || mMessage == eCompositionCommit ||
           mMessage == eCompositionCommitAsIs;
  }

  bool IsFollowedByCompositionEnd() const {
    return IsFollowedByCompositionEnd(mOriginalMessage);
  }

  static bool IsFollowedByCompositionEnd(EventMessage aEventMessage) {
    return aEventMessage == eCompositionCommit ||
           aEventMessage == eCompositionCommitAsIs;
  }
};

/******************************************************************************
 * mozilla::WidgetQueryContentEvent
 ******************************************************************************/

class WidgetQueryContentEvent : public WidgetGUIEvent {
 private:
  friend class dom::PBrowserParent;
  friend class dom::PBrowserChild;
  ALLOW_DEPRECATED_READPARAM

  WidgetQueryContentEvent()
      : mUseNativeLineBreak(true),
        mWithFontRanges(false),
        mNeedsToFlushLayout(true) {
    MOZ_CRASH("WidgetQueryContentEvent is created without proper arguments");
  }

 public:
  virtual WidgetQueryContentEvent* AsQueryContentEvent() override {
    return this;
  }

  WidgetQueryContentEvent(bool aIsTrusted, EventMessage aMessage,
                          nsIWidget* aWidget)
      : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eQueryContentEventClass),
        mUseNativeLineBreak(true),
        mWithFontRanges(false),
        mNeedsToFlushLayout(true) {}

  WidgetQueryContentEvent(EventMessage aMessage,
                          const WidgetQueryContentEvent& aOtherEvent)
      : WidgetGUIEvent(aOtherEvent.IsTrusted(), aMessage,
                       const_cast<nsIWidget*>(aOtherEvent.mWidget.get()),
                       eQueryContentEventClass),
        mUseNativeLineBreak(aOtherEvent.mUseNativeLineBreak),
        mWithFontRanges(false),
        mNeedsToFlushLayout(aOtherEvent.mNeedsToFlushLayout) {}

  WidgetEvent* Duplicate() const override {
    // This event isn't an internal event of any DOM event.
    NS_ASSERTION(!IsAllowedToDispatchDOMEvent(),
                 "WidgetQueryContentEvent needs to support Duplicate()");
    MOZ_CRASH("WidgetQueryContentEvent doesn't support Duplicate()");
  }

  struct Options final {
    bool mUseNativeLineBreak;
    bool mRelativeToInsertionPoint;

    explicit Options()
        : mUseNativeLineBreak(true), mRelativeToInsertionPoint(false) {}

    explicit Options(const WidgetQueryContentEvent& aEvent)
        : mUseNativeLineBreak(aEvent.mUseNativeLineBreak),
          mRelativeToInsertionPoint(aEvent.mInput.mRelativeToInsertionPoint) {}
  };

  void Init(const Options& aOptions) {
    mUseNativeLineBreak = aOptions.mUseNativeLineBreak;
    mInput.mRelativeToInsertionPoint = aOptions.mRelativeToInsertionPoint;
    MOZ_ASSERT(mInput.IsValidEventMessage(mMessage));
  }

  void InitForQueryTextContent(int64_t aOffset, uint32_t aLength,
                               const Options& aOptions = Options()) {
    NS_ASSERTION(mMessage == eQueryTextContent, "wrong initializer is called");
    mInput.mOffset = aOffset;
    mInput.mLength = aLength;
    Init(aOptions);
    MOZ_ASSERT(mInput.IsValidOffset());
  }

  void InitForQueryCaretRect(int64_t aOffset,
                             const Options& aOptions = Options()) {
    NS_ASSERTION(mMessage == eQueryCaretRect, "wrong initializer is called");
    mInput.mOffset = aOffset;
    Init(aOptions);
    MOZ_ASSERT(mInput.IsValidOffset());
  }

  void InitForQueryTextRect(int64_t aOffset, uint32_t aLength,
                            const Options& aOptions = Options()) {
    NS_ASSERTION(mMessage == eQueryTextRect, "wrong initializer is called");
    mInput.mOffset = aOffset;
    mInput.mLength = aLength;
    Init(aOptions);
    MOZ_ASSERT(mInput.IsValidOffset());
  }

  void InitForQuerySelectedText(SelectionType aSelectionType,
                                const Options& aOptions = Options()) {
    MOZ_ASSERT(mMessage == eQuerySelectedText);
    MOZ_ASSERT(aSelectionType != SelectionType::eNone);
    mInput.mSelectionType = aSelectionType;
    Init(aOptions);
  }

  void InitForQueryDOMWidgetHittest(
      const mozilla::LayoutDeviceIntPoint& aPoint) {
    NS_ASSERTION(mMessage == eQueryDOMWidgetHittest,
                 "wrong initializer is called");
    mRefPoint = aPoint;
  }

  void InitForQueryTextRectArray(uint32_t aOffset, uint32_t aLength,
                                 const Options& aOptions = Options()) {
    NS_ASSERTION(mMessage == eQueryTextRectArray,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
    mInput.mLength = aLength;
    Init(aOptions);
  }

  void RequestFontRanges() {
    MOZ_ASSERT(mMessage == eQueryTextContent);
    mWithFontRanges = true;
  }

  bool Succeeded() const {
    if (mReply.isNothing()) {
      return false;
    }
    switch (mMessage) {
      case eQueryTextContent:
      case eQueryTextRect:
      case eQueryCaretRect:
        return mReply->mOffsetAndData.isSome();
      default:
        return true;
    }
  }

  bool Failed() const { return !Succeeded(); }

  bool FoundSelection() const {
    MOZ_ASSERT(mMessage == eQuerySelectedText);
    return Succeeded() && mReply->mOffsetAndData.isSome();
  }

  bool FoundChar() const {
    MOZ_ASSERT(mMessage == eQueryCharacterAtPoint);
    return Succeeded() && mReply->mOffsetAndData.isSome();
  }

  bool FoundTentativeCaretOffset() const {
    MOZ_ASSERT(mMessage == eQueryCharacterAtPoint);
    return Succeeded() && mReply->mTentativeCaretOffset.isSome();
  }

  bool DidNotFindSelection() const {
    MOZ_ASSERT(mMessage == eQuerySelectedText);
    return Failed() || mReply->mOffsetAndData.isNothing();
  }

  bool DidNotFindChar() const {
    MOZ_ASSERT(mMessage == eQueryCharacterAtPoint);
    return Failed() || mReply->mOffsetAndData.isNothing();
  }

  bool DidNotFindTentativeCaretOffset() const {
    MOZ_ASSERT(mMessage == eQueryCharacterAtPoint);
    return Failed() || mReply->mTentativeCaretOffset.isNothing();
  }

  bool mUseNativeLineBreak;
  bool mWithFontRanges;
  bool mNeedsToFlushLayout;
  struct Input final {
    uint32_t EndOffset() const {
      CheckedInt<uint32_t> endOffset = CheckedInt<uint32_t>(mOffset) + mLength;
      return NS_WARN_IF(!endOffset.isValid()) ? UINT32_MAX : endOffset.value();
    }

    int64_t mOffset;
    uint32_t mLength;
    SelectionType mSelectionType;
    // If mOffset is true, mOffset is relative to the start offset of
    // composition if there is, otherwise, the start of the first selection
    // range.
    bool mRelativeToInsertionPoint;

    Input()
        : mOffset(0),
          mLength(0),
          mSelectionType(SelectionType::eNormal),
          mRelativeToInsertionPoint(false) {}

    bool IsValidOffset() const {
      return mRelativeToInsertionPoint || mOffset >= 0;
    }
    bool IsValidEventMessage(EventMessage aEventMessage) const {
      if (!mRelativeToInsertionPoint) {
        return true;
      }
      switch (aEventMessage) {
        case eQueryTextContent:
        case eQueryCaretRect:
        case eQueryTextRect:
          return true;
        default:
          return false;
      }
    }
    bool MakeOffsetAbsolute(uint32_t aInsertionPointOffset) {
      if (NS_WARN_IF(!mRelativeToInsertionPoint)) {
        return true;
      }
      mRelativeToInsertionPoint = false;
      // If mOffset + aInsertionPointOffset becomes negative value,
      // we should assume the absolute offset is 0.
      if (mOffset < 0 && -mOffset > aInsertionPointOffset) {
        mOffset = 0;
        return true;
      }
      // Otherwise, we don't allow too large offset.
      CheckedInt<uint32_t> absOffset(mOffset + aInsertionPointOffset);
      if (NS_WARN_IF(!absOffset.isValid())) {
        mOffset = UINT32_MAX;
        return false;
      }
      mOffset = absOffset.value();
      return true;
    }
  } mInput;

  struct Reply final {
    EventMessage const mEventMessage;
    void* mContentsRoot = nullptr;
    Maybe<OffsetAndData<uint32_t>> mOffsetAndData;
    // mTentativeCaretOffset is used by only eQueryCharacterAtPoint.
    // This is the offset where caret would be if user clicked at the mRefPoint.
    Maybe<uint32_t> mTentativeCaretOffset;
    // mRect is used by eQueryTextRect, eQueryCaretRect, eQueryCharacterAtPoint
    // and eQueryEditorRect. The coordinates is system coordinates relative to
    // the top level widget of mFocusedWidget.  E.g., if a <xul:panel> which
    // is owned by a window has focused editor, the offset of mRect is relative
    // to the owner window, not the <xul:panel>.
    mozilla::LayoutDeviceIntRect mRect;
    // The return widget has the caret. This is set at all query events.
    nsIWidget* mFocusedWidget = nullptr;
    // mozilla::WritingMode value at the end (focus) of the selection
    mozilla::WritingMode mWritingMode;
    // Used by eQuerySelectionAsTransferable
    nsCOMPtr<nsITransferable> mTransferable;
    // Used by eQueryTextContent with font ranges requested
    CopyableAutoTArray<mozilla::FontRange, 1> mFontRanges;
    // Used by eQueryTextRectArray
    CopyableTArray<mozilla::LayoutDeviceIntRect> mRectArray;
    // true if selection is reversed (end < start)
    bool mReversed = false;
    // true if DOM element under mouse belongs to widget
    bool mWidgetIsHit = false;
    // true if mContentRoot is focused editable content
    bool mIsEditableContent = false;
    // Set to the element that a drop at the given coordinates would target
    mozilla::dom::Element* mDropElement;
    // Set to the frame that a drop at the given coordinates would target
    nsIFrame* mDropFrame;

    Reply() = delete;
    explicit Reply(EventMessage aEventMessage) : mEventMessage(aEventMessage) {}

    // Don't allow to copy/move because of `mEventMessage`.
    Reply(const Reply& aOther) = delete;
    Reply(Reply&& aOther) = delete;
    Reply& operator=(const Reply& aOther) = delete;
    Reply& operator=(Reply&& aOther) = delete;

    MOZ_NEVER_INLINE_DEBUG uint32_t StartOffset() const {
      MOZ_ASSERT(mOffsetAndData.isSome());
      return mOffsetAndData->StartOffset();
    }
    MOZ_NEVER_INLINE_DEBUG uint32_t EndOffset() const {
      MOZ_ASSERT(mOffsetAndData.isSome());
      return mOffsetAndData->EndOffset();
    }
    MOZ_NEVER_INLINE_DEBUG uint32_t DataLength() const {
      MOZ_ASSERT(mOffsetAndData.isSome() ||
                 mEventMessage == eQuerySelectedText);
      return mOffsetAndData.isSome() ? mOffsetAndData->Length() : 0;
    }
    MOZ_NEVER_INLINE_DEBUG uint32_t AnchorOffset() const {
      MOZ_ASSERT(mEventMessage == eQuerySelectedText);
      MOZ_ASSERT(mOffsetAndData.isSome());
      return StartOffset() + (mReversed ? DataLength() : 0);
    }

    MOZ_NEVER_INLINE_DEBUG uint32_t FocusOffset() const {
      MOZ_ASSERT(mEventMessage == eQuerySelectedText);
      MOZ_ASSERT(mOffsetAndData.isSome());
      return StartOffset() + (mReversed ? 0 : DataLength());
    }

    const WritingMode& WritingModeRef() const {
      MOZ_ASSERT(mEventMessage == eQuerySelectedText ||
                 mEventMessage == eQueryCaretRect ||
                 mEventMessage == eQueryTextRect);
      MOZ_ASSERT(mOffsetAndData.isSome() ||
                 mEventMessage == eQuerySelectedText);
      return mWritingMode;
    }

    MOZ_NEVER_INLINE_DEBUG const nsString& DataRef() const {
      MOZ_ASSERT(mOffsetAndData.isSome() ||
                 mEventMessage == eQuerySelectedText);
      return mOffsetAndData.isSome() ? mOffsetAndData->DataRef()
                                     : EmptyString();
    }
    MOZ_NEVER_INLINE_DEBUG bool IsDataEmpty() const {
      MOZ_ASSERT(mOffsetAndData.isSome() ||
                 mEventMessage == eQuerySelectedText);
      return mOffsetAndData.isSome() ? mOffsetAndData->IsDataEmpty() : true;
    }
    MOZ_NEVER_INLINE_DEBUG bool IsOffsetInRange(uint32_t aOffset) const {
      MOZ_ASSERT(mOffsetAndData.isSome() ||
                 mEventMessage == eQuerySelectedText);
      return mOffsetAndData.isSome() ? mOffsetAndData->IsOffsetInRange(aOffset)
                                     : false;
    }
    MOZ_NEVER_INLINE_DEBUG bool IsOffsetInRangeOrEndOffset(
        uint32_t aOffset) const {
      MOZ_ASSERT(mOffsetAndData.isSome() ||
                 mEventMessage == eQuerySelectedText);
      return mOffsetAndData.isSome()
                 ? mOffsetAndData->IsOffsetInRangeOrEndOffset(aOffset)
                 : false;
    }
    MOZ_NEVER_INLINE_DEBUG void TruncateData(uint32_t aLength = 0) {
      MOZ_ASSERT(mOffsetAndData.isSome());
      mOffsetAndData->TruncateData(aLength);
    }

    friend std::ostream& operator<<(std::ostream& aStream,
                                    const Reply& aReply) {
      aStream << "{ ";
      if (aReply.mEventMessage == eQuerySelectedText ||
          aReply.mEventMessage == eQueryTextContent ||
          aReply.mEventMessage == eQueryTextRect ||
          aReply.mEventMessage == eQueryCaretRect ||
          aReply.mEventMessage == eQueryCharacterAtPoint) {
        aStream << "mOffsetAndData=" << ToString(aReply.mOffsetAndData).c_str()
                << ", ";
        if (aReply.mEventMessage == eQueryCharacterAtPoint) {
          aStream << "mTentativeCaretOffset="
                  << ToString(aReply.mTentativeCaretOffset).c_str() << ", ";
        }
      }
      if (aReply.mOffsetAndData.isSome() && aReply.mOffsetAndData->Length()) {
        if (aReply.mEventMessage == eQuerySelectedText) {
          aStream << ", mReversed=" << (aReply.mReversed ? "true" : "false");
        }
        if (aReply.mEventMessage == eQuerySelectionAsTransferable) {
          aStream << ", mTransferable=0x" << aReply.mTransferable;
        }
      }
      if (aReply.mEventMessage == eQuerySelectedText ||
          aReply.mEventMessage == eQueryTextRect ||
          aReply.mEventMessage == eQueryCaretRect) {
        aStream << ", mWritingMode=" << ToString(aReply.mWritingMode).c_str();
      }
      aStream << ", mContentsRoot=0x" << aReply.mContentsRoot
              << ", mIsEditableContent="
              << (aReply.mIsEditableContent ? "true" : "false")
              << ", mFocusedWidget=0x" << aReply.mFocusedWidget;
      if (aReply.mEventMessage == eQueryTextContent) {
        aStream << ", mFontRanges={ Length()=" << aReply.mFontRanges.Length()
                << " }";
      } else if (aReply.mEventMessage == eQueryTextRect ||
                 aReply.mEventMessage == eQueryCaretRect ||
                 aReply.mEventMessage == eQueryCharacterAtPoint) {
        aStream << ", mRect=" << ToString(aReply.mRect).c_str();
      } else if (aReply.mEventMessage == eQueryTextRectArray) {
        aStream << ", mRectArray={ Length()=" << aReply.mRectArray.Length()
                << " }";
      } else if (aReply.mEventMessage == eQueryDOMWidgetHittest) {
        aStream << ", mWidgetIsHit="
                << (aReply.mWidgetIsHit ? "true" : "false");
      }
      return aStream << " }";
    }
  };

  void EmplaceReply() { mReply.emplace(mMessage); }
  Maybe<Reply> mReply;

  // values of mComputedScrollAction
  enum { SCROLL_ACTION_NONE, SCROLL_ACTION_LINE, SCROLL_ACTION_PAGE };
};

/******************************************************************************
 * mozilla::WidgetSelectionEvent
 ******************************************************************************/

class WidgetSelectionEvent : public WidgetGUIEvent {
 private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;
  ALLOW_DEPRECATED_READPARAM

  WidgetSelectionEvent()
      : mOffset(0),
        mLength(0),
        mReversed(false),
        mExpandToClusterBoundary(true),
        mSucceeded(false),
        mUseNativeLineBreak(true),
        mReason(nsISelectionListener::NO_REASON) {}

 public:
  virtual WidgetSelectionEvent* AsSelectionEvent() override { return this; }

  WidgetSelectionEvent(bool aIsTrusted, EventMessage aMessage,
                       nsIWidget* aWidget)
      : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eSelectionEventClass),
        mOffset(0),
        mLength(0),
        mReversed(false),
        mExpandToClusterBoundary(true),
        mSucceeded(false),
        mUseNativeLineBreak(true),
        mReason(nsISelectionListener::NO_REASON) {}

  virtual WidgetEvent* Duplicate() const override {
    // This event isn't an internal event of any DOM event.
    NS_ASSERTION(!IsAllowedToDispatchDOMEvent(),
                 "WidgetSelectionEvent needs to support Duplicate()");
    MOZ_CRASH("WidgetSelectionEvent doesn't support Duplicate()");
    return nullptr;
  }

  // Start offset of selection
  uint32_t mOffset;
  // Length of selection
  uint32_t mLength;
  // Selection "anchor" should be in front
  bool mReversed;
  // Cluster-based or character-based
  bool mExpandToClusterBoundary;
  // true if setting selection succeeded.
  bool mSucceeded;
  // true if native line breaks are used for mOffset and mLength
  bool mUseNativeLineBreak;
  // Fennec provides eSetSelection reason codes for downstream
  // use in AccessibleCaret visibility logic.
  int16_t mReason;
};

/******************************************************************************
 * mozilla::InternalEditorInputEvent
 ******************************************************************************/

class InternalEditorInputEvent : public InternalUIEvent {
 public:
  InternalEditorInputEvent() = delete;
  virtual InternalEditorInputEvent* AsEditorInputEvent() override {
    return this;
  }

  InternalEditorInputEvent(bool aIsTrusted, EventMessage aMessage,
                           nsIWidget* aWidget = nullptr,
                           const WidgetEventTime* aTime = nullptr)
      : InternalUIEvent(aIsTrusted, aMessage, aWidget, eEditorInputEventClass,
                        aTime) {}

  virtual WidgetEvent* Duplicate() const override {
    MOZ_ASSERT(mClass == eEditorInputEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    InternalEditorInputEvent* result =
        new InternalEditorInputEvent(false, mMessage, nullptr, this);
    result->AssignEditorInputEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  nsString mData = VoidString();
  RefPtr<dom::DataTransfer> mDataTransfer;
  OwningNonNullStaticRangeArray mTargetRanges;

  EditorInputType mInputType = EditorInputType::eUnknown;

  bool mIsComposing = false;

  void AssignEditorInputEventData(const InternalEditorInputEvent& aEvent,
                                  bool aCopyTargets) {
    AssignUIEventData(aEvent, aCopyTargets);

    mData = aEvent.mData;
    mDataTransfer = aEvent.mDataTransfer;
    mTargetRanges = aEvent.mTargetRanges.Clone();
    mInputType = aEvent.mInputType;
    mIsComposing = aEvent.mIsComposing;
  }

  void GetDOMInputTypeName(nsAString& aInputTypeName) {
    GetDOMInputTypeName(mInputType, aInputTypeName);
  }
  static void GetDOMInputTypeName(EditorInputType aInputType,
                                  nsAString& aInputTypeName);
  static EditorInputType GetEditorInputType(const nsAString& aInputType);

  static void Shutdown();

 private:
  static const char16_t* const kInputTypeNames[];
  using InputTypeHashtable = nsTHashMap<nsStringHashKey, EditorInputType>;
  static InputTypeHashtable* sInputTypeHashtable;
};

/******************************************************************************
 * mozilla::InternalLegacyTextEvent
 ******************************************************************************/

class InternalLegacyTextEvent : public InternalUIEvent {
 public:
  InternalLegacyTextEvent() = delete;

  virtual InternalLegacyTextEvent* AsLegacyTextEvent() override { return this; }

  InternalLegacyTextEvent(bool aIsTrusted, EventMessage aMessage,
                          nsIWidget* aWidget = nullptr,
                          const WidgetEventTime* aTime = nullptr)
      : InternalUIEvent(aIsTrusted, aMessage, aWidget, eLegacyTextEventClass,
                        aTime) {}

  virtual WidgetEvent* Duplicate() const override {
    MOZ_ASSERT(mClass == eLegacyTextEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    InternalLegacyTextEvent* result =
        new InternalLegacyTextEvent(false, mMessage, nullptr, this);
    result->AssignLegacyTextEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  nsString mData;
  RefPtr<dom::DataTransfer> mDataTransfer;
  EditorInputType mInputType = EditorInputType::eUnknown;

  void AssignLegacyTextEventData(const InternalLegacyTextEvent& aEvent,
                                 bool aCopyTargets) {
    AssignUIEventData(aEvent, aCopyTargets);

    mData = aEvent.mData;
    mDataTransfer = aEvent.mDataTransfer;
    mInputType = aEvent.mInputType;
  }
};

}  // namespace mozilla

#endif  // mozilla_TextEvents_h__
