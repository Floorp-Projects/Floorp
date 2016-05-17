/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import static org.mozilla.gecko.home.HomeConfig.createBuiltinPanelConfig;

import java.util.ArrayList;
import java.util.EnumSet;
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
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.util.Log;

public class HomeConfigPrefsBackend implements HomeConfigBackend {
    private static final String LOGTAG = "GeckoHomeConfigBackend";

    // Increment this to trigger a migration.
    private static final int VERSION = 6;

    // This key was originally used to store only an array of panel configs.
    public static final String PREFS_CONFIG_KEY_OLD = "home_panels";

    // This key is now used to store a version number with the array of panel configs.
    public static final String PREFS_CONFIG_KEY = "home_panels_with_version";

    // Keys used with JSON object stored in prefs.
    private static final String JSON_KEY_PANELS = "panels";
    private static final String JSON_KEY_VERSION = "version";

    private static final String PREFS_LOCALE_KEY = "home_locale";

    private static final String RELOAD_BROADCAST = "HomeConfigPrefsBackend:Reload";

    private final Context mContext;
    private ReloadBroadcastReceiver mReloadBroadcastReceiver;
    private OnReloadListener mReloadListener;

    private static boolean sMigrationDone;

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
        panelConfigs.add(createBuiltinPanelConfig(mContext, PanelType.COMBINED_HISTORY));


        panelConfigs.add(createBuiltinPanelConfig(mContext, PanelType.RECENT_TABS));

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

    protected enum Position {
        NONE, // Not present.
        FRONT, // At the front of the list of panels.
        BACK, // At the back of the list of panels.
    }

    /**
     * Create and insert a built-in panel configuration.
     *
     * @param context Android context.
     * @param jsonPanels array of JSON panels to update in place.
     * @param panelType to add.
     * @param positionOnPhones where to place the new panel on phones.
     * @param positionOnTablets where to place the new panel on tablets.
     * @throws JSONException
     */
    protected static void addBuiltinPanelConfig(Context context, JSONArray jsonPanels,
            PanelType panelType, Position positionOnPhones, Position positionOnTablets) throws JSONException {
        // Add the new panel.
        final JSONObject jsonPanelConfig =
                createBuiltinPanelConfig(context, panelType).toJSON();

        // If any panel is enabled, then we should make the new panel enabled.
        jsonPanelConfig.put(PanelConfig.JSON_KEY_DISABLED,
                                 allPanelsAreDisabled(jsonPanels));

        final boolean isTablet = HardwareUtils.isTablet();
        final boolean isPhone = !isTablet;

        // Maybe add the new panel to the front of the array.
        if ((isPhone && positionOnPhones == Position.FRONT) ||
            (isTablet && positionOnTablets == Position.FRONT)) {
            // This is an inefficient way to stretch [a, b, c] to [a, a, b, c].
            for (int i = jsonPanels.length(); i >= 1; i--) {
                jsonPanels.put(i, jsonPanels.get(i - 1));
            }
            // And this inserts [d, a, b, c].
            jsonPanels.put(0, jsonPanelConfig);
        }

        // Maybe add the new panel to the back of the array.
        if ((isPhone && positionOnPhones == Position.BACK) ||
            (isTablet && positionOnTablets == Position.BACK)) {
            jsonPanels.put(jsonPanelConfig);
        }
    }

    /**
     * Updates the panels to combine the History and Sync panels into the (Combined) History panel.
     *
     * Tries to replace the History panel with the Combined History panel if visible, or falls back to
     * replacing the Sync panel if it's visible. That way, we minimize panel reordering during a migration.
     * @param context Android context
     * @param jsonPanels array of original JSON panels
     * @return new array of updated JSON panels
     * @throws JSONException
     */
    private static JSONArray combineHistoryAndSyncPanels(Context context, JSONArray jsonPanels) throws JSONException {
        EnumSet<PanelConfig.Flags> historyFlags = null;
        EnumSet<PanelConfig.Flags> syncFlags = null;

        int historyIndex = -1;
        int syncIndex = -1;

        // Determine state and location of History and Sync panels.
        for (int i = 0; i < jsonPanels.length(); i++) {
            JSONObject panelObj = jsonPanels.getJSONObject(i);
            final PanelConfig panelConfig = new PanelConfig(panelObj);
            final PanelType type = panelConfig.getType();
            if (type == PanelType.DEPRECATED_HISTORY) {
                historyIndex = i;
                historyFlags = panelConfig.getFlags();
            } else if (type == PanelType.DEPRECATED_REMOTE_TABS) {
                syncIndex = i;
                syncFlags = panelConfig.getFlags();
            } else if (type == PanelType.COMBINED_HISTORY) {
                // Partial landing of bug 1220928 combined the History and Sync panels of users who didn't
                // have home panel customizations (including new users), thus they don't this migration.
                return jsonPanels;
            }
        }

        if (historyIndex == -1 || syncIndex == -1) {
            throw new IllegalArgumentException("Expected both History and Sync panels to be present prior to Combined History.");
        }

        PanelConfig newPanel;
        int replaceIndex;
        int removeIndex;

        if (historyFlags.contains(PanelConfig.Flags.DISABLED_PANEL) && !syncFlags.contains(PanelConfig.Flags.DISABLED_PANEL)) {
            // Replace the Sync panel if it's visible and the History panel is disabled.
            replaceIndex = syncIndex;
            removeIndex = historyIndex;
            newPanel = createBuiltinPanelConfig(context, PanelType.COMBINED_HISTORY, syncFlags);
        } else {
            // Otherwise, just replace the History panel.
            replaceIndex = historyIndex;
            removeIndex = syncIndex;
            newPanel = createBuiltinPanelConfig(context, PanelType.COMBINED_HISTORY, historyFlags);
        }

        // Copy the array with updated panel and removed panel.
        final JSONArray newArray = new JSONArray();
        for (int i = 0; i < jsonPanels.length(); i++) {
            if (i == replaceIndex) {
                newArray.put(newPanel.toJSON());
            } else if (i == removeIndex) {
                continue;
            } else {
                newArray.put(jsonPanels.get(i));
            }
        }

        return newArray;
    }

    private static void ensureDefaultPanelForV5(Context context, JSONArray jsonPanels) throws JSONException {
        int historyIndex = -1;

        for (int i = 0; i < jsonPanels.length(); i++) {
            final PanelConfig panelConfig = new PanelConfig(jsonPanels.getJSONObject(i));
            if (panelConfig.isDefault()) {
                return;
            }

            if (panelConfig.getType() == PanelType.COMBINED_HISTORY) {
                historyIndex = i;
            }
        }

        // Make the History panel default. We can't modify existing PanelConfigs, so make a new one.
        final PanelConfig historyPanelConfig = createBuiltinPanelConfig(context, PanelType.COMBINED_HISTORY, EnumSet.of(PanelConfig.Flags.DEFAULT_PANEL));
        jsonPanels.put(historyIndex, historyPanelConfig.toJSON());
    }

    /**
     * Remove the reading list panel.
     * If the reading list panel used to be the default panel, we make bookmarks the new default.
     */
    private static JSONArray removeReadingListPanel(Context context, JSONArray jsonPanels) throws JSONException {
        boolean wasDefault = false;
        int bookmarksIndex = -1;

        // JSONArrary doesn't provide remove() for API < 19, therefore we need to manually copy all
        // the items we don't want deleted into a new array.
        final JSONArray newJSONPanels = new JSONArray();

        for (int i = 0; i < jsonPanels.length(); i++) {
            final JSONObject panelJSON = jsonPanels.getJSONObject(i);
            final PanelConfig panelConfig = new PanelConfig(panelJSON);

            if (panelConfig.getType() == PanelType.DEPRECATED_READING_LIST) {
                // If this panel was the default we'll need to assign a new default:
                wasDefault = panelConfig.isDefault();
            } else {
                if (panelConfig.getType() == PanelType.BOOKMARKS) {
                    bookmarksIndex = newJSONPanels.length();
                }

                newJSONPanels.put(panelJSON);
            }
        }

        if (wasDefault) {
            // This will make the bookmarks panel visible if it was previously hidden - this is desired
            // since this will make the new equivalent of the reading list visible by default.
            final JSONObject bookmarksPanelConfig = createBuiltinPanelConfig(context, PanelType.BOOKMARKS, EnumSet.of(PanelConfig.Flags.DEFAULT_PANEL)).toJSON();
            if (bookmarksIndex != -1) {
                newJSONPanels.put(bookmarksIndex, bookmarksPanelConfig);
            } else {
                newJSONPanels.put(bookmarksPanelConfig);
            }
        }

        return newJSONPanels;
    }

    /**
     * Checks to see if the reading list panel already exists.
     *
     * @param jsonPanels JSONArray array representing the curent set of panel configs.
     *
     * @return boolean Whether or not the reading list panel exists.
     */
    private static boolean readingListPanelExists(JSONArray jsonPanels) {
        final int count = jsonPanels.length();
        for (int i = 0; i < count; i++) {
            try {
                final JSONObject jsonPanelConfig = jsonPanels.getJSONObject(i);
                final PanelConfig panelConfig = new PanelConfig(jsonPanelConfig);
                if (panelConfig.getType() == PanelType.DEPRECATED_READING_LIST) {
                    return true;
                }
            } catch (Exception e) {
                // It's okay to ignore this exception, since an invalid reading list
                // panel config is equivalent to no reading list panel.
                Log.e(LOGTAG, "Exception loading PanelConfig from JSON", e);
            }
        }
        return false;
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

        JSONArray jsonPanels;
        final int version;

        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(context);
        if (prefs.contains(PREFS_CONFIG_KEY_OLD)) {
            // Our original implementation did not contain versioning, so this is implicitly version 0.
            jsonPanels = new JSONArray(jsonString);
            version = 0;
        } else {
            final JSONObject json = new JSONObject(jsonString);
            jsonPanels = json.getJSONArray(JSON_KEY_PANELS);
            version = json.getInt(JSON_KEY_VERSION);
        }

        if (version == VERSION) {
            return jsonPanels;
        }

        Log.d(LOGTAG, "Performing migration");

        final SharedPreferences.Editor prefsEditor = prefs.edit();

        for (int v = version + 1; v <= VERSION; v++) {
            Log.d(LOGTAG, "Migrating to version = " + v);

            switch (v) {
                case 1:
                    // Add "Recent Tabs" panel.
                    addBuiltinPanelConfig(context, jsonPanels,
                            PanelType.RECENT_TABS, Position.FRONT, Position.BACK);

                    // Remove the old pref key.
                    prefsEditor.remove(PREFS_CONFIG_KEY_OLD);
                    break;

                case 2:
                    // Add "Remote Tabs"/"Synced Tabs" panel.
                    addBuiltinPanelConfig(context, jsonPanels,
                            PanelType.DEPRECATED_REMOTE_TABS, Position.FRONT, Position.BACK);
                    break;

                case 3:
                    // Add the "Reading List" panel if it does not exist. At one time,
                    // the Reading List panel was shown only to devices that were not
                    // considered "low memory". Now, we expose the panel to all devices.
                    // This migration should only occur for "low memory" devices.
                    // Note: This will not agree with the default configuration, which
                    // has DEPRECATED_REMOTE_TABS after DEPRECATED_READING_LIST on some devices.
                    if (!readingListPanelExists(jsonPanels)) {
                        addBuiltinPanelConfig(context, jsonPanels,
                                PanelType.DEPRECATED_READING_LIST, Position.BACK, Position.BACK);
                    }
                    break;

                case 4:
                    // Combine the History and Sync panels. In order to minimize an unexpected reordering
                    // of panels, we try to replace the History panel if it's visible, and fall back to
                    // the Sync panel if that's visible.
                    jsonPanels = combineHistoryAndSyncPanels(context, jsonPanels);
                    break;

                case 5:
                    // This is the fix for bug 1264136 where we lost track of the default panel during some migrations.
                    ensureDefaultPanelForV5(context, jsonPanels);
                    break;

                case 6:
                    jsonPanels = removeReadingListPanel(context, jsonPanels);
                    break;
            }
        }

        // Save the new panel config and the new version number.
        final JSONObject newJson = new JSONObject();
        newJson.put(JSON_KEY_PANELS, jsonPanels);
        newJson.put(JSON_KEY_VERSION, VERSION);

        prefsEditor.putString(PREFS_CONFIG_KEY, newJson.toString());
        prefsEditor.apply();

        return jsonPanels;
    }

    private State loadConfigFromString(String jsonString) {
        final JSONArray jsonPanelConfigs;
        try {
            jsonPanelConfigs = maybePerformMigration(mContext, jsonString);
            updatePrefsFromConfig(jsonPanelConfigs);
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
        editor.apply();

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
            editor.apply();

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

    /**
     * Update prefs that depend on home panels state.
     *
     * This includes the prefs that keep track of whether bookmarks or history are enabled, which are
     * used to control the visibility of the corresponding menu items.
     */
    private void updatePrefsFromConfig(JSONArray panelsArray) {
        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(mContext);
        if (!prefs.contains(HomeConfig.PREF_KEY_BOOKMARKS_PANEL_ENABLED)
                || !prefs.contains(HomeConfig.PREF_KEY_HISTORY_PANEL_ENABLED)) {

            final String bookmarkType = PanelType.BOOKMARKS.toString();
            final String historyType = PanelType.COMBINED_HISTORY.toString();
            try {
                for (int i = 0; i < panelsArray.length(); i++) {
                    final JSONObject panelObj = panelsArray.getJSONObject(i);
                    final String panelType = panelObj.optString(PanelConfig.JSON_KEY_TYPE, null);
                    if (panelType == null) {
                        break;
                    }
                    final boolean isDisabled = panelObj.optBoolean(PanelConfig.JSON_KEY_DISABLED, false);
                    if (bookmarkType.equals(panelType)) {
                        prefs.edit().putBoolean(HomeConfig.PREF_KEY_BOOKMARKS_PANEL_ENABLED, !isDisabled).apply();
                    } else if (historyType.equals(panelType)) {
                        prefs.edit().putBoolean(HomeConfig.PREF_KEY_HISTORY_PANEL_ENABLED, !isDisabled).apply();
                    }
                }
            } catch (JSONException e) {
                Log.e(LOGTAG, "Error fetching panel from config to update prefs");
            }
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
