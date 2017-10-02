/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.support.annotation.VisibleForTesting;
import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.Locales;

import java.util.Collections;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/**
 * Static functions for accessing user configuration information.
 *
 * TODO (bug 1405161): we should move all configuration accesses to this class to centralize them.
 */
public class ActivityStreamConfiguration {

    private static final Set<Locale> enabledLocales;

    static {
        final String[] enabledLocaleTags = new String[] {
                // Sorted alphabetically to preserve blame for additions/removals.
                "de-AT",
                "de-CH",
                "de-DE",
                "en-GB",
                "en-US",
                "en-ZA",
        };

        final Set<Locale> mutableEnabledLocales = new HashSet<>();
        for (final String enabledLocaleTag : enabledLocaleTags) {
            mutableEnabledLocales.add(Locales.parseLocaleCode(enabledLocaleTag));
        }
        enabledLocales = Collections.unmodifiableSet(mutableEnabledLocales);
    }

    private ActivityStreamConfiguration() {}

    public static boolean isPocketEnabledByLocale(final Context context) {
        // Unintuitively, `BrowserLocaleManager.getCurrentLocale` returns the current user overridden locale,
        // or null if the user hasn't overidden the system default.
        final Locale currentLocale;
        final Locale userOverriddenLocale = BrowserLocaleManager.getInstance().getCurrentLocale(context);
        if (userOverriddenLocale != null) {
            currentLocale = userOverriddenLocale;
        } else {
            // `locale` was deprecated in API 24 but our target SDK is too low to use the new method.
            currentLocale = context.getResources().getConfiguration().locale;
        }

        return isPocketEnabledByLocaleInner(currentLocale);
    }

    @VisibleForTesting static boolean isPocketEnabledByLocaleInner(final Locale locale) {
        return enabledLocales.contains(locale);
    }
}
