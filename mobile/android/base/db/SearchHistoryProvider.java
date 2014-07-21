/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.db.BrowserContract.SearchHistory;

import android.content.ContentUris;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.SQLException;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

public class SearchHistoryProvider extends SharedBrowserDatabaseProvider {
    private static final String LOG_TAG = "GeckoSearchProvider";
    private static final boolean DEBUG_ENABLED = false;

    /**
     * Collapse whitespace.
     */
    private String stripWhitespace(String query) {
        if (TextUtils.isEmpty(query)) {
            return "";
        }

        // Collapse whitespace
        return query.trim().replaceAll("\\s+", " ");
    }


    @Override
    public Uri insertInTransaction(Uri uri, ContentValues cv) {
        final String query = stripWhitespace(cv.getAsString(SearchHistory.QUERY));

        // We don't support inserting empty search queries.
        if (TextUtils.isEmpty(query)) {
            return null;
        }

        final SQLiteDatabase db = getWritableDatabase(uri);
        long id = -1;

        /*
         * Attempt to insert the query. The catch block handles the case when
         * the query already exists in the DB.
         */
        try {
            cv.put(SearchHistory.QUERY, query);
            cv.put(SearchHistory.VISITS, 1);
            cv.put(SearchHistory.DATE_LAST_VISITED, System.currentTimeMillis());

            id = db.insertOrThrow(SearchHistory.TABLE_NAME, null, cv);

            if (id > 0) {
                return ContentUris.withAppendedId(uri, id);
            }
        } catch (SQLException e) {
            // This happens when the column already exists for this term.
            if (DEBUG_ENABLED) {
                Log.w(LOG_TAG, String.format("Query `%s` already in db", query));
            }
        }

        /*
         * Increment the VISITS counter and update the DATE_LAST_VISITED.
         */
        final String sql = "UPDATE " + SearchHistory.TABLE_NAME + " SET " +
                            SearchHistory.VISITS + " = " + SearchHistory.VISITS + " + 1, " +
                            SearchHistory.DATE_LAST_VISITED + " = " + System.currentTimeMillis() +
                            " WHERE " + SearchHistory.QUERY + " = ?";

        final Cursor c = db.rawQuery(sql, new String[] { query });

        try {
            if (c.getCount() > 1) {
                // There is a UNIQUE constraint on the QUERY column,
                // so there should only be one match.
                return null;
            }
            if (c.moveToFirst()) {
                return ContentUris.withAppendedId(uri, c.getInt(c.getColumnIndex(SearchHistory._ID)));
            }
        } finally {
            c.close();
        }

        return null;
    }

    @Override
    public int deleteInTransaction(Uri uri, String selection, String[] selectionArgs) {
        return getWritableDatabase(uri).delete(SearchHistory.TABLE_NAME,
                                               selection, selectionArgs);
    }

    /**
     * Since we are managing counts and the full-text db, an update
     * could mangle the internal state. So we disable it.
     */
    @Override
    public int updateInTransaction(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        throw new UnsupportedOperationException("This content provider does not support updating items");
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder) {
        String groupBy = null;
        String having = null;
        final Cursor cursor = getReadableDatabase(uri).query(SearchHistory.TABLE_NAME, projection,
                selection, selectionArgs, groupBy, having, sortOrder);
        cursor.setNotificationUri(getContext().getContentResolver(), uri);
        return cursor;
    }

    @Override
    public String getType(Uri uri) {
        return SearchHistory.CONTENT_TYPE;
    }
}
