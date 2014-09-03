/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.text.TextUtils;
import android.util.Log;
import android.widget.Toast;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.LocaleAware;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.search.providers.SearchEngine;
import org.mozilla.search.providers.SearchEngineManager;

import java.util.List;

/**
 * This activity allows users to modify the settings for the search activity.
 *
 * A note on implementation: At the moment, we don't have tablet-specific designs.
 * Therefore, this implementation uses the old-style PreferenceActivity. When
 * we start optimizing for tablets, we can migrate to Fennec's PreferenceFragment
 * implementation.
 *
 * TODO: Change this to PreferenceFragment when we stop supporting devices older than SDK 11.
 */
public class SearchPreferenceActivity extends PreferenceActivity
        implements SharedPreferences.OnSharedPreferenceChangeListener {

    private static final String LOG_TAG = "SearchPreferenceActivity";

    public static final String PREF_CLEAR_HISTORY_KEY = "search.not_a_preference.clear_history";
    public static final String PREF_SEARCH_ENGINE_KEY = "search.engines.default";

    private SearchEngineManager searchEngineManager;

    @Override
    @SuppressWarnings("deprecation")
    protected void onCreate(Bundle savedInstanceState) {
        LocaleAware.initializeLocale(getApplicationContext());
        super.onCreate(savedInstanceState);

        getPreferenceManager().setSharedPreferencesName(GeckoSharedPrefs.APP_PREFS_NAME);

        searchEngineManager = new SearchEngineManager(this);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
            if (getActionBar() != null) {
                getActionBar().setDisplayHomeAsUpEnabled(true);
            }
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        searchEngineManager.destroy();
    }

    @Override
    protected void onResume() {
        super.onResume();
        GeckoSharedPrefs.forApp(this).registerOnSharedPreferenceChangeListener(this);
    }

    @Override
    protected void onPause() {
        super.onPause();
        GeckoSharedPrefs.forApp(this).unregisterOnSharedPreferenceChangeListener(this);
    }

    @Override
    protected void onPostCreate(Bundle savedInstanceState) {
        super.onPostCreate(savedInstanceState);
        setupPrefsScreen();
    }

    @SuppressWarnings("deprecation")
    private void setupPrefsScreen() {
        addPreferencesFromResource(R.xml.search_preferences);

        // Attach click listener to clear history button.
        final Preference clearHistoryButton = findPreference(PREF_CLEAR_HISTORY_KEY);
        clearHistoryButton.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                final AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(SearchPreferenceActivity.this);
                dialogBuilder.setNegativeButton(android.R.string.cancel, null);
                dialogBuilder.setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        Telemetry.sendUIEvent(TelemetryContract.Event.SANITIZE, TelemetryContract.Method.MENU, "search-history");
                        clearHistory();
                    }
                });
                dialogBuilder.setMessage(R.string.pref_clearHistory_dialogMessage);
                dialogBuilder.show();
                return false;
            }
        });

        setUpSearchEnginePref();
    }

    @SuppressWarnings("deprecation")
    private void setUpSearchEnginePref() {
        final AsyncTask<Void, Void, List<SearchEngine>> task = new AsyncTask<Void, Void, List<SearchEngine>>() {
            @Override
            protected List<SearchEngine> doInBackground(Void... params) {
                return searchEngineManager.getAllEngines();
            }

            @Override
            protected void onPostExecute(List<SearchEngine> engines) {
                final CharSequence[] entries = new CharSequence[engines.size()];
                final CharSequence[] entryValues = new CharSequence[engines.size()];

                for (int i = 0; i < engines.size(); i++) {
                    final SearchEngine engine = engines.get(i);
                    entries[i] = engine.getName();
                    entryValues[i] = engine.getIdentifier();
                }

                final ListPreference searchEnginePref = (ListPreference) findPreference(PREF_SEARCH_ENGINE_KEY);
                searchEnginePref.setEntries(entries);
                searchEnginePref.setEntryValues(entryValues);

                if (searchEnginePref.getValue() == null) {
                    searchEnginePref.setValue(getResources().getString(R.string.default_engine_identifier));
                }
                searchEnginePref.setSummary(searchEnginePref.getEntry());
            }
        };
        task.execute();
    }

    private void clearHistory() {
        final AsyncTask<Void, Void, Boolean> clearHistoryTask = new AsyncTask<Void, Void, Boolean>() {
            @Override
            protected Boolean doInBackground(Void... params) {
                final int numDeleted = getContentResolver().delete(
                        BrowserContract.SearchHistory.CONTENT_URI, null, null);
                return numDeleted >= 0;
            }

            @Override
            protected void onPostExecute(Boolean success) {
                if (success) {
                    getContentResolver().notifyChange(BrowserContract.SearchHistory.CONTENT_URI, null);
                    Toast.makeText(SearchPreferenceActivity.this, SearchPreferenceActivity.this.getResources()
                            .getString(R.string.pref_clearHistory_confirmation), Toast.LENGTH_SHORT).show();
                } else {
                    Log.e(LOG_TAG, "Error clearing search history.");
                }
            }
        };
        clearHistoryTask.execute();
    }

    /**
     * Update summaries when the value of a shared preference changes.
     */
    @Override
    @SuppressWarnings("deprecation")
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        if (TextUtils.equals(PREF_SEARCH_ENGINE_KEY, key)) {
            final ListPreference searchEnginePref = (ListPreference) findPreference(PREF_SEARCH_ENGINE_KEY);
            searchEnginePref.setSummary(searchEnginePref.getEntry());
            Telemetry.sendUIEvent(TelemetryContract.Event.SEARCH_SET_DEFAULT,
                                  TelemetryContract.Method.DIALOG,
                                  searchEnginePref.getValue().toLowerCase());
        }
    }
}
