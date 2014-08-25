/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.providers;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.util.GeckoJarReader;
import org.mozilla.search.Constants;
import org.mozilla.search.SearchPreferenceActivity;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.io.InputStream;

public class SearchEngineManager implements SharedPreferences.OnSharedPreferenceChangeListener {
    private static final String LOG_TAG = "SearchEngineManager";

    private Context context;
    private SearchEngineCallback changeCallback;
    private SearchEngine engine;

    public static interface SearchEngineCallback {
        public void execute(SearchEngine engine);
    }

    public SearchEngineManager(Context context) {
        this.context = context;
        GeckoSharedPrefs.forApp(context).registerOnSharedPreferenceChangeListener(this);
    }

    public void setChangeCallback(SearchEngineCallback changeCallback) {
        this.changeCallback = changeCallback;
    }

    /**
     * Perform an action with the user's default search engine.
     *
     * @param callback The callback to be used with the user's default search engine. The call
     *                 may be sync or async; if the call is async, it will be called on the
     *                 ui thread.
     */
    public void getEngine(SearchEngineCallback callback) {
        if (engine != null) {
            callback.execute(engine);
        } else {
            getEngineFromPrefs(callback);
        }
    }

    public void destroy() {
        GeckoSharedPrefs.forApp(context).unregisterOnSharedPreferenceChangeListener(this);
        context = null;
        changeCallback = null;
        engine = null;
    }

    @Override
    public void onSharedPreferenceChanged(final SharedPreferences sharedPreferences, final String key) {
        if (!TextUtils.equals(SearchPreferenceActivity.PREF_SEARCH_ENGINE_KEY, key)) {
            return;
        }
        getEngineFromPrefs(changeCallback);
    }

    /**
     * Look up the current search engine in shared preferences.
     * Creates a SearchEngine instance and caches it for use on the main thread.
     *
     * @param callback a SearchEngineCallback to be called after successfully looking
     *                 up the search engine. This will run on the UI thread.
     */
    private void getEngineFromPrefs(final SearchEngineCallback callback) {
        final AsyncTask<Void, Void, SearchEngine> task = new AsyncTask<Void, Void, SearchEngine>() {
            @Override
            protected SearchEngine doInBackground(Void... params) {
                final String identifier = GeckoSharedPrefs.forApp(context)
                        .getString(SearchPreferenceActivity.PREF_SEARCH_ENGINE_KEY, Constants.DEFAULT_SEARCH_ENGINE)
                        .toLowerCase();
                return createEngine(identifier);
            }

            @Override
            protected void onPostExecute(SearchEngine engine) {
                // Only touch engine on the main thread.
                SearchEngineManager.this.engine = engine;
                if (callback != null) {
                    callback.execute(engine);
                }
            }
        };
        task.execute();
    }

    /**
     * Creates a SearchEngine instance from an open search plugin.
     * This method does disk I/O, call it from a background thread.
     *
     * @param identifier search engine identifier (e.g. "google")
     * @return SearchEngine instance for identifier
     */
    private SearchEngine createEngine(String identifier) {
        InputStream in = getEngineFromJar(identifier);

        // Fallback for standalone search activity.
        if (in == null) {
            in = getEngineFromAssets(identifier);
        }

        if (in == null) {
            throw new IllegalArgumentException("Couldn't find search engine for identifier: " + identifier);
        }

        try {
            try {
                return new SearchEngine(identifier, in);
            } finally {
                in.close();
            }
        } catch (IOException e) {
            Log.e(LOG_TAG, "Exception creating search engine", e);
        } catch (XmlPullParserException e) {
            Log.e(LOG_TAG, "Exception creating search engine", e);
        }

        return null;
    }

    /**
     * Fallback for standalone search activity. These assets are not included
     * in mozilla-central.
     *
     * @param identifier search engine identifier (e.g. "google")
     * @return InputStream for open search plugin XML
     */
    private InputStream getEngineFromAssets(String identifier) {
        try {
            return context.getResources().getAssets().open(identifier + ".xml");
        } catch (IOException e) {
            Log.e(LOG_TAG, "Exception getting search engine from assets", e);
            return null;
        }
    }

    /**
     * Reads open search plugin XML file from the gecko jar. This will only work
     * if the search activity is built as part of mozilla-central.
     *
     * @param identifier search engine identifier (e.g. "google")
     * @return InputStream for open search plugin XML
     */
    private InputStream getEngineFromJar(String identifier) {
        // TODO: Get the real value for this
        final String locale = "en-US";

        final String path = "!/chrome/" + locale + "/locale/" + locale + "/browser/searchplugins/" + identifier + ".xml";
        final String url = "jar:jar:file://" + context.getPackageResourcePath() + "!/" + AppConstants.OMNIJAR_NAME + path;

        return GeckoJarReader.getStream(url);
    }
}
