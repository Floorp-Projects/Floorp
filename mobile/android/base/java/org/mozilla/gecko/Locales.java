/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.lang.reflect.Method;
import java.util.Locale;

import org.mozilla.gecko.LocaleManager;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.StrictMode;
import android.support.v4.app.FragmentActivity;
import android.support.v7.app.AppCompatActivity;

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
    public static LocaleManager getLocaleManager() {
        try {
            final Class<?> clazz = Class.forName("org.mozilla.gecko.BrowserLocaleManager");
            final Method getInstance = clazz.getMethod("getInstance");
            final LocaleManager localeManager = (LocaleManager) getInstance.invoke(null);
            return localeManager;
        } catch (Exception e) {
          throw new RuntimeException(e);
        }
    }

    public static void initializeLocale(Context context) {
        final LocaleManager localeManager = getLocaleManager();
        final StrictMode.ThreadPolicy savedPolicy = StrictMode.allowThreadDiskReads();
        StrictMode.allowThreadDiskWrites();
        try {
            localeManager.getAndApplyPersistedLocale(context);
        } finally {
            StrictMode.setThreadPolicy(savedPolicy);
        }
    }

    public static abstract class LocaleAwareAppCompatActivity extends AppCompatActivity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            Locales.initializeLocale(getApplicationContext());
            super.onCreate(savedInstanceState);
        }

    }
    public static abstract class LocaleAwareFragmentActivity extends FragmentActivity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            Locales.initializeLocale(getApplicationContext());
            super.onCreate(savedInstanceState);
        }
    }

    public static abstract class LocaleAwareActivity extends Activity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            Locales.initializeLocale(getApplicationContext());
            super.onCreate(savedInstanceState);
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
}
