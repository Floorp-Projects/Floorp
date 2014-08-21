/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
package org.mozilla.gecko.db;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.History;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.Telemetry;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.ContentValues;
import android.content.ContentResolver;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;
import android.support.v4.util.LruCache;
import android.util.Log;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

// Holds metadata info about urls. Supports some helper functions for getting back a HashMap of key value data.
public class URLMetadata {
    private static final String LOGTAG = "GeckoURLMetadata";

    // This returns a list of columns in the table. It's used to simplify some loops for reading/writing data.
    @SuppressWarnings("serial")
    private static final Set<String> getModel() {
        return new HashSet<String>() {{
            add(URLMetadataTable.URL_COLUMN);
            add(URLMetadataTable.TILE_IMAGE_URL_COLUMN);
            add(URLMetadataTable.TILE_COLOR_COLUMN);
        }};
    }

    // Store a cache of recent results. This number is chosen to match the max number of tiles on about:home
    private static final int CACHE_SIZE = 9;
    // Note: Members of this cache are unmodifiable.
    private static final LruCache<String, Map<String, Object>> cache = new LruCache<String, Map<String, Object>>(CACHE_SIZE);

    /**
     * Converts a JSON object into a unmodifiable Map of known metadata properties.
     * Will throw away any properties that aren't stored in the database.
     */
    public static Map<String, Object> fromJSON(JSONObject obj) {
        Map<String, Object> data = new HashMap<String, Object>();

        Set<String> model = getModel();
        for (String key : model) {
            if (obj.has(key)) {
                data.put(key, obj.optString(key));
            }
        }

        return Collections.unmodifiableMap(data);
    }

    /**
     * Converts a Cursor into a unmodifiable Map of known metadata properties.
     * Will throw away any properties that aren't stored in the database.
     * Will also not iterate through multiple rows in the cursor.
     */
    private static Map<String, Object> fromCursor(Cursor c) {
        Map<String, Object> data = new HashMap<String, Object>();

        Set<String> model = getModel();
        String[] columns = c.getColumnNames();
        for (String column : columns) {
            if (model.contains(column)) {
                try {
                    data.put(column, c.getString(c.getColumnIndexOrThrow(column)));
                } catch (Exception ex) {
                    Log.i(LOGTAG, "Error getting data for " + column, ex);
                }
            }
        }

        return Collections.unmodifiableMap(data);
    }

    /**
     * Returns an unmodifiable Map of url->Metadata (i.e. A second HashMap) for a list of urls.
     * Must not be called from UI or Gecko threads.
     */
    public static Map<String, Map<String, Object>> getForUrls(final ContentResolver cr,
                                                              final List<String> urls,
                                                              final List<String> columns) {
        ThreadUtils.assertNotOnUiThread();
        ThreadUtils.assertNotOnGeckoThread();

        final Map<String, Map<String, Object>> data = new HashMap<String, Map<String, Object>>();

        // Nothing to query for
        if (urls.isEmpty() || columns.isEmpty()) {
            Log.e(LOGTAG, "Queried metadata for nothing");
            return data;
        }

        // Search the cache for any of these urls
        List<String> urlsToQuery = new ArrayList<String>();
        for (String url : urls) {
            final Map<String, Object> hit = cache.get(url);
            if (hit != null) {
                // Cache hit!
                data.put(url, hit);
            } else {
                urlsToQuery.add(url);
            }
        }

        Telemetry.HistogramAdd("FENNEC_TILES_CACHE_HIT", data.size());

        // If everything was in the cache, we're done!
        if (urlsToQuery.size() == 0) {
            return Collections.unmodifiableMap(data);
        }

        final String selection = DBUtils.computeSQLInClause(urlsToQuery.size(), URLMetadataTable.URL_COLUMN);
        // We need the url to build our final HashMap, so we force it to be included in the query.
        if (!columns.contains(URLMetadataTable.URL_COLUMN)) {
            columns.add(URLMetadataTable.URL_COLUMN);
        }

        final Cursor cursor = cr.query(URLMetadataTable.CONTENT_URI,
                                       columns.toArray(new String[columns.size()]), // columns,
                                       selection, // selection
                                       urlsToQuery.toArray(new String[urlsToQuery.size()]), // selectionargs
                                       null);
        try {
            if (!cursor.moveToFirst()) {
                return Collections.unmodifiableMap(data);
            }

            do {
                final Map<String, Object> metadata = fromCursor(cursor);
                final String url = cursor.getString(cursor.getColumnIndexOrThrow(URLMetadataTable.URL_COLUMN));

                data.put(url, metadata);
                cache.put(url, metadata);
            } while(cursor.moveToNext());

        } finally {
            cursor.close();
        }

        return Collections.unmodifiableMap(data);
    }

    /**
     * Saves a HashMap of metadata into the database. Will iterate through columns
     * in the Database and only save rows with matching keys in the HashMap.
     * Must not be called from UI or Gecko threads.
     */
    public static void save(final ContentResolver cr, final String url, final Map<String, Object> data) {
        ThreadUtils.assertNotOnUiThread();
        ThreadUtils.assertNotOnGeckoThread();

        try {
            ContentValues values = new ContentValues();

            Set<String> model = getModel();
            for (String key : model) {
                if (data.containsKey(key)) {
                    values.put(key, (String) data.get(key));
                }
            }

            if (values.size() == 0) {
                return;
            }

            Uri uri = URLMetadataTable.CONTENT_URI.buildUpon()
                                 .appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true")
                                 .build();
            cr.update(uri, values, URLMetadataTable.URL_COLUMN + "=?", new String[] {
                (String) data.get(URLMetadataTable.URL_COLUMN)
            });
        } catch (Exception ex) {
            Log.e(LOGTAG, "error saving", ex);
        }
    }

    public static int deleteUnused(final ContentResolver cr) {
        final String selection = URLMetadataTable.URL_COLUMN + " NOT IN "
                + "(SELECT " + History.URL
                + " FROM " + History.TABLE_NAME
                + " WHERE " + History.IS_DELETED + " = 0"
                + " UNION "
                + " SELECT " + Bookmarks.URL
                + " FROM " + Bookmarks.TABLE_NAME
                + " WHERE " + Bookmarks.IS_DELETED + " = 0 "
                + " AND " + Bookmarks.URL + " IS NOT NULL)";

        return cr.delete(URLMetadataTable.CONTENT_URI, selection, null);
    }
}
