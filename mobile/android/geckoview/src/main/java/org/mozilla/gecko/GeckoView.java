/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.LayerView;

import android.content.Context;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

public class GeckoView extends LayerView {
    private static final String LOGTAG = "GeckoView";
    private static final boolean DEBUG = false;

    protected GeckoSession mSession;
    private boolean mStateSaved;

    private InputConnectionListener mInputConnectionListener;
    private boolean mIsResettingFocus;

    private static class SavedState extends BaseSavedState {
        public final GeckoSession session;

        public SavedState(final Parcelable superState, final GeckoSession session) {
            super(superState);
            this.session = session;
        }

        /* package */ SavedState(final Parcel in) {
            super(in);
            session = in.readParcelable(getClass().getClassLoader());
        }

        @Override // BaseSavedState
        public void writeToParcel(final Parcel dest, final int flags) {
            super.writeToParcel(dest, flags);
            dest.writeParcelable(session, flags);
        }

        public static final Creator<SavedState> CREATOR = new Creator<SavedState>() {
            @Override
            public SavedState createFromParcel(final Parcel in) {
                return new SavedState(in);
            }

            @Override
            public SavedState[] newArray(final int size) {
                return new SavedState[size];
            }
        };
    }

    public GeckoView(Context context) {
        super(context);
        initializeView();
    }

    public GeckoView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initializeView();
    }

    public void setSession(final GeckoSession session) {
        if (mSession != null && mSession.isOpen()) {
            throw new IllegalStateException("Current session is open");
        }
        mSession = session;
    }

    public GeckoSession getSession() {
        return mSession;
    }

    public EventDispatcher getEventDispatcher() {
        return mSession.getEventDispatcher();
    }

    public GeckoSessionSettings getSettings() {
        return mSession.getSettings();
    }

    protected Object getCompositor() {
        return super.getCompositor();
    }

    @Override
    public void onAttachedToWindow() {
        if (mSession == null) {
            mSession = new GeckoSession();
        }

        if (!mSession.isOpen()) {
            mSession.openWindow(getContext().getApplicationContext());
        }
        mSession.attachView(this);

        super.onAttachedToWindow();
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        super.destroy();

        if (mStateSaved) {
            // If we saved state earlier, we don't want to close the window.
            return;
        }

        if (mSession != null && mSession.isOpen()) {
            mSession.closeWindow();
        }
    }

    @Override
    protected Parcelable onSaveInstanceState() {
        mStateSaved = true;
        return new SavedState(super.onSaveInstanceState(), mSession);
    }

    @Override
    protected void onRestoreInstanceState(final Parcelable state) {
        mStateSaved = false;

        if (!(state instanceof SavedState)) {
            super.onRestoreInstanceState(state);
            return;
        }

        final SavedState ss = (SavedState) state;
        super.onRestoreInstanceState(ss.getSuperState());

        if (mSession == null) {
            mSession = ss.session;
        } else if (ss.session != null) {
            mSession.transferFrom(ss.session);
        }
    }

    /* package */ void setInputConnectionListener(final InputConnectionListener icl) {
        mInputConnectionListener = icl;
    }

    @Override
    public void onFocusChanged(boolean gainFocus, int direction, Rect previouslyFocusedRect) {
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);

        if (!gainFocus || mIsResettingFocus) {
            return;
        }

        post(new Runnable() {
            @Override
            public void run() {
                if (!isFocused()) {
                    return;
                }

                final InputMethodManager imm = InputMethods.getInputMethodManager(getContext());
                // Bug 1404111: Through View#onFocusChanged, the InputMethodManager queues
                // up a checkFocus call for the next spin of the message loop, so by
                // posting this Runnable after super#onFocusChanged, the IMM should have
                // completed its focus change handling at this point and we should be the
                // active view for input handling.

                // If however onViewDetachedFromWindow for the previously active view gets
                // called *after* onFocusChanged, but *before* the focus change has been
                // fully processed by the IMM with the help of checkFocus, the IMM will
                // lose track of the currently active view, which means that we can't
                // interact with the IME.
                if (!imm.isActive(GeckoView.this)) {
                    // If that happens, we bring the IMM's internal state back into sync
                    // by clearing and resetting our focus.
                    mIsResettingFocus = true;
                    clearFocus();
                    // After calling clearFocus we might regain focus automatically, but
                    // we explicitly request it again in case this doesn't happen.  If
                    // we've already got the focus back, this will then be a no-op anyway.
                    requestFocus();
                    mIsResettingFocus = false;
                }
            }
        });
    }

    @Override
    public Handler getHandler() {
        if (mInputConnectionListener != null) {
            return mInputConnectionListener.getHandler(super.getHandler());
        }
        return super.getHandler();
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        if (mInputConnectionListener != null) {
            return mInputConnectionListener.onCreateInputConnection(outAttrs);
        }
        return null;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (super.onKeyPreIme(keyCode, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyPreIme(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (super.onKeyUp(keyCode, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyUp(keyCode, event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (super.onKeyDown(keyCode, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        if (super.onKeyLongPress(keyCode, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyLongPress(keyCode, event);
    }

    @Override
    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        if (super.onKeyMultiple(keyCode, repeatCount, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyMultiple(keyCode, repeatCount, event);
    }

    @Override
    public boolean isIMEEnabled() {
        return mInputConnectionListener != null &&
                mInputConnectionListener.isIMEEnabled();
    }
}
