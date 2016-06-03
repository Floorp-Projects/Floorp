/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "prtime.h"

#include "IMContextWrapper.h"
#include "nsGtkKeyUtils.h"
#include "nsWindow.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Likely.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"
#include "WritingModes.h"

namespace mozilla {
namespace widget {

PRLogModuleInfo* gGtkIMLog = nullptr;

static inline const char*
ToChar(bool aBool)
{
    return aBool ? "true" : "false";
}

static const char*
GetEnabledStateName(uint32_t aState)
{
    switch (aState) {
        case IMEState::DISABLED:
            return "DISABLED";
        case IMEState::ENABLED:
            return "ENABLED";
        case IMEState::PASSWORD:
            return "PASSWORD";
        case IMEState::PLUGIN:
            return "PLUG_IN";
        default:
            return "UNKNOWN ENABLED STATUS!!";
    }
}

static const char*
GetEventType(GdkEventKey* aKeyEvent)
{
    switch (aKeyEvent->type) {
        case GDK_KEY_PRESS:
            return "GDK_KEY_PRESS";
        case GDK_KEY_RELEASE:
            return "GDK_KEY_RELEASE";
        default:
            return "Unknown";
    }
}

class GetWritingModeName : public nsAutoCString
{
public:
  explicit GetWritingModeName(const WritingMode& aWritingMode)
  {
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
  virtual ~GetWritingModeName() {}
};

class GetTextRangeStyleText final : public nsAutoCString
{
public:
    explicit GetTextRangeStyleText(const TextRangeStyle& aStyle)
    {
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
    void AppendLineStyle(uint8_t aLineStyle)
    {
        switch (aLineStyle) {
            case TextRangeStyle::LINESTYLE_NONE:
                AppendLiteral("LINESTYLE_NONE");
                break;
            case TextRangeStyle::LINESTYLE_SOLID:
                AppendLiteral("LINESTYLE_SOLID");
                break;
            case TextRangeStyle::LINESTYLE_DOTTED:
                AppendLiteral("LINESTYLE_DOTTED");
                break;
            case TextRangeStyle::LINESTYLE_DASHED:
                AppendLiteral("LINESTYLE_DASHED");
                break;
            case TextRangeStyle::LINESTYLE_DOUBLE:
                AppendLiteral("LINESTYLE_DOUBLE");
                break;
            case TextRangeStyle::LINESTYLE_WAVY:
                AppendLiteral("LINESTYLE_WAVY");
                break;
            default:
                AppendPrintf("Invalid(0x%02X)", aLineStyle);
                break;
        }
    }
    void AppendColor(nscolor aColor)
    {
        AppendPrintf("{ R=0x%02X, G=0x%02X, B=0x%02X, A=0x%02X }",
                     NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor),
                     NS_GET_A(aColor));
    }
    virtual ~GetTextRangeStyleText() {};
};

const static bool kUseSimpleContextDefault = MOZ_WIDGET_GTK == 2;

/******************************************************************************
 * IMContextWrapper
 ******************************************************************************/

IMContextWrapper* IMContextWrapper::sLastFocusedContext = nullptr;
bool IMContextWrapper::sUseSimpleContext;

NS_IMPL_ISUPPORTS(IMContextWrapper,
                  TextEventDispatcherListener,
                  nsISupportsWeakReference)

IMContextWrapper::IMContextWrapper(nsWindow* aOwnerWindow)
    : mOwnerWindow(aOwnerWindow)
    , mLastFocusedWindow(nullptr)
    , mContext(nullptr)
    , mSimpleContext(nullptr)
    , mDummyContext(nullptr)
    , mComposingContext(nullptr)
    , mCompositionStart(UINT32_MAX)
    , mProcessingKeyEvent(nullptr)
    , mCompositionState(eCompositionState_NotComposing)
    , mIsIMFocused(false)
    , mIsDeletingSurrounding(false)
    , mLayoutChanged(false)
    , mSetCursorPositionOnKeyEvent(true)
    , mPendingResettingIMContext(false)
{
    if (!gGtkIMLog) {
        gGtkIMLog = PR_NewLogModule("nsGtkIMModuleWidgets");
    }
    static bool sFirstInstance = true;
    if (sFirstInstance) {
        sFirstInstance = false;
        sUseSimpleContext =
            Preferences::GetBool(
                "intl.ime.use_simple_context_on_password_field",
                kUseSimpleContextDefault);
    }
    Init();
}

void
IMContextWrapper::Init()
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p Init(), mOwnerWindow=%p",
         this, mOwnerWindow));

    MozContainer* container = mOwnerWindow->GetMozContainer();
    NS_PRECONDITION(container, "container is null");
    GdkWindow* gdkWindow = gtk_widget_get_window(GTK_WIDGET(container));

    // NOTE: gtk_im_*_new() abort (kill) the whole process when it fails.
    //       So, we don't need to check the result.

    // Normal context.
    mContext = gtk_im_multicontext_new();
    gtk_im_context_set_client_window(mContext, gdkWindow);
    g_signal_connect(mContext, "preedit_changed",
        G_CALLBACK(IMContextWrapper::OnChangeCompositionCallback), this);
    g_signal_connect(mContext, "retrieve_surrounding",
        G_CALLBACK(IMContextWrapper::OnRetrieveSurroundingCallback), this);
    g_signal_connect(mContext, "delete_surrounding",
        G_CALLBACK(IMContextWrapper::OnDeleteSurroundingCallback), this);
    g_signal_connect(mContext, "commit",
        G_CALLBACK(IMContextWrapper::OnCommitCompositionCallback), this);
    g_signal_connect(mContext, "preedit_start",
        G_CALLBACK(IMContextWrapper::OnStartCompositionCallback), this);
    g_signal_connect(mContext, "preedit_end",
        G_CALLBACK(IMContextWrapper::OnEndCompositionCallback), this);

    // Simple context
    if (sUseSimpleContext) {
        mSimpleContext = gtk_im_context_simple_new();
        gtk_im_context_set_client_window(mSimpleContext, gdkWindow);
        g_signal_connect(mSimpleContext, "preedit_changed",
            G_CALLBACK(&IMContextWrapper::OnChangeCompositionCallback),
            this);
        g_signal_connect(mSimpleContext, "retrieve_surrounding",
            G_CALLBACK(&IMContextWrapper::OnRetrieveSurroundingCallback),
            this);
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
}

IMContextWrapper::~IMContextWrapper()
{
    if (this == sLastFocusedContext) {
        sLastFocusedContext = nullptr;
    }
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p ~IMContextWrapper()", this));
}

NS_IMETHODIMP
IMContextWrapper::NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                            const IMENotification& aNotification)
{
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
IMContextWrapper::OnRemovedFrom(TextEventDispatcher* aTextEventDispatcher)
{
    // XXX When input transaction is being stolen by add-on, what should we do?
}

NS_IMETHODIMP_(void)
IMContextWrapper::WillDispatchKeyboardEvent(
                      TextEventDispatcher* aTextEventDispatcher,
                      WidgetKeyboardEvent& aKeyboardEvent,
                      uint32_t aIndexOfKeypress,
                      void* aData)
{
    KeymapWrapper::WillDispatchKeyboardEvent(aKeyboardEvent,
                                             static_cast<GdkEventKey*>(aData));
}

TextEventDispatcher*
IMContextWrapper::GetTextEventDispatcher()
{
  if (NS_WARN_IF(!mLastFocusedWindow)) {
    return nullptr;
  }
  TextEventDispatcher* dispatcher =
    mLastFocusedWindow->GetTextEventDispatcher();
  // nsIWidget::GetTextEventDispatcher() shouldn't return nullptr.
  MOZ_RELEASE_ASSERT(dispatcher);
  return dispatcher;
}

nsIMEUpdatePreference
IMContextWrapper::GetIMEUpdatePreference() const
{
    // While a plugin has focus, IMContextWrapper doesn't need any
    // notifications.
    if (mInputContext.mIMEState.mEnabled == IMEState::PLUGIN) {
      return nsIMEUpdatePreference();
    }

    nsIMEUpdatePreference::Notifications notifications =
        nsIMEUpdatePreference::NOTIFY_SELECTION_CHANGE;
    // If it's not enabled, we don't need position change notification.
    if (IsEnabled()) {
        notifications |= nsIMEUpdatePreference::NOTIFY_POSITION_CHANGE;
    }
    nsIMEUpdatePreference updatePreference(notifications);
    // We shouldn't notify IME of selection change caused by changes of
    // composition string.  Therefore, we don't need to be notified selection
    // changes which are caused by compositionchange events handled.
    updatePreference.DontNotifyChangesCausedByComposition();
    return updatePreference;
}

void
IMContextWrapper::OnDestroyWindow(nsWindow* aWindow)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p OnDestroyWindow(aWindow=%p), mLastFocusedWindow=%p, "
         "mOwnerWindow=%p, mLastFocusedModule=%p",
         this, aWindow, mLastFocusedWindow, mOwnerWindow, sLastFocusedContext));

    NS_PRECONDITION(aWindow, "aWindow must not be null");

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
    mInputContext.mIMEState.mEnabled = IMEState::DISABLED;

    MOZ_LOG(gGtkIMLog, LogLevel::Debug,
        ("GTKIM: %p   OnDestroyWindow(), succeeded, Completely destroyed",
         this));
}

// Work around gtk bug http://bugzilla.gnome.org/show_bug.cgi?id=483223:
// (and the similar issue of GTK+ IIIM)
// The GTK+ XIM and IIIM modules register handlers for the "closed" signal
// on the display, but:
//  * The signal handlers are not disconnected when the module is unloaded.
//
// The GTK+ XIM module has another problem:
//  * When the signal handler is run (with the module loaded) it tries
//    XFree (and fails) on a pointer that did not come from Xmalloc.
//
// To prevent these modules from being unloaded, use static variables to
// hold ref of GtkIMContext class.
// For GTK+ XIM module, to prevent the signal handler from being run,
// find the signal handlers and remove them.
//
// GtkIMContextXIMs share XOpenIM connections and display closed signal
// handlers (where possible).

void
IMContextWrapper::PrepareToDestroyContext(GtkIMContext* aContext)
{
#if (MOZ_WIDGET_GTK == 2)
    GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT(aContext);
    GtkIMContext *slave = multicontext->slave;
#else
    GtkIMContext *slave = nullptr; //TODO GTK3
#endif
    if (!slave) {
        return;
    }

    GType slaveType = G_TYPE_FROM_INSTANCE(slave);
    const gchar *im_type_name = g_type_name(slaveType);
    if (strcmp(im_type_name, "GtkIMContextIIIM") == 0) {
        // Add a reference to prevent the IIIM module from being unloaded
        static gpointer gtk_iiim_context_class =
            g_type_class_ref(slaveType);
        // Mute unused variable warning:
        (void)gtk_iiim_context_class;
    }
}

void
IMContextWrapper::OnFocusWindow(nsWindow* aWindow)
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p OnFocusWindow(aWindow=%p), mLastFocusedWindow=%p",
         this, aWindow, mLastFocusedWindow));
    mLastFocusedWindow = aWindow;
    Focus();
}

void
IMContextWrapper::OnBlurWindow(nsWindow* aWindow)
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p OnBlurWindow(aWindow=%p), mLastFocusedWindow=%p, "
         "mIsIMFocused=%s",
         this, aWindow, mLastFocusedWindow, ToChar(mIsIMFocused)));

    if (!mIsIMFocused || mLastFocusedWindow != aWindow) {
        return;
    }

    Blur();
}

bool
IMContextWrapper::OnKeyEvent(nsWindow* aCaller,
                             GdkEventKey* aEvent,
                             bool aKeyDownEventWasSent /* = false */)
{
    NS_PRECONDITION(aEvent, "aEvent must be non-null");

    if (!mInputContext.mIMEState.MaybeEditable() ||
        MOZ_UNLIKELY(IsDestroyed())) {
        return false;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p OnKeyEvent(aCaller=%p, aKeyDownEventWasSent=%s), "
         "mCompositionState=%s, current context=%p, active context=%p, "
         "aEvent(%p): { type=%s, keyval=%s, unicode=0x%X }",
         this, aCaller, ToChar(aKeyDownEventWasSent),
         GetCompositionStateName(), GetCurrentContext(), GetActiveContext(),
         aEvent, GetEventType(aEvent), gdk_keyval_name(aEvent->keyval),
         gdk_keyval_to_unicode(aEvent->keyval)));

    if (aCaller != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   OnKeyEvent(), FAILED, the caller isn't focused "
             "window, mLastFocusedWindow=%p",
             this, mLastFocusedWindow));
        return false;
    }

    // Even if old IM context has composition, key event should be sent to
    // current context since the user expects so.
    GtkIMContext* currentContext = GetCurrentContext();
    if (MOZ_UNLIKELY(!currentContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   OnKeyEvent(), FAILED, there are no context",
             this));
        return false;
    }

    if (mSetCursorPositionOnKeyEvent) {
        SetCursorPosition(currentContext);
        mSetCursorPositionOnKeyEvent = false;
    }

    mKeyDownEventWasSent = aKeyDownEventWasSent;
    mFilterKeyEvent = true;
    mProcessingKeyEvent = aEvent;
    gboolean isFiltered =
        gtk_im_context_filter_keypress(currentContext, aEvent);
    mProcessingKeyEvent = nullptr;

    // We filter the key event if the event was not committed (because
    // it's probably part of a composition) or if the key event was
    // committed _and_ changed.  This way we still let key press
    // events go through as simple key press events instead of
    // composed characters.
    bool filterThisEvent = isFiltered && mFilterKeyEvent;

    if (IsComposingOnCurrentContext() && !isFiltered) {
        if (aEvent->type == GDK_KEY_PRESS) {
            if (!mDispatchedCompositionString.IsEmpty()) {
                // If there is composition string, we shouldn't dispatch
                // any keydown events during composition.
                filterThisEvent = true;
            } else {
                // A Hangul input engine for SCIM doesn't emit preedit_end
                // signal even when composition string becomes empty.  On the
                // other hand, we should allow to make composition with empty
                // string for other languages because there *might* be such
                // IM.  For compromising this issue, we should dispatch
                // compositionend event, however, we don't need to reset IM
                // actually.
                DispatchCompositionCommitEvent(currentContext, &EmptyString());
                filterThisEvent = false;
            }
        } else {
            // Key release event may not be consumed by IM, however, we
            // shouldn't dispatch any keyup event during composition.
            filterThisEvent = true;
        }
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Debug,
        ("GTKIM: %p   OnKeyEvent(), succeeded, filterThisEvent=%s "
         "(isFiltered=%s, mFilterKeyEvent=%s), mCompositionState=%s",
         this, ToChar(filterThisEvent), ToChar(isFiltered),
         ToChar(mFilterKeyEvent), GetCompositionStateName()));

    return filterThisEvent;
}

void
IMContextWrapper::OnFocusChangeInGecko(bool aFocus)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p OnFocusChangeInGecko(aFocus=%s), "
         "mCompositionState=%s, mIsIMFocused=%s",
         this, ToChar(aFocus), GetCompositionStateName(),
         ToChar(mIsIMFocused)));

    // We shouldn't carry over the removed string to another editor.
    mSelectedString.Truncate();
    mSelection.Clear();
}

void
IMContextWrapper::ResetIME()
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p ResetIME(), mCompositionState=%s, mIsIMFocused=%s",
         this, GetCompositionStateName(), ToChar(mIsIMFocused)));

    GtkIMContext* activeContext = GetActiveContext();
    if (MOZ_UNLIKELY(!activeContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   ResetIME(), FAILED, there are no context",
             this));
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

    MOZ_LOG(gGtkIMLog, LogLevel::Debug,
        ("GTKIM: %p   ResetIME() called gtk_im_context_reset(), "
         "activeContext=%p, mCompositionState=%s, compositionString=%s, "
         "mIsIMFocused=%s",
         this, activeContext, GetCompositionStateName(),
         NS_ConvertUTF16toUTF8(compositionString).get(),
         ToChar(mIsIMFocused)));

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

nsresult
IMContextWrapper::EndIMEComposition(nsWindow* aCaller)
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return NS_OK;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p EndIMEComposition(aCaller=%p), "
         "mCompositionState=%s",
         this, aCaller, GetCompositionStateName()));

    if (aCaller != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   EndIMEComposition(), FAILED, the caller isn't "
             "focused window, mLastFocusedWindow=%p",
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

void
IMContextWrapper::OnLayoutChange()
{
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

void
IMContextWrapper::OnUpdateComposition()
{
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

void
IMContextWrapper::SetInputContext(nsWindow* aCaller,
                                  const InputContext* aContext,
                                  const InputContextAction* aAction)
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p SetInputContext(aCaller=%p, aContext={ mIMEState={ "
         "mEnabled=%s }, mHTMLInputType=%s })",
         this, aCaller, GetEnabledStateName(aContext->mIMEState.mEnabled),
         NS_ConvertUTF16toUTF8(aContext->mHTMLInputType).get()));

    if (aCaller != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   SetInputContext(), FAILED, "
             "the caller isn't focused window, mLastFocusedWindow=%p",
             this, mLastFocusedWindow));
        return;
    }

    if (!mContext) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   SetInputContext(), FAILED, "
             "there are no context",
             this));
        return;
    }


    if (sLastFocusedContext != this) {
        mInputContext = *aContext;
        MOZ_LOG(gGtkIMLog, LogLevel::Debug,
            ("GTKIM: %p   SetInputContext(), succeeded, "
             "but we're not active",
             this));
        return;
    }

    bool changingEnabledState =
        aContext->mIMEState.mEnabled != mInputContext.mIMEState.mEnabled ||
        aContext->mHTMLInputType != mInputContext.mHTMLInputType;

    // Release current IME focus if IME is enabled.
    if (changingEnabledState && mInputContext.mIMEState.MaybeEditable()) {
        EndIMEComposition(mLastFocusedWindow);
        Blur();
    }

    mInputContext = *aContext;

    if (changingEnabledState) {
#if (MOZ_WIDGET_GTK == 3)
        static bool sInputPurposeSupported = !gtk_check_version(3, 6, 0);
        if (sInputPurposeSupported && mInputContext.mIMEState.MaybeEditable()) {
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
                if (mInputContext.mIMEState.mEnabled == IMEState::PASSWORD) {
                    purpose = GTK_INPUT_PURPOSE_PASSWORD;
                } else if (inputType.EqualsLiteral("email")) {
                    purpose = GTK_INPUT_PURPOSE_EMAIL;
                } else if (inputType.EqualsLiteral("url")) {
                    purpose = GTK_INPUT_PURPOSE_URL;
                } else if (inputType.EqualsLiteral("tel")) {
                    purpose = GTK_INPUT_PURPOSE_PHONE;
                } else if (inputType.EqualsLiteral("number")) {
                    purpose = GTK_INPUT_PURPOSE_NUMBER;
                }

                g_object_set(currentContext, "input-purpose", purpose, nullptr);
            }
        }
#endif // #if (MOZ_WIDGET_GTK == 3)

        // Even when aState is not enabled state, we need to set IME focus.
        // Because some IMs are updating the status bar of them at this time.
        // Be aware, don't use aWindow here because this method shouldn't move
        // focus actually.
        Focus();

        // XXX Should we call Blur() when it's not editable?  E.g., it might be
        //     better to close VKB automatically.
    }
}

InputContext
IMContextWrapper::GetInputContext()
{
    mInputContext.mIMEState.mOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
    return mInputContext;
}

GtkIMContext*
IMContextWrapper::GetCurrentContext() const
{
    if (IsEnabled()) {
        return mContext;
    }
    if (mInputContext.mIMEState.mEnabled == IMEState::PASSWORD) {
        return mSimpleContext;
    }
    return mDummyContext;
}

bool
IMContextWrapper::IsValidContext(GtkIMContext* aContext) const
{
    if (!aContext) {
        return false;
    }
    return aContext == mContext ||
           aContext == mSimpleContext ||
           aContext == mDummyContext;
}

bool
IMContextWrapper::IsEnabled() const
{
    return mInputContext.mIMEState.mEnabled == IMEState::ENABLED ||
           mInputContext.mIMEState.mEnabled == IMEState::PLUGIN ||
           (!sUseSimpleContext &&
            mInputContext.mIMEState.mEnabled == IMEState::PASSWORD);
}

void
IMContextWrapper::Focus()
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p Focus(), sLastFocusedContext=%p",
         this, sLastFocusedContext));

    if (mIsIMFocused) {
        NS_ASSERTION(sLastFocusedContext == this,
                     "We're not active, but the IM was focused?");
        return;
    }

    GtkIMContext* currentContext = GetCurrentContext();
    if (!currentContext) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   Focus(), FAILED, there are no context",
             this));
        return;
    }

    if (sLastFocusedContext && sLastFocusedContext != this) {
        sLastFocusedContext->Blur();
    }

    sLastFocusedContext = this;

    gtk_im_context_focus_in(currentContext);
    mIsIMFocused = true;
    mSetCursorPositionOnKeyEvent = true;

    if (!IsEnabled()) {
        // We should release IME focus for uim and scim.
        // These IMs are using snooper that is released at losing focus.
        Blur();
    }
}

void
IMContextWrapper::Blur()
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p Blur(), mIsIMFocused=%s",
         this, ToChar(mIsIMFocused)));

    if (!mIsIMFocused) {
        return;
    }

    GtkIMContext* currentContext = GetCurrentContext();
    if (!currentContext) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   Blur(), FAILED, there are no context",
             this));
        return;
    }

    gtk_im_context_focus_out(currentContext);
    mIsIMFocused = false;
}

void
IMContextWrapper::OnSelectionChange(nsWindow* aCaller,
                                    const IMENotification& aIMENotification)
{
    mSelection.Assign(aIMENotification);

    if (MOZ_UNLIKELY(IsDestroyed())) {
        return;
    }

    const IMENotification::SelectionChangeDataBase& selectionChangeData =
        aIMENotification.mSelectionChangeData;

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p OnSelectionChange(aCaller=0x%p, aIMENotification={ "
         "mSelectionChangeData={ mOffset=%u, Length()=%u, mReversed=%s, "
         "mWritingMode=%s, mCausedByComposition=%s, "
         "mCausedBySelectionEvent=%s, mOccurredDuringComposition=%s "
         "} }), mCompositionState=%s, mIsDeletingSurrounding=%s",
         this, aCaller, selectionChangeData.mOffset,
         selectionChangeData.Length(),
         ToChar(selectionChangeData.mReversed),
         GetWritingModeName(selectionChangeData.GetWritingMode()).get(),
         ToChar(selectionChangeData.mCausedByComposition),
         ToChar(selectionChangeData.mCausedBySelectionEvent),
         ToChar(selectionChangeData.mOccurredDuringComposition),
         GetCompositionStateName(), ToChar(mIsDeletingSurrounding)));

    if (aCaller != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   OnSelectionChange(), FAILED, "
             "the caller isn't focused window, mLastFocusedWindow=%p",
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
                ("GTKIM: %p   OnSelectionChange(), FAILED, "
                 "new offset is too large, cannot keep composing",
                 this));
        } else {
            // Modify the selection start offset with new offset.
            mCompositionStart = mSelection.mOffset;
            // XXX We should modify mSelectedString? But how?
            MOZ_LOG(gGtkIMLog, LogLevel::Debug,
                ("GTKIM: %p   OnSelectionChange(), ignored, mCompositionStart "
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
      IsComposing() && !selectionChangeData.mOccurredDuringComposition;
    if (occurredBeforeComposition) {
        mPendingResettingIMContext = true;
    }

    // When the selection change is caused by dispatching composition event,
    // selection set event and/or occurred before starting current composition,
    // we shouldn't notify IME of that and commit existing composition.
    if (!selectionChangeData.mCausedByComposition &&
        !selectionChangeData.mCausedBySelectionEvent &&
        !occurredBeforeComposition) {
        ResetIME();
    }
}

/* static */
void
IMContextWrapper::OnStartCompositionCallback(GtkIMContext* aContext,
                                             IMContextWrapper* aModule)
{
    aModule->OnStartCompositionNative(aContext);
}

void
IMContextWrapper::OnStartCompositionNative(GtkIMContext* aContext)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p OnStartCompositionNative(aContext=%p), "
         "current context=%p",
         this, aContext, GetCurrentContext()));

    // See bug 472635, we should do nothing if IM context doesn't match.
    if (GetCurrentContext() != aContext) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   OnStartCompositionNative(), FAILED, "
             "given context doesn't match",
             this));
        return;
    }

    mComposingContext = static_cast<GtkIMContext*>(g_object_ref(aContext));

    if (!DispatchCompositionStart(aContext)) {
        return;
    }
    mCompositionTargetRange.mOffset = mCompositionStart;
    mCompositionTargetRange.mLength = 0;
}

/* static */
void
IMContextWrapper::OnEndCompositionCallback(GtkIMContext* aContext,
                                           IMContextWrapper* aModule)
{
    aModule->OnEndCompositionNative(aContext);
}

void
IMContextWrapper::OnEndCompositionNative(GtkIMContext* aContext)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p OnEndCompositionNative(aContext=%p)",
         this, aContext));

    // See bug 472635, we should do nothing if IM context doesn't match.
    // Note that if this is called after focus move, the context may different
    // from any our owning context.
    if (!IsValidContext(aContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p    OnEndCompositionNative(), FAILED, "
             "given context doesn't match with any context",
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
void
IMContextWrapper::OnChangeCompositionCallback(GtkIMContext* aContext,
                                              IMContextWrapper* aModule)
{
    aModule->OnChangeCompositionNative(aContext);
}

void
IMContextWrapper::OnChangeCompositionNative(GtkIMContext* aContext)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p OnChangeCompositionNative(aContext=%p)",
         this, aContext));

    // See bug 472635, we should do nothing if IM context doesn't match.
    // Note that if this is called after focus move, the context may different
    // from any our owning context.
    if (!IsValidContext(aContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   OnChangeCompositionNative(), FAILED, "
             "given context doesn't match with any context",
             this));
        return;
    }

    nsAutoString compositionString;
    GetCompositionString(aContext, compositionString);
    if (!IsComposing() && compositionString.IsEmpty()) {
        mDispatchedCompositionString.Truncate();
        return; // Don't start the composition with empty string.
    }

    // Be aware, widget can be gone
    DispatchCompositionChangeEvent(aContext, compositionString);
}

/* static */
gboolean
IMContextWrapper::OnRetrieveSurroundingCallback(GtkIMContext* aContext,
                                                IMContextWrapper* aModule)
{
    return aModule->OnRetrieveSurroundingNative(aContext);
}

gboolean
IMContextWrapper::OnRetrieveSurroundingNative(GtkIMContext* aContext)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p OnRetrieveSurroundingNative(aContext=%p), "
         "current context=%p",
         this, aContext, GetCurrentContext()));

    // See bug 472635, we should do nothing if IM context doesn't match.
    if (GetCurrentContext() != aContext) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   OnRetrieveSurroundingNative(), FAILED, "
             "given context doesn't match",
             this));
        return FALSE;
    }

    nsAutoString uniStr;
    uint32_t cursorPos;
    if (NS_FAILED(GetCurrentParagraph(uniStr, cursorPos))) {
        return FALSE;
    }

    NS_ConvertUTF16toUTF8 utf8Str(nsDependentSubstring(uniStr, 0, cursorPos));
    uint32_t cursorPosInUTF8 = utf8Str.Length();
    AppendUTF16toUTF8(nsDependentSubstring(uniStr, cursorPos), utf8Str);
    gtk_im_context_set_surrounding(aContext, utf8Str.get(), utf8Str.Length(),
                                   cursorPosInUTF8);
    return TRUE;
}

/* static */
gboolean
IMContextWrapper::OnDeleteSurroundingCallback(GtkIMContext* aContext,
                                              gint aOffset,
                                              gint aNChars,
                                              IMContextWrapper* aModule)
{
    return aModule->OnDeleteSurroundingNative(aContext, aOffset, aNChars);
}

gboolean
IMContextWrapper::OnDeleteSurroundingNative(GtkIMContext* aContext,
                                            gint aOffset,
                                            gint aNChars)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p OnDeleteSurroundingNative(aContext=%p, aOffset=%ld, "
         "aNChar=%ld), current context=%p",
         this, aContext, GetCurrentContext()));

    // See bug 472635, we should do nothing if IM context doesn't match.
    if (GetCurrentContext() != aContext) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   OnDeleteSurroundingNative(), FAILED, "
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
        ("GTKIM: %p   OnDeleteSurroundingNative(), FAILED, "
         "cannot delete text",
         this));
    return FALSE;
}
                         
/* static */
void
IMContextWrapper::OnCommitCompositionCallback(GtkIMContext* aContext,
                                              const gchar* aString,
                                              IMContextWrapper* aModule)
{
    aModule->OnCommitCompositionNative(aContext, aString);
}

void
IMContextWrapper::OnCommitCompositionNative(GtkIMContext* aContext,
                                            const gchar* aUTF8Char)
{
    const gchar emptyStr = 0;
    const gchar *commitString = aUTF8Char ? aUTF8Char : &emptyStr;

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p OnCommitCompositionNative(aContext=%p), "
         "current context=%p, active context=%p, commitString=\"%s\", "
         "mProcessingKeyEvent=%p, IsComposingOn(aContext)=%s",
         this, aContext, GetCurrentContext(), GetActiveContext(), commitString,
         mProcessingKeyEvent, ToChar(IsComposingOn(aContext))));

    // See bug 472635, we should do nothing if IM context doesn't match.
    if (!IsValidContext(aContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   OnCommitCompositionNative(), FAILED, "
             "given context doesn't match",
             this));
        return;
    }

    // If we are not in composition and committing with empty string,
    // we need to do nothing because if we continued to handle this
    // signal, we would dispatch compositionstart, text, compositionend
    // events with empty string.  Of course, they are unnecessary events
    // for Web applications and our editor.
    if (!IsComposingOn(aContext) && !commitString[0]) {
        return;
    }

    // If IME doesn't change their keyevent that generated this commit,
    // don't send it through XIM - just send it as a normal key press
    // event.
    // NOTE: While a key event is being handled, this might be caused on
    // current context.  Otherwise, this may be caused on active context.
    if (!IsComposingOn(aContext) && mProcessingKeyEvent &&
        aContext == GetCurrentContext()) {
        char keyval_utf8[8]; /* should have at least 6 bytes of space */
        gint keyval_utf8_len;
        guint32 keyval_unicode;

        keyval_unicode = gdk_keyval_to_unicode(mProcessingKeyEvent->keyval);
        keyval_utf8_len = g_unichar_to_utf8(keyval_unicode, keyval_utf8);
        keyval_utf8[keyval_utf8_len] = '\0';

        if (!strcmp(commitString, keyval_utf8)) {
            MOZ_LOG(gGtkIMLog, LogLevel::Info,
                ("GTKIM: %p   OnCommitCompositionNative(), "
                 "we'll send normal key event",
                 this));
            mFilterKeyEvent = false;
            return;
        }
    }

    NS_ConvertUTF8toUTF16 str(commitString);
    // Be aware, widget can be gone
    DispatchCompositionCommitEvent(aContext, &str);
}

void
IMContextWrapper::GetCompositionString(GtkIMContext* aContext,
                                       nsAString& aCompositionString)
{
    gchar *preedit_string;
    gint cursor_pos;
    PangoAttrList *feedback_list;
    gtk_im_context_get_preedit_string(aContext, &preedit_string,
                                      &feedback_list, &cursor_pos);
    if (preedit_string && *preedit_string) {
        CopyUTF8toUTF16(preedit_string, aCompositionString);
    } else {
        aCompositionString.Truncate();
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p GetCompositionString(aContext=%p), "
         "aCompositionString=\"%s\"",
         this, aContext, preedit_string));

    pango_attr_list_unref(feedback_list);
    g_free(preedit_string);
}

bool
IMContextWrapper::DispatchCompositionStart(GtkIMContext* aContext)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p DispatchCompositionStart(aContext=%p)",
         this, aContext));

    if (IsComposing()) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DispatchCompositionStart(), FAILED, "
             "we're already in composition",
             this));
        return true;
    }

    if (!mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DispatchCompositionStart(), FAILED, "
             "there are no focused window in this module",
             this));
        return false;
    }

    if (NS_WARN_IF(!EnsureToCacheSelection())) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DispatchCompositionStart(), FAILED, "
             "cannot query the selection offset",
             this));
        return false;
    }

    // XXX The composition start point might be changed by composition events
    //     even though we strongly hope it doesn't happen.
    //     Every composition event should have the start offset for the result
    //     because it may high cost if we query the offset every time.
    mCompositionStart = mSelection.mOffset;
    mDispatchedCompositionString.Truncate();

    if (mProcessingKeyEvent && !mKeyDownEventWasSent &&
        mProcessingKeyEvent->type == GDK_KEY_PRESS) {
        // If this composition is started by a native keydown event, we need to
        // dispatch our keydown event here (before composition start).
        nsCOMPtr<nsIWidget> kungFuDeathGrip = mLastFocusedWindow;
        bool isCancelled;
        mLastFocusedWindow->DispatchKeyDownEvent(mProcessingKeyEvent,
                                                 &isCancelled);
        MOZ_LOG(gGtkIMLog, LogLevel::Debug,
            ("GTKIM: %p   DispatchCompositionStart(), FAILED, keydown event "
             "is dispatched",
             this));
        if (static_cast<nsWindow*>(kungFuDeathGrip.get())->IsDestroyed() ||
            kungFuDeathGrip != mLastFocusedWindow) {
            MOZ_LOG(gGtkIMLog, LogLevel::Error,
                ("GTKIM: %p   DispatchCompositionStart(), FAILED, the focused "
                 "widget was destroyed/changed by keydown event",
                 this));
            return false;
        }
    }

    RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher();
    nsresult rv = dispatcher->BeginNativeInputTransaction();
    if (NS_WARN_IF(NS_FAILED(rv))) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DispatchCompositionStart(), FAILED, "
             "due to BeginNativeInputTransaction() failure",
             this));
        return false;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Debug,
        ("GTKIM: %p   DispatchCompositionStart(), dispatching "
         "compositionstart... (mCompositionStart=%u)",
         this, mCompositionStart));
    mCompositionState = eCompositionState_CompositionStartDispatched;
    nsEventStatus status;
    RefPtr<nsWindow> lastFocusedWindow = mLastFocusedWindow;
    dispatcher->StartComposition(status);
    if (lastFocusedWindow->IsDestroyed() ||
        lastFocusedWindow != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DispatchCompositionStart(), FAILED, the focused "
             "widget was destroyed/changed by compositionstart event",
             this));
        return false;
    }

    return true;
}

bool
IMContextWrapper::DispatchCompositionChangeEvent(
                      GtkIMContext* aContext,
                      const nsAString& aCompositionString)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p DispatchCompositionChangeEvent(aContext=%p)",
         this, aContext));

    if (!mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DispatchCompositionChangeEvent(), FAILED, "
             "there are no focused window in this module",
             this));
        return false;
    }

    if (!IsComposing()) {
        MOZ_LOG(gGtkIMLog, LogLevel::Debug,
            ("GTKIM: %p   DispatchCompositionChangeEvent(), the composition "
             "wasn't started, force starting...",
             this));
        nsCOMPtr<nsIWidget> kungFuDeathGrip = mLastFocusedWindow;
        if (!DispatchCompositionStart(aContext)) {
            return false;
        }
    }

    RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher();
    nsresult rv = dispatcher->BeginNativeInputTransaction();
    if (NS_WARN_IF(NS_FAILED(rv))) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DispatchCompositionChangeEvent(), FAILED, "
             "due to BeginNativeInputTransaction() failure",
             this));
        return false;
    }

    // Store the selected string which will be removed by following
    // compositionchange event.
    if (mCompositionState == eCompositionState_CompositionStartDispatched) {
        if (NS_WARN_IF(!EnsureToCacheSelection(&mSelectedString))) {
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
            ("GTKIM: %p   DispatchCompositionChangeEvent(), FAILED, "
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
    mCompositionTargetRange.mOffset = rangeArray->TargetClauseOffset();
    mCompositionTargetRange.mLength = rangeArray->TargetClauseLength();

    RefPtr<nsWindow> lastFocusedWindow(mLastFocusedWindow);
    nsEventStatus status;
    rv = dispatcher->FlushPendingComposition(status);
    if (NS_WARN_IF(NS_FAILED(rv))) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DispatchCompositionChangeEvent(), FAILED, "
             "due to FlushPendingComposition() failure",
             this));
        return false;
    }

    if (lastFocusedWindow->IsDestroyed() ||
        lastFocusedWindow != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DispatchCompositionChangeEvent(), FAILED, the "
             "focused widget was destroyed/changed by "
             "compositionchange event",
             this));
        return false;
    }
    return true;
}

bool
IMContextWrapper::DispatchCompositionCommitEvent(
                      GtkIMContext* aContext,
                      const nsAString* aCommitString)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p DispatchCompositionCommitEvent(aContext=%p, "
         "aCommitString=%p, (\"%s\"))",
         this, aContext, aCommitString,
         aCommitString ? NS_ConvertUTF16toUTF8(*aCommitString).get() : ""));

    if (!mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DispatchCompositionCommitEvent(), FAILED, "
             "there are no focused window in this module",
             this));
        return false;
    }

    if (!IsComposing()) {
        if (!aCommitString || aCommitString->IsEmpty()) {
            MOZ_LOG(gGtkIMLog, LogLevel::Error,
                ("GTKIM: %p   DispatchCompositionCommitEvent(), FAILED, "
                 "there is no composition and empty commit string",
                 this));
            return true;
        }
        MOZ_LOG(gGtkIMLog, LogLevel::Debug,
            ("GTKIM: %p   DispatchCompositionCommitEvent(), "
             "the composition wasn't started, force starting...",
             this));
        nsCOMPtr<nsIWidget> kungFuDeathGrip(mLastFocusedWindow);
        if (!DispatchCompositionStart(aContext)) {
            return false;
        }
    }

    RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher();
    nsresult rv = dispatcher->BeginNativeInputTransaction();
    if (NS_WARN_IF(NS_FAILED(rv))) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DispatchCompositionCommitEvent(), FAILED, "
             "due to BeginNativeInputTransaction() failure",
             this));
        return false;
    }

    RefPtr<nsWindow> lastFocusedWindow(mLastFocusedWindow);

    mCompositionState = eCompositionState_NotComposing;
    mCompositionStart = UINT32_MAX;
    mCompositionTargetRange.Clear();
    mDispatchedCompositionString.Truncate();

    nsEventStatus status;
    rv = dispatcher->CommitComposition(status, aCommitString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DispatchCompositionChangeEvent(), FAILED, "
             "due to CommitComposition() failure",
             this));
        return false;
    }

    if (lastFocusedWindow->IsDestroyed() ||
        lastFocusedWindow != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DispatchCompositionCommitEvent(), FAILED, "
             "the focused widget was destroyed/changed by "
             "compositioncommit event",
             this));
        return false;
    }

    return true;
}

already_AddRefed<TextRangeArray>
IMContextWrapper::CreateTextRangeArray(GtkIMContext* aContext,
                                       const nsAString& aCompositionString)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p CreateTextRangeArray(aContext=%p, "
         "aCompositionString=\"%s\" (Length()=%u))",
         this, aContext, NS_ConvertUTF16toUTF8(aCompositionString).get(),
         aCompositionString.Length()));

    RefPtr<TextRangeArray> textRangeArray = new TextRangeArray();

    gchar *preedit_string;
    gint cursor_pos_in_chars;
    PangoAttrList *feedback_list;
    gtk_im_context_get_preedit_string(aContext, &preedit_string,
                                      &feedback_list, &cursor_pos_in_chars);
    if (!preedit_string || !*preedit_string) {
        if (!aCompositionString.IsEmpty()) {
            MOZ_LOG(gGtkIMLog, LogLevel::Error,
                ("GTKIM: %p   CreateTextRangeArray(), FAILED, due to "
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
                ("GTKIM: %p   CreateTextRangeArray(), failed to get UTF-8 "
                 "string before the caret (cursor_pos_in_chars=%d)",
                 this, cursor_pos_in_chars));
        } else {
            glong caretOffset = 0;
            gunichar2* utf16StrBeforeCaret =
                g_utf8_to_utf16(preedit_string, charAfterCaret - preedit_string,
                                nullptr, &caretOffset, nullptr);
            if (NS_WARN_IF(!utf16StrBeforeCaret) ||
                NS_WARN_IF(caretOffset < 0)) {
                MOZ_LOG(gGtkIMLog, LogLevel::Warning,
                    ("GTKIM: %p   CreateTextRangeArray(), WARNING, failed to "
                     "convert to UTF-16 string before the caret "
                     "(cursor_pos_in_chars=%d, caretOffset=%d)",
                     this, cursor_pos_in_chars, caretOffset));
            } else {
                caretOffsetInUTF16 = static_cast<uint32_t>(caretOffset);
                uint32_t compositionStringLength = aCompositionString.Length();
                if (NS_WARN_IF(caretOffsetInUTF16 > compositionStringLength)) {
                    MOZ_LOG(gGtkIMLog, LogLevel::Warning,
                        ("GTKIM: %p   CreateTextRangeArray(), WARNING, "
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
            ("GTKIM: %p   CreateTextRangeArray(), FAILED, iterator couldn't "
             "be allocated",
             this));
        pango_attr_list_unref(feedback_list);
        g_free(preedit_string);
        return textRangeArray.forget();
    }

    do {
        TextRange range;
        if (!SetTextRange(iter, preedit_string, caretOffsetInUTF16, range)) {
            continue;
        }
        textRangeArray->AppendElement(range);
    } while (pango_attr_iterator_next(iter));

    TextRange range;
    range.mStartOffset = range.mEndOffset = caretOffsetInUTF16;
    range.mRangeType = TextRangeType::eCaret;
    textRangeArray->AppendElement(range);
    MOZ_LOG(gGtkIMLog, LogLevel::Debug,
        ("GTKIM: %p   CreateTextRangeArray(), mStartOffset=%u, "
         "mEndOffset=%u, mRangeType=%s",
         this, range.mStartOffset, range.mEndOffset,
         ToChar(range.mRangeType)));

    pango_attr_iterator_destroy(iter);
    pango_attr_list_unref(feedback_list);
    g_free(preedit_string);

    return textRangeArray.forget();
}

/* static */
nscolor
IMContextWrapper::ToNscolor(PangoAttrColor* aPangoAttrColor)
{
    PangoColor& pangoColor = aPangoAttrColor->color;
    uint8_t r = pangoColor.red / 0x100;
    uint8_t g = pangoColor.green / 0x100;
    uint8_t b = pangoColor.blue / 0x100;
    return NS_RGB(r, g, b);
}

bool
IMContextWrapper::SetTextRange(PangoAttrIterator* aPangoAttrIter,
                               const gchar* aUTF8CompositionString,
                               uint32_t aUTF16CaretOffset,
                               TextRange& aTextRange) const
{
    // Set the range offsets in UTF-16 string.
    gint utf8ClauseStart, utf8ClauseEnd;
    pango_attr_iterator_range(aPangoAttrIter, &utf8ClauseStart, &utf8ClauseEnd);
    if (utf8ClauseStart == utf8ClauseEnd) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   SetTextRange(), FAILED, due to collapsed range",
             this));
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
                ("GTKIM: %p   SetTextRange(), FAILED, due to g_utf8_to_utf16() "
                 "failure (retrieving previous string of current clause)",
                 this));
            return false;
        }

        aTextRange.mStartOffset = utf16PreviousClausesLength;
        g_free(utf16PreviousClausesString);
    }

    glong utf16CurrentClauseLength;
    gunichar2* utf16CurrentClauseString =
        g_utf8_to_utf16(aUTF8CompositionString + utf8ClauseStart,
                        utf8ClauseEnd - utf8ClauseStart,
                        nullptr, &utf16CurrentClauseLength, nullptr);

    if (NS_WARN_IF(!utf16CurrentClauseString)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   SetTextRange(), FAILED, due to g_utf8_to_utf16() "
             "failure (retrieving current clause)",
             this));
        return false;
    }

    aTextRange.mEndOffset = aTextRange.mStartOffset + utf16CurrentClauseLength;
    g_free(utf16CurrentClauseString);
    utf16CurrentClauseString = nullptr;

    // Set styles
    TextRangeStyle& style = aTextRange.mRangeStyle;

    // Underline
    PangoAttrInt* attrUnderline =
        reinterpret_cast<PangoAttrInt*>(
            pango_attr_iterator_get(aPangoAttrIter, PANGO_ATTR_UNDERLINE));
    if (attrUnderline) {
        switch (attrUnderline->value) {
            case PANGO_UNDERLINE_NONE:
                style.mLineStyle = TextRangeStyle::LINESTYLE_NONE;
                break;
            case PANGO_UNDERLINE_DOUBLE:
                style.mLineStyle = TextRangeStyle::LINESTYLE_DOUBLE;
                break;
            case PANGO_UNDERLINE_ERROR:
                style.mLineStyle = TextRangeStyle::LINESTYLE_WAVY;
                break;
            case PANGO_UNDERLINE_SINGLE:
            case PANGO_UNDERLINE_LOW:
                style.mLineStyle = TextRangeStyle::LINESTYLE_SOLID;
                break;
            default:
                MOZ_LOG(gGtkIMLog, LogLevel::Warning,
                    ("GTKIM: %p   SetTextRange(), retrieved unknown underline "
                     "style: %d",
                     this, attrUnderline->value));
                style.mLineStyle = TextRangeStyle::LINESTYLE_SOLID;
                break;
        }
        style.mDefinedStyles |= TextRangeStyle::DEFINED_LINESTYLE;

        // Underline color
        PangoAttrColor* attrUnderlineColor =
            reinterpret_cast<PangoAttrColor*>(
                pango_attr_iterator_get(aPangoAttrIter,
                                        PANGO_ATTR_UNDERLINE_COLOR));
        if (attrUnderlineColor) {
            style.mUnderlineColor = ToNscolor(attrUnderlineColor);
            style.mDefinedStyles |= TextRangeStyle::DEFINED_UNDERLINE_COLOR;
        }
    } else {
        style.mLineStyle = TextRangeStyle::LINESTYLE_NONE;
        style.mDefinedStyles |= TextRangeStyle::DEFINED_LINESTYLE;
    }

    // Don't set colors if they are not specified.  They should be computed by
    // textframe if only one of the colors are specified.

    // Foreground color (text color)
    PangoAttrColor* attrForeground =
        reinterpret_cast<PangoAttrColor*>(
            pango_attr_iterator_get(aPangoAttrIter, PANGO_ATTR_FOREGROUND));
    if (attrForeground) {
        style.mForegroundColor = ToNscolor(attrForeground);
        style.mDefinedStyles |= TextRangeStyle::DEFINED_FOREGROUND_COLOR;
    }

    // Background color
    PangoAttrColor* attrBackground =
        reinterpret_cast<PangoAttrColor*>(
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
     *      clause is NS_TEXTRANGE_SELECTEDCONVERTEDTEXT.
     *   2: If only attrUnderline is specified, we assumed the clause is
     *      NS_TEXTRANGE_CONVERTEDTEXT.
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

    if (!attrUnderline && !attrForeground && !attrBackground) {
        MOZ_LOG(gGtkIMLog, LogLevel::Warning,
            ("GTKIM: %p   SetTextRange(), FAILED, due to no attr, "
             "aTextRange= { mStartOffset=%u, mEndOffset=%u }",
             this, aTextRange.mStartOffset, aTextRange.mEndOffset));
        return false;
    }

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
        aTextRange.mRangeType = TextRangeType::NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
    }
    // Otherwise, we should assume that the clause is converted but not
    // selected.
    else {
        aTextRange.mRangeType = TextRangeType::NS_TEXTRANGE_CONVERTEDTEXT;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Debug,
        ("GTKIM: %p   SetTextRange(), succeeded, aTextRange= { "
         "mStartOffset=%u, mEndOffset=%u, mRangeType=%s, mRangeStyle=%s }",
         this, aTextRange.mStartOffset, aTextRange.mEndOffset,
         ToChar(aTextRange.mRangeType),
         GetTextRangeStyleText(aTextRange.mRangeStyle).get()));

    return true;
}

void
IMContextWrapper::SetCursorPosition(GtkIMContext* aContext)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p SetCursorPosition(aContext=%p), "
         "mCompositionTargetRange={ mOffset=%u, mLength=%u }"
         "mSelection={ mOffset=%u, mLength=%u, mWritingMode=%s }",
         this, aContext, mCompositionTargetRange.mOffset,
         mCompositionTargetRange.mLength,
         mSelection.mOffset, mSelection.mLength,
         GetWritingModeName(mSelection.mWritingMode).get()));

    bool useCaret = false;
    if (!mCompositionTargetRange.IsValid()) {
        if (!mSelection.IsValid()) {
            MOZ_LOG(gGtkIMLog, LogLevel::Error,
                ("GTKIM: %p   SetCursorPosition(), FAILED, "
                 "mCompositionTargetRange and mSelection are invalid",
                 this));
            return;
        }
        useCaret = true;
    }

    if (!mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   SetCursorPosition(), FAILED, due to no focused "
             "window",
             this));
        return;
    }

    if (MOZ_UNLIKELY(!aContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   SetCursorPosition(), FAILED, due to no context",
             this));
        return;
    }

    WidgetQueryContentEvent charRect(true,
                                     useCaret ? eQueryCaretRect :
                                                eQueryTextRect,
                                     mLastFocusedWindow);
    if (useCaret) {
        charRect.InitForQueryCaretRect(mSelection.mOffset);
    } else {
        if (mSelection.mWritingMode.IsVertical()) {
            // For preventing the candidate window to overlap the target
            // clause, we should set fake (typically, very tall) caret rect.
            uint32_t length = mCompositionTargetRange.mLength ?
                mCompositionTargetRange.mLength : 1;
            charRect.InitForQueryTextRect(mCompositionTargetRange.mOffset,
                                          length);
        } else {
            charRect.InitForQueryTextRect(mCompositionTargetRange.mOffset, 1);
        }
    }
    InitEvent(charRect);
    nsEventStatus status;
    mLastFocusedWindow->DispatchEvent(&charRect, status);
    if (!charRect.mSucceeded) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   SetCursorPosition(), FAILED, %s was failed",
             this, useCaret ? "eQueryCaretRect" : "eQueryTextRect"));
        return;
    }

    nsWindow* rootWindow =
        static_cast<nsWindow*>(mLastFocusedWindow->GetTopLevelWidget());

    // Get the position of the rootWindow in screen.
    LayoutDeviceIntPoint root = rootWindow->WidgetToScreenOffset();

    // Get the position of IM context owner window in screen.
    LayoutDeviceIntPoint owner = mOwnerWindow->WidgetToScreenOffset();

    // Compute the caret position in the IM owner window.
    LayoutDeviceIntRect rect = charRect.mReply.mRect + root - owner;
    rect.width = 0;
    GdkRectangle area = rootWindow->DevicePixelsToGdkRectRoundOut(rect);

    gtk_im_context_set_cursor_location(aContext, &area);
}

nsresult
IMContextWrapper::GetCurrentParagraph(nsAString& aText,
                                      uint32_t& aCursorPos)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p GetCurrentParagraph(), mCompositionState=%s",
         this, GetCompositionStateName()));

    if (!mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   GetCurrentParagraph(), FAILED, there are no "
             "focused window in this module",
             this));
        return NS_ERROR_NULL_POINTER;
    }

    nsEventStatus status;

    uint32_t selOffset = mCompositionStart;
    uint32_t selLength = mSelectedString.Length();

    // If focused editor doesn't have composition string, we should use
    // current selection.
    if (!EditorHasCompositionString()) {
        // Query cursor position & selection
        if (NS_WARN_IF(!EnsureToCacheSelection())) {
            MOZ_LOG(gGtkIMLog, LogLevel::Error,
                ("GTKIM: %p   GetCurrentParagraph(), FAILED, due to no "
                 "valid selection information",
                 this));
            return NS_ERROR_FAILURE;
        }

        selOffset = mSelection.mOffset;
        selLength = mSelection.mLength;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Debug,
        ("GTKIM: %p   GetCurrentParagraph(), selOffset=%u, selLength=%u",
         this, selOffset, selLength));

    // XXX nsString::Find and nsString::RFind take int32_t for offset, so,
    //     we cannot support this request when the current offset is larger
    //     than INT32_MAX.
    if (selOffset > INT32_MAX || selLength > INT32_MAX ||
        selOffset + selLength > INT32_MAX) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   GetCurrentParagraph(), FAILED, The selection is "
             "out of range",
             this));
        return NS_ERROR_FAILURE;
    }

    // Get all text contents of the focused editor
    WidgetQueryContentEvent queryTextContentEvent(true, eQueryTextContent,
                                                  mLastFocusedWindow);
    queryTextContentEvent.InitForQueryTextContent(0, UINT32_MAX);
    mLastFocusedWindow->DispatchEvent(&queryTextContentEvent, status);
    NS_ENSURE_TRUE(queryTextContentEvent.mSucceeded, NS_ERROR_FAILURE);

    nsAutoString textContent(queryTextContentEvent.mReply.mString);
    if (selOffset + selLength > textContent.Length()) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   GetCurrentParagraph(), FAILED, The selection is "
             "invalid, textContent.Length()=%u",
             this, textContent.Length()));
        return NS_ERROR_FAILURE;
    }

    // Remove composing string and restore the selected string because
    // GtkEntry doesn't remove selected string until committing, however,
    // our editor does it.  We should emulate the behavior for IME.
    if (EditorHasCompositionString() &&
        mDispatchedCompositionString != mSelectedString) {
        textContent.Replace(mCompositionStart,
            mDispatchedCompositionString.Length(), mSelectedString);
    }

    // Get only the focused paragraph, by looking for newlines
    int32_t parStart = (selOffset == 0) ? 0 :
        textContent.RFind("\n", false, selOffset - 1, -1) + 1;
    int32_t parEnd = textContent.Find("\n", false, selOffset + selLength, -1);
    if (parEnd < 0) {
        parEnd = textContent.Length();
    }
    aText = nsDependentSubstring(textContent, parStart, parEnd - parStart);
    aCursorPos = selOffset - uint32_t(parStart);

    MOZ_LOG(gGtkIMLog, LogLevel::Debug,
        ("GTKIM: %p   GetCurrentParagraph(), succeeded, aText=%s, "
         "aText.Length()=%u, aCursorPos=%u",
         this, NS_ConvertUTF16toUTF8(aText).get(),
         aText.Length(), aCursorPos));

    return NS_OK;
}

nsresult
IMContextWrapper::DeleteText(GtkIMContext* aContext,
                             int32_t aOffset,
                             uint32_t aNChars)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GTKIM: %p DeleteText(aContext=%p, aOffset=%d, aNChars=%d), "
         "mCompositionState=%s",
         this, aContext, aOffset, aNChars, GetCompositionStateName()));

    if (!mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DeleteText(), FAILED, there are no focused window "
             "in this module",
             this));
        return NS_ERROR_NULL_POINTER;
    }

    if (!aNChars) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DeleteText(), FAILED, aNChars must not be zero",
             this));
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
        if (!DispatchCompositionCommitEvent(aContext, &mSelectedString)) {
            MOZ_LOG(gGtkIMLog, LogLevel::Error,
                ("GTKIM: %p   DeleteText(), FAILED, quitting from DeletText",
                 this));
            return NS_ERROR_FAILURE;
        }
    } else {
        if (NS_WARN_IF(!EnsureToCacheSelection())) {
            MOZ_LOG(gGtkIMLog, LogLevel::Error,
                ("GTKIM: %p   DeleteText(), FAILED, due to no valid selection "
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
    NS_ENSURE_TRUE(queryTextContentEvent.mSucceeded, NS_ERROR_FAILURE);
    if (queryTextContentEvent.mReply.mString.IsEmpty()) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DeleteText(), FAILED, there is no contents",
             this));
        return NS_ERROR_FAILURE;
    }

    NS_ConvertUTF16toUTF8 utf8Str(
        nsDependentSubstring(queryTextContentEvent.mReply.mString,
                             0, selOffset));
    glong offsetInUTF8Characters =
        g_utf8_strlen(utf8Str.get(), utf8Str.Length()) + aOffset;
    if (offsetInUTF8Characters < 0) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DeleteText(), FAILED, aOffset is too small for "
             "current cursor pos (computed offset: %d)",
             this, offsetInUTF8Characters));
        return NS_ERROR_FAILURE;
    }

    AppendUTF16toUTF8(
        nsDependentSubstring(queryTextContentEvent.mReply.mString, selOffset),
        utf8Str);
    glong countOfCharactersInUTF8 =
        g_utf8_strlen(utf8Str.get(), utf8Str.Length());
    glong endInUTF8Characters =
        offsetInUTF8Characters + aNChars;
    if (countOfCharactersInUTF8 < endInUTF8Characters) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DeleteText(), FAILED, aNChars is too large for "
             "current contents (content length: %d, computed end offset: %d)",
             this, countOfCharactersInUTF8, endInUTF8Characters));
        return NS_ERROR_FAILURE;
    }

    gchar* charAtOffset =
        g_utf8_offset_to_pointer(utf8Str.get(), offsetInUTF8Characters);
    gchar* charAtEnd =
        g_utf8_offset_to_pointer(utf8Str.get(), endInUTF8Characters);

    // Set selection to delete
    WidgetSelectionEvent selectionEvent(true, eSetSelection,
                                        mLastFocusedWindow);

    nsDependentCSubstring utf8StrBeforeOffset(utf8Str, 0,
                                              charAtOffset - utf8Str.get());
    selectionEvent.mOffset =
        NS_ConvertUTF8toUTF16(utf8StrBeforeOffset).Length();

    nsDependentCSubstring utf8DeletingStr(utf8Str,
                                          utf8StrBeforeOffset.Length(),
                                          charAtEnd - charAtOffset);
    selectionEvent.mLength =
        NS_ConvertUTF8toUTF16(utf8DeletingStr).Length();

    selectionEvent.mReversed = false;
    selectionEvent.mExpandToClusterBoundary = false;
    lastFocusedWindow->DispatchEvent(&selectionEvent, status);

    if (!selectionEvent.mSucceeded ||
        lastFocusedWindow != mLastFocusedWindow ||
        lastFocusedWindow->Destroyed()) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DeleteText(), FAILED, setting selection caused "
             "focus change or window destroyed",
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
            ("GTKIM: %p   DeleteText(), FAILED, deleting the selection caused "
             "focus change or window destroyed",
             this));
        return NS_ERROR_FAILURE;
    }

    if (!wasComposing) {
        return NS_OK;
    }

    // Restore the composition at new caret position.
    if (!DispatchCompositionStart(aContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DeleteText(), FAILED, resterting composition start",
             this));
        return NS_ERROR_FAILURE;
    }

    if (!editorHadCompositionString) {
        return NS_OK;
    }

    nsAutoString compositionString;
    GetCompositionString(aContext, compositionString);
    if (!DispatchCompositionChangeEvent(aContext, compositionString)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
            ("GTKIM: %p   DeleteText(), FAILED, restoring composition string",
             this));
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

void
IMContextWrapper::InitEvent(WidgetGUIEvent& aEvent)
{
    aEvent.mTime = PR_Now() / 1000;
}

bool
IMContextWrapper::EnsureToCacheSelection(nsAString* aSelectedString)
{
    if (aSelectedString) {
        aSelectedString->Truncate();
    }

    if (mSelection.IsValid() &&
        (!mSelection.Collapsed() || !aSelectedString)) {
       return true;
    }

    if (NS_WARN_IF(!mLastFocusedWindow)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
                ("GTKIM: %p EnsureToCacheSelection(), FAILED, due to "
                 "no focused window",
                 this));
        return false;
    }

    nsEventStatus status;
    WidgetQueryContentEvent selection(true, eQuerySelectedText,
                                      mLastFocusedWindow);
    InitEvent(selection);
    mLastFocusedWindow->DispatchEvent(&selection, status);
    if (NS_WARN_IF(!selection.mSucceeded)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
                ("GTKIM: %p EnsureToCacheSelection(), FAILED, due to "
                 "failure of query selection event",
                 this));
        return false;
    }

    mSelection.Assign(selection);
    if (!mSelection.IsValid()) {
        MOZ_LOG(gGtkIMLog, LogLevel::Error,
                ("GTKIM: %p EnsureToCacheSelection(), FAILED, due to "
                 "failure of query selection event (invalid result)",
                 this));
        return false;
    }

    if (!mSelection.Collapsed() && aSelectedString) {
        aSelectedString->Assign(selection.mReply.mString);
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Debug,
            ("GTKIM: %p EnsureToCacheSelection(), Succeeded, mSelection="
             "{ mOffset=%u, mLength=%u, mWritingMode=%s }",
             this, mSelection.mOffset, mSelection.mLength,
             GetWritingModeName(mSelection.mWritingMode).get()));
    return true;
}

/******************************************************************************
 * IMContextWrapper::Selection
 ******************************************************************************/

void
IMContextWrapper::Selection::Assign(const IMENotification& aIMENotification)
{
    MOZ_ASSERT(aIMENotification.mMessage == NOTIFY_IME_OF_SELECTION_CHANGE);
    mOffset = aIMENotification.mSelectionChangeData.mOffset;
    mLength = aIMENotification.mSelectionChangeData.Length();
    mWritingMode = aIMENotification.mSelectionChangeData.GetWritingMode();
}

void
IMContextWrapper::Selection::Assign(const WidgetQueryContentEvent& aEvent)
{
    MOZ_ASSERT(aEvent.mMessage == eQuerySelectedText);
    MOZ_ASSERT(aEvent.mSucceeded);
    mOffset = aEvent.mReply.mOffset;
    mLength = aEvent.mReply.mString.Length();
    mWritingMode = aEvent.GetWritingMode();
}

} // namespace widget
} // namespace mozilla
