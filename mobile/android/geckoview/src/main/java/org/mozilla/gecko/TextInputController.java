/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ThreadUtils;

import android.os.Handler;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
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

    private final GeckoSession mSession;
    private final GeckoEditable mEditable = new GeckoEditable();
    private final GeckoEditableChild mEditableChild = new GeckoEditableChild(mEditable);
    private Delegate mInputConnection;

    /* package */ TextInputController(final @NonNull GeckoSession session) {
        mSession = session;
        mEditable.setDefaultEditableChild(mEditableChild);
    }

    /* package */ void onWindowReady(final NativeQueue queue,
                                     final GeckoSession.Window window) {
        if (queue.isReady()) {
            window.attachEditable(mEditable, mEditableChild);
        } else {
            queue.queueUntilReady(window, "attachEditable",
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
     * For example: <pre>{@code
     * @Override
     * public Handler getHandler() {
     *     if (Build.VERSION.SDK_INT >= 24) {
     *         return super.getHandler();
     *     }
     *     return getSession().getTextInputController().getHandler(super.getHandler());
     * }
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

    private synchronized void ensureInputConnection() {
        if (mInputConnection == null) {
            mInputConnection = GeckoInputConnection.create(mSession,
                                                           /* view */ null,
                                                           mEditable);
            mEditable.setListener((GeckoEditableListener) mInputConnection);
        }
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
        mEditable.setListener((GeckoEditableListener) mInputConnection);
    }

    /**
     * Get an InputConnection instance. For full functionality, call {@link
     * #setView(View)} first before calling this method.
     *
     * @param outAttrs EditorInfo instance to be filled on return.
     * @return InputConnection instance or null if input method is not active.
     */
    public synchronized @Nullable InputConnection onCreateInputConnection(
            final @NonNull EditorInfo attrs) {
        // May be called on any thread.
        ensureInputConnection();
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
        ensureInputConnection();
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
        ensureInputConnection();
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
        ensureInputConnection();
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
        ensureInputConnection();
        return mInputConnection.onKeyLongPress(keyCode, event);
    }

    /**
     * Process a KeyEvent as a multiple-press event.
     *
     * @param keyCode Key code.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    public boolean onKeyMultiple(final int keyCode, final int repeatCount,
                                 final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        ensureInputConnection();
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
