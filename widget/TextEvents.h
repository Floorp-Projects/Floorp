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

#define NS_DEFINE_VK(aDOMKeyName, aDOMKeyCode) NS_##aDOMKeyName = aDOMKeyCode

enum
{
#include "mozilla/VirtualKeyCodeList.h"
};

#undef NS_DEFINE_VK

namespace mozilla {

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
  ShortcutKeyCandidate(uint32_t aCharCode, bool aIgnoreShift)
    : mCharCode(aCharCode)
    , mIgnoreShift(aIgnoreShift)
  {
  }
  // The charCode value which must match keyboard shortcut definition.
  uint32_t mCharCode;
  // true if Shift state can be ignored.  Otherwise, Shift key state must
  // match keyboard shortcut definition.
  bool mIgnoreShift;
};

/******************************************************************************
 * mozilla::WidgetKeyboardEvent
 ******************************************************************************/

class WidgetKeyboardEvent : public WidgetInputEvent
{
private:
  friend class dom::PBrowserParent;
  friend class dom::PBrowserChild;

protected:
  WidgetKeyboardEvent()
    : keyCode(0)
    , charCode(0)
    , mPseudoCharCode(0)
    , location(nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD)
    , isChar(false)
    , mIsRepeat(false)
    , mIsComposing(false)
    , mKeyNameIndex(mozilla::KEY_NAME_INDEX_Unidentified)
    , mCodeNameIndex(CODE_NAME_INDEX_UNKNOWN)
    , mNativeKeyEvent(nullptr)
    , mUniqueId(0)
#ifdef XP_MACOSX
    , mNativeKeyCode(0)
    , mNativeModifierFlags(0)
#endif
    , mInputMethodAppState(eNotHandled)
    , mIsSynthesizedByTIP(false)
  {
  }

public:
  virtual WidgetKeyboardEvent* AsKeyboardEvent() override { return this; }

  WidgetKeyboardEvent(bool aIsTrusted, EventMessage aMessage,
                      nsIWidget* aWidget,
                      EventClassID aEventClassID = eKeyboardEventClass)
    : WidgetInputEvent(aIsTrusted, aMessage, aWidget, aEventClassID)
    , keyCode(0)
    , charCode(0)
    , mPseudoCharCode(0)
    , location(nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD)
    , isChar(false)
    , mIsRepeat(false)
    , mIsComposing(false)
    , mKeyNameIndex(mozilla::KEY_NAME_INDEX_Unidentified)
    , mCodeNameIndex(CODE_NAME_INDEX_UNKNOWN)
    , mNativeKeyEvent(nullptr)
    , mUniqueId(0)
#ifdef XP_MACOSX
    , mNativeKeyCode(0)
    , mNativeModifierFlags(0)
#endif
    , mInputMethodAppState(eNotHandled)
    , mIsSynthesizedByTIP(false)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eKeyboardEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetKeyboardEvent* result =
      new WidgetKeyboardEvent(false, mMessage, nullptr);
    result->AssignKeyEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  // A DOM keyCode value or 0.  If a keypress event whose charCode is 0, this
  // should be 0.
  uint32_t keyCode;
  // If the instance is a keypress event of a printable key, this is a UTF-16
  // value of the key.  Otherwise, 0.  This value must not be a control
  // character when some modifiers are active.  Then, this value should be an
  // unmodified value except Shift and AltGr.
  uint32_t charCode;
  // mPseudoCharCode is valid only when mMessage is an eKeyDown event.
  // This stores charCode value of keypress event which is fired with same
  // key value and same modifier state.
  uint32_t mPseudoCharCode;
  // One of nsIDOMKeyEvent::DOM_KEY_LOCATION_*
  uint32_t location;
  // OS translated Unicode chars which are used for accesskey and accelkey
  // handling. The handlers will try from first character to last character.
  nsTArray<AlternativeCharCode> alternativeCharCodes;
  // Indicates whether the event signifies a printable character
  bool isChar;
  // Indicates whether the event is generated by auto repeat or not.
  // if this is keyup event, always false.
  bool mIsRepeat;
  // Indicates whether the event is generated during IME (or deadkey)
  // composition.  This is initialized by EventStateManager.  So, key event
  // dispatchers don't need to initialize this.
  bool mIsComposing;
  // DOM KeyboardEvent.key
  KeyNameIndex mKeyNameIndex;
  // DOM KeyboardEvent.code
  CodeNameIndex mCodeNameIndex;
  // DOM KeyboardEvent.key only when mKeyNameIndex is KEY_NAME_INDEX_USE_STRING.
  nsString mKeyValue;
  // DOM KeyboardEvent.code only when mCodeNameIndex is
  // CODE_NAME_INDEX_USE_STRING.
  nsString mCodeValue;
  // OS-specific native event can optionally be preserved
  void* mNativeKeyEvent;
  // Unique id associated with a keydown / keypress event. Used in identifing
  // keypress events for removal from async event dispatch queue in metrofx
  // after preventDefault is called on keydown events. It's ok if this wraps
  // over long periods.
  uint32_t mUniqueId;

#ifdef XP_MACOSX
  // Values given by a native NSEvent, for use with Cocoa NPAPI plugins.
  uint16_t mNativeKeyCode;
  uint32_t mNativeModifierFlags;
  nsString mNativeCharacters;
  nsString mNativeCharactersIgnoringModifiers;
  // If this is non-empty, create a text event for plugins instead of a
  // keyboard event.
  nsString mPluginTextEventString;
#endif

  // Indicates that the event is being handled by input method app
  typedef uint8_t InputMethodAppStateType;
  enum InputMethodAppState : InputMethodAppStateType
  {
    eNotHandled, // not yet handled by intput method app
    eHandling,   // being handled by intput method app
    eHandled     // handled by input method app
  };
  InputMethodAppState mInputMethodAppState;

  // Indicates whether the event is synthesized from Text Input Processor
  // or an actual event from nsAppShell.
  bool mIsSynthesizedByTIP;

  // If the key should cause keypress events, this returns true.
  // Otherwise, false.
  bool ShouldCauseKeypressEvents() const;

  // charCode value of non-eKeyPress events is always 0.  However, if
  // non-eKeyPress event has one or more alternative char code values,
  // its first item should be the charCode value of following eKeyPress event.
  // PseudoCharCode() returns charCode value for eKeyPress event,
  // the first alternative char code value of non-eKeyPress event or 0.
  uint32_t PseudoCharCode() const
  {
    return mMessage == eKeyPress ? charCode : mPseudoCharCode;
  }
  void SetCharCode(uint32_t aCharCode)
  {
    if (mMessage == eKeyPress) {
      charCode = aCharCode;
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
  void GetShortcutKeyCandidates(ShortcutKeyCandidateArray& aCandidates);

  /**
   * Get the candidates for access key.
   *
   * @param aCandidates [out] the candidate access key list.
   *                          the first item is most preferred.
   */
  void GetAccessKeyCandidates(nsTArray<uint32_t>& aCandidates);

  static void Shutdown();

  /**
   * ComputeLocationFromCodeValue() returns one of .location value
   * (nsIDOMKeyEvent::DOM_KEY_LOCATION_*) which is the most preferred value
   * for the specified specified code value.
   */
  static uint32_t ComputeLocationFromCodeValue(CodeNameIndex aCodeNameIndex);

  /**
   * ComputeKeyCodeFromKeyNameIndex() return a .keyCode value which can be
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

    keyCode = aEvent.keyCode;
    charCode = aEvent.charCode;
    mPseudoCharCode = aEvent.mPseudoCharCode;
    location = aEvent.location;
    alternativeCharCodes = aEvent.alternativeCharCodes;
    isChar = aEvent.isChar;
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
#ifdef XP_MACOSX
    mNativeKeyCode = aEvent.mNativeKeyCode;
    mNativeModifierFlags = aEvent.mNativeModifierFlags;
    mNativeCharacters.Assign(aEvent.mNativeCharacters);
    mNativeCharactersIgnoringModifiers.
      Assign(aEvent.mNativeCharactersIgnoringModifiers);
    mPluginTextEventString.Assign(aEvent.mPluginTextEventString);
#endif
    mInputMethodAppState = aEvent.mInputMethodAppState;
    mIsSynthesizedByTIP = aEvent.mIsSynthesizedByTIP;
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
};


/******************************************************************************
 * mozilla::InternalBeforeAfterKeyboardEvent
 *
 * This is extended from WidgetKeyboardEvent and is mapped to DOM event
 * "BeforeAfterKeyboardEvent".
 *
 * Event mMessage: eBeforeKeyDown
 *                 eBeforeKeyUp
 *                 eAfterKeyDown
 *                 eAfterKeyUp
 ******************************************************************************/
class InternalBeforeAfterKeyboardEvent : public WidgetKeyboardEvent
{
private:
  friend class dom::PBrowserParent;
  friend class dom::PBrowserChild;

  InternalBeforeAfterKeyboardEvent()
  {
  }

public:
  // Extra member for InternalBeforeAfterKeyboardEvent. Indicates whether
  // default actions of keydown/keyup event is prevented.
  Nullable<bool> mEmbeddedCancelled;

  virtual InternalBeforeAfterKeyboardEvent* AsBeforeAfterKeyboardEvent() override
  {
    return this;
  }

  InternalBeforeAfterKeyboardEvent(bool aIsTrusted, EventMessage aMessage,
                                   nsIWidget* aWidget)
    : WidgetKeyboardEvent(aIsTrusted, aMessage, aWidget,
                          eBeforeAfterKeyboardEventClass)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eBeforeAfterKeyboardEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    InternalBeforeAfterKeyboardEvent* result =
      new InternalBeforeAfterKeyboardEvent(false, mMessage, nullptr);
    result->AssignBeforeAfterKeyEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  void AssignBeforeAfterKeyEventData(
         const InternalBeforeAfterKeyboardEvent& aEvent,
         bool aCopyTargets)
  {
    AssignKeyEventData(aEvent, aCopyTargets);
    mEmbeddedCancelled = aEvent.mEmbeddedCancelled;
  }

  void AssignBeforeAfterKeyEventData(
         const WidgetKeyboardEvent& aEvent,
         bool aCopyTargets)
  {
    AssignKeyEventData(aEvent, aCopyTargets);
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
    // XXX compositionstart is cancelable in draft of DOM3 Events.
    //     However, it doesn't make sense for us, we cannot cancel composition
    //     when we send compositionstart event.
    mFlags.mCancelable = false;
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
    return mOriginalMessage == eCompositionCommit ||
           mOriginalMessage == eCompositionCommitAsIs;
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

  virtual WidgetEvent* Duplicate() const override
  {
    // This event isn't an internal event of any DOM event.
    NS_ASSERTION(!IsAllowedToDispatchDOMEvent(),
      "WidgetQueryContentEvent needs to support Duplicate()");
    MOZ_CRASH("WidgetQueryContentEvent doesn't support Duplicate()");
  }

  void InitForQueryTextContent(uint32_t aOffset, uint32_t aLength,
                               bool aUseNativeLineBreak = true)
  {
    NS_ASSERTION(mMessage == eQueryTextContent,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
    mInput.mLength = aLength;
    mUseNativeLineBreak = aUseNativeLineBreak;
  }

  void InitForQueryCaretRect(uint32_t aOffset,
                             bool aUseNativeLineBreak = true)
  {
    NS_ASSERTION(mMessage == eQueryCaretRect,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
    mUseNativeLineBreak = aUseNativeLineBreak;
  }

  void InitForQueryTextRect(uint32_t aOffset, uint32_t aLength,
                            bool aUseNativeLineBreak = true)
  {
    NS_ASSERTION(mMessage == eQueryTextRect,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
    mInput.mLength = aLength;
    mUseNativeLineBreak = aUseNativeLineBreak;
  }

  void InitForQueryDOMWidgetHittest(const mozilla::LayoutDeviceIntPoint& aPoint)
  {
    NS_ASSERTION(mMessage == eQueryDOMWidgetHittest,
                 "wrong initializer is called");
    refPoint = aPoint;
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
  struct
  {
    uint32_t EndOffset() const
    {
      CheckedInt<uint32_t> endOffset =
        CheckedInt<uint32_t>(mOffset) + mLength;
      return NS_WARN_IF(!endOffset.isValid()) ? UINT32_MAX : endOffset.value();
    }

    uint32_t mOffset;
    uint32_t mLength;
  } mInput;

  struct Reply
  {
    void* mContentsRoot;
    uint32_t mOffset;
    // mTentativeCaretOffset is used by only eQueryCharacterAtPoint.
    // This is the offset where caret would be if user clicked at the refPoint.
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
                           nsIWidget* aWidget)
    : InternalUIEvent(aIsTrusted, aMessage, aWidget, eEditorInputEventClass)
    , mIsComposing(false)
  {
    if (!aIsTrusted) {
      mFlags.mBubbles = false;
      mFlags.mCancelable = false;
      return;
    }

    mFlags.mBubbles = true;
    mFlags.mCancelable = false;
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
