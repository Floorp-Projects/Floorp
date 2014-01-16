/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.home.HomeConfig.HomeConfigBackend;
import org.mozilla.gecko.home.HomeConfig.OnChangeListener;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.HomeConfig.PanelType;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.util.Log;

import java.util.ArrayList;
import java.util.Collections;
import java.util.EnumSet;
import java.util.List;

class HomeConfigPrefsBackend implements HomeConfigBackend {
    private static final String LOGTAG = "GeckoHomeConfigBackend";

    private static final String PREFS_KEY = "home_panels";

    private static final String JSON_KEY_TYPE = "type";
    private static final String JSON_KEY_TITLE = "title";
    private static final String JSON_KEY_ID = "id";
    private static final String JSON_KEY_DEFAULT = "default";

    private static final int IS_DEFAULT = 1;

    // UUIDs used to create PanelConfigs for default built-in panels 
    private static final String TOP_SITES_PANEL_ID = "4becc86b-41eb-429a-a042-88fe8b5a094e";
    private static final String BOOKMARKS_PANEL_ID = "7f6d419a-cd6c-4e34-b26f-f68b1b551907";
    private static final String READING_LIST_PANEL_ID = "20f4549a-64ad-4c32-93e4-1dcef792733b";
    private static final String HISTORY_PANEL_ID = "f134bf20-11f7-4867-ab8b-e8e705d7fbe8";

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

        panelConfigs.add(new PanelConfig(PanelType.TOP_SITES,
                                         mContext.getString(R.string.home_top_sites_title),
                                         TOP_SITES_PANEL_ID,
                                         EnumSet.of(PanelConfig.Flags.DEFAULT_PANEL)));

        panelConfigs.add(new PanelConfig(PanelType.BOOKMARKS,
                                         mContext.getString(R.string.bookmarks_title),
                                         BOOKMARKS_PANEL_ID));

        // We disable reader mode support on low memory devices. Hence the
        // reading list panel should not show up on such devices.
        if (!HardwareUtils.isLowMemoryPlatform()) {
            panelConfigs.add(new PanelConfig(PanelType.READING_LIST,
                                             mContext.getString(R.string.reading_list_title),
                                             READING_LIST_PANEL_ID));
        }

        final PanelConfig historyEntry = new PanelConfig(PanelType.HISTORY,
                                                         mContext.getString(R.string.home_history_title),
                                                         HISTORY_PANEL_ID);

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

                final PanelConfig panelConfig = loadPanelConfigFromJSON(jsonPanelConfig);
                panelConfigs.add(panelConfig);
            } catch (Exception e) {
                Log.e(LOGTAG, "Exception loading PanelConfig from JSON", e);
            }
        }

        return panelConfigs;
    }

    private PanelConfig loadPanelConfigFromJSON(JSONObject jsonPanelConfig)
            throws JSONException, IllegalArgumentException {
        final PanelType type = PanelType.fromId(jsonPanelConfig.getString(JSON_KEY_TYPE));
        final String title = jsonPanelConfig.getString(JSON_KEY_TITLE);
        final String id = jsonPanelConfig.getString(JSON_KEY_ID);

        final EnumSet<PanelConfig.Flags> flags = EnumSet.noneOf(PanelConfig.Flags.class);
        final boolean isDefault = (jsonPanelConfig.optInt(JSON_KEY_DEFAULT, -1) == IS_DEFAULT);
        if (isDefault) {
            flags.add(PanelConfig.Flags.DEFAULT_PANEL);
        }

        return new PanelConfig(type, title, id, flags);
    }

    @Override
    public List<PanelConfig> load() {
        final SharedPreferences prefs = getSharedPreferences();
        final String jsonString = prefs.getString(PREFS_KEY, null);

        final List<PanelConfig> panelConfigs;
        if (TextUtils.isEmpty(jsonString)) {
            panelConfigs = loadDefaultConfig();
        } else {
            panelConfigs = loadConfigFromString(jsonString);
        }

        return Collections.unmodifiableList(panelConfigs);
    }

    private JSONObject convertPanelConfigToJSON(PanelConfig PanelConfig) throws JSONException {
        final JSONObject jsonPanelConfig = new JSONObject();

        jsonPanelConfig.put(JSON_KEY_TYPE, PanelConfig.getType().toString());
        jsonPanelConfig.put(JSON_KEY_TITLE, PanelConfig.getTitle());
        jsonPanelConfig.put(JSON_KEY_ID, PanelConfig.getId());

        if (PanelConfig.isDefault()) {
            jsonPanelConfig.put(JSON_KEY_DEFAULT, IS_DEFAULT);
        }

        return jsonPanelConfig;
    }

    @Override
    public void save(List<PanelConfig> panelConfigs) {
        final JSONArray jsonPanelConfigs = new JSONArray();

        final int count = panelConfigs.size();
        for (int i = 0; i < count; i++) {
            try {
                final PanelConfig PanelConfig = panelConfigs.get(i);

                final JSONObject jsonPanelConfig = convertPanelConfigToJSON(PanelConfig);
                jsonPanelConfigs.put(jsonPanelConfig);
            } catch (Exception e) {
                Log.e(LOGTAG, "Exception converting PanelConfig to JSON", e);
            }
        }

        final SharedPreferences prefs = getSharedPreferences();
        final SharedPreferences.Editor editor = prefs.edit();

        final String jsonString = jsonPanelConfigs.toString();
        editor.putString(PREFS_KEY, jsonString);
        editor.commit();
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
            if (TextUtils.equals(key, PREFS_KEY)) {
                mChangeListener.onChange();
            }
        }
    }
}
