/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.Locale;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;

/**
 * Implement this interface to provide Fennec's locale switching functionality.
 *
 * The LocaleManager is responsible for persisting and applying selected locales,
 * and correcting configurations after Android has changed them.
 */
public interface LocaleManager {
    void initialize(Context context);

    /**
     * @return true if locale switching is enabled.
     */
    boolean isEnabled();
    Locale getCurrentLocale(Context context);
    String getAndApplyPersistedLocale(Context context);
    void correctLocale(Context context, Resources resources, Configuration newConfig);
    void updateConfiguration(Context context, Locale locale);
    String setSelectedLocale(Context context, String localeCode);
    boolean systemLocaleDidChange();
    void resetToSystemLocale(Context context);

    /**
     * Call this in your onConfigurationChanged handler. This method is expected
     * to do the appropriate thing: if the user has selected a locale, it
     * corrects the incoming configuration; if not, it signals the new locale to
     * use.
     */
    Locale onSystemConfigurationChanged(Context context, Resources resources, Configuration configuration, Locale currentActivityLocale);
    String getFallbackLocaleTag();
}
