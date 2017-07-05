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
#include "mozilla/EventForwards.h" // for KeyNameIndex, temporarily
#include "mozilla/TextRange.h"
#include "mozilla/FontRange.h"
#include "nsCOMPtr.h"
#include "nsIDOMKeyEvent.h"
#include "nsISelectionController.h"
#include "nsISelectionListener.h"
#include "nsITransferable.h"
#include "nsRect.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "WritingModes.h"

class nsStringHashKey;
template<class, class> class nsDataHashtable;

/******************************************************************************
 * virtual keycode values
 ******************************************************************************/

enum
{
#define NS_DEFINE_VK(aDOMKeyName, aDOMKeyCode) NS_##aDOMKeyName = aDOMKeyCode,
#include "mozilla/VirtualKeyCodeList.h"
#undef NS_DEFINE_VK
  NS_VK_UNKNOWN = 0xFF
};

namespace mozilla {

enum : uint32_t
{
  eKeyLocationStandard = nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD,
  eKeyLocationLeft     = nsIDOMKeyEvent::DOM_KEY_LOCATION_LEFT,
  eKeyLocationRight    = nsIDOMKeyEvent::DOM_KEY_LOCATION_RIGHT,
  eKeyLocationNumpad   = nsIDOMKeyEvent::DOM_KEY_LOCATION_NUMPAD
};

const nsCString GetDOMKeyCodeName(uint32_t aKeyCode);

namespace dom {
  class PBrowserParent;
  class PBrowserChild;
} // namespace dom
namespace plugins {
  class PPluginInstanceChild;
} // namespace plugins

/******************************************************************************
 * mozilla::AlternativeCharCode
 *
 * This stores alternative charCode values of a key event with some modifiers.
 * The stored values proper for testing shortcut key or access key.
 ******************************************************************************/

struct AlternativeCharCode
{
  AlternativeCharCode() :
    mUnshiftedCharCode(0), mShiftedCharCode(0)
  {
  }
  AlternativeCharCode(uint32_t aUnshiftedCharCode, uint32_t aShiftedCharCode) :
    mUnshiftedCharCode(aUnshiftedCharCode), mShiftedCharCode(aShiftedCharCode)
  {
  }
  uint32_t mUnshiftedCharCode;
  uint32_t mShiftedCharCode;
};

/******************************************************************************
 * mozilla::ShortcutKeyCandidate
 *
 * This stores a candidate of shortcut key combination.
 ******************************************************************************/

struct ShortcutKeyCandidate
{
  ShortcutKeyCandidate()
    : mCharCode(0)
    , mIgnoreShift(0)
  {
  }
  ShortcutKeyCandidate(uint32_t aCharCode, bool aIgnoreShift)
    : mCharCode(aCharCode)
    , mIgnoreShift(aIgnoreShift)
  {
  }
  // The mCharCode value which must match keyboard shortcut definition.
  uint32_t mCharCode;
  // true if Shift state can be ignored.  Otherwise, Shift key state must
  // match keyboard shortcut definition.
  bool mIgnoreShift;
};

/******************************************************************************
 * mozilla::IgnoreModifierState
 *
 * This stores flags for modifiers that should be ignored when matching
 * XBL handlers.
 ******************************************************************************/

struct IgnoreModifierState
{
  // When mShift is true, Shift key state will be ignored.
  bool mShift;
  // When mOS is true, OS key state will be ignored.
  bool mOS;

  IgnoreModifierState()
    : mShift(false)
    , mOS(false)
  {
  }
};

/******************************************************************************
 * mozilla::WidgetKeyboardEvent
 ******************************************************************************/

class WidgetKeyboardEvent : public WidgetInputEvent
{
private:
  friend class dom::PBrowserParent;
  friend class dom::PBrowserChild;
  friend struct IPC::ParamTraits<WidgetKeyboardEvent>;

protected:
  WidgetKeyboardEvent()
    : mNativeKeyEvent(nullptr)
    , mKeyCode(0)
    , mCharCode(0)
    , mPseudoCharCode(0)
    , mLocation(eKeyLocationStandard)
    , mAccessKeyForwardedToChild(false)
    , mUniqueId(0)
#ifdef XP_MACOSX
    , mNativeModifierFlags(0)
    , mNativeKeyCode(0)
#endif // #ifdef XP_MACOSX
    , mKeyNameIndex(KEY_NAME_INDEX_Unidentified)
    , mCodeNameIndex(CODE_NAME_INDEX_UNKNOWN)
    , mIsRepeat(false)
    , mIsComposing(false)
    , mIsSynthesizedByTIP(false)
    , mEditCommandsForSingleLineEditorInitialized(false)
    , mEditCommandsForMultiLineEditorInitialized(false)
    , mEditCommandsForRichTextEditorInitialized(false)
  {
  }

public:
  virtual WidgetKeyboardEvent* AsKeyboardEvent() override { return this; }

  WidgetKeyboardEvent(bool aIsTrusted, EventMessage aMessage,
                      nsIWidget* aWidget,
                      EventClassID aEventClassID = eKeyboardEventClass)
    : WidgetInputEvent(aIsTrusted, aMessage, aWidget, aEventClassID)
    , mNativeKeyEvent(nullptr)
    , mKeyCode(0)
    , mCharCode(0)
    , mPseudoCharCode(0)
    , mLocation(eKeyLocationStandard)
    , mAccessKeyForwardedToChild(false)
    , mUniqueId(0)
#ifdef XP_MACOSX
    , mNativeModifierFlags(0)
    , mNativeKeyCode(0)
#endif // #ifdef XP_MACOSX
    , mKeyNameIndex(KEY_NAME_INDEX_Unidentified)
    , mCodeNameIndex(CODE_NAME_INDEX_UNKNOWN)
    , mIsRepeat(false)
    , mIsComposing(false)
    , mIsSynthesizedByTIP(false)
    , mEditCommandsForSingleLineEditorInitialized(false)
    , mEditCommandsForMultiLineEditorInitialized(false)
    , mEditCommandsForRichTextEditorInitialized(false)
  {
    // If this is a keyboard event on a plugin, it shouldn't fired on content.
    if (IsKeyEventOnPlugin()) {
      mFlags.mOnlySystemGroupDispatchInContent = true;
      StopCrossProcessForwarding();
    }
  }

  static bool IsKeyDownOrKeyDownOnPlugin(EventMessage aMessage)
  {
    return aMessage == eKeyDown || aMessage == eKeyDownOnPlugin;
  }
  bool IsKeyDownOrKeyDownOnPlugin() const
  {
    return IsKeyDownOrKeyDownOnPlugin(mMessage);
  }
  static bool IsKeyUpOrKeyUpOnPlugin(EventMessage aMessage)
  {
    return aMessage == eKeyUp || aMessage == eKeyUpOnPlugin;
  }
  bool IsKeyUpOrKeyUpOnPlugin() const
  {
    return IsKeyUpOrKeyUpOnPlugin(mMessage);
  }
  static bool IsKeyEventOnPlugin(EventMessage aMessage)
  {
    return aMessage == eKeyDownOnPlugin || aMessage == eKeyUpOnPlugin;
  }
  bool IsKeyEventOnPlugin() const
  {
    return IsKeyEventOnPlugin(mMessage);
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eKeyboardEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetKeyboardEvent* result =
      new WidgetKeyboardEvent(false, mMessage, nullptr);
    result->AssignKeyEventData(*this, true);
    result->mEditCommandsForSingleLineEditor = mEditCommandsForSingleLineEditor;
    result->mEditCommandsForMultiLineEditor = mEditCommandsForMultiLineEditor;
    result->mEditCommandsForRichTextEditor = mEditCommandsForRichTextEditor;
    result->mFlags = mFlags;
    return result;
  }

  // OS translated Unicode chars which are used for accesskey and accelkey
  // handling. The handlers will try from first character to last character.
  nsTArray<AlternativeCharCode> mAlternativeCharCodes;
  // DOM KeyboardEvent.key only when mKeyNameIndex is KEY_NAME_INDEX_USE_STRING.
  nsString mKeyValue;
  // DOM KeyboardEvent.code only when mCodeNameIndex is
  // CODE_NAME_INDEX_USE_STRING.
  nsString mCodeValue;

#ifdef XP_MACOSX
  // Values given by a native NSEvent, for use with Cocoa NPAPI plugins.
  nsString mNativeCharacters;
  nsString mNativeCharactersIgnoringModifiers;
  // If this is non-empty, create a text event for plugins instead of a
  // keyboard event.
  nsString mPluginTextEventString;
#endif // #ifdef XP_MACOSX

  // OS-specific native event can optionally be preserved
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
  // True if accesskey handling was forwarded to the child via
  // TabParent::HandleAccessKey. In this case, parent process menu access key
  // handling should be delayed until it is determined that there exists no
  // overriding access key in the content process.
  bool mAccessKeyForwardedToChild;
  // Unique id associated with a keydown / keypress event. It's ok if this wraps
  // over long periods.
  uint32_t mUniqueId;

#ifdef XP_MACOSX
  // Values given by a native NSEvent, for use with Cocoa NPAPI plugins.
  uint32_t mNativeModifierFlags;
  uint16_t mNativeKeyCode;
#endif // #ifdef XP_MACOSX

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

  /**
   * Retrieves all edit commands from mWidget.  This shouldn't be called when
   * the instance is an untrusted event, doesn't have widget or in non-chrome
   * process.
   */
  void InitAllEditCommands();

  /**
   * Retrieves edit commands from mWidget only for aType.  This shouldn't be
   * called when the instance is an untrusted event or doesn't have widget.
   */
  void InitEditCommandsFor(nsIWidget::NativeKeyBindingsType aType);

  /**
   * PreventNativeKeyBindings() makes the instance to not cause any edit
   * actions even if it matches with a native key binding.
   */
  void PreventNativeKeyBindings()
  {
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
  const nsTArray<CommandInt>&
    EditCommandsConstRef(nsIWidget::NativeKeyBindingsType aType) const
  {
    return const_cast<WidgetKeyboardEvent*>(this)->EditCommandsRef(aType);
  }

  /**
   * IsEditCommandsInitialized() returns true if edit commands for aType
   * was already initialized.  Otherwise, false.
   */
  bool IsEditCommandsInitialized(
         nsIWidget::NativeKeyBindingsType aType) const
  {
    return const_cast<WidgetKeyboardEvent*>(this)->
             IsEditCommandsInitializedRef(aType);
  }

#ifdef DEBUG
  /**
   * AreAllEditCommandsInitialized() returns true if edit commands for all
   * types were already initialized.  Otherwise, false.
   */
  bool AreAllEditCommandsInitialized() const
  {
    return mEditCommandsForSingleLineEditorInitialized &&
           mEditCommandsForMultiLineEditorInitialized &&
           mEditCommandsForRichTextEditorInitialized;
  }
#endif // #ifdef DEBUG

  /**
   * Execute edit commands for aType.
   *
   * @return        true if the caller should do nothing anymore.
   *                false, otherwise.
   */
  typedef void (*DoCommandCallback)(Command, void*);
  bool ExecuteEditCommands(nsIWidget::NativeKeyBindingsType aType,
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
  uint32_t PseudoCharCode() const
  {
    return mMessage == eKeyPress ? mCharCode : mPseudoCharCode;
  }
  void SetCharCode(uint32_t aCharCode)
  {
    if (mMessage == eKeyPress) {
      mCharCode = aCharCode;
    } else {
      mPseudoCharCode = aCharCode;
    }
  }

  void GetDOMKeyName(nsAString& aKeyName)
  {
    if (mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
      aKeyName = mKeyValue;
      return;
    }
    GetDOMKeyName(mKeyNameIndex, aKeyName);
  }
  void GetDOMCodeName(nsAString& aCodeName)
  {
    if (mCodeNameIndex == CODE_NAME_INDEX_USE_STRING) {
      aCodeName = mCodeValue;
      return;
    }
    GetDOMCodeName(mCodeNameIndex, aCodeName);
  }

  bool IsModifierKeyEvent() const
  {
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
   * GetModifierForKeyName() returns a value of Modifier which is activated
   * by the aKeyNameIndex.
   */
  static Modifier GetModifierForKeyName(KeyNameIndex aKeyNameIndex);

  /**
   * IsLockableModifier() returns true if aKeyNameIndex is a lockable modifier
   * key such as CapsLock and NumLock.
   */
  static bool IsLockableModifier(KeyNameIndex aKeyNameIndex);

  static void GetDOMKeyName(KeyNameIndex aKeyNameIndex,
                            nsAString& aKeyName);
  static void GetDOMCodeName(CodeNameIndex aCodeNameIndex,
                             nsAString& aCodeName);

  static KeyNameIndex GetKeyNameIndex(const nsAString& aKeyValue);
  static CodeNameIndex GetCodeNameIndex(const nsAString& aCodeValue);

  static const char* GetCommandStr(Command aCommand);

  void AssignKeyEventData(const WidgetKeyboardEvent& aEvent, bool aCopyTargets)
  {
    AssignInputEventData(aEvent, aCopyTargets);

    mKeyCode = aEvent.mKeyCode;
    mCharCode = aEvent.mCharCode;
    mPseudoCharCode = aEvent.mPseudoCharCode;
    mLocation = aEvent.mLocation;
    mAlternativeCharCodes = aEvent.mAlternativeCharCodes;
    mIsRepeat = aEvent.mIsRepeat;
    mIsComposing = aEvent.mIsComposing;
    mAccessKeyForwardedToChild = aEvent.mAccessKeyForwardedToChild;
    mKeyNameIndex = aEvent.mKeyNameIndex;
    mCodeNameIndex = aEvent.mCodeNameIndex;
    mKeyValue = aEvent.mKeyValue;
    mCodeValue = aEvent.mCodeValue;
    // Don't copy mNativeKeyEvent because it may be referred after its instance
    // is destroyed.
    mNativeKeyEvent = nullptr;
    mUniqueId = aEvent.mUniqueId;
#ifdef XP_MACOSX
    mNativeKeyCode = aEvent.mNativeKeyCode;
    mNativeModifierFlags = aEvent.mNativeModifierFlags;
    mNativeCharacters.Assign(aEvent.mNativeCharacters);
    mNativeCharactersIgnoringModifiers.
      Assign(aEvent.mNativeCharactersIgnoringModifiers);
    mPluginTextEventString.Assign(aEvent.mPluginTextEventString);
#endif
    mIsSynthesizedByTIP = aEvent.mIsSynthesizedByTIP;

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

private:
  static const char16_t* const kKeyNames[];
  static const char16_t* const kCodeNames[];
  typedef nsDataHashtable<nsStringHashKey,
                          KeyNameIndex> KeyNameIndexHashtable;
  typedef nsDataHashtable<nsStringHashKey,
                          CodeNameIndex> CodeNameIndexHashtable;
  static KeyNameIndexHashtable* sKeyNameIndexHashtable;
  static CodeNameIndexHashtable* sCodeNameIndexHashtable;

  // mEditCommandsFor*Editor store edit commands.  This should be initialized
  // with InitEditCommandsFor().
  // XXX Ideally, this should be array of Command rather than CommandInt.
  //     However, ParamTraits isn't aware of enum array.
  nsTArray<CommandInt> mEditCommandsForSingleLineEditor;
  nsTArray<CommandInt> mEditCommandsForMultiLineEditor;
  nsTArray<CommandInt> mEditCommandsForRichTextEditor;

  nsTArray<CommandInt>& EditCommandsRef(nsIWidget::NativeKeyBindingsType aType)
  {
    switch (aType) {
      case nsIWidget::NativeKeyBindingsForSingleLineEditor:
        return mEditCommandsForSingleLineEditor;
      case nsIWidget::NativeKeyBindingsForMultiLineEditor:
        return mEditCommandsForMultiLineEditor;
      case nsIWidget::NativeKeyBindingsForRichTextEditor:
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

  bool& IsEditCommandsInitializedRef(nsIWidget::NativeKeyBindingsType aType)
  {
    switch (aType) {
      case nsIWidget::NativeKeyBindingsForSingleLineEditor:
        return mEditCommandsForSingleLineEditorInitialized;
      case nsIWidget::NativeKeyBindingsForMultiLineEditor:
        return mEditCommandsForMultiLineEditorInitialized;
      case nsIWidget::NativeKeyBindingsForRichTextEditor:
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

class WidgetCompositionEvent : public WidgetGUIEvent
{
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

  WidgetCompositionEvent()
  {
  }

public:
  virtual WidgetCompositionEvent* AsCompositionEvent() override
  {
    return this;
  }

  WidgetCompositionEvent(bool aIsTrusted, EventMessage aMessage,
                         nsIWidget* aWidget)
    : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eCompositionEventClass)
    , mNativeIMEContext(aWidget)
    , mOriginalMessage(eVoidEvent)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eCompositionEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetCompositionEvent* result =
      new WidgetCompositionEvent(false, mMessage, nullptr);
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

  void AssignCompositionEventData(const WidgetCompositionEvent& aEvent,
                                  bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);

    mData = aEvent.mData;
    mOriginalMessage = aEvent.mOriginalMessage;
    mRanges = aEvent.mRanges;

    // Currently, we don't need to copy the other members because they are
    // for internal use only (not available from JS).
  }

  bool IsComposing() const
  {
    return mRanges && mRanges->IsComposing();
  }

  uint32_t TargetClauseOffset() const
  {
    return mRanges ? mRanges->TargetClauseOffset() : 0;
  }

  uint32_t TargetClauseLength() const
  {
    uint32_t length = UINT32_MAX;
    if (mRanges) {
      length = mRanges->TargetClauseLength();
    }
    return length == UINT32_MAX ? mData.Length() : length;
  }

  uint32_t RangeCount() const
  {
    return mRanges ? mRanges->Length() : 0;
  }

  bool CausesDOMTextEvent() const
  {
    return mMessage == eCompositionChange ||
           mMessage == eCompositionCommit ||
           mMessage == eCompositionCommitAsIs;
  }

  bool CausesDOMCompositionEndEvent() const
  {
    return mMessage == eCompositionEnd ||
           mMessage == eCompositionCommit ||
           mMessage == eCompositionCommitAsIs;
  }

  bool IsFollowedByCompositionEnd() const
  {
    return IsFollowedByCompositionEnd(mOriginalMessage);
  }

  static bool IsFollowedByCompositionEnd(EventMessage aEventMessage)
  {
    return aEventMessage == eCompositionCommit ||
           aEventMessage == eCompositionCommitAsIs;
  }
};

/******************************************************************************
 * mozilla::WidgetQueryContentEvent
 ******************************************************************************/

class WidgetQueryContentEvent : public WidgetGUIEvent
{
private:
  friend class dom::PBrowserParent;
  friend class dom::PBrowserChild;

  WidgetQueryContentEvent()
    : mSucceeded(false)
    , mUseNativeLineBreak(true)
    , mWithFontRanges(false)
  {
    MOZ_CRASH("WidgetQueryContentEvent is created without proper arguments");
  }

public:
  virtual WidgetQueryContentEvent* AsQueryContentEvent() override
  {
    return this;
  }

  WidgetQueryContentEvent(bool aIsTrusted, EventMessage aMessage,
                          nsIWidget* aWidget)
    : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eQueryContentEventClass)
    , mSucceeded(false)
    , mUseNativeLineBreak(true)
    , mWithFontRanges(false)
  {
  }

  WidgetQueryContentEvent(EventMessage aMessage,
                          const WidgetQueryContentEvent& aOtherEvent)
    : WidgetGUIEvent(aOtherEvent.IsTrusted(), aMessage,
                     const_cast<nsIWidget*>(aOtherEvent.mWidget.get()),
                     eQueryContentEventClass)
    , mSucceeded(false)
    , mUseNativeLineBreak(aOtherEvent.mUseNativeLineBreak)
    , mWithFontRanges(false)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    // This event isn't an internal event of any DOM event.
    NS_ASSERTION(!IsAllowedToDispatchDOMEvent(),
      "WidgetQueryContentEvent needs to support Duplicate()");
    MOZ_CRASH("WidgetQueryContentEvent doesn't support Duplicate()");
  }

  struct Options final
  {
    bool mUseNativeLineBreak;
    bool mRelativeToInsertionPoint;

    explicit Options()
      : mUseNativeLineBreak(true)
      , mRelativeToInsertionPoint(false)
    {
    }

    explicit Options(const WidgetQueryContentEvent& aEvent)
      : mUseNativeLineBreak(aEvent.mUseNativeLineBreak)
      , mRelativeToInsertionPoint(aEvent.mInput.mRelativeToInsertionPoint)
    {
    }
  };

  void Init(const Options& aOptions)
  {
    mUseNativeLineBreak = aOptions.mUseNativeLineBreak;
    mInput.mRelativeToInsertionPoint = aOptions.mRelativeToInsertionPoint;
    MOZ_ASSERT(mInput.IsValidEventMessage(mMessage));
  }

  void InitForQueryTextContent(int64_t aOffset, uint32_t aLength,
                               const Options& aOptions = Options())
  {
    NS_ASSERTION(mMessage == eQueryTextContent,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
    mInput.mLength = aLength;
    Init(aOptions);
    MOZ_ASSERT(mInput.IsValidOffset());
  }

  void InitForQueryCaretRect(int64_t aOffset,
                             const Options& aOptions = Options())
  {
    NS_ASSERTION(mMessage == eQueryCaretRect,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
    Init(aOptions);
    MOZ_ASSERT(mInput.IsValidOffset());
  }

  void InitForQueryTextRect(int64_t aOffset, uint32_t aLength,
                            const Options& aOptions = Options())
  {
    NS_ASSERTION(mMessage == eQueryTextRect,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
    mInput.mLength = aLength;
    Init(aOptions);
    MOZ_ASSERT(mInput.IsValidOffset());
  }

  void InitForQuerySelectedText(SelectionType aSelectionType,
                                const Options& aOptions = Options())
  {
    MOZ_ASSERT(mMessage == eQuerySelectedText);
    MOZ_ASSERT(aSelectionType != SelectionType::eNone);
    mInput.mSelectionType = aSelectionType;
    Init(aOptions);
  }

  void InitForQueryDOMWidgetHittest(const mozilla::LayoutDeviceIntPoint& aPoint)
  {
    NS_ASSERTION(mMessage == eQueryDOMWidgetHittest,
                 "wrong initializer is called");
    mRefPoint = aPoint;
  }

  void InitForQueryTextRectArray(uint32_t aOffset, uint32_t aLength,
                                 const Options& aOptions = Options())
  {
    NS_ASSERTION(mMessage == eQueryTextRectArray,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
    mInput.mLength = aLength;
    Init(aOptions);
  }

  void RequestFontRanges()
  {
    NS_ASSERTION(mMessage == eQueryTextContent,
                 "not querying text content");
    mWithFontRanges = true;
  }

  uint32_t GetSelectionStart(void) const
  {
    NS_ASSERTION(mMessage == eQuerySelectedText,
                 "not querying selection");
    return mReply.mOffset + (mReply.mReversed ? mReply.mString.Length() : 0);
  }

  uint32_t GetSelectionEnd(void) const
  {
    NS_ASSERTION(mMessage == eQuerySelectedText,
                 "not querying selection");
    return mReply.mOffset + (mReply.mReversed ? 0 : mReply.mString.Length());
  }

  mozilla::WritingMode GetWritingMode(void) const
  {
    NS_ASSERTION(mMessage == eQuerySelectedText ||
                 mMessage == eQueryCaretRect ||
                 mMessage == eQueryTextRect,
                 "not querying selection or text rect");
    return mReply.mWritingMode;
  }

  bool mSucceeded;
  bool mUseNativeLineBreak;
  bool mWithFontRanges;
  struct Input final
  {
    uint32_t EndOffset() const
    {
      CheckedInt<uint32_t> endOffset =
        CheckedInt<uint32_t>(mOffset) + mLength;
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
      : mOffset(0)
      , mLength(0)
      , mSelectionType(SelectionType::eNormal)
      , mRelativeToInsertionPoint(false)
    {
    }

    bool IsValidOffset() const
    {
      return mRelativeToInsertionPoint || mOffset >= 0;
    }
    bool IsValidEventMessage(EventMessage aEventMessage) const
    {
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
    bool MakeOffsetAbsolute(uint32_t aInsertionPointOffset)
    {
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
      CheckedInt<uint32_t> absOffset = mOffset + aInsertionPointOffset;
      if (NS_WARN_IF(!absOffset.isValid())) {
        mOffset = UINT32_MAX;
        return false;
      }
      mOffset = absOffset.value();
      return true;
    }
  } mInput;

  struct Reply final
  {
    void* mContentsRoot;
    uint32_t mOffset;
    // mTentativeCaretOffset is used by only eQueryCharacterAtPoint.
    // This is the offset where caret would be if user clicked at the mRefPoint.
    uint32_t mTentativeCaretOffset;
    nsString mString;
    // mRect is used by eQueryTextRect, eQueryCaretRect, eQueryCharacterAtPoint
    // and eQueryEditorRect. The coordinates is system coordinates relative to
    // the top level widget of mFocusedWidget.  E.g., if a <xul:panel> which
    // is owned by a window has focused editor, the offset of mRect is relative
    // to the owner window, not the <xul:panel>.
    mozilla::LayoutDeviceIntRect mRect;
    // The return widget has the caret. This is set at all query events.
    nsIWidget* mFocusedWidget;
    // mozilla::WritingMode value at the end (focus) of the selection
    mozilla::WritingMode mWritingMode;
    // Used by eQuerySelectionAsTransferable
    nsCOMPtr<nsITransferable> mTransferable;
    // Used by eQueryTextContent with font ranges requested
    AutoTArray<mozilla::FontRange, 1> mFontRanges;
    // Used by eQueryTextRectArray
    nsTArray<mozilla::LayoutDeviceIntRect> mRectArray;
    // true if selection is reversed (end < start)
    bool mReversed;
    // true if the selection exists
    bool mHasSelection;
    // true if DOM element under mouse belongs to widget
    bool mWidgetIsHit;

    Reply()
      : mContentsRoot(nullptr)
      , mOffset(NOT_FOUND)
      , mTentativeCaretOffset(NOT_FOUND)
      , mFocusedWidget(nullptr)
      , mReversed(false)
      , mHasSelection(false)
      , mWidgetIsHit(false)
    {
    }
  } mReply;

  enum
  {
    NOT_FOUND = UINT32_MAX
  };

  // values of mComputedScrollAction
  enum
  {
    SCROLL_ACTION_NONE,
    SCROLL_ACTION_LINE,
    SCROLL_ACTION_PAGE
  };
};

/******************************************************************************
 * mozilla::WidgetSelectionEvent
 ******************************************************************************/

class WidgetSelectionEvent : public WidgetGUIEvent
{
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

  WidgetSelectionEvent()
    : mOffset(0)
    , mLength(0)
    , mReversed(false)
    , mExpandToClusterBoundary(true)
    , mSucceeded(false)
    , mUseNativeLineBreak(true)
  {
  }

public:
  virtual WidgetSelectionEvent* AsSelectionEvent() override
  {
    return this;
  }

  WidgetSelectionEvent(bool aIsTrusted, EventMessage aMessage,
                       nsIWidget* aWidget)
    : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eSelectionEventClass)
    , mOffset(0)
    , mLength(0)
    , mReversed(false)
    , mExpandToClusterBoundary(true)
    , mSucceeded(false)
    , mUseNativeLineBreak(true)
    , mReason(nsISelectionListener::NO_REASON)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
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

class InternalEditorInputEvent : public InternalUIEvent
{
private:
  InternalEditorInputEvent()
    : mIsComposing(false)
  {
  }

public:
  virtual InternalEditorInputEvent* AsEditorInputEvent() override
  {
    return this;
  }

  InternalEditorInputEvent(bool aIsTrusted, EventMessage aMessage,
                           nsIWidget* aWidget = nullptr)
    : InternalUIEvent(aIsTrusted, aMessage, aWidget, eEditorInputEventClass)
    , mIsComposing(false)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eEditorInputEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    InternalEditorInputEvent* result =
      new InternalEditorInputEvent(false, mMessage, nullptr);
    result->AssignEditorInputEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  bool mIsComposing;

  void AssignEditorInputEventData(const InternalEditorInputEvent& aEvent,
                                  bool aCopyTargets)
  {
    AssignUIEventData(aEvent, aCopyTargets);

    mIsComposing = aEvent.mIsComposing;
  }
};

} // namespace mozilla

#endif // mozilla_TextEvents_h__
