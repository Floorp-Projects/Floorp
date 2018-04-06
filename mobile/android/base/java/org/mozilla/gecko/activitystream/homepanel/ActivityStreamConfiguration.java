/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;

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

    private static final Set<Locale> pocketEnabledLocales;

    /**
     * Notes for testing: Firefox for Android can get its locale from two places: the system locale and an override for
     * this locale specified in Fennec settings. As such, you should be sure to test all locale updates with both
     * settings when possible. For example, Deutsch will be "de-DE" in the Android system but "de" when using the Fennec
     * override.
     */
    @VisibleForTesting static final String[] pocketEnabledLocaleTags = new String[] {
            // Sorted alphabetically to preserve blame for additions/removals.
            "de", // used by Firefox multi-locale builds.
            "de-AT",
            "de-CH",
            "de-DE",
            "en-GB",
            "en-US",
            "en-ZA",
    };

    static {
        final Set<Locale> mutableEnabledLocales = new HashSet<>();
        for (final String enabledLocaleTag : pocketEnabledLocaleTags) {
            mutableEnabledLocales.add(Locales.parseLocaleCode(enabledLocaleTag));
        }
        pocketEnabledLocales = Collections.unmodifiableSet(mutableEnabledLocales);
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

    public static boolean isPocketRecommendingTopSites(final Context context) {
        return isPocketEnabledByLocale(context) &&
                GeckoSharedPrefs.forProfile(context).getBoolean(ActivityStreamPanel.PREF_POCKET_ENABLED,
                        context.getResources().getBoolean(R.bool.pref_activitystream_pocket_enabled_default));
    }

    @VisibleForTesting static boolean isPocketEnabledByLocaleInner(final Locale locale) {
        // This comparison will use Locale.equals and thus compare all Locale fields including variant (some variant
        // examples are "scotland" (Scottish english) and "oxendict" (Oxford Dictionary standard spelling)). The tags
        // we've whitelisted have no variant so they will fail to match locales with variants, even if the language and
        // country are the same. In practice, I can't find a use of variants in Android locales, so I've opted to leave
        // this comparison the way it is to 1) avoid churn, 2) keep the code simple, and 3) because I don't know how
        // Product would want to handle these variants. If we wanted to ignore variants in comparisons, we could compare
        // the language and country only.
        return pocketEnabledLocales.contains(locale);
    }
}
