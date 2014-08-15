/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.providers;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.search.Constants;
import org.mozilla.search.SearchPreferenceActivity;

public class SearchEngineManager implements SharedPreferences.OnSharedPreferenceChangeListener {
    private static final String LOG_TAG = "SearchEngineManager";

    private Context context;
    private SearchEngineCallback changeCallback;
    private SearchEngine engine;

    // Add new engines to this enum. Also update createInstance, the factory method below.
    public static enum Engine {
        BING,
        GOOGLE,
        YAHOO
    }

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

    /**
     * Manually lookup the current search engine.
     *
     * @param callback a SearchEngineCallback to be called after successfully looking
     *                 up the search engine. This will run on the UI thread.
     */
    private void getEngineFromPrefs(final SearchEngineCallback callback) {
        final AsyncTask<Void, Void, String> task = new AsyncTask<Void, Void, String>() {
            @Override
            protected String doInBackground(Void... params) {
                return GeckoSharedPrefs.forApp(context).getString(SearchPreferenceActivity.PREF_SEARCH_ENGINE_KEY, null);
            }

            @Override
            protected void onPostExecute(String engineName) {
                updateEngine(engineName);
                callback.execute(engine);
            }
        };
        task.execute();
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        if (TextUtils.equals(SearchPreferenceActivity.PREF_SEARCH_ENGINE_KEY, key)) {
            updateEngine(sharedPreferences.getString(key, null));

            if (changeCallback != null) {
                changeCallback.execute(engine);
            }
        }
    }

    /**
     * Notify the searchEngineChangeListener that the default search engine has changed.
     *
     * @param engineName The name of the new search engine. This should be a member
     *                   of SearchEngineFactory.Engine. If null, then it will use the
     *                   default search engine.
     * @return true if this caused the engine to be changed.
     */
    private void updateEngine(String engineName) {

        if (TextUtils.isEmpty(engineName)) {
            engineName = Constants.DEFAULT_SEARCH_ENGINE;
        }

        try {
            engine = createEngine(engineName);
        } catch (IllegalArgumentException e) {
            Log.e(LOG_TAG, "Search engine not found for " + engineName + " reverting to default engine.", e);
            engine = createEngine(Constants.DEFAULT_SEARCH_ENGINE);
        }
    }

    private static SearchEngine createEngine(String engineName) {
        switch (Engine.valueOf(engineName)) {
            case BING:
                return new BingSearchEngine();
            case GOOGLE:
                return new GoogleSearchEngine();
            case YAHOO:
                return new YahooSearchEngine();
        }

        // The return statement is unreachable since Engine.valueOf will throw
        // IllegalArgumentException if engineName cannot be resolved.
        return null;
    }
}
