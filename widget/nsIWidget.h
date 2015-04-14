/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIWidget_h__
#define nsIWidget_h__

#include "nsISupports.h"
#include "nsColor.h"
#include "nsRect.h"
#include "nsStringGlue.h"

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsWidgetInitData.h"
#include "nsTArray.h"
#include "nsITheme.h"
#include "nsITimer.h"
#include "nsXULAppAPI.h"
#include "mozilla/EventForwards.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/gfx/Point.h"
#include "nsIObserver.h"
#include "Units.h"

// forward declarations
class   nsFontMetrics;
class   nsDeviceContext;
struct  nsFont;
class   nsIRollupListener;
class   imgIContainer;
class   nsIContent;
class   ViewWrapper;
class   nsIWidgetListener;
class   nsIntRegion;
class   nsIScreen;

namespace mozilla {
class CompositorVsyncDispatcher;
namespace dom {
class TabChild;
}
namespace plugins {
class PluginWidgetChild;
}
namespace layers {
class Composer2D;
class CompositorChild;
class LayerManager;
class LayerManagerComposite;
class PLayerTransactionChild;
}
namespace gfx {
class DrawTarget;
}
namespace widget {
class TextEventDispatcher;
}
}

/**
 * Callback function that processes events.
 *
 * The argument is actually a subtype (subclass) of WidgetEvent which carries
 * platform specific information about the event. Platform specific code
 * knows how to deal with it.
 *
 * The return value determines whether or not the default action should take
 * place.
 */
typedef nsEventStatus (* EVENT_CALLBACK)(mozilla::WidgetGUIEvent* aEvent);

// Hide the native window system's real window type so as to avoid
// including native window system types and APIs. This is necessary
// to ensure cross-platform code.
typedef void* nsNativeWidget;

/**
 * Flags for the GetNativeData and SetNativeData functions
 */
#define NS_NATIVE_WINDOW      0
#define NS_NATIVE_GRAPHIC     1
#define NS_NATIVE_TMP_WINDOW  2
#define NS_NATIVE_WIDGET      3
#define NS_NATIVE_DISPLAY     4
#define NS_NATIVE_REGION      5
#define NS_NATIVE_OFFSETX     6
#define NS_NATIVE_OFFSETY     7
#define NS_NATIVE_PLUGIN_PORT 8
#define NS_NATIVE_SCREEN      9
// The toplevel GtkWidget containing this nsIWidget:
#define NS_NATIVE_SHELLWIDGET 10
// Has to match to NPNVnetscapeWindow, and shareable across processes
// HWND on Windows and XID on X11
#define NS_NATIVE_SHAREABLE_WINDOW 11
#define NS_NATIVE_OPENGL_CONTEXT   12
#ifdef XP_MACOSX
#define NS_NATIVE_PLUGIN_PORT_QD    100
#define NS_NATIVE_PLUGIN_PORT_CG    101
#endif
#ifdef XP_WIN
#define NS_NATIVE_TSF_THREAD_MGR       100
#define NS_NATIVE_TSF_CATEGORY_MGR     101
#define NS_NATIVE_TSF_DISPLAY_ATTR_MGR 102
#define NS_NATIVE_ICOREWINDOW          103 // winrt specific
#endif
#if defined(MOZ_WIDGET_GTK)
// set/get nsPluginNativeWindowGtk, e10s specific
#define NS_NATIVE_PLUGIN_OBJECT_PTR    104
#endif
// See RegisterPluginWindowForRemoteUpdates
#define NS_NATIVE_PLUGIN_ID            105

#define NS_IWIDGET_IID \
{ 0x316E4600, 0x15DB, 0x47AE, \
  { 0xBF, 0xE4, 0x5B, 0xCD, 0xFF, 0x80, 0x80, 0x83 } };

/*
 * Window shadow styles
 * Also used for the -moz-window-shadow CSS property
 */

#define NS_STYLE_WINDOW_SHADOW_NONE             0
#define NS_STYLE_WINDOW_SHADOW_DEFAULT          1
#define NS_STYLE_WINDOW_SHADOW_MENU             2
#define NS_STYLE_WINDOW_SHADOW_TOOLTIP          3
#define NS_STYLE_WINDOW_SHADOW_SHEET            4

/**
 * Transparency modes
 */

enum nsTransparencyMode {
  eTransparencyOpaque = 0,  // Fully opaque
  eTransparencyTransparent, // Parts of the window may be transparent
  eTransparencyGlass,       // Transparent parts of the window have Vista AeroGlass effect applied
  eTransparencyBorderlessGlass // As above, but without a border around the opaque areas when there would otherwise be one with eTransparencyGlass
};

/**
 * Cursor types.
 */

enum nsCursor {   ///(normal cursor,       usually rendered as an arrow)
                eCursor_standard, 
                  ///(system is busy,      usually rendered as a hourglass or watch)
                eCursor_wait, 
                  ///(Selecting something, usually rendered as an IBeam)
                eCursor_select, 
                  ///(can hyper-link,      usually rendered as a human hand)
                eCursor_hyperlink, 
                  ///(north/south/west/east edge sizing)
                eCursor_n_resize,
                eCursor_s_resize,
                eCursor_w_resize,
                eCursor_e_resize,
                  ///(corner sizing)
                eCursor_nw_resize,
                eCursor_se_resize,
                eCursor_ne_resize,
                eCursor_sw_resize,
                eCursor_crosshair,
                eCursor_move,
                eCursor_help,
                eCursor_copy, // CSS3
                eCursor_alias,
                eCursor_context_menu,
                eCursor_cell,
                eCursor_grab,
                eCursor_grabbing,
                eCursor_spinning,
                eCursor_zoom_in,
                eCursor_zoom_out,
                eCursor_not_allowed,
                eCursor_col_resize,
                eCursor_row_resize,
                eCursor_no_drop,
                eCursor_vertical_text,
                eCursor_all_scroll,
                eCursor_nesw_resize,
                eCursor_nwse_resize,
                eCursor_ns_resize,
                eCursor_ew_resize,
                eCursor_none,
                // This one better be the last one in this list.
                eCursorCount
                }; 

enum nsTopLevelWidgetZPlacement { // for PlaceBehind()
  eZPlacementBottom = 0,  // bottom of the window stack
  eZPlacementBelow,       // just below another widget
  eZPlacementTop          // top of the window stack
};

/**
 * Before the OS goes to sleep, this topic is notified.
 */
#define NS_WIDGET_SLEEP_OBSERVER_TOPIC "sleep_notification"

/**
 * After the OS wakes up, this topic is notified.
 */
#define NS_WIDGET_WAKE_OBSERVER_TOPIC "wake_notification"

/**
 * Before the OS suspends the current process, this topic is notified.  Some
 * OS will kill processes that are suspended instead of resuming them.
 * For that reason this topic may be useful to safely close down resources.
 */
#define NS_WIDGET_SUSPEND_PROCESS_OBSERVER_TOPIC "suspend_process_notification"

/**
 * After the current process resumes from being suspended, this topic is
 * notified.
 */
#define NS_WIDGET_RESUME_PROCESS_OBSERVER_TOPIC "resume_process_notification"

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
struct nsIMEUpdatePreference {

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


/* 
 * Contains IMEStatus plus information about the current 
 * input context that the IME can use as hints if desired.
 */

namespace mozilla {
namespace widget {

struct IMEState {
  /**
   * IME enabled states, the mEnabled value of
   * SetInputContext()/GetInputContext() should be one value of following
   * values.
   *
   * WARNING: If you change these values, you also need to edit:
   *   nsIDOMWindowUtils.idl
   *   nsContentUtils::GetWidgetStatusFromIMEStatus
   */
  enum Enabled {
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
  enum Open {
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

  IMEState() : mEnabled(ENABLED), mOpen(DONT_CHANGE_OPEN_STATE) { }

  explicit IMEState(Enabled aEnabled, Open aOpen = DONT_CHANGE_OPEN_STATE) :
    mEnabled(aEnabled), mOpen(aOpen)
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

struct InputContext {
  InputContext()
    : mNativeIMEContext(nullptr)
    , mOrigin(XRE_IsParentProcess() ? ORIGIN_MAIN : ORIGIN_CONTENT)
  {}

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
   * mOrigin indicates whether this focus event refers to main or remote content.
   */
  enum Origin
  {
    // Adjusting focus of content on the main process
    ORIGIN_MAIN,
    // Adjusting focus of content in a remote process
    ORIGIN_CONTENT
  };
  Origin mOrigin;

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

struct InputContextAction {
  /**
   * mCause indicates what action causes calling nsIWidget::SetInputContext().
   * It must be one of following values.
   */
  enum Cause {
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
  enum FocusChange {
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

  bool ContentGotFocusByTrustedCause() const {
    return (mFocusChange == GOT_FOCUS &&
            mCause != CAUSE_UNKNOWN);
  }

  bool UserMightRequestOpenVKB() const {
    return (mFocusChange == FOCUS_NOT_CHANGED &&
            mCause == CAUSE_MOUSE);
  }

  InputContextAction() :
    mCause(CAUSE_UNKNOWN), mFocusChange(FOCUS_NOT_CHANGED)
  {
  }

  explicit InputContextAction(Cause aCause,
                              FocusChange aFocusChange = FOCUS_NOT_CHANGED) :
    mCause(aCause), mFocusChange(aFocusChange)
  {
  }
};

/**
 * Size constraints for setting the minimum and maximum size of a widget.
 * Values are in device pixels.
 */
struct SizeConstraints {
  SizeConstraints()
    : mMaxSize(NS_MAXSIZE, NS_MAXSIZE)
  {
  }

  SizeConstraints(mozilla::LayoutDeviceIntSize aMinSize,
                  mozilla::LayoutDeviceIntSize aMaxSize)
  : mMinSize(aMinSize),
    mMaxSize(aMaxSize)
  {
  }

  mozilla::LayoutDeviceIntSize mMinSize;
  mozilla::LayoutDeviceIntSize mMaxSize;
};

// IMEMessage is shared by IMEStateManager and TextComposition.
// Update values in GeckoEditable.java if you make changes here.
// XXX Negative values are used in Android...
typedef int8_t IMEMessageType;
enum IMEMessage : IMEMessageType
{
  // An editable content is getting focus
  NOTIFY_IME_OF_FOCUS = 1,
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

struct IMENotification
{
  IMENotification()
    : mMessage(static_cast<IMEMessage>(-1))
  {}

  MOZ_IMPLICIT IMENotification(IMEMessage aMessage)
    : mMessage(aMessage)
  {
    switch (aMessage) {
      case NOTIFY_IME_OF_SELECTION_CHANGE:
        mSelectionChangeData.mCausedByComposition = false;
        break;
      case NOTIFY_IME_OF_TEXT_CHANGE:
        mTextChangeData.mStartOffset = 0;
        mTextChangeData.mOldEndOffset = 0;
        mTextChangeData.mNewEndOffset = 0;
        mTextChangeData.mCausedByComposition = false;
        break;
      case NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
        mMouseButtonEventData.mEventMessage = 0;
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

  IMEMessage mMessage;

  union
  {
    // NOTIFY_IME_OF_SELECTION_CHANGE specific data
    struct
    {
      bool mCausedByComposition;
    } mSelectionChangeData;

    // NOTIFY_IME_OF_TEXT_CHANGE specific data
    struct
    {
      uint32_t mStartOffset;
      uint32_t mOldEndOffset;
      uint32_t mNewEndOffset;

      bool mCausedByComposition;

      uint32_t OldLength() const { return mOldEndOffset - mStartOffset; }
      uint32_t NewLength() const { return mNewEndOffset - mStartOffset; }
      int32_t AdditionalLength() const
      {
        return static_cast<int32_t>(mNewEndOffset - mOldEndOffset);
      }
      bool IsInInt32Range() const
      {
        return mStartOffset <= INT32_MAX &&
               mOldEndOffset <= INT32_MAX &&
               mNewEndOffset <= INT32_MAX;
      }
    } mTextChangeData;

    // NOTIFY_IME_OF_MOUSE_BUTTON_EVENT specific data
    struct
    {
      // The value of WidgetEvent::message
      uint32_t mEventMessage;
      // Character offset from the start of the focused editor under the cursor
      uint32_t mOffset;
      // Cursor position in pixels relative to the widget
      struct
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
      } mCursorPos;
      // Character rect in pixels under the cursor relative to the widget
      struct
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
      } mCharRect;
      // The value of WidgetMouseEventBase::button and buttons
      int16_t mButton;
      int16_t mButtons;
      // The value of WidgetInputEvent::modifiers
      Modifiers mModifiers;
    } mMouseButtonEventData;
  };

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

struct AutoObserverNotifier {
  AutoObserverNotifier(nsIObserver* aObserver,
                       const char* aTopic)
    : mObserver(aObserver)
    , mTopic(aTopic)
  {
  }

  void SkipNotification()
  {
    mObserver = nullptr;
  }

  ~AutoObserverNotifier()
  {
    if (mObserver) {
      mObserver->Observe(nullptr, mTopic, nullptr);
    }
  }

private:
  nsCOMPtr<nsIObserver> mObserver;
  const char* mTopic;
};

} // namespace widget
} // namespace mozilla

/**
 * The base class for all the widgets. It provides the interface for
 * all basic and necessary functionality.
 */
class nsIWidget : public nsISupports {
  protected:
    typedef mozilla::dom::TabChild TabChild;

  public:
    typedef mozilla::layers::Composer2D Composer2D;
    typedef mozilla::layers::CompositorChild CompositorChild;
    typedef mozilla::layers::LayerManager LayerManager;
    typedef mozilla::layers::LayerManagerComposite LayerManagerComposite;
    typedef mozilla::layers::LayersBackend LayersBackend;
    typedef mozilla::layers::PLayerTransactionChild PLayerTransactionChild;
    typedef mozilla::widget::IMEMessage IMEMessage;
    typedef mozilla::widget::IMENotification IMENotification;
    typedef mozilla::widget::IMEState IMEState;
    typedef mozilla::widget::InputContext InputContext;
    typedef mozilla::widget::InputContextAction InputContextAction;
    typedef mozilla::widget::SizeConstraints SizeConstraints;
    typedef mozilla::widget::TextEventDispatcher TextEventDispatcher;
    typedef mozilla::CompositorVsyncDispatcher CompositorVsyncDispatcher;

    // Used in UpdateThemeGeometries.
    struct ThemeGeometry {
      // The ThemeGeometryType value for the themed widget, see
      // nsITheme::ThemeGeometryTypeForWidget.
      nsITheme::ThemeGeometryType mType;
      // The device-pixel rect within the window for the themed widget
      nsIntRect mRect;

      ThemeGeometry(nsITheme::ThemeGeometryType aType, const nsIntRect& aRect)
        : mType(aType)
        , mRect(aRect)
      { }
    };

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IWIDGET_IID)

    nsIWidget()
      : mLastChild(nullptr)
      , mPrevSibling(nullptr)
      , mOnDestroyCalled(false)
      , mWindowType(eWindowType_child)
      , mZIndex(0)

    {
      ClearNativeTouchSequence(nullptr);
    }

        
    /**
     * Create and initialize a widget. 
     *
     * All the arguments can be null in which case a top level window
     * with size 0 is created. The event callback function has to be
     * provided only if the caller wants to deal with the events this
     * widget receives.  The event callback is basically a preprocess
     * hook called synchronously. The return value determines whether
     * the event goes to the default window procedure or it is hidden
     * to the os. The assumption is that if the event handler returns
     * false the widget does not see the event. The widget should not 
     * automatically clear the window to the background color. The 
     * calling code must handle paint messages and clear the background 
     * itself. 
     *
     * In practice at least one of aParent and aNativeParent will be null. If
     * both are null the widget isn't parented (e.g. context menus or
     * independent top level windows).
     *
     * The dimensions given in aRect are specified in the parent's
     * coordinate system, or for parentless widgets such as top-level
     * windows, in global CSS pixels.
     *
     * @param     aParent       parent nsIWidget
     * @param     aNativeParent native parent widget
     * @param     aRect         the widget dimension
     * @param     aInitData     data that is used for widget initialization
     *
     */
    NS_IMETHOD Create(nsIWidget        *aParent,
                      nsNativeWidget   aNativeParent,
                      const nsIntRect  &aRect,
                      nsWidgetInitData *aInitData = nullptr) = 0;

    /**
     * Allocate, initialize, and return a widget that is a child of
     * |this|.  The returned widget (if nonnull) has gone through the
     * equivalent of CreateInstance(widgetCID) + Create(...).
     *
     * |CreateChild()| lets widget backends decide whether to parent
     * the new child widget to this, nonnatively parent it, or both.
     * This interface exists to support the PuppetWidget backend,
     * which is entirely non-native.  All other params are the same as
     * for |Create()|.
     *
     * |aForceUseIWidgetParent| forces |CreateChild()| to only use the
     * |nsIWidget*| this, not its native widget (if it exists), when
     * calling |Create()|.  This is a timid hack around poorly
     * understood code, and shouldn't be used in new code.
     */
    virtual already_AddRefed<nsIWidget>
    CreateChild(const nsIntRect  &aRect,
                nsWidgetInitData *aInitData = nullptr,
                bool             aForceUseIWidgetParent = false) = 0;

    /**
     * Attach to a top level widget. 
     *
     * In cases where a top level chrome widget is being used as a content
     * container, attach a secondary listener and update the device
     * context. The primary widget listener will continue to be called for
     * notifications relating to the top-level window, whereas other
     * notifications such as painting and events will instead be called via
     * the attached listener. SetAttachedWidgetListener should be used to
     * assign the attached listener.
     *
     * aUseAttachedEvents if true, events are sent to the attached listener
     * instead of the normal listener.
     */
    NS_IMETHOD AttachViewToTopLevel(bool aUseAttachedEvents) = 0;

    /**
     * Accessor functions to get and set the attached listener. Used by
     * nsView in connection with AttachViewToTopLevel above.
     */
    virtual void SetAttachedWidgetListener(nsIWidgetListener* aListener) = 0;
    virtual nsIWidgetListener* GetAttachedWidgetListener() = 0;

    /**
     * Accessor functions to get and set the listener which handles various
     * actions for the widget.
     */
    //@{
    virtual nsIWidgetListener* GetWidgetListener() = 0;
    virtual void SetWidgetListener(nsIWidgetListener* alistener) = 0;
    //@}

    /**
     * Close and destroy the internal native window. 
     * This method does not delete the widget.
     */

    NS_IMETHOD Destroy(void) = 0;

    /**
     * Destroyed() returns true if Destroy() has been called already.
     * Otherwise, false.
     */
    bool Destroyed() const { return mOnDestroyCalled; }


    /**
     * Reparent a widget
     *
     * Change the widget's parent. Null parents are allowed.
     *
     * @param     aNewParent   new parent 
     */
    NS_IMETHOD SetParent(nsIWidget* aNewParent) = 0;

    /**
     * Return the parent Widget of this Widget or nullptr if this is a 
     * top level window
     *
     * @return the parent widget or nullptr if it does not have a parent
     *
     */
    virtual nsIWidget* GetParent(void) = 0;

    /**
     * Return the top level Widget of this Widget
     *
     * @return the top level widget
     */
    virtual nsIWidget* GetTopLevelWidget() = 0;

    /**
     * Return the top (non-sheet) parent of this Widget if it's a sheet,
     * or nullptr if this isn't a sheet (or some other error occurred).
     * Sheets are only supported on some platforms (currently only OS X).
     *
     * @return the top (non-sheet) parent widget or nullptr
     *
     */
    virtual nsIWidget* GetSheetWindowParent(void) = 0;

    /**
     * Return the physical DPI of the screen containing the window ...
     * the number of device pixels per inch.
     */
    virtual float GetDPI() = 0;

    /**
     * Returns the CompositorVsyncDispatcher associated with this widget
     */
    virtual CompositorVsyncDispatcher* GetCompositorVsyncDispatcher() = 0;

    /**
     * Return the default scale factor for the window. This is the
     * default number of device pixels per CSS pixel to use. This should
     * depend on OS/platform settings such as the Mac's "UI scale factor"
     * or Windows' "font DPI". This will take into account Gecko preferences
     * overriding the system setting.
     */
    mozilla::CSSToLayoutDeviceScale GetDefaultScale();

    /**
     * Return the Gecko override of the system default scale, if any;
     * returns <= 0.0 if the system scale should be used as-is.
     * nsIWidget::GetDefaultScale() [above] takes this into account.
     * It is exposed here so that code that wants to check for a
     * default-scale override without having a widget on hand can
     * easily access the same value.
     * Note that any scale override is a browser-wide value, whereas
     * the default GetDefaultScale value (when no override is present)
     * may vary between widgets (or screens).
     */
    static double DefaultScaleOverride();

    /**
     * Return the first child of this widget.  Will return null if
     * there are no children.
     */
    nsIWidget* GetFirstChild() const {
        return mFirstChild;
    }
    
    /**
     * Return the last child of this widget.  Will return null if
     * there are no children.
     */
    nsIWidget* GetLastChild() const {
        return mLastChild;
    }

    /**
     * Return the next sibling of this widget
     */
    nsIWidget* GetNextSibling() const {
        return mNextSibling;
    }
    
    /**
     * Set the next sibling of this widget
     */
    void SetNextSibling(nsIWidget* aSibling) {
        mNextSibling = aSibling;
    }
    
    /**
     * Return the previous sibling of this widget
     */
    nsIWidget* GetPrevSibling() const {
        return mPrevSibling;
    }

    /**
     * Set the previous sibling of this widget
     */
    void SetPrevSibling(nsIWidget* aSibling) {
        mPrevSibling = aSibling;
    }

    /**
     * Show or hide this widget
     *
     * @param aState true to show the Widget, false to hide it
     *
     */
    NS_IMETHOD Show(bool aState) = 0;

    /**
     * Make the window modal
     *
     */
    NS_IMETHOD SetModal(bool aModal) = 0;

    /**
     * The maximum number of simultaneous touch contacts supported by the device.
     * In the case of devices with multiple digitizers (e.g. multiple touch screens),
     * the value will be the maximum of the set of maximum supported contacts by
     * each individual digitizer.
     */
    virtual uint32_t GetMaxTouchPoints() const = 0;

    /**
     * Returns whether the window is visible
     *
     */
    virtual bool IsVisible() const = 0;

    /**
     * Perform platform-dependent sanity check on a potential window position.
     * This is guaranteed to work only for top-level windows.
     *
     * @param aAllowSlop: if true, allow the window to slop offscreen;
     *                    the window should be partially visible. if false,
     *                    force the entire window onscreen (or at least
     *                    the upper-left corner, if it's too large).
     * @param aX in: an x position expressed in screen coordinates.
     *           out: the x position constrained to fit on the screen(s).
     * @param aY in: an y position expressed in screen coordinates.
     *           out: the y position constrained to fit on the screen(s).
     * @return vapid success indication. but see also the parameters.
     *
     **/
    NS_IMETHOD ConstrainPosition(bool aAllowSlop,
                                 int32_t *aX,
                                 int32_t *aY) = 0;

    /**
     * NOTE:
     *
     * For a top-level window widget, the "parent's coordinate system" is the
     * "global" display pixel coordinate space, *not* device pixels (which
     * may be inconsistent between multiple screens, at least in the Mac OS
     * case with mixed hi-dpi and lo-dpi displays). This applies to all the
     * following Move and Resize widget APIs.
     *
     * The display-/device-pixel distinction becomes important for (at least)
     * Mac OS X with Hi-DPI (retina) displays, and Windows when the UI scale
     * factor is set to other than 100%.
     *
     * The Move and Resize methods take floating-point parameters, rather than
     * integer ones. This is important when manipulating top-level widgets,
     * where the coordinate system may not be an integral multiple of the
     * device-pixel space.
     **/

    /**
     * Move this widget.
     *
     * Coordinates refer to the top-left of the widget.  For toplevel windows
     * with decorations, this is the top-left of the titlebar and frame .
     *
     * @param aX the new x position expressed in the parent's coordinate system
     * @param aY the new y position expressed in the parent's coordinate system
     *
     **/
    NS_IMETHOD Move(double aX, double aY) = 0;

    /**
     * Reposition this widget so that the client area has the given offset.
     *
     * @param aX       the new x offset of the client area expressed as an
     *                 offset from the origin of the client area of the parent
     *                 widget (for root widgets and popup widgets it is in
     *                 screen coordinates)
     * @param aY       the new y offset of the client area expressed as an
     *                 offset from the origin of the client area of the parent
     *                 widget (for root widgets and popup widgets it is in
     *                 screen coordinates)
     *
     **/
    NS_IMETHOD MoveClient(double aX, double aY) = 0;

    /**
     * Resize this widget. Any size constraints set for the window by a
     * previous call to SetSizeConstraints will be applied.
     *
     * @param aWidth  the new width expressed in the parent's coordinate system
     * @param aHeight the new height expressed in the parent's coordinate system
     * @param aRepaint whether the widget should be repainted
     *
     */
    NS_IMETHOD Resize(double aWidth,
                      double aHeight,
                      bool   aRepaint) = 0;

    /**
     * Move or resize this widget. Any size constraints set for the window by
     * a previous call to SetSizeConstraints will be applied.
     *
     * @param aX       the new x position expressed in the parent's coordinate system
     * @param aY       the new y position expressed in the parent's coordinate system
     * @param aWidth   the new width expressed in the parent's coordinate system
     * @param aHeight  the new height expressed in the parent's coordinate system
     * @param aRepaint whether the widget should be repainted if the size changes
     *
     */
    NS_IMETHOD Resize(double aX,
                      double aY,
                      double aWidth,
                      double aHeight,
                      bool   aRepaint) = 0;

    /**
     * Resize the widget so that the inner client area has the given size.
     *
     * @param aWidth   the new width of the client area.
     * @param aHeight  the new height of the client area.
     * @param aRepaint whether the widget should be repainted
     *
     */
    NS_IMETHOD ResizeClient(double aWidth,
                            double aHeight,
                            bool   aRepaint) = 0;

    /**
     * Resize and reposition the widget so tht inner client area has the given
     * offset and size.
     *
     * @param aX       the new x offset of the client area expressed as an
     *                 offset from the origin of the client area of the parent
     *                 widget (for root widgets and popup widgets it is in
     *                 screen coordinates)
     * @param aY       the new y offset of the client area expressed as an
     *                 offset from the origin of the client area of the parent
     *                 widget (for root widgets and popup widgets it is in
     *                 screen coordinates)
     * @param aWidth   the new width of the client area.
     * @param aHeight  the new height of the client area.
     * @param aRepaint whether the widget should be repainted
     *
     */
    NS_IMETHOD ResizeClient(double aX,
                            double aY,
                            double aWidth,
                            double aHeight,
                            bool   aRepaint) = 0;

    /**
     * Sets the widget's z-index.
     */
    virtual void SetZIndex(int32_t aZIndex) = 0;

    /**
     * Gets the widget's z-index. 
     */
    int32_t GetZIndex()
    {
      return mZIndex;
    }

    /**
     * Position this widget just behind the given widget. (Used to
     * control z-order for top-level widgets. Get/SetZIndex by contrast
     * control z-order for child widgets of other widgets.)
     * @param aPlacement top, bottom, or below a widget
     *                   (if top or bottom, param aWidget is ignored)
     * @param aWidget    widget to place this widget behind
     *                   (only if aPlacement is eZPlacementBelow).
     *                   null is equivalent to aPlacement of eZPlacementTop
     * @param aActivate  true to activate the widget after placing it
     */
    NS_IMETHOD PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                           nsIWidget *aWidget, bool aActivate) = 0;

    /**
     * Minimize, maximize or normalize the window size.
     * Takes a value from nsSizeMode (see nsIWidgetListener.h)
     */
    NS_IMETHOD SetSizeMode(int32_t aMode) = 0;

    /**
     * Return size mode (minimized, maximized, normalized).
     * Returns a value from nsSizeMode (see nsIWidgetListener.h)
     */
    virtual int32_t SizeMode() = 0;

    /**
     * Enable or disable this Widget
     *
     * @param aState true to enable the Widget, false to disable it.
     *
     */
    NS_IMETHOD Enable(bool aState) = 0;

    /**
     * Ask whether the widget is enabled
     */
    virtual bool IsEnabled() const = 0;

    /**
     * Request activation of this window or give focus to this widget.
     *
     * @param aRaise If true, this function requests activation of this
     *               widget's toplevel window.
     *               If false, the appropriate toplevel window (which in
     *               the case of popups may not be this widget's toplevel
     *               window) is already active.
     */
    NS_IMETHOD SetFocus(bool aRaise = false) = 0;

    /**
     * Get this widget's outside dimensions relative to its parent widget. For
     * popup widgets the returned rect is in screen coordinates and not
     * relative to its parent widget.
     *
     * @param aRect   On return it holds the  x, y, width and height of
     *                this widget.
     */
    NS_IMETHOD GetBounds(nsIntRect &aRect) = 0;

    /**
     * Get this widget's outside dimensions in global coordinates. This
     * includes any title bar on the window.
     *
     * @param aRect   On return it holds the  x, y, width and height of
     *                this widget.
     */
    NS_IMETHOD GetScreenBounds(nsIntRect &aRect) = 0;

    /**
     * Similar to GetScreenBounds except that this function will always
     * get the size when the widget is in the nsSizeMode_Normal size mode
     * even if the current size mode is not nsSizeMode_Normal.
     * This method will fail if the size mode is not nsSizeMode_Normal and
     * the platform doesn't have the ability.
     * This method will always succeed if the current size mode is
     * nsSizeMode_Normal.
     *
     * @param aRect   On return it holds the  x, y, width and height of
     *                this widget.
     */
    NS_IMETHOD GetRestoredBounds(nsIntRect &aRect) = 0;

    /**
     * Get this widget's client area bounds, if the window has a 3D border
     * appearance this returns the area inside the border. The position is the
     * position of the client area relative to the client area of the parent
     * widget (for root widgets and popup widgets it is in screen coordinates).
     *
     * @param aRect   On return it holds the  x. y, width and height of
     *                the client area of this widget.
     */
    NS_IMETHOD GetClientBounds(nsIntRect &aRect) = 0;

    /**
     * Get the non-client area dimensions of the window.
     * 
     */
    NS_IMETHOD GetNonClientMargins(nsIntMargin &margins) = 0;

    /**
     * Sets the non-client area dimensions of the window. Pass -1 to restore
     * the system default frame size for that border. Pass zero to remove
     * a border, or pass a specific value adjust a border. Units are in
     * pixels. (DPI dependent)
     *
     * Platform notes:
     *  Windows: shrinking top non-client height will remove application
     *  icon and window title text. Glass desktops will refuse to set
     *  dimensions between zero and size < system default.
     *
     */
    NS_IMETHOD SetNonClientMargins(nsIntMargin &margins) = 0;

    /**
     * Get the client offset from the window origin.
     *
     * @return the x and y of the offset.
     *
     */
    virtual nsIntPoint GetClientOffset() = 0;


    /**
     * Equivalent to GetClientBounds but only returns the size.
     */
    virtual mozilla::gfx::IntSize GetClientSize() {
      // Dependeing on the backend, overloading this method may be useful if
      // if requesting the client offset is expensive.
      nsIntRect rect;
      GetClientBounds(rect);
      return mozilla::gfx::IntSize(rect.width, rect.height);
    }

    /**
     * Set the background color for this widget
     *
     * @param aColor the new background color
     *
     */

    virtual void SetBackgroundColor(const nscolor &aColor) { }

    /**
     * Get the cursor for this widget.
     *
     * @return this widget's cursor.
     */

    virtual nsCursor GetCursor(void) = 0;

    /**
     * Set the cursor for this widget
     *
     * @param aCursor the new cursor for this widget
     */

    NS_IMETHOD SetCursor(nsCursor aCursor) = 0;

    /**
     * If a cursor type is currently cached locally for this widget, clear the
     * cached cursor to force an update on the next SetCursor call.
     */

    virtual void ClearCachedCursor() = 0;

    /**
     * Sets an image as the cursor for this widget.
     *
     * @param aCursor the cursor to set
     * @param aX the X coordinate of the hotspot (from left).
     * @param aY the Y coordinate of the hotspot (from top).
     * @retval NS_ERROR_NOT_IMPLEMENTED if setting images as cursors is not
     *         supported
     */
    NS_IMETHOD SetCursor(imgIContainer* aCursor,
                         uint32_t aHotspotX, uint32_t aHotspotY) = 0;

    /** 
     * Get the window type of this widget.
     */
    nsWindowType WindowType() { return mWindowType; }

    /**
     * Determines if this widget is one of the three types of plugin widgets.
     */
    bool IsPlugin() {
      return mWindowType == eWindowType_plugin ||
             mWindowType == eWindowType_plugin_ipc_chrome ||
             mWindowType == eWindowType_plugin_ipc_content;
    }

    /**
     * Set the transparency mode of the top-level window containing this widget.
     * So, e.g., if you call this on the widget for an IFRAME, the top level
     * browser window containing the IFRAME actually gets set. Be careful.
     *
     * This can fail if the platform doesn't support
     * transparency/glass. By default widgets are not
     * transparent.  This will also fail if the toplevel window is not
     * a Mozilla window, e.g., if the widget is in an embedded
     * context.
     *
     * After transparency/glass has been enabled, the initial alpha channel
     * value for all pixels is 1, i.e., opaque.
     * If the window is resized then the alpha channel values for
     * all pixels are reset to 1.
     * Pixel RGB color values are already premultiplied with alpha channel values.
     */
    virtual void SetTransparencyMode(nsTransparencyMode aMode) = 0;

    /**
     * Get the transparency mode of the top-level window that contains this
     * widget.
     */
    virtual nsTransparencyMode GetTransparencyMode() = 0;

    /**
     * This represents a command to set the bounds and clip region of
     * a child widget.
     */
    struct Configuration {
        nsIWidget* mChild;
        uintptr_t mWindowID; // e10s specific, the unique plugin port id
        bool mVisible; // e10s specific, widget visibility
        nsIntRect mBounds;
        nsTArray<nsIntRect> mClipRegion;
    };

    /**
     * Sets the clip region of each mChild (which must actually be a child
     * of this widget) to the union of the pixel rects given in
     * mClipRegion, all relative to the top-left of the child
     * widget. Clip regions are not implemented on all platforms and only
     * need to actually work for children that are plugins.
     *
     * Also sets the bounds of each child to mBounds.
     *
     * This will invalidate areas of the children that have changed, but
     * does not need to invalidate any part of this widget.
     *
     * Children should be moved in the order given; the array is
     * sorted so to minimize unnecessary invalidation if children are
     * moved in that order.
     */
    virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations) = 0;
    virtual nsresult SetWindowClipRegion(const nsTArray<nsIntRect>& aRects,
                                         bool aIntersectWithExisting) = 0;

    /**
     * Appends to aRects the rectangles constituting this widget's clip
     * region. If this widget is not clipped, appends a single rectangle
     * (0, 0, bounds.width, bounds.height).
     */
    virtual void GetWindowClipRegion(nsTArray<nsIntRect>* aRects) = 0;

    /**
     * Register or unregister native plugin widgets which receive Configuration
     * data from the content process via the compositor.
     *
     * Lookups are used by the main thread via the compositor to lookup widgets
     * based on a unique window id. On Windows and Linux this is the
     * NS_NATIVE_PLUGIN_PORT (hwnd/XID). This tracking maintains a reference to
     * widgets held. Consumers are responsible for removing widgets from this
     * list.
     */
    virtual void RegisterPluginWindowForRemoteUpdates() = 0;
    virtual void UnregisterPluginWindowForRemoteUpdates() = 0;
    static nsIWidget* LookupRegisteredPluginWindow(uintptr_t aWindowID);

    /**
     * Iterates across the list of registered plugin widgets and updates thier
     * visibility based on which plugins are included in the 'visible' list.
     *
     * The compositor knows little about tabs, but it does know which plugin
     * widgets are currently included in the visible layer tree. It calls this
     * helper to hide widgets it knows nothing about.
     */
    static void UpdateRegisteredPluginWindowVisibility(nsTArray<uintptr_t>& aVisibleList);

    /**
     * Set the shadow style of the window.
     *
     * Ignored on child widgets and on non-Mac platforms.
     */
    NS_IMETHOD SetWindowShadowStyle(int32_t aStyle) = 0;

    /*
     * On Mac OS X, this method shows or hides the pill button in the titlebar
     * that's used to collapse the toolbar.
     *
     * Ignored on child widgets and on non-Mac platforms.
     */
    virtual void SetShowsToolbarButton(bool aShow) = 0;

    /*
     * On Mac OS X Lion, this method shows or hides the full screen button in
     * the titlebar that handles native full screen mode.
     *
     * Ignored on child widgets, non-Mac platforms, & pre-Lion Mac.
     */
    virtual void SetShowsFullScreenButton(bool aShow) = 0;

    enum WindowAnimationType {
      eGenericWindowAnimation,
      eDocumentWindowAnimation
    };

    /**
     * Sets the kind of top-level window animation this widget should have.  On
     * Mac OS X, this causes a particular kind of animation to be shown when the
     * window is first made visible.
     *
     * Ignored on child widgets and on non-Mac platforms.
     */
    virtual void SetWindowAnimationType(WindowAnimationType aType) = 0;

    /**
     * Specifies whether the window title should be drawn even if the window
     * contents extend into the titlebar. Ignored on windows that don't draw
     * in the titlebar. Only implemented on OS X.
     */
    virtual void SetDrawsTitle(bool aDrawTitle) {}

    /**
     * Indicates whether the widget should attempt to make titlebar controls
     * easier to see on dark titlebar backgrounds.
     */
    virtual void SetUseBrightTitlebarForeground(bool aBrightForeground) {}

    /** 
     * Hide window chrome (borders, buttons) for this widget.
     *
     */
    NS_IMETHOD HideWindowChrome(bool aShouldHide) = 0;

    /**
     * Put the toplevel window into or out of fullscreen mode.
     * If aTargetScreen is given, attempt to go fullscreen on that screen,
     * if possible.  (If not, it behaves as if aTargetScreen is null.)
     * If !aFullScreen, aTargetScreen is ignored.
     * aTargetScreen support is currently only implemented on Windows.
     */
    NS_IMETHOD MakeFullScreen(bool aFullScreen, nsIScreen* aTargetScreen = nullptr) = 0;

    /**
     * Invalidate a specified rect for a widget so that it will be repainted
     * later.
     */
    NS_IMETHOD Invalidate(const nsIntRect & aRect) = 0;

    enum LayerManagerPersistence
    {
      LAYER_MANAGER_CURRENT = 0,
      LAYER_MANAGER_PERSISTENT
    };

    /**
     * Return the widget's LayerManager. The layer tree for that
     * LayerManager is what gets rendered to the widget.
     *
     * @param aAllowRetaining an outparam that states whether the returned
     * layer manager should be used for retained layers
     */
    inline LayerManager* GetLayerManager(bool* aAllowRetaining = nullptr)
    {
        return GetLayerManager(nullptr, mozilla::layers::LayersBackend::LAYERS_NONE,
                               LAYER_MANAGER_CURRENT, aAllowRetaining);
    }

    inline LayerManager* GetLayerManager(LayerManagerPersistence aPersistence,
                                         bool* aAllowRetaining = nullptr)
    {
        return GetLayerManager(nullptr, mozilla::layers::LayersBackend::LAYERS_NONE,
                               aPersistence, aAllowRetaining);
    }

    /**
     * Like GetLayerManager(), but prefers creating a layer manager of
     * type |aBackendHint| instead of what would normally be created.
     * LayersBackend::LAYERS_NONE means "no hint".
     */
    virtual LayerManager* GetLayerManager(PLayerTransactionChild* aShadowManager,
                                          LayersBackend aBackendHint,
                                          LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                          bool* aAllowRetaining = nullptr) = 0;

    /**
     * Called before each layer manager transaction to allow any preparation
     * for DrawWindowUnderlay/Overlay that needs to be on the main thread.
     *
     * Always called on the main thread.
     */
    virtual void PrepareWindowEffects() = 0;

    /**
     * Called when shutting down the LayerManager to clean-up any cached resources.
     *
     * Always called from the compositing thread, which may be the main-thread if
     * OMTC is not enabled.
     */
    virtual void CleanupWindowEffects() = 0;

    /**
     * Called before rendering using OMTC. Returns false when the widget is
     * not ready to be rendered (for example while the window is closed).
     *
     * Always called from the compositing thread, which may be the main-thread if
     * OMTC is not enabled.
     */
    virtual bool PreRender(LayerManagerComposite* aManager) = 0;

    /**
     * Called after rendering using OMTC. Not called when rendering was
     * cancelled by a negative return value from PreRender.
     *
     * Always called from the compositing thread, which may be the main-thread if
     * OMTC is not enabled.
     */
    virtual void PostRender(LayerManagerComposite* aManager) = 0;

    /**
     * Called before the LayerManager draws the layer tree.
     *
     * Always called from the compositing thread.
     */
    virtual void DrawWindowUnderlay(LayerManagerComposite* aManager, nsIntRect aRect) = 0;

    /**
     * Called after the LayerManager draws the layer tree
     *
     * Always called from the compositing thread.
     */
    virtual void DrawWindowOverlay(LayerManagerComposite* aManager, nsIntRect aRect) = 0;

    /**
     * Return a DrawTarget for the window which can be composited into.
     *
     * Called by BasicCompositor on the compositor thread for OMTC drawing
     * before each composition.
     */
    virtual mozilla::TemporaryRef<mozilla::gfx::DrawTarget> StartRemoteDrawing() = 0;

    /**
     * Ensure that what was painted into the DrawTarget returned from
     * StartRemoteDrawing reaches the screen.
     *
     * Called by BasicCompositor on the compositor thread for OMTC drawing
     * after each composition.
     */
    virtual void EndRemoteDrawing() = 0;

    /**
     * Clean up any resources used by Start/EndRemoteDrawing.
     *
     * Called by BasicCompositor on the compositor thread for OMTC drawing
     * when the compositor is destroyed.
     */
    virtual void CleanupRemoteDrawing() = 0;

    /**
     * Called when Gecko knows which themed widgets exist in this window.
     * The passed array contains an entry for every themed widget of the right
     * type (currently only NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR and
     * NS_THEME_TOOLBAR) within the window, except for themed widgets which are
     * transformed or have effects applied to them (e.g. CSS opacity or
     * filters).
     * This could sometimes be called during display list construction
     * outside of painting.
     * If called during painting, it will be called before we actually
     * paint anything.
     */
    virtual void UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries) = 0;

    /**
     * Informs the widget about the region of the window that is opaque.
     *
     * @param aOpaqueRegion the region of the window that is opaque.
     */
    virtual void UpdateOpaqueRegion(const nsIntRegion &aOpaqueRegion) {}

    /**
     * Informs the widget about the region of the window that is draggable.
     */
    virtual void UpdateWindowDraggingRegion(const nsIntRegion& aRegion) {}

    /**
     * Internal methods
     */

    //@{
    virtual void AddChild(nsIWidget* aChild) = 0;
    virtual void RemoveChild(nsIWidget* aChild) = 0;
    virtual void* GetNativeData(uint32_t aDataType) = 0;
    virtual void SetNativeData(uint32_t aDataType, uintptr_t aVal) = 0;
    virtual void FreeNativeData(void * data, uint32_t aDataType) = 0;//~~~

    //@}

    /**
     * Set the widget's title.
     * Must be called after Create.
     *
     * @param aTitle string displayed as the title of the widget
     */

    NS_IMETHOD SetTitle(const nsAString& aTitle) = 0;

    /**
     * Set the widget's icon.
     * Must be called after Create.
     *
     * @param anIconSpec string specifying the icon to use; convention is to pass
     *                   a resource: URL from which a platform-dependent resource
     *                   file name will be constructed
     */

    NS_IMETHOD SetIcon(const nsAString& anIconSpec) = 0;

    /**
     * Return this widget's origin in screen coordinates. The untyped version
     * exists temporarily to ease conversion to typed coordinates.
     *
     * @return screen coordinates stored in the x,y members
     */

    virtual mozilla::LayoutDeviceIntPoint WidgetToScreenOffset() = 0;
    virtual nsIntPoint WidgetToScreenOffsetUntyped() {
      return mozilla::LayoutDeviceIntPoint::ToUntyped(WidgetToScreenOffset());
    }

    /**
     * Given the specified client size, return the corresponding window size,
     * which includes the area for the borders and titlebar. This method
     * should work even when the window is not yet visible.
     */
    virtual mozilla::LayoutDeviceIntSize ClientToWindowSize(
                const mozilla::LayoutDeviceIntSize& aClientSize) = 0;

    /**
     * Dispatches an event to the widget
     *
     */
    NS_IMETHOD DispatchEvent(mozilla::WidgetGUIEvent* event,
                             nsEventStatus & aStatus) = 0;

    /**
     * Dispatches an event that must be handled by APZ first, when APZ is
     * enabled. If invoked in the child process, it is forwarded to the
     * parent process synchronously.
     */
    virtual nsEventStatus DispatchAPZAwareEvent(mozilla::WidgetInputEvent* aEvent) = 0;

    /**
     * Dispatches an event that must be transformed by APZ first, but is not
     * actually handled by APZ. If invoked in the child process, it is
     * forwarded to the parent process synchronously.
     */
    virtual nsEventStatus DispatchInputEvent(mozilla::WidgetInputEvent* aEvent) = 0;

    /**
     * Enables the dropping of files to a widget (XXX this is temporary)
     *
     */
    NS_IMETHOD EnableDragDrop(bool aEnable) = 0;
   
    /**
     * Enables/Disables system mouse capture.
     * @param aCapture true enables mouse capture, false disables mouse capture 
     *
     */
    NS_IMETHOD CaptureMouse(bool aCapture) = 0;

    /**
     * Classify the window for the window manager. Mostly for X11.
     */
    NS_IMETHOD SetWindowClass(const nsAString& xulWinType) = 0;

    /**
     * Enables/Disables system capture of any and all events that would cause a
     * popup to be rolled up. aListener should be set to a non-null value for
     * any popups that are not managed by the popup manager.
     * @param aDoCapture true enables capture, false disables capture 
     *
     */
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener* aListener, bool aDoCapture) = 0;

    /**
     * Bring this window to the user's attention.  This is intended to be a more
     * gentle notification than popping the window to the top or putting up an
     * alert.  See, for example, Win32 FlashWindow or the NotificationManager on
     * the Mac.  The notification should be suppressed if the window is already
     * in the foreground and should be dismissed when the user brings this window
     * to the foreground.
     * @param aCycleCount Maximum number of times to animate the window per system 
     *                    conventions. If set to -1, cycles indefinitely until 
     *                    window is brought into the foreground.
     */
    NS_IMETHOD GetAttention(int32_t aCycleCount) = 0;

    /**
     * Ask whether there user input events pending.  All input events are
     * included, including those not targeted at this nsIwidget instance.
     */
    virtual bool HasPendingInputEvent() = 0;

    /**
     * Set the background color of the window titlebar for this widget. On Mac,
     * for example, this will remove the grey gradient and bottom border and
     * instead show a single, solid color.
     *
     * Ignored on any platform that does not support it. Ignored by widgets that
     * do not represent windows.
     *
     * @param aColor  The color to set the title bar background to. Alpha values 
     *                other than fully transparent (0) are respected if possible  
     *                on the platform. An alpha of 0 will cause the window to 
     *                draw with the default style for the platform.
     *
     * @param aActive Whether the color should be applied to active or inactive
     *                windows.
     */
    NS_IMETHOD SetWindowTitlebarColor(nscolor aColor, bool aActive) = 0;

    /**
     * If set to true, the window will draw its contents into the titlebar
     * instead of below it.
     *
     * Ignored on any platform that does not support it. Ignored by widgets that
     * do not represent windows.
     * May result in a resize event, so should only be called from places where
     * reflow and painting is allowed.
     *
     * @param aState Whether drawing into the titlebar should be activated.
     */
    virtual void SetDrawsInTitlebar(bool aState) = 0;

    /*
     * Determine whether the widget shows a resize widget. If it does,
     * aResizerRect returns the resizer's rect.
     *
     * Returns false on any platform that does not support it.
     *
     * @param aResizerRect The resizer's rect in device pixels.
     * @return Whether a resize widget is shown.
     */
    virtual bool ShowsResizeIndicator(nsIntRect* aResizerRect) = 0;

    /**
     * Return the popup that was last rolled up, or null if there isn't one.
     */
    virtual nsIContent* GetLastRollup() = 0;

    /**
     * Begin a window resizing drag, based on the event passed in.
     */
    NS_IMETHOD BeginResizeDrag(mozilla::WidgetGUIEvent* aEvent,
                               int32_t aHorizontal,
                               int32_t aVertical) = 0;

    /**
     * Begin a window moving drag, based on the event passed in.
     */
    NS_IMETHOD BeginMoveDrag(mozilla::WidgetMouseEvent* aEvent) = 0;

    enum Modifiers {
        CAPS_LOCK = 0x01, // when CapsLock is active
        NUM_LOCK = 0x02, // when NumLock is active
        SHIFT_L = 0x0100,
        SHIFT_R = 0x0200,
        CTRL_L = 0x0400,
        CTRL_R = 0x0800,
        ALT_L = 0x1000, // includes Option
        ALT_R = 0x2000,
        COMMAND_L = 0x4000,
        COMMAND_R = 0x8000,
        HELP = 0x10000,
        FUNCTION = 0x100000,
        NUMERIC_KEY_PAD = 0x01000000 // when the key is coming from the keypad
    };
    /**
     * Utility method intended for testing. Dispatches native key events
     * to this widget to simulate the press and release of a key.
     * @param aNativeKeyboardLayout a *platform-specific* constant.
     * On Mac, this is the resource ID for a 'uchr' or 'kchr' resource.
     * On Windows, it is converted to a hex string and passed to
     * LoadKeyboardLayout, see
     * http://msdn.microsoft.com/en-us/library/ms646305(VS.85).aspx
     * @param aNativeKeyCode a *platform-specific* keycode.
     * On Windows, this is the virtual key code.
     * @param aModifiers some combination of the above 'Modifiers' flags;
     * not all flags will apply to all platforms. Mac ignores the _R
     * modifiers. Windows ignores COMMAND, NUMERIC_KEY_PAD, HELP and
     * FUNCTION.
     * @param aCharacters characters that the OS would decide to generate
     * from the event. On Windows, this is the charCode passed by
     * WM_CHAR.
     * @param aUnmodifiedCharacters characters that the OS would decide
     * to generate from the event if modifier keys (other than shift)
     * were assumed inactive. Needed on Mac, ignored on Windows.
     * @param aObserver the observer that will get notified once the events
     * have been dispatched.
     * @return NS_ERROR_NOT_AVAILABLE to indicate that the keyboard
     * layout is not supported and the event was not fired
     */
    virtual nsresult SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                              int32_t aNativeKeyCode,
                                              uint32_t aModifierFlags,
                                              const nsAString& aCharacters,
                                              const nsAString& aUnmodifiedCharacters,
                                              nsIObserver* aObserver) = 0;

    /**
     * Utility method intended for testing. Dispatches native mouse events
     * may even move the mouse cursor. On Mac the events are guaranteed to
     * be sent to the window containing this widget, but on Windows they'll go
     * to whatever's topmost on the screen at that position, so for
     * cross-platform testing ensure that your window is at the top of the
     * z-order.
     * @param aPoint screen location of the mouse, in device
     * pixels, with origin at the top left
     * @param aNativeMessage *platform-specific* event type (e.g. on Mac,
     * NSMouseMoved; on Windows, MOUSEEVENTF_MOVE, MOUSEEVENTF_LEFTDOWN etc)
     * @param aModifierFlags *platform-specific* modifier flags (ignored
     * on Windows)
     * @param aObserver the observer that will get notified once the events
     * have been dispatched.
     */
    virtual nsresult SynthesizeNativeMouseEvent(mozilla::LayoutDeviceIntPoint aPoint,
                                                uint32_t aNativeMessage,
                                                uint32_t aModifierFlags,
                                                nsIObserver* aObserver) = 0;

    /**
     * A shortcut to SynthesizeNativeMouseEvent, abstracting away the native message.
     * aPoint is location in device pixels to which the mouse pointer moves to.
     * @param aObserver the observer that will get notified once the events
     * have been dispatched.
     */
    virtual nsresult SynthesizeNativeMouseMove(mozilla::LayoutDeviceIntPoint aPoint,
                                               nsIObserver* aObserver) = 0;

    /**
     * Utility method intended for testing. Dispatching native mouse scroll
     * events may move the mouse cursor.
     *
     * @param aPoint            Mouse cursor position in screen coordinates.
     *                          In device pixels, the origin at the top left of
     *                          the primary display.
     * @param aNativeMessage    Platform native message.
     * @param aDeltaX           The delta value for X direction.  If the native
     *                          message doesn't indicate X direction scrolling,
     *                          this may be ignored.
     * @param aDeltaY           The delta value for Y direction.  If the native
     *                          message doesn't indicate Y direction scrolling,
     *                          this may be ignored.
     * @param aDeltaZ           The delta value for Z direction.  If the native
     *                          message doesn't indicate Z direction scrolling,
     *                          this may be ignored.
     * @param aModifierFlags    Must be values of Modifiers, or zero.
     * @param aAdditionalFlags  See nsIDOMWidnowUtils' consts and their
     *                          document.
     * @param aObserver         The observer that will get notified once the
     *                          events have been dispatched.
     */
    virtual nsresult SynthesizeNativeMouseScrollEvent(mozilla::LayoutDeviceIntPoint aPoint,
                                                      uint32_t aNativeMessage,
                                                      double aDeltaX,
                                                      double aDeltaY,
                                                      double aDeltaZ,
                                                      uint32_t aModifierFlags,
                                                      uint32_t aAdditionalFlags,
                                                      nsIObserver* aObserver) = 0;

    /*
     * TouchPointerState states for SynthesizeNativeTouchPoint. Match
     * touch states in nsIDOMWindowUtils.idl.
     */
    enum TouchPointerState {
      // The pointer is in a hover state above the digitizer
      TOUCH_HOVER    = 0x01,
      // The pointer is in contact with the digitizer
      TOUCH_CONTACT  = 0x02,
      // The pointer has been removed from the digitizer detection area
      TOUCH_REMOVE   = 0x04,
      // The pointer has been canceled. Will cancel any pending os level
      // gestures that would triggered as a result of completion of the
      // input sequence. This may not cancel moz platform related events
      // that might get tirggered by input already delivered.
      TOUCH_CANCEL   = 0x08
    };

    /*
     * Create a new or update an existing touch pointer on the digitizer.
     * To trigger os level gestures, individual touch points should
     * transition through a complete set of touch states which should be
     * sent as individual messages.
     *
     * @param aPointerId The touch point id to create or update.
     * @param aPointerState one or more of the touch states listed above
     * @param aScreenX, aScreenY screen coords of this event
     * @param aPressure 0.0 -> 1.0 float val indicating pressure
     * @param aOrientation 0 -> 359 degree value indicating the
     * orientation of the pointer. Use 90 for normal taps.
     * @param aObserver The observer that will get notified once the events
     * have been dispatched.
     */
    virtual nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                                TouchPointerState aPointerState,
                                                nsIntPoint aPointerScreenPoint,
                                                double aPointerPressure,
                                                uint32_t aPointerOrientation,
                                                nsIObserver* aObserver) = 0;

    /*
     * Helper for simulating a simple tap event with one touch point. When
     * aLongTap is true, simulates a native long tap with a duration equal to
     * ui.click_hold_context_menus.delay. This pref is compatible with the
     * apzc long tap duration. Defaults to 1.5 seconds.
     * @param aObserver The observer that will get notified once the events
     * have been dispatched.
     */
    nsresult SynthesizeNativeTouchTap(nsIntPoint aPointerScreenPoint,
                                      bool aLongTap,
                                      nsIObserver* aObserver);

    /*
     * Cancels all active simulated touch input points and pending long taps.
     * Native widgets should track existing points such that they can clear the
     * digitizer state when this call is made.
     * @param aObserver The observer that will get notified once the touch
     * sequence has been cleared.
     */
    virtual nsresult ClearNativeTouchSequence(nsIObserver* aObserver);

private:
  class LongTapInfo
  {
  public:
    LongTapInfo(int32_t aPointerId, nsIntPoint& aPoint,
                mozilla::TimeDuration aDuration,
                nsIObserver* aObserver) :
      mPointerId(aPointerId),
      mPosition(aPoint),
      mDuration(aDuration),
      mObserver(aObserver),
      mStamp(mozilla::TimeStamp::Now())
    {
    }

    int32_t mPointerId;
    nsIntPoint mPosition;
    mozilla::TimeDuration mDuration;
    nsCOMPtr<nsIObserver> mObserver;
    mozilla::TimeStamp mStamp;
  };

  static void OnLongTapTimerCallback(nsITimer* aTimer, void* aClosure);

  nsAutoPtr<LongTapInfo> mLongTapTouchPoint;
  nsCOMPtr<nsITimer> mLongTapTimer;
  static int32_t sPointerIdCounter;

public:
    /**
     * Activates a native menu item at the position specified by the index
     * string. The index string is a string of positive integers separated
     * by the "|" (pipe) character. The last integer in the string represents
     * the item index in a submenu located using the integers preceding it.
     *
     * Example: 1|0|4
     * In this string, the first integer represents the top-level submenu
     * in the native menu bar. Since the integer is 1, it is the second submeu
     * in the native menu bar. Within that, the first item (index 0) is a
     * submenu, and we want to activate the 5th item within that submenu.
     */
    virtual nsresult ActivateNativeMenuItemAt(const nsAString& indexString) = 0;

    /**
     * This is used for native menu system testing.
     *
     * Updates a native menu at the position specified by the index string.
     * The index string is a string of positive integers separated by the "|" 
     * (pipe) character.
     *
     * Example: 1|0|4
     * In this string, the first integer represents the top-level submenu
     * in the native menu bar. Since the integer is 1, it is the second submeu
     * in the native menu bar. Within that, the first item (index 0) is a
     * submenu, and we want to update submenu at index 4 within that submenu.
     *
     * If this is called with an empty string it forces a full reload of the
     * menu system.
     */
    virtual nsresult ForceUpdateNativeMenuAt(const nsAString& indexString) = 0;

    /**
     * Notify IME of the specified notification.
     *
     * @return If the notification is mouse button event and it's consumed by
     *         IME, this returns NS_SUCCESS_EVENT_CONSUMED.
     */
    NS_IMETHOD NotifyIME(const IMENotification& aIMENotification) = 0;

    /**
     * Start plugin IME.  If this results in a string getting committed, the
     * result is in aCommitted (otherwise aCommitted is empty).
     *
     * aKeyboardEvent     The event with which plugin IME is to be started
     * panelX and panelY  Location in screen coordinates of the IME input panel
     *                    (should be just under the plugin)
     * aCommitted         The string committed during IME -- otherwise empty
     */
    NS_IMETHOD StartPluginIME(const mozilla::WidgetKeyboardEvent& aKeyboardEvent,
                              int32_t aPanelX, int32_t aPanelY,
                              nsString& aCommitted) = 0;

    /**
     * Tells the widget whether or not a plugin (inside the widget) has the
     * keyboard focus.  Should be sent when the keyboard focus changes too or
     * from a plugin.
     *
     * aFocused  Whether or not a plugin is focused
     */
    NS_IMETHOD SetPluginFocused(bool& aFocused) = 0;

    /*
     * Notifies the input context changes.
     */
    NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                      const InputContextAction& aAction) = 0;

    /*
     * Get current input context.
     */
    NS_IMETHOD_(InputContext) GetInputContext() = 0;

    /*
     * Given a WidgetKeyboardEvent, this method synthesizes a corresponding
     * native (OS-level) event for it. This method allows tests to simulate
     * keystrokes that trigger native key bindings (which require a native
     * event).
     */
    NS_IMETHOD AttachNativeKeyEvent(mozilla::WidgetKeyboardEvent& aEvent) = 0;

    /*
     * Execute native key bindings for aType.
     */
    typedef void (*DoCommandCallback)(mozilla::Command, void*);
    enum NativeKeyBindingsType
    {
      NativeKeyBindingsForSingleLineEditor,
      NativeKeyBindingsForMultiLineEditor,
      NativeKeyBindingsForRichTextEditor
    };
    NS_IMETHOD_(bool) ExecuteNativeKeyBinding(
                        NativeKeyBindingsType aType,
                        const mozilla::WidgetKeyboardEvent& aEvent,
                        DoCommandCallback aCallback,
                        void* aCallbackData) = 0;

    /*
     * Get toggled key states.
     * aKeyCode should be NS_VK_CAPS_LOCK or  NS_VK_NUM_LOCK or
     * NS_VK_SCROLL_LOCK.
     * aLEDState is the result for current LED state of the key.
     * If the LED is 'ON', it returns TRUE, otherwise, FALSE.
     * If the platform doesn't support the LED state (or we cannot get the
     * state), this method returns NS_ERROR_NOT_IMPLEMENTED.
     */
    NS_IMETHOD GetToggledKeyState(uint32_t aKeyCode, bool* aLEDState) = 0;

    /*
     * Retrieves preference for IME updates
     */
    virtual nsIMEUpdatePreference GetIMEUpdatePreference() = 0;

    /*
     * Call this method when a dialog is opened which has a default button.
     * The button's rectangle should be supplied in aButtonRect.
     */ 
    NS_IMETHOD OnDefaultButtonLoaded(const nsIntRect &aButtonRect) = 0;

    /**
     * Compute the overridden system mouse scroll speed on the root content of
     * web pages.  The widget may set the same value as aOriginalDelta.  E.g.,
     * when the system scrolling settings were customized, widget can respect
     * the will of the user.
     *
     * This is called only when the mouse wheel event scrolls the root content
     * of the web pages by line.  In other words, this isn't called when the
     * mouse wheel event is used for zoom, page scroll and other special
     * actions.  And also this isn't called when the user doesn't use the
     * system wheel speed settings.
     *
     * @param aOriginalDeltaX   The X delta value of the current mouse wheel
     *                          scrolling event.
     * @param aOriginalDeltaX   The Y delta value of the current mouse wheel
     *                          scrolling event.
     * @param aOverriddenDeltaX The overridden mouse scrolling speed along X
     *                          axis. This value may be same as aOriginalDeltaX.
     * @param aOverriddenDeltaY The overridden mouse scrolling speed along Y
     *                          axis. This value may be same as aOriginalDeltaY.
     */
    NS_IMETHOD OverrideSystemMouseScrollSpeed(double aOriginalDeltaX,
                                              double aOriginalDeltaY,
                                              double& aOverriddenDeltaX,
                                              double& aOverriddenDeltaY) = 0;

    /**
     * Return true if this process shouldn't use platform widgets, and
     * so should use PuppetWidgets instead.  If this returns true, the
     * result of creating and using a platform widget is undefined,
     * and likely to end in crashes or other buggy behavior.
     */
    static bool
    UsePuppetWidgets()
    {
      return XRE_GetProcessType() == GeckoProcessType_Content;
    }

    /**
     * Allocate and return a "puppet widget" that doesn't directly
     * correlate to a platform widget; platform events and data must
     * be fed to it.  Currently used in content processes.  NULL is
     * returned if puppet widgets aren't supported in this build
     * config, on this platform, or for this process type.
     *
     * This function is called "Create" to match CreateInstance().
     * The returned widget must still be nsIWidget::Create()d.
     */
    static already_AddRefed<nsIWidget>
    CreatePuppetWidget(TabChild* aTabChild);

    /**
     * Allocate and return a "plugin proxy widget", a subclass of PuppetWidget
     * used in wrapping a PPluginWidget connection for remote widgets. Note
     * this call creates the base object, it does not create the widget. Use
     * nsIWidget's Create to do this.
     */
    static already_AddRefed<nsIWidget>
    CreatePluginProxyWidget(TabChild* aTabChild,
                            mozilla::plugins::PluginWidgetChild* aActor);

    /**
     * Reparent this widget's native widget.
     * @param aNewParent the native widget of aNewParent is the new native
     *                   parent widget
     */
    NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent) = 0;

    /**
     * Return the internal format of the default framebuffer for this
     * widget.
     */
    virtual uint32_t GetGLFrameBufferFormat() { return 0; /*GL_NONE*/ }

    /**
     * Return true if widget has it's own GL context
     */
    virtual bool HasGLContext() { return false; }

    /**
     * Returns true to indicate that this widget paints an opaque background
     * that we want to be visible under the page, so layout should not force
     * a default background.
     */
    virtual bool WidgetPaintsBackground() { return false; }

    virtual bool NeedsPaint() {
       if (!IsVisible()) {
           return false;
       }
       nsIntRect bounds;
       nsresult rv = GetBounds(bounds);
       NS_ENSURE_SUCCESS(rv, false);
       return !bounds.IsEmpty();
    }

    /**
     * Get the natural bounds of this widget.  This method is only
     * meaningful for widgets for which Gecko implements screen
     * rotation natively.  When this is the case, GetBounds() returns
     * the widget bounds taking rotation into account, and
     * GetNaturalBounds() returns the bounds *not* taking rotation
     * into account.
     *
     * No code outside of the composition pipeline should know or care
     * about this.  If you're not an agent of the compositor, you
     * probably shouldn't call this method.
     */
    virtual nsIntRect GetNaturalBounds() {
        nsIntRect bounds;
        GetBounds(bounds);
        return bounds;
    }

    /**
     * Set size constraints on the window size such that it is never less than
     * the specified minimum size and never larger than the specified maximum
     * size. The size constraints are sizes of the outer rectangle including
     * the window frame and title bar. Use 0 for an unconstrained minimum size
     * and NS_MAXSIZE for an unconstrained maximum size. Note that this method
     * does not necessarily change the size of a window to conform to this size,
     * thus Resize should be called afterwards.
     *
     * @param aConstraints: the size constraints in device pixels
     */
    virtual void SetSizeConstraints(const SizeConstraints& aConstraints) = 0;

    /**
     * Return the size constraints currently observed by the widget.
     *
     * @return the constraints in device pixels
     */
    virtual const SizeConstraints& GetSizeConstraints() const = 0;

    /**
     * If this is owned by a TabChild, return that.  Otherwise return
     * null.
     */
    virtual TabChild* GetOwningTabChild() { return nullptr; }

    /**
     * If this isn't directly compositing to its window surface,
     * return the compositor which is doing that on our behalf.
     */
    virtual CompositorChild* GetRemoteRenderer()
    { return nullptr; }

    /**
     * If this widget has a more efficient composer available for its
     * native framebuffer, return it.
     *
     * This can be called from a non-main thread, but that thread must
     * hold a strong reference to this.
     */
    virtual Composer2D* GetComposer2D()
    { return nullptr; }

    /**
     * Some platforms (only cocoa right now) round widget coordinates to the
     * nearest even pixels (see bug 892994), this function allows us to
     * determine how widget coordinates will be rounded.
     */
    virtual int32_t RoundsWidgetCoordinatesTo() { return 1; }

    /**
     * GetTextEventDispatcher() returns TextEventDispatcher belonging to the
     * widget.  Note that this never returns nullptr.
     */
    NS_IMETHOD_(TextEventDispatcher*) GetTextEventDispatcher() = 0;

protected:
    /**
     * Like GetDefaultScale, but taking into account only the system settings
     * and ignoring Gecko preferences.
     */
    virtual double GetDefaultScaleInternal() { return 1.0; }

    // keep the list of children.  We also keep track of our siblings.
    // The ownership model is as follows: parent holds a strong ref to
    // the first element of the list, and each element holds a strong
    // ref to the next element in the list.  The prevsibling and
    // lastchild pointers are weak, which is fine as long as they are
    // maintained properly.
    nsCOMPtr<nsIWidget> mFirstChild;
    nsIWidget* MOZ_NON_OWNING_REF mLastChild;
    nsCOMPtr<nsIWidget> mNextSibling;
    nsIWidget* MOZ_NON_OWNING_REF mPrevSibling;
    // When Destroy() is called, the sub class should set this true.
    bool mOnDestroyCalled;
    nsWindowType mWindowType;
    int32_t mZIndex;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIWidget, NS_IWIDGET_IID)

#endif // nsIWidget_h__
