/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.mozilla.geckoview.GeckoSession.TrackingProtectionDelegate;

public final class GeckoRuntimeSettings implements Parcelable {
    /**
     * Settings builder used to construct the settings object.
     */
    public static final class Builder {
        private final GeckoRuntimeSettings mSettings;

        public Builder() {
            mSettings = new GeckoRuntimeSettings();
        }

        public Builder(final GeckoRuntimeSettings settings) {
            mSettings = new GeckoRuntimeSettings(settings);
        }

        /**
         * Finalize and return the settings.
         *
         * @return The constructed settings.
         */
        public @NonNull GeckoRuntimeSettings build() {
            return new GeckoRuntimeSettings(mSettings);
        }

        /**
         * Set the content process hint flag.
         *
         * @param use If true, this will reload the content process for future use.
         *            Default is false.
         * @return This Builder instance.

         */
        public @NonNull Builder useContentProcessHint(final boolean use) {
            mSettings.mUseContentProcess = use;
            return this;
        }

        /**
         * Set the custom Gecko process arguments.
         *
         * @param args The Gecko process arguments.
         * @return This Builder instance.
         */
        public @NonNull Builder arguments(final @NonNull String[] args) {
            if (args == null) {
                throw new IllegalArgumentException("Arguments must not  be null");
            }
            mSettings.mArgs = args;
            return this;
        }

        /**
         * Set the custom Gecko intent extras.
         *
         * @param extras The Gecko intent extras.
         * @return This Builder instance.
         */
        public @NonNull Builder extras(final @NonNull Bundle extras) {
            if (extras == null) {
                throw new IllegalArgumentException("Extras must not  be null");
            }
            mSettings.mExtras = extras;
            return this;
        }

        /**
         * Set whether JavaScript support should be enabled.
         *
         * @param flag A flag determining whether JavaScript should be enabled.
         *             Default is true.
         * @return This Builder instance.
         */
        public @NonNull Builder javaScriptEnabled(final boolean flag) {
            mSettings.mJavaScript.set(flag);
            return this;
        }

        /**
         * Set whether remote debugging support should be enabled.
         *
         * @param enabled True if remote debugging should be enabled.
         * @return This Builder instance.
         */
        public @NonNull Builder remoteDebuggingEnabled(final boolean enabled) {
            mSettings.mRemoteDebugging.set(enabled);
            return this;
        }

        /**
         * Set whether support for web fonts should be enabled.
         *
         * @param flag A flag determining whether web fonts should be enabled.
         *             Default is true.
         * @return This Builder instance.
         */
        public @NonNull Builder webFontsEnabled(final boolean flag) {
            mSettings.mWebFonts.set(flag);
            return this;
        }

        /**
         * Set whether crash reporting for native code should be enabled. This will cause
         * a SIGSEGV handler to be installed, and any crash encountered there will be
         * reported to Mozilla.
         *
         * @param enabled A flag determining whether native crash reporting should be enabled.
         *                Defaults to false.
         * @return This Builder.
         */
        public @NonNull Builder nativeCrashReportingEnabled(final boolean enabled) {
            mSettings.mNativeCrashReporting = enabled;
            return this;
        }

        /**
         * Set whether crash reporting for Java code should be enabled. This will cause
         * a default unhandled exception handler to be installed, and any exceptions encountered
         * will automatically reported to Mozilla.
         *
         * @param enabled A flag determining whether Java crash reporting should be enabled.
         *                Defaults to false.
         * @return This Builder.
         */
        public @NonNull Builder javaCrashReportingEnabled(final boolean enabled) {
            mSettings.mJavaCrashReporting = enabled;
            return this;
        }

        /**
         * Set whether there should be a pause during startup. This is useful if you need to
         * wait for a debugger to attach.
         *
         * @param enabled A flag determining whether there will be a pause early in startup.
         *                Defaults to false.
         * @return This Builder.
         */
        public @NonNull Builder pauseForDebugger(boolean enabled) {
            mSettings.mDebugPause = enabled;
            return this;
        }

        /**
         * Set cookie storage behavior.
         *
         * @param behavior The storage behavior that should be applied.
         *                 Use one of the {@link #COOKIE_ACCEPT_ALL COOKIE_ACCEPT_*} flags.
         * @return The Builder instance.
         */
        public @NonNull Builder cookieBehavior(@CookieBehavior int behavior) {
            mSettings.mCookieBehavior.set(behavior);
            return this;
        }

        /**
         * Set the cookie lifetime.
         *
         * @param lifetime The enforced cookie lifetime.
         *                 Use one of the {@link #COOKIE_LIFETIME_NORMAL COOKIE_LIFETIME_*} flags.
         *                 Use {@link #cookieLifetimeDays} to set expiration
         *                 days for {@link #COOKIE_LIFETIME_DAYS}.
         * @return The Builder instance.
         */
        public @NonNull Builder cookieLifetime(@CookieLifetime int lifetime) {
            mSettings.mCookieLifetime.set(lifetime);
            return this;
        }

        /**
         * Set the expiration days for {@link #COOKIE_LIFETIME_DAYS}.
         *
         * @param days Limit lifetime to N days. Only enforced for {@link #COOKIE_LIFETIME_DAYS}.
         * @return The Builder instance.
         */
        public @NonNull Builder cookieLifetimeDays(int days) {
            if (mSettings.mCookieLifetime.get() != COOKIE_LIFETIME_DAYS) {
                throw new IllegalStateException(
                    "Setting expiration days for incompatible cookie lifetime");
            }
            mSettings.mCookieLifetimeDays.set(days);
            return this;
        }

        /**
         * Set tracking protection blocking categories.
         *
         * @param categories The categories of trackers that should be blocked.
         *                   Use one or more of the
         *                   {@link TrackingProtectionDelegate#CATEGORY_AD TrackingProtectionDelegate.CATEGORY_*} flags.
         * @return This Builder instance.
         **/
        public @NonNull Builder trackingProtectionCategories(
                @TrackingProtectionDelegate.Category int categories) {
            mSettings.mTrackingProtection
                     .set(TrackingProtection.buildPrefValue(categories));
            return this;
        }

        /**
         * Set whether or not web console messages should go to logcat.
         *
         * Note: If enabled, Gecko performance may be negatively impacted if
         * content makes heavy use of the console API.
         *
         * @param enabled A flag determining whether or not web console messages should be
         *                printed to logcat.
         * @return The builder instance.
         */
        public @NonNull Builder consoleOutput(boolean enabled) {
            mSettings.mConsoleOutput.set(enabled);
            return this;
        }
    }

    /* package */ GeckoRuntime runtime;
    /* package */ boolean mUseContentProcess;
    /* package */ String[] mArgs;
    /* package */ Bundle mExtras;
    /* package */ int prefCount;

    private class Pref<T> {
        public final String name;
        public final T defaultValue;
        private T mValue;
        private boolean mIsSet;

        public Pref(final String name, final T defaultValue) {
            GeckoRuntimeSettings.this.prefCount++;

            this.name = name;
            this.defaultValue = defaultValue;
            mValue = defaultValue;
        }

        public void set(T newValue) {
            mValue = newValue;
            mIsSet = true;
            flush();
        }

        public T get() {
            return mValue;
        }

        public void flush() {
            if (GeckoRuntimeSettings.this.runtime != null) {
                GeckoRuntimeSettings.this.runtime.setPref(name, mValue, mIsSet);
            }
        }
    }

    /* package */ Pref<Boolean> mJavaScript = new Pref<Boolean>(
        "javascript.enabled", true);
    /* package */ Pref<Boolean> mRemoteDebugging = new Pref<Boolean>(
        "devtools.debugger.remote-enabled", false);
    /* package */ Pref<Boolean> mWebFonts = new Pref<Boolean>(
        "browser.display.use_document_fonts", true);
    /* package */ Pref<Integer> mCookieBehavior = new Pref<Integer>(
        "network.cookie.cookieBehavior", COOKIE_ACCEPT_ALL);
    /* package */ Pref<Integer> mCookieLifetime = new Pref<Integer>(
        "network.cookie.lifetimePolicy", COOKIE_LIFETIME_NORMAL);
    /* package */ Pref<Integer> mCookieLifetimeDays = new Pref<Integer>(
        "network.cookie.lifetime.days", 90);
    /* package */ Pref<String> mTrackingProtection = new Pref<String>(
        "urlclassifier.trackingTable",
        TrackingProtection.buildPrefValue(TrackingProtectionDelegate.CATEGORY_ALL));
    /* package */ Pref<Boolean> mConsoleOutput = new Pref<Boolean>(
        "geckoview.console.enabled", false);

    /* package */ boolean mNativeCrashReporting;
    /* package */ boolean mJavaCrashReporting;
    /* package */ boolean mDebugPause;

    private final Pref<?>[] mPrefs = new Pref<?>[] {
        mCookieBehavior, mCookieLifetime, mCookieLifetimeDays, mConsoleOutput,
        mJavaScript, mRemoteDebugging, mTrackingProtection, mWebFonts
    };

    /* package */ GeckoRuntimeSettings() {
        this(null);
    }

    /* package */ GeckoRuntimeSettings(final @Nullable GeckoRuntimeSettings settings) {
        if (BuildConfig.DEBUG && prefCount != mPrefs.length) {
            throw new AssertionError("Add new pref to prefs list");
        }

        if (settings == null) {
            mArgs = new String[0];
            mExtras = new Bundle();
            return;
        }

        mUseContentProcess = settings.getUseContentProcessHint();
        mArgs = settings.getArguments().clone();
        mExtras = new Bundle(settings.getExtras());

        for (int i = 0; i < mPrefs.length; i++) {
            if (!settings.mPrefs[i].mIsSet) {
                continue;
            }
            // We know this is safe.
            @SuppressWarnings("unchecked")
            final Pref<Object> uncheckedPref = (Pref<Object>) mPrefs[i];
            uncheckedPref.set(settings.mPrefs[i].get());
        }

        mNativeCrashReporting = settings.mNativeCrashReporting;
        mJavaCrashReporting = settings.mJavaCrashReporting;
        mDebugPause = settings.mDebugPause;
    }

    /* package */ void flush() {
        for (final Pref<?> pref: mPrefs) {
            pref.flush();
        }
    }

    /**
     * Get the content process hint flag.
     *
     * @return The content process hint flag.
     */
    public boolean getUseContentProcessHint() {
        return mUseContentProcess;
    }

    /**
     * Get the custom Gecko process arguments.
     *
     * @return The Gecko process arguments.
     */
    public String[] getArguments() {
        return mArgs;
    }

    /**
     * Get the custom Gecko intent extras.
     *
     * @return The Gecko intent extras.
     */
    public Bundle getExtras() {
        return mExtras;
    }

    /**
     * Get whether JavaScript support is enabled.
     *
     * @return Whether JavaScript support is enabled.
     */
    public boolean getJavaScriptEnabled() {
        return mJavaScript.get();
    }

    /**
     * Set whether JavaScript support should be enabled.
     *
     * @param flag A flag determining whether JavaScript should be enabled.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setJavaScriptEnabled(final boolean flag) {
        mJavaScript.set(flag);
        return this;
    }

    /**
     * Get whether remote debugging support is enabled.
     *
     * @return True if remote debugging support is enabled.
     */
    public boolean getRemoteDebuggingEnabled() {
        return mRemoteDebugging.get();
    }

    /**
     * Set whether remote debugging support should be enabled.
     *
     * @param enabled True if remote debugging should be enabled.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setRemoteDebuggingEnabled(final boolean enabled) {
        mRemoteDebugging.set(enabled);
        return this;
    }

    /**
     * Get whether web fonts support is enabled.
     *
     * @return Whether web fonts support is enabled.
     */
    public boolean getWebFontsEnabled() {
        return mWebFonts.get();
    }

    /**
     * Set whether support for web fonts should be enabled.
     *
     * @param flag A flag determining whether web fonts should be enabled.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setWebFontsEnabled(final boolean flag) {
        mWebFonts.set(flag);
        return this;
    }

    /**
     * Get whether native crash reporting is enabled or not.
     *
     * @return True if native crash reporting is enabled.
     */
    public boolean getNativeCrashReportingEnabled() {
        return mNativeCrashReporting;
    }

    /**
     * Get whether Java crash reporting is enabled or not.
     *
     * @return True if Java crash reporting is enabled.
     */
    public boolean getJavaCrashReportingEnabled() {
        return mJavaCrashReporting;
    }

    /**
     * Gets whether the pause-for-debugger is enabled or not.
     *
     * @return True if the pause is enabled.
     */
    public boolean getPauseForDebuggerEnabled() { return mDebugPause; }

    // Sync values with nsICookieService.idl.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ COOKIE_ACCEPT_ALL, COOKIE_ACCEPT_FIRST_PARTY,
              COOKIE_ACCEPT_NONE, COOKIE_ACCEPT_VISITED })
    public @interface CookieBehavior {}
    /**
     * Accept first-party and third-party cookies and site data.
     */
    public static final int COOKIE_ACCEPT_ALL = 0;
    /**
     * Accept only first-party cookies and site data to block cookies which are
     * not associated with the domain of the visited site.
     */
    public static final int COOKIE_ACCEPT_FIRST_PARTY = 1;
    /**
     * Do not store any cookies and site data.
     */
    public static final int COOKIE_ACCEPT_NONE = 2;
    /**
     * Accept first-party and third-party cookies and site data only from
     * sites previously visited in a first-party context.
     */
    public static final int COOKIE_ACCEPT_VISITED = 3;

    /**
     * Get the assigned cookie storage behavior.
     *
     * @return The assigned behavior, as one of {@link #COOKIE_ACCEPT_ALL COOKIE_ACCEPT_*} flags.
     */
    public @CookieBehavior int getCookieBehavior() {
        return mCookieBehavior.get();
    }

    /**
     * Set cookie storage behavior.
     *
     * @param behavior The storage behavior that should be applied.
     *                 Use one of the {@link #COOKIE_ACCEPT_ALL COOKIE_ACCEPT_*} flags.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setCookieBehavior(
            @CookieBehavior int behavior) {
        mCookieBehavior.set(behavior);
        return this;
    }

    // Sync values with nsICookieService.idl.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ COOKIE_LIFETIME_NORMAL, COOKIE_LIFETIME_RUNTIME,
              COOKIE_LIFETIME_DAYS })
    public @interface CookieLifetime {}
    /**
     * Accept default cookie lifetime.
     */
    public static final int COOKIE_LIFETIME_NORMAL = 0;
    /**
     * Downgrade cookie lifetime to this runtime's lifetime.
     */
    public static final int COOKIE_LIFETIME_RUNTIME = 2;
    /**
     * Limit cookie lifetime to N days.
     * Defaults to 90 days.
     */
    public static final int COOKIE_LIFETIME_DAYS = 3;

    /**
     * Get the assigned cookie lifetime.
     *
     * @return The assigned lifetime, as one of {@link #COOKIE_LIFETIME_NORMAL COOKIE_LIFETIME_*} flags.
     */
    public @CookieBehavior int getCookieLifetime() {
        return mCookieLifetime.get();
    }

    /**
     * Get the enforced lifetime expiration days.
     *
     * Note: This is only enforced for {@link #COOKIE_LIFETIME_DAYS}.
     *
     * @return The enforced expiration days.
     */
    public int getCookieLifetimeDays() {
        return mCookieLifetimeDays.get();
    }

    /**
     * Set the cookie lifetime.
     *
     * @param lifetime The enforced cookie lifetime.
     *                 Use one of the {@link #COOKIE_LIFETIME_NORMAL COOKIE_LIFETIME_*} flags.
     *                 Use {@link #setCookieLifetimeDays} to set expiration
     *                 days for {@link #COOKIE_LIFETIME_DAYS}.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setCookieLifetime(
            @CookieLifetime int lifetime) {
        mCookieLifetime.set(lifetime);
        return this;
    }

    /**
     * Set the expiration days for {@link #COOKIE_LIFETIME_DAYS}.
     *
     * @param days Limit lifetime to N days. Only enforced for {@link #COOKIE_LIFETIME_DAYS}.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setCookieLifetimeDays(int days) {
        if (mCookieLifetime.get() != COOKIE_LIFETIME_DAYS) {
            throw new IllegalStateException(
                "Setting expiration days for incompatible cookie lifetime");
        }
        mCookieLifetimeDays.set(days);
        return this;
    }

    /**
     * Get the set tracking protection blocking categories.
     *
     * @return categories The categories of trackers that are set to be blocked.
     *                    Use one or more of the
     *                    {@link TrackingProtectionDelegate#CATEGORY_AD TrackingProtectionDelegate.CATEGORY_*} flags.
     **/
    public @TrackingProtectionDelegate.Category int getTrackingProtectionCategories() {
        return TrackingProtection.listToCategory(mTrackingProtection.get());
    }

    /**
     * Set tracking protection blocking categories.
     *
     * @param categories The categories of trackers that should be blocked.
     *                   Use one or more of the
     *                   {@link TrackingProtectionDelegate#CATEGORY_AD TrackingProtectionDelegate.CATEGORY_*} flags.
     * @return This GeckoRuntimeSettings instance.
     **/
    public @NonNull GeckoRuntimeSettings setTrackingProtectionCategories(
            @TrackingProtectionDelegate.Category int categories) {
        mTrackingProtection.set(TrackingProtection.buildPrefValue(categories));
        return this;
    }

    /**
     * Set whether or not web console messages should go to logcat.
     *
     * Note: If enabled, Gecko performance may be negatively impacted if
     * content makes heavy use of the console API.
     *
     * @param enabled A flag determining whether or not web console messages should be
     *                printed to logcat.
     * @return This GeckoRuntimeSettings instance.
     */

    public @NonNull GeckoRuntimeSettings setConsoleOutputEnabled(boolean enabled) {
        mConsoleOutput.set(enabled);
        return this;
    }

    /**
     * Get whether or not web console messages are sent to logcat.
     *
     * @return This GeckoRuntimeSettings instance.
     */
    public boolean getConsoleOutputEnabled() {
        return mConsoleOutput.get();
    }

    @Override // Parcelable
    public int describeContents() {
        return 0;
    }

    @Override // Parcelable
    public void writeToParcel(Parcel out, int flags) {
        ParcelableUtils.writeBoolean(out, mUseContentProcess);
        out.writeStringArray(mArgs);
        mExtras.writeToParcel(out, flags);

        for (final Pref<?> pref : mPrefs) {
            out.writeValue(pref.get());
        }

        ParcelableUtils.writeBoolean(out, mNativeCrashReporting);
        ParcelableUtils.writeBoolean(out, mJavaCrashReporting);
        ParcelableUtils.writeBoolean(out, mDebugPause);
    }

    // AIDL code may call readFromParcel even though it's not part of Parcelable.
    public void readFromParcel(final Parcel source) {
        mUseContentProcess = ParcelableUtils.readBoolean(source);
        mArgs = source.createStringArray();
        mExtras.readFromParcel(source);

        for (final Pref<?> pref : mPrefs) {
            // We know this is safe.
            @SuppressWarnings("unchecked")
            final Pref<Object> uncheckedPref = (Pref<Object>) pref;
            uncheckedPref.set(source.readValue(getClass().getClassLoader()));
        }

        mNativeCrashReporting = ParcelableUtils.readBoolean(source);
        mJavaCrashReporting = ParcelableUtils.readBoolean(source);
        mDebugPause = ParcelableUtils.readBoolean(source);
    }

    public static final Parcelable.Creator<GeckoRuntimeSettings> CREATOR
        = new Parcelable.Creator<GeckoRuntimeSettings>() {
        @Override
        public GeckoRuntimeSettings createFromParcel(final Parcel in) {
            final GeckoRuntimeSettings settings = new GeckoRuntimeSettings();
            settings.readFromParcel(in);
            return settings;
        }

        @Override
        public GeckoRuntimeSettings[] newArray(final int size) {
            return new GeckoRuntimeSettings[size];
        }
    };
}
