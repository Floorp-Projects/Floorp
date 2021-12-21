/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import androidx.preference.PreferenceManager;
import org.mozilla.focus.R;
import org.mozilla.focus.generated.LocaleList;
import java.util.Collection;
import java.util.Locale;
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
    private static String PREF_LOCALE = null;

    // These are volatile because we don't impose restrictions
    // over which thread calls our methods.
    private volatile Locale currentLocale;
    private boolean systemLocaleDidChange;
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
     * Returns a list of supported locale codes
     */
    public static Collection<String> getPackagedLocaleTags() {
        return LocaleList.BUNDLED_LOCALES;
    }

}
