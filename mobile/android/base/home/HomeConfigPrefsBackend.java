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
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.home.HomeConfig.HomeConfigBackend;
import org.mozilla.gecko.home.HomeConfig.OnReloadListener;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.HomeConfig.PanelType;
import org.mozilla.gecko.home.HomeConfig.State;
import org.mozilla.gecko.util.HardwareUtils;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.util.Log;

class HomeConfigPrefsBackend implements HomeConfigBackend {
    private static final String LOGTAG = "GeckoHomeConfigBackend";

    // Increment this to trigger a migration.
    private static final int VERSION = 1;

    // This key was originally used to store only an array of panel configs.
    private static final String PREFS_CONFIG_KEY_OLD = "home_panels";

    // This key is now used to store a version number with the array of panel configs.
    private static final String PREFS_CONFIG_KEY = "home_panels_with_version";

    // Keys used with JSON object stored in prefs.
    private static final String JSON_KEY_PANELS = "panels";
    private static final String JSON_KEY_VERSION = "version";

    private static final String PREFS_LOCALE_KEY = "home_locale";

    private static final String RELOAD_BROADCAST = "HomeConfigPrefsBackend:Reload";

    private final Context mContext;
    private ReloadBroadcastReceiver mReloadBroadcastReceiver;
    private OnReloadListener mReloadListener;

    private static boolean sMigrationDone = false;

    public HomeConfigPrefsBackend(Context context) {
        mContext = context;
    }

    private SharedPreferences getSharedPreferences() {
        return GeckoSharedPrefs.forProfile(mContext);
    }

    private State loadDefaultConfig() {
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
        final PanelConfig recentTabsEntry = createBuiltinPanelConfig(mContext, PanelType.RECENT_TABS);

        // On tablets, the history panel is the last.
        // On phones, the history panel is the first one.
        if (HardwareUtils.isTablet()) {
            panelConfigs.add(historyEntry);
            panelConfigs.add(recentTabsEntry);
        } else {
            panelConfigs.add(0, historyEntry);
            panelConfigs.add(0, recentTabsEntry);
        }

        return new State(panelConfigs, true);
    }

    /**
     * Iterate through the panels to check if they are all disabled.
     */
    private static boolean allPanelsAreDisabled(JSONArray jsonPanels) throws JSONException {
        final int count = jsonPanels.length();
        for (int i = 0; i < count; i++) {
            final JSONObject jsonPanelConfig = jsonPanels.getJSONObject(i);

            if (!jsonPanelConfig.optBoolean(PanelConfig.JSON_KEY_DISABLED, false)) {
                return false;
            }
        }

        return true;
    }

    /**
     * Migrates JSON config data storage.
     *
     * @param context Context used to get shared preferences and create built-in panel.
     * @param jsonString String currently stored in preferences.
     *
     * @return JSONArray array representing new set of panel configs.
     */
    private static synchronized JSONArray maybePerformMigration(Context context, String jsonString) throws JSONException {
        // If the migration is already done, we're at the current version.
        if (sMigrationDone) {
            final JSONObject json = new JSONObject(jsonString);
            return json.getJSONArray(JSON_KEY_PANELS);
        }

        // Make sure we only do this version check once.
        sMigrationDone = true;

        final JSONArray originalJsonPanels;
        final int version;

        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(context);
        if (prefs.contains(PREFS_CONFIG_KEY_OLD)) {
            // Our original implementation did not contain versioning, so this is implicitly version 0.
            originalJsonPanels = new JSONArray(jsonString);
            version = 0;
        } else {
            final JSONObject json = new JSONObject(jsonString);
            originalJsonPanels = json.getJSONArray(JSON_KEY_PANELS);
            version = json.getInt(JSON_KEY_VERSION);
        }

        if (version == VERSION) {
            return originalJsonPanels;
        }

        Log.d(LOGTAG, "Performing migration");

        final JSONArray newJsonPanels = new JSONArray();
        final SharedPreferences.Editor prefsEditor = prefs.edit();

        for (int v = version + 1; v <= VERSION; v++) {
            Log.d(LOGTAG, "Migrating to version = " + v);

            switch (v) {
                case 1:
                    // Add "Recent Tabs" panel
                    final JSONObject jsonRecentTabsConfig =
                            createBuiltinPanelConfig(context, PanelType.RECENT_TABS).toJSON();

                    // If any panel is enabled, then we should make the recent tabs
                    // panel enabled.
                    jsonRecentTabsConfig.put(PanelConfig.JSON_KEY_DISABLED,
                                             allPanelsAreDisabled(originalJsonPanels));

                    // Add the new panel to the front of the array on phones.
                    if (!HardwareUtils.isTablet()) {
                        newJsonPanels.put(jsonRecentTabsConfig);
                    }

                    // Copy the original panel configs.
                    final int count = originalJsonPanels.length();
                    for (int i = 0; i < count; i++) {
                        final JSONObject jsonPanelConfig = originalJsonPanels.getJSONObject(i);
                        newJsonPanels.put(jsonPanelConfig);
                    }

                    // Add the new panel to the end of the array on tablets.
                    if (HardwareUtils.isTablet()) {
                        newJsonPanels.put(jsonRecentTabsConfig);
                    }

                    // Remove the old pref key.
                    prefsEditor.remove(PREFS_CONFIG_KEY_OLD);
                    break;
            }
        }

        // Save the new panel config and the new version number.
        final JSONObject newJson = new JSONObject();
        newJson.put(JSON_KEY_PANELS, newJsonPanels);
        newJson.put(JSON_KEY_VERSION, VERSION);

        prefsEditor.putString(PREFS_CONFIG_KEY, newJson.toString());
        prefsEditor.commit();

        return newJsonPanels;
    }

    private State loadConfigFromString(String jsonString) {
        final JSONArray jsonPanelConfigs;
        try {
            jsonPanelConfigs = maybePerformMigration(mContext, jsonString);
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

        return new State(panelConfigs, false);
    }

    @Override
    public State load() {
        final SharedPreferences prefs = getSharedPreferences();

        final String key = (prefs.contains(PREFS_CONFIG_KEY_OLD) ? PREFS_CONFIG_KEY_OLD : PREFS_CONFIG_KEY);
        final String jsonString = prefs.getString(key, null);

        final State configState;
        if (TextUtils.isEmpty(jsonString)) {
            configState = loadDefaultConfig();
        } else {
            configState = loadConfigFromString(jsonString);
        }

        return configState;
    }

    @Override
    public void save(State configState) {
        final SharedPreferences prefs = getSharedPreferences();
        final SharedPreferences.Editor editor = prefs.edit();

        // No need to save the state to disk if it represents the default
        // HomeConfig configuration. Simply force all existing HomeConfigLoader
        // instances to refresh their contents.
        if (!configState.isDefault()) {
            final JSONArray jsonPanelConfigs = new JSONArray();

            for (PanelConfig panelConfig : configState) {
                try {
                    final JSONObject jsonPanelConfig = panelConfig.toJSON();
                    jsonPanelConfigs.put(jsonPanelConfig);
                } catch (Exception e) {
                    Log.e(LOGTAG, "Exception converting PanelConfig to JSON", e);
                }
            }

            try {
                final JSONObject json = new JSONObject();
                json.put(JSON_KEY_PANELS, jsonPanelConfigs);
                json.put(JSON_KEY_VERSION, VERSION);

                editor.putString(PREFS_CONFIG_KEY, json.toString());
            } catch (JSONException e) {
                Log.e(LOGTAG, "Exception saving PanelConfig state", e);
            }
        }

        editor.putString(PREFS_LOCALE_KEY, Locale.getDefault().toString());
        editor.commit();

        // Trigger reload listeners on all live backend instances
        sendReloadBroadcast();
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
            // see HomePanelsManager.onLocaleReady().
            if (!prefs.contains(PREFS_CONFIG_KEY)) {
                locale = currentLocale;
            }
        }

        return locale;
    }

    @Override
    public void setOnReloadListener(OnReloadListener listener) {
        if (mReloadListener != null) {
            unregisterReloadReceiver();
            mReloadBroadcastReceiver = null;
        }

        mReloadListener = listener;

        if (mReloadListener != null) {
            mReloadBroadcastReceiver = new ReloadBroadcastReceiver();
            registerReloadReceiver();
        }
    }

    private void sendReloadBroadcast() {
        final LocalBroadcastManager lbm = LocalBroadcastManager.getInstance(mContext);
        final Intent reloadIntent = new Intent(RELOAD_BROADCAST);
        lbm.sendBroadcast(reloadIntent);
    }

    private void registerReloadReceiver() {
        final LocalBroadcastManager lbm = LocalBroadcastManager.getInstance(mContext);
        lbm.registerReceiver(mReloadBroadcastReceiver, new IntentFilter(RELOAD_BROADCAST));
    }

    private void unregisterReloadReceiver() {
        final LocalBroadcastManager lbm = LocalBroadcastManager.getInstance(mContext);
        lbm.unregisterReceiver(mReloadBroadcastReceiver);
    }

    private class ReloadBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            mReloadListener.onReload();
        }
    }
}
