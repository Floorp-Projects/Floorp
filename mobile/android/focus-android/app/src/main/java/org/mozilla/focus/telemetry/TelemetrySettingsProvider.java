/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry;

import android.content.Context;
import android.content.res.Resources;

import org.mozilla.focus.R;
import org.mozilla.focus.search.SearchEngineManager;
import org.mozilla.focus.utils.Browsers;
import org.mozilla.telemetry.TelemetryHolder;
import org.mozilla.telemetry.measurement.SettingsMeasurement;

/**
 * SharedPreferenceSettingsProvider implementation that additionally injects settings value for
 * runtime preferences like "default browser" and "search engine".
 */
/* package */ class TelemetrySettingsProvider extends SettingsMeasurement.SharedPreferenceSettingsProvider {
    private final Context context;
    private final String prefKeyDefaultBrowser;
    private final String prefKeySearchEngine;

    /* package */ TelemetrySettingsProvider(Context context) {
        this.context = context;

        final Resources resources = context.getResources();
        prefKeyDefaultBrowser = resources.getString(R.string.pref_key_default_browser);
        prefKeySearchEngine = resources.getString(R.string.pref_key_search_engine);
    }

    public boolean containsKey(String key) {
        if (key.equals(prefKeyDefaultBrowser)) {
            // Not actually a setting - but we want to report this like a setting.
            return true;
        }

        if (key.equals(prefKeySearchEngine)) {
            // We always want to report the current search engine - even if it's not in settings yet.
            return true;
        }

        return super.containsKey(key);
    }

    @Override
    public java.lang.Object getValue(String key) {
        if (key.equals(prefKeyDefaultBrowser)) {
            // The default browser is not actually a setting. We determine if we are the
            // default and then inject this into telemetry.
            final Context context = TelemetryHolder.get().getConfiguration().getContext();
            final Browsers browsers = new Browsers(context, Browsers.TRADITIONAL_BROWSER_URL);
            return Boolean.toString(browsers.isDefaultBrowser(context));
        }

        if (key.equals(prefKeySearchEngine)) {
            java.lang.Object value = super.getValue(key);
            if (value == null) {
                // If the user has never selected a search engine then this value is null.
                // However we still want to report the current search engine of the user.
                // Therefore we inject this value at runtime.
                value = SearchEngineManager.getInstance().getDefaultSearchEngine(context).getName();
            } else if (SearchEngineManager.getInstance().isCustomSearchEngine((String) value, context)) {
                // Don't collect possibly sensitive info for custom search engines, send "custom" instead
                value = SearchEngineManager.ENGINE_TYPE_CUSTOM;
            }
            return value;
        }

        return super.getValue(key);
    }
}
