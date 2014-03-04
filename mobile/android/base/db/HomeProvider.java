/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.io.IOException;
import java.io.InputStream;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.HomeItems;
import org.mozilla.gecko.sqlite.SQLiteBridge;

import android.content.ContentValues;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.util.Log;

public class HomeProvider extends SQLiteBridgeContentProvider {
    private static final String LOGTAG = "GeckoHomeProvider";

    // This should be kept in sync with the db version in mobile/android/modules/HomeProvider.jsm
    private static int DB_VERSION = 1;
    private static String DB_FILENAME = "home.sqlite";

    private static final String TABLE_ITEMS = "items";

    // Endpoint to return static fake data.
    static final int ITEMS_FAKE = 100;
    static final int ITEMS = 101;
    static final int ITEMS_ID = 102;

    static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

    static {
        URI_MATCHER.addURI(BrowserContract.HOME_AUTHORITY, "items/fake", ITEMS_FAKE);
        URI_MATCHER.addURI(BrowserContract.HOME_AUTHORITY, "items", ITEMS);
        URI_MATCHER.addURI(BrowserContract.HOME_AUTHORITY, "items/#", ITEMS_ID);
    }

    public HomeProvider() {
        super(LOGTAG);
    }

    @Override
    public String getType(Uri uri) {
        final int match = URI_MATCHER.match(uri);

        switch (match) {
            case ITEMS_FAKE: {
                return HomeItems.CONTENT_TYPE;
            }
            case ITEMS: {
                return HomeItems.CONTENT_TYPE;
            }
            default: {
                throw new UnsupportedOperationException("Unknown type " + uri);
            }
        }
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        final int match = URI_MATCHER.match(uri);

        // If we're querying the fake items, don't try to get the database.
        if (match == ITEMS_FAKE) {
            return queryFakeItems(uri, projection, selection, selectionArgs, sortOrder);
        }

        // Otherwise, let the SQLiteContentProvider implementation take care of this query for us!
        return super.query(uri, projection, selection, selectionArgs, sortOrder);
    }

    /**
     * Returns a cursor populated with static fake data.
     */
    private Cursor queryFakeItems(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        JSONArray items = null;
        try {
            items = new JSONArray(getRawFakeItems());
        } catch (IOException e) {
            Log.e(LOGTAG, "Error getting fake home items", e);
            return null;
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error parsing fake_home_items.json", e);
            return null;
        }

        final String[] itemsColumns = new String[] {
            HomeItems._ID,
            HomeItems.DATASET_ID,
            HomeItems.URL,
            HomeItems.TITLE,
            HomeItems.DESCRIPTION,
            HomeItems.IMAGE_URL,
            HomeItems.FILTER
        };

        final MatrixCursor c = new MatrixCursor(itemsColumns);
        for (int i = 0; i < items.length(); i++) {
            try {
                final JSONObject item = items.getJSONObject(i);
                c.addRow(new Object[] {
                    item.getInt("id"),
                    item.getString("dataset_id"),
                    item.getString("url"),
                    item.getString("title"),
                    item.getString("description"),
                    item.getString("image_url"),
                    item.getString("filter")
                });
            } catch (JSONException e) {
                Log.e(LOGTAG, "Error creating cursor row for fake home item", e);
            }
        }
        return c;
    }

    private String getRawFakeItems() throws IOException {
        final InputStream inputStream = getContext().getResources().openRawResource(R.raw.fake_home_items);
        final byte[] buffer = new byte[1024];
        StringBuilder s = new StringBuilder();
        int count;

        while ((count = inputStream.read(buffer)) != -1) {
            s.append(new String(buffer, 0, count));
        }
        return s.toString();
    }

    /**
     * SQLiteBridgeContentProvider implementation
     */

    @Override
    protected String getDBName(){
        return DB_FILENAME;
    }

    @Override
    protected int getDBVersion(){
        return DB_VERSION;
    }

    @Override
    public String getTable(Uri uri) {
        final int match = URI_MATCHER.match(uri);
        switch (match) {
            case ITEMS: {
                return TABLE_ITEMS;
            }
            default: {
                throw new UnsupportedOperationException("Unknown table " + uri);
            }
        }
    }

    @Override
    public String getSortOrder(Uri uri, String aRequested) {
        return null;
    }

    @Override
    public void setupDefaults(Uri uri, ContentValues values) { }

    @Override
    public void initGecko() { }

    @Override
    public void onPreInsert(ContentValues values, Uri uri, SQLiteBridge db) { }

    @Override
    public void onPreUpdate(ContentValues values, Uri uri, SQLiteBridge db) { }

    @Override
    public void onPostQuery(Cursor cursor, Uri uri, SQLiteBridge db) { }


}
