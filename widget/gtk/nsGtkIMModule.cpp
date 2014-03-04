/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif // MOZ_LOGGING
#include "prlog.h"
#include "prtime.h"

#include "nsGtkIMModule.h"
#include "nsWindow.h"
#include "mozilla/Likely.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;
using namespace mozilla::widget;

#ifdef PR_LOGGING
PRLogModuleInfo* gGtkIMLog = nullptr;

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
#endif

const static bool kUseSimpleContextDefault = MOZ_WIDGET_GTK == 2;

nsGtkIMModule* nsGtkIMModule::sLastFocusedModule = nullptr;
bool nsGtkIMModule::sUseSimpleContext;

nsGtkIMModule::nsGtkIMModule(nsWindow* aOwnerWindow) :
    mOwnerWindow(aOwnerWindow), mLastFocusedWindow(nullptr),
    mContext(nullptr),
    mSimpleContext(nullptr),
    mDummyContext(nullptr),
    mCompositionStart(UINT32_MAX), mProcessingKeyEvent(nullptr),
    mCompositionTargetOffset(UINT32_MAX),
    mCompositionState(eCompositionState_NotComposing),
    mIsIMFocused(false), mIgnoreNativeCompositionEvent(false)
{
#ifdef PR_LOGGING
    if (!gGtkIMLog) {
        gGtkIMLog = PR_NewLogModule("nsGtkIMModuleWidgets");
    }
#endif
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
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p) was gone", this));
}

void
nsGtkIMModule::OnDestroyWindow(nsWindow* aWindow)
{
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): OnDestroyWindow, aWindow=%p, mLastFocusedWindow=%p, mOwnerWindow=%p, mLastFocusedModule=%p",
         this, aWindow, mLastFocusedWindow, mOwnerWindow, sLastFocusedModule));

    NS_PRECONDITION(aWindow, "aWindow must not be null");

    if (mLastFocusedWindow == aWindow) {
        CancelIMEComposition(aWindow);
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

    mOwnerWindow = nullptr;
    mLastFocusedWindow = nullptr;
    mInputContext.mIMEState.mEnabled = IMEState::DISABLED;

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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
    MozContainer* container = mOwnerWindow->GetMozContainer();
    NS_PRECONDITION(container, "The container of the window is null");

    GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT(aContext);
#if (MOZ_WIDGET_GTK == 2)
    GtkIMContext *slave = multicontext->slave;
#else
    GtkIMContext *slave = nullptr; //TODO GTK3
#endif
    if (!slave) {
        return;
    }

    GType slaveType = G_TYPE_FROM_INSTANCE(slave);
    const gchar *im_type_name = g_type_name(slaveType);
    if (strcmp(im_type_name, "GtkIMContextXIM") == 0) {
        if (gtk_check_version(2, 12, 1) == nullptr) {
            return; // gtk bug has been fixed
        }

        struct GtkIMContextXIM
        {
            GtkIMContext parent;
            gpointer private_data;
            // ... other fields
        };

        gpointer signal_data =
            reinterpret_cast<GtkIMContextXIM*>(slave)->private_data;
        if (!signal_data) {
            return;
        }

        g_signal_handlers_disconnect_matched(
            gtk_widget_get_display(GTK_WIDGET(container)),
            G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, signal_data);

        // Add a reference to prevent the XIM module from being unloaded
        // and reloaded: each time the module is loaded and used, it
        // opens (and doesn't close) new XOpenIM connections.
        static gpointer gtk_xim_context_class =
            g_type_class_ref(slaveType);
        // Mute unused variable warning:
        (void)gtk_xim_context_class;
    } else if (strcmp(im_type_name, "GtkIMContextIIIM") == 0) {
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

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): OnBlurWindow, aWindow=%p, mLastFocusedWindow=%p, mIsIMFocused=%s",
         this, aWindow, mLastFocusedWindow, mIsIMFocused ? "YES" : "NO"));

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

    if (!IsEditable() || MOZ_UNLIKELY(IsDestroyed())) {
        return false;
    }

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): OnKeyEvent, aCaller=%p, aKeyDownEventWasSent=%s",
         this, aCaller, aKeyDownEventWasSent ? "TRUE" : "FALSE"));
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("    aEvent: type=%s, keyval=%s, unicode=0x%X",
         aEvent->type == GDK_KEY_PRESS ? "GDK_KEY_PRESS" :
         aEvent->type == GDK_KEY_RELEASE ? "GDK_KEY_RELEASE" : "Unknown",
         gdk_keyval_name(aEvent->keyval),
         gdk_keyval_to_unicode(aEvent->keyval)));

    if (aCaller != mLastFocusedWindow) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, the caller isn't focused window, mLastFocusedWindow=%p",
             mLastFocusedWindow));
        return false;
    }

    GtkIMContext* im = GetContext();
    if (MOZ_UNLIKELY(!im)) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, there are no context"));
        return false;
    }

    mKeyDownEventWasSent = aKeyDownEventWasSent;
    mFilterKeyEvent = true;
    mProcessingKeyEvent = aEvent;
    gboolean isFiltered = gtk_im_context_filter_keypress(im, aEvent);
    mProcessingKeyEvent = nullptr;

    // We filter the key event if the event was not committed (because
    // it's probably part of a composition) or if the key event was
    // committed _and_ changed.  This way we still let key press
    // events go through as simple key press events instead of
    // composed characters.
    bool filterThisEvent = isFiltered && mFilterKeyEvent;

    if (IsComposing() && !isFiltered) {
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
                CommitCompositionBy(EmptyString());
                filterThisEvent = false;
            }
        } else {
            // Key release event may not be consumed by IM, however, we
            // shouldn't dispatch any keyup event during composition.
            filterThisEvent = true;
        }
    }

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("    filterThisEvent=%s (isFiltered=%s, mFilterKeyEvent=%s)",
         filterThisEvent ? "TRUE" : "FALSE", isFiltered ? "YES" : "NO",
         mFilterKeyEvent ? "YES" : "NO"));

    return filterThisEvent;
}

void
nsGtkIMModule::OnFocusChangeInGecko(bool aFocus)
{
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): OnFocusChangeInGecko, aFocus=%s, "
         "mCompositionState=%s, mIsIMFocused=%s, "
         "mIgnoreNativeCompositionEvent=%s",
         this, aFocus ? "YES" : "NO", GetCompositionStateName(),
         mIsIMFocused ? "YES" : "NO",
         mIgnoreNativeCompositionEvent ? "YES" : "NO"));

    // We shouldn't carry over the removed string to another editor.
    mSelectedString.Truncate();

    if (aFocus) {
        // If we failed to commit forcedely in previous focused editor,
        // we should reopen the gate for native signals in new focused editor.
        mIgnoreNativeCompositionEvent = false;
    }
}

void
nsGtkIMModule::ResetIME()
{
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): ResetIME, mCompositionState=%s, mIsIMFocused=%s",
         this, GetCompositionStateName(), mIsIMFocused ? "YES" : "NO"));

    GtkIMContext *im = GetContext();
    if (MOZ_UNLIKELY(!im)) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, there are no context"));
        return;
    }

    mIgnoreNativeCompositionEvent = true;
    gtk_im_context_reset(im);
}

nsresult
nsGtkIMModule::CommitIMEComposition(nsWindow* aCaller)
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return NS_OK;
    }

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): CommitIMEComposition, aCaller=%p, "
         "mCompositionState=%s",
         this, aCaller, GetCompositionStateName()));

    if (aCaller != mLastFocusedWindow) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    WARNING: the caller isn't focused window, mLastFocusedWindow=%p",
             mLastFocusedWindow));
        return NS_OK;
    }

    if (!IsComposing()) {
        return NS_OK;
    }

    // XXX We should commit composition ourselves temporary...
    ResetIME();
    CommitCompositionBy(mDispatchedCompositionString);

    return NS_OK;
}

nsresult
nsGtkIMModule::CancelIMEComposition(nsWindow* aCaller)
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return NS_OK;
    }

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): CancelIMEComposition, aCaller=%p",
         this, aCaller));

    if (aCaller != mLastFocusedWindow) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, the caller isn't focused window, mLastFocusedWindow=%p",
             mLastFocusedWindow));
        return NS_OK;
    }

    if (!IsComposing()) {
        return NS_OK;
    }

    GtkIMContext *im = GetContext();
    if (MOZ_UNLIKELY(!im)) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, there are no context"));
        return NS_OK;
    }

    ResetIME();
    CommitCompositionBy(EmptyString());

    return NS_OK;
}

void
nsGtkIMModule::OnUpdateComposition(void)
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return;
    }

    SetCursorPosition(mCompositionTargetOffset);
}

void
nsGtkIMModule::SetInputContext(nsWindow* aCaller,
                               const InputContext* aContext,
                               const InputContextAction* aAction)
{
    if (MOZ_UNLIKELY(IsDestroyed())) {
        return;
    }

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): SetInputContext, aCaller=%p, aState=%s mHTMLInputType=%s",
         this, aCaller, GetEnabledStateName(aContext->mIMEState.mEnabled),
         NS_ConvertUTF16toUTF8(aContext->mHTMLInputType).get()));

    if (aCaller != mLastFocusedWindow) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, the caller isn't focused window, mLastFocusedWindow=%p",
             mLastFocusedWindow));
        return;
    }

    if (!mContext) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, there are no context"));
        return;
    }


    if (sLastFocusedModule != this) {
        mInputContext = *aContext;
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    SUCCEEDED, but we're not active"));
        return;
    }

    bool changingEnabledState =
        aContext->mIMEState.mEnabled != mInputContext.mIMEState.mEnabled ||
        aContext->mHTMLInputType != mInputContext.mHTMLInputType;

    // Release current IME focus if IME is enabled.
    if (changingEnabledState && IsEditable()) {
        CommitIMEComposition(mLastFocusedWindow);
        Blur();
    }

    mInputContext = *aContext;

    if (changingEnabledState) {
#if (MOZ_WIDGET_GTK == 3)
        static bool sInputPurposeSupported = !gtk_check_version(3, 6, 0);
        if (sInputPurposeSupported && IsEditable()) {
            GtkIMContext* context = GetContext();
            if (context) {
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

                g_object_set(context, "input-purpose", purpose, nullptr);
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

/* static */
bool
nsGtkIMModule::IsVirtualKeyboardOpened()
{
    return false;
}

GtkIMContext*
nsGtkIMModule::GetContext()
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
nsGtkIMModule::IsEnabled()
{
    return mInputContext.mIMEState.mEnabled == IMEState::ENABLED ||
           mInputContext.mIMEState.mEnabled == IMEState::PLUGIN ||
           (!sUseSimpleContext &&
            mInputContext.mIMEState.mEnabled == IMEState::PASSWORD);
}

bool
nsGtkIMModule::IsEditable()
{
    return mInputContext.mIMEState.mEnabled == IMEState::ENABLED ||
           mInputContext.mIMEState.mEnabled == IMEState::PLUGIN ||
           mInputContext.mIMEState.mEnabled == IMEState::PASSWORD;
}

void
nsGtkIMModule::Focus()
{
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): Focus, sLastFocusedModule=%p",
         this, sLastFocusedModule));

    if (mIsIMFocused) {
        NS_ASSERTION(sLastFocusedModule == this,
                     "We're not active, but the IM was focused?");
        return;
    }

    GtkIMContext *im = GetContext();
    if (!im) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, there are no context"));
        return;
    }

    if (sLastFocusedModule && sLastFocusedModule != this) {
        sLastFocusedModule->Blur();
    }

    sLastFocusedModule = this;

    gtk_im_context_focus_in(im);
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
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): Blur, mIsIMFocused=%s",
         this, mIsIMFocused ? "YES" : "NO"));

    if (!mIsIMFocused) {
        return;
    }

    GtkIMContext *im = GetContext();
    if (!im) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, there are no context"));
        return;
    }

    gtk_im_context_focus_out(im);
    mIsIMFocused = false;
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
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): OnStartCompositionNative, aContext=%p",
         this, aContext));

    // See bug 472635, we should do nothing if IM context doesn't match.
    if (GetContext() != aContext) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, given context doesn't match, GetContext()=%p",
             GetContext()));
        return;
    }

    if (!DispatchCompositionStart()) {
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
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): OnEndCompositionNative, aContext=%p",
         this, aContext));

    // See bug 472635, we should do nothing if IM context doesn't match.
    if (GetContext() != aContext) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, given context doesn't match, GetContext()=%p",
             GetContext()));
        return;
    }

    bool shouldIgnoreThisEvent = ShouldIgnoreNativeCompositionEvent();

    // Finish the cancelling mode here rather than DispatchCompositionEnd()
    // because DispatchCompositionEnd() is called ourselves when we need to
    // commit the composition string *before* the focus moves completely.
    // Note that the native commit can be fired *after* ResetIME().
    mIgnoreNativeCompositionEvent = false;

    if (!IsComposing() || shouldIgnoreThisEvent) {
        // If we already handled the commit event, we should do nothing here.
        return;
    }

    // Be aware, widget can be gone
    DispatchCompositionEnd();
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
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): OnChangeCompositionNative, aContext=%p",
         this, aContext));

    // See bug 472635, we should do nothing if IM context doesn't match.
    if (GetContext() != aContext) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, given context doesn't match, GetContext()=%p",
             GetContext()));
        return;
    }

    if (ShouldIgnoreNativeCompositionEvent()) {
        return;
    }

    nsAutoString compositionString;
    GetCompositionString(compositionString);
    if (!IsComposing() && compositionString.IsEmpty()) {
        mDispatchedCompositionString.Truncate();
        return; // Don't start the composition with empty string.
    }

    // Be aware, widget can be gone
    DispatchTextEvent(compositionString, false);
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
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): OnRetrieveSurroundingNative, aContext=%p, current context=%p",
         this, aContext, GetContext()));

    if (GetContext() != aContext) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, given context doesn't match, GetContext()=%p",
             GetContext()));
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
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): OnDeleteSurroundingNative, aContext=%p, current context=%p",
         this, aContext, GetContext()));

    if (GetContext() != aContext) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, given context doesn't match, GetContext()=%p",
             GetContext()));
        return FALSE;
    }

    if (NS_SUCCEEDED(DeleteText(aOffset, (uint32_t)aNChars))) {
        return TRUE;
    }

    // failed
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): OnCommitCompositionNative, aContext=%p, current context=%p, commitString=\"%s\"",
         this, aContext, GetContext(), commitString));

    // See bug 472635, we should do nothing if IM context doesn't match.
    if (GetContext() != aContext) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, given context doesn't match, GetContext()=%p",
             GetContext()));
        return;
    }

    // If we are not in composition and committing with empty string,
    // we need to do nothing because if we continued to handle this
    // signal, we would dispatch compositionstart, text, compositionend
    // events with empty string.  Of course, they are unnecessary events
    // for Web applications and our editor.
    if (!IsComposing() && !commitString[0]) {
        return;
    }

    if (ShouldIgnoreNativeCompositionEvent()) {
        return;
    }

    // If IME doesn't change their keyevent that generated this commit,
    // don't send it through XIM - just send it as a normal key press
    // event.
    if (!IsComposing() && mProcessingKeyEvent) {
        char keyval_utf8[8]; /* should have at least 6 bytes of space */
        gint keyval_utf8_len;
        guint32 keyval_unicode;

        keyval_unicode = gdk_keyval_to_unicode(mProcessingKeyEvent->keyval);
        keyval_utf8_len = g_unichar_to_utf8(keyval_unicode, keyval_utf8);
        keyval_utf8[keyval_utf8_len] = '\0';

        if (!strcmp(commitString, keyval_utf8)) {
            PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
                ("GtkIMModule(%p): OnCommitCompositionNative, we'll send normal key event",
                 this));
            mFilterKeyEvent = false;
            return;
        }
    }

    NS_ConvertUTF8toUTF16 str(commitString);
    CommitCompositionBy(str); // Be aware, widget can be gone
}

bool
nsGtkIMModule::CommitCompositionBy(const nsAString& aString)
{
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): CommitCompositionBy, aString=\"%s\", "
         "mDispatchedCompositionString=\"%s\"",
         this, NS_ConvertUTF16toUTF8(aString).get(),
         NS_ConvertUTF16toUTF8(mDispatchedCompositionString).get()));

    if (!DispatchTextEvent(aString, true)) {
        return false;
    }
    // We should dispatch the compositionend event here because some IMEs
    // might not fire "preedit_end" native event.
    return DispatchCompositionEnd(); // Be aware, widget can be gone
}

void
nsGtkIMModule::GetCompositionString(nsAString &aCompositionString)
{
    gchar *preedit_string;
    gint cursor_pos;
    PangoAttrList *feedback_list;
    gtk_im_context_get_preedit_string(GetContext(), &preedit_string,
                                      &feedback_list, &cursor_pos);
    if (preedit_string && *preedit_string) {
        CopyUTF8toUTF16(preedit_string, aCompositionString);
    } else {
        aCompositionString.Truncate();
    }

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): GetCompositionString, result=\"%s\"",
         this, preedit_string));

    pango_attr_list_unref(feedback_list);
    g_free(preedit_string);
}

bool
nsGtkIMModule::DispatchCompositionStart()
{
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): DispatchCompositionStart", this));

    if (IsComposing()) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    WARNING, we're already in composition"));
        return true;
    }

    if (!mLastFocusedWindow) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, there are no focused window in this module"));
        return false;
    }

    nsEventStatus status;
    WidgetQueryContentEvent selection(true, NS_QUERY_SELECTED_TEXT,
                                      mLastFocusedWindow);
    InitEvent(selection);
    mLastFocusedWindow->DispatchEvent(&selection, status);

    if (!selection.mSucceeded || selection.mReply.mOffset == UINT32_MAX) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    keydown event is dispatched"));
        if (static_cast<nsWindow*>(kungFuDeathGrip.get())->IsDestroyed() ||
            kungFuDeathGrip != mLastFocusedWindow) {
            PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
                ("    NOTE, the focused widget was destroyed/changed by keydown event"));
            return false;
        }
    }

    if (mIgnoreNativeCompositionEvent) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    WARNING, mIgnoreNativeCompositionEvent is already TRUE, but we forcedly reset"));
        mIgnoreNativeCompositionEvent = false;
    }

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("    mCompositionStart=%u", mCompositionStart));
    mCompositionState = eCompositionState_CompositionStartDispatched;
    WidgetCompositionEvent compEvent(true, NS_COMPOSITION_START,
                                     mLastFocusedWindow);
    InitEvent(compEvent);
    nsCOMPtr<nsIWidget> kungFuDeathGrip = mLastFocusedWindow;
    mLastFocusedWindow->DispatchEvent(&compEvent, status);
    if (static_cast<nsWindow*>(kungFuDeathGrip.get())->IsDestroyed() ||
        kungFuDeathGrip != mLastFocusedWindow) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    NOTE, the focused widget was destroyed/changed by compositionstart event"));
        return false;
    }

    return true;
}

bool
nsGtkIMModule::DispatchCompositionEnd()
{
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): DispatchCompositionEnd, "
         "mDispatchedCompositionString=\"%s\"",
         this, NS_ConvertUTF16toUTF8(mDispatchedCompositionString).get()));

    if (!IsComposing()) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    WARNING, we have alrady finished the composition"));
        return false;
    }

    if (!mLastFocusedWindow) {
        mDispatchedCompositionString.Truncate();
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, there are no focused window in this module"));
        return false;
    }

    WidgetCompositionEvent compEvent(true, NS_COMPOSITION_END,
                                     mLastFocusedWindow);
    InitEvent(compEvent);
    compEvent.data = mDispatchedCompositionString;
    nsEventStatus status;
    nsCOMPtr<nsIWidget> kungFuDeathGrip = mLastFocusedWindow;
    mLastFocusedWindow->DispatchEvent(&compEvent, status);
    mCompositionState = eCompositionState_NotComposing;
    mCompositionStart = UINT32_MAX;
    mCompositionTargetOffset = UINT32_MAX;
    mDispatchedCompositionString.Truncate();
    if (static_cast<nsWindow*>(kungFuDeathGrip.get())->IsDestroyed() ||
        kungFuDeathGrip != mLastFocusedWindow) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    NOTE, the focused widget was destroyed/changed by compositionend event"));
        return false;
    }

    return true;
}

bool
nsGtkIMModule::DispatchTextEvent(const nsAString &aCompositionString,
                                 bool aIsCommit)
{
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): DispatchTextEvent, aIsCommit=%s",
         this, aIsCommit ? "TRUE" : "FALSE"));

    if (!mLastFocusedWindow) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, there are no focused window in this module"));
        return false;
    }

    if (!IsComposing()) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    The composition wasn't started, force starting..."));
        nsCOMPtr<nsIWidget> kungFuDeathGrip = mLastFocusedWindow;
        if (!DispatchCompositionStart()) {
            return false;
        }
    }

    nsEventStatus status;
    nsRefPtr<nsWindow> lastFocusedWindow = mLastFocusedWindow;

    if (aCompositionString != mDispatchedCompositionString) {
      WidgetCompositionEvent compositionUpdate(true, NS_COMPOSITION_UPDATE,
                                               mLastFocusedWindow);
      InitEvent(compositionUpdate);
      compositionUpdate.data = aCompositionString;
      mDispatchedCompositionString = aCompositionString;
      mLastFocusedWindow->DispatchEvent(&compositionUpdate, status);
      if (lastFocusedWindow->IsDestroyed() ||
          lastFocusedWindow != mLastFocusedWindow) {
          PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
              ("    NOTE, the focused widget was destroyed/changed by compositionupdate"));
          return false;
      }
    }

    // Store the selected string which will be removed by following text event.
    if (mCompositionState == eCompositionState_CompositionStartDispatched) {
        // XXX We should assume, for now, any web applications don't change
        //     selection at handling this text event.
        WidgetQueryContentEvent querySelectedTextEvent(true,
                                                       NS_QUERY_SELECTED_TEXT,
                                                       mLastFocusedWindow);
        mLastFocusedWindow->DispatchEvent(&querySelectedTextEvent, status);
        if (querySelectedTextEvent.mSucceeded) {
            mSelectedString = querySelectedTextEvent.mReply.mString;
            mCompositionStart = querySelectedTextEvent.mReply.mOffset;
        }
    }

    WidgetTextEvent textEvent(true, NS_TEXT_TEXT, mLastFocusedWindow);
    InitEvent(textEvent);

    uint32_t targetOffset = mCompositionStart;

    if (!aIsCommit) {
        // NOTE: SetTextRangeList() assumes that mDispatchedCompositionString
        //       has been updated already.
        textEvent.mRanges = CreateTextRangeArray();
        targetOffset += textEvent.mRanges->TargetClauseOffset();
    }

    textEvent.theText = mDispatchedCompositionString.get();

    mCompositionState = aIsCommit ?
        eCompositionState_CommitTextEventDispatched :
        eCompositionState_TextEventDispatched;

    mLastFocusedWindow->DispatchEvent(&textEvent, status);
    if (lastFocusedWindow->IsDestroyed() ||
        lastFocusedWindow != mLastFocusedWindow) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    NOTE, the focused widget was destroyed/changed by text event"));
        return false;
    }

    // We cannot call SetCursorPosition for e10s-aware.
    // DispatchEvent is async on e10s, so composition rect isn't updated now
    // on tab parent.
    mCompositionTargetOffset = targetOffset;

    return true;
}

already_AddRefed<TextRangeArray>
nsGtkIMModule::CreateTextRangeArray()
{
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): CreateTextRangeArray", this));

    nsRefPtr<TextRangeArray> textRangeArray = new TextRangeArray();

    gchar *preedit_string;
    gint cursor_pos;
    PangoAttrList *feedback_list;
    gtk_im_context_get_preedit_string(GetContext(), &preedit_string,
                                      &feedback_list, &cursor_pos);
    if (!preedit_string || !*preedit_string) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    preedit_string is null"));
        pango_attr_list_unref(feedback_list);
        g_free(preedit_string);
        return textRangeArray.forget();
    }

    PangoAttrIterator* iter;
    iter = pango_attr_list_get_iterator(feedback_list);
    if (!iter) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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

        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    mStartOffset=%u, mEndOffset=%u, mRangeType=%s",
             range.mStartOffset, range.mEndOffset,
             GetRangeTypeName(range.mRangeType)));
    } while (pango_attr_iterator_next(iter));

    TextRange range;
    if (cursor_pos < 0) {
        range.mStartOffset = 0;
    } else if (uint32_t(cursor_pos) > mDispatchedCompositionString.Length()) {
        range.mStartOffset = mDispatchedCompositionString.Length();
    } else {
        range.mStartOffset = uint32_t(cursor_pos);
    }
    range.mEndOffset = range.mStartOffset;
    range.mRangeType = NS_TEXTRANGE_CARETPOSITION;
    textRangeArray->AppendElement(range);

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("    mStartOffset=%u, mEndOffset=%u, mRangeType=%s",
         range.mStartOffset, range.mEndOffset,
         GetRangeTypeName(range.mRangeType)));

    pango_attr_iterator_destroy(iter);
    pango_attr_list_unref(feedback_list);
    g_free(preedit_string);

    return textRangeArray.forget();
}

void
nsGtkIMModule::SetCursorPosition(uint32_t aTargetOffset)
{
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): SetCursorPosition, aTargetOffset=%u",
         this, aTargetOffset));

    if (aTargetOffset == UINT32_MAX) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, aTargetOffset is wrong offset"));
        return;
    }

    if (!mLastFocusedWindow) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, there are no focused window"));
        return;
    }

    GtkIMContext *im = GetContext();
    if (!im) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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

    gtk_im_context_set_cursor_location(im, &area);
}

nsresult
nsGtkIMModule::GetCurrentParagraph(nsAString& aText, uint32_t& aCursorPos)
{
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): GetCurrentParagraph, mCompositionState=%s",
         this, GetCompositionStateName()));

    if (!mLastFocusedWindow) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("        selOffset=%u, selLength=%u",
         selOffset, selLength));

    // XXX nsString::Find and nsString::RFind take int32_t for offset, so,
    //     we cannot support this request when the current offset is larger
    //     than INT32_MAX.
    if (selOffset > INT32_MAX || selLength > INT32_MAX ||
        selOffset + selLength > INT32_MAX) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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

    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("    aText=%s, aText.Length()=%u, aCursorPos=%u",
         NS_ConvertUTF16toUTF8(aText).get(),
         aText.Length(), aCursorPos));

    return NS_OK;
}

nsresult
nsGtkIMModule::DeleteText(const int32_t aOffset, const uint32_t aNChars)
{
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): DeleteText, aOffset=%d, aNChars=%d, "
         "mCompositionState=%s",
         this, aOffset, aNChars, GetCompositionStateName()));

    if (!mLastFocusedWindow) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, there are no focused window in this module"));
        return NS_ERROR_NULL_POINTER;
    }

    if (!aNChars) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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
        if (editorHadCompositionString &&
            !DispatchTextEvent(mSelectedString, false)) {
            PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
                ("    FAILED, quitting from DeletText"));
            return NS_ERROR_FAILURE;
        }
        if (!DispatchCompositionEnd()) {
            PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, there is no contents"));
        return NS_ERROR_FAILURE;
    }

    NS_ConvertUTF16toUTF8 utf8Str(
        nsDependentSubstring(queryTextContentEvent.mReply.mString,
                             0, selOffset));
    glong offsetInUTF8Characters =
        g_utf8_strlen(utf8Str.get(), utf8Str.Length()) + aOffset;
    if (offsetInUTF8Characters < 0) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, deleting the selection caused focus change "
             "or window destroyed"));
        return NS_ERROR_FAILURE;
    }

    if (!wasComposing) {
        return NS_OK;
    }

    // Restore the composition at new caret position.
    if (!DispatchCompositionStart()) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
            ("    FAILED, resterting composition start"));
        return NS_ERROR_FAILURE;
    }

    if (!editorHadCompositionString) {
        return NS_OK;
    }

    nsAutoString compositionString;
    GetCompositionString(compositionString);
    if (!DispatchTextEvent(compositionString, true)) {
        PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
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

bool
nsGtkIMModule::ShouldIgnoreNativeCompositionEvent()
{
    PR_LOG(gGtkIMLog, PR_LOG_ALWAYS,
        ("GtkIMModule(%p): ShouldIgnoreNativeCompositionEvent, mLastFocusedWindow=%p, mIgnoreNativeCompositionEvent=%s",
         this, mLastFocusedWindow,
         mIgnoreNativeCompositionEvent ? "YES" : "NO"));

    if (!mLastFocusedWindow) {
        return true; // cannot continue
    }

    return mIgnoreNativeCompositionEvent;
}
