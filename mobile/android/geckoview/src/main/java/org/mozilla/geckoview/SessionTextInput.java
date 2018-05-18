/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.InputMethods;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.GeckoEditableChild;
import org.mozilla.gecko.IGeckoEditableParent;
import org.mozilla.gecko.NativeQueue;
import org.mozilla.gecko.util.ThreadUtils;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.RectF;
import android.os.Handler;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
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

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * SessionTextInput handles text input for GeckoSession through key events or input
 * methods. It is typically used to implement certain methods in View such as {@code
 * onCreateInputConnection()}, by forwarding such calls to corresponding methods in
 * SessionTextInput.
 */
public final class SessionTextInput {
    /* package */ static final String LOGTAG = "GeckoSessionTextInput";

    /**
     * Interface that SessionTextInput uses for performing operations such as opening and closing
     * the software keyboard. If the delegate is not set, these operations are forwarded to the
     * system InputMethodManager automatically.
     */
    public interface Delegate {
        @Retention(RetentionPolicy.SOURCE)
        @IntDef({RESTART_REASON_FOCUS, RESTART_REASON_BLUR, RESTART_REASON_CONTENT_CHANGE})
        @interface RestartReason {}
        /** Restarting input due to an input field gaining focus. */
        int RESTART_REASON_FOCUS = 0;
        /** Restarting input due to an input field losing focus. */
        int RESTART_REASON_BLUR = 1;
        /**
         * Restarting input due to the content of the input field changing. For example, the
         * input field type may have changed, or the current composition may have been committed
         * outside of the input method.
         */
        int RESTART_REASON_CONTENT_CHANGE = 2;

        /**
         * Reset the input method, and discard any existing states such as the current composition
         * or current autocompletion. Because the current focused editor may have changed, as
         * part of the reset, a custom input method would normally call {@link
         * #onCreateInputConnection} to update its knowledge of the focused editor. Note that
         * {@code restartInput} should be used to detect changes in focus, rather than {@link
         * #showSoftInput} or {@link #hideSoftInput}, because focus changes are not always
         * accompanied by requests to show or hide the soft input.
         *
         * @param reason Reason for the reset.
         */
        void restartInput(@RestartReason int reason);

        /**
         * Display the soft input.
         *
         * @see #hideSoftInput
         * */
        void showSoftInput();

        /**
         * Hide the soft input.
         *
         * @see #showSoftInput
         * */
        void hideSoftInput();

        /**
         * Update the soft input on the current selection.
         *
         * @param selStart Start offset of the selection.
         * @param selEnd End offset of the selection.
         * @param compositionStart Composition start offset, or -1 if there is no composition.
         * @param compositionEnd Composition end offset, or -1 if there is no composition.
         */
        void updateSelection(int selStart, int selEnd, int compositionStart, int compositionEnd);

        /**
         * Update the soft input on the current extracted text as requested through
         * InputConnection.getExtractText.
         *
         * @param request The extract text request.
         * @param text The extracted text.
         */
        void updateExtractedText(@NonNull ExtractedTextRequest request,
                                 @NonNull ExtractedText text);

        /**
         * Update the cursor-anchor information as requested through
         * InputConnection.requestCursorUpdates.
         *
         * @param info Cursor-anchor information.
         */
        void updateCursorAnchorInfo(@NonNull CursorAnchorInfo info);
    }

    // Interface to access GeckoInputConnection from SessionTextInput.
    /* package */ interface InputConnectionClient {
        View getView();
        Handler getHandler(Handler defHandler);
        InputConnection onCreateInputConnection(EditorInfo attrs);
        boolean isInputActive();
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

        void sendKeyEvent(@Nullable View view, boolean inputActive, int action,
                          @NonNull KeyEvent event);
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
        void onDefaultKeyEvent(KeyEvent event);
        void updateCompositionRects(final RectF[] aRects);
    }

    private final class DefaultDelegate implements Delegate {
        private InputMethodManager getInputMethodManager(@Nullable final View view) {
            if (view == null) {
                return null;
            }
            return (InputMethodManager) view.getContext()
                                            .getSystemService(Context.INPUT_METHOD_SERVICE);
        }

        @Override
        public void restartInput(int reason) {
            ThreadUtils.assertOnUiThread();
            final View view = getView();
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
        public void showSoftInput() {
            ThreadUtils.assertOnUiThread();
            final View view = getView();
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
        public void hideSoftInput() {
            ThreadUtils.assertOnUiThread();
            final View view = getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
            }
        }

        @Override
        public void updateSelection(final int selStart, final int selEnd,
                                    final int compositionStart, final int compositionEnd) {
            ThreadUtils.assertOnUiThread();
            final View view = getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                imm.updateSelection(view, selStart, selEnd, compositionStart, compositionEnd);
            }
        }

        @Override
        public void updateExtractedText(@NonNull final ExtractedTextRequest request,
                                        @NonNull final ExtractedText text) {
            ThreadUtils.assertOnUiThread();
            final View view = getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                imm.updateExtractedText(view, request.token, text);
            }
        }

        @TargetApi(21)
        @Override
        public void updateCursorAnchorInfo(@NonNull final CursorAnchorInfo info) {
            ThreadUtils.assertOnUiThread();
            final View view = getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                imm.updateCursorAnchorInfo(view, info);
            }
        }
    }

    private final GeckoSession mSession;
    private final NativeQueue mQueue;
    private final GeckoEditable mEditable = new GeckoEditable();
    private final GeckoEditableChild mEditableChild = new GeckoEditableChild(mEditable);
    private InputConnectionClient mInputConnection;
    private Delegate mDelegate;

    /* package */ SessionTextInput(final @NonNull GeckoSession session,
                                   final @NonNull NativeQueue queue) {
        mSession = session;
        mQueue = queue;
        mEditable.setDefaultEditableChild(mEditableChild);
    }

    /* package */ void onWindowChanged(final GeckoSession.Window window) {
        if (mQueue.isReady()) {
            window.attachEditable(mEditable, mEditableChild);
        } else {
            mQueue.queueUntilReady(window, "attachEditable",
                                   IGeckoEditableParent.class, mEditable,
                                   GeckoEditableChild.class, mEditableChild);
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
    public synchronized @NonNull Handler getHandler(final @NonNull Handler defHandler) {
        // May be called on any thread.
        if (mInputConnection != null) {
            return mInputConnection.getHandler(defHandler);
        }
        return defHandler;
    }

    /**
     * Get the current View for text input.
     *
     * @return Current text input View or null if not set.
     * @see #setView(View)
     */
    public @Nullable View getView() {
        ThreadUtils.assertOnUiThread();
        return mInputConnection != null ? mInputConnection.getView() : null;
    }

    /**
     * Set the View for text input. The current View is used to interact with the system
     * input method manager and to display certain text input UI elements.
     *
     * @param view Text input View or null to clear current View.
     */
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
     * Get an InputConnection instance. For full functionality, call {@link
     * #setView(View)} first before calling this method.
     *
     * @param attrs EditorInfo instance to be filled on return.
     * @return InputConnection instance or null if input method is not active.
     */
    public synchronized @Nullable InputConnection onCreateInputConnection(
            final @NonNull EditorInfo attrs) {
        // May be called on any thread.
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
    public boolean onKeyPreIme(final int keyCode, final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyPreIme(getView(), isInputActive(), keyCode, event);
    }

    /**
     * Process a KeyEvent as a key-down event.
     *
     * @param keyCode Key code.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    public boolean onKeyDown(final int keyCode, final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyDown(getView(), isInputActive(), keyCode, event);
    }

    /**
     * Process a KeyEvent as a key-up event.
     *
     * @param keyCode Key code.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    public boolean onKeyUp(final int keyCode, final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyUp(getView(), isInputActive(), keyCode, event);
    }

    /**
     * Process a KeyEvent as a long-press event.
     *
     * @param keyCode Key code.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    public boolean onKeyLongPress(final int keyCode, final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyLongPress(getView(), isInputActive(), keyCode, event);
    }

    /**
     * Process a KeyEvent as a multiple-press event.
     *
     * @param keyCode Key code.
     * @param repeatCount Key repeat count.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    public boolean onKeyMultiple(final int keyCode, final int repeatCount,
                                 final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyMultiple(getView(), isInputActive(), keyCode, repeatCount, event);
    }

    /**
     * Return whether there is an active input connection, usually as a result of a
     * focused input field.
     *
     * @return True if input is active.
     */
    public boolean isInputActive() {
        ThreadUtils.assertOnUiThread();
        return mInputConnection != null && mInputConnection.isInputActive();
    }

    /**
     * Set the current text input delegate.
     *
     * @param delegate Delegate instance or null to restore to default.
     */
    public void setDelegate(@Nullable final Delegate delegate) {
        ThreadUtils.assertOnUiThread();
        mDelegate = delegate;
    }

    /**
     * Get the current text input delegate.
     *
     * @return Delegate instance or a default instance if no delegate has been set.
     */
    public Delegate getDelegate() {
        ThreadUtils.assertOnUiThread();
        if (mDelegate == null) {
            mDelegate = new DefaultDelegate();
        }
        return mDelegate;
    }
}
