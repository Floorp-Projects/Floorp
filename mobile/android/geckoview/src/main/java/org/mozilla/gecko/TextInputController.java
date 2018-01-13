/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.ThreadUtils;

import android.graphics.RectF;
import android.os.Handler;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.Editable;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

/**
 * TextInputController handles text input for GeckoSession through key events or input
 * methods. It is typically used to implement certain methods in View such as {@code
 * onCreateInputConnection()}, by forwarding such calls to corresponding methods in
 * TextInputController.
 */
public final class TextInputController {

    // Interface to access GeckoInputConnection from TextInputController.
    /* package */ interface Delegate {
        View getView();
        Handler getHandler(Handler defHandler);
        InputConnection onCreateInputConnection(EditorInfo attrs);
        boolean onKeyPreIme(int keyCode, KeyEvent event);
        boolean onKeyDown(int keyCode, KeyEvent event);
        boolean onKeyUp(int keyCode, KeyEvent event);
        boolean onKeyLongPress(int keyCode, KeyEvent event);
        boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event);
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

        void sendKeyEvent(KeyEvent event, int action, int metaState);
        Editable getEditable();
        void setBatchMode(boolean isBatchMode);
        void setSuppressKeyUp(boolean suppress);
        Handler setInputConnectionHandler(Handler handler);
        void postToInputConnection(Runnable runnable);
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

    private final GeckoSession mSession;
    private final NativeQueue mQueue;
    private final GeckoEditable mEditable = new GeckoEditable();
    private final GeckoEditableChild mEditableChild = new GeckoEditableChild(mEditable);
    private Delegate mInputConnection;

    /* package */ TextInputController(final @NonNull GeckoSession session,
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
     *     return getSession().getTextInputController().getHandler(super.getHandler());
     * }</pre>
     *
     * @param defHandler Handler returned by the system {@code getHandler} implementation.
     * @return Handler to return to the system through {@code getHandler}.
     */
    public @NonNull Handler getHandler(final @NonNull Handler defHandler) {
        // May be called on any thread.
        if (mInputConnection != null) {
            return mInputConnection.getHandler(defHandler);
        }
        return defHandler;
    }

    private synchronized boolean ensureInputConnection() {
        if (!mQueue.isReady()) {
            return false;
        }

        if (mInputConnection == null) {
            mInputConnection = GeckoInputConnection.create(mSession,
                                                           /* view */ null,
                                                           mEditable);
            mEditable.setListener((EditableListener) mInputConnection);
        }
        return true;
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
    public @Nullable InputConnection onCreateInputConnection(final @NonNull EditorInfo attrs) {
        // May be called on any thread.
        if (!ensureInputConnection()) {
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
        if (!ensureInputConnection()) {
            return false;
        }
        return mInputConnection.onKeyPreIme(keyCode, event);
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
        if (!ensureInputConnection()) {
            return false;
        }
        return mInputConnection.onKeyDown(keyCode, event);
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
        if (!ensureInputConnection()) {
            return false;
        }
        return mInputConnection.onKeyUp(keyCode, event);
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
        if (!ensureInputConnection()) {
            return false;
        }
        return mInputConnection.onKeyLongPress(keyCode, event);
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
        if (!ensureInputConnection()) {
            return false;
        }
        return mInputConnection.onKeyMultiple(keyCode, repeatCount, event);
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
}
