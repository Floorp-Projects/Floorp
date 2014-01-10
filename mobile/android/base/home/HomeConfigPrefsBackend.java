/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.home.HomeConfig.HomeConfigBackend;
import org.mozilla.gecko.home.HomeConfig.OnChangeListener;
import org.mozilla.gecko.home.HomeConfig.PageEntry;
import org.mozilla.gecko.home.HomeConfig.PageType;
import org.mozilla.gecko.home.ListManager.ListInfo;
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

    private final Context mContext;
    private PrefsListener mPrefsListener;
    private OnChangeListener mChangeListener;

    public HomeConfigPrefsBackend(Context context) {
        mContext = context;
    }

    private SharedPreferences getSharedPreferences() {
        return PreferenceManager.getDefaultSharedPreferences(mContext);
    }

    private List<PageEntry> loadDefaultConfig() {
        final ArrayList<PageEntry> pageEntries = new ArrayList<PageEntry>();

        pageEntries.add(new PageEntry(PageType.TOP_SITES,
                                      mContext.getString(R.string.home_top_sites_title),
                                      EnumSet.of(PageEntry.Flags.DEFAULT_PAGE)));

        pageEntries.add(new PageEntry(PageType.BOOKMARKS,
                                      mContext.getString(R.string.bookmarks_title)));

        // We disable reader mode support on low memory devices. Hence the
        // reading list page should not show up on such devices.
        if (!HardwareUtils.isLowMemoryPlatform()) {
            pageEntries.add(new PageEntry(PageType.READING_LIST,
                                          mContext.getString(R.string.reading_list_title)));
        }

        final PageEntry historyEntry = new PageEntry(PageType.HISTORY,
                                                     mContext.getString(R.string.home_history_title));

        // On tablets, the history page is the last.
        // On phones, the history page is the first one.
        if (HardwareUtils.isTablet()) {
            pageEntries.add(historyEntry);
        } else {
            pageEntries.add(0, historyEntry);
        }

        return pageEntries;
    }

    private List<PageEntry> loadConfigFromString(String jsonString) {
        final JSONArray jsonPageEntries;
        try {
            jsonPageEntries = new JSONArray(jsonString);
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error loading the list of home pages from JSON prefs", e);

            // Fallback to default config
            return loadDefaultConfig();
        }

        final ArrayList<PageEntry> pageEntries = new ArrayList<PageEntry>();

        final int count = jsonPageEntries.length();
        for (int i = 0; i < count; i++) {
            try {
                final JSONObject jsonPageEntry = jsonPageEntries.getJSONObject(i);

                final PageEntry pageEntry = loadPageEntryFromJSON(jsonPageEntry);
                pageEntries.add(pageEntry);
            } catch (Exception e) {
                Log.e(LOGTAG, "Exception loading page entry from JSON", e);
            }
        }

        return pageEntries;
    }

    private PageEntry loadPageEntryFromJSON(JSONObject jsonPageEntry)
            throws JSONException, IllegalArgumentException {
        final PageType type = PageType.fromId(jsonPageEntry.getString(JSON_KEY_TYPE));
        final String title = jsonPageEntry.getString(JSON_KEY_TITLE);
        final String id = jsonPageEntry.getString(JSON_KEY_ID);

        final EnumSet<PageEntry.Flags> flags = EnumSet.noneOf(PageEntry.Flags.class);
        final boolean isDefault = (jsonPageEntry.optInt(JSON_KEY_DEFAULT, -1) == IS_DEFAULT);
        if (isDefault) {
            flags.add(PageEntry.Flags.DEFAULT_PAGE);
        }

        return new PageEntry(type, title, id, flags);
    }

    @Override
    public List<PageEntry> load() {
        final SharedPreferences prefs = getSharedPreferences();
        final String jsonString = prefs.getString(PREFS_KEY, null);

        final List<PageEntry> pageEntries;
        if (TextUtils.isEmpty(jsonString)) {
            pageEntries = loadDefaultConfig();
        } else {
            pageEntries = loadConfigFromString(jsonString);
        }

        return Collections.unmodifiableList(pageEntries);
    }

    private JSONObject convertPageEntryToJSON(PageEntry pageEntry) throws JSONException {
        final JSONObject jsonPageEntry = new JSONObject();

        jsonPageEntry.put(JSON_KEY_TYPE, pageEntry.getType().toString());
        jsonPageEntry.put(JSON_KEY_TITLE, pageEntry.getTitle());
        jsonPageEntry.put(JSON_KEY_ID, pageEntry.getId());

        if (pageEntry.isDefault()) {
            jsonPageEntry.put(JSON_KEY_DEFAULT, IS_DEFAULT);
        }

        return jsonPageEntry;
    }

    @Override
    public void save(List<PageEntry> pageEntries) {
        final JSONArray jsonPageEntries = new JSONArray();

        final int count = pageEntries.size();
        for (int i = 0; i < count; i++) {
            try {
                final PageEntry pageEntry = pageEntries.get(i);

                final JSONObject jsonPageEntry = convertPageEntryToJSON(pageEntry);
                jsonPageEntries.put(jsonPageEntry);
            } catch (Exception e) {
                Log.e(LOGTAG, "Exception loading page entry from JSON", e);
            }
        }

        final SharedPreferences prefs = getSharedPreferences();
        final SharedPreferences.Editor editor = prefs.edit();

        final String jsonString = jsonPageEntries.toString();
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
