/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.LocaleList;
import android.os.StrictMode;
import android.text.TextUtils;

import java.util.LinkedHashSet;
import java.util.Locale;
import java.util.Set;

/**
 * This is a helper class to do typical locale switching operations without
 * hitting StrictMode errors or adding boilerplate to common activity
 * subclasses.
 *
 * Either call {@link Locales#initializeLocale(Context)} in your
 * <code>onCreate</code> method, or inherit from
 * <code>LocaleAwareFragmentActivity</code> or <code>LocaleAwareActivity</code>.
 */
public class Locales {
    private static final String LOGTAG = "Locales";

    /**
     * Is only required by locale aware activities, AND  Application. In most cases you should be
     * using LocaleAwareAppCompatActivity or friends.
     * @param context
     */
    public static void initializeLocale(Context context) {
        final LocaleManager localeManager = LocaleManager.getInstance();
        final StrictMode.ThreadPolicy savedPolicy = StrictMode.allowThreadDiskReads();
        StrictMode.allowThreadDiskWrites();
        try {
            localeManager.getAndApplyPersistedLocale(context);
        } finally {
            StrictMode.setThreadPolicy(savedPolicy);
        }
    }

    /**
     * Sometimes we want just the language for a locale, not the entire language
     * tag. But Java's .getLanguage method is wrong.
     *
     * This method is equivalent to the first part of
     * {@link Locales#getLanguageTag(Locale)}.
     *
     * @return a language string, such as "he" for the Hebrew locales.
     */
    public static String getLanguage(final Locale locale) {
        // Can, but should never be, an empty string.
        final String language = locale.getLanguage();

        // Modernize certain language codes.
        if (language.equals("iw")) {
            return "he";
        }

        if (language.equals("in")) {
            return "id";
        }

        if (language.equals("ji")) {
            return "yi";
        }

        return language;
    }

    /**
     * Gecko uses locale codes like "es-ES", whereas a Java {@link Locale}
     * stringifies as "es_ES".
     *
     * This method approximates the Java 7 method
     * <code>Locale#toLanguageTag()</code>.
     *
     * @return a locale string suitable for passing to Gecko.
     */
    public static String getLanguageTag(final Locale locale) {
        // If this were Java 7:
        // return locale.toLanguageTag();

        final String language = getLanguage(locale);
        final String country = locale.getCountry(); // Can be an empty string.
        if (country.equals("")) {
            return language;
        }
        return language + "-" + country;
    }

    public static Locale parseLocaleCode(final String localeCode) {
        int index;
        if ((index = localeCode.indexOf('-')) != -1 ||
            (index = localeCode.indexOf('_')) != -1) {
            final String langCode = localeCode.substring(0, index);
            final String countryCode = localeCode.substring(index + 1);
            return new Locale(langCode, countryCode);
        }

        return new Locale(localeCode);
    }

    /**
     * Get a set of all countries (lowercase) in the list of default locales for this device.
     */
    public static Set<String> getCountriesInDefaultLocaleList() {
        final Set<String> countries = new LinkedHashSet<>();

        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.N) {
            final LocaleList list = LocaleList.getDefault();
            for (int i = 0; i < list.size(); i++) {
                final String country = list.get(i).getCountry();
                if (!TextUtils.isEmpty(country)) {
                    countries.add(country.toLowerCase(Locale.US));
                }
            }
        } else {
            final String country = Locale.getDefault().getCountry();
            if (!TextUtils.isEmpty(country)) {
                countries.add(country.toLowerCase(Locale.US));
            }
        }

        return countries;
    }

    /**
     * Get a Resources instance with the currently selected locale applied.
     */
    public static Resources getLocalizedResources(Context context) {
        final Resources currentResources = context.getResources();

        final Locale currentLocale = LocaleManager.getInstance().getCurrentLocale(context);
        @SuppressWarnings("deprecation") final Locale viewLocale = currentResources.getConfiguration().locale;

        if (currentLocale == null || viewLocale == null) {
            return currentResources;
        }

        if (currentLocale.toLanguageTag().equals(viewLocale.toLanguageTag())) {
            return currentResources;
        }

        final Configuration configuration = new Configuration(currentResources.getConfiguration());
        configuration.setLocale(currentLocale);

        return context.createConfigurationContext(configuration).getResources();
    }
}
