/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.Log;

import androidx.preference.PreferenceManager;

import org.mozilla.focus.R;
import org.mozilla.focus.generated.LocaleList;

import java.util.Collection;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * This class manages persistence, application, and otherwise handling of
 * user-specified locales.
 *
 * Of note:
 *
 * * It's a singleton, because its scope extends to that of the application,
 *   and definitionally all changes to the locale of the app must go through
 *   this.
 * * It's lazy.
 * * It relies on using the SharedPreferences file owned by the app for performance.
 */
public class LocaleManager {
    private static final String LOG_TAG = "GeckoLocales";

    private static String PREF_LOCALE = null;

    private static final String FALLBACK_LOCALE_TAG = "en-US";

    // These are volatile because we don't impose restrictions
    // over which thread calls our methods.
    private volatile Locale currentLocale;
    private volatile Locale systemLocale = Locale.getDefault();

    private final AtomicBoolean inited = new AtomicBoolean(false);
    private boolean systemLocaleDidChange;
    private BroadcastReceiver receiver;

    private static final AtomicReference<LocaleManager> instance = new AtomicReference<>();

    public static LocaleManager getInstance() {
        LocaleManager localeManager = instance.get();
        if (localeManager != null) {
            return localeManager;
        }

        localeManager = new LocaleManager();
        if (instance.compareAndSet(null, localeManager)) {
            return localeManager;
        } else {
            return instance.get();
        }
    }

    /**
     * Ensure that you call this early in your application startup,
     * and with a context that's sufficiently long-lived (typically
     * the application context).
     *
     * Calling multiple times is harmless.
     */
    public void initialize(final Context context) {
        if (!inited.compareAndSet(false, true)) {
            return;
        }

        receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                final Locale current = systemLocale;

                // We don't trust Locale.getDefault() here, because we make a
                // habit of mutating it! Use the one Android supplies, because
                // that gets regularly reset.
                // The default value of systemLocale is fine, because we haven't
                // yet swizzled Locale during static initialization.
                systemLocale = context.getResources().getConfiguration().locale;
                systemLocaleDidChange = true;

                Log.d(LOG_TAG, "System locale changed from " + current + " to " + systemLocale);
            }
        };
        context.registerReceiver(receiver, new IntentFilter(Intent.ACTION_LOCALE_CHANGED));
    }

    public boolean systemLocaleDidChange() {
        return systemLocaleDidChange;
    }

    /**
     * Every time the system gives us a new configuration, it
     * carries the external locale. Fix it.
     */
    public void correctLocale(Context context, Resources res, Configuration config) {
        final Locale current = getCurrentLocale(context);
        if (current == null) {
            Log.d(LOG_TAG, "No selected locale. No correction needed.");
            return;
        }

        // I know it's tempting to short-circuit here if the config seems to be
        // up-to-date, but the rest is necessary.

        config.locale = current;

        // The following two lines are heavily commented in case someone
        // decides to chase down performance improvements and decides to
        // question what's going on here.
        // Both lines should be cheap, *but*...

        // This is unnecessary for basic string choice, but it almost
        // certainly comes into play when rendering numbers, deciding on RTL,
        // etc. Take it out if you can prove that's not the case.
        Locale.setDefault(current);

        // This seems to be a no-op, but every piece of documentation under the
        // sun suggests that it's necessary, and it certainly makes sense.
        res.updateConfiguration(config, null);
    }

    /**
     * We can be in one of two states.
     *
     * If the user has not explicitly chosen a Firefox-specific locale, we say
     * we are "mirroring" the system locale.
     *
     * When we are not mirroring, system locale changes do not impact Firefox
     * and are essentially ignored; the user's locale selection is the only
     * thing we care about, and we actively correct incoming configuration
     * changes to reflect the user's chosen locale.
     *
     * By contrast, when we are mirroring, system locale changes cause Firefox
     * to reflect the new system locale, as if the user picked the new locale.
     *
     * If we're currently mirroring the system locale, this method returns the
     * supplied configuration's locale, unless the current activity locale is
     * correct. If we're not currently mirroring, this method updates the
     * configuration object to match the user's currently selected locale, and
     * returns that, unless the current activity locale is correct.
     *
     * If the current activity locale is correct, returns null.
     *
     * The caller is expected to redisplay themselves accordingly.
     *
     * This method is intended to be called from inside
     * <code>onConfigurationChanged(Configuration)</code> as part of a strategy
     * to detect and either apply or undo system locale changes.
     */
    public Locale onSystemConfigurationChanged(final Context context, final Resources resources, final Configuration configuration, final Locale currentActivityLocale) {
        if (!isMirroringSystemLocale(context)) {
            correctLocale(context, resources, configuration);
        }

        final Locale changed = configuration.locale;
        if (changed.equals(currentActivityLocale)) {
            return null;
        }

        return changed;
    }

    public String getAndApplyPersistedLocale(Context context) {
        initialize(context);

        final long t1 = android.os.SystemClock.uptimeMillis();
        final String localeCode = getPersistedLocale(context);
        if (localeCode == null) {
            return null;
        }

        // Note that we don't tell Gecko about this. We notify Gecko when the
        // locale is set, not when we update Java.
        final String resultant = updateLocale(context, localeCode);

        if (resultant == null) {
            // Update the configuration anyway.
            updateConfiguration(context, currentLocale);
        }

        final long t2 = android.os.SystemClock.uptimeMillis();
        Log.i(LOG_TAG, "Locale read and update took: " + (t2 - t1) + "ms.");
        return resultant;
    }

    /**
     * Returns the set locale if it changed.
     *
     * Always persists and notifies Gecko.
     */
    public String setSelectedLocale(Context context, String localeCode) {
        final String resultant = updateLocale(context, localeCode);

        // We always persist and notify Gecko, even if nothing seemed to
        // change. This might happen if you're picking a locale that's the same
        // as the current OS locale. The OS locale might change next time we
        // launch, and we need the Gecko pref and persisted locale to have been
        // set by the time that happens.
        persistLocale(context, localeCode);

        return resultant;
    }

    public void resetToSystemLocale(Context context) {
        // Wipe the pref.
        final SharedPreferences settings = getSharedPreferences(context);
        settings.edit().remove(PREF_LOCALE).apply();

        // Apply the system locale.
        updateLocale(context, systemLocale);
    }

    /**
     * This is public to allow for an activity to force the
     * current locale to be applied if necessary (e.g., when
     * a new activity launches).
     */
    public void updateConfiguration(Context context, Locale locale) {
        Resources res = context.getResources();
        Configuration config = res.getConfiguration();

        // We should use setLocale, but it's unexpectedly missing
        // on real devices.
        config.locale = locale;

        config.setLayoutDirection(locale);

        res.updateConfiguration(config, null);
    }

    private SharedPreferences getSharedPreferences(final Context context) {
        if (PREF_LOCALE == null) {
            PREF_LOCALE = context.getResources().getString(R.string.pref_key_locale);
        }

        return PreferenceManager.getDefaultSharedPreferences(context);
    }

    /**
     * @return the persisted locale in Java format: "en_US".
     */
    private String getPersistedLocale(Context context) {
        final SharedPreferences settings = getSharedPreferences(context);
        final String locale = settings.getString(PREF_LOCALE, "");

        if ("".equals(locale)) {
            return null;
        }
        return locale;
    }

    private void persistLocale(Context context, String localeCode) {
        final SharedPreferences settings = getSharedPreferences(context);
        settings.edit().putString(PREF_LOCALE, localeCode).apply();
    }

    public Locale getCurrentLocale(Context context) {
        if (currentLocale != null) {
            return currentLocale;
        }

        final String current = getPersistedLocale(context);
        if (current == null) {
            return null;
        }
        return currentLocale = Locales.parseLocaleCode(current);
    }

    /**
     * Updates the Java locale and the Android configuration.
     *
     * Returns the persisted locale if it differed.
     *
     * Does not notify Gecko.
     *
     * @param localeCode a locale string in Java format: "en_US".
     * @return if it differed, a locale string in Java format: "en_US".
     */
    private String updateLocale(Context context, String localeCode) {
        // Fast path.
        final Locale defaultLocale = Locale.getDefault();
        Log.d("LOCALE", "Trying to check locale");
        if (defaultLocale.toString().equals(localeCode)) {
            Log.d("LOCALE", "Early return");
            return null;
        }

        final Locale locale = Locales.parseLocaleCode(localeCode);

        return updateLocale(context, locale);
    }

    /**
     * @return the Java locale string: e.g., "en_US".
     */
    private String updateLocale(Context context, final Locale locale) {
        // Fast path.
        if (Locale.getDefault().equals(locale)) {
            return null;
        }

        Locale.setDefault(locale);
        currentLocale = locale;

        // Update resources.
        updateConfiguration(context, locale);

        return locale.toString();
    }

    public boolean isMirroringSystemLocale(final Context context) {
        return getPersistedLocale(context) == null;
    }

    /**
     * Returns a list of supported locale codes
     */
    public static Collection<String> getPackagedLocaleTags() {
        return LocaleList.BUNDLED_LOCALES;
    }

    public static String getFallbackLocaleTag() {
        return FALLBACK_LOCALE_TAG;
    }
}
