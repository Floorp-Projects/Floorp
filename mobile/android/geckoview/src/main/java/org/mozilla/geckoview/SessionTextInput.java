/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.RectF;
import android.os.Handler;
import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.text.Editable;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import org.mozilla.gecko.IGeckoEditableParent;
import org.mozilla.gecko.InputMethods;
import org.mozilla.gecko.NativeQueue;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.ThreadUtils;

/**
 * {@code SessionTextInput} handles text input for {@code GeckoSession} through key events or input
 * methods. It is typically used to implement certain methods in {@link android.view.View}
 * such as {@link android.view.View#onCreateInputConnection}, by forwarding such calls to
 * corresponding methods in {@code SessionTextInput}.
 * <p>
 * For full functionality, {@code SessionTextInput} requires a {@link android.view.View} to be set
 * first through {@link #setView}. When a {@link android.view.View} is not set or set to null,
 * {@code SessionTextInput} will operate in a reduced functionality mode. See {@link
 * #onCreateInputConnection} and methods in {@link GeckoSession.TextInputDelegate} for changes in
 * behavior in this viewless mode.
 */
public final class SessionTextInput {
    /* package */ static final String LOGTAG = "GeckoSessionTextInput";
    private static final boolean DEBUG = false;

    // Interface to access GeckoInputConnection from SessionTextInput.
    /* package */ interface InputConnectionClient {
        View getView();
        Handler getHandler(Handler defHandler);
        InputConnection onCreateInputConnection(EditorInfo attrs);
    }

    // Interface to access GeckoEditable from GeckoInputConnection.
    /* package */ interface EditableClient {
        // The following value is used by requestCursorUpdates
        // ONE_SHOT calls updateCompositionRects() after getting current composing
        // character rects.
        @WrapForJNI final int ONE_SHOT = 1;
        // START_MONITOR start the monitor for composing character rects.  If is is
        // updaed,  call updateCompositionRects()
        @WrapForJNI final int START_MONITOR = 2;
        // ENDT_MONITOR stops the monitor for composing character rects.
        @WrapForJNI final int END_MONITOR = 3;

        void sendKeyEvent(@Nullable View view, int action, @NonNull KeyEvent event);
        Editable getEditable();
        void setBatchMode(boolean isBatchMode);
        Handler setInputConnectionHandler(@NonNull Handler handler);
        void postToInputConnection(@NonNull Runnable runnable);
        void requestCursorUpdates(int requestMode);
    }

    // Interface to access GeckoInputConnection from GeckoEditable.
    /* package */ interface EditableListener {
        // IME notification type for notifyIME(), corresponding to NotificationToIME enum.
        @WrapForJNI final int NOTIFY_IME_OF_TOKEN = -3;
        @WrapForJNI final int NOTIFY_IME_OPEN_VKB = -2;
        @WrapForJNI final int NOTIFY_IME_REPLY_EVENT = -1;
        @WrapForJNI final int NOTIFY_IME_OF_FOCUS = 1;
        @WrapForJNI final int NOTIFY_IME_OF_BLUR = 2;
        @WrapForJNI final int NOTIFY_IME_TO_COMMIT_COMPOSITION = 8;
        @WrapForJNI final int NOTIFY_IME_TO_CANCEL_COMPOSITION = 9;

        // IME enabled state for notifyIMEContext().
        final int IME_STATE_UNKNOWN = -1;
        final int IME_STATE_DISABLED = 0;
        final int IME_STATE_ENABLED = 1;
        final int IME_STATE_PASSWORD = 2;

        // Flags for notifyIMEContext().
        @WrapForJNI final int IME_FLAG_PRIVATE_BROWSING = 1;
        @WrapForJNI final int IME_FLAG_USER_ACTION = 2;

        void notifyIME(int type);
        void notifyIMEContext(int state, String typeHint, String modeHint,
                              String actionHint, int flag);
        void onSelectionChange();
        void onTextChange();
        void onDiscardComposition();
        void onDefaultKeyEvent(KeyEvent event);
        void updateCompositionRects(final RectF[] aRects);
    }

    private static final class DefaultDelegate implements GeckoSession.TextInputDelegate {
        public static final DefaultDelegate INSTANCE = new DefaultDelegate();

        private InputMethodManager getInputMethodManager(@Nullable final View view) {
            if (view == null) {
                return null;
            }
            return (InputMethodManager) view.getContext()
                                            .getSystemService(Context.INPUT_METHOD_SERVICE);
        }

        @Override
        public void restartInput(@NonNull final GeckoSession session, final int reason) {
            ThreadUtils.assertOnUiThread();
            final View view = session.getTextInput().getView();

            final InputMethodManager imm = getInputMethodManager(view);
            if (imm == null) {
                return;
            }

            // InputMethodManager has internal logic to detect if we are restarting input
            // in an already focused View, which is the case here because all content text
            // fields are inside one LayerView. When this happens, InputMethodManager will
            // tell the input method to soft reset instead of hard reset. Stock latin IME
            // on Android 4.2+ has a quirk that when it soft resets, it does not clear the
            // composition. The following workaround tricks the IME into clearing the
            // composition when soft resetting.
            if (InputMethods.needsSoftResetWorkaround(
                    InputMethods.getCurrentInputMethod(view.getContext()))) {
                // Fake a selection change, because the IME clears the composition when
                // the selection changes, even if soft-resetting. Offsets here must be
                // different from the previous selection offsets, and -1 seems to be a
                // reasonable, deterministic value
                imm.updateSelection(view, -1, -1, -1, -1);
            }

            try {
                imm.restartInput(view);
            } catch (RuntimeException e) {
                Log.e(LOGTAG, "Error restarting input", e);
            }
        }

        @Override
        public void showSoftInput(@NonNull final GeckoSession session) {
            ThreadUtils.assertOnUiThread();
            final View view = session.getTextInput().getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                if (view.hasFocus() && !imm.isActive(view)) {
                    // Marshmallow workaround: The view has focus but it is not the active
                    // view for the input method. (Bug 1211848)
                    view.clearFocus();
                    view.requestFocus();
                }
                imm.showSoftInput(view, 0);
            }
        }

        @Override
        public void hideSoftInput(@NonNull final GeckoSession session) {
            ThreadUtils.assertOnUiThread();
            final View view = session.getTextInput().getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
            }
        }

        @Override
        public void updateSelection(@NonNull final GeckoSession session,
                                    final int selStart, final int selEnd,
                                    final int compositionStart, final int compositionEnd) {
            ThreadUtils.assertOnUiThread();
            final View view = session.getTextInput().getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                // When composition start and end is -1,
                // InputMethodManager.updateSelection will remove composition
                // on most IMEs. If not working, we have to add a workaround
                // to EditableListener.onDiscardComposition.
                imm.updateSelection(view, selStart, selEnd, compositionStart, compositionEnd);
            }
        }

        @Override
        public void updateExtractedText(@NonNull final GeckoSession session,
                                        @NonNull final ExtractedTextRequest request,
                                        @NonNull final ExtractedText text) {
            ThreadUtils.assertOnUiThread();
            final View view = session.getTextInput().getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                imm.updateExtractedText(view, request.token, text);
            }
        }

        @TargetApi(21)
        @Override
        public void updateCursorAnchorInfo(@NonNull final GeckoSession session,
                                           @NonNull final CursorAnchorInfo info) {
            ThreadUtils.assertOnUiThread();
            final View view = session.getTextInput().getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                imm.updateCursorAnchorInfo(view, info);
            }
        }
    }

    private final GeckoSession mSession;
    private final NativeQueue mQueue;
    private final GeckoEditable mEditable;
    private InputConnectionClient mInputConnection;
    private GeckoSession.TextInputDelegate mDelegate;

    /* package */ SessionTextInput(final @NonNull GeckoSession session,
                                   final @NonNull NativeQueue queue) {
        mSession = session;
        mQueue = queue;
        mEditable = new GeckoEditable(session);
    }

    /* package */ void onWindowChanged(final GeckoSession.Window window) {
        if (mQueue.isReady()) {
            window.attachEditable(mEditable);
        } else {
            mQueue.queueUntilReady(window, "attachEditable",
                                   IGeckoEditableParent.class, mEditable);
        }
    }

    /**
     * Get a Handler for the background input method thread. In order to use a background
     * thread for input method operations on systems prior to Nougat, first override
     * {@code View.getHandler()} for the View returning the InputConnection instance, and
     * then call this method from the overridden method.
     *
     * For example:<pre>
     * &#64;Override
     * public Handler getHandler() {
     *     if (Build.VERSION.SDK_INT &gt;= 24) {
     *         return super.getHandler();
     *     }
     *     return getSession().getTextInput().getHandler(super.getHandler());
     * }</pre>
     *
     * @param defHandler Handler returned by the system {@code getHandler} implementation.
     * @return Handler to return to the system through {@code getHandler}.
     */
    @AnyThread
    public synchronized @NonNull Handler getHandler(final @NonNull Handler defHandler) {
        // May be called on any thread.
        if (mInputConnection != null) {
            return mInputConnection.getHandler(defHandler);
        }
        return defHandler;
    }

    /**
     * Get the current {@link android.view.View} for text input.
     *
     * @return Current text input View or null if not set.
     * @see #setView(View)
     */
    @UiThread
    public @Nullable View getView() {
        ThreadUtils.assertOnUiThread();
        return mInputConnection != null ? mInputConnection.getView() : null;
    }

    /**
     * Set the current {@link android.view.View} for text input. The {@link android.view.View}
     * is used to interact with the system input method manager and to display certain text input
     * UI elements. See the {@code SessionTextInput} class documentation for information on
     * viewless mode, when the current {@link android.view.View} is not set or set to null.
     *
     * @param view Text input View or null to clear current View.
     * @see #getView()
     */
    @UiThread
    public synchronized void setView(final @Nullable View view) {
        ThreadUtils.assertOnUiThread();

        if (view == null) {
            mInputConnection = null;
        } else if (mInputConnection == null || mInputConnection.getView() != view) {
            mInputConnection = GeckoInputConnection.create(mSession, view, mEditable);
        }
        mEditable.setListener((EditableListener) mInputConnection);
    }

    /**
     * Get an {@link android.view.inputmethod.InputConnection} instance. In viewless mode,
     * this method still fills out the {@link android.view.inputmethod.EditorInfo} object,
     * but the return value will always be null.
     *
     * @param attrs EditorInfo instance to be filled on return.
     * @return InputConnection instance, or null if there is no active input
     *         (or if in viewless mode).
     */
    @AnyThread
    public synchronized @Nullable InputConnection onCreateInputConnection(
            final @NonNull EditorInfo attrs) {
        // May be called on any thread.
        mEditable.onCreateInputConnection(attrs);

        if (!mQueue.isReady() || mInputConnection == null) {
            return null;
        }
        return mInputConnection.onCreateInputConnection(attrs);
    }

    /**
     * Process a KeyEvent as a pre-IME event.
     *
     * @param keyCode Key code.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    @UiThread
    public boolean onKeyPreIme(final int keyCode, final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyPreIme(getView(), keyCode, event);
    }

    /**
     * Process a KeyEvent as a key-down event.
     *
     * @param keyCode Key code.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    @UiThread
    public boolean onKeyDown(final int keyCode, final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyDown(getView(), keyCode, event);
    }

    /**
     * Process a KeyEvent as a key-up event.
     *
     * @param keyCode Key code.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    @UiThread
    public boolean onKeyUp(final int keyCode, final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyUp(getView(), keyCode, event);
    }

    /**
     * Process a KeyEvent as a long-press event.
     *
     * @param keyCode Key code.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    @UiThread
    public boolean onKeyLongPress(final int keyCode, final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyLongPress(getView(), keyCode, event);
    }

    /**
     * Process a KeyEvent as a multiple-press event.
     *
     * @param keyCode Key code.
     * @param repeatCount Key repeat count.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    @UiThread
    public boolean onKeyMultiple(final int keyCode, final int repeatCount,
                                 final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyMultiple(getView(), keyCode, repeatCount, event);
    }

    /**
     * Set the current text input delegate.
     *
     * @param delegate TextInputDelegate instance or null to restore to default.
     */
    @UiThread
    public void setDelegate(@Nullable final GeckoSession.TextInputDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mDelegate = delegate;
    }

    /**
     * Get the current text input delegate.
     *
     * @return TextInputDelegate instance or a default instance if no delegate has been set.
     */
    @UiThread
    public @NonNull GeckoSession.TextInputDelegate getDelegate() {
        ThreadUtils.assertOnUiThread();
        if (mDelegate == null) {
            mDelegate = DefaultDelegate.INSTANCE;
        }
        return mDelegate;
    }
}
