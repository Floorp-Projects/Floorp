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

    public static final Key<Boolean> USE_TRACKING_PROTECTION =
        new Key<Boolean>("useTrackingProtection");
    public static final Key<Boolean> USE_PRIVATE_MODE =
        new Key<Boolean>("usePrivateMode");

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

    /* package */ GeckoBundle asBundle() {
        return mBundle;
    }

    private void dispatchUpdate() {
        if (mEventDispatcher != null) {
            mEventDispatcher.dispatch("GeckoView:UpdateSettings", null);
        }
    }
}
