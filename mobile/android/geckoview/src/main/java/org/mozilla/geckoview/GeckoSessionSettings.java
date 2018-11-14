/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.util.GeckoBundle;

import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;

import java.util.Arrays;
import java.util.Collection;

public final class GeckoSessionSettings implements Parcelable {
    private static final String LOGTAG = "GeckoSessionSettings";
    private static final boolean DEBUG = false;

    // This needs to match nsIDocShell.idl
    public static final int DISPLAY_MODE_BROWSER = 0;
    public static final int DISPLAY_MODE_MINIMAL_UI = 1;
    public static final int DISPLAY_MODE_STANDALONE = 2;
    public static final int DISPLAY_MODE_FULLSCREEN = 3;

    // This needs to match GeckoViewContentSettings.js and GeckoViewSettings.jsm
    public static final int USER_AGENT_MODE_MOBILE = 0;
    public static final int USER_AGENT_MODE_DESKTOP = 1;
    public static final int USER_AGENT_MODE_VR = 2;

    public static class Key<T> {
        /* package */ final String name;
        /* package */ final boolean initOnly;
        /* package */ final Collection<T> values;

        /* package */ Key(final String name) {
            this(name, /* initOnly */ false, /* values */ null);
        }

        /* package */ Key(final String name, final boolean initOnly,
                          final Collection<T> values) {
            this.name = name;
            this.initOnly = initOnly;
            this.values = values;
        }
    }

    /**
     * Key to set the chrome window URI, or null to use default URI.
     * Read-only once session is open.
     */
    public static final Key<String> CHROME_URI =
        new Key<String>("chromeUri", /* initOnly */ true, /* values */ null);
    /**
     * Key to set the window screen ID, or 0 to use default ID.
     * Read-only once session is open.
     */
    public static final Key<Integer> SCREEN_ID =
        new Key<Integer>("screenId", /* initOnly */ true, /* values */ null);

    /*
     * Key to enable and disable tracking protection.
     */
    public static final Key<Boolean> USE_TRACKING_PROTECTION =
        new Key<Boolean>("useTrackingProtection");
    /*
     * Key to enable and disable private mode browsing.
     * Read-only once session is open.
     */
    public static final Key<Boolean> USE_PRIVATE_MODE =
        new Key<Boolean>("usePrivateMode", /* initOnly */ true, /* values */ null);

    /*
     * Key to enable and disable multiprocess browsing (e10s).
     * Read-only once session is open.
     */
    public static final Key<Boolean> USE_MULTIPROCESS =
        new Key<Boolean>("useMultiprocess", /* initOnly */ true, /* values */ null);

    /*
     * Key to specify which user agent mode we should use.
     */
    public static final Key<Integer> USER_AGENT_MODE =
        new Key<Integer>("userAgentMode", /* initOnly */ false,
                         Arrays.asList(USER_AGENT_MODE_MOBILE, USER_AGENT_MODE_DESKTOP, USER_AGENT_MODE_VR));

    /*
     * Key to specify which display-mode we should use.
     */
    public static final Key<Integer> DISPLAY_MODE =
        new Key<Integer>("displayMode", /* initOnly */ false,
                         Arrays.asList(DISPLAY_MODE_BROWSER, DISPLAY_MODE_MINIMAL_UI,
                                       DISPLAY_MODE_STANDALONE, DISPLAY_MODE_FULLSCREEN));

    /*
     * Key to specify if media should be suspended when the session is inactive.
     */
    public static final Key<Boolean> SUSPEND_MEDIA_WHEN_INACTIVE =
        new Key<Boolean>("suspendMediaWhenInactive", /* initOnly */ false, /* values */ null);

    /*
     * Key to specify if entire accessible tree should be exposed with no caching.
     */
    public static final Key<Boolean> FULL_ACCESSIBILITY_TREE =
            new Key<Boolean>("fullAccessibilityTree", /* initOnly */ false, /* values */ null);

    private final GeckoSession mSession;
    private final GeckoBundle mBundle;

    public GeckoSessionSettings() {
        this(null, null);
    }

    public GeckoSessionSettings(final @NonNull GeckoSessionSettings settings) {
        this(settings, null);
    }

    /* package */ GeckoSessionSettings(final @Nullable GeckoSessionSettings settings,
                                       final @Nullable GeckoSession session) {
        mSession = session;

        if (settings != null) {
            mBundle = new GeckoBundle(settings.mBundle);
            return;
        }

        mBundle = new GeckoBundle();
        mBundle.putString(CHROME_URI.name, null);
        mBundle.putInt(SCREEN_ID.name, 0);
        mBundle.putBoolean(USE_TRACKING_PROTECTION.name, false);
        mBundle.putBoolean(USE_PRIVATE_MODE.name, false);
        mBundle.putBoolean(USE_MULTIPROCESS.name, true);
        mBundle.putBoolean(SUSPEND_MEDIA_WHEN_INACTIVE.name, false);
        mBundle.putBoolean(FULL_ACCESSIBILITY_TREE.name, false);
        mBundle.putInt(USER_AGENT_MODE.name, USER_AGENT_MODE_MOBILE);
        mBundle.putInt(DISPLAY_MODE.name, DISPLAY_MODE_BROWSER);
    }

    public void setBoolean(final Key<Boolean> key, final boolean value) {
        synchronized (mBundle) {
            if (valueChangedLocked(key, value)) {
                mBundle.putBoolean(key.name, value);
                dispatchUpdate();
            }
        }
    }

    public boolean getBoolean(final Key<Boolean> key) {
        synchronized (mBundle) {
            return mBundle.getBoolean(key.name);
        }
    }

    public void setInt(final Key<Integer> key, final int value) {
        synchronized (mBundle) {
            if (valueChangedLocked(key, value)) {
                mBundle.putInt(key.name, value);
                dispatchUpdate();
            }
        }
    }

    public int getInt(final Key<Integer> key) {
        synchronized (mBundle) {
            return mBundle.getInt(key.name);
        }
    }

    public void setString(final Key<String> key, final String value) {
        synchronized (mBundle) {
            if (valueChangedLocked(key, value)) {
                mBundle.putString(key.name, value);
                dispatchUpdate();
            }
        }
    }

    public String getString(final Key<String> key) {
        synchronized (mBundle) {
            return mBundle.getString(key.name);
        }
    }

    /* package */ GeckoBundle toBundle() {
        return new GeckoBundle(mBundle);
    }

    @Override
    public String toString() {
        return mBundle.toString();
    }

    @Override
    public boolean equals(final Object other) {
        return other instanceof GeckoSessionSettings &&
                mBundle.equals(((GeckoSessionSettings) other).mBundle);
    }

    @Override
    public int hashCode() {
        return mBundle.hashCode();
    }

    private <T> boolean valueChangedLocked(final Key<T> key, T value) {
        if (key.initOnly && mSession != null && mSession.isOpen()) {
            throw new IllegalStateException("Read-only property");
        } else if (key.values != null && !key.values.contains(value)) {
            throw new IllegalArgumentException("Invalid value");
        }

        final Object old = mBundle.get(key.name);
        return (old != value) && (old == null || !old.equals(value));
    }

    private void dispatchUpdate() {
        if (mSession != null && mSession.isOpen()) {
            mSession.getEventDispatcher().dispatch("GeckoView:UpdateSettings", toBundle());
        }
    }

    @Override // Parcelable
    public int describeContents() {
        return 0;
    }

    @Override // Parcelable
    public void writeToParcel(Parcel out, int flags) {
        mBundle.writeToParcel(out, flags);
    }

    // AIDL code may call readFromParcel even though it's not part of Parcelable.
    public void readFromParcel(final Parcel source) {
        mBundle.readFromParcel(source);
    }

    public static final Parcelable.Creator<GeckoSessionSettings> CREATOR
            = new Parcelable.Creator<GeckoSessionSettings>() {
        @Override
        public GeckoSessionSettings createFromParcel(final Parcel in) {
            final GeckoSessionSettings settings = new GeckoSessionSettings();
            settings.readFromParcel(in);
            return settings;
        }

        @Override
        public GeckoSessionSettings[] newArray(final int size) {
            return new GeckoSessionSettings[size];
        }
    };
}
