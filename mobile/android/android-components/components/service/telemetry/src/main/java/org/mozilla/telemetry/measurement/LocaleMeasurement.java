/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import java.util.Locale;

public class LocaleMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "locale";

    public LocaleMeasurement() {
        super(FIELD_NAME);
    }

    @Override
    public Object flush() {
        return getLanguageTag(Locale.getDefault());
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
    public String getLanguageTag(final Locale locale) {
        // If this were Java 7:
        // return locale.toLanguageTag();

        final String language = getLanguage(locale);
        final String country = locale.getCountry(); // Can be an empty string.
        if (country.equals("")) {
            return language;
        }
        return language + "-" + country;
    }

    /**
     * Sometimes we want just the language for a locale, not the entire language
     * tag. But Java's .getLanguage method is wrong.
     *
     * @return a language string, such as "he" for the Hebrew locales.
     */
    private String getLanguage(final Locale locale) {
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
}
