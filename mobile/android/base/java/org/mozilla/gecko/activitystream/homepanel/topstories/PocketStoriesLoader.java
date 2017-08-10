/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.topstories;

import android.content.AsyncTaskLoader;
import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.util.FileUtils;
import org.mozilla.gecko.util.ProxySelector;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * Loader implementation for loading fresh or cached content from the Pocket Stories API.
 * {@link #loadInBackground()} returns a JSON string of Pocket stories.
 *
 * NB: Using AsyncTaskLoader rather than AsyncTask so that loader is tied Activity lifecycle.
 *
 * Access to Pocket Stories is controlled by a private Pocket API token.
 * Add the following to your mozconfig to compile with the Pocket Stories:
 *
 *   export MOZ_ANDROID_POCKET=1
 *   ac_add_options --with-pocket-api-keyfile=$topsrcdir/mobile/android/base/pocket-api-sandbox.token
 *
 * and include the Pocket API token in the token file.
 */

public class PocketStoriesLoader extends AsyncTaskLoader<String> {
    public static String LOGTAG = "PocketStoriesLoader";

    // Pocket SharedPreferences keys
    private static final String POCKET_PREFS_FILE = "PocketStories";
    private static final String CACHE_TIMESTAMP_MILLIS_PREFIX = "timestampMillis-";
    private static final String STORIES_CACHE_PREFIX = "storiesCache-";

    // Pocket API params and defaults
    private static final String GLOBAL_ENDPOINT = "https://getpocket.com/v3/firefox/global-recs";
    private static final String PARAM_APIKEY = "consumer_key";
    private static final String APIKEY = AppConstants.MOZ_POCKET_API_KEY;
    private static final String PARAM_COUNT = "count";
    private static final int DEFAULT_COUNT = 20;
    private static final String PARAM_LOCALE = "locale_lang";

    private static final long REFRESH_INTERVAL_MILLIS = TimeUnit.HOURS.toMillis(3);

    private static final int BUFFER_SIZE = 2048;
    private static final int CONNECT_TIMEOUT = (int) TimeUnit.SECONDS.toMillis(15);
    private static final int READ_TIMEOUT = (int) TimeUnit.SECONDS.toMillis(15);

    private String localeLang;
    private final SharedPreferences sharedPreferences;

    public PocketStoriesLoader(Context context) {
        super(context);

        sharedPreferences = context.getSharedPreferences(POCKET_PREFS_FILE, Context.MODE_PRIVATE);
        localeLang = Locales.getLanguageTag(Locale.getDefault());
    }

    @Override
    protected void onStartLoading() {
        // Check timestamp to determine if we have cached stories. This won't properly handle a client manually
        // changing clock times, but this is not a time-sensitive task.
        final long previousTime = sharedPreferences.getLong(CACHE_TIMESTAMP_MILLIS_PREFIX + localeLang, 0);
        if (System.currentTimeMillis() - previousTime > REFRESH_INTERVAL_MILLIS) {
            forceLoad();
        } else {
            deliverResult(sharedPreferences.getString(STORIES_CACHE_PREFIX + localeLang, null));
        }
    }

    @Override
    protected void onReset() {
        localeLang = Locales.getLanguageTag(Locale.getDefault());
    }

    @Override
    public String loadInBackground() {
        if (APIKEY == null) {
            Log.e(LOGTAG, "Missing Pocket API key! See class comment about how to set up a mozconfig.");
            return null;
        }
        return makeAPIRequestWithKey(APIKEY);
    }

    protected String makeAPIRequestWithKey(final String apiKey) {
        HttpURLConnection connection = null;

        final Uri uri = Uri.parse(GLOBAL_ENDPOINT)
                .buildUpon()
                .appendQueryParameter(PARAM_APIKEY, apiKey)
                .appendQueryParameter(PARAM_COUNT, String.valueOf(DEFAULT_COUNT))
                .appendQueryParameter(PARAM_LOCALE, localeLang)
                .build();
        try {
            connection = (HttpURLConnection) ProxySelector.openConnectionWithProxy(new URI(uri.toString()));

            connection.setConnectTimeout(CONNECT_TIMEOUT);
            connection.setReadTimeout(READ_TIMEOUT);

            final InputStream stream = new BufferedInputStream(connection.getInputStream());
            final String output = FileUtils.readStringFromInputStreamAndCloseStream(stream, BUFFER_SIZE);

            // Update cache and timestamp.
            sharedPreferences.edit().putLong(CACHE_TIMESTAMP_MILLIS_PREFIX + localeLang, System.currentTimeMillis())
                                    .putString(STORIES_CACHE_PREFIX + localeLang, output)
                                    .apply();

            return output;
        } catch (IOException e) {
            Log.e(LOGTAG, "Problem opening connection or reading input stream", e);
            return null;
        } catch (URISyntaxException e) {
            Log.e(LOGTAG, "Couldn't create URI", e);
            return null;
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

}
