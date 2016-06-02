/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
package org.mozilla.gecko.db;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.util.Log;
import android.util.LruCache;

// Holds metadata info about URLs. Supports some helper functions for getting back a HashMap of key value data.
public class LocalURLMetadata implements URLMetadata {
    private static final String LOGTAG = "GeckoURLMetadata";
    private final Uri uriWithProfile;

    public LocalURLMetadata(String mProfile) {
        uriWithProfile = DBUtils.appendProfileWithDefault(mProfile, URLMetadataTable.CONTENT_URI);
    }

    // A list of columns in the table. It's used to simplify some loops for reading/writing data.
    private static final Set<String> COLUMNS;
    static {
        final HashSet<String> tempModel = new HashSet<>(4);
        tempModel.add(URLMetadataTable.URL_COLUMN);
        tempModel.add(URLMetadataTable.TILE_IMAGE_URL_COLUMN);
        tempModel.add(URLMetadataTable.TILE_COLOR_COLUMN);
        tempModel.add(URLMetadataTable.TOUCH_ICON_COLUMN);
        COLUMNS = Collections.unmodifiableSet(tempModel);
    }

    // Store a cache of recent results. This number is chosen to match the max number of tiles on about:home
    private static final int CACHE_SIZE = 9;
    // Note: Members of this cache are unmodifiable.
    private final LruCache<String, Map<String, Object>> cache = new LruCache<String, Map<String, Object>>(CACHE_SIZE);

    /**
     * Converts a JSON object into a unmodifiable Map of known metadata properties.
     * Will throw away any properties that aren't stored in the database.
     *
     * Incoming data can include a list like: {touchIconList:{56:"http://x.com/56.png", 76:"http://x.com/76.png"}}.
     * This will then be filtered to find the most appropriate touchIcon, i.e. the closest icon size that is larger
     * than (or equal to) the preferred homescreen launcher icon size, which is then stored in the "touchIcon" property.
     */
    @Override
    public Map<String, Object> fromJSON(JSONObject obj) {
        Map<String, Object> data = new HashMap<String, Object>();

        for (String key : COLUMNS) {
            if (obj.has(key)) {
                data.put(key, obj.optString(key));
            }
        }


        try {
            JSONObject icons;
            if (obj.has("touchIconList") &&
                    (icons = obj.getJSONObject("touchIconList")).length() > 0) {
                int preferredSize = GeckoAppShell.getPreferredIconSize();

                Iterator<String> keys = icons.keys();

                ArrayList<Integer> sizes = new ArrayList<Integer>(icons.length());
                while (keys.hasNext()) {
                    sizes.add(new Integer(keys.next()));
                }

                final int bestSize = Favicons.selectBestSizeFromList(sizes, preferredSize);
                final String iconURL = icons.getString(Integer.toString(bestSize));

                data.put(URLMetadataTable.TOUCH_ICON_COLUMN, iconURL);
            }
        } catch (JSONException e) {
            Log.w(LOGTAG, "Exception processing touchIconList for LocalURLMetadata; ignoring.", e);
        }

        return Collections.unmodifiableMap(data);
    }

    /**
     * Converts a Cursor into a unmodifiable Map of known metadata properties.
     * Will throw away any properties that aren't stored in the database.
     * Will also not iterate through multiple rows in the cursor.
     */
    private Map<String, Object> fromCursor(Cursor c) {
        Map<String, Object> data = new HashMap<String, Object>();

        String[] columns = c.getColumnNames();
        for (String column : columns) {
            if (COLUMNS.contains(column)) {
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
    @Override
    public Map<String, Map<String, Object>> getForURLs(final ContentResolver cr,
                                                       final Collection<String> urls,
                                                       final List<String> requestedColumns) {
        ThreadUtils.assertNotOnUiThread();
        ThreadUtils.assertNotOnGeckoThread();

        final Map<String, Map<String, Object>> data = new HashMap<String, Map<String, Object>>();

        // Nothing to query for
        if (urls.isEmpty() || requestedColumns.isEmpty()) {
            Log.e(LOGTAG, "Queried metadata for nothing");
            return data;
        }

        // Search the cache for any of these urls
        List<String> urlsToQuery = new ArrayList<String>();
        for (String url : urls) {
            final Map<String, Object> hit = cache.get(url);
            if (hit != null) {
                // Cache hit: we've found the URL in the cache, however we may not have cached the desired columns
                // for that URL. Hence we need to check whether our cache hit contains those columns, and directly
                // retrieve the desired data if not. (E.g. the top sites panel retrieves the tile, and tilecolor. If
                // we later try to retrieve the touchIcon for a top-site the cache hit will only point to
                // tile+tilecolor, and not the required touchIcon. In this case we don't want to use the cache.)
                boolean useCache = true;
                for (String c: requestedColumns) {
                    if (!hit.containsKey(c)) {
                        useCache = false;
                    }
                }
                if (useCache) {
                    data.put(url, hit);
                } else {
                    urlsToQuery.add(url);
                }
            } else {
                urlsToQuery.add(url);
            }
        }

        // If everything was in the cache, we're done!
        if (urlsToQuery.size() == 0) {
            return Collections.unmodifiableMap(data);
        }

        final String selection = DBUtils.computeSQLInClause(urlsToQuery.size(), URLMetadataTable.URL_COLUMN);
        List<String> columns = requestedColumns;
        // We need the url to build our final HashMap, so we force it to be included in the query.
        if (!columns.contains(URLMetadataTable.URL_COLUMN)) {
            // The requestedColumns may be immutable (e.g. if the caller used Collections.singletonList), hence
            // we have to create a copy.
            columns = new ArrayList<String>(columns);
            columns.add(URLMetadataTable.URL_COLUMN);
        }

        final Cursor cursor = cr.query(uriWithProfile,
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
            } while (cursor.moveToNext());

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
    @Override
    public void save(final ContentResolver cr, final Map<String, Object> data) {
        ThreadUtils.assertNotOnUiThread();
        ThreadUtils.assertNotOnGeckoThread();

        try {
            ContentValues values = new ContentValues();

            for (String key : COLUMNS) {
                if (data.containsKey(key)) {
                    values.put(key, (String) data.get(key));
                }
            }

            if (values.size() == 0) {
                return;
            }

            Uri uri = uriWithProfile.buildUpon()
                                 .appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true")
                                 .build();
            cr.update(uri, values, URLMetadataTable.URL_COLUMN + "=?", new String[] {
                (String) data.get(URLMetadataTable.URL_COLUMN)
            });
        } catch (Exception ex) {
            Log.e(LOGTAG, "error saving", ex);
        }
    }
}
