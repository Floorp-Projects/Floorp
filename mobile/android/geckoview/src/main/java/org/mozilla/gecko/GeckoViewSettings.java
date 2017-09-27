/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.GeckoBundle;

import android.util.Log;

public final class GeckoViewSettings {
    private static final String LOGTAG = "GeckoViewSettings";
    private static final boolean DEBUG = false;

    private static class Key<T> {
        private final String text;

        public Key(final String text) {
            this.text = text;
        }
    }

    public enum DisplayMode {
        // This needs to match nsIDocShell.idl
        BROWSER(0),
        MINIMAL_UI(1),
        STANDALONE(2),
        FULLSCREEN(3);

        private final int mMode;

        DisplayMode(int mode) {
            mMode = mode;
        }

        public int value() {
            return mMode;
        }
    }

    /*
     * Key to enabled and disable tracking protection.
     */
    public static final Key<Boolean> USE_TRACKING_PROTECTION =
        new Key<Boolean>("useTrackingProtection");
    /*
     * Key to enabled and disable private mode browsing.
     */
    public static final Key<Boolean> USE_PRIVATE_MODE =
        new Key<Boolean>("usePrivateMode");

    /*
     * Key to enabled and disable multiprocess browsing (e10s).
     * Note: can only be set during GeckoView initialization, changes during an
     * active GeckoView session will be ignored.
     */
    public static final Key<Boolean> USE_MULTIPROCESS =
        new Key<Boolean>("useMultiprocess");

    /*
     * Key to specify which display-mode we should use
     */
    public static final Key<Integer> DISPLAY_MODE =
        new Key<Integer>("displayMode");

    public static final Key<Boolean> USE_REMOTE_DEBUGGER =
        new Key<Boolean>("useRemoteDebugger");

    public static final Key<String> DEBUGGER_SOCKET_DIR =
        new Key<String>("debuggerSocketDir");

    private final EventDispatcher mEventDispatcher;
    private final GeckoBundle mBundle;

    public GeckoViewSettings() {
        this(null);
    }

    /* package */ GeckoViewSettings(EventDispatcher eventDispatcher) {
        mEventDispatcher = eventDispatcher;
        mBundle = new GeckoBundle();

        setBoolean(USE_TRACKING_PROTECTION, false);
        setBoolean(USE_PRIVATE_MODE, false);
        setBoolean(USE_MULTIPROCESS, true);
        setInt(DISPLAY_MODE, DisplayMode.BROWSER.value());
        setBoolean(USE_REMOTE_DEBUGGER, false);
        // Set in GeckoView.init().
        setString(DEBUGGER_SOCKET_DIR, "");
    }

    /* package */ GeckoViewSettings(GeckoViewSettings settings, EventDispatcher eventDispatcher) {
        mBundle = new GeckoBundle(settings.mBundle);
        mEventDispatcher = eventDispatcher;
    }

    public void setBoolean(final Key<Boolean> key, boolean value) {
        synchronized (mBundle) {
            final Object old = mBundle.get(key.text);
            if (old != null && old.equals(value)) {
                return;
            }
            mBundle.putBoolean(key.text, value);
        }
        dispatchUpdate();
    }

    public boolean getBoolean(final Key<Boolean> key) {
        synchronized (mBundle) {
            return mBundle.getBoolean(key.text);
        }
    }

    public void setInt(final Key<Integer> key, int value) {
        synchronized (mBundle) {
            final Object old = mBundle.get(key.text);
            if (old != null && old.equals(value)) {
                return;
            }
            mBundle.putInt(key.text, value);
        }
        dispatchUpdate();
    }

    public int getInt(final Key<Integer> key) {
        synchronized (mBundle) {
            return mBundle.getInt(key.text);
        }
    }

    public void setString(final Key<String> key, final String value) {
        synchronized (mBundle) {
            final Object old = mBundle.get(key.text);
            if (old != null && old.equals(value)) {
                return;
            }
            mBundle.putString(key.text, value);
        }
        dispatchUpdate();
    }

    public String getString(final Key<String> key) {
        synchronized (mBundle) {
            return mBundle.getString(key.text);
        }
    }

    /* package */ GeckoBundle asBundle() {
        return mBundle;
    }

    private void dispatchUpdate() {
        if (mEventDispatcher != null) {
            mEventDispatcher.dispatch("GeckoView:UpdateSettings", null);
        }
    }
}
