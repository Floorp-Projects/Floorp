/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import static org.mozilla.gecko.home.HomeConfig.createBuiltinPanelConfig;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;
import java.util.Locale;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.home.HomeConfig.HomeConfigBackend;
import org.mozilla.gecko.home.HomeConfig.OnChangeListener;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.HomeConfig.PanelType;
import org.mozilla.gecko.util.HardwareUtils;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.util.Log;

class HomeConfigPrefsBackend implements HomeConfigBackend {
    private static final String LOGTAG = "GeckoHomeConfigBackend";

    private static final String PREFS_CONFIG_KEY = "home_panels";
    private static final String PREFS_LOCALE_KEY = "home_locale";

    private final Context mContext;
    private PrefsListener mPrefsListener;
    private OnChangeListener mChangeListener;

    public HomeConfigPrefsBackend(Context context) {
        mContext = context;
    }

    private SharedPreferences getSharedPreferences() {
        return PreferenceManager.getDefaultSharedPreferences(mContext);
    }

    private List<PanelConfig> loadDefaultConfig() {
        final ArrayList<PanelConfig> panelConfigs = new ArrayList<PanelConfig>();

        panelConfigs.add(createBuiltinPanelConfig(mContext, PanelType.TOP_SITES,
                                                  EnumSet.of(PanelConfig.Flags.DEFAULT_PANEL)));

        panelConfigs.add(createBuiltinPanelConfig(mContext, PanelType.BOOKMARKS));

        // We disable reader mode support on low memory devices. Hence the
        // reading list panel should not show up on such devices.
        if (!HardwareUtils.isLowMemoryPlatform()) {
            panelConfigs.add(createBuiltinPanelConfig(mContext, PanelType.READING_LIST));
        }

        final PanelConfig historyEntry = createBuiltinPanelConfig(mContext, PanelType.HISTORY);

        // On tablets, the history panel is the last.
        // On phones, the history panel is the first one.
        if (HardwareUtils.isTablet()) {
            panelConfigs.add(historyEntry);
        } else {
            panelConfigs.add(0, historyEntry);
        }

        return panelConfigs;
    }

    private List<PanelConfig> loadConfigFromString(String jsonString) {
        final JSONArray jsonPanelConfigs;
        try {
            jsonPanelConfigs = new JSONArray(jsonString);
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error loading the list of home panels from JSON prefs", e);

            // Fallback to default config
            return loadDefaultConfig();
        }

        final ArrayList<PanelConfig> panelConfigs = new ArrayList<PanelConfig>();

        final int count = jsonPanelConfigs.length();
        for (int i = 0; i < count; i++) {
            try {
                final JSONObject jsonPanelConfig = jsonPanelConfigs.getJSONObject(i);
                final PanelConfig panelConfig = new PanelConfig(jsonPanelConfig);
                panelConfigs.add(panelConfig);
            } catch (Exception e) {
                Log.e(LOGTAG, "Exception loading PanelConfig from JSON", e);
            }
        }

        return panelConfigs;
    }

    @Override
    public List<PanelConfig> load() {
        final SharedPreferences prefs = getSharedPreferences();
        final String jsonString = prefs.getString(PREFS_CONFIG_KEY, null);

        final List<PanelConfig> panelConfigs;
        if (TextUtils.isEmpty(jsonString)) {
            panelConfigs = loadDefaultConfig();
        } else {
            panelConfigs = loadConfigFromString(jsonString);
        }

        return panelConfigs;
    }

    @Override
    public void save(List<PanelConfig> panelConfigs) {
        final JSONArray jsonPanelConfigs = new JSONArray();

        final int count = panelConfigs.size();
        for (int i = 0; i < count; i++) {
            try {
                final PanelConfig panelConfig = panelConfigs.get(i);
                final JSONObject jsonPanelConfig = panelConfig.toJSON();
                jsonPanelConfigs.put(jsonPanelConfig);
            } catch (Exception e) {
                Log.e(LOGTAG, "Exception converting PanelConfig to JSON", e);
            }
        }

        final SharedPreferences prefs = getSharedPreferences();
        final SharedPreferences.Editor editor = prefs.edit();

        final String jsonString = jsonPanelConfigs.toString();
        editor.putString(PREFS_CONFIG_KEY, jsonString);
        editor.putString(PREFS_LOCALE_KEY, Locale.getDefault().toString());
        editor.commit();
    }

    @Override
    public String getLocale() {
        final SharedPreferences prefs = getSharedPreferences();

        String locale = prefs.getString(PREFS_LOCALE_KEY, null);
        if (locale == null) {
            // Initialize config with the current locale
            final String currentLocale = Locale.getDefault().toString();

            final SharedPreferences.Editor editor = prefs.edit();
            editor.putString(PREFS_LOCALE_KEY, currentLocale);
            editor.commit();

            // If the user has saved HomeConfig before, return null this
            // one time to trigger a refresh and ensure we use the
            // correct locale for the saved state. For more context,
            // see HomeConfigInvalidator.onLocaleReady().
            if (!prefs.contains(PREFS_CONFIG_KEY)) {
                locale = currentLocale;
            }
        }

        return locale;
    }

    @Override
    public void setOnChangeListener(OnChangeListener listener) {
        final SharedPreferences prefs = getSharedPreferences();

        if (mChangeListener != null) {
            prefs.unregisterOnSharedPreferenceChangeListener(mPrefsListener);
            mPrefsListener = null;
        }

        mChangeListener = listener;

        if (mChangeListener != null) {
            mPrefsListener = new PrefsListener();
            prefs.registerOnSharedPreferenceChangeListener(mPrefsListener);
        }
    }

    private class PrefsListener implements OnSharedPreferenceChangeListener {
        @Override
        public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
            if (TextUtils.equals(key, PREFS_CONFIG_KEY)) {
                mChangeListener.onChange();
            }
        }
    }
}
