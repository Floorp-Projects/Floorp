/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.support.annotation.WorkerThread;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.focus.ext.AssetManagerKt;
import org.mozilla.focus.locale.Locales;
import org.mozilla.focus.utils.Settings;
import org.xmlpull.v1.XmlPullParserException;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Set;

import edu.umd.cs.findbugs.annotations.SuppressFBWarnings;

public class SearchEngineManager extends BroadcastReceiver {
    private static final String LOG_TAG = SearchEngineManager.class.getSimpleName();

    private static SearchEngineManager instance = new SearchEngineManager();

    private List<SearchEngine> searchEngines;

    /**
     * A flag indicating that data has been loaded, or is loading. This lets us detect if data
     * has been requested without a preceeding init().
     */
    private boolean loadHasBeenTriggered = false;

    public static SearchEngineManager getInstance() {
        return instance;
    }

    private SearchEngineManager() {}

    public void init(Context context) {
        context.registerReceiver(this, new IntentFilter(Intent.ACTION_LOCALE_CHANGED));

        loadSearchEngines(context);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (!Intent.ACTION_LOCALE_CHANGED.equals(intent.getAction())) {
            // This is not the broadcast you are looking for.
            return;
        }

        loadSearchEngines(context.getApplicationContext());
    }

    private void loadSearchEngines(final Context context) {
        new Thread("SearchEngines-Load") {
            @Override
            public void run() {
                loadFromDisk(context);
            }
        }.start();
    }

    @WorkerThread
    private synchronized void loadFromDisk(Context context) {
        loadHasBeenTriggered = true;
        final AssetManager assetManager = context.getAssets();
        final Locale locale = Locale.getDefault();
        final List<SearchEngine> searchEngines = new ArrayList<>();

        try {
            final JSONArray engineNames = loadSearchEngineListForLocale(context);

            final String localePath = "search/" + Locales.getLanguageTag(locale);
            final String languagePath = "search/" + Locales.getLanguage(locale);
            final String defaultPath = "search/default";

            final List<String> localeEngines = Arrays.asList(assetManager.list(localePath));
            final List<String> languageEngines = Arrays.asList(assetManager.list(languagePath));
            final List<String> defaultEngines = Arrays.asList(assetManager.list(defaultPath));

            for (int i = 0; i < engineNames.length(); i++) {
                final String engineName = engineNames.getString(i);
                final String fileName = engineName + ".xml";

                if (localeEngines.contains(fileName)) {
                    searchEngines.add(SearchEngineParser.load(assetManager, engineName, localePath + "/" + fileName));
                } else if (languageEngines.contains(fileName)) {
                    searchEngines.add(SearchEngineParser.load(assetManager, engineName, languagePath + "/" + fileName));
                } else if (defaultEngines.contains(fileName)) {
                    searchEngines.add(SearchEngineParser.load(assetManager, engineName, defaultPath + "/" + fileName));
                } else {
                    Log.e(LOG_TAG, "Couldn't find configuration for engine: " + engineName);
                }
            }
        } catch (IOException e) {
            Log.e(LOG_TAG, "IOException while loading search engines", e);
        } catch (JSONException e) {
            throw new AssertionError("Reading search engine failed: ", e);
        } finally {
            searchEngines.addAll(loadCustomSearchEngines(context));
            this.searchEngines = searchEngines;

            notifyAll();
        }
    }

    private List<SearchEngine> loadCustomSearchEngines(Context context) {
        final List<SearchEngine> searchEngines = new LinkedList<>();
        final SharedPreferences prefs = context.getSharedPreferences(SearchEngine.PREF_FILE_SEARCH_ENGINES, Context.MODE_PRIVATE);
        final Set<String> engines = prefs.getStringSet(SearchEngine.PREF_KEY_CUSTOM_SEARCH_ENGINES, Collections.<String>emptySet());
        try {
            for (String engine : engines) {
                final InputStream engineInputStream = new ByteArrayInputStream(prefs.getString(engine, "").getBytes(StandardCharsets.UTF_8));
                searchEngines.add(SearchEngineParser.load(engine, engineInputStream));
            }
        } catch (IOException e) {
            Log.e(LOG_TAG, "IOException whil loading custom search engines", e);
        } catch (XmlPullParserException e) {
            Log.e(LOG_TAG, "Couldn't load custom search engines", e);
        }
        return searchEngines;
    }

    private JSONArray loadSearchEngineListForLocale(Context context) throws IOException {
        try {
            final Locale locale = Locale.getDefault();

            final JSONObject configuration = AssetManagerKt.readJSONObject(context.getAssets(), "search/search_configuration.json");

            // Try to find a configuration for the language tag first (de-DE)
            final String languageTag = Locales.getLanguageTag(locale);
            if (configuration.has(languageTag)) {
                return configuration.getJSONArray(languageTag);
            }

            // Try to find a configuration for just the language (de)
            final String language = Locales.getLanguage(locale);
            if (configuration.has(language)) {
                return configuration.getJSONArray(language);
            }

            // No configuration for the current locale found. Let's use the default configuration.
            return configuration.getJSONArray("default");
        } catch (JSONException e) {
            // Assertion error because this shouldn't happen: We check whether a key exists before
            // reading it. An error here would mean the JSON file is corrupt.
            throw new AssertionError("Reading search configuration failed", e);
        }
    }

    public synchronized List<SearchEngine> getSearchEngines() {
        awaitLoadingSearchEnginesLocked();

        return searchEngines;
    }

    public synchronized SearchEngine getDefaultSearchEngine(Context context) {
        awaitLoadingSearchEnginesLocked();

        final String defaultSearch = Settings.getInstance(context).getDefaultSearchEngineName();
        if (defaultSearch != null) {
            for (SearchEngine searchEngine : searchEngines) {
                if (defaultSearch.equals(searchEngine.getName())) {
                    return searchEngine;
                }
            }
        }

        return searchEngines.get(0);
    }

    // Our (searchEngines == null) check is deemed to be an unsynchronised access. Similarly loadHasBeenTriggered
    // also doesn't need synchronisation:
    @SuppressFBWarnings(value = "IS2_INCONSISTENT_SYNC", justification = "Variable is not being accessed, it is merely being tested for existence")
    public void awaitLoadingSearchEnginesLocked() {
        if (!loadHasBeenTriggered) {
            throw new IllegalStateException("Attempting to retrieve search engines without a corresponding init()");
        }

        while (searchEngines == null) {
            try {
                wait();
            } catch (InterruptedException ignored) {
                // Ignore
            }
        }
    }
}
