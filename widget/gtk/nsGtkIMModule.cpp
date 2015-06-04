/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "prtime.h"

#include "nsGtkIMModule.h"
#include "nsWindow.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Likely.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;
using namespace mozilla::widget;

PRLogModuleInfo* gGtkIMLog = nullptr;

static const char*
GetBoolName(bool aBool)
{
  return aBool ? "true" : "false";
}

static const char*
GetRangeTypeName(uint32_t aRangeType)
{
    switch (aRangeType) {
        case NS_TEXTRANGE_RAWINPUT:
            return "NS_TEXTRANGE_RAWINPUT";
        case NS_TEXTRANGE_CONVERTEDTEXT:
            return "NS_TEXTRANGE_CONVERTEDTEXT";
        case NS_TEXTRANGE_SELECTEDRAWTEXT:
            return "NS_TEXTRANGE_SELECTEDRAWTEXT";
        case NS_TEXTRANGE_SELECTEDCONVERTEDTEXT:
            return "NS_TEXTRANGE_SELECTEDCONVERTEDTEXT";
        case NS_TEXTRANGE_CARETPOSITION:
            return "NS_TEXTRANGE_CARETPOSITION";
        default:
            return "UNKNOWN SELECTION TYPE!!";
    }
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

const static bool kUseSimpleContextDefault = MOZ_WIDGET_GTK == 2;

nsGtkIMModule* nsGtkIMModule::sLastFocusedModule = nullptr;
bool nsGtkIMModule::sUseSimpleContext;

nsGtkIMModule::nsGtkIMModule(nsWindow* aOwnerWindow)
    : mOwnerWindow(aOwnerWindow)
    , mLastFocusedWindow(nullptr)
    , mContext(nullptr)
    , mSimpleContext(nullptr)
    , mDummyContext(nullptr)
    , mComposingContext(nullptr)
    , mCompositionStart(UINT32_MAX)
    , mProcessingKeyEvent(nullptr)
    , mCompositionTargetOffset(UINT32_MAX)
    , mCompositionState(eCompositionState_NotComposing)
    , mIsIMFocused(false)
    , mIsDeletingSurrounding(false)
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
nsGtkIMModule::Init()
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): Init, mOwnerWindow=%p",
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
                     G_CALLBACK(nsGtkIMModule::OnChangeCompositionCallback),
                     this);
    g_signal_connect(mContext, "retrieve_surrounding",
                     G_CALLBACK(nsGtkIMModule::OnRetrieveSurroundingCallback),
                     this);
    g_signal_connect(mContext, "delete_surrounding",
                     G_CALLBACK(nsGtkIMModule::OnDeleteSurroundingCallback),
                     this);
    g_signal_connect(mContext, "commit",
                     G_CALLBACK(nsGtkIMModule::OnCommitCompositionCallback),
                     this);
    g_signal_connect(mContext, "preedit_start",
                     G_CALLBACK(nsGtkIMModule::OnStartCompositionCallback),
                     this);
    g_signal_connect(mContext, "preedit_end",
                     G_CALLBACK(nsGtkIMModule::OnEndCompositionCallback),
                     this);

    // Simple context
    if (sUseSimpleContext) {
        mSimpleContext = gtk_im_context_simple_new();
        gtk_im_context_set_client_window(mSimpleContext, gdkWindow);
        g_signal_connect(mSimpleContext, "preedit_changed",
            G_CALLBACK(&nsGtkIMModule::OnChangeCompositionCallback),
            this);
        g_signal_connect(mSimpleContext, "retrieve_surrounding",
            G_CALLBACK(&nsGtkIMModule::OnRetrieveSurroundingCallback),
            this);
        g_signal_connect(mSimpleContext, "delete_surrounding",
            G_CALLBACK(&nsGtkIMModule::OnDeleteSurroundingCallback),
            this);
        g_signal_connect(mSimpleContext, "commit",
            G_CALLBACK(&nsGtkIMModule::OnCommitCompositionCallback),
            this);
        g_signal_connect(mSimpleContext, "preedit_start",
            G_CALLBACK(nsGtkIMModule::OnStartCompositionCallback),
            this);
        g_signal_connect(mSimpleContext, "preedit_end",
            G_CALLBACK(nsGtkIMModule::OnEndCompositionCallback),
            this);
    }

    // Dummy context
    mDummyContext = gtk_im_multicontext_new();
    gtk_im_context_set_client_window(mDummyContext, gdkWindow);
}

nsGtkIMModule::~nsGtkIMModule()
{
    if (this == sLastFocusedModule) {
        sLastFocusedModule = nullptr;
    }
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p) was gone", this));
}

void
nsGtkIMModule::OnDestroyWindow(nsWindow* aWindow)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): OnDestroyWindow, aWindow=%p, mLastFocusedWindow=%p, mOwnerWindow=%p, mLastFocusedModule=%p",
         this, aWindow, mLastFocusedWindow, mOwnerWindow, sLastFocusedModule));

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

    if (sLastFocusedModule == this) {
        sLastFocusedModule = nullptr;
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

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("    SUCCEEDED, Completely destroyed"));
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
nsGtkIMModule::PrepareToDestroyContext(GtkIMContext *aContext)
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
nsGtkIMModule::OnFocusWindow(nsWindow* aWindow)
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): OnFocusWindow, aWindow=%p, mLastFocusedWindow=%p",
         this, aWindow, mLastFocusedWindow));
    mLastFocusedWindow = aWindow;
    Focus();
}

void
nsGtkIMModule::OnBlurWindow(nsWindow* aWindow)
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): OnBlurWindow, aWindow=%p, mLastFocusedWindow=%p, "
         "mIsIMFocused=%s",
         this, aWindow, mLastFocusedWindow, GetBoolName(mIsIMFocused)));

    if (!mIsIMFocused || mLastFocusedWindow != aWindow) {
        return;
    }

    Blur();
}

bool
nsGtkIMModule::OnKeyEvent(nsWindow* aCaller, GdkEventKey* aEvent,
                          bool aKeyDownEventWasSent /* = false */)
{
    NS_PRECONDITION(aEvent, "aEvent must be non-null");

    if (!mInputContext.mIMEState.MaybeEditable() ||
        MOZ_UNLIKELY(IsDestroyed())) {
        return false;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): OnKeyEvent, aCaller=%p, aKeyDownEventWasSent=%s, "
         "mCompositionState=%s, current context=%p, active context=%p, "
         "aEvent(%p): { type=%s, keyval=%s, unicode=0x%X }",
         this, aCaller, GetBoolName(aKeyDownEventWasSent),
         GetCompositionStateName(), GetCurrentContext(), GetActiveContext(),
         aEvent, GetEventType(aEvent), gdk_keyval_name(aEvent->keyval),
         gdk_keyval_to_unicode(aEvent->keyval)));

    if (aCaller != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, the caller isn't focused window, mLastFocusedWindow=%p",
             mLastFocusedWindow));
        return false;
    }

    // Even if old IM context has composition, key event should be sent to
    // current context since the user expects so.
    GtkIMContext* currentContext = GetCurrentContext();
    if (MOZ_UNLIKELY(!currentContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, there are no context"));
        return false;
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

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("    filterThisEvent=%s (isFiltered=%s, mFilterKeyEvent=%s), "
         "mCompositionState=%s",
         GetBoolName(filterThisEvent), GetBoolName(isFiltered),
         GetBoolName(mFilterKeyEvent), GetCompositionStateName()));

    return filterThisEvent;
}

void
nsGtkIMModule::OnFocusChangeInGecko(bool aFocus)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): OnFocusChangeInGecko, aFocus=%s, "
         "mCompositionState=%s, mIsIMFocused=%s",
         this, GetBoolName(aFocus), GetCompositionStateName(),
         GetBoolName(mIsIMFocused)));

    // We shouldn't carry over the removed string to another editor.
    mSelectedString.Truncate();
}

void
nsGtkIMModule::ResetIME()
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): ResetIME, mCompositionState=%s, mIsIMFocused=%s",
         this, GetCompositionStateName(), GetBoolName(mIsIMFocused)));

    GtkIMContext* activeContext = GetActiveContext();
    if (MOZ_UNLIKELY(!activeContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, there are no context"));
        return;
    }

    nsRefPtr<nsGtkIMModule> kungFuDeathGrip(this);
    nsRefPtr<nsWindow> lastFocusedWindow(mLastFocusedWindow);

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

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): ResetIME() called gtk_im_context_reset(), "
         "activeContext=%p, mCompositionState=%s, compositionString=%s, "
         "mIsIMFocused=%s",
         this, activeContext, GetCompositionStateName(),
         NS_ConvertUTF16toUTF8(compositionString).get(),
         GetBoolName(mIsIMFocused)));

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
nsGtkIMModule::EndIMEComposition(nsWindow* aCaller)
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return NS_OK;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): EndIMEComposition, aCaller=%p, "
         "mCompositionState=%s",
         this, aCaller, GetCompositionStateName()));

    if (aCaller != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    WARNING: the caller isn't focused window, mLastFocusedWindow=%p",
             mLastFocusedWindow));
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
nsGtkIMModule::OnUpdateComposition()
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return;
    }

    SetCursorPosition(GetActiveContext(), mCompositionTargetOffset);
}

void
nsGtkIMModule::SetInputContext(nsWindow* aCaller,
                               const InputContext* aContext,
                               const InputContextAction* aAction)
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): SetInputContext, aCaller=%p, aState=%s mHTMLInputType=%s",
         this, aCaller, GetEnabledStateName(aContext->mIMEState.mEnabled),
         NS_ConvertUTF16toUTF8(aContext->mHTMLInputType).get()));

    if (aCaller != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, the caller isn't focused window, mLastFocusedWindow=%p",
             mLastFocusedWindow));
        return;
    }

    if (!mContext) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, there are no context"));
        return;
    }


    if (sLastFocusedModule != this) {
        mInputContext = *aContext;
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    SUCCEEDED, but we're not active"));
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
nsGtkIMModule::GetInputContext()
{
    mInputContext.mIMEState.mOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
    return mInputContext;
}

GtkIMContext*
nsGtkIMModule::GetCurrentContext() const
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
nsGtkIMModule::IsValidContext(GtkIMContext* aContext) const
{
    if (!aContext) {
        return false;
    }
    return aContext == mContext ||
           aContext == mSimpleContext ||
           aContext == mDummyContext;
}

bool
nsGtkIMModule::IsEnabled() const
{
    return mInputContext.mIMEState.mEnabled == IMEState::ENABLED ||
           mInputContext.mIMEState.mEnabled == IMEState::PLUGIN ||
           (!sUseSimpleContext &&
            mInputContext.mIMEState.mEnabled == IMEState::PASSWORD);
}

void
nsGtkIMModule::Focus()
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): Focus, sLastFocusedModule=%p",
         this, sLastFocusedModule));

    if (mIsIMFocused) {
        NS_ASSERTION(sLastFocusedModule == this,
                     "We're not active, but the IM was focused?");
        return;
    }

    GtkIMContext* currentContext = GetCurrentContext();
    if (!currentContext) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, there are no context"));
        return;
    }

    if (sLastFocusedModule && sLastFocusedModule != this) {
        sLastFocusedModule->Blur();
    }

    sLastFocusedModule = this;

    gtk_im_context_focus_in(currentContext);
    mIsIMFocused = true;

    if (!IsEnabled()) {
        // We should release IME focus for uim and scim.
        // These IMs are using snooper that is released at losing focus.
        Blur();
    }
}

void
nsGtkIMModule::Blur()
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): Blur, mIsIMFocused=%s",
         this, GetBoolName(mIsIMFocused)));

    if (!mIsIMFocused) {
        return;
    }

    GtkIMContext* currentContext = GetCurrentContext();
    if (!currentContext) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, there are no context"));
        return;
    }

    gtk_im_context_focus_out(currentContext);
    mIsIMFocused = false;
}

void
nsGtkIMModule::OnSelectionChange(nsWindow* aCaller)
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return;
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): OnSelectionChange(aCaller=0x%p), "
         "mCompositionState=%s, mIsDeletingSurrounding=%s",
         this, aCaller, GetCompositionStateName(),
         mIsDeletingSurrounding ? "true" : "false"));

    if (aCaller != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    WARNING: the caller isn't focused window, "
             "mLastFocusedWindow=%p",
             mLastFocusedWindow));
        return;
    }

    // The focused editor might have placeholder text with normal text node.
    // In such case, the text node must be removed from a compositionstart
    // event handler.  So, we're dispatching NS_COMPOSITION_START,
    // we should ignore selection change notification.
    if (mCompositionState == eCompositionState_CompositionStartDispatched) {
        nsCOMPtr<nsIWidget> focusedWindow(mLastFocusedWindow);
        nsEventStatus status;
        WidgetQueryContentEvent selection(true, NS_QUERY_SELECTED_TEXT,
                                          focusedWindow);
        InitEvent(selection);
        mLastFocusedWindow->DispatchEvent(&selection, status);

        bool cannotContinueComposition = false;
        if (MOZ_UNLIKELY(IsDestroyed())) {
            MOZ_LOG(gGtkIMLog, LogLevel::Info,
                ("    ERROR: nsGtkIMModule instance is destroyed during "
                 "querying selection offset"));
            return;
        } else if (NS_WARN_IF(!selection.mSucceeded)) {
            cannotContinueComposition = true;
            MOZ_LOG(gGtkIMLog, LogLevel::Info,
                ("    ERROR: failed to retrieve new caret offset"));
        } else if (selection.mReply.mOffset == UINT32_MAX) {
            cannotContinueComposition = true;
            MOZ_LOG(gGtkIMLog, LogLevel::Info,
                ("    ERROR: new offset is too large, cannot keep composing"));
        } else if (!mLastFocusedWindow || focusedWindow != mLastFocusedWindow) {
            cannotContinueComposition = true;
            MOZ_LOG(gGtkIMLog, LogLevel::Info,
                ("    ERROR: focus is changed during querying selection "
                 "offset"));
        } else if (focusedWindow->Destroyed()) {
            cannotContinueComposition = true;
            MOZ_LOG(gGtkIMLog, LogLevel::Info,
                ("    ERROR: focused window started to be being destroyed "
                 "during querying selection offset"));
        }

        if (!cannotContinueComposition) {
            // Modify the selection start offset with new offset.
            mCompositionStart = selection.mReply.mOffset;
            MOZ_LOG(gGtkIMLog, LogLevel::Info,
                ("    NOTE: mCompositionStart is updated to %u, "
                 "the selection change doesn't cause resetting IM context",
                 mCompositionStart));
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

    ResetIME();
}

/* static */
void
nsGtkIMModule::OnStartCompositionCallback(GtkIMContext *aContext,
                                          nsGtkIMModule* aModule)
{
    aModule->OnStartCompositionNative(aContext);
}

void
nsGtkIMModule::OnStartCompositionNative(GtkIMContext *aContext)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): OnStartCompositionNative, aContext=%p, "
         "current context=%p",
         this, aContext, GetCurrentContext()));

    // See bug 472635, we should do nothing if IM context doesn't match.
    if (GetCurrentContext() != aContext) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, given context doesn't match"));
        return;
    }

    mComposingContext = static_cast<GtkIMContext*>(g_object_ref(aContext));

    if (!DispatchCompositionStart(aContext)) {
        return;
    }
    mCompositionTargetOffset = mCompositionStart;
}

/* static */
void
nsGtkIMModule::OnEndCompositionCallback(GtkIMContext *aContext,
                                        nsGtkIMModule* aModule)
{
    aModule->OnEndCompositionNative(aContext);
}

void
nsGtkIMModule::OnEndCompositionNative(GtkIMContext *aContext)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): OnEndCompositionNative, aContext=%p",
         this, aContext));

    // See bug 472635, we should do nothing if IM context doesn't match.
    // Note that if this is called after focus move, the context may different
    // from any our owning context.
    if (!IsValidContext(aContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, given context doesn't match with any context"));
        return;
    }

    g_object_unref(mComposingContext);
    mComposingContext = nullptr;

    if (!IsComposing()) {
        // If we already handled the commit event, we should do nothing here.
        return;
    }

    // Be aware, widget can be gone
    DispatchCompositionCommitEvent(aContext);
}

/* static */
void
nsGtkIMModule::OnChangeCompositionCallback(GtkIMContext *aContext,
                                           nsGtkIMModule* aModule)
{
    aModule->OnChangeCompositionNative(aContext);
}

void
nsGtkIMModule::OnChangeCompositionNative(GtkIMContext *aContext)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): OnChangeCompositionNative, aContext=%p",
         this, aContext));

    // See bug 472635, we should do nothing if IM context doesn't match.
    // Note that if this is called after focus move, the context may different
    // from any our owning context.
    if (!IsValidContext(aContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, given context doesn't match with any context"));
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
nsGtkIMModule::OnRetrieveSurroundingCallback(GtkIMContext  *aContext,
                                             nsGtkIMModule *aModule)
{
    return aModule->OnRetrieveSurroundingNative(aContext);
}

gboolean
nsGtkIMModule::OnRetrieveSurroundingNative(GtkIMContext *aContext)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): OnRetrieveSurroundingNative, aContext=%p, "
         "current context=%p",
         this, aContext, GetCurrentContext()));

    // See bug 472635, we should do nothing if IM context doesn't match.
    if (GetCurrentContext() != aContext) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, given context doesn't match"));
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
nsGtkIMModule::OnDeleteSurroundingCallback(GtkIMContext  *aContext,
                                           gint           aOffset,
                                           gint           aNChars,
                                           nsGtkIMModule *aModule)
{
    return aModule->OnDeleteSurroundingNative(aContext, aOffset, aNChars);
}

gboolean
nsGtkIMModule::OnDeleteSurroundingNative(GtkIMContext  *aContext,
                                         gint           aOffset,
                                         gint           aNChars)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): OnDeleteSurroundingNative, aContext=%p, "
         "current context=%p",
         this, aContext, GetCurrentContext()));

    // See bug 472635, we should do nothing if IM context doesn't match.
    if (GetCurrentContext() != aContext) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, given context doesn't match"));
        return FALSE;
    }

    AutoRestore<bool> saveDeletingSurrounding(mIsDeletingSurrounding);
    mIsDeletingSurrounding = true;
    if (NS_SUCCEEDED(DeleteText(aContext, aOffset, (uint32_t)aNChars))) {
        return TRUE;
    }

    // failed
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("    FAILED, cannot delete text"));
    return FALSE;
}
                         
/* static */
void
nsGtkIMModule::OnCommitCompositionCallback(GtkIMContext *aContext,
                                           const gchar *aString,
                                           nsGtkIMModule* aModule)
{
    aModule->OnCommitCompositionNative(aContext, aString);
}

void
nsGtkIMModule::OnCommitCompositionNative(GtkIMContext *aContext,
                                         const gchar *aUTF8Char)
{
    const gchar emptyStr = 0;
    const gchar *commitString = aUTF8Char ? aUTF8Char : &emptyStr;

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): OnCommitCompositionNative, aContext=%p, "
         "current context=%p, active context=%p, commitString=\"%s\", "
         "mProcessingKeyEvent=%p, IsComposingOn(aContext)=%s",
         this, aContext, GetCurrentContext(), GetActiveContext(), commitString,
         mProcessingKeyEvent, GetBoolName(IsComposingOn(aContext))));

    // See bug 472635, we should do nothing if IM context doesn't match.
    if (!IsValidContext(aContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, given context doesn't match"));
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
                ("GtkIMModule(%p): OnCommitCompositionNative, we'll send normal key event",
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
nsGtkIMModule::GetCompositionString(GtkIMContext* aContext,
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
        ("GtkIMModule(%p): GetCompositionString, result=\"%s\"",
         this, preedit_string));

    pango_attr_list_unref(feedback_list);
    g_free(preedit_string);
}

bool
nsGtkIMModule::DispatchCompositionStart(GtkIMContext* aContext)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): DispatchCompositionStart, aContext=%p",
         this, aContext));

    if (IsComposing()) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    WARNING, we're already in composition"));
        return true;
    }

    if (!mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, there are no focused window in this module"));
        return false;
    }

    nsEventStatus status;
    WidgetQueryContentEvent selection(true, NS_QUERY_SELECTED_TEXT,
                                      mLastFocusedWindow);
    InitEvent(selection);
    mLastFocusedWindow->DispatchEvent(&selection, status);

    if (!selection.mSucceeded || selection.mReply.mOffset == UINT32_MAX) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, cannot query the selection offset"));
        return false;
    }

    // XXX The composition start point might be changed by composition events
    //     even though we strongly hope it doesn't happen.
    //     Every composition event should have the start offset for the result
    //     because it may high cost if we query the offset every time.
    mCompositionStart = selection.mReply.mOffset;
    mDispatchedCompositionString.Truncate();

    if (mProcessingKeyEvent && !mKeyDownEventWasSent &&
        mProcessingKeyEvent->type == GDK_KEY_PRESS) {
        // If this composition is started by a native keydown event, we need to
        // dispatch our keydown event here (before composition start).
        nsCOMPtr<nsIWidget> kungFuDeathGrip = mLastFocusedWindow;
        bool isCancelled;
        mLastFocusedWindow->DispatchKeyDownEvent(mProcessingKeyEvent,
                                                 &isCancelled);
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    keydown event is dispatched"));
        if (static_cast<nsWindow*>(kungFuDeathGrip.get())->IsDestroyed() ||
            kungFuDeathGrip != mLastFocusedWindow) {
            MOZ_LOG(gGtkIMLog, LogLevel::Info,
                ("    NOTE, the focused widget was destroyed/changed by keydown event"));
            return false;
        }
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("    mCompositionStart=%u", mCompositionStart));
    mCompositionState = eCompositionState_CompositionStartDispatched;
    WidgetCompositionEvent compEvent(true, NS_COMPOSITION_START,
                                     mLastFocusedWindow);
    InitEvent(compEvent);
    nsCOMPtr<nsIWidget> kungFuDeathGrip = mLastFocusedWindow;
    mLastFocusedWindow->DispatchEvent(&compEvent, status);
    if (static_cast<nsWindow*>(kungFuDeathGrip.get())->IsDestroyed() ||
        kungFuDeathGrip != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    NOTE, the focused widget was destroyed/changed by compositionstart event"));
        return false;
    }

    return true;
}

bool
nsGtkIMModule::DispatchCompositionChangeEvent(
                   GtkIMContext* aContext,
                   const nsAString& aCompositionString)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): DispatchCompositionChangeEvent, aContext=%p",
         this, aContext));

    if (!mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, there are no focused window in this module"));
        return false;
    }

    if (!IsComposing()) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    The composition wasn't started, force starting..."));
        nsCOMPtr<nsIWidget> kungFuDeathGrip = mLastFocusedWindow;
        if (!DispatchCompositionStart(aContext)) {
            return false;
        }
    }

    nsEventStatus status;
    nsRefPtr<nsWindow> lastFocusedWindow = mLastFocusedWindow;

    // Store the selected string which will be removed by following
    // compositionchange event.
    if (mCompositionState == eCompositionState_CompositionStartDispatched) {
        // XXX We should assume, for now, any web applications don't change
        //     selection at handling this compositionchange event.
        WidgetQueryContentEvent querySelectedTextEvent(true,
                                                       NS_QUERY_SELECTED_TEXT,
                                                       mLastFocusedWindow);
        mLastFocusedWindow->DispatchEvent(&querySelectedTextEvent, status);
        if (querySelectedTextEvent.mSucceeded) {
            mSelectedString = querySelectedTextEvent.mReply.mString;
            mCompositionStart = querySelectedTextEvent.mReply.mOffset;
        }
    }

    WidgetCompositionEvent compositionChangeEvent(true, NS_COMPOSITION_CHANGE,
                                                  mLastFocusedWindow);
    InitEvent(compositionChangeEvent);

    uint32_t targetOffset = mCompositionStart;

    compositionChangeEvent.mData =
      mDispatchedCompositionString = aCompositionString;

    compositionChangeEvent.mRanges =
      CreateTextRangeArray(aContext, mDispatchedCompositionString);
    targetOffset += compositionChangeEvent.mRanges->TargetClauseOffset();

    mCompositionState = eCompositionState_CompositionChangeEventDispatched;

    mLastFocusedWindow->DispatchEvent(&compositionChangeEvent, status);
    if (lastFocusedWindow->IsDestroyed() ||
        lastFocusedWindow != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    NOTE, the focused widget was destroyed/changed by "
             "compositionchange event"));
        return false;
    }

    // We cannot call SetCursorPosition for e10s-aware.
    // DispatchEvent is async on e10s, so composition rect isn't updated now
    // on tab parent.
    mCompositionTargetOffset = targetOffset;

    return true;
}

bool
nsGtkIMModule::DispatchCompositionCommitEvent(
                   GtkIMContext* aContext,
                   const nsAString* aCommitString)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): DispatchCompositionCommitEvent, aContext=%p, "
         "aCommitString=%p, (\"%s\")",
         this, aContext, aCommitString,
         aCommitString ? NS_ConvertUTF16toUTF8(*aCommitString).get() : ""));

    if (!mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, there are no focused window in this module"));
        return false;
    }

    if (!IsComposing()) {
        if (!aCommitString || aCommitString->IsEmpty()) {
            MOZ_LOG(gGtkIMLog, LogLevel::Info,
                ("    FAILED, there is no composition and empty commit "
                 "string"));
            return true;
        }
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    The composition wasn't started, force starting..."));
        nsCOMPtr<nsIWidget> kungFuDeathGrip(mLastFocusedWindow);
        if (!DispatchCompositionStart(aContext)) {
            return false;
        }
    }

    nsRefPtr<nsWindow> lastFocusedWindow(mLastFocusedWindow);

    uint32_t message = aCommitString ? NS_COMPOSITION_COMMIT :
                                       NS_COMPOSITION_COMMIT_AS_IS;
    mCompositionState = eCompositionState_NotComposing;
    mCompositionStart = UINT32_MAX;
    mCompositionTargetOffset = UINT32_MAX;
    mDispatchedCompositionString.Truncate();

    WidgetCompositionEvent compositionCommitEvent(true, message,
                                                  mLastFocusedWindow);
    InitEvent(compositionCommitEvent);
    if (message == NS_COMPOSITION_COMMIT) {
        compositionCommitEvent.mData = *aCommitString;
    }

    nsEventStatus status = nsEventStatus_eIgnore;
    mLastFocusedWindow->DispatchEvent(&compositionCommitEvent, status);

    if (lastFocusedWindow->IsDestroyed() ||
        lastFocusedWindow != mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    NOTE, the focused widget was destroyed/changed by "
             "compositioncommit event"));
        return false;
    }

    return true;
}

already_AddRefed<TextRangeArray>
nsGtkIMModule::CreateTextRangeArray(GtkIMContext* aContext,
                                    const nsAString& aLastDispatchedData)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): CreateTextRangeArray, aContext=%p, "
         "aLastDispatchedData=\"%s\" (length=%u)",
         this, aContext, NS_ConvertUTF16toUTF8(aLastDispatchedData).get(),
         aLastDispatchedData.Length()));

    nsRefPtr<TextRangeArray> textRangeArray = new TextRangeArray();

    gchar *preedit_string;
    gint cursor_pos;
    PangoAttrList *feedback_list;
    gtk_im_context_get_preedit_string(aContext, &preedit_string,
                                      &feedback_list, &cursor_pos);
    if (!preedit_string || !*preedit_string) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    preedit_string is null"));
        pango_attr_list_unref(feedback_list);
        g_free(preedit_string);
        return textRangeArray.forget();
    }

    PangoAttrIterator* iter;
    iter = pango_attr_list_get_iterator(feedback_list);
    if (!iter) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, iterator couldn't be allocated"));
        pango_attr_list_unref(feedback_list);
        g_free(preedit_string);
        return textRangeArray.forget();
    }

    /*
     * Depend on gtk2's implementation on XIM support.
     * In aFeedback got from gtk2, there are only three types of data:
     * PANGO_ATTR_UNDERLINE, PANGO_ATTR_FOREGROUND, PANGO_ATTR_BACKGROUND.
     * Corresponding to XIMUnderline, XIMReverse.
     * Don't take PANGO_ATTR_BACKGROUND into account, since
     * PANGO_ATTR_BACKGROUND and PANGO_ATTR_FOREGROUND are always
     * a couple.
     */
    do {
        PangoAttribute* attrUnderline =
            pango_attr_iterator_get(iter, PANGO_ATTR_UNDERLINE);
        PangoAttribute* attrForeground =
            pango_attr_iterator_get(iter, PANGO_ATTR_FOREGROUND);
        if (!attrUnderline && !attrForeground) {
            continue;
        }

        // Get the range of the current attribute(s)
        gint start, end;
        pango_attr_iterator_range(iter, &start, &end);

        TextRange range;
        // XIMReverse | XIMUnderline
        if (attrUnderline && attrForeground) {
            range.mRangeType = NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
        }
        // XIMUnderline
        else if (attrUnderline) {
            range.mRangeType = NS_TEXTRANGE_CONVERTEDTEXT;
        }
        // XIMReverse
        else if (attrForeground) {
            range.mRangeType = NS_TEXTRANGE_SELECTEDRAWTEXT;
        } else {
            range.mRangeType = NS_TEXTRANGE_RAWINPUT;
        }

        gunichar2* uniStr = nullptr;
        if (start == 0) {
            range.mStartOffset = 0;
        } else {
            glong uniStrLen;
            uniStr = g_utf8_to_utf16(preedit_string, start,
                                     nullptr, &uniStrLen, nullptr);
            if (uniStr) {
                range.mStartOffset = uniStrLen;
                g_free(uniStr);
                uniStr = nullptr;
            }
        }

        glong uniStrLen;
        uniStr = g_utf8_to_utf16(preedit_string + start, end - start,
                                 nullptr, &uniStrLen, nullptr);
        if (!uniStr) {
            range.mEndOffset = range.mStartOffset;
        } else {
            range.mEndOffset = range.mStartOffset + uniStrLen;
            g_free(uniStr);
            uniStr = nullptr;
        }

        textRangeArray->AppendElement(range);

        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    mStartOffset=%u, mEndOffset=%u, mRangeType=%s",
             range.mStartOffset, range.mEndOffset,
             GetRangeTypeName(range.mRangeType)));
    } while (pango_attr_iterator_next(iter));

    TextRange range;
    if (cursor_pos < 0) {
        range.mStartOffset = 0;
    } else if (uint32_t(cursor_pos) > aLastDispatchedData.Length()) {
        range.mStartOffset = aLastDispatchedData.Length();
    } else {
        range.mStartOffset = uint32_t(cursor_pos);
    }
    range.mEndOffset = range.mStartOffset;
    range.mRangeType = NS_TEXTRANGE_CARETPOSITION;
    textRangeArray->AppendElement(range);

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("    mStartOffset=%u, mEndOffset=%u, mRangeType=%s",
         range.mStartOffset, range.mEndOffset,
         GetRangeTypeName(range.mRangeType)));

    pango_attr_iterator_destroy(iter);
    pango_attr_list_unref(feedback_list);
    g_free(preedit_string);

    return textRangeArray.forget();
}

void
nsGtkIMModule::SetCursorPosition(GtkIMContext* aContext,
                                 uint32_t aTargetOffset)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): SetCursorPosition, aContext=%p, aTargetOffset=%u",
         this, aContext, aTargetOffset));

    if (aTargetOffset == UINT32_MAX) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, aTargetOffset is wrong offset"));
        return;
    }

    if (!mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, there are no focused window"));
        return;
    }

    if (MOZ_UNLIKELY(!aContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, there are no context"));
        return;
    }

    WidgetQueryContentEvent charRect(true, NS_QUERY_TEXT_RECT,
                                     mLastFocusedWindow);
    charRect.InitForQueryTextRect(aTargetOffset, 1);
    InitEvent(charRect);
    nsEventStatus status;
    mLastFocusedWindow->DispatchEvent(&charRect, status);
    if (!charRect.mSucceeded) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, NS_QUERY_TEXT_RECT was failed"));
        return;
    }
    nsWindow* rootWindow =
        static_cast<nsWindow*>(mLastFocusedWindow->GetTopLevelWidget());

    // Get the position of the rootWindow in screen.
    gint rootX, rootY;
    gdk_window_get_origin(rootWindow->GetGdkWindow(), &rootX, &rootY);

    // Get the position of IM context owner window in screen.
    gint ownerX, ownerY;
    gdk_window_get_origin(mOwnerWindow->GetGdkWindow(), &ownerX, &ownerY);

    // Compute the caret position in the IM owner window.
    GdkRectangle area;
    area.x = charRect.mReply.mRect.x + rootX - ownerX;
    area.y = charRect.mReply.mRect.y + rootY - ownerY;
    area.width  = 0;
    area.height = charRect.mReply.mRect.height;

    gtk_im_context_set_cursor_location(aContext, &area);
}

nsresult
nsGtkIMModule::GetCurrentParagraph(nsAString& aText, uint32_t& aCursorPos)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): GetCurrentParagraph, mCompositionState=%s",
         this, GetCompositionStateName()));

    if (!mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, there are no focused window in this module"));
        return NS_ERROR_NULL_POINTER;
    }

    nsEventStatus status;

    uint32_t selOffset = mCompositionStart;
    uint32_t selLength = mSelectedString.Length();

    // If focused editor doesn't have composition string, we should use
    // current selection.
    if (!EditorHasCompositionString()) {
        // Query cursor position & selection
        WidgetQueryContentEvent querySelectedTextEvent(true,
                                                       NS_QUERY_SELECTED_TEXT,
                                                       mLastFocusedWindow);
        mLastFocusedWindow->DispatchEvent(&querySelectedTextEvent, status);
        NS_ENSURE_TRUE(querySelectedTextEvent.mSucceeded, NS_ERROR_FAILURE);

        selOffset = querySelectedTextEvent.mReply.mOffset;
        selLength = querySelectedTextEvent.mReply.mString.Length();
    }

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("        selOffset=%u, selLength=%u",
         selOffset, selLength));

    // XXX nsString::Find and nsString::RFind take int32_t for offset, so,
    //     we cannot support this request when the current offset is larger
    //     than INT32_MAX.
    if (selOffset > INT32_MAX || selLength > INT32_MAX ||
        selOffset + selLength > INT32_MAX) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, The selection is out of range"));
        return NS_ERROR_FAILURE;
    }

    // Get all text contents of the focused editor
    WidgetQueryContentEvent queryTextContentEvent(true,
                                                  NS_QUERY_TEXT_CONTENT,
                                                  mLastFocusedWindow);
    queryTextContentEvent.InitForQueryTextContent(0, UINT32_MAX);
    mLastFocusedWindow->DispatchEvent(&queryTextContentEvent, status);
    NS_ENSURE_TRUE(queryTextContentEvent.mSucceeded, NS_ERROR_FAILURE);

    nsAutoString textContent(queryTextContentEvent.mReply.mString);
    if (selOffset + selLength > textContent.Length()) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, The selection is invalid, textContent.Length()=%u",
             textContent.Length()));
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

    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("    aText=%s, aText.Length()=%u, aCursorPos=%u",
         NS_ConvertUTF16toUTF8(aText).get(),
         aText.Length(), aCursorPos));

    return NS_OK;
}

nsresult
nsGtkIMModule::DeleteText(GtkIMContext* aContext,
                          int32_t aOffset,
                          uint32_t aNChars)
{
    MOZ_LOG(gGtkIMLog, LogLevel::Info,
        ("GtkIMModule(%p): DeleteText, aContext=%p, aOffset=%d, aNChars=%d, "
         "mCompositionState=%s",
         this, aContext, aOffset, aNChars, GetCompositionStateName()));

    if (!mLastFocusedWindow) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, there are no focused window in this module"));
        return NS_ERROR_NULL_POINTER;
    }

    if (!aNChars) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, aNChars must not be zero"));
        return NS_ERROR_INVALID_ARG;
    }

    nsRefPtr<nsWindow> lastFocusedWindow(mLastFocusedWindow);
    nsEventStatus status;

    // First, we should cancel current composition because editor cannot
    // handle changing selection and deleting text.
    uint32_t selOffset;
    bool wasComposing = IsComposing();
    bool editorHadCompositionString = EditorHasCompositionString();
    if (wasComposing) {
        selOffset = mCompositionStart;
        if (!DispatchCompositionCommitEvent(aContext, &mSelectedString)) {
            MOZ_LOG(gGtkIMLog, LogLevel::Info,
                ("    FAILED, quitting from DeletText"));
            return NS_ERROR_FAILURE;
        }
    } else {
        // Query cursor position & selection
        WidgetQueryContentEvent querySelectedTextEvent(true,
                                                       NS_QUERY_SELECTED_TEXT,
                                                       mLastFocusedWindow);
        lastFocusedWindow->DispatchEvent(&querySelectedTextEvent, status);
        NS_ENSURE_TRUE(querySelectedTextEvent.mSucceeded, NS_ERROR_FAILURE);

        selOffset = querySelectedTextEvent.mReply.mOffset;
    }

    // Get all text contents of the focused editor
    WidgetQueryContentEvent queryTextContentEvent(true,
                                                  NS_QUERY_TEXT_CONTENT,
                                                  mLastFocusedWindow);
    queryTextContentEvent.InitForQueryTextContent(0, UINT32_MAX);
    mLastFocusedWindow->DispatchEvent(&queryTextContentEvent, status);
    NS_ENSURE_TRUE(queryTextContentEvent.mSucceeded, NS_ERROR_FAILURE);
    if (queryTextContentEvent.mReply.mString.IsEmpty()) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, there is no contents"));
        return NS_ERROR_FAILURE;
    }

    NS_ConvertUTF16toUTF8 utf8Str(
        nsDependentSubstring(queryTextContentEvent.mReply.mString,
                             0, selOffset));
    glong offsetInUTF8Characters =
        g_utf8_strlen(utf8Str.get(), utf8Str.Length()) + aOffset;
    if (offsetInUTF8Characters < 0) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, aOffset is too small for current cursor pos "
             "(computed offset: %d)",
             offsetInUTF8Characters));
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
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, aNChars is too large for current contents "
             "(content length: %d, computed end offset: %d)",
             countOfCharactersInUTF8, endInUTF8Characters));
        return NS_ERROR_FAILURE;
    }

    gchar* charAtOffset =
        g_utf8_offset_to_pointer(utf8Str.get(), offsetInUTF8Characters);
    gchar* charAtEnd =
        g_utf8_offset_to_pointer(utf8Str.get(), endInUTF8Characters);

    // Set selection to delete
    WidgetSelectionEvent selectionEvent(true, NS_SELECTION_SET,
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
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, setting selection caused focus change "
             "or window destroyed"));
        return NS_ERROR_FAILURE;
    }

    // Delete the selection
    WidgetContentCommandEvent contentCommandEvent(true,
                                                  NS_CONTENT_COMMAND_DELETE,
                                                  mLastFocusedWindow);
    mLastFocusedWindow->DispatchEvent(&contentCommandEvent, status);

    if (!contentCommandEvent.mSucceeded ||
        lastFocusedWindow != mLastFocusedWindow ||
        lastFocusedWindow->Destroyed()) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, deleting the selection caused focus change "
             "or window destroyed"));
        return NS_ERROR_FAILURE;
    }

    if (!wasComposing) {
        return NS_OK;
    }

    // Restore the composition at new caret position.
    if (!DispatchCompositionStart(aContext)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, resterting composition start"));
        return NS_ERROR_FAILURE;
    }

    if (!editorHadCompositionString) {
        return NS_OK;
    }

    nsAutoString compositionString;
    GetCompositionString(aContext, compositionString);
    if (!DispatchCompositionChangeEvent(aContext, compositionString)) {
        MOZ_LOG(gGtkIMLog, LogLevel::Info,
            ("    FAILED, restoring composition string"));
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

void
nsGtkIMModule::InitEvent(WidgetGUIEvent& aEvent)
{
    aEvent.time = PR_Now() / 1000;
}
