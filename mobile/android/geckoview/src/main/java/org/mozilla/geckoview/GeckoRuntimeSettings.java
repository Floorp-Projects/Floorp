/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Locale;

import android.app.Service;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.os.LocaleList;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.AnyThread;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;

@AnyThread
public final class GeckoRuntimeSettings extends RuntimeSettings {
    /**
     * Settings builder used to construct the settings object.
     */
    @AnyThread
    public static final class Builder
            extends RuntimeSettings.Builder<GeckoRuntimeSettings> {
        @Override
        protected @NonNull GeckoRuntimeSettings newSettings(
                final @Nullable GeckoRuntimeSettings settings) {
            return new GeckoRuntimeSettings(settings);
        }

        /**
         * Set the content process hint flag.
         *
         * @param use If true, this will reload the content process for future use.
         *            Default is false.
         * @return This Builder instance.

         */
        public @NonNull Builder useContentProcessHint(final boolean use) {
            getSettings().mUseContentProcess = use;
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
            getSettings().mArgs = args;
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
            getSettings().mExtras = extras;
            return this;
        }

        /**
         * Path to configuration file from which GeckoView will read configuration options such as
         * Gecko process arguments, environment variables, and preferences.
         *
         * @param configFilePath Configuration file path to read from, or <code>null</code> to use
         *                       default location <code>/data/local/tmp/$PACKAGE-geckoview-config.yaml</code>.
         * @return This Builder instance.
         */
        public @NonNull Builder configFilePath(final @Nullable String configFilePath) {
            getSettings().mConfigFilePath = configFilePath;
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
            getSettings().mJavaScript.set(flag);
            return this;
        }

        /**
         * Set whether remote debugging support should be enabled.
         *
         * @param enabled True if remote debugging should be enabled.
         * @return This Builder instance.
         */
        public @NonNull Builder remoteDebuggingEnabled(final boolean enabled) {
            getSettings().mRemoteDebugging.set(enabled);
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
            getSettings().mWebFonts.set(flag ? 1 : 0);
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
        public @NonNull Builder pauseForDebugger(final boolean enabled) {
            getSettings().mDebugPause = enabled;
            return this;
        }
        /**
         * Set whether the to report the full bit depth of the device.
         *
         * By default, 24 bits are reported for high memory devices and 16 bits
         * for low memory devices. If set to true, the device's maximum bit depth is
         * reported. On most modern devices this will be 32 bit screen depth.
         *
         * @param enable A flag determining whether maximum screen depth should be used.
         * @return This Builder.
         */
        public @NonNull Builder useMaxScreenDepth(final boolean enable) {
            getSettings().mUseMaxScreenDepth = enable;
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
        public @NonNull Builder consoleOutput(final boolean enabled) {
            getSettings().mConsoleOutput.set(enabled);
            return this;
        }

        /**
         * Set whether or not font sizes in web content should be automatically scaled according to
         * the device's current system font scale setting.
         *
         * @param enabled A flag determining whether or not font sizes should be scaled automatically
         *                to match the device's system font scale.
         * @return The builder instance.
         */
        public @NonNull Builder automaticFontSizeAdjustment(final boolean enabled) {
            getSettings().setAutomaticFontSizeAdjustment(enabled);
            return this;
        }

        /**
         * Set a font size factor that will operate as a global text zoom. All font sizes will be
         * multiplied by this factor.
         *
         * <p>The default factor is 1.0.
         *
         * <p>This setting cannot be modified if
         * {@link Builder#automaticFontSizeAdjustment automatic font size adjustment}
         * has already been enabled.
         *
         * @param fontSizeFactor The factor to be used for scaling all text. Setting a value of 0
         *                       disables both this feature and
         *                       {@link Builder#fontInflation font inflation}.
         * @return The builder instance.
         */
        public @NonNull Builder fontSizeFactor(final float fontSizeFactor) {
            getSettings().setFontSizeFactor(fontSizeFactor);
            return this;
        }

        /**
         * Set whether or not font inflation for non mobile-friendly pages should be enabled. The
         * default value of this setting is <code>false</code>.
         *
         * <p>When enabled, font sizes will be increased on all pages that are lacking a
         * &lt;meta&gt; viewport tag and have been loaded in a session using
         * {@link GeckoSessionSettings#VIEWPORT_MODE_MOBILE}. To improve readability, the font
         * inflation logic will attempt to increase font sizes for the main text content of the page
         * only.
         *
         * <p>The magnitude of font inflation applied depends on the
         * {@link Builder#fontSizeFactor font size factor} currently in use.
         *
         * <p>This setting cannot be modified if
         * {@link Builder#automaticFontSizeAdjustment automatic font size adjustment}
         * has already been enabled.
         *
         * @param enabled A flag determining whether or not font inflation should be enabled.
         * @return The builder instance.
         */
        public @NonNull Builder fontInflation(final boolean enabled) {
            getSettings().setFontInflationEnabled(enabled);
            return this;
        }

        /**
         * Set the display density override.
         *
         * @param density The display density value to use for overriding the system default.
         * @return The builder instance.
         */
        public @NonNull Builder displayDensityOverride(final float density) {
            getSettings().mDisplayDensityOverride = density;
            return this;
        }

        /**
         * Set the display DPI override.
         *
         * @param dpi The display DPI value to use for overriding the system default.
         * @return The builder instance.
         */
        public @NonNull Builder displayDpiOverride(final int dpi) {
            getSettings().mDisplayDpiOverride = dpi;
            return this;
        }

        /**
         * Set the screen size override.
         *
         * @param width The screen width value to use for overriding the system default.
         * @param height The screen height value to use for overriding the system default.
         * @return The builder instance.
         */
        public @NonNull Builder screenSizeOverride(final int width, final int height) {
            getSettings().mScreenWidthOverride = width;
            getSettings().mScreenHeightOverride = height;
            return this;
        }

        /**
         * When set, the specified {@link android.app.Service} will be started by
         * an {@link android.content.Intent} with action {@link GeckoRuntime#ACTION_CRASHED} when
         * a crash is encountered. Crash details can be found in the Intent extras, such as
         * {@link GeckoRuntime#EXTRA_MINIDUMP_PATH}.
         * <br><br>
         * The crash handler Service must be declared to run in a different process from
         * the {@link GeckoRuntime}. Additionally, the handler will be run as a foreground service,
         * so the normal rules about activating a foreground service apply.
         * <br><br>
         * In practice, you have one of three
         * options once the crash handler is started:
         * <ul>
         * <li>Call {@link android.app.Service#startForeground(int, android.app.Notification)}. You can then
         * take as much time as necessary to report the crash.</li>
         * <li>Start an activity. Unless you also call {@link android.app.Service#startForeground(int, android.app.Notification)}
         * this should be in a different process from the crash handler, since Android will
         * kill the crash handler process as part of the background execution limitations.</li>
         * <li>Schedule work via {@link android.app.job.JobScheduler}. This will allow you to
         * do substantial work in the background without execution limits.</li>
         * </ul><br>
         * You can use {@link CrashReporter} to send the report to Mozilla, which provides Mozilla
         * with data needed to fix the crash. Be aware that the minidump may contain
         * personally identifiable information (PII). Consult Mozilla's
         * <a href="https://www.mozilla.org/en-US/privacy/">privacy policy</a> for information
         * on how this data will be handled.
         *
         * @param handler The class for the crash handler Service.
         * @return This builder instance.
         *
         * @see <a href="https://developer.android.com/about/versions/oreo/background">Android Background Execution Limits</a>
         * @see GeckoRuntime#ACTION_CRASHED
         */
        public @NonNull Builder crashHandler(final @Nullable Class<? extends Service> handler) {
            getSettings().mCrashHandler = handler;
            return this;
        }

        /**
         * Set the locale.
         *
         * @param requestedLocales List of locale codes in Gecko format ("en" or "en-US").
         * @return The builder instance.
         */
        public @NonNull Builder locales(final @Nullable String[] requestedLocales) {
            getSettings().mRequestedLocales = requestedLocales;
            return this;
        }

        public @NonNull Builder contentBlocking(
                final @NonNull ContentBlocking.Settings cb) {
            getSettings().mContentBlocking = cb;
            return this;
        }

        /**
         * Sets video autoplay mode.
         * May be either {@link GeckoRuntimeSettings#AUTOPLAY_DEFAULT_ALLOWED} or {@link GeckoRuntimeSettings#AUTOPLAY_DEFAULT_BLOCKED}
         * @param autoplay Allows or blocks video autoplay.
         * @return This Builder instance.
         */
        public @NonNull Builder autoplayDefault(final @AutoplayDefault int autoplay) {
            getSettings().mAutoplayDefault.set(autoplay);
            return this;
        }

        /**
         * Sets the preferred color scheme override for web content.
         *
         * @param scheme The preferred color scheme. Must be one of the
         *               {@link GeckoRuntimeSettings#COLOR_SCHEME_LIGHT COLOR_SCHEME_*} constants.
         * @return This Builder instance.
         */
        public @NonNull Builder preferredColorScheme(final @ColorScheme int scheme) {
            getSettings().mPreferredColorScheme.set(scheme);
            return this;
        }

        /**
         * Set whether auto-zoom to editable fields should be enabled.
         *
         * @param flag True if auto-zoom should be enabled, false otherwise.
         * @return This Builder instance.
         */
        public @NonNull Builder inputAutoZoomEnabled(final boolean flag) {
            getSettings().mInputAutoZoom.set(flag);
            return this;
        }

        /**
         * Set whether double tap zooming should be enabled.
         *
         * @param flag True if double tap zooming should be enabled, false otherwise.
         * @return This Builder instance.
         */
        public @NonNull Builder doubleTapZoomingEnabled(final boolean flag) {
            getSettings().mDoubleTapZooming.set(flag);
            return this;
        }

        /**
         * Sets the WebGL MSAA level.
         *
         * @param level number of MSAA samples, 0 if MSAA should be disabled.
         * @return This Builder instance.
         */
        public @NonNull Builder glMsaaLevel(final int level) {
            getSettings().mGlMsaaLevel.set(level);
            return this;
        }

        /**
         * Add a {@link RuntimeTelemetry.Delegate} instance to this
         * GeckoRuntime.  This delegate can be used by the app to receive
         * streaming telemetry data from GeckoView.
         *
         * @param delegate the delegate that will handle telemetry
         * @return The builder instance.
         */
        public @NonNull Builder telemetryDelegate(
                final @NonNull RuntimeTelemetry.Delegate delegate) {
            getSettings().mTelemetryProxy = new RuntimeTelemetry.Proxy(delegate);
            getSettings().mTelemetryEnabled.set(true);
            return this;
        }

        /**
         * Enables GeckoView and Gecko Logging.
         * Logging is on by default. Does not control all logging in Gecko.
         * Logging done in Java code must be stripped out at build time.
         *
         * @param enable True if logging is enabled.
         * @return This Builder instance.
         */
        public @NonNull Builder debugLogging(final boolean enable) {
            getSettings().mDevToolsConsoleToLogcat.set(enable);
            getSettings().mConsoleServiceToLogcat.set(enable);
            getSettings().mGeckoViewLogLevel.set(enable ? "Debug" : "Fatal");
            return this;
        }
    }

    private GeckoRuntime mRuntime;
    /* package */ boolean mUseContentProcess;
    /* package */ String[] mArgs;
    /* package */ Bundle mExtras;
    /* package */ String mConfigFilePath;

    /* package */ ContentBlocking.Settings mContentBlocking;

    public @NonNull ContentBlocking.Settings getContentBlocking() {
        return mContentBlocking;
    }

    /* package */ final Pref<Boolean> mJavaScript = new Pref<Boolean>(
        "javascript.enabled", true);
    /* package */ final Pref<Boolean> mRemoteDebugging = new Pref<Boolean>(
        "devtools.debugger.remote-enabled", false);
    /* package */ final Pref<Integer> mWebFonts = new Pref<Integer>(
        "browser.display.use_document_fonts", 1);
    /* package */ final Pref<Boolean> mConsoleOutput = new Pref<Boolean>(
        "geckoview.console.enabled", false);
    /* package */ final Pref<Integer> mAutoplayDefault = new Pref<Integer>(
        "media.autoplay.default", AUTOPLAY_DEFAULT_BLOCKED);
    /* package */ final Pref<Integer> mFontSizeFactor = new Pref<>(
        "font.size.systemFontScale", 100);
    /* package */ final Pref<Integer> mFontInflationMinTwips = new Pref<>(
        "font.size.inflation.minTwips", 0);
    /* package */ final Pref<Integer> mPreferredColorScheme = new Pref<>(
        "ui.systemUsesDarkTheme", -1);
    /* package */ final Pref<Boolean> mInputAutoZoom = new Pref<>(
            "formhelper.autozoom", true);
    /* package */ final Pref<Boolean> mDoubleTapZooming = new Pref<>(
            "apz.allow_double_tap_zooming", true);
    /* package */ final Pref<Integer> mGlMsaaLevel = new Pref<>(
            "gl.msaa-level", 0);
    /* package */ final Pref<Boolean> mTelemetryEnabled = new Pref<>(
            "toolkit.telemetry.geckoview.streaming", false);
    /* package */ final Pref<String> mGeckoViewLogLevel = new Pref<>(
            "geckoview.logging", "Debug");
    /* package */ final Pref<Boolean> mConsoleServiceToLogcat = new Pref<>(
            "consoleservice.logcat", true);
    /* package */ final Pref<Boolean> mDevToolsConsoleToLogcat = new Pref<>(
            "devtools.console.stdout.chrome", true);

    /* package */ boolean mDebugPause;
    /* package */ boolean mUseMaxScreenDepth;
    /* package */ float mDisplayDensityOverride = -1.0f;
    /* package */ int mDisplayDpiOverride;
    /* package */ int mScreenWidthOverride;
    /* package */ int mScreenHeightOverride;
    /* package */ Class<? extends Service> mCrashHandler;
    /* package */ String[] mRequestedLocales;
    /* package */ RuntimeTelemetry.Proxy mTelemetryProxy;

    /**
     * Attach and commit the settings to the given runtime.
     * @param runtime The runtime to attach to.
     */
    /* package */ void attachTo(final @NonNull GeckoRuntime runtime) {
        mRuntime = runtime;
        commit();
    }

    @Override // RuntimeSettings
    public @Nullable GeckoRuntime getRuntime() {
        return mRuntime;
    }

    /* package */ GeckoRuntimeSettings() {
        this(null);
    }

    /* package */ GeckoRuntimeSettings(final @Nullable GeckoRuntimeSettings settings) {
        super(/* parent */ null);

        if (settings == null) {
            mArgs = new String[0];
            mExtras = new Bundle();
            mContentBlocking = new ContentBlocking.Settings(
                    this /* parent */, null /* settings */);
            return;
        }

        updateSettings(settings);
    }

    private void updateSettings(final @NonNull GeckoRuntimeSettings settings) {
        updatePrefs(settings);

        mUseContentProcess = settings.getUseContentProcessHint();
        mArgs = settings.getArguments().clone();
        mExtras = new Bundle(settings.getExtras());
        mContentBlocking = new ContentBlocking.Settings(
                this /* parent */, settings.mContentBlocking);

        mDebugPause = settings.mDebugPause;
        mUseMaxScreenDepth = settings.mUseMaxScreenDepth;
        mDisplayDensityOverride = settings.mDisplayDensityOverride;
        mDisplayDpiOverride = settings.mDisplayDpiOverride;
        mScreenWidthOverride = settings.mScreenWidthOverride;
        mScreenHeightOverride = settings.mScreenHeightOverride;
        mCrashHandler = settings.mCrashHandler;
        mRequestedLocales = settings.mRequestedLocales;
        mConfigFilePath = settings.mConfigFilePath;
        mTelemetryProxy = settings.mTelemetryProxy;
    }

    /* package */ void commit() {
        commitLocales();
        commitResetPrefs();
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
    public @NonNull String[] getArguments() {
        return mArgs;
    }

    /**
     * Get the custom Gecko intent extras.
     *
     * @return The Gecko intent extras.
     */
    public @NonNull Bundle getExtras() {
        return mExtras;
    }

    /**
     * Path to configuration file from which GeckoView will read configuration options such as
     * Gecko process arguments, environment variables, and preferences.
     *
     * @return Path to configuration file from which GeckoView will read configuration options,
     * or <code>null</code> for default location
     * <code>/data/local/tmp/$PACKAGE-geckoview-config.yaml</code>.
     */
    public @Nullable String getConfigFilePath() {
        return mConfigFilePath;
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
        mJavaScript.commit(flag);
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
        mRemoteDebugging.commit(enabled);
        return this;
    }

    /**
     * Get whether web fonts support is enabled.
     *
     * @return Whether web fonts support is enabled.
     */
    public boolean getWebFontsEnabled() {
        return mWebFonts.get() != 0 ? true : false;
    }

    /**
     * Set whether support for web fonts should be enabled.
     *
     * @param flag A flag determining whether web fonts should be enabled.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setWebFontsEnabled(final boolean flag) {
        mWebFonts.commit(flag ? 1 : 0);
        return this;
    }

    /**
     * Gets whether the pause-for-debugger is enabled or not.
     *
     * @return True if the pause is enabled.
     */
    public boolean getPauseForDebuggerEnabled() {
        return mDebugPause;
    }

    /**
     * Gets whether the compositor should use the maximum screen depth when rendering.
     *
     * @return True if the maximum screen depth should be used.
     */
    public boolean getUseMaxScreenDepth() {
        return mUseMaxScreenDepth;
    }

    /**
     * Gets the display density override value.
     *
     * @return Returns a positive number. Will return null if not set.
     */
    public @Nullable Float getDisplayDensityOverride() {
        if (mDisplayDensityOverride > 0.0f) {
            return mDisplayDensityOverride;
        }
        return null;
    }

    /**
     * Gets the display DPI override value.
     *
     * @return Returns a positive number. Will return null if not set.
     */
    public @Nullable Integer getDisplayDpiOverride() {
        if (mDisplayDpiOverride > 0) {
            return mDisplayDpiOverride;
        }
        return null;
    }

    public @Nullable Class<? extends Service> getCrashHandler() {
        return mCrashHandler;
    }

    /**
     * Gets the screen size  override value.
     *
     * @return Returns a Rect containing the dimensions to use for the window size.
     * Will return null if not set.
     */
    public @Nullable Rect getScreenSizeOverride() {
        if ((mScreenWidthOverride > 0) && (mScreenHeightOverride > 0)) {
            return new Rect(0, 0, mScreenWidthOverride, mScreenHeightOverride);
        }
        return null;
    }

    /**
     * Gets the list of requested locales.
     *
     * @return A list of locale codes in Gecko format ("en" or "en-US").
     */
    public @Nullable String[] getLocales() {
        return mRequestedLocales;
    }

    /**
     * Set the locale.
     *
     * @param requestedLocales An ordered list of locales in Gecko format ("en-US").
     */
    public void setLocales(final @Nullable String[] requestedLocales) {
        mRequestedLocales = requestedLocales;
        commitLocales();
    }

    private void commitLocales() {
        final GeckoBundle data = new GeckoBundle(1);
        data.putStringArray("requestedLocales", mRequestedLocales);
        data.putString("acceptLanguages", computeAcceptLanguages());
        EventDispatcher.getInstance().dispatch("GeckoView:SetLocale", data);
    }

    private String computeAcceptLanguages() {
        ArrayList<String> locales = new ArrayList<String>();

        // Explicitly-set app prefs come first:
        if (mRequestedLocales != null) {
            for (String locale : mRequestedLocales) {
                locales.add(locale.toLowerCase());
            }
        }
        // OS prefs come second:
        for (String locale : getDefaultLocales()) {
            locale = locale.toLowerCase();
            if (!locales.contains(locale)) {
                locales.add(locale);
            }
        }

        return TextUtils.join(",", locales);
    }

    private static String[] getDefaultLocales() {
        if (Build.VERSION.SDK_INT >= 24) {
            final LocaleList localeList = LocaleList.getDefault();
            String[] locales = new String[localeList.size()];
            for (int i = 0; i < localeList.size(); i++) {
                locales[i] = localeList.get(i).toLanguageTag();
            }
            return locales;
        }
        String[] locales = new String[1];
        final Locale locale = Locale.getDefault();
        if (Build.VERSION.SDK_INT >= 21) {
            locales[0] = locale.toLanguageTag();
            return locales;
        }

        locales[0] = getLanguageTag(locale);
        return locales;
    }

    private static String getLanguageTag(final Locale locale) {
        final StringBuilder out = new StringBuilder(locale.getLanguage());
        final String country = locale.getCountry();
        final String variant = locale.getVariant();
        if (!TextUtils.isEmpty(country)) {
            out.append('-').append(country);
        }
        if (!TextUtils.isEmpty(variant)) {
            out.append('-').append(variant);
        }
        // e.g. "en", "en-US", or "en-US-POSIX".
        return out.toString();
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

    public @NonNull GeckoRuntimeSettings setConsoleOutputEnabled(final boolean enabled) {
        mConsoleOutput.commit(enabled);
        return this;
    }

    /**
     * Get whether or not web console messages are sent to logcat.
     *
     * @return True if console output is enabled.
     */
    public boolean getConsoleOutputEnabled() {
        return mConsoleOutput.get();
    }

    /**
     * Set whether or not font sizes in web content should be automatically scaled according to
     * the device's current system font scale setting. Enabling this will prevent modification of
     * the {@link GeckoRuntimeSettings#setFontSizeFactor font size factor}.
     * Disabling this setting will restore the previously used value for the
     * {@link GeckoRuntimeSettings#getFontSizeFactor font size factor}.
     *
     * @param enabled A flag determining whether or not font sizes should be scaled automatically
     *                to match the device's system font scale.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setAutomaticFontSizeAdjustment(final boolean enabled) {
        GeckoFontScaleListener.getInstance().setEnabled(enabled);
        return this;
    }

    /**
     * Get whether or not the font sizes for web content are automatically adjusted to match the
     * device's system font scale setting.
     *
     * @return True if font sizes are automatically adjusted.
     */
    public boolean getAutomaticFontSizeAdjustment() {
        return GeckoFontScaleListener.getInstance().getEnabled();
    }

    // Sync values with dom/media/nsIAutoplay.idl.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ AUTOPLAY_DEFAULT_ALLOWED, AUTOPLAY_DEFAULT_BLOCKED })
    /* package */ @interface AutoplayDefault {}

    /**
     * Autoplay video is allowed.
     */
    public static final int AUTOPLAY_DEFAULT_ALLOWED = 0;

    /**
     * Autoplay video is blocked.
     */
    public static final int AUTOPLAY_DEFAULT_BLOCKED = 1;

    /**
     * Sets video autoplay mode.
     * May be either {@link GeckoRuntimeSettings#AUTOPLAY_DEFAULT_ALLOWED} or {@link GeckoRuntimeSettings#AUTOPLAY_DEFAULT_BLOCKED}
     * @param autoplay Allows or blocks video autoplay.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setAutoplayDefault(final @AutoplayDefault int autoplay) {
        mAutoplayDefault.commit(autoplay);
        return this;
    }

    /**
     * Gets the current video autoplay mode.
     * @return The current video autoplay mode. Will be either {@link GeckoRuntimeSettings#AUTOPLAY_DEFAULT_ALLOWED}
     * or {@link GeckoRuntimeSettings#AUTOPLAY_DEFAULT_BLOCKED}
     */
    public @AutoplayDefault int getAutoplayDefault() {
        return mAutoplayDefault.get();
    }

    private static final int FONT_INFLATION_BASE_VALUE = 120;

    /**
     * Set a font size factor that will operate as a global text zoom. All font sizes will be
     * multiplied by this factor.
     *
     * <p>The default factor is 1.0.
     *
     * <p>Currently, any changes only take effect after a reload of the session.
     *
     * <p>This setting cannot be modified while
     * {@link GeckoRuntimeSettings#setAutomaticFontSizeAdjustment automatic font size adjustment}
     * is enabled.
     *
     * @param fontSizeFactor The factor to be used for scaling all text. Setting a value of 0
     *                       disables both this feature and
     *                       {@link GeckoRuntimeSettings#setFontInflationEnabled font inflation}.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setFontSizeFactor(final float fontSizeFactor) {
        if (getAutomaticFontSizeAdjustment()) {
            throw new IllegalStateException("Not allowed when automatic font size adjustment is enabled");
        }
        return setFontSizeFactorInternal(fontSizeFactor);
    }

    /* package */ @NonNull GeckoRuntimeSettings setFontSizeFactorInternal(
            final float fontSizeFactor) {
        if (fontSizeFactor < 0) {
            throw new IllegalArgumentException("fontSizeFactor cannot be < 0");
        }

        final int fontSizePercentage = Math.round(fontSizeFactor * 100);
        mFontSizeFactor.commit(fontSizePercentage);
        if (getFontInflationEnabled()) {
            final int scaledFontInflation = Math.round(FONT_INFLATION_BASE_VALUE * fontSizeFactor);
            mFontInflationMinTwips.commit(scaledFontInflation);
        }
        return this;
    }

    /**
     * Gets the currently applied font size factor.
     *
     * @return The currently applied font size factor.
     */
    public float getFontSizeFactor() {
        return mFontSizeFactor.get() / 100f;
    }

    /**
     * Set whether or not font inflation for non mobile-friendly pages should be enabled. The
     * default value of this setting is <code>false</code>.
     *
     * <p>When enabled, font sizes will be increased on all pages that are lacking a &lt;meta&gt;
     * viewport tag and have been loaded in a session using
     * {@link GeckoSessionSettings#VIEWPORT_MODE_MOBILE}. To improve readability, the font inflation
     * logic will attempt to increase font sizes for the main text content of the page only.
     *
     * <p>The magnitude of font inflation applied depends on the
     * {@link GeckoRuntimeSettings#setFontSizeFactor font size factor} currently in use.
     *
     * <p>Currently, any changes only take effect after a reload of the session.
     *
     * @param enabled A flag determining whether or not font inflation should be enabled.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setFontInflationEnabled(final boolean enabled) {
        final int minTwips =
                enabled ? Math.round(FONT_INFLATION_BASE_VALUE * getFontSizeFactor()) : 0;
        mFontInflationMinTwips.commit(minTwips);
        return this;
    }

    /**
     * Get whether or not font inflation for non mobile-friendly pages is currently enabled.
     *
     * @return True if font inflation is enabled.
     */
    public boolean getFontInflationEnabled() {
        return mFontInflationMinTwips.get() > 0;
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({COLOR_SCHEME_LIGHT,
             COLOR_SCHEME_DARK,
             COLOR_SCHEME_SYSTEM})
    /* package */ @interface ColorScheme {}

    /** A light theme for web content is preferred. */
    public static final int COLOR_SCHEME_LIGHT = 0;
    /** A dark theme for web content is preferred. */
    public static final int COLOR_SCHEME_DARK = 1;
    /** The preferred color scheme will be based on system settings. */
    public static final int COLOR_SCHEME_SYSTEM = -1;

    /**
     * Gets the preferred color scheme override for web content.
     *
     * @return One of the {@link GeckoRuntimeSettings#COLOR_SCHEME_LIGHT COLOR_SCHEME_*} constants.
     */
    public @ColorScheme int getPreferredColorScheme() {
        return mPreferredColorScheme.get();
    }

    /**
     * Sets the preferred color scheme override for web content.
     *
     * @param scheme The preferred color scheme. Must be one of the
     *               {@link GeckoRuntimeSettings#COLOR_SCHEME_LIGHT COLOR_SCHEME_*} constants.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setPreferredColorScheme(final @ColorScheme int scheme) {
        mPreferredColorScheme.commit(scheme);
        return this;
    }

    /**
     * Gets whether auto-zoom to editable fields is enabled.
     *
     * @return True if auto-zoom is enabled, false otherwise.
     */
    public boolean getInputAutoZoomEnabled() {
        return mInputAutoZoom.get();
    }

    /**
     * Set whether auto-zoom to editable fields should be enabled.
     *
     * @param flag True if auto-zoom should be enabled, false otherwise.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setInputAutoZoomEnabled(final boolean flag) {
        mInputAutoZoom.commit(flag);
        return this;
    }

    /**
     * Gets whether double-tap zooming is enabled.
     *
     * @return True if double-tap zooming is enabled, false otherwise.
     */
    public boolean getDoubleTapZoomingEnabled() {
        return mDoubleTapZooming.get();
    }

    /**
     * Sets whether double tap zooming is enabled.
     *
     * @param flag true if double tap zooming should be enabled, false otherwise.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setDoubleTapZoomingEnabled(final boolean flag) {
        mDoubleTapZooming.commit(flag);
        return this;
    }

    /**
     * Gets the current WebGL MSAA level.
     *
     * @return number of MSAA samples, 0 if MSAA is disabled.
     */
    public int getGlMsaaLevel() {
        return mGlMsaaLevel.get();
    }

    /**
     * Sets the WebGL MSAA level.
     *
     * @param level number of MSAA samples, 0 if MSAA should be disabled.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setGlMsaaLevel(final int level) {
        mGlMsaaLevel.commit(level);
        return this;
    }

    public @Nullable RuntimeTelemetry.Delegate getTelemetryDelegate() {
        return mTelemetryProxy.getDelegate();
    }

    @Override // Parcelable
    public void writeToParcel(final Parcel out, final int flags) {
        super.writeToParcel(out, flags);

        ParcelableUtils.writeBoolean(out, mUseContentProcess);
        out.writeStringArray(mArgs);
        mExtras.writeToParcel(out, flags);
        ParcelableUtils.writeBoolean(out, mDebugPause);
        ParcelableUtils.writeBoolean(out, mUseMaxScreenDepth);
        out.writeFloat(mDisplayDensityOverride);
        out.writeInt(mDisplayDpiOverride);
        out.writeInt(mScreenWidthOverride);
        out.writeInt(mScreenHeightOverride);
        out.writeString(mCrashHandler != null ? mCrashHandler.getName() : null);
        out.writeStringArray(mRequestedLocales);
        out.writeString(mConfigFilePath);
    }

    // AIDL code may call readFromParcel even though it's not part of Parcelable.
    public void readFromParcel(final @NonNull Parcel source) {
        super.readFromParcel(source);

        mUseContentProcess = ParcelableUtils.readBoolean(source);
        mArgs = source.createStringArray();
        mExtras.readFromParcel(source);
        mDebugPause = ParcelableUtils.readBoolean(source);
        mUseMaxScreenDepth = ParcelableUtils.readBoolean(source);
        mDisplayDensityOverride = source.readFloat();
        mDisplayDpiOverride = source.readInt();
        mScreenWidthOverride = source.readInt();
        mScreenHeightOverride = source.readInt();

        final String crashHandlerName = source.readString();
        if (crashHandlerName != null) {
            try {
                @SuppressWarnings("unchecked")
                final Class<? extends Service> handler =
                        (Class<? extends Service>) Class.forName(crashHandlerName);

                mCrashHandler = handler;
            } catch (ClassNotFoundException e) {
            }
        }

        mRequestedLocales = source.createStringArray();
        mConfigFilePath = source.readString();
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
