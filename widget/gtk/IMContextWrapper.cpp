/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "nsString.h"
#include "prtime.h"

#include "IMContextWrapper.h"
#include "nsGtkKeyUtils.h"
#include "nsWindow.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Likely.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_intl.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"
#include "mozilla/ToString.h"
#include "WritingModes.h"

namespace mozilla {
namespace widget {

LazyLogModule gGtkIMLog("nsGtkIMModuleWidgets");

static inline const char* ToChar(bool aBool) {
  return aBool ? "true" : "false";
}

static const char* GetEventType(GdkEventKey* aKeyEvent) {
  switch (aKeyEvent->type) {
    case GDK_KEY_PRESS:
      return "GDK_KEY_PRESS";
    case GDK_KEY_RELEASE:
      return "GDK_KEY_RELEASE";
    default:
      return "Unknown";
  }
}

class GetEventStateName : public nsAutoCString {
 public:
  explicit GetEventStateName(guint aState,
                             IMContextWrapper::IMContextID aIMContextID =
                                 IMContextWrapper::IMContextID::Unknown) {
    if (aState & GDK_SHIFT_MASK) {
      AppendModifier("shift");
    }
    if (aState & GDK_CONTROL_MASK) {
      AppendModifier("control");
    }
    if (aState & GDK_MOD1_MASK) {
      AppendModifier("mod1");
    }
    if (aState & GDK_MOD2_MASK) {
      AppendModifier("mod2");
    }
    if (aState & GDK_MOD3_MASK) {
      AppendModifier("mod3");
    }
    if (aState & GDK_MOD4_MASK) {
      AppendModifier("mod4");
    }
    if (aState & GDK_MOD4_MASK) {
      AppendModifier("mod5");
    }
    if (aState & GDK_MOD4_MASK) {
      AppendModifier("mod5");
    }
    switch (aIMContextID) {
      case IMContextWrapper::IMContextID::IBus:
        static const guint IBUS_HANDLED_MASK = 1 << 24;
        static const guint IBUS_IGNORED_MASK = 1 << 25;
        if (aState & IBUS_HANDLED_MASK) {
          AppendModifier("IBUS_HANDLED_MASK");
        }
        if (aState & IBUS_IGNORED_MASK) {
          AppendModifier("IBUS_IGNORED_MASK");
        }
        break;
      case IMContextWrapper::IMContextID::Fcitx:
      case IMContextWrapper::IMContextID::Fcitx5:
        static const guint FcitxKeyState_HandledMask = 1 << 24;
        static const guint FcitxKeyState_IgnoredMask = 1 << 25;
        if (aState & FcitxKeyState_HandledMask) {
          AppendModifier("FcitxKeyState_HandledMask");
        }
        if (aState & FcitxKeyState_IgnoredMask) {
          AppendModifier("FcitxKeyState_IgnoredMask");
        }
        break;
      default:
        break;
    }
  }

 private:
  void AppendModifier(const char* aModifierName) {
    if (!IsEmpty()) {
      AppendLiteral(" + ");
    }
    Append(aModifierName);
  }
};

class GetWritingModeName : public nsAutoCString {
 public:
  explicit GetWritingModeName(const WritingMode& aWritingMode) {
    if (!aWritingMode.IsVertical()) {
      AssignLiteral("Horizontal");
      return;
    }
    if (aWritingMode.IsVerticalLR()) {
      AssignLiteral("Vertical (LTR)");
      return;
    }
    AssignLiteral("Vertical (RTL)");
  }
  virtual ~GetWritingModeName() = default;
};

class GetTextRangeStyleText final : public nsAutoCString {
 public:
  explicit GetTextRangeStyleText(const TextRangeStyle& aStyle) {
    if (!aStyle.IsDefined()) {
      AssignLiteral("{ IsDefined()=false }");
      return;
    }

    if (aStyle.IsLineStyleDefined()) {
      AppendLiteral("{ mLineStyle=");
      AppendLineStyle(aStyle.mLineStyle);
      if (aStyle.IsUnderlineColorDefined()) {
        AppendLiteral(", mUnderlineColor=");
        AppendColor(aStyle.mUnderlineColor);
      } else {
        AppendLiteral(", IsUnderlineColorDefined=false");
      }
    } else {
      AppendLiteral("{ IsLineStyleDefined()=false");
    }

    if (aStyle.IsForegroundColorDefined()) {
      AppendLiteral(", mForegroundColor=");
      AppendColor(aStyle.mForegroundColor);
    } else {
      AppendLiteral(", IsForegroundColorDefined()=false");
    }

    if (aStyle.IsBackgroundColorDefined()) {
      AppendLiteral(", mBackgroundColor=");
      AppendColor(aStyle.mBackgroundColor);
    } else {
      AppendLiteral(", IsBackgroundColorDefined()=false");
    }

    AppendLiteral(" }");
  }
  void AppendLineStyle(TextRangeStyle::LineStyle aLineStyle) {
    switch (aLineStyle) {
      case TextRangeStyle::LineStyle::None:
        AppendLiteral("LineStyle::None");
        break;
      case TextRangeStyle::LineStyle::Solid:
        AppendLiteral("LineStyle::Solid");
        break;
      case TextRangeStyle::LineStyle::Dotted:
        AppendLiteral("LineStyle::Dotted");
        break;
      case TextRangeStyle::LineStyle::Dashed:
        AppendLiteral("LineStyle::Dashed");
        break;
      case TextRangeStyle::LineStyle::Double:
        AppendLiteral("LineStyle::Double");
        break;
      case TextRangeStyle::LineStyle::Wavy:
        AppendLiteral("LineStyle::Wavy");
        break;
      default:
        AppendPrintf("Invalid(0x%02X)",
                     static_cast<TextRangeStyle::LineStyleType>(aLineStyle));
        break;
    }
  }
  void AppendColor(nscolor aColor) {
    AppendPrintf("{ R=0x%02X, G=0x%02X, B=0x%02X, A=0x%02X }", NS_GET_R(aColor),
                 NS_GET_G(aColor), NS_GET_B(aColor), NS_GET_A(aColor));
  }
  virtual ~GetTextRangeStyleText() = default;
};

const static bool kUseSimpleContextDefault = false;

/******************************************************************************
 * SelectionStyleProvider
 *
 * IME (e.g., fcitx, ~4.2.8.3) may look up selection colors of widget, which
 * is related to the window associated with the IM context, to support any
 * colored widgets.  Our editor (like <input type="text">) is rendered as
 * native GtkTextView as far as possible by default and if editor color is
 * changed by web apps, nsTextFrame may swap background color of foreground
 * color of composition string for making composition string is always
 * visually distinct in normal text.
 *
 * So, we would like IME to set style of composition string to good colors
 * in GtkTextView.  Therefore, this class overwrites selection colors of
 * our widget with selection colors of GtkTextView so that it's possible IME
 * to refer selection colors of GtkTextView via our widget.
 ******************************************************************************/

static Maybe<nscolor> GetSystemColor(LookAndFeel::ColorID aId) {
  return LookAndFeel::GetColor(aId, LookAndFeel::ColorScheme::Light,
                               LookAndFeel::UseStandins::No);
}

class SelectionStyleProvider final {
 public:
  static SelectionStyleProvider* GetExistingInstance() { return sInstance; }

  static SelectionStyleProvider* GetInstance() {
    if (sHasShutDown) {
      return nullptr;
    }
    if (!sInstance) {
      sInstance = new SelectionStyleProvider();
    }
    return sInstance;
  }

  static void Shutdown() {
    if (sInstance) {
      g_object_unref(sInstance->mProvider);
    }
    delete sInstance;
    sInstance = nullptr;
    sHasShutDown = true;
  }

  // aGDKWindow is a GTK window which will be associated with an IM context.
  void AttachTo(GdkWindow* aGDKWindow) {
    GtkWidget* widget = nullptr;
    // gdk_window_get_user_data() typically returns pointer to widget that
    // window belongs to.  If it's widget, fcitx retrieves selection colors
    // of them.  So, we need to overwrite its style.
    gdk_window_get_user_data(aGDKWindow, (gpointer*)&widget);
    if (GTK_IS_WIDGET(widget)) {
      gtk_style_context_add_provider(gtk_widget_get_style_context(widget),
                                     GTK_STYLE_PROVIDER(mProvider),
                                     GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
  }

  void OnThemeChanged() {
    // fcitx refers GtkStyle::text[GTK_STATE_SELECTED] and
    // GtkStyle::bg[GTK_STATE_SELECTED] (although pair of text and *base*
    // or *fg* and bg is correct).  gtk_style_update_from_context() will
    // set these colors using the widget's GtkStyleContext and so the
    // colors can be controlled by a ":selected" CSS rule.
    nsAutoCString style(":selected{");
    // FYI: LookAndFeel always returns selection colors of GtkTextView.
    if (auto selectionForegroundColor =
            GetSystemColor(LookAndFeel::ColorID::Highlight)) {
      double alpha =
          static_cast<double>(NS_GET_A(*selectionForegroundColor)) / 0xFF;
      style.AppendPrintf("color:rgba(%u,%u,%u,",
                         NS_GET_R(*selectionForegroundColor),
                         NS_GET_G(*selectionForegroundColor),
                         NS_GET_B(*selectionForegroundColor));
      // We can't use AppendPrintf here, because it does locale-specific
      // formatting of floating-point values.
      style.AppendFloat(alpha);
      style.AppendPrintf(");");
    }
    if (auto selectionBackgroundColor =
            GetSystemColor(LookAndFeel::ColorID::Highlighttext)) {
      double alpha =
          static_cast<double>(NS_GET_A(*selectionBackgroundColor)) / 0xFF;
      style.AppendPrintf("background-color:rgba(%u,%u,%u,",
                         NS_GET_R(*selectionBackgroundColor),
                         NS_GET_G(*selectionBackgroundColor),
                         NS_GET_B(*selectionBackgroundColor));
      style.AppendFloat(alpha);
      style.AppendPrintf(");");
    }
    style.AppendLiteral("}");
    gtk_css_provider_load_from_data(mProvider, style.get(), -1, nullptr);
  }

 private:
  static SelectionStyleProvider* sInstance;
  static bool sHasShutDown;
  GtkCssProvider* const mProvider;

  SelectionStyleProvider() : mProvider(gtk_css_provider_new()) {
    OnThemeChanged();
  }
};

SelectionStyleProvider* SelectionStyleProvider::sInstance = nullptr;
bool SelectionStyleProvider::sHasShutDown = false;

/******************************************************************************
 * IMContextWrapper
 ******************************************************************************/

IMContextWrapper* IMContextWrapper::sLastFocusedContext = nullptr;
guint16 IMContextWrapper::sWaitingSynthesizedKeyPressHardwareKeyCode = 0;
bool IMContextWrapper::sUseSimpleContext;

NS_IMPL_ISUPPORTS(IMContextWrapper, TextEventDispatcherListener,
                  nsISupportsWeakReference)

IMContextWrapper::IMContextWrapper(nsWindow* aOwnerWindow)
    : mOwnerWindow(aOwnerWindow),
      mLastFocusedWindow(nullptr),
      mContext(nullptr),
      mSimpleContext(nullptr),
      mDummyContext(nullptr),
      mComposingContext(nullptr),
      mCompositionStart(UINT32_MAX),
      mProcessingKeyEvent(nullptr),
      mCompositionState(eCompositionState_NotComposing),
      mIMContextID(IMContextID::Unknown),
      mIsIMFocused(false),
      mFallbackToKeyEvent(false),
      mKeyboardEventWasDispatched(false),
      mKeyboardEventWasConsumed(false),
      mIsDeletingSurrounding(false),
      mLayoutChanged(false),
      mSetCursorPositionOnKeyEvent(true),
      mPendingResettingIMContext(false),
      mRetrieveSurroundingSignalReceived(false),
      mMaybeInDeadKeySequence(false),
      mIsIMInAsyncKeyHandlingMode(false) {
  static bool sFirstInstance = true;
  if (sFirstInstance) {
    sFirstInstance = false;
    sUseSimpleContext =
        Preferences::GetBool("intl.ime.use_simple_context_on_password_field",
                             kUseSimpleContextDefault);
  }
  Init();
}

static bool IsIBusInSyncMode() {
  // See ibus_im_context_class_init() in client/gtk2/ibusimcontext.c
  // https://github.com/ibus/ibus/blob/86963f2f94d1e4fc213b01c2bc2ba9dcf4b22219/client/gtk2/ibusimcontext.c#L610
  const char* env = PR_GetEnv("IBUS_ENABLE_SYNC_MODE");

  // See _get_boolean_env() in client/gtk2/ibusimcontext.c
  // https://github.com/ibus/ibus/blob/86963f2f94d1e4fc213b01c2bc2ba9dcf4b22219/client/gtk2/ibusimcontext.c#L520-L537
  if (!env) {
    return false;
  }
  nsDependentCString envStr(env);
  if (envStr.IsEmpty() || envStr.EqualsLiteral("0") ||
      envStr.EqualsLiteral("false") || envStr.EqualsLiteral("False") ||
      envStr.EqualsLiteral("FALSE")) {
    return false;
  }
  return true;
}

static bool GetFcitxBoolEnv(const char* aEnv) {
  // See fcitx_utils_get_boolean_env in src/lib/fcitx-utils/utils.c
  // https://github.com/fcitx/fcitx/blob/0c87840dc7d9460c2cb5feaeefec299d0d3d62ec/src/lib/fcitx-utils/utils.c#L721-L736
  const char* env = PR_GetEnv(aEnv);
  if (!env) {
    return false;
  }
  nsDependentCString envStr(env);
  if (envStr.IsEmpty() || envStr.EqualsLiteral("0") ||
      envStr.EqualsLiteral("false")) {
    return false;
  }
  return true;
}

static bool IsFcitxInSyncMode() {
  // See fcitx_im_context_class_init() in src/frontend/gtk2/fcitximcontext.c
  // https://github.com/fcitx/fcitx/blob/78b98d9230dc9630e99d52e3172bdf440ffd08c4/src/frontend/gtk2/fcitximcontext.c#L395-L398
  return GetFcitxBoolEnv("IBUS_ENABLE_SYNC_MODE") ||
         GetFcitxBoolEnv("FCITX_ENABLE_SYNC_MODE");
}

nsDependentCSubstring IMContextWrapper::GetIMName() const {
  const char* contextIDChar =
      gtk_im_multicontext_get_context_id(GTK_IM_MULTICONTEXT(mContext));
  if (!contextIDChar) {
    return nsDependentCSubstring();
  }

  nsDependentCSubstring im(contextIDChar, strlen(contextIDChar));

  // If the context is XIM, actual engine must be specified with
  // |XMODIFIERS=@im=foo|.
  const char* xmodifiersChar = PR_GetEnv("XMODIFIERS");
  if (!xmodifiersChar || !im.EqualsLiteral("xim")) {
    return im;
  }

  nsDependentCString xmodifiers(xmodifiersChar);
  int32_t atIMValueStart = xmodifiers.Find("@im=") + 4;
  if (atIMValueStart < 4 ||
      xmodifiers.Length() <= static_cast<size_t>(atIMValueStart)) {
    return im;
  }

  int32_t atIMValueEnd = xmodifiers.Find("@", false, atIMValueStart);
  if (atIMValueEnd > atIMValueStart) {
    return nsDependentCSubstring(xmodifiersChar + atIMValueStart,
                                 atIMValueEnd - atIMValueStart);
  }

  if (atIMValueEnd == kNotFound) {
    return nsDependentCSubstring(xmodifiersChar + atIMValueStart,
                                 strlen(xmodifiersChar) - atIMValueStart);
  }

  return im;
}

void IMContextWrapper::Init() {
  MozContainer* container = mOwnerWindow->GetMozContainer();
  MOZ_ASSERT(container, "container is null");
  GdkWindow* gdkWindow = gtk_widget_get_window(GTK_WIDGET(container));

  // Overwrite selection colors of the window before associating the window
  // with IM context since IME may look up selection colors via IM context
  // to support any colored widgets.
  SelectionStyleProvider::GetInstance()->AttachTo(gdkWindow);

  // NOTE: gtk_im_*_new() abort (kill) the whole process when it fails.
  //       So, we don't need to check the result.

  // Normal context.
  mContext = gtk_im_multicontext_new();
  gtk_im_context_set_client_window(mContext, gdkWindow);
  g_signal_connect(mContext, "preedit_changed",
                   G_CALLBACK(IMContextWrapper::OnChangeCompositionCallback),
                   this);
  g_signal_connect(mContext, "retrieve_surrounding",
                   G_CALLBACK(IMContextWrapper::OnRetrieveSurroundingCallback),
                   this);
  g_signal_connect(mContext, "delete_surrounding",
                   G_CALLBACK(IMContextWrapper::OnDeleteSurroundingCallback),
                   this);
  g_signal_connect(mContext, "commit",
                   G_CALLBACK(IMContextWrapper::OnCommitCompositionCallback),
                   this);
  g_signal_connect(mContext, "preedit_start",
                   G_CALLBACK(IMContextWrapper::OnStartCompositionCallback),
                   this);
  g_signal_connect(mContext, "preedit_end",
                   G_CALLBACK(IMContextWrapper::OnEndCompositionCallback),
                   this);
  nsDependentCSubstring im = GetIMName();
  if (im.EqualsLiteral("ibus")) {
    mIMContextID = IMContextID::IBus;
    mIsIMInAsyncKeyHandlingMode = !IsIBusInSyncMode();
    // Although ibus has key snooper mode, it's forcibly disabled on Firefox
    // in default settings by its whitelist since we always send key events
    // to IME before handling shortcut keys.  The whitelist can be
    // customized with env, IBUS_NO_SNOOPER_APPS, but we don't need to
    // support such rare cases for reducing maintenance cost.
    mIsKeySnooped = false;
  } else if (im.EqualsLiteral("fcitx")) {
    mIMContextID = IMContextID::Fcitx;
    mIsIMInAsyncKeyHandlingMode = !IsFcitxInSyncMode();
    // Although Fcitx has key snooper mode similar to ibus, it's also
    // disabled on Firefox in default settings by its whitelist.  The
    // whitelist can be customized with env, IBUS_NO_SNOOPER_APPS or
    // FCITX_NO_SNOOPER_APPS, but we don't need to support such rare cases
    // for reducing maintenance cost.
    mIsKeySnooped = false;
  } else if (im.EqualsLiteral("fcitx5")) {
    mIMContextID = IMContextID::Fcitx5;
    mIsIMInAsyncKeyHandlingMode = true;  // does not have sync mode.
    mIsKeySnooped = false;               // never use key snooper.
  } else if (im.EqualsLiteral("uim")) {
    mIMContextID = IMContextID::Uim;
    mIsIMInAsyncKeyHandlingMode = false;
    // We cannot know if uim uses key snooper since it's build option of
    // uim.  Therefore, we need to retrieve the consideration from the
    // pref for making users and distributions allowed to choose their
    // preferred value.
    mIsKeySnooped =
        Preferences::GetBool("intl.ime.hack.uim.using_key_snooper", true);
  } else if (im.EqualsLiteral("scim")) {
    mIMContextID = IMContextID::Scim;
    mIsIMInAsyncKeyHandlingMode = false;
    mIsKeySnooped = false;
  } else if (im.EqualsLiteral("iiim")) {
    mIMContextID = IMContextID::IIIMF;
    mIsIMInAsyncKeyHandlingMode = false;
    mIsKeySnooped = false;
  } else if (im.EqualsLiteral("wayland")) {
    mIMContextID = IMContextID::Wayland;
    mIsIMInAsyncKeyHandlingMode = false;
    mIsKeySnooped = true;
  } else {
    mIMContextID = IMContextID::Unknown;
    mIsIMInAsyncKeyHandlingMode = false;
    mIsKeySnooped = false;
  }

  // Simple context
  if (sUseSimpleContext) {
    mSimpleContext = gtk_im_context_simple_new();
    gtk_im_context_set_client_window(mSimpleContext, gdkWindow);
    g_signal_connect(mSimpleContext, "preedit_changed",
                     G_CALLBACK(&IMContextWrapper::OnChangeCompositionCallback),
                     this);
    g_signal_connect(
        mSimpleContext, "retrieve_surrounding",
        G_CALLBACK(&IMContextWrapper::OnRetrieveSurroundingCallback), this);
    g_signal_connect(mSimpleContext, "delete_surrounding",
                     G_CALLBACK(&IMContextWrapper::OnDeleteSurroundingCallback),
                     this);
    g_signal_connect(mSimpleContext, "commit",
                     G_CALLBACK(&IMContextWrapper::OnCommitCompositionCallback),
                     this);
    g_signal_connect(mSimpleContext, "preedit_start",
                     G_CALLBACK(IMContextWrapper::OnStartCompositionCallback),
                     this);
    g_signal_connect(mSimpleContext, "preedit_end",
                     G_CALLBACK(IMContextWrapper::OnEndCompositionCallback),
                     this);
  }

  // Dummy context
  mDummyContext = gtk_im_multicontext_new();
  gtk_im_context_set_client_window(mDummyContext, gdkWindow);

  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p Init(), mOwnerWindow=%p, mContext=%p (im=\"%s\"), "
           "mIsIMInAsyncKeyHandlingMode=%s, mIsKeySnooped=%s, "
           "mSimpleContext=%p, mDummyContext=%p, "
           "gtk_im_multicontext_get_context_id()=\"%s\", "
           "PR_GetEnv(\"XMODIFIERS\")=\"%s\"",
           this, mOwnerWindow, mContext, nsAutoCString(im).get(),
           ToChar(mIsIMInAsyncKeyHandlingMode), ToChar(mIsKeySnooped),
           mSimpleContext, mDummyContext,
           gtk_im_multicontext_get_context_id(GTK_IM_MULTICONTEXT(mContext)),
           PR_GetEnv("XMODIFIERS")));
}

/* static */
void IMContextWrapper::Shutdown() { SelectionStyleProvider::Shutdown(); }

IMContextWrapper::~IMContextWrapper() {
  if (this == sLastFocusedContext) {
    sLastFocusedContext = nullptr;
  }
  MOZ_LOG(gGtkIMLog, LogLevel::Info, ("0x%p ~IMContextWrapper()", this));
}

NS_IMETHODIMP
IMContextWrapper::NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                            const IMENotification& aNotification) {
  switch (aNotification.mMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION:
    case REQUEST_TO_CANCEL_COMPOSITION: {
      nsWindow* window =
          static_cast<nsWindow*>(aTextEventDispatcher->GetWidget());
      return EndIMEComposition(window);
    }
    case NOTIFY_IME_OF_FOCUS:
      OnFocusChangeInGecko(true);
      return NS_OK;
    case NOTIFY_IME_OF_BLUR:
      OnFocusChangeInGecko(false);
      return NS_OK;
    case NOTIFY_IME_OF_POSITION_CHANGE:
      OnLayoutChange();
      return NS_OK;
    case NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED:
      OnUpdateComposition();
      return NS_OK;
    case NOTIFY_IME_OF_SELECTION_CHANGE: {
      nsWindow* window =
          static_cast<nsWindow*>(aTextEventDispatcher->GetWidget());
      OnSelectionChange(window, aNotification);
      return NS_OK;
    }
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

NS_IMETHODIMP_(void)
IMContextWrapper::OnRemovedFrom(TextEventDispatcher* aTextEventDispatcher) {
  // XXX When input transaction is being stolen by add-on, what should we do?
}

NS_IMETHODIMP_(void)
IMContextWrapper::WillDispatchKeyboardEvent(
    TextEventDispatcher* aTextEventDispatcher,
    WidgetKeyboardEvent& aKeyboardEvent, uint32_t aIndexOfKeypress,
    void* aData) {
  KeymapWrapper::WillDispatchKeyboardEvent(aKeyboardEvent,
                                           static_cast<GdkEventKey*>(aData));
}

TextEventDispatcher* IMContextWrapper::GetTextEventDispatcher() {
  if (NS_WARN_IF(!mLastFocusedWindow)) {
    return nullptr;
  }
  TextEventDispatcher* dispatcher =
      mLastFocusedWindow->GetTextEventDispatcher();
  // nsIWidget::GetTextEventDispatcher() shouldn't return nullptr.
  MOZ_RELEASE_ASSERT(dispatcher);
  return dispatcher;
}

NS_IMETHODIMP_(IMENotificationRequests)
IMContextWrapper::GetIMENotificationRequests() {
  IMENotificationRequests::Notifications notifications =
      IMENotificationRequests::NOTIFY_NOTHING;
  // If it's not enabled, we don't need position change notification.
  if (IsEnabled()) {
    notifications |= IMENotificationRequests::NOTIFY_POSITION_CHANGE;
  }
  return IMENotificationRequests(notifications);
}

void IMContextWrapper::OnDestroyWindow(nsWindow* aWindow) {
  MOZ_LOG(
      gGtkIMLog, LogLevel::Info,
      ("0x%p OnDestroyWindow(aWindow=0x%p), mLastFocusedWindow=0x%p, "
       "mOwnerWindow=0x%p, mLastFocusedModule=0x%p",
       this, aWindow, mLastFocusedWindow, mOwnerWindow, sLastFocusedContext));

  MOZ_ASSERT(aWindow, "aWindow must not be null");

  if (mLastFocusedWindow == aWindow) {
    EndIMEComposition(aWindow);
    if (mIsIMFocused) {
      Blur();
    }
    mLastFocusedWindow = nullptr;
  }

  if (mOwnerWindow != aWindow) {
    return;
  }

  if (sLastFocusedContext == this) {
    sLastFocusedContext = nullptr;
  }

  /**
   * NOTE:
   *   The given window is the owner of this, so, we must release the
   *   contexts now.  But that might be referred from other nsWindows
   *   (they are children of this.  But we don't know why there are the
   *   cases).  So, we need to clear the pointers that refers to contexts
   *   and this if the other referrers are still alive. See bug 349727.
   */
  if (mContext) {
    PrepareToDestroyContext(mContext);
    gtk_im_context_set_client_window(mContext, nullptr);
    g_object_unref(mContext);
    mContext = nullptr;
  }

  if (mSimpleContext) {
    gtk_im_context_set_client_window(mSimpleContext, nullptr);
    g_object_unref(mSimpleContext);
    mSimpleContext = nullptr;
  }

  if (mDummyContext) {
    // mContext and mDummyContext have the same slaveType and signal_data
    // so no need for another workaround_gtk_im_display_closed.
    gtk_im_context_set_client_window(mDummyContext, nullptr);
    g_object_unref(mDummyContext);
    mDummyContext = nullptr;
  }

  if (NS_WARN_IF(mComposingContext)) {
    g_object_unref(mComposingContext);
    mComposingContext = nullptr;
  }

  mOwnerWindow = nullptr;
  mLastFocusedWindow = nullptr;
  mInputContext.mIMEState.mEnabled = IMEEnabled::Disabled;
  mPostingKeyEvents.Clear();

  MOZ_LOG(gGtkIMLog, LogLevel::Debug,
          ("0x%p   OnDestroyWindow(), succeeded, Completely destroyed", this));
}

void IMContextWrapper::PrepareToDestroyContext(GtkIMContext* aContext) {
  if (mIMContextID == IMContextID::IIIMF) {
    // IIIM module registers handlers for the "closed" signal on the
    // display, but the signal handler is not disconnected when the module
    // is unloaded.  To prevent the module from being unloaded, use static
    // variable to hold reference of slave context class declared by IIIM.
    // Note that this does not grab any instance, it grabs the "class".
    static gpointer sGtkIIIMContextClass = nullptr;
    if (!sGtkIIIMContextClass) {
      // We retrieved slave context class with g_type_name() and actual
      // slave context instance when our widget was GTK2.  That must be
      // _GtkIMContext::priv::slave in GTK3.  However, _GtkIMContext::priv
      // is an opacity struct named _GtkIMMulticontextPrivate, i.e., it's
      // not exposed by GTK3.  Therefore, we cannot access the instance
      // safely.  So, we need to retrieve the slave context class with
      // g_type_from_name("GtkIMContextIIIM") directly (anyway, we needed
      // to compare the class name with "GtkIMContextIIIM").
      GType IIMContextType = g_type_from_name("GtkIMContextIIIM");
      if (IIMContextType) {
        sGtkIIIMContextClass = g_type_class_ref(IIMContextType);
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
                ("0x%p PrepareToDestroyContext(), added to reference to "
                 "GtkIMContextIIIM class to prevent it from being unloaded",
                 this));
      } else {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
                ("0x%p PrepareToDestroyContext(), FAILED to prevent the "
                 "IIIM module from being uploaded",
                 this));
      }
    }
  }
}

void IMContextWrapper::OnFocusWindow(nsWindow* aWindow) {
  if (MOZ_UNLIKELY(IsDestroyed())) {
    return;
  }

  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p OnFocusWindow(aWindow=0x%p), mLastFocusedWindow=0x%p", this,
           aWindow, mLastFocusedWindow));
  mLastFocusedWindow = aWindow;
  Focus();
}

void IMContextWrapper::OnBlurWindow(nsWindow* aWindow) {
  if (MOZ_UNLIKELY(IsDestroyed())) {
    return;
  }

  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p OnBlurWindow(aWindow=0x%p), mLastFocusedWindow=0x%p, "
           "mIsIMFocused=%s",
           this, aWindow, mLastFocusedWindow, ToChar(mIsIMFocused)));

  if (!mIsIMFocused || mLastFocusedWindow != aWindow) {
    return;
  }

  Blur();
}

KeyHandlingState IMContextWrapper::OnKeyEvent(
    nsWindow* aCaller, GdkEventKey* aEvent,
    bool aKeyboardEventWasDispatched /* = false */) {
  MOZ_ASSERT(aEvent, "aEvent must be non-null");

  if (!mInputContext.mIMEState.IsEditable() || MOZ_UNLIKELY(IsDestroyed())) {
    return KeyHandlingState::eNotHandled;
  }

  MOZ_LOG(gGtkIMLog, LogLevel::Info, (">>>>>>>>>>>>>>>>"));
  MOZ_LOG(
      gGtkIMLog, LogLevel::Info,
      ("0x%p OnKeyEvent(aCaller=0x%p, "
       "aEvent(0x%p): { type=%s, keyval=%s, unicode=0x%X, state=%s, "
       "time=%u, hardware_keycode=%u, group=%u }, "
       "aKeyboardEventWasDispatched=%s)",
       this, aCaller, aEvent, GetEventType(aEvent),
       gdk_keyval_name(aEvent->keyval), gdk_keyval_to_unicode(aEvent->keyval),
       GetEventStateName(aEvent->state, mIMContextID).get(), aEvent->time,
       aEvent->hardware_keycode, aEvent->group,
       ToChar(aKeyboardEventWasDispatched)));
  MOZ_LOG(
      gGtkIMLog, LogLevel::Info,
      ("0x%p   OnKeyEvent(), mMaybeInDeadKeySequence=%s, "
       "mCompositionState=%s, current context=%p, active context=%p, "
       "mIMContextID=%s, mIsIMInAsyncKeyHandlingMode=%s",
       this, ToChar(mMaybeInDeadKeySequence), GetCompositionStateName(),
       GetCurrentContext(), GetActiveContext(), ToString(mIMContextID).c_str(),
       ToChar(mIsIMInAsyncKeyHandlingMode)));

  if (aCaller != mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   OnKeyEvent(), FAILED, the caller isn't focused "
             "window, mLastFocusedWindow=0x%p",
             this, mLastFocusedWindow));
    return KeyHandlingState::eNotHandled;
  }

  // Even if old IM context has composition, key event should be sent to
  // current context since the user expects so.
  GtkIMContext* currentContext = GetCurrentContext();
  if (MOZ_UNLIKELY(!currentContext)) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   OnKeyEvent(), FAILED, there are no context", this));
    return KeyHandlingState::eNotHandled;
  }

  if (mSetCursorPositionOnKeyEvent) {
    SetCursorPosition(currentContext);
    mSetCursorPositionOnKeyEvent = false;
  }

  // Let's support dead key event even if active keyboard layout also
  // supports complicated composition like CJK IME.
  bool isDeadKey =
      KeymapWrapper::ComputeDOMKeyNameIndex(aEvent) == KEY_NAME_INDEX_Dead;
  mMaybeInDeadKeySequence |= isDeadKey;

  // If current context is mSimpleContext, both ibus and fcitx handles key
  // events synchronously.  So, only when current context is mContext which
  // is GtkIMMulticontext, the key event may be handled by IME asynchronously.
  bool probablyHandledAsynchronously =
      mIsIMInAsyncKeyHandlingMode && currentContext == mContext;

  // If we're not sure whether the event is handled asynchronously, this is
  // set to true.
  bool maybeHandledAsynchronously = false;

  // If aEvent is a synthesized event for async handling, this will be set to
  // true.
  bool isHandlingAsyncEvent = false;

  // If we've decided that the event won't be synthesized asyncrhonously
  // by IME, but actually IME did it, this is set to true.
  bool isUnexpectedAsyncEvent = false;

  // If IM is ibus or fcitx and it handles key events asynchronously,
  // they mark aEvent->state as "handled by me" when they post key event
  // to another process.  Unfortunately, we need to check this hacky
  // flag because it's difficult to store all pending key events by
  // an array or a hashtable.
  if (probablyHandledAsynchronously) {
    switch (mIMContextID) {
      case IMContextID::IBus: {
        // See src/ibustypes.h
        static const guint IBUS_IGNORED_MASK = 1 << 25;
        // If IBUS_IGNORED_MASK was set to aEvent->state, the event
        // has already been handled by another process and it wasn't
        // used by IME.
        isHandlingAsyncEvent = !!(aEvent->state & IBUS_IGNORED_MASK);
        if (!isHandlingAsyncEvent) {
          // On some environments, IBUS_IGNORED_MASK flag is not set as
          // expected.  In such case, we keep pusing all events into the queue.
          // I.e., that causes eating a lot of memory until it's blurred.
          // Therefore, we need to check whether there is same timestamp event
          // in the queue.  This redundant cost should be low because in most
          // causes, key events in the queue should be 2 or 4.
          isHandlingAsyncEvent =
              mPostingKeyEvents.IndexOf(aEvent) != GdkEventKeyQueue::NoIndex();
          if (isHandlingAsyncEvent) {
            MOZ_LOG(gGtkIMLog, LogLevel::Info,
                    ("0x%p   OnKeyEvent(), aEvent->state does not have "
                     "IBUS_IGNORED_MASK but "
                     "same event in the queue.  So, assuming it's a "
                     "synthesized event",
                     this));
          }
        }

        // If it's a synthesized event, let's remove it from the posting
        // event queue first.  Otherwise the following blocks cannot use
        // `break`.
        if (isHandlingAsyncEvent) {
          MOZ_LOG(gGtkIMLog, LogLevel::Info,
                  ("0x%p   OnKeyEvent(), aEvent->state has IBUS_IGNORED_MASK "
                   "or aEvent is in the "
                   "posting event queue, so, it won't be handled "
                   "asynchronously anymore. Removing "
                   "the posted events from the queue",
                   this));
          probablyHandledAsynchronously = false;
          mPostingKeyEvents.RemoveEvent(aEvent);
        }

        // ibus won't send back key press events in a dead key sequcne.
        if (mMaybeInDeadKeySequence && aEvent->type == GDK_KEY_PRESS) {
          probablyHandledAsynchronously = false;
          if (isHandlingAsyncEvent) {
            isUnexpectedAsyncEvent = true;
            break;
          }
          // Some keyboard layouts which have dead keys may send
          // "empty" key event to make us call
          // gtk_im_context_filter_keypress() to commit composed
          // character during a GDK_KEY_PRESS event dispatching.
          if (!gdk_keyval_to_unicode(aEvent->keyval) &&
              !aEvent->hardware_keycode) {
            isUnexpectedAsyncEvent = true;
            break;
          }
          break;
        }
        // ibus may handle key events synchronously if focused editor is
        // <input type="password"> or |ime-mode: disabled;|.  However, in
        // some environments, not so actually.  Therefore, we need to check
        // the result of gtk_im_context_filter_keypress() later.
        if (mInputContext.mIMEState.mEnabled == IMEEnabled::Password) {
          probablyHandledAsynchronously = false;
          maybeHandledAsynchronously = !isHandlingAsyncEvent;
          break;
        }
        break;
      }
      case IMContextID::Fcitx:
      case IMContextID::Fcitx5: {
        // See src/lib/fcitx-utils/keysym.h
        static const guint FcitxKeyState_IgnoredMask = 1 << 25;
        // If FcitxKeyState_IgnoredMask was set to aEvent->state,
        // the event has already been handled by another process and
        // it wasn't used by IME.
        isHandlingAsyncEvent = !!(aEvent->state & FcitxKeyState_IgnoredMask);
        if (!isHandlingAsyncEvent) {
          // On some environments, FcitxKeyState_IgnoredMask flag *might* be not
          // set as expected. If there were such cases, we'd keep pusing all
          // events into the queue.  I.e., that would cause eating a lot of
          // memory until it'd be blurred.  Therefore, we should check whether
          // there is same timestamp event in the queue.  This redundant cost
          // should be low because in most causes, key events in the queue
          // should be 2 or 4.
          isHandlingAsyncEvent =
              mPostingKeyEvents.IndexOf(aEvent) != GdkEventKeyQueue::NoIndex();
          if (isHandlingAsyncEvent) {
            MOZ_LOG(gGtkIMLog, LogLevel::Info,
                    ("0x%p   OnKeyEvent(), aEvent->state does not have "
                     "FcitxKeyState_IgnoredMask "
                     "but same event in the queue.  So, assuming it's a "
                     "synthesized event",
                     this));
          }
        }

        // fcitx won't send back key press events in a dead key sequcne.
        if (mMaybeInDeadKeySequence && aEvent->type == GDK_KEY_PRESS) {
          probablyHandledAsynchronously = false;
          if (isHandlingAsyncEvent) {
            isUnexpectedAsyncEvent = true;
            break;
          }
          // Some keyboard layouts which have dead keys may send
          // "empty" key event to make us call
          // gtk_im_context_filter_keypress() to commit composed
          // character during a GDK_KEY_PRESS event dispatching.
          if (!gdk_keyval_to_unicode(aEvent->keyval) &&
              !aEvent->hardware_keycode) {
            isUnexpectedAsyncEvent = true;
            break;
          }
        }

        // fcitx handles key events asynchronously even if focused
        // editor cannot use IME actually.

        if (isHandlingAsyncEvent) {
          MOZ_LOG(gGtkIMLog, LogLevel::Info,
                  ("0x%p   OnKeyEvent(), aEvent->state has "
                   "FcitxKeyState_IgnoredMask or aEvent is in "
                   "the posting event queue, so, it won't be handled "
                   "asynchronously anymore. "
                   "Removing the posted events from the queue",
                   this));
          probablyHandledAsynchronously = false;
          mPostingKeyEvents.RemoveEvent(aEvent);
          break;
        }
        break;
      }
      default:
        MOZ_ASSERT_UNREACHABLE(
            "IME may handle key event "
            "asyncrhonously, but not yet confirmed if it comes agian "
            "actually");
    }
  }

  if (!isUnexpectedAsyncEvent) {
    mKeyboardEventWasDispatched = aKeyboardEventWasDispatched;
    mKeyboardEventWasConsumed = false;
  } else {
    // If we didn't expect this event, we've alreday dispatched eKeyDown
    // event or eKeyUp event for that.
    mKeyboardEventWasDispatched = true;
    // And in this case, we need to assume that another key event hasn't
    // been receivied and mKeyboardEventWasConsumed keeps storing the
    // dispatched eKeyDown or eKeyUp event's state.
  }
  mFallbackToKeyEvent = false;
  mProcessingKeyEvent = aEvent;
  gboolean isFiltered = gtk_im_context_filter_keypress(currentContext, aEvent);

  // If we're not sure whether the event is handled by IME asynchronously or
  // synchronously, we need to trust the result of
  // gtk_im_context_filter_keypress().  If it consumed and but did nothing,
  // we can assume that another event will be synthesized.
  if (!isHandlingAsyncEvent && maybeHandledAsynchronously) {
    probablyHandledAsynchronously |=
        isFiltered && !mFallbackToKeyEvent && !mKeyboardEventWasDispatched;
  }

  if (aEvent->type == GDK_KEY_PRESS) {
    if (isFiltered && probablyHandledAsynchronously) {
      sWaitingSynthesizedKeyPressHardwareKeyCode = aEvent->hardware_keycode;
    } else {
      sWaitingSynthesizedKeyPressHardwareKeyCode = 0;
    }
  }

  // The caller of this shouldn't handle aEvent anymore if we've dispatched
  // composition events or modified content with other events.
  bool filterThisEvent = isFiltered && !mFallbackToKeyEvent;

  if (IsComposingOnCurrentContext() && !isFiltered &&
      aEvent->type == GDK_KEY_PRESS && mDispatchedCompositionString.IsEmpty()) {
    // A Hangul input engine for SCIM doesn't emit preedit_end
    // signal even when composition string becomes empty.  On the
    // other hand, we should allow to make composition with empty
    // string for other languages because there *might* be such
    // IM.  For compromising this issue, we should dispatch
    // compositionend event, however, we don't need to reset IM
    // actually.
    // NOTE: Don't dispatch key events as "processed by IME" since
    // we need to dispatch keyboard events as IME wasn't handled it.
    mProcessingKeyEvent = nullptr;
    DispatchCompositionCommitEvent(currentContext, &EmptyString());
    mProcessingKeyEvent = aEvent;
    // In this case, even though we handle the keyboard event here,
    // but we should dispatch keydown event as
    filterThisEvent = false;
  }

  if (filterThisEvent && !mKeyboardEventWasDispatched) {
    // If IME handled the key event but we've not dispatched eKeyDown nor
    // eKeyUp event yet, we need to dispatch here unless the key event is
    // now being handled by other IME process.
    if (!probablyHandledAsynchronously) {
      MaybeDispatchKeyEventAsProcessedByIME(eVoidEvent);
      // Be aware, the widget might have been gone here.
    }
    // If we need to wait reply from IM, IM may send some signals to us
    // without sending the key event again.  In such case, we need to
    // dispatch keyboard events with a copy of aEvent.  Therefore, we
    // need to use information of this key event to dispatch an KeyDown
    // or eKeyUp event later.
    else {
      MOZ_LOG(gGtkIMLog, LogLevel::Info,
              ("0x%p   OnKeyEvent(), putting aEvent into the queue...", this));
      mPostingKeyEvents.PutEvent(aEvent);
    }
  }

  mProcessingKeyEvent = nullptr;

  if (aEvent->type == GDK_KEY_PRESS && !filterThisEvent) {
    // If the key event hasn't been handled by active IME nor keyboard
    // layout, we can assume that the dead key sequence has been or was
    // ended.  Note that we should not reset it when the key event is
    // GDK_KEY_RELEASE since it may not be filtered by active keyboard
    // layout even in composition.
    mMaybeInDeadKeySequence = false;
  }

  MOZ_LOG(
      gGtkIMLog, LogLevel::Debug,
      ("0x%p   OnKeyEvent(), succeeded, filterThisEvent=%s "
       "(isFiltered=%s, mFallbackToKeyEvent=%s, "
       "probablyHandledAsynchronously=%s, maybeHandledAsynchronously=%s), "
       "mPostingKeyEvents.Length()=%zu, mCompositionState=%s, "
       "mMaybeInDeadKeySequence=%s, mKeyboardEventWasDispatched=%s, "
       "mKeyboardEventWasConsumed=%s",
       this, ToChar(filterThisEvent), ToChar(isFiltered),
       ToChar(mFallbackToKeyEvent), ToChar(probablyHandledAsynchronously),
       ToChar(maybeHandledAsynchronously), mPostingKeyEvents.Length(),
       GetCompositionStateName(), ToChar(mMaybeInDeadKeySequence),
       ToChar(mKeyboardEventWasDispatched), ToChar(mKeyboardEventWasConsumed)));
  MOZ_LOG(gGtkIMLog, LogLevel::Info, ("<<<<<<<<<<<<<<<<\n\n"));

  if (filterThisEvent) {
    return KeyHandlingState::eHandled;
  }
  // If another call of this method has already dispatched eKeyDown event,
  // we should return KeyHandlingState::eNotHandledButEventDispatched because
  // the caller should've stopped handling the event if preceding eKeyDown
  // event was consumed.
  if (aKeyboardEventWasDispatched) {
    return KeyHandlingState::eNotHandledButEventDispatched;
  }
  if (!mKeyboardEventWasDispatched) {
    return KeyHandlingState::eNotHandled;
  }
  return mKeyboardEventWasConsumed
             ? KeyHandlingState::eNotHandledButEventConsumed
             : KeyHandlingState::eNotHandledButEventDispatched;
}

void IMContextWrapper::OnFocusChangeInGecko(bool aFocus) {
  MOZ_LOG(
      gGtkIMLog, LogLevel::Info,
      ("0x%p OnFocusChangeInGecko(aFocus=%s), "
       "mCompositionState=%s, mIsIMFocused=%s",
       this, ToChar(aFocus), GetCompositionStateName(), ToChar(mIsIMFocused)));

  // We shouldn't carry over the removed string to another editor.
  mSelectedStringRemovedByComposition.Truncate();
  mSelection.Clear();

  // When the focus changes, we need to inform IM about the new cursor
  // position. Chinese input methods generally rely on this because they
  // usually don't start composition until a character is picked.
  if (aFocus && EnsureToCacheSelection()) {
    SetCursorPosition(GetActiveContext());
  }
}

void IMContextWrapper::ResetIME() {
  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p ResetIME(), mCompositionState=%s, mIsIMFocused=%s", this,
           GetCompositionStateName(), ToChar(mIsIMFocused)));

  GtkIMContext* activeContext = GetActiveContext();
  if (MOZ_UNLIKELY(!activeContext)) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   ResetIME(), FAILED, there are no context", this));
    return;
  }

  RefPtr<IMContextWrapper> kungFuDeathGrip(this);
  RefPtr<nsWindow> lastFocusedWindow(mLastFocusedWindow);

  mPendingResettingIMContext = false;
  gtk_im_context_reset(activeContext);

  // The last focused window might have been destroyed by a DOM event handler
  // which was called by us during a call of gtk_im_context_reset().
  if (!lastFocusedWindow ||
      NS_WARN_IF(lastFocusedWindow != mLastFocusedWindow) ||
      lastFocusedWindow->Destroyed()) {
    return;
  }

  nsAutoString compositionString;
  GetCompositionString(activeContext, compositionString);

  MOZ_LOG(
      gGtkIMLog, LogLevel::Debug,
      ("0x%p   ResetIME() called gtk_im_context_reset(), "
       "activeContext=0x%p, mCompositionState=%s, compositionString=%s, "
       "mIsIMFocused=%s",
       this, activeContext, GetCompositionStateName(),
       NS_ConvertUTF16toUTF8(compositionString).get(), ToChar(mIsIMFocused)));

  // XXX IIIMF (ATOK X3 which is one of the Language Engine of it is still
  //     used in Japan!) sends only "preedit_changed" signal with empty
  //     composition string synchronously.  Therefore, if composition string
  //     is now empty string, we should assume that the IME won't send
  //     "commit" signal.
  if (IsComposing() && compositionString.IsEmpty()) {
    // WARNING: The widget might have been gone after this.
    DispatchCompositionCommitEvent(activeContext, &EmptyString());
  }
}

nsresult IMContextWrapper::EndIMEComposition(nsWindow* aCaller) {
  if (MOZ_UNLIKELY(IsDestroyed())) {
    return NS_OK;
  }

  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p EndIMEComposition(aCaller=0x%p), "
           "mCompositionState=%s",
           this, aCaller, GetCompositionStateName()));

  if (aCaller != mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   EndIMEComposition(), FAILED, the caller isn't "
             "focused window, mLastFocusedWindow=0x%p",
             this, mLastFocusedWindow));
    return NS_OK;
  }

  if (!IsComposing()) {
    return NS_OK;
  }

  // Currently, GTK has API neither to commit nor to cancel composition
  // forcibly.  Therefore, TextComposition will recompute commit string for
  // the request even if native IME will cause unexpected commit string.
  // So, we don't need to emulate commit or cancel composition with
  // proper composition events.
  // XXX ResetIME() might not enough for finishing compositoin on some
  //     environments.  We should emulate focus change too because some IMEs
  //     may commit or cancel composition at blur.
  ResetIME();

  return NS_OK;
}

void IMContextWrapper::OnLayoutChange() {
  if (MOZ_UNLIKELY(IsDestroyed())) {
    return;
  }

  if (IsComposing()) {
    SetCursorPosition(GetActiveContext());
  } else {
    // If not composing, candidate window position is updated before key
    // down
    mSetCursorPositionOnKeyEvent = true;
  }
  mLayoutChanged = true;
}

void IMContextWrapper::OnUpdateComposition() {
  if (MOZ_UNLIKELY(IsDestroyed())) {
    return;
  }

  if (!IsComposing()) {
    // Composition has been committed.  So we need update selection for
    // caret later
    mSelection.Clear();
    EnsureToCacheSelection();
    mSetCursorPositionOnKeyEvent = true;
  }

  // If we've already set candidate window position, we don't need to update
  // the position with update composition notification.
  if (!mLayoutChanged) {
    SetCursorPosition(GetActiveContext());
  }
}

void IMContextWrapper::SetInputContext(nsWindow* aCaller,
                                       const InputContext* aContext,
                                       const InputContextAction* aAction) {
  if (MOZ_UNLIKELY(IsDestroyed())) {
    return;
  }

  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p SetInputContext(aCaller=0x%p, aContext={ mIMEState={ "
           "mEnabled=%s }, mHTMLInputType=%s })",
           this, aCaller, ToString(aContext->mIMEState.mEnabled).c_str(),
           NS_ConvertUTF16toUTF8(aContext->mHTMLInputType).get()));

  if (aCaller != mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   SetInputContext(), FAILED, "
             "the caller isn't focused window, mLastFocusedWindow=0x%p",
             this, mLastFocusedWindow));
    return;
  }

  if (!mContext) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   SetInputContext(), FAILED, "
             "there are no context",
             this));
    return;
  }

  if (sLastFocusedContext != this) {
    mInputContext = *aContext;
    MOZ_LOG(gGtkIMLog, LogLevel::Debug,
            ("0x%p   SetInputContext(), succeeded, "
             "but we're not active",
             this));
    return;
  }

  bool changingEnabledState = aContext->IsInputAttributeChanged(mInputContext);

  // Release current IME focus if IME is enabled.
  if (changingEnabledState && mInputContext.mIMEState.IsEditable()) {
    EndIMEComposition(mLastFocusedWindow);
    Blur();
  }

  mInputContext = *aContext;

  if (changingEnabledState) {
    if (mInputContext.mIMEState.IsEditable()) {
      GtkIMContext* currentContext = GetCurrentContext();
      if (currentContext) {
        GtkInputPurpose purpose = GTK_INPUT_PURPOSE_FREE_FORM;
        const nsString& inputType = mInputContext.mHTMLInputType;
        // Password case has difficult issue.  Desktop IMEs disable
        // composition if input-purpose is password.
        // For disabling IME on |ime-mode: disabled;|, we need to check
        // mEnabled value instead of inputType value.  This hack also
        // enables composition on
        // <input type="password" style="ime-mode: enabled;">.
        // This is right behavior of ime-mode on desktop.
        //
        // On the other hand, IME for tablet devices may provide a
        // specific software keyboard for password field.  If so,
        // the behavior might look strange on both:
        //   <input type="text" style="ime-mode: disabled;">
        //   <input type="password" style="ime-mode: enabled;">
        //
        // Temporarily, we should focus on desktop environment for now.
        // I.e., let's ignore tablet devices for now.  When somebody
        // reports actual trouble on tablet devices, we should try to
        // look for a way to solve actual problem.
        if (mInputContext.mIMEState.mEnabled == IMEEnabled::Password) {
          purpose = GTK_INPUT_PURPOSE_PASSWORD;
        } else if (inputType.EqualsLiteral("email")) {
          purpose = GTK_INPUT_PURPOSE_EMAIL;
        } else if (inputType.EqualsLiteral("url")) {
          purpose = GTK_INPUT_PURPOSE_URL;
        } else if (inputType.EqualsLiteral("tel")) {
          purpose = GTK_INPUT_PURPOSE_PHONE;
        } else if (inputType.EqualsLiteral("number")) {
          purpose = GTK_INPUT_PURPOSE_NUMBER;
        } else if (mInputContext.mHTMLInputInputmode.EqualsLiteral("decimal")) {
          purpose = GTK_INPUT_PURPOSE_NUMBER;
        } else if (mInputContext.mHTMLInputInputmode.EqualsLiteral("email")) {
          purpose = GTK_INPUT_PURPOSE_EMAIL;
        } else if (mInputContext.mHTMLInputInputmode.EqualsLiteral("numeric")) {
          purpose = GTK_INPUT_PURPOSE_DIGITS;
        } else if (mInputContext.mHTMLInputInputmode.EqualsLiteral("tel")) {
          purpose = GTK_INPUT_PURPOSE_PHONE;
        } else if (mInputContext.mHTMLInputInputmode.EqualsLiteral("url")) {
          purpose = GTK_INPUT_PURPOSE_URL;
        }
        // Search by type and inputmode isn't supported on GTK.

        g_object_set(currentContext, "input-purpose", purpose, nullptr);

        // Although GtkInputHints is enum type, value is bit field.
        gint hints = GTK_INPUT_HINT_NONE;
        if (mInputContext.mHTMLInputInputmode.EqualsLiteral("none")) {
          hints |= GTK_INPUT_HINT_INHIBIT_OSK;
        }

        if (mInputContext.mAutocapitalize.EqualsLiteral("characters")) {
          hints |= GTK_INPUT_HINT_UPPERCASE_CHARS;
        } else if (mInputContext.mAutocapitalize.EqualsLiteral("sentences")) {
          hints |= GTK_INPUT_HINT_UPPERCASE_SENTENCES;
        } else if (mInputContext.mAutocapitalize.EqualsLiteral("words")) {
          hints |= GTK_INPUT_HINT_UPPERCASE_WORDS;
        }

        g_object_set(currentContext, "input-hints", hints, nullptr);
      }
    }

    // Even when aState is not enabled state, we need to set IME focus.
    // Because some IMs are updating the status bar of them at this time.
    // Be aware, don't use aWindow here because this method shouldn't move
    // focus actually.
    Focus();

    // XXX Should we call Blur() when it's not editable?  E.g., it might be
    //     better to close VKB automatically.
  }
}

InputContext IMContextWrapper::GetInputContext() {
  mInputContext.mIMEState.mOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
  return mInputContext;
}

GtkIMContext* IMContextWrapper::GetCurrentContext() const {
  if (IsEnabled()) {
    return mContext;
  }
  if (mInputContext.mIMEState.mEnabled == IMEEnabled::Password) {
    return mSimpleContext;
  }
  return mDummyContext;
}

bool IMContextWrapper::IsValidContext(GtkIMContext* aContext) const {
  if (!aContext) {
    return false;
  }
  return aContext == mContext || aContext == mSimpleContext ||
         aContext == mDummyContext;
}

bool IMContextWrapper::IsEnabled() const {
  return mInputContext.mIMEState.mEnabled == IMEEnabled::Enabled ||
         (!sUseSimpleContext &&
          mInputContext.mIMEState.mEnabled == IMEEnabled::Password);
}

void IMContextWrapper::Focus() {
  MOZ_LOG(
      gGtkIMLog, LogLevel::Info,
      ("0x%p Focus(), sLastFocusedContext=0x%p", this, sLastFocusedContext));

  if (mIsIMFocused) {
    NS_ASSERTION(sLastFocusedContext == this,
                 "We're not active, but the IM was focused?");
    return;
  }

  GtkIMContext* currentContext = GetCurrentContext();
  if (!currentContext) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   Focus(), FAILED, there are no context", this));
    return;
  }

  if (sLastFocusedContext && sLastFocusedContext != this) {
    sLastFocusedContext->Blur();
  }

  sLastFocusedContext = this;

  // Forget all posted key events when focus is moved since they shouldn't
  // be fired in different editor.
  sWaitingSynthesizedKeyPressHardwareKeyCode = 0;
  mPostingKeyEvents.Clear();

  gtk_im_context_focus_in(currentContext);
  mIsIMFocused = true;
  mSetCursorPositionOnKeyEvent = true;

  if (!IsEnabled()) {
    // We should release IME focus for uim and scim.
    // These IMs are using snooper that is released at losing focus.
    Blur();
  }
}

void IMContextWrapper::Blur() {
  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p Blur(), mIsIMFocused=%s", this, ToChar(mIsIMFocused)));

  if (!mIsIMFocused) {
    return;
  }

  GtkIMContext* currentContext = GetCurrentContext();
  if (!currentContext) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   Blur(), FAILED, there are no context", this));
    return;
  }

  gtk_im_context_focus_out(currentContext);
  mIsIMFocused = false;
}

void IMContextWrapper::OnSelectionChange(
    nsWindow* aCaller, const IMENotification& aIMENotification) {
  const bool isSelectionRangeChanged =
      mSelection.mOffset != aIMENotification.mSelectionChangeData.mOffset ||
      mSelection.mString != *aIMENotification.mSelectionChangeData.mString;
  mSelection.Assign(aIMENotification);
  const bool retrievedSurroundingSignalReceived =
      mRetrieveSurroundingSignalReceived;
  mRetrieveSurroundingSignalReceived = false;

  if (MOZ_UNLIKELY(IsDestroyed())) {
    return;
  }

  const IMENotification::SelectionChangeDataBase& selectionChangeData =
      aIMENotification.mSelectionChangeData;

  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p OnSelectionChange(aCaller=0x%p, aIMENotification={ "
           "mSelectionChangeData=%s }), "
           "mCompositionState=%s, mIsDeletingSurrounding=%s, "
           "mRetrieveSurroundingSignalReceived=%s, isSelectionRangeChanged=%s",
           this, aCaller, ToString(selectionChangeData).c_str(),
           GetCompositionStateName(), ToChar(mIsDeletingSurrounding),
           ToChar(retrievedSurroundingSignalReceived),
           ToChar(isSelectionRangeChanged)));

  if (aCaller != mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   OnSelectionChange(), FAILED, "
             "the caller isn't focused window, mLastFocusedWindow=0x%p",
             this, mLastFocusedWindow));
    return;
  }

  if (!IsComposing()) {
    // Now we have no composition (mostly situation on calling this method)
    // If we have it, it will set by
    // NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED.
    mSetCursorPositionOnKeyEvent = true;
  }

  // The focused editor might have placeholder text with normal text node.
  // In such case, the text node must be removed from a compositionstart
  // event handler.  So, we're dispatching eCompositionStart,
  // we should ignore selection change notification.
  if (mCompositionState == eCompositionState_CompositionStartDispatched) {
    if (NS_WARN_IF(!mSelection.IsValid())) {
      MOZ_LOG(gGtkIMLog, LogLevel::Error,
              ("0x%p   OnSelectionChange(), FAILED, "
               "new offset is too large, cannot keep composing",
               this));
    } else {
      // Modify the selection start offset with new offset.
      mCompositionStart = mSelection.mOffset;
      // XXX We should modify mSelectedStringRemovedByComposition?
      // But how?
      MOZ_LOG(gGtkIMLog, LogLevel::Debug,
              ("0x%p   OnSelectionChange(), ignored, mCompositionStart "
               "is updated to %u, the selection change doesn't cause "
               "resetting IM context",
               this, mCompositionStart));
      // And don't reset the IM context.
      return;
    }
    // Otherwise, reset the IM context due to impossible to keep composing.
  }

  // If the selection change is caused by deleting surrounding text,
  // we shouldn't need to notify IME of selection change.
  if (mIsDeletingSurrounding) {
    return;
  }

  bool occurredBeforeComposition =
      IsComposing() && !selectionChangeData.mOccurredDuringComposition &&
      !selectionChangeData.mCausedByComposition;
  if (occurredBeforeComposition) {
    mPendingResettingIMContext = true;
  }

  // When the selection change is caused by dispatching composition event,
  // selection set event and/or occurred before starting current composition,
  // we shouldn't notify IME of that and commit existing composition.
  // Don't do this even if selection is not changed actually.  For example,
  // fcitx has direct input mode which does not insert composing string, but
  // inserts commited text for each key sequence (i.e., there is "invisible"
  // composition string).  In the world after bug 1712269, we don't use a
  // set of composition events for this kind of IME.  Therefore,
  // SelectionChangeData.mCausedByComposition is not expected value for here
  // if this call is caused by a preceding commit.  And if the preceding commit
  // is triggered by a key type for next word, resetting IME state makes fcitx
  // discard the pending input for the next word.  Thus, we need to check
  // whether the selection range is actually changed here.
  if (!selectionChangeData.mCausedByComposition &&
      !selectionChangeData.mCausedBySelectionEvent && isSelectionRangeChanged &&
      !occurredBeforeComposition) {
    // Hack for ibus-pinyin.  ibus-pinyin will synthesize a set of
    // composition which commits with empty string after calling
    // gtk_im_context_reset().  Therefore, selecting text causes
    // unexpectedly removing it.  For preventing it but not breaking the
    // other IMEs which use surrounding text, we should call it only when
    // surrounding text has been retrieved after last selection range was
    // set.  If it's not retrieved, that means that current IME doesn't
    // have any content cache, so, it must not need the notification of
    // selection change.
    if (IsComposing() || retrievedSurroundingSignalReceived) {
      ResetIME();
    }
  }
}

/* static */
void IMContextWrapper::OnThemeChanged() {
  if (auto* provider = SelectionStyleProvider::GetExistingInstance()) {
    provider->OnThemeChanged();
  }
}

/* static */
void IMContextWrapper::OnStartCompositionCallback(GtkIMContext* aContext,
                                                  IMContextWrapper* aModule) {
  aModule->OnStartCompositionNative(aContext);
}

void IMContextWrapper::OnStartCompositionNative(GtkIMContext* aContext) {
  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p OnStartCompositionNative(aContext=0x%p), "
           "current context=0x%p, mComposingContext=0x%p",
           this, aContext, GetCurrentContext(), mComposingContext));

  // See bug 472635, we should do nothing if IM context doesn't match.
  if (GetCurrentContext() != aContext) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   OnStartCompositionNative(), FAILED, "
             "given context doesn't match",
             this));
    return;
  }

  if (mComposingContext && aContext != mComposingContext) {
    // XXX For now, we should ignore this odd case, just logging.
    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("0x%p   OnStartCompositionNative(), Warning, "
             "there is already a composing context but starting new "
             "composition with different context",
             this));
  }

  // IME may start composition without "preedit_start" signal.  Therefore,
  // mComposingContext will be initialized in DispatchCompositionStart().

  if (!DispatchCompositionStart(aContext)) {
    return;
  }
  mCompositionTargetRange.mOffset = mCompositionStart;
  mCompositionTargetRange.mLength = 0;
}

/* static */
void IMContextWrapper::OnEndCompositionCallback(GtkIMContext* aContext,
                                                IMContextWrapper* aModule) {
  aModule->OnEndCompositionNative(aContext);
}

void IMContextWrapper::OnEndCompositionNative(GtkIMContext* aContext) {
  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p OnEndCompositionNative(aContext=0x%p), mComposingContext=0x%p",
           this, aContext, mComposingContext));

  // See bug 472635, we should do nothing if IM context doesn't match.
  // Note that if this is called after focus move, the context may different
  // from any our owning context.
  if (!IsValidContext(aContext)) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p    OnEndCompositionNative(), FAILED, "
             "given context doesn't match with any context",
             this));
    return;
  }

  // If we've not started composition with aContext, we should ignore it.
  if (aContext != mComposingContext) {
    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("0x%p    OnEndCompositionNative(), Warning, "
             "given context doesn't match with mComposingContext",
             this));
    return;
  }

  g_object_unref(mComposingContext);
  mComposingContext = nullptr;

  // If we already handled the commit event, we should do nothing here.
  if (IsComposing()) {
    if (!DispatchCompositionCommitEvent(aContext)) {
      // If the widget is destroyed, we should do nothing anymore.
      return;
    }
  }

  if (mPendingResettingIMContext) {
    ResetIME();
  }
}

/* static */
void IMContextWrapper::OnChangeCompositionCallback(GtkIMContext* aContext,
                                                   IMContextWrapper* aModule) {
  aModule->OnChangeCompositionNative(aContext);
}

void IMContextWrapper::OnChangeCompositionNative(GtkIMContext* aContext) {
  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p OnChangeCompositionNative(aContext=0x%p), "
           "mComposingContext=0x%p",
           this, aContext, mComposingContext));

  // See bug 472635, we should do nothing if IM context doesn't match.
  // Note that if this is called after focus move, the context may different
  // from any our owning context.
  if (!IsValidContext(aContext)) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   OnChangeCompositionNative(), FAILED, "
             "given context doesn't match with any context",
             this));
    return;
  }

  if (mComposingContext && aContext != mComposingContext) {
    // XXX For now, we should ignore this odd case, just logging.
    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("0x%p   OnChangeCompositionNative(), Warning, "
             "given context doesn't match with composing context",
             this));
  }

  nsAutoString compositionString;
  GetCompositionString(aContext, compositionString);
  if (!IsComposing() && compositionString.IsEmpty()) {
    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("0x%p   OnChangeCompositionNative(), Warning, does nothing "
             "because has not started composition and composing string is "
             "empty",
             this));
    mDispatchedCompositionString.Truncate();
    return;  // Don't start the composition with empty string.
  }

  // Be aware, widget can be gone
  DispatchCompositionChangeEvent(aContext, compositionString);
}

/* static */
gboolean IMContextWrapper::OnRetrieveSurroundingCallback(
    GtkIMContext* aContext, IMContextWrapper* aModule) {
  return aModule->OnRetrieveSurroundingNative(aContext);
}

gboolean IMContextWrapper::OnRetrieveSurroundingNative(GtkIMContext* aContext) {
  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p OnRetrieveSurroundingNative(aContext=0x%p), "
           "current context=0x%p",
           this, aContext, GetCurrentContext()));

  // See bug 472635, we should do nothing if IM context doesn't match.
  if (GetCurrentContext() != aContext) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   OnRetrieveSurroundingNative(), FAILED, "
             "given context doesn't match",
             this));
    return FALSE;
  }

  nsAutoString uniStr;
  uint32_t cursorPos;
  if (NS_FAILED(GetCurrentParagraph(uniStr, cursorPos))) {
    return FALSE;
  }

  // Despite taking a pointer and a length, IBus wants the string to be
  // zero-terminated and doesn't like U+0000 within the string.
  uniStr.ReplaceChar(char16_t(0), char16_t(0xFFFD));

  NS_ConvertUTF16toUTF8 utf8Str(nsDependentSubstring(uniStr, 0, cursorPos));
  uint32_t cursorPosInUTF8 = utf8Str.Length();
  AppendUTF16toUTF8(nsDependentSubstring(uniStr, cursorPos), utf8Str);
  gtk_im_context_set_surrounding(aContext, utf8Str.get(), utf8Str.Length(),
                                 cursorPosInUTF8);
  mRetrieveSurroundingSignalReceived = true;
  return TRUE;
}

/* static */
gboolean IMContextWrapper::OnDeleteSurroundingCallback(
    GtkIMContext* aContext, gint aOffset, gint aNChars,
    IMContextWrapper* aModule) {
  return aModule->OnDeleteSurroundingNative(aContext, aOffset, aNChars);
}

gboolean IMContextWrapper::OnDeleteSurroundingNative(GtkIMContext* aContext,
                                                     gint aOffset,
                                                     gint aNChars) {
  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p OnDeleteSurroundingNative(aContext=0x%p, aOffset=%d, "
           "aNChar=%d), current context=0x%p",
           this, aContext, aOffset, aNChars, GetCurrentContext()));

  // See bug 472635, we should do nothing if IM context doesn't match.
  if (GetCurrentContext() != aContext) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   OnDeleteSurroundingNative(), FAILED, "
             "given context doesn't match",
             this));
    return FALSE;
  }

  AutoRestore<bool> saveDeletingSurrounding(mIsDeletingSurrounding);
  mIsDeletingSurrounding = true;
  if (NS_SUCCEEDED(DeleteText(aContext, aOffset, (uint32_t)aNChars))) {
    return TRUE;
  }

  // failed
  MOZ_LOG(gGtkIMLog, LogLevel::Error,
          ("0x%p   OnDeleteSurroundingNative(), FAILED, "
           "cannot delete text",
           this));
  return FALSE;
}

/* static */
void IMContextWrapper::OnCommitCompositionCallback(GtkIMContext* aContext,
                                                   const gchar* aString,
                                                   IMContextWrapper* aModule) {
  aModule->OnCommitCompositionNative(aContext, aString);
}

void IMContextWrapper::OnCommitCompositionNative(GtkIMContext* aContext,
                                                 const gchar* aUTF8Char) {
  const gchar emptyStr = 0;
  const gchar* commitString = aUTF8Char ? aUTF8Char : &emptyStr;
  NS_ConvertUTF8toUTF16 utf16CommitString(commitString);

  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p OnCommitCompositionNative(aContext=0x%p), "
           "current context=0x%p, active context=0x%p, commitString=\"%s\", "
           "mProcessingKeyEvent=0x%p, IsComposingOn(aContext)=%s",
           this, aContext, GetCurrentContext(), GetActiveContext(),
           commitString, mProcessingKeyEvent, ToChar(IsComposingOn(aContext))));

  // See bug 472635, we should do nothing if IM context doesn't match.
  if (!IsValidContext(aContext)) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   OnCommitCompositionNative(), FAILED, "
             "given context doesn't match",
             this));
    return;
  }

  // If we are not in composition and committing with empty string,
  // we need to do nothing because if we continued to handle this
  // signal, we would dispatch compositionstart, text, compositionend
  // events with empty string.  Of course, they are unnecessary events
  // for Web applications and our editor.
  if (!IsComposingOn(aContext) && utf16CommitString.IsEmpty()) {
    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("0x%p   OnCommitCompositionNative(), Warning, does nothing "
             "because has not started composition and commit string is empty",
             this));
    return;
  }

  // If IME doesn't change their keyevent that generated this commit,
  // we should treat that IME didn't handle the key event because
  // web applications want to receive "keydown" and "keypress" event
  // in such case.
  // NOTE: While a key event is being handled, this might be caused on
  // current context.  Otherwise, this may be caused on active context.
  if (!IsComposingOn(aContext) && mProcessingKeyEvent &&
      mProcessingKeyEvent->type == GDK_KEY_PRESS &&
      aContext == GetCurrentContext()) {
    char keyval_utf8[8]; /* should have at least 6 bytes of space */
    gint keyval_utf8_len;
    guint32 keyval_unicode;

    keyval_unicode = gdk_keyval_to_unicode(mProcessingKeyEvent->keyval);
    keyval_utf8_len = g_unichar_to_utf8(keyval_unicode, keyval_utf8);
    keyval_utf8[keyval_utf8_len] = '\0';

    // If committing string is exactly same as a character which is
    // produced by the key, eKeyDown and eKeyPress event should be
    // dispatched by the caller of OnKeyEvent() normally.  Note that
    // mMaybeInDeadKeySequence will be set to false by OnKeyEvent()
    // since we set mFallbackToKeyEvent to true here.
    if (!strcmp(commitString, keyval_utf8)) {
      MOZ_LOG(gGtkIMLog, LogLevel::Info,
              ("0x%p   OnCommitCompositionNative(), "
               "we'll send normal key event",
               this));
      mFallbackToKeyEvent = true;
      return;
    }

    // If we're in a dead key sequence, commit string is a character in
    // the BMP and mProcessingKeyEvent produces some characters but it's
    // not same as committing string, we should dispatch an eKeyPress
    // event from here.
    WidgetKeyboardEvent keyDownEvent(true, eKeyDown, mLastFocusedWindow);
    KeymapWrapper::InitKeyEvent(keyDownEvent, mProcessingKeyEvent, false);
    if (mMaybeInDeadKeySequence && utf16CommitString.Length() == 1 &&
        keyDownEvent.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
      mKeyboardEventWasDispatched = true;
      // Anyway, we're not in dead key sequence anymore.
      mMaybeInDeadKeySequence = false;

      RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher();
      nsresult rv = dispatcher->BeginNativeInputTransaction();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
                ("0x%p   OnCommitCompositionNative(), FAILED, "
                 "due to BeginNativeInputTransaction() failure",
                 this));
        return;
      }

      // First, dispatch eKeyDown event.
      keyDownEvent.mKeyValue = utf16CommitString;
      nsEventStatus status = nsEventStatus_eIgnore;
      bool dispatched = dispatcher->DispatchKeyboardEvent(
          eKeyDown, keyDownEvent, status, mProcessingKeyEvent);
      if (!dispatched || status == nsEventStatus_eConsumeNoDefault) {
        mKeyboardEventWasConsumed = true;
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
                ("0x%p   OnCommitCompositionNative(), "
                 "doesn't dispatch eKeyPress event because the preceding "
                 "eKeyDown event was not dispatched or was consumed",
                 this));
        return;
      }
      if (mLastFocusedWindow != keyDownEvent.mWidget ||
          mLastFocusedWindow->Destroyed()) {
        MOZ_LOG(gGtkIMLog, LogLevel::Warning,
                ("0x%p   OnCommitCompositionNative(), Warning, "
                 "stop dispatching eKeyPress event because the preceding "
                 "eKeyDown event caused changing focused widget or "
                 "destroyed",
                 this));
        return;
      }
      MOZ_LOG(gGtkIMLog, LogLevel::Info,
              ("0x%p   OnCommitCompositionNative(), "
               "dispatched eKeyDown event for the committed character",
               this));

      // Next, dispatch eKeyPress event.
      dispatcher->MaybeDispatchKeypressEvents(keyDownEvent, status,
                                              mProcessingKeyEvent);
      MOZ_LOG(gGtkIMLog, LogLevel::Info,
              ("0x%p   OnCommitCompositionNative(), "
               "dispatched eKeyPress event for the committed character",
               this));
      return;
    }
  }

  NS_ConvertUTF8toUTF16 str(commitString);
  // Be aware, widget can be gone
  DispatchCompositionCommitEvent(aContext, &str);
}

void IMContextWrapper::GetCompositionString(GtkIMContext* aContext,
                                            nsAString& aCompositionString) {
  gchar* preedit_string;
  gint cursor_pos;
  PangoAttrList* feedback_list;
  gtk_im_context_get_preedit_string(aContext, &preedit_string, &feedback_list,
                                    &cursor_pos);
  if (preedit_string && *preedit_string) {
    CopyUTF8toUTF16(MakeStringSpan(preedit_string), aCompositionString);
  } else {
    aCompositionString.Truncate();
  }

  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p GetCompositionString(aContext=0x%p), "
           "aCompositionString=\"%s\"",
           this, aContext, preedit_string));

  pango_attr_list_unref(feedback_list);
  g_free(preedit_string);
}

bool IMContextWrapper::MaybeDispatchKeyEventAsProcessedByIME(
    EventMessage aFollowingEvent) {
  if (!mLastFocusedWindow) {
    return false;
  }

  if (!mIsKeySnooped &&
      ((!mProcessingKeyEvent && mPostingKeyEvents.IsEmpty()) ||
       (mProcessingKeyEvent && mKeyboardEventWasDispatched))) {
    return true;
  }

  // A "keydown" or "keyup" event handler may change focus with the
  // following event.  In such case, we need to cancel this composition.
  // So, we need to store IM context now because mComposingContext may be
  // overwritten with different context if calling this method recursively.
  // Note that we don't need to grab the context here because |context|
  // will be used only for checking if it's same as mComposingContext.
  GtkIMContext* oldCurrentContext = GetCurrentContext();
  GtkIMContext* oldComposingContext = mComposingContext;

  RefPtr<nsWindow> lastFocusedWindow(mLastFocusedWindow);

  if (mProcessingKeyEvent || !mPostingKeyEvents.IsEmpty()) {
    if (mProcessingKeyEvent) {
      mKeyboardEventWasDispatched = true;
    }
    // If we're not handling a key event synchronously, the signal may be
    // sent by IME without sending key event to us.  In such case, we
    // should dispatch keyboard event for the last key event which was
    // posted to other IME process.
    GdkEventKey* sourceEvent = mProcessingKeyEvent
                                   ? mProcessingKeyEvent
                                   : mPostingKeyEvents.GetFirstEvent();

    MOZ_LOG(
        gGtkIMLog, LogLevel::Info,
        ("0x%p MaybeDispatchKeyEventAsProcessedByIME("
         "aFollowingEvent=%s), dispatch %s %s "
         "event: { type=%s, keyval=%s, unicode=0x%X, state=%s, "
         "time=%u, hardware_keycode=%u, group=%u }",
         this, ToChar(aFollowingEvent),
         ToChar(sourceEvent->type == GDK_KEY_PRESS ? eKeyDown : eKeyUp),
         mProcessingKeyEvent ? "processing" : "posted",
         GetEventType(sourceEvent), gdk_keyval_name(sourceEvent->keyval),
         gdk_keyval_to_unicode(sourceEvent->keyval),
         GetEventStateName(sourceEvent->state, mIMContextID).get(),
         sourceEvent->time, sourceEvent->hardware_keycode, sourceEvent->group));

    // Let's dispatch eKeyDown event or eKeyUp event now.  Note that only
    // when we're not in a dead key composition, we should mark the
    // eKeyDown and eKeyUp event as "processed by IME" since we should
    // expose raw keyCode and key value to web apps the key event is a
    // part of a dead key sequence.
    // FYI: We should ignore if default of preceding keydown or keyup
    //      event is prevented since even on the other browsers, web
    //      applications cannot cancel the following composition event.
    //      Spec bug: https://github.com/w3c/uievents/issues/180
    KeymapWrapper::DispatchKeyDownOrKeyUpEvent(lastFocusedWindow, sourceEvent,
                                               !mMaybeInDeadKeySequence,
                                               &mKeyboardEventWasConsumed);
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("0x%p   MaybeDispatchKeyEventAsProcessedByIME(), keydown or keyup "
             "event is dispatched",
             this));

    if (!mProcessingKeyEvent) {
      MOZ_LOG(gGtkIMLog, LogLevel::Info,
              ("0x%p   MaybeDispatchKeyEventAsProcessedByIME(), removing first "
               "event from the queue",
               this));
      mPostingKeyEvents.RemoveEvent(sourceEvent);
    }
  } else {
    MOZ_ASSERT(mIsKeySnooped);
    // Currently, we support key snooper mode of uim and wayland only.
    MOZ_ASSERT(mIMContextID == IMContextID::Uim ||
               mIMContextID == IMContextID::Wayland);
    // uim sends "preedit_start" signal and "preedit_changed" separately
    // at starting composition, "commit" and "preedit_end" separately at
    // committing composition.

    // Currently, we should dispatch only fake eKeyDown event because
    // we cannot decide which is the last signal of each key operation
    // and Chromium also dispatches only "keydown" event in this case.
    bool dispatchFakeKeyDown = false;
    switch (aFollowingEvent) {
      case eCompositionStart:
      case eCompositionCommit:
      case eCompositionCommitAsIs:
      case eContentCommandInsertText:
        dispatchFakeKeyDown = true;
        break;
      // XXX Unfortunately, I don't have a good idea to prevent to
      //     dispatch redundant eKeyDown event for eCompositionStart
      //     immediately after "delete_surrounding" signal.  However,
      //     not dispatching eKeyDown event is worse than dispatching
      //     redundant eKeyDown events.
      case eContentCommandDelete:
        dispatchFakeKeyDown = true;
        break;
      // We need to prevent to dispatch redundant eKeyDown event for
      // eCompositionChange immediately after eCompositionStart.  So,
      // We should not dispatch eKeyDown event if dispatched composition
      // string is still empty string.
      case eCompositionChange:
        dispatchFakeKeyDown = !mDispatchedCompositionString.IsEmpty();
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Do you forget to handle the case?");
        break;
    }

    if (dispatchFakeKeyDown) {
      WidgetKeyboardEvent fakeKeyDownEvent(true, eKeyDown, lastFocusedWindow);
      fakeKeyDownEvent.mKeyCode = NS_VK_PROCESSKEY;
      fakeKeyDownEvent.mKeyNameIndex = KEY_NAME_INDEX_Process;
      // It's impossible to get physical key information in this case but
      // this should be okay since web apps shouldn't do anything with
      // physical key information during composition.
      fakeKeyDownEvent.mCodeNameIndex = CODE_NAME_INDEX_UNKNOWN;

      MOZ_LOG(gGtkIMLog, LogLevel::Info,
              ("0x%p MaybeDispatchKeyEventAsProcessedByIME("
               "aFollowingEvent=%s), dispatch fake eKeyDown event",
               this, ToChar(aFollowingEvent)));

      KeymapWrapper::DispatchKeyDownOrKeyUpEvent(
          lastFocusedWindow, fakeKeyDownEvent, &mKeyboardEventWasConsumed);
      MOZ_LOG(gGtkIMLog, LogLevel::Info,
              ("0x%p   MaybeDispatchKeyEventAsProcessedByIME(), "
               "fake keydown event is dispatched",
               this));
    }
  }

  if (lastFocusedWindow->IsDestroyed() ||
      lastFocusedWindow != mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("0x%p   MaybeDispatchKeyEventAsProcessedByIME(), Warning, the "
             "focused widget was destroyed/changed by a key event",
             this));
    return false;
  }

  // If the dispatched keydown event caused moving focus and that also
  // caused changing active context, we need to cancel composition here.
  if (GetCurrentContext() != oldCurrentContext) {
    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("0x%p   MaybeDispatchKeyEventAsProcessedByIME(), Warning, the key "
             "event causes changing active IM context",
             this));
    if (mComposingContext == oldComposingContext) {
      // Only when the context is still composing, we should call
      // ResetIME() here.  Otherwise, it should've already been
      // cleaned up.
      ResetIME();
    }
    return false;
  }

  return true;
}

bool IMContextWrapper::DispatchCompositionStart(GtkIMContext* aContext) {
  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p DispatchCompositionStart(aContext=0x%p)", this, aContext));

  if (IsComposing()) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DispatchCompositionStart(), FAILED, "
             "we're already in composition",
             this));
    return true;
  }

  if (!mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DispatchCompositionStart(), FAILED, "
             "there are no focused window in this module",
             this));
    return false;
  }

  if (NS_WARN_IF(!EnsureToCacheSelection())) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DispatchCompositionStart(), FAILED, "
             "cannot query the selection offset",
             this));
    return false;
  }

  mComposingContext = static_cast<GtkIMContext*>(g_object_ref(aContext));
  MOZ_ASSERT(mComposingContext);

  // Keep the last focused window alive
  RefPtr<nsWindow> lastFocusedWindow(mLastFocusedWindow);

  // XXX The composition start point might be changed by composition events
  //     even though we strongly hope it doesn't happen.
  //     Every composition event should have the start offset for the result
  //     because it may high cost if we query the offset every time.
  mCompositionStart = mSelection.mOffset;
  mDispatchedCompositionString.Truncate();

  // If this composition is started by a key press, we need to dispatch
  // eKeyDown or eKeyUp event before dispatching eCompositionStart event.
  // Note that dispatching a keyboard event which is marked as "processed
  // by IME" is okay since Chromium also dispatches keyboard event as so.
  if (!MaybeDispatchKeyEventAsProcessedByIME(eCompositionStart)) {
    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("0x%p   DispatchCompositionStart(), Warning, "
             "MaybeDispatchKeyEventAsProcessedByIME() returned false",
             this));
    return false;
  }

  RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher();
  nsresult rv = dispatcher->BeginNativeInputTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DispatchCompositionStart(), FAILED, "
             "due to BeginNativeInputTransaction() failure",
             this));
    return false;
  }

  static bool sHasSetTelemetry = false;
  if (!sHasSetTelemetry) {
    sHasSetTelemetry = true;
    NS_ConvertUTF8toUTF16 im(GetIMName());
    // 72 is kMaximumKeyStringLength in TelemetryScalar.cpp
    if (im.Length() > 72) {
      if (NS_IS_SURROGATE_PAIR(im[72 - 2], im[72 - 1])) {
        im.Truncate(72 - 2);
      } else {
        im.Truncate(72 - 1);
      }
      // U+2026 is "..."
      im.Append(char16_t(0x2026));
    }
    Telemetry::ScalarSet(Telemetry::ScalarID::WIDGET_IME_NAME_ON_LINUX, im,
                         true);
  }

  MOZ_LOG(gGtkIMLog, LogLevel::Debug,
          ("0x%p   DispatchCompositionStart(), dispatching "
           "compositionstart... (mCompositionStart=%u)",
           this, mCompositionStart));
  mCompositionState = eCompositionState_CompositionStartDispatched;
  nsEventStatus status;
  dispatcher->StartComposition(status);
  if (lastFocusedWindow->IsDestroyed() ||
      lastFocusedWindow != mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DispatchCompositionStart(), FAILED, the focused "
             "widget was destroyed/changed by compositionstart event",
             this));
    return false;
  }

  return true;
}

bool IMContextWrapper::DispatchCompositionChangeEvent(
    GtkIMContext* aContext, const nsAString& aCompositionString) {
  MOZ_LOG(
      gGtkIMLog, LogLevel::Info,
      ("0x%p DispatchCompositionChangeEvent(aContext=0x%p)", this, aContext));

  if (!mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DispatchCompositionChangeEvent(), FAILED, "
             "there are no focused window in this module",
             this));
    return false;
  }

  if (!IsComposing()) {
    MOZ_LOG(gGtkIMLog, LogLevel::Debug,
            ("0x%p   DispatchCompositionChangeEvent(), the composition "
             "wasn't started, force starting...",
             this));
    if (!DispatchCompositionStart(aContext)) {
      return false;
    }
  }
  // If this composition string change caused by a key press, we need to
  // dispatch eKeyDown or eKeyUp before dispatching eCompositionChange event.
  else if (!MaybeDispatchKeyEventAsProcessedByIME(eCompositionChange)) {
    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("0x%p   DispatchCompositionChangeEvent(), Warning, "
             "MaybeDispatchKeyEventAsProcessedByIME() returned false",
             this));
    return false;
  }

  RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher();
  nsresult rv = dispatcher->BeginNativeInputTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DispatchCompositionChangeEvent(), FAILED, "
             "due to BeginNativeInputTransaction() failure",
             this));
    return false;
  }

  // Store the selected string which will be removed by following
  // compositionchange event.
  if (mCompositionState == eCompositionState_CompositionStartDispatched) {
    if (NS_WARN_IF(
            !EnsureToCacheSelection(&mSelectedStringRemovedByComposition))) {
      // XXX How should we behave in this case??
    } else {
      // XXX We should assume, for now, any web applications don't change
      //     selection at handling this compositionchange event.
      mCompositionStart = mSelection.mOffset;
    }
  }

  RefPtr<TextRangeArray> rangeArray =
      CreateTextRangeArray(aContext, aCompositionString);

  rv = dispatcher->SetPendingComposition(aCompositionString, rangeArray);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DispatchCompositionChangeEvent(), FAILED, "
             "due to SetPendingComposition() failure",
             this));
    return false;
  }

  mCompositionState = eCompositionState_CompositionChangeEventDispatched;

  // We cannot call SetCursorPosition for e10s-aware.
  // DispatchEvent is async on e10s, so composition rect isn't updated now
  // on tab parent.
  mDispatchedCompositionString = aCompositionString;
  mLayoutChanged = false;
  mCompositionTargetRange.mOffset =
      mCompositionStart + rangeArray->TargetClauseOffset();
  mCompositionTargetRange.mLength = rangeArray->TargetClauseLength();

  RefPtr<nsWindow> lastFocusedWindow(mLastFocusedWindow);
  nsEventStatus status;
  rv = dispatcher->FlushPendingComposition(status);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DispatchCompositionChangeEvent(), FAILED, "
             "due to FlushPendingComposition() failure",
             this));
    return false;
  }

  if (lastFocusedWindow->IsDestroyed() ||
      lastFocusedWindow != mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DispatchCompositionChangeEvent(), FAILED, the "
             "focused widget was destroyed/changed by "
             "compositionchange event",
             this));
    return false;
  }
  return true;
}

bool IMContextWrapper::DispatchCompositionCommitEvent(
    GtkIMContext* aContext, const nsAString* aCommitString) {
  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p DispatchCompositionCommitEvent(aContext=0x%p, "
           "aCommitString=0x%p, (\"%s\"))",
           this, aContext, aCommitString,
           aCommitString ? NS_ConvertUTF16toUTF8(*aCommitString).get() : ""));

  if (!mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DispatchCompositionCommitEvent(), FAILED, "
             "there are no focused window in this module",
             this));
    return false;
  }

  // TODO: We need special care to handle request to commit composition
  //       by content while we're committing composition because we have
  //       commit string information now but IME may not have composition
  //       anymore.  Therefore, we may not be able to handle commit as
  //       expected.  However, this is rare case because this situation
  //       never occurs with remote content.  So, it's okay to fix this
  //       issue later.  (Perhaps, TextEventDisptcher should do it for
  //       all platforms.  E.g., creating WillCommitComposition()?)
  RefPtr<nsWindow> lastFocusedWindow(mLastFocusedWindow);
  RefPtr<TextEventDispatcher> dispatcher;
  if (!IsComposing() &&
      !StaticPrefs::intl_ime_use_composition_events_for_insert_text()) {
    if (!aCommitString || aCommitString->IsEmpty()) {
      MOZ_LOG(gGtkIMLog, LogLevel::Error,
              ("0x%p   DispatchCompositionCommitEvent(), FAILED, "
               "did nothing due to inserting empty string without composition",
               this));
      return true;
    }
    if (MOZ_UNLIKELY(!EnsureToCacheSelection())) {
      MOZ_LOG(gGtkIMLog, LogLevel::Warning,
              ("0x%p   DispatchCompositionCommitEvent(), Warning, "
               "Failed to cache selection before dispatching "
               "eContentCommandInsertText event",
               this));
    }
    if (!MaybeDispatchKeyEventAsProcessedByIME(eContentCommandInsertText)) {
      MOZ_LOG(gGtkIMLog, LogLevel::Warning,
              ("0x%p   DispatchCompositionCommitEvent(), Warning, "
               "MaybeDispatchKeyEventAsProcessedByIME() returned false",
               this));
      return false;
    }
    // Emulate selection until receiving actual selection range.  This is
    // important for OnSelectionChange.  If selection is not changed by web
    // apps, i.e., selection range is same as what selection expects, we
    // shouldn't reset IME because the trigger of causing this commit may be an
    // input for next composition and we shouldn't cancel it.
    if (mSelection.IsValid()) {
      mSelection.CollapseTo(mSelection.mOffset + aCommitString->Length(),
                            mSelection.mWritingMode);
      MOZ_LOG(gGtkIMLog, LogLevel::Info,
              ("0x%p   DispatchCompositionCommitEvent(), "
               "mSelection={ mOffset=%u, mString=\"%s\", mWritingMode=%s }",
               this, mSelection.mOffset,
               NS_ConvertUTF16toUTF8(mSelection.mString).get(),
               ToString(mSelection.mWritingMode).c_str()));
    }
    MOZ_ASSERT(!dispatcher);
  } else {
    if (!IsComposing()) {
      if (!aCommitString || aCommitString->IsEmpty()) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
                ("0x%p   DispatchCompositionCommitEvent(), FAILED, "
                 "there is no composition and empty commit string",
                 this));
        return true;
      }
      MOZ_LOG(gGtkIMLog, LogLevel::Debug,
              ("0x%p   DispatchCompositionCommitEvent(), "
               "the composition wasn't started, force starting...",
               this));
      if (!DispatchCompositionStart(aContext)) {
        return false;
      }
    }
    // If this commit caused by a key press, we need to dispatch eKeyDown or
    // eKeyUp before dispatching composition events.
    else if (!MaybeDispatchKeyEventAsProcessedByIME(
                 aCommitString ? eCompositionCommit : eCompositionCommitAsIs)) {
      MOZ_LOG(gGtkIMLog, LogLevel::Warning,
              ("0x%p   DispatchCompositionCommitEvent(), Warning, "
               "MaybeDispatchKeyEventAsProcessedByIME() returned false",
               this));
      mCompositionState = eCompositionState_NotComposing;
      return false;
    }

    dispatcher = GetTextEventDispatcher();
    MOZ_ASSERT(dispatcher);
    nsresult rv = dispatcher->BeginNativeInputTransaction();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(gGtkIMLog, LogLevel::Error,
              ("0x%p   DispatchCompositionCommitEvent(), FAILED, "
               "due to BeginNativeInputTransaction() failure",
               this));
      return false;
    }

    // Emulate selection until receiving actual selection range.
    mSelection.CollapseTo(
        mCompositionStart + (aCommitString
                                 ? aCommitString->Length()
                                 : mDispatchedCompositionString.Length()),
        mSelection.mWritingMode);
  }

  mCompositionState = eCompositionState_NotComposing;
  // Reset dead key sequence too because GTK doesn't support dead key chain
  // (i.e., a key press doesn't cause both producing some characters and
  // restarting new dead key sequence at one time).  So, committing
  // composition means end of a dead key sequence.
  mMaybeInDeadKeySequence = false;
  mCompositionStart = UINT32_MAX;
  mCompositionTargetRange.Clear();
  mDispatchedCompositionString.Truncate();
  mSelectedStringRemovedByComposition.Truncate();

  if (!dispatcher) {
    MOZ_ASSERT(aCommitString);
    MOZ_ASSERT(!aCommitString->IsEmpty());
    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetContentCommandEvent insertTextEvent(true, eContentCommandInsertText,
                                              lastFocusedWindow);
    insertTextEvent.mString.emplace(*aCommitString);
    lastFocusedWindow->DispatchEvent(&insertTextEvent, status);

    if (!insertTextEvent.mSucceeded) {
      MOZ_LOG(gGtkIMLog, LogLevel::Error,
              ("0x%p   DispatchCompositionChangeEvent(), FAILED, inserting "
               "text failed",
               this));
      return false;
    }
  } else {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsresult rv = dispatcher->CommitComposition(status, aCommitString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(gGtkIMLog, LogLevel::Error,
              ("0x%p   DispatchCompositionChangeEvent(), FAILED, "
               "due to CommitComposition() failure",
               this));
      return false;
    }
  }

  if (lastFocusedWindow->IsDestroyed() ||
      lastFocusedWindow != mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DispatchCompositionCommitEvent(), FAILED, "
             "the focused widget was destroyed/changed by "
             "compositioncommit event",
             this));
    return false;
  }

  return true;
}

already_AddRefed<TextRangeArray> IMContextWrapper::CreateTextRangeArray(
    GtkIMContext* aContext, const nsAString& aCompositionString) {
  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p CreateTextRangeArray(aContext=0x%p, "
           "aCompositionString=\"%s\" (Length()=%u))",
           this, aContext, NS_ConvertUTF16toUTF8(aCompositionString).get(),
           aCompositionString.Length()));

  RefPtr<TextRangeArray> textRangeArray = new TextRangeArray();

  gchar* preedit_string;
  gint cursor_pos_in_chars;
  PangoAttrList* feedback_list;
  gtk_im_context_get_preedit_string(aContext, &preedit_string, &feedback_list,
                                    &cursor_pos_in_chars);
  if (!preedit_string || !*preedit_string) {
    if (!aCompositionString.IsEmpty()) {
      MOZ_LOG(gGtkIMLog, LogLevel::Error,
              ("0x%p   CreateTextRangeArray(), FAILED, due to "
               "preedit_string is null",
               this));
    }
    pango_attr_list_unref(feedback_list);
    g_free(preedit_string);
    return textRangeArray.forget();
  }

  // Convert caret offset from offset in characters to offset in UTF-16
  // string.  If we couldn't proper offset in UTF-16 string, we should
  // assume that the caret is at the end of the composition string.
  uint32_t caretOffsetInUTF16 = aCompositionString.Length();
  if (NS_WARN_IF(cursor_pos_in_chars < 0)) {
    // Note that this case is undocumented.  We should assume that the
    // caret is at the end of the composition string.
  } else if (cursor_pos_in_chars == 0) {
    caretOffsetInUTF16 = 0;
  } else {
    gchar* charAfterCaret =
        g_utf8_offset_to_pointer(preedit_string, cursor_pos_in_chars);
    if (NS_WARN_IF(!charAfterCaret)) {
      MOZ_LOG(gGtkIMLog, LogLevel::Warning,
              ("0x%p   CreateTextRangeArray(), failed to get UTF-8 "
               "string before the caret (cursor_pos_in_chars=%d)",
               this, cursor_pos_in_chars));
    } else {
      glong caretOffset = 0;
      gunichar2* utf16StrBeforeCaret =
          g_utf8_to_utf16(preedit_string, charAfterCaret - preedit_string,
                          nullptr, &caretOffset, nullptr);
      if (NS_WARN_IF(!utf16StrBeforeCaret) || NS_WARN_IF(caretOffset < 0)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Warning,
                ("0x%p   CreateTextRangeArray(), WARNING, failed to "
                 "convert to UTF-16 string before the caret "
                 "(cursor_pos_in_chars=%d, caretOffset=%ld)",
                 this, cursor_pos_in_chars, caretOffset));
      } else {
        caretOffsetInUTF16 = static_cast<uint32_t>(caretOffset);
        uint32_t compositionStringLength = aCompositionString.Length();
        if (NS_WARN_IF(caretOffsetInUTF16 > compositionStringLength)) {
          MOZ_LOG(gGtkIMLog, LogLevel::Warning,
                  ("0x%p   CreateTextRangeArray(), WARNING, "
                   "caretOffsetInUTF16=%u is larger than "
                   "compositionStringLength=%u",
                   this, caretOffsetInUTF16, compositionStringLength));
          caretOffsetInUTF16 = compositionStringLength;
        }
      }
      if (utf16StrBeforeCaret) {
        g_free(utf16StrBeforeCaret);
      }
    }
  }

  PangoAttrIterator* iter;
  iter = pango_attr_list_get_iterator(feedback_list);
  if (!iter) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   CreateTextRangeArray(), FAILED, iterator couldn't "
             "be allocated",
             this));
    pango_attr_list_unref(feedback_list);
    g_free(preedit_string);
    return textRangeArray.forget();
  }

  uint32_t minOffsetOfClauses = aCompositionString.Length();
  uint32_t maxOffsetOfClauses = 0;
  do {
    TextRange range;
    if (!SetTextRange(iter, preedit_string, caretOffsetInUTF16, range)) {
      continue;
    }
    MOZ_ASSERT(range.Length());
    minOffsetOfClauses = std::min(minOffsetOfClauses, range.mStartOffset);
    maxOffsetOfClauses = std::max(maxOffsetOfClauses, range.mEndOffset);
    textRangeArray->AppendElement(range);
  } while (pango_attr_iterator_next(iter));

  // If the IME doesn't define clause from the start of the composition,
  // we should insert dummy clause information since TextRangeArray assumes
  // that there must be a clause whose start is 0 when there is one or
  // more clauses.
  if (minOffsetOfClauses) {
    TextRange dummyClause;
    dummyClause.mStartOffset = 0;
    dummyClause.mEndOffset = minOffsetOfClauses;
    dummyClause.mRangeType = TextRangeType::eRawClause;
    textRangeArray->InsertElementAt(0, dummyClause);
    maxOffsetOfClauses = std::max(maxOffsetOfClauses, dummyClause.mEndOffset);
    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("0x%p   CreateTextRangeArray(), inserting a dummy clause "
             "at the beginning of the composition string mStartOffset=%u, "
             "mEndOffset=%u, mRangeType=%s",
             this, dummyClause.mStartOffset, dummyClause.mEndOffset,
             ToChar(dummyClause.mRangeType)));
  }

  // If the IME doesn't define clause at end of the composition, we should
  // insert dummy clause information since TextRangeArray assumes that there
  // must be a clase whose end is the length of the composition string when
  // there is one or more clauses.
  if (!textRangeArray->IsEmpty() &&
      maxOffsetOfClauses < aCompositionString.Length()) {
    TextRange dummyClause;
    dummyClause.mStartOffset = maxOffsetOfClauses;
    dummyClause.mEndOffset = aCompositionString.Length();
    dummyClause.mRangeType = TextRangeType::eRawClause;
    textRangeArray->AppendElement(dummyClause);
    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("0x%p   CreateTextRangeArray(), inserting a dummy clause "
             "at the end of the composition string mStartOffset=%u, "
             "mEndOffset=%u, mRangeType=%s",
             this, dummyClause.mStartOffset, dummyClause.mEndOffset,
             ToChar(dummyClause.mRangeType)));
  }

  TextRange range;
  range.mStartOffset = range.mEndOffset = caretOffsetInUTF16;
  range.mRangeType = TextRangeType::eCaret;
  textRangeArray->AppendElement(range);
  MOZ_LOG(
      gGtkIMLog, LogLevel::Debug,
      ("0x%p   CreateTextRangeArray(), mStartOffset=%u, "
       "mEndOffset=%u, mRangeType=%s",
       this, range.mStartOffset, range.mEndOffset, ToChar(range.mRangeType)));

  pango_attr_iterator_destroy(iter);
  pango_attr_list_unref(feedback_list);
  g_free(preedit_string);

  return textRangeArray.forget();
}

/* static */
nscolor IMContextWrapper::ToNscolor(PangoAttrColor* aPangoAttrColor) {
  PangoColor& pangoColor = aPangoAttrColor->color;
  uint8_t r = pangoColor.red / 0x100;
  uint8_t g = pangoColor.green / 0x100;
  uint8_t b = pangoColor.blue / 0x100;
  return NS_RGB(r, g, b);
}

bool IMContextWrapper::SetTextRange(PangoAttrIterator* aPangoAttrIter,
                                    const gchar* aUTF8CompositionString,
                                    uint32_t aUTF16CaretOffset,
                                    TextRange& aTextRange) const {
  // Set the range offsets in UTF-16 string.
  gint utf8ClauseStart, utf8ClauseEnd;
  pango_attr_iterator_range(aPangoAttrIter, &utf8ClauseStart, &utf8ClauseEnd);
  if (utf8ClauseStart == utf8ClauseEnd) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   SetTextRange(), FAILED, due to collapsed range", this));
    return false;
  }

  if (!utf8ClauseStart) {
    aTextRange.mStartOffset = 0;
  } else {
    glong utf16PreviousClausesLength;
    gunichar2* utf16PreviousClausesString =
        g_utf8_to_utf16(aUTF8CompositionString, utf8ClauseStart, nullptr,
                        &utf16PreviousClausesLength, nullptr);

    if (NS_WARN_IF(!utf16PreviousClausesString)) {
      MOZ_LOG(gGtkIMLog, LogLevel::Error,
              ("0x%p   SetTextRange(), FAILED, due to g_utf8_to_utf16() "
               "failure (retrieving previous string of current clause)",
               this));
      return false;
    }

    aTextRange.mStartOffset = utf16PreviousClausesLength;
    g_free(utf16PreviousClausesString);
  }

  glong utf16CurrentClauseLength;
  gunichar2* utf16CurrentClauseString = g_utf8_to_utf16(
      aUTF8CompositionString + utf8ClauseStart, utf8ClauseEnd - utf8ClauseStart,
      nullptr, &utf16CurrentClauseLength, nullptr);

  if (NS_WARN_IF(!utf16CurrentClauseString)) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   SetTextRange(), FAILED, due to g_utf8_to_utf16() "
             "failure (retrieving current clause)",
             this));
    return false;
  }

  // iBus Chewing IME tells us that there is an empty clause at the end of
  // the composition string but we should ignore it since our code doesn't
  // assume that there is an empty clause.
  if (!utf16CurrentClauseLength) {
    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("0x%p   SetTextRange(), FAILED, due to current clause length "
             "is 0",
             this));
    return false;
  }

  aTextRange.mEndOffset = aTextRange.mStartOffset + utf16CurrentClauseLength;
  g_free(utf16CurrentClauseString);
  utf16CurrentClauseString = nullptr;

  // Set styles
  TextRangeStyle& style = aTextRange.mRangeStyle;

  // Underline
  PangoAttrInt* attrUnderline = reinterpret_cast<PangoAttrInt*>(
      pango_attr_iterator_get(aPangoAttrIter, PANGO_ATTR_UNDERLINE));
  if (attrUnderline) {
    switch (attrUnderline->value) {
      case PANGO_UNDERLINE_NONE:
        style.mLineStyle = TextRangeStyle::LineStyle::None;
        break;
      case PANGO_UNDERLINE_DOUBLE:
        style.mLineStyle = TextRangeStyle::LineStyle::Double;
        break;
      case PANGO_UNDERLINE_ERROR:
        style.mLineStyle = TextRangeStyle::LineStyle::Wavy;
        break;
      case PANGO_UNDERLINE_SINGLE:
      case PANGO_UNDERLINE_LOW:
        style.mLineStyle = TextRangeStyle::LineStyle::Solid;
        break;
      default:
        MOZ_LOG(gGtkIMLog, LogLevel::Warning,
                ("0x%p   SetTextRange(), retrieved unknown underline "
                 "style: %d",
                 this, attrUnderline->value));
        style.mLineStyle = TextRangeStyle::LineStyle::Solid;
        break;
    }
    style.mDefinedStyles |= TextRangeStyle::DEFINED_LINESTYLE;

    // Underline color
    PangoAttrColor* attrUnderlineColor = reinterpret_cast<PangoAttrColor*>(
        pango_attr_iterator_get(aPangoAttrIter, PANGO_ATTR_UNDERLINE_COLOR));
    if (attrUnderlineColor) {
      style.mUnderlineColor = ToNscolor(attrUnderlineColor);
      style.mDefinedStyles |= TextRangeStyle::DEFINED_UNDERLINE_COLOR;
    }
  } else {
    style.mLineStyle = TextRangeStyle::LineStyle::None;
    style.mDefinedStyles |= TextRangeStyle::DEFINED_LINESTYLE;
  }

  // Don't set colors if they are not specified.  They should be computed by
  // textframe if only one of the colors are specified.

  // Foreground color (text color)
  PangoAttrColor* attrForeground = reinterpret_cast<PangoAttrColor*>(
      pango_attr_iterator_get(aPangoAttrIter, PANGO_ATTR_FOREGROUND));
  if (attrForeground) {
    style.mForegroundColor = ToNscolor(attrForeground);
    style.mDefinedStyles |= TextRangeStyle::DEFINED_FOREGROUND_COLOR;
  }

  // Background color
  PangoAttrColor* attrBackground = reinterpret_cast<PangoAttrColor*>(
      pango_attr_iterator_get(aPangoAttrIter, PANGO_ATTR_BACKGROUND));
  if (attrBackground) {
    style.mBackgroundColor = ToNscolor(attrBackground);
    style.mDefinedStyles |= TextRangeStyle::DEFINED_BACKGROUND_COLOR;
  }

  /**
   * We need to judge the meaning of the clause for a11y.  Before we support
   * IME specific composition string style, we used following rules:
   *
   *   1: If attrUnderline and attrForground are specified, we assumed the
   *      clause is TextRangeType::eSelectedClause.
   *   2: If only attrUnderline is specified, we assumed the clause is
   *      TextRangeType::eConvertedClause.
   *   3: If only attrForground is specified, we assumed the clause is
   *      TextRangeType::eSelectedRawClause.
   *   4: If neither attrUnderline nor attrForeground is specified, we assumed
   *      the clause is TextRangeType::eRawClause.
   *
   * However, this rules are odd since there can be two or more selected
   * clauses.  Additionally, our old rules caused that IME developers/users
   * cannot specify composition string style as they want.
   *
   * So, we shouldn't guess the meaning from its visual style.
   */

  // If the range covers whole of composition string and the caret is at
  // the end of the composition string, the range is probably not converted.
  if (!utf8ClauseStart &&
      utf8ClauseEnd == static_cast<gint>(strlen(aUTF8CompositionString)) &&
      aTextRange.mEndOffset == aUTF16CaretOffset) {
    aTextRange.mRangeType = TextRangeType::eRawClause;
  }
  // Typically, the caret is set at the start of the selected clause.
  // So, if the caret is in the clause, we can assume that the clause is
  // selected.
  else if (aTextRange.mStartOffset <= aUTF16CaretOffset &&
           aTextRange.mEndOffset > aUTF16CaretOffset) {
    aTextRange.mRangeType = TextRangeType::eSelectedClause;
  }
  // Otherwise, we should assume that the clause is converted but not
  // selected.
  else {
    aTextRange.mRangeType = TextRangeType::eConvertedClause;
  }

  MOZ_LOG(gGtkIMLog, LogLevel::Debug,
          ("0x%p   SetTextRange(), succeeded, aTextRange= { "
           "mStartOffset=%u, mEndOffset=%u, mRangeType=%s, mRangeStyle=%s }",
           this, aTextRange.mStartOffset, aTextRange.mEndOffset,
           ToChar(aTextRange.mRangeType),
           GetTextRangeStyleText(aTextRange.mRangeStyle).get()));

  return true;
}

void IMContextWrapper::SetCursorPosition(GtkIMContext* aContext) {
  MOZ_LOG(
      gGtkIMLog, LogLevel::Info,
      ("0x%p SetCursorPosition(aContext=0x%p), "
       "mCompositionTargetRange={ mOffset=%u, mLength=%u }"
       "mSelection={ mOffset=%u, Length()=%u, mWritingMode=%s }",
       this, aContext, mCompositionTargetRange.mOffset,
       mCompositionTargetRange.mLength, mSelection.mOffset, mSelection.Length(),
       GetWritingModeName(mSelection.mWritingMode).get()));

  bool useCaret = false;
  if (!mCompositionTargetRange.IsValid()) {
    if (!mSelection.IsValid()) {
      MOZ_LOG(gGtkIMLog, LogLevel::Error,
              ("0x%p   SetCursorPosition(), FAILED, "
               "mCompositionTargetRange and mSelection are invalid",
               this));
      return;
    }
    useCaret = true;
  }

  if (!mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   SetCursorPosition(), FAILED, due to no focused "
             "window",
             this));
    return;
  }

  if (MOZ_UNLIKELY(!aContext)) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   SetCursorPosition(), FAILED, due to no context", this));
    return;
  }

  WidgetQueryContentEvent queryCaretOrTextRectEvent(
      true, useCaret ? eQueryCaretRect : eQueryTextRect, mLastFocusedWindow);
  if (useCaret) {
    queryCaretOrTextRectEvent.InitForQueryCaretRect(mSelection.mOffset);
  } else {
    if (mSelection.mWritingMode.IsVertical()) {
      // For preventing the candidate window to overlap the target
      // clause, we should set fake (typically, very tall) caret rect.
      uint32_t length =
          mCompositionTargetRange.mLength ? mCompositionTargetRange.mLength : 1;
      queryCaretOrTextRectEvent.InitForQueryTextRect(
          mCompositionTargetRange.mOffset, length);
    } else {
      queryCaretOrTextRectEvent.InitForQueryTextRect(
          mCompositionTargetRange.mOffset, 1);
    }
  }
  InitEvent(queryCaretOrTextRectEvent);
  nsEventStatus status;
  mLastFocusedWindow->DispatchEvent(&queryCaretOrTextRectEvent, status);
  if (queryCaretOrTextRectEvent.Failed()) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   SetCursorPosition(), FAILED, %s was failed", this,
             useCaret ? "eQueryCaretRect" : "eQueryTextRect"));
    return;
  }

  nsWindow* rootWindow =
      static_cast<nsWindow*>(mLastFocusedWindow->GetTopLevelWidget());

  // Get the position of the rootWindow in screen.
  LayoutDeviceIntPoint root = rootWindow->WidgetToScreenOffset();

  // Get the position of IM context owner window in screen.
  LayoutDeviceIntPoint owner = mOwnerWindow->WidgetToScreenOffset();

  // Compute the caret position in the IM owner window.
  LayoutDeviceIntRect rect =
      queryCaretOrTextRectEvent.mReply->mRect + root - owner;
  rect.width = 0;
  GdkRectangle area = rootWindow->DevicePixelsToGdkRectRoundOut(rect);

  gtk_im_context_set_cursor_location(aContext, &area);
}

nsresult IMContextWrapper::GetCurrentParagraph(nsAString& aText,
                                               uint32_t& aCursorPos) {
  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p GetCurrentParagraph(), mCompositionState=%s", this,
           GetCompositionStateName()));

  if (!mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   GetCurrentParagraph(), FAILED, there are no "
             "focused window in this module",
             this));
    return NS_ERROR_NULL_POINTER;
  }

  nsEventStatus status;

  uint32_t selOffset = mCompositionStart;
  uint32_t selLength = mSelectedStringRemovedByComposition.Length();

  // If focused editor doesn't have composition string, we should use
  // current selection.
  if (!EditorHasCompositionString()) {
    // Query cursor position & selection
    if (NS_WARN_IF(!EnsureToCacheSelection())) {
      MOZ_LOG(gGtkIMLog, LogLevel::Error,
              ("0x%p   GetCurrentParagraph(), FAILED, due to no "
               "valid selection information",
               this));
      return NS_ERROR_FAILURE;
    }

    selOffset = mSelection.mOffset;
    selLength = mSelection.Length();
  }

  MOZ_LOG(gGtkIMLog, LogLevel::Debug,
          ("0x%p   GetCurrentParagraph(), selOffset=%u, selLength=%u", this,
           selOffset, selLength));

  // XXX nsString::Find and nsString::RFind take int32_t for offset, so,
  //     we cannot support this request when the current offset is larger
  //     than INT32_MAX.
  if (selOffset > INT32_MAX || selLength > INT32_MAX ||
      selOffset + selLength > INT32_MAX) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   GetCurrentParagraph(), FAILED, The selection is "
             "out of range",
             this));
    return NS_ERROR_FAILURE;
  }

  // Get all text contents of the focused editor
  WidgetQueryContentEvent queryTextContentEvent(true, eQueryTextContent,
                                                mLastFocusedWindow);
  queryTextContentEvent.InitForQueryTextContent(0, UINT32_MAX);
  mLastFocusedWindow->DispatchEvent(&queryTextContentEvent, status);
  if (NS_WARN_IF(queryTextContentEvent.Failed())) {
    return NS_ERROR_FAILURE;
  }

  if (selOffset + selLength > queryTextContentEvent.mReply->DataLength()) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   GetCurrentParagraph(), FAILED, The selection is "
             "invalid, queryTextContentEvent={ mReply=%s }",
             this, ToString(queryTextContentEvent.mReply).c_str()));
    return NS_ERROR_FAILURE;
  }

  // Remove composing string and restore the selected string because
  // GtkEntry doesn't remove selected string until committing, however,
  // our editor does it.  We should emulate the behavior for IME.
  nsAutoString textContent(queryTextContentEvent.mReply->DataRef());
  if (EditorHasCompositionString() &&
      mDispatchedCompositionString != mSelectedStringRemovedByComposition) {
    textContent.Replace(mCompositionStart,
                        mDispatchedCompositionString.Length(),
                        mSelectedStringRemovedByComposition);
  }

  // Get only the focused paragraph, by looking for newlines
  int32_t parStart =
      (selOffset == 0) ? 0
                       : textContent.RFind("\n", false, selOffset - 1, -1) + 1;
  int32_t parEnd = textContent.Find("\n", false, selOffset + selLength, -1);
  if (parEnd < 0) {
    parEnd = textContent.Length();
  }
  aText = nsDependentSubstring(textContent, parStart, parEnd - parStart);
  aCursorPos = selOffset - uint32_t(parStart);

  MOZ_LOG(
      gGtkIMLog, LogLevel::Debug,
      ("0x%p   GetCurrentParagraph(), succeeded, aText=%s, "
       "aText.Length()=%u, aCursorPos=%u",
       this, NS_ConvertUTF16toUTF8(aText).get(), aText.Length(), aCursorPos));

  return NS_OK;
}

nsresult IMContextWrapper::DeleteText(GtkIMContext* aContext, int32_t aOffset,
                                      uint32_t aNChars) {
  MOZ_LOG(gGtkIMLog, LogLevel::Info,
          ("0x%p DeleteText(aContext=0x%p, aOffset=%d, aNChars=%u), "
           "mCompositionState=%s",
           this, aContext, aOffset, aNChars, GetCompositionStateName()));

  if (!mLastFocusedWindow) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DeleteText(), FAILED, there are no focused window "
             "in this module",
             this));
    return NS_ERROR_NULL_POINTER;
  }

  if (!aNChars) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DeleteText(), FAILED, aNChars must not be zero", this));
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<nsWindow> lastFocusedWindow(mLastFocusedWindow);
  nsEventStatus status;

  // First, we should cancel current composition because editor cannot
  // handle changing selection and deleting text.
  uint32_t selOffset;
  bool wasComposing = IsComposing();
  bool editorHadCompositionString = EditorHasCompositionString();
  if (wasComposing) {
    selOffset = mCompositionStart;
    if (!DispatchCompositionCommitEvent(aContext,
                                        &mSelectedStringRemovedByComposition)) {
      MOZ_LOG(gGtkIMLog, LogLevel::Error,
              ("0x%p   DeleteText(), FAILED, quitting from DeletText", this));
      return NS_ERROR_FAILURE;
    }
  } else {
    if (NS_WARN_IF(!EnsureToCacheSelection())) {
      MOZ_LOG(gGtkIMLog, LogLevel::Error,
              ("0x%p   DeleteText(), FAILED, due to no valid selection "
               "information",
               this));
      return NS_ERROR_FAILURE;
    }
    selOffset = mSelection.mOffset;
  }

  // Get all text contents of the focused editor
  WidgetQueryContentEvent queryTextContentEvent(true, eQueryTextContent,
                                                mLastFocusedWindow);
  queryTextContentEvent.InitForQueryTextContent(0, UINT32_MAX);
  mLastFocusedWindow->DispatchEvent(&queryTextContentEvent, status);
  if (NS_WARN_IF(queryTextContentEvent.Failed())) {
    return NS_ERROR_FAILURE;
  }
  if (queryTextContentEvent.mReply->IsDataEmpty()) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DeleteText(), FAILED, there is no contents", this));
    return NS_ERROR_FAILURE;
  }

  NS_ConvertUTF16toUTF8 utf8Str(nsDependentSubstring(
      queryTextContentEvent.mReply->DataRef(), 0, selOffset));
  glong offsetInUTF8Characters =
      g_utf8_strlen(utf8Str.get(), utf8Str.Length()) + aOffset;
  if (offsetInUTF8Characters < 0) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DeleteText(), FAILED, aOffset is too small for "
             "current cursor pos (computed offset: %ld)",
             this, offsetInUTF8Characters));
    return NS_ERROR_FAILURE;
  }

  AppendUTF16toUTF8(
      nsDependentSubstring(queryTextContentEvent.mReply->DataRef(), selOffset),
      utf8Str);
  glong countOfCharactersInUTF8 =
      g_utf8_strlen(utf8Str.get(), utf8Str.Length());
  glong endInUTF8Characters = offsetInUTF8Characters + aNChars;
  if (countOfCharactersInUTF8 < endInUTF8Characters) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DeleteText(), FAILED, aNChars is too large for "
             "current contents (content length: %ld, computed end offset: %ld)",
             this, countOfCharactersInUTF8, endInUTF8Characters));
    return NS_ERROR_FAILURE;
  }

  gchar* charAtOffset =
      g_utf8_offset_to_pointer(utf8Str.get(), offsetInUTF8Characters);
  gchar* charAtEnd =
      g_utf8_offset_to_pointer(utf8Str.get(), endInUTF8Characters);

  // Set selection to delete
  WidgetSelectionEvent selectionEvent(true, eSetSelection, mLastFocusedWindow);

  nsDependentCSubstring utf8StrBeforeOffset(utf8Str, 0,
                                            charAtOffset - utf8Str.get());
  selectionEvent.mOffset = NS_ConvertUTF8toUTF16(utf8StrBeforeOffset).Length();

  nsDependentCSubstring utf8DeletingStr(utf8Str, utf8StrBeforeOffset.Length(),
                                        charAtEnd - charAtOffset);
  selectionEvent.mLength = NS_ConvertUTF8toUTF16(utf8DeletingStr).Length();

  selectionEvent.mReversed = false;
  selectionEvent.mExpandToClusterBoundary = false;
  lastFocusedWindow->DispatchEvent(&selectionEvent, status);

  if (!selectionEvent.mSucceeded || lastFocusedWindow != mLastFocusedWindow ||
      lastFocusedWindow->Destroyed()) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DeleteText(), FAILED, setting selection caused "
             "focus change or window destroyed",
             this));
    return NS_ERROR_FAILURE;
  }

  // If this deleting text caused by a key press, we need to dispatch
  // eKeyDown or eKeyUp before dispatching eContentCommandDelete event.
  if (!MaybeDispatchKeyEventAsProcessedByIME(eContentCommandDelete)) {
    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("0x%p   DeleteText(), Warning, "
             "MaybeDispatchKeyEventAsProcessedByIME() returned false",
             this));
    return NS_ERROR_FAILURE;
  }

  // Delete the selection
  WidgetContentCommandEvent contentCommandEvent(true, eContentCommandDelete,
                                                mLastFocusedWindow);
  mLastFocusedWindow->DispatchEvent(&contentCommandEvent, status);

  if (!contentCommandEvent.mSucceeded ||
      lastFocusedWindow != mLastFocusedWindow ||
      lastFocusedWindow->Destroyed()) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p   DeleteText(), FAILED, deleting the selection caused "
             "focus change or window destroyed",
             this));
    return NS_ERROR_FAILURE;
  }

  if (!wasComposing) {
    return NS_OK;
  }

  // Restore the composition at new caret position.
  if (!DispatchCompositionStart(aContext)) {
    MOZ_LOG(
        gGtkIMLog, LogLevel::Error,
        ("0x%p   DeleteText(), FAILED, resterting composition start", this));
    return NS_ERROR_FAILURE;
  }

  if (!editorHadCompositionString) {
    return NS_OK;
  }

  nsAutoString compositionString;
  GetCompositionString(aContext, compositionString);
  if (!DispatchCompositionChangeEvent(aContext, compositionString)) {
    MOZ_LOG(
        gGtkIMLog, LogLevel::Error,
        ("0x%p   DeleteText(), FAILED, restoring composition string", this));
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void IMContextWrapper::InitEvent(WidgetGUIEvent& aEvent) {
  aEvent.mTime = PR_Now() / 1000;
}

bool IMContextWrapper::EnsureToCacheSelection(nsAString* aSelectedString) {
  if (aSelectedString) {
    aSelectedString->Truncate();
  }

  if (mSelection.IsValid()) {
    if (aSelectedString) {
      *aSelectedString = mSelection.mString;
    }
    return true;
  }

  if (NS_WARN_IF(!mLastFocusedWindow)) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p EnsureToCacheSelection(), FAILED, due to "
             "no focused window",
             this));
    return false;
  }

  nsEventStatus status;
  WidgetQueryContentEvent querySelectedTextEvent(true, eQuerySelectedText,
                                                 mLastFocusedWindow);
  InitEvent(querySelectedTextEvent);
  mLastFocusedWindow->DispatchEvent(&querySelectedTextEvent, status);
  if (NS_WARN_IF(querySelectedTextEvent.Failed())) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p EnsureToCacheSelection(), FAILED, due to "
             "failure of query selection event",
             this));
    return false;
  }

  mSelection.Assign(querySelectedTextEvent);
  if (!mSelection.IsValid()) {
    MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("0x%p EnsureToCacheSelection(), FAILED, due to "
             "failure of query selection event (invalid result)",
             this));
    return false;
  }

  if (!mSelection.Collapsed() && aSelectedString) {
    aSelectedString->Assign(querySelectedTextEvent.mReply->DataRef());
  }

  MOZ_LOG(gGtkIMLog, LogLevel::Debug,
          ("0x%p EnsureToCacheSelection(), Succeeded, mSelection="
           "{ mOffset=%u, Length()=%u, mWritingMode=%s }",
           this, mSelection.mOffset, mSelection.Length(),
           GetWritingModeName(mSelection.mWritingMode).get()));
  return true;
}

/******************************************************************************
 * IMContextWrapper::Selection
 ******************************************************************************/

void IMContextWrapper::Selection::Assign(
    const IMENotification& aIMENotification) {
  MOZ_ASSERT(aIMENotification.mMessage == NOTIFY_IME_OF_SELECTION_CHANGE);
  mString = aIMENotification.mSelectionChangeData.String();
  mOffset = aIMENotification.mSelectionChangeData.mOffset;
  mWritingMode = aIMENotification.mSelectionChangeData.GetWritingMode();
}

void IMContextWrapper::Selection::Assign(
    const WidgetQueryContentEvent& aEvent) {
  MOZ_ASSERT(aEvent.mMessage == eQuerySelectedText);
  MOZ_ASSERT(aEvent.Succeeded());
  MOZ_ASSERT(aEvent.mReply->mOffsetAndData.isSome());
  mString = aEvent.mReply->DataRef();
  mOffset = aEvent.mReply->StartOffset();
  mWritingMode = aEvent.mReply->WritingModeRef();
}

}  // namespace widget
}  // namespace mozilla
