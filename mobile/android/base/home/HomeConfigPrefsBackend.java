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

    private static final String PREFS_CONFIG_KEY = "home_panels";
    private static final String PREFS_LOCALE_KEY = "home_locale";

    private static final String RELOAD_BROADCAST = "HomeConfigPrefsBackend:Reload";

    private final Context mContext;
    private ReloadBroadcastReceiver mReloadBroadcastReceiver;
    private OnReloadListener mReloadListener;

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

        // On tablets, the history panel is the last.
        // On phones, the history panel is the first one.
        if (HardwareUtils.isTablet()) {
            panelConfigs.add(historyEntry);
        } else {
            panelConfigs.add(0, historyEntry);
        }

        return new State(panelConfigs, true);
    }

    private State loadConfigFromString(String jsonString) {
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

        return new State(panelConfigs, false);
    }

    @Override
    public State load() {
        final SharedPreferences prefs = getSharedPreferences();
        final String jsonString = prefs.getString(PREFS_CONFIG_KEY, null);

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

            editor.putString(PREFS_CONFIG_KEY, jsonPanelConfigs.toString());
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
            // see HomeConfigInvalidator.onLocaleReady().
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
