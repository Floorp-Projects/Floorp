/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_IMEData_h_
#define mozilla_widget_IMEData_h_

#include "nsPoint.h"
#include "nsRect.h"
#include "nsStringGlue.h"

namespace mozilla {

class WritingMode;

} // namespace mozilla

/**
 * Preference for receiving IME updates
 *
 * If mWantUpdates is not NOTIFY_NOTHING, nsTextStateManager will observe text
 * change and/or selection change and call nsIWidget::NotifyIME() with
 * NOTIFY_IME_OF_SELECTION_CHANGE and/or NOTIFY_IME_OF_TEXT_CHANGE.
 * Please note that the text change observing cost is very expensive especially
 * on an HTML editor has focus.
 * If the IME implementation on a particular platform doesn't care about
 * NOTIFY_IME_OF_SELECTION_CHANGE and/or NOTIFY_IME_OF_TEXT_CHANGE,
 * they should set mWantUpdates to NOTIFY_NOTHING to avoid the cost.
 * If the IME implementation needs notifications even while our process is
 * deactive, it should also set NOTIFY_DURING_DEACTIVE.
 */
struct nsIMEUpdatePreference final
{
  typedef uint8_t Notifications;

  enum : Notifications
  {
    NOTIFY_NOTHING                       = 0,
    NOTIFY_SELECTION_CHANGE              = 1 << 0,
    NOTIFY_TEXT_CHANGE                   = 1 << 1,
    NOTIFY_POSITION_CHANGE               = 1 << 2,
    // NOTIFY_MOUSE_BUTTON_EVENT_ON_CHAR is used when mouse button is pressed
    // or released on a character in the focused editor.  The notification is
    // notified to IME as a mouse event.  If it's consumed by IME, NotifyIME()
    // returns NS_SUCCESS_EVENT_CONSUMED.  Otherwise, it returns NS_OK if it's
    // handled without any error.
    NOTIFY_MOUSE_BUTTON_EVENT_ON_CHAR    = 1 << 3,
    // Following values indicate when widget needs or doesn't need notification.
    NOTIFY_CHANGES_CAUSED_BY_COMPOSITION = 1 << 6,
    // NOTE: NOTIFY_DURING_DEACTIVE isn't supported in environments where two
    //       or more compositions are possible.  E.g., Mac and Linux (GTK).
    NOTIFY_DURING_DEACTIVE               = 1 << 7,
    // Changes are notified in following conditions if the instance is
    // just constructed.  If some platforms don't need change notifications
    // in some of following conditions, the platform should remove following
    // flags before returing the instance from nsIWidget::GetUpdatePreference().
    DEFAULT_CONDITIONS_OF_NOTIFYING_CHANGES =
      NOTIFY_CHANGES_CAUSED_BY_COMPOSITION
  };

  nsIMEUpdatePreference()
    : mWantUpdates(DEFAULT_CONDITIONS_OF_NOTIFYING_CHANGES)
  {
  }

  explicit nsIMEUpdatePreference(Notifications aWantUpdates)
    : mWantUpdates(aWantUpdates | DEFAULT_CONDITIONS_OF_NOTIFYING_CHANGES)
  {
  }

  void DontNotifyChangesCausedByComposition()
  {
    mWantUpdates &= ~DEFAULT_CONDITIONS_OF_NOTIFYING_CHANGES;
  }

  bool WantSelectionChange() const
  {
    return !!(mWantUpdates & NOTIFY_SELECTION_CHANGE);
  }

  bool WantTextChange() const
  {
    return !!(mWantUpdates & NOTIFY_TEXT_CHANGE);
  }

  bool WantPositionChanged() const
  {
    return !!(mWantUpdates & NOTIFY_POSITION_CHANGE);
  }

  bool WantChanges() const
  {
    return WantSelectionChange() || WantTextChange();
  }

  bool WantMouseButtonEventOnChar() const
  {
    return !!(mWantUpdates & NOTIFY_MOUSE_BUTTON_EVENT_ON_CHAR);
  }

  bool WantChangesCausedByComposition() const
  {
    return WantChanges() &&
             !!(mWantUpdates & NOTIFY_CHANGES_CAUSED_BY_COMPOSITION);
  }

  bool WantDuringDeactive() const
  {
    return !!(mWantUpdates & NOTIFY_DURING_DEACTIVE);
  }

  Notifications mWantUpdates;
};


/**
 * Contains IMEStatus plus information about the current 
 * input context that the IME can use as hints if desired.
 */

namespace mozilla {
namespace widget {

struct IMEState final
{
  /**
   * IME enabled states, the mEnabled value of
   * SetInputContext()/GetInputContext() should be one value of following
   * values.
   *
   * WARNING: If you change these values, you also need to edit:
   *   nsIDOMWindowUtils.idl
   *   nsContentUtils::GetWidgetStatusFromIMEStatus
   */
  enum Enabled
  {
    /**
     * 'Disabled' means the user cannot use IME. So, the IME open state should
     * be 'closed' during 'disabled'.
     */
    DISABLED,
    /**
     * 'Enabled' means the user can use IME.
     */
    ENABLED,
    /**
     * 'Password' state is a special case for the password editors.
     * E.g., on mac, the password editors should disable the non-Roman
     * keyboard layouts at getting focus. Thus, the password editor may have
     * special rules on some platforms.
     */
    PASSWORD,
    /**
     * This state is used when a plugin is focused.
     * When a plug-in is focused content, we should send native events
     * directly. Because we don't process some native events, but they may
     * be needed by the plug-in.
     */
    PLUGIN
  };
  Enabled mEnabled;

  /**
   * IME open states the mOpen value of SetInputContext() should be one value of
   * OPEN, CLOSE or DONT_CHANGE_OPEN_STATE.  GetInputContext() should return
   * OPEN, CLOSE or OPEN_STATE_NOT_SUPPORTED.
   */
  enum Open
  {
    /**
     * 'Unsupported' means the platform cannot return actual IME open state.
     * This value is used only by GetInputContext().
     */
    OPEN_STATE_NOT_SUPPORTED,
    /**
     * 'Don't change' means the widget shouldn't change IME open state when
     * SetInputContext() is called.
     */
    DONT_CHANGE_OPEN_STATE = OPEN_STATE_NOT_SUPPORTED,
    /**
     * 'Open' means that IME should compose in its primary language (or latest
     * input mode except direct ASCII character input mode).  Even if IME is
     * opened by this value, users should be able to close IME by theirselves.
     * Web contents can specify this value by |ime-mode: active;|.
     */
    OPEN,
    /**
     * 'Closed' means that IME shouldn't handle key events (or should handle
     * as ASCII character inputs on mobile device).  Even if IME is closed by
     * this value, users should be able to open IME by theirselves.
     * Web contents can specify this value by |ime-mode: inactive;|.
     */
    CLOSED
  };
  Open mOpen;

  IMEState()
    : mEnabled(ENABLED)
    , mOpen(DONT_CHANGE_OPEN_STATE)
  {
  }

  explicit IMEState(Enabled aEnabled, Open aOpen = DONT_CHANGE_OPEN_STATE)
    : mEnabled(aEnabled)
    , mOpen(aOpen)
  {
  }

  // Returns true if the user can input characters.
  // This means that a plain text editor, an HTML editor, a password editor or
  // a plain text editor whose ime-mode is "disabled".
  bool IsEditable() const
  {
    return mEnabled == ENABLED || mEnabled == PASSWORD;
  }
  // Returns true if the user might be able to input characters.
  // This means that a plain text editor, an HTML editor, a password editor,
  // a plain text editor whose ime-mode is "disabled" or a windowless plugin
  // has focus.
  bool MaybeEditable() const
  {
    return IsEditable() || mEnabled == PLUGIN;
  }
};

struct InputContext final
{
  InputContext()
    : mNativeIMEContext(nullptr)
    , mOrigin(XRE_IsParentProcess() ? ORIGIN_MAIN : ORIGIN_CONTENT)
    , mMayBeIMEUnaware(false)
  {
  }

  bool IsPasswordEditor() const
  {
    return mHTMLInputType.LowerCaseEqualsLiteral("password");
  }

  IMEState mIMEState;

  /* The type of the input if the input is a html input field */
  nsString mHTMLInputType;

  /* The type of the inputmode */
  nsString mHTMLInputInputmode;

  /* A hint for the action that is performed when the input is submitted */
  nsString mActionHint;

  /* Native IME context for the widget.  This doesn't come from the argument of
     SetInputContext().  If there is only one context in the process, this may
     be nullptr. */
  void* mNativeIMEContext;

  /**
   * mOrigin indicates whether this focus event refers to main or remote
   * content.
   */
  enum Origin
  {
    // Adjusting focus of content on the main process
    ORIGIN_MAIN,
    // Adjusting focus of content in a remote process
    ORIGIN_CONTENT
  };
  Origin mOrigin;

  /* True if the webapp may be unaware of IME events such as input event or
   * composiion events. This enables a key-events-only mode on Android for
   * compatibility with webapps relying on key listeners. */
  bool mMayBeIMEUnaware;

  bool IsOriginMainProcess() const
  {
    return mOrigin == ORIGIN_MAIN;
  }

  bool IsOriginContentProcess() const
  {
    return mOrigin == ORIGIN_CONTENT;
  }

  bool IsOriginCurrentProcess() const
  {
    if (XRE_IsParentProcess()) {
      return IsOriginMainProcess();
    }
    return IsOriginContentProcess();
  }
};

struct InputContextAction final
{
  /**
   * mCause indicates what action causes calling nsIWidget::SetInputContext().
   * It must be one of following values.
   */
  enum Cause
  {
    // The cause is unknown but originated from content. Focus might have been
    // changed by content script.
    CAUSE_UNKNOWN,
    // The cause is unknown but originated from chrome. Focus might have been
    // changed by chrome script.
    CAUSE_UNKNOWN_CHROME,
    // The cause is user's keyboard operation.
    CAUSE_KEY,
    // The cause is user's mouse operation.
    CAUSE_MOUSE
  };
  Cause mCause;

  /**
   * mFocusChange indicates what happened for focus.
   */
  enum FocusChange
  {
    FOCUS_NOT_CHANGED,
    // A content got focus.
    GOT_FOCUS,
    // Focused content lost focus.
    LOST_FOCUS,
    // Menu got pseudo focus that means focused content isn't changed but
    // keyboard events will be handled by menu.
    MENU_GOT_PSEUDO_FOCUS,
    // Menu lost pseudo focus that means focused content will handle keyboard
    // events.
    MENU_LOST_PSEUDO_FOCUS
  };
  FocusChange mFocusChange;

  bool ContentGotFocusByTrustedCause() const
  {
    return (mFocusChange == GOT_FOCUS &&
            mCause != CAUSE_UNKNOWN);
  }

  bool UserMightRequestOpenVKB() const
  {
    return (mFocusChange == FOCUS_NOT_CHANGED &&
            mCause == CAUSE_MOUSE);
  }

  InputContextAction()
    : mCause(CAUSE_UNKNOWN)
    , mFocusChange(FOCUS_NOT_CHANGED)
  {
  }

  explicit InputContextAction(Cause aCause,
                              FocusChange aFocusChange = FOCUS_NOT_CHANGED)
    : mCause(aCause)
    , mFocusChange(aFocusChange)
  {
  }
};

// IMEMessage is shared by IMEStateManager and TextComposition.
// Update values in GeckoEditable.java if you make changes here.
// XXX Negative values are used in Android...
typedef int8_t IMEMessageType;
enum IMEMessage : IMEMessageType
{
  // This is used by IMENotification internally.  This means that the instance
  // hasn't been initialized yet.
  NOTIFY_IME_OF_NOTHING,
  // An editable content is getting focus
  NOTIFY_IME_OF_FOCUS,
  // An editable content is losing focus
  NOTIFY_IME_OF_BLUR,
  // Selection in the focused editable content is changed
  NOTIFY_IME_OF_SELECTION_CHANGE,
  // Text in the focused editable content is changed
  NOTIFY_IME_OF_TEXT_CHANGE,
  // Composition string has been updated
  NOTIFY_IME_OF_COMPOSITION_UPDATE,
  // Position or size of focused element may be changed.
  NOTIFY_IME_OF_POSITION_CHANGE,
  // Mouse button event is fired on a character in focused editor
  NOTIFY_IME_OF_MOUSE_BUTTON_EVENT,
  // Request to commit current composition to IME
  // (some platforms may not support)
  REQUEST_TO_COMMIT_COMPOSITION,
  // Request to cancel current composition to IME
  // (some platforms may not support)
  REQUEST_TO_CANCEL_COMPOSITION
};

struct IMENotification final
{
  IMENotification()
    : mMessage(NOTIFY_IME_OF_NOTHING)
  {
  }

  IMENotification(const IMENotification& aOther)
    : mMessage(NOTIFY_IME_OF_NOTHING)
  {
    Assign(aOther);
  }

  ~IMENotification()
  {
    Clear();
  }

  MOZ_IMPLICIT IMENotification(IMEMessage aMessage)
    : mMessage(aMessage)
  {
    switch (aMessage) {
      case NOTIFY_IME_OF_SELECTION_CHANGE:
        mSelectionChangeData.mString = new nsString();
        mSelectionChangeData.Clear();
        break;
      case NOTIFY_IME_OF_TEXT_CHANGE:
        mTextChangeData.Clear();
        break;
      case NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
        mMouseButtonEventData.mEventMessage = NS_EVENT_NULL;
        mMouseButtonEventData.mOffset = UINT32_MAX;
        mMouseButtonEventData.mCursorPos.Set(nsIntPoint(0, 0));
        mMouseButtonEventData.mCharRect.Set(nsIntRect(0, 0, 0, 0));
        mMouseButtonEventData.mButton = -1;
        mMouseButtonEventData.mButtons = 0;
        mMouseButtonEventData.mModifiers = 0;
      default:
        break;
    }
  }

  void Assign(const IMENotification& aOther)
  {
    bool changingMessage = mMessage != aOther.mMessage;
    if (changingMessage) {
      Clear();
      mMessage = aOther.mMessage;
    }
    switch (mMessage) {
      case NOTIFY_IME_OF_SELECTION_CHANGE:
        if (changingMessage) {
          mSelectionChangeData.mString = new nsString();
        }
        mSelectionChangeData.Assign(aOther.mSelectionChangeData);
        break;
      case NOTIFY_IME_OF_TEXT_CHANGE:
        mTextChangeData = aOther.mTextChangeData;
        break;
      case NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
        mMouseButtonEventData = aOther.mMouseButtonEventData;
        break;
      default:
        break;
    }
  }

  IMENotification& operator=(const IMENotification& aOther)
  {
    Assign(aOther);
    return *this;
  }

  void Clear()
  {
    if (mMessage == NOTIFY_IME_OF_SELECTION_CHANGE) {
      MOZ_ASSERT(mSelectionChangeData.mString);
      delete mSelectionChangeData.mString;
      mSelectionChangeData.mString = nullptr;
    }
    mMessage = NOTIFY_IME_OF_NOTHING;
  }

  bool HasNotification() const
  {
    return mMessage != NOTIFY_IME_OF_NOTHING;
  }

  void MergeWith(const IMENotification& aNotification)
  {
    switch (mMessage) {
      case NOTIFY_IME_OF_NOTHING:
        MOZ_ASSERT(aNotification.mMessage != NOTIFY_IME_OF_NOTHING);
        Assign(aNotification);
        break;
      case NOTIFY_IME_OF_SELECTION_CHANGE:
        MOZ_ASSERT(aNotification.mMessage == NOTIFY_IME_OF_SELECTION_CHANGE);
        mSelectionChangeData.Assign(aNotification.mSelectionChangeData);
        break;
      case NOTIFY_IME_OF_TEXT_CHANGE:
        MOZ_ASSERT(aNotification.mMessage == NOTIFY_IME_OF_TEXT_CHANGE);
        mTextChangeData += aNotification.mTextChangeData;
        break;
      case NOTIFY_IME_OF_POSITION_CHANGE:
      case NOTIFY_IME_OF_COMPOSITION_UPDATE:
        MOZ_ASSERT(aNotification.mMessage == mMessage);
        break;
      default:
        MOZ_CRASH("Merging notification isn't supported");
        break;
    }
  }

  IMEMessage mMessage;

  struct Point
  {
    int32_t mX;
    int32_t mY;

    void Set(const nsIntPoint& aPoint)
    {
      mX = aPoint.x;
      mY = aPoint.y;
    }
    nsIntPoint AsIntPoint() const
    {
      return nsIntPoint(mX, mY);
    }
  };

  struct Rect
  {
    int32_t mX;
    int32_t mY;
    int32_t mWidth;
    int32_t mHeight;

    void Set(const nsIntRect& aRect)
    {
      mX = aRect.x;
      mY = aRect.y;
      mWidth = aRect.width;
      mHeight = aRect.height;
    }
    nsIntRect AsIntRect() const
    {
      return nsIntRect(mX, mY, mWidth, mHeight);
    }
  };

  // NOTIFY_IME_OF_SELECTION_CHANGE specific data
  struct SelectionChangeDataBase
  {
    // Selection range.
    uint32_t mOffset;

    // Selected string
    nsString* mString;

    // Writing mode at the selection.
    uint8_t mWritingMode;

    bool mReversed;
    bool mCausedByComposition;
    bool mCausedBySelectionEvent;

    void SetWritingMode(const WritingMode& aWritingMode);
    WritingMode GetWritingMode() const;

    uint32_t StartOffset() const
    {
      return mOffset + (mReversed ? Length() : 0);
    }
    uint32_t EndOffset() const
    {
      return mOffset + (mReversed ? 0 : Length());
    }
    const nsString& String() const
    {
      return *mString;
    }
    uint32_t Length() const
    {
      return mString->Length();
    }
    bool IsInInt32Range() const
    {
      return mOffset + Length() <= INT32_MAX;
    }
    void Clear()
    {
      mOffset = UINT32_MAX;
      mString->Truncate();
      mWritingMode = 0;
      mReversed = false;
      mCausedByComposition = false;
      mCausedBySelectionEvent = false;
    }
    bool IsValid() const
    {
      return mOffset != UINT32_MAX;
    }
    void Assign(const SelectionChangeDataBase& aOther)
    {
      mOffset = aOther.mOffset;
      *mString = aOther.String();
      mWritingMode = aOther.mWritingMode;
      mReversed = aOther.mReversed;
      mCausedByComposition = aOther.mCausedByComposition;
      mCausedBySelectionEvent = aOther.mCausedBySelectionEvent;
    }
  };

  // SelectionChangeDataBase cannot have constructors because it's used in
  // the union.  Therefore, SelectionChangeData should only implement
  // constructors.  In other words, add other members to
  // SelectionChangeDataBase.
  struct SelectionChangeData final : public SelectionChangeDataBase
  {
    SelectionChangeData()
    {
      mString = &mStringInstance;
      Clear();
    }
    explicit SelectionChangeData(const SelectionChangeDataBase& aOther)
    {
      mString = &mStringInstance;
      Assign(aOther);
    }
    SelectionChangeData(const SelectionChangeData& aOther)
    {
      mString = &mStringInstance;
      Assign(aOther);
    }
    SelectionChangeData& operator=(const SelectionChangeDataBase& aOther)
    {
      mString = &mStringInstance;
      Assign(aOther);
      return *this;
    }
    SelectionChangeData& operator=(const SelectionChangeData& aOther)
    {
      mString = &mStringInstance;
      Assign(aOther);
      return *this;
    }

  private:
    // When SelectionChangeData is used outside of union, it shouldn't create
    // nsString instance in the heap as far as possible.
    nsString mStringInstance;
  };

  struct TextChangeDataBase
  {
    // mStartOffset is the start offset of modified or removed text in
    // original content and inserted text in new content.
    uint32_t mStartOffset;
    // mRemovalEndOffset is the end offset of modified or removed text in
    // original content.  If the value is same as mStartOffset, no text hasn't
    // been removed yet.
    uint32_t mRemovedEndOffset;
    // mAddedEndOffset is the end offset of inserted text or same as
    // mStartOffset if just removed.  The vlaue is offset in the new content.
    uint32_t mAddedEndOffset;

    bool mCausedByComposition;

    uint32_t OldLength() const
    {
      MOZ_ASSERT(IsValid());
      return mRemovedEndOffset - mStartOffset;
    }
    uint32_t NewLength() const
    {
      MOZ_ASSERT(IsValid());
      return mAddedEndOffset - mStartOffset;
    }

    // Positive if text is added. Negative if text is removed.
    int64_t Difference() const 
    {
      return mAddedEndOffset - mRemovedEndOffset;
    }

    bool IsInInt32Range() const
    {
      MOZ_ASSERT(IsValid());
      return mStartOffset <= INT32_MAX &&
             mRemovedEndOffset <= INT32_MAX &&
             mAddedEndOffset <= INT32_MAX;
    }

    bool IsValid() const
    {
      return !(mStartOffset == UINT32_MAX &&
               !mRemovedEndOffset && !mAddedEndOffset);
    }

    void Clear()
    {
      mStartOffset = UINT32_MAX;
      mRemovedEndOffset = mAddedEndOffset = 0;
    }

    void MergeWith(const TextChangeDataBase& aOther);
    TextChangeDataBase& operator+=(const TextChangeDataBase& aOther)
    {
      MergeWith(aOther);
      return *this;
    }

#ifdef DEBUG
    void Test();
#endif // #ifdef DEBUG
  };

  // TextChangeDataBase cannot have constructors because they are used in union.
  // Therefore, TextChangeData should only implement constructor.  In other
  // words, add other members to TextChangeDataBase.
  struct TextChangeData : public TextChangeDataBase
  {
    TextChangeData() { Clear(); }

    TextChangeData(uint32_t aStartOffset,
                   uint32_t aRemovedEndOffset,
                   uint32_t aAddedEndOffset,
                   bool aCausedByComposition)
    {
      MOZ_ASSERT(aRemovedEndOffset >= aStartOffset,
                 "removed end offset must not be smaller than start offset");
      MOZ_ASSERT(aAddedEndOffset >= aStartOffset,
                 "added end offset must not be smaller than start offset");
      mStartOffset = aStartOffset;
      mRemovedEndOffset = aRemovedEndOffset;
      mAddedEndOffset = aAddedEndOffset;
      mCausedByComposition = aCausedByComposition;
    }
  };

  struct MouseButtonEventData
  {
    // The value of WidgetEvent::mMessage
    EventMessage mEventMessage;
    // Character offset from the start of the focused editor under the cursor
    uint32_t mOffset;
    // Cursor position in pixels relative to the widget
    Point mCursorPos;
    // Character rect in pixels under the cursor relative to the widget
    Rect mCharRect;
    // The value of WidgetMouseEventBase::button and buttons
    int16_t mButton;
    int16_t mButtons;
    // The value of WidgetInputEvent::modifiers
    Modifiers mModifiers;
  };

  union
  {
    // NOTIFY_IME_OF_SELECTION_CHANGE specific data
    SelectionChangeDataBase mSelectionChangeData;

    // NOTIFY_IME_OF_TEXT_CHANGE specific data
    TextChangeDataBase mTextChangeData;

    // NOTIFY_IME_OF_MOUSE_BUTTON_EVENT specific data
    MouseButtonEventData mMouseButtonEventData;
  };

  void SetData(const SelectionChangeDataBase& aSelectionChangeData)
  {
    MOZ_RELEASE_ASSERT(mMessage == NOTIFY_IME_OF_SELECTION_CHANGE);
    mSelectionChangeData.Assign(aSelectionChangeData);
  }
  void SetData(const SelectionChangeDataBase& aSelectionChangeData,
               bool aCausedByComposition,
               bool aCausedBySelectionEvent)
  {
    MOZ_RELEASE_ASSERT(mMessage == NOTIFY_IME_OF_SELECTION_CHANGE);
    mSelectionChangeData.Assign(aSelectionChangeData);
    mSelectionChangeData.mCausedByComposition = aCausedByComposition;
    mSelectionChangeData.mCausedBySelectionEvent = aCausedBySelectionEvent;
  }

  void SetData(const TextChangeDataBase& aTextChangeData)
  {
    MOZ_RELEASE_ASSERT(mMessage == NOTIFY_IME_OF_TEXT_CHANGE);
    mTextChangeData = aTextChangeData;
  }

  bool IsCausedByComposition() const
  {
    switch (mMessage) {
      case NOTIFY_IME_OF_SELECTION_CHANGE:
        return mSelectionChangeData.mCausedByComposition;
      case NOTIFY_IME_OF_TEXT_CHANGE:
        return mTextChangeData.mCausedByComposition;
      default:
        return false;
    }
  }
};

} // namespace widget
} // namespace mozilla

#endif // #ifndef mozilla_widget_IMEData_h_
