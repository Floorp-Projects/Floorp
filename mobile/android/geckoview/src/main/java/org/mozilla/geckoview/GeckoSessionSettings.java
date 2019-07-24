/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.util.GeckoBundle;

import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import java.util.Arrays;
import java.util.Collection;

@AnyThread
public final class GeckoSessionSettings implements Parcelable {

    /**
     * Settings builder used to construct the settings object.
     */
    @AnyThread
    public static final class Builder {
        private final GeckoSessionSettings mSettings;

        public Builder() {
            mSettings = new GeckoSessionSettings();
        }

        public Builder(final GeckoSessionSettings settings) {
            mSettings = new GeckoSessionSettings(settings);
        }

        /**
         * Finalize and return the settings.
         *
         * @return The constructed settings.
         */
        public @NonNull GeckoSessionSettings build() {
            return new GeckoSessionSettings(mSettings);
        }

        /**
         * Set the chrome URI.
         *
         * @param uri The URI to set the Chrome URI to.
         * @return This Builder instance.

         */
        public @NonNull Builder chromeUri(final @NonNull String uri) {
            mSettings.setChromeUri(uri);
            return this;
        }

        /**
         * Set the screen id.
         *
         * @param id The screen id.
         * @return This Builder instance.
         */
        public @NonNull Builder screenId(final int id) {
            mSettings.setScreenId(id);
            return this;
        }

        /**
         * Set the privacy mode for this instance.
         *
         * @param flag A flag determining whether Private Mode should be enabled.
         *             Default is false.
         * @return This Builder instance.
         */
        public @NonNull Builder usePrivateMode(final boolean flag) {
            mSettings.setUsePrivateMode(flag);
            return this;
        }

        /**
         * Set the session context ID for this instance.
         * Setting a context ID partitions the cookie jars based on the provided
         * IDs. This isolates the browser storage like cookies and localStorage
         * between sessions, only sessions that share the same ID share storage
         * data.
         *
         * Warning: Storage data is collected persistently for each context,
         * to delete context data, call {@link StorageController#clearDataForSessionContext}
         * for the given context.
         *
         * @param value The custom context ID.
         *              The default ID is null, which removes isolation for this
         *              instance.
         * @return This Builder instance.
         */
        public @NonNull Builder contextId(final @Nullable String value) {
            mSettings.setContextId(value);
            return this;
        }

        /**
         * Set whether multi-process support should be enabled.
         *
         * @param flag A flag determining whether multi-process should be enabled.
         *             Default is false.
         * @return This Builder instance.
         */
        public @NonNull Builder useMultiprocess(final boolean flag) {
            mSettings.setUseMultiprocess(flag);
            return this;
        }

        /**
         * Set whether tracking protection should be enabled.
         *
         * @param flag A flag determining whether tracking protection should be enabled.
         *             Default is false.
         * @return This Builder instance.
         */
        public @NonNull Builder useTrackingProtection(final boolean flag) {
            mSettings.setUseTrackingProtection(flag);
            return this;
        }

        /**
         * Set the user agent mode.
         *
         * @param mode The mode to set the user agent to.
         *             Use one or more of the
         *             {@link GeckoSessionSettings#USER_AGENT_MODE_MOBILE GeckoSessionSettings.USER_AGENT_MODE_*} flags.
         * @return This Builder instance.
         */
        public @NonNull Builder userAgentMode(final int mode) {
            mSettings.setUserAgentMode(mode);
            return this;
        }

        /**
         * Override the user agent.
         *
         * @param agent The user agent to use.
         * @return This Builder instance.

         */
        public @NonNull Builder userAgentOverride(final @NonNull String agent) {
            mSettings.setUserAgentOverride(agent);
            return this;
        }

        /**
         * Specify which display-mode to use.
         *
         * @param mode The mode to set the display to.
         *             Use one or more of the
         *             {@link GeckoSessionSettings#DISPLAY_MODE_BROWSER GeckoSessionSettings.DISPLAY_MODE_*} flags.
         * @return This Builder instance.
         */
        public @NonNull Builder displayMode(final int mode) {
            mSettings.setDisplayMode(mode);
            return this;
        }

        /**
         * Set whether to suspend the playing of media when the session is inactive.
         *
         * @param flag A flag determining whether media should be suspended.
         *             Default is false.
         * @return This Builder instance.
         */
        public @NonNull Builder suspendMediaWhenInactive(final boolean flag) {
            mSettings.setSuspendMediaWhenInactive(flag);
            return this;
        }

        /**
         * Set whether JavaScript support should be enabled.
         *
         * @param flag A flag determining whether JavaScript should be enabled.
         *             Default is true.
         * @return This Builder instance.
         */
        public @NonNull Builder allowJavascript(final boolean flag) {
            mSettings.setAllowJavascript(flag);
            return this;
        }

        /**
         * Set whether the entire accessible tree should be exposed with no caching.
         *
         * @param flag A flag determining if the entire accessible tree should be exposed.
         *             Default is false.
         * @return This Builder instance.
         */
        public @NonNull Builder fullAccessibilityTree(final boolean flag) {
            mSettings.setFullAccessibilityTree(flag);
            return this;
        }


        /**
         * Specify which viewport mode to use.
         *
         * @param mode The mode to set the viewport to.
         *             Use one or more of the
         *             {@link GeckoSessionSettings#VIEWPORT_MODE_MOBILE GeckoSessionSettings.VIEWPORT_MODE_*} flags.
         * @return This Builder instance.
         */
        public @NonNull Builder viewportMode(final int mode) {
            mSettings.setViewportMode(mode);
            return this;
        }
    }

    private static final String LOGTAG = "GeckoSessionSettings";
    private static final boolean DEBUG = false;

    // This needs to match nsIDocShell.idl
    public static final int DISPLAY_MODE_BROWSER = 0;
    public static final int DISPLAY_MODE_MINIMAL_UI = 1;
    public static final int DISPLAY_MODE_STANDALONE = 2;
    public static final int DISPLAY_MODE_FULLSCREEN = 3;

    // This needs to match GeckoViewSettingsChild.js and GeckoViewSettings.jsm
    public static final int USER_AGENT_MODE_MOBILE = 0;
    public static final int USER_AGENT_MODE_DESKTOP = 1;
    public static final int USER_AGENT_MODE_VR = 2;

    // This needs to match GeckoViewSettingsChild.js
    /**
     * Mobile-friendly pages will be rendered using a viewport based on their &lt;meta&gt; viewport
     * tag. All other pages will be rendered using a special desktop mode viewport, which has a
     * width of 980 CSS px.
     */
    public static final int VIEWPORT_MODE_MOBILE = 0;

    /**
     * All pages will be rendered using the special desktop mode viewport, which has a width of
     * 980 CSS px, regardless of whether the page has a &lt;meta&gt; viewport tag specified or not.
     */
    public static final int VIEWPORT_MODE_DESKTOP = 1;

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
    private static final Key<String> CHROME_URI =
        new Key<String>("chromeUri", /* initOnly */ true, /* values */ null);
    /**
     * Key to set the window screen ID, or 0 to use default ID.
     * Read-only once session is open.
     */
    private static final Key<Integer> SCREEN_ID =
        new Key<Integer>("screenId", /* initOnly */ true, /* values */ null);

    /**
     * Key to enable and disable tracking protection.
     */
    private static final Key<Boolean> USE_TRACKING_PROTECTION =
        new Key<Boolean>("useTrackingProtection");
    /**
     * Key to enable and disable private mode browsing.
     * Read-only once session is open.
     */
    private static final Key<Boolean> USE_PRIVATE_MODE =
        new Key<Boolean>("usePrivateMode", /* initOnly */ true, /* values */ null);

    /**
     * Key to enable and disable multiprocess browsing (e10s).
     * Read-only once session is open.
     */
    private static final Key<Boolean> USE_MULTIPROCESS =
        new Key<Boolean>("useMultiprocess", /* initOnly */ true, /* values */ null);

    /**
     * Key to specify which user agent mode we should use.
     */
    private static final Key<Integer> USER_AGENT_MODE =
        new Key<Integer>("userAgentMode", /* initOnly */ false,
                         Arrays.asList(USER_AGENT_MODE_MOBILE, USER_AGENT_MODE_DESKTOP, USER_AGENT_MODE_VR));

    /**
     * Key to specify the user agent override string.
     * Set value to null to use the user agent specified by USER_AGENT_MODE.
     */
    private static final Key<String> USER_AGENT_OVERRIDE =
        new Key<String>("userAgentOverride", /* initOnly */ false, /* values */ null);

    /**
     * Key to specify which viewport mode we should use.
     */
    private static final Key<Integer> VIEWPORT_MODE =
        new Key<Integer>("viewportMode", /* initOnly */ false,
                         Arrays.asList(VIEWPORT_MODE_MOBILE, VIEWPORT_MODE_DESKTOP));

    /**
     * Key to specify which display-mode we should use.
     */
    private static final Key<Integer> DISPLAY_MODE =
        new Key<Integer>("displayMode", /* initOnly */ false,
                         Arrays.asList(DISPLAY_MODE_BROWSER, DISPLAY_MODE_MINIMAL_UI,
                                       DISPLAY_MODE_STANDALONE, DISPLAY_MODE_FULLSCREEN));

    /**
     * Key to specify if media should be suspended when the session is inactive.
     */
    private static final Key<Boolean> SUSPEND_MEDIA_WHEN_INACTIVE =
        new Key<Boolean>("suspendMediaWhenInactive", /* initOnly */ false, /* values */ null);

    /**
     * Key to specify if JavaScript should be allowed on this session.
     */
    private static final Key<Boolean> ALLOW_JAVASCRIPT =
            new Key<Boolean>("allowJavascript", /* initOnly */ false, /* values */ null);
    /**
     * Key to specify if entire accessible tree should be exposed with no caching.
     */
    private static final Key<Boolean> FULL_ACCESSIBILITY_TREE =
            new Key<Boolean>("fullAccessibilityTree", /* initOnly */ false, /* values */ null);

    /**
     * Internal Gecko key to specify the session context ID.
     * Derived from `UNSAFE_CONTEXT_ID`.
     */
    private static final Key<String> CONTEXT_ID =
        new Key<String>("sessionContextId", /* initOnly */ true, /* values */ null);

    /**
     * User-provided key to specify the session context ID.
     */
    private static final Key<String> UNSAFE_CONTEXT_ID =
        new Key<String>("unsafeSessionContextId", /* initOnly */ true, /* values */ null);

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
        mBundle.putBoolean(ALLOW_JAVASCRIPT.name, true);
        mBundle.putBoolean(FULL_ACCESSIBILITY_TREE.name, false);
        mBundle.putInt(USER_AGENT_MODE.name, USER_AGENT_MODE_MOBILE);
        mBundle.putString(USER_AGENT_OVERRIDE.name, null);
        mBundle.putInt(VIEWPORT_MODE.name, VIEWPORT_MODE_MOBILE);
        mBundle.putInt(DISPLAY_MODE.name, DISPLAY_MODE_BROWSER);
        mBundle.putString(CONTEXT_ID.name, null);
        mBundle.putString(UNSAFE_CONTEXT_ID.name, null);
    }

    /**
     * Set whether tracking protection should be enabled.
     *
     * @param value A flag determining whether tracking protection should be enabled.
     *             Default is false.
     */
    public void setUseTrackingProtection(final boolean value) {
        setBoolean(USE_TRACKING_PROTECTION, value);
    }

    /**
     * Set the privacy mode for this instance.
     *
     * @param value A flag determining whether Private Mode should be enabled.
     *             Default is false.
     */
    private void setUsePrivateMode(final boolean value) {
        setBoolean(USE_PRIVATE_MODE, value);
    }


    /**
     * Set whether multi-process support should be enabled.
     *
     * @param value A flag determining whether multi-process should be enabled.
     *             Default is false.
     */
    private void setUseMultiprocess(final boolean value) {
        setBoolean(USE_MULTIPROCESS, value);
    }

    /**
     * Set whether to suspend the playing of media when the session is inactive.
     *
     * @param value A flag determining whether media should be suspended.
     *             Default is false.
     */
    public void setSuspendMediaWhenInactive(final boolean value) {
        setBoolean(SUSPEND_MEDIA_WHEN_INACTIVE, value);
    }


    /**
     * Set whether JavaScript support should be enabled.
     *
     * @param value A flag determining whether JavaScript should be enabled.
     *             Default is true.
     */
    public void setAllowJavascript(final boolean value) {
        setBoolean(ALLOW_JAVASCRIPT, value);
    }


    /**
     * Set whether the entire accessible tree should be exposed with no caching.
     *
     * @param value A flag determining full accessibility tree should be exposed.
     *             Default is false.
     */
    public void setFullAccessibilityTree(final boolean value) {
        setBoolean(FULL_ACCESSIBILITY_TREE, value);
    }

    private void setBoolean(final Key<Boolean> key, final boolean value) {
        synchronized (mBundle) {
            if (valueChangedLocked(key, value)) {
                mBundle.putBoolean(key.name, value);
                dispatchUpdate();
            }
        }
    }

    /**
     * Whether tracking protection is enabled.
     *
     * @return true if tracking protection is enabled, false if not.
     */
    public boolean getUseTrackingProtection() {
        return getBoolean(USE_TRACKING_PROTECTION);
    }

    /**
     * Whether private mode is enabled.
     *
     * @return true if private mode is enabled, false if not.
     */
    public boolean getUsePrivateMode() {
        return getBoolean(USE_PRIVATE_MODE);
    }

    /**
     * The context ID for this session.
     *
     * @return The context ID for this session.
     */
    public @Nullable String getContextId() {
        // Return the user-provided unsafe string.
        return getString(UNSAFE_CONTEXT_ID);
    }

    /**
     * Whether multiprocess is enabled.
     *
     * @return true if multiprocess is enabled, false if not.
     */
    public boolean getUseMultiprocess() {
        return getBoolean(USE_MULTIPROCESS);
    }

    /**
     * Whether media will be suspended when the session is inactice.
     *
     * @return true if media will be suspended, false if not.
     */
    public boolean getSuspendMediaWhenInactive() {
        return getBoolean(SUSPEND_MEDIA_WHEN_INACTIVE);
    }

    /**
     * Whether javascript execution is allowed.
     *
     * @return true if javascript execution is allowed, false if not.
     */
    public boolean getAllowJavascript() {
        return getBoolean(ALLOW_JAVASCRIPT);
    }

    /**
     * Whether entire accessible tree is exposed with no caching.
     *
     * @return true if accessibility tree is exposed, false if not.
     */
    public boolean getFullAccessibilityTree() {
        return getBoolean(FULL_ACCESSIBILITY_TREE);
    }

    private boolean getBoolean(final Key<Boolean> key) {
        synchronized (mBundle) {
            return mBundle.getBoolean(key.name);
        }
    }

    /**
     * Set the screen id.
     *
     * @param value The screen id.
     */
    private void setScreenId(final int value) {
        setInt(SCREEN_ID, value);
    }


    /**
     * Specify which user agent mode we should use
     *
     * @param value One or more of the
     *             {@link GeckoSessionSettings#USER_AGENT_MODE_MOBILE GeckoSessionSettings.USER_AGENT_MODE_*} flags.
     */
    public void setUserAgentMode(final int value) {
        setInt(USER_AGENT_MODE, value);
    }


    /**
     * Set the display mode.
     *
     * @param value The mode to set the display to.
     *             Use one or more of the
     *             {@link GeckoSessionSettings#DISPLAY_MODE_BROWSER GeckoSessionSettings.DISPLAY_MODE_*} flags.
     */
    public void setDisplayMode(final int value) {
        setInt(DISPLAY_MODE, value);
    }


    /**
     * Specify which viewport mode we should use
     *
     * @param value One or more of the
     *             {@link GeckoSessionSettings#VIEWPORT_MODE_MOBILE GeckoSessionSettings.VIEWPORT_MODE_*} flags.
     */
    public void setViewportMode(final int value) {
        setInt(VIEWPORT_MODE, value);
    }

    private void setInt(final Key<Integer> key, final int value) {
        synchronized (mBundle) {
            if (valueChangedLocked(key, value)) {
                mBundle.putInt(key.name, value);
                dispatchUpdate();
            }
        }
    }

    /**
     * Set the window screen ID.
     * Read-only once session is open.
     * Use the {@link Builder} to set on session open.
     *
     * @return Key to set the window screen ID. 0 is the default ID.
     */
    public int getScreenId() {
        return getInt(SCREEN_ID);
    }

    /**
     * The current user agent Mode
     * @return One or more of the
     *      {@link GeckoSessionSettings#USER_AGENT_MODE_MOBILE GeckoSessionSettings.USER_AGENT_MODE_*} flags.
     */
    public int getUserAgentMode() {
        return getInt(USER_AGENT_MODE);
    }

    /**
     * The current display mode.
     * @return )One or more of the
     *      {@link GeckoSessionSettings#DISPLAY_MODE_BROWSER GeckoSessionSettings.DISPLAY_MODE_*} flags.
     */
    public int getDisplayMode() {
        return getInt(DISPLAY_MODE);
    }

    /**
     * The current viewport Mode
     * @return One or more of the
     *      {@link GeckoSessionSettings#VIEWPORT_MODE GeckoSessionSettings.VIEWPORT_MODE_*} flags.
     */
    public int getViewportMode() {
        return getInt(VIEWPORT_MODE);
    }

    private int getInt(final Key<Integer> key) {
        synchronized (mBundle) {
            return mBundle.getInt(key.name);
        }
    }

    /**
     * Set the chrome URI.
     *
     * @param value The URI to set the Chrome URI to.

     */
    private void setChromeUri(final @NonNull String value) {
        setString(CHROME_URI, value);
    }


    /**
     * Specify the user agent override string.
     * Set value to null to use the user agent specified by USER_AGENT_MODE.
     * @param value The string to override the user agent with.
     */
    public void setUserAgentOverride(final @Nullable String value) {
        setString(USER_AGENT_OVERRIDE, value);
    }

    private void setContextId(final @Nullable String value) {
        setString(UNSAFE_CONTEXT_ID, value);
        setString(CONTEXT_ID, StorageController.createSafeSessionContextId(value));
    }

    private void setString(final Key<String> key, final String value) {
        synchronized (mBundle) {
            if (valueChangedLocked(key, value)) {
                mBundle.putString(key.name, value);
                dispatchUpdate();
            }
        }
    }

    /**
     * Set the chrome window URI.
     * Read-only once session is open.
     * Use the {@link Builder} to set on session open.
     *
     * @return Key to set the chrome window URI, or null to use default URI.
     */
    public @Nullable String getChromeUri() {
        return getString(CHROME_URI);
    }

    /**
     * The user agent override string.
     * @return The current user agent string or null if the agent is specified by
     *          {@link GeckoSessionSettings#USER_AGENT_MODE}
     */
    public @Nullable String getUserAgentOverride() {
        return getString(USER_AGENT_OVERRIDE);
    }

    private String getString(final Key<String> key) {
        synchronized (mBundle) {
            return mBundle.getString(key.name);
        }
    }

    /* package */ @NonNull GeckoBundle toBundle() {
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

    private <T> boolean valueChangedLocked(final Key<T> key, final T value) {
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
    public void writeToParcel(final @NonNull Parcel out, final int flags) {
        mBundle.writeToParcel(out, flags);
    }

    // AIDL code may call readFromParcel even though it's not part of Parcelable.
    public void readFromParcel(final @NonNull Parcel source) {
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
