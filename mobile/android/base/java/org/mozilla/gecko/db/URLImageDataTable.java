/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.db;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.History;

import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;

// Holds metadata info about urls. Supports some helper functions for getting back a HashMap of key value data.
public class URLImageDataTable extends BaseTable {
    private static final String LOGTAG = "GeckoURLImageDataTable";

    private static final String TABLE = "metadata"; // Name of the table in the db
    private static final int TABLE_ID_NUMBER = BrowserProvider.METADATA;

    // Uri for querying this table
    public static final Uri CONTENT_URI = Uri.withAppendedPath(BrowserContract.AUTHORITY_URI, "metadata");

    // Columns in the table
    public static final String ID_COLUMN = "id";
    public static final String URL_COLUMN = "url";
    public static final String TILE_IMAGE_URL_COLUMN = "tileImage";
    public static final String TILE_COLOR_COLUMN = "tileColor";
    public static final String TOUCH_ICON_COLUMN = "touchIcon";

    URLImageDataTable() { }

    @Override
    protected String getTable() {
        return TABLE;
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
        String create = "CREATE TABLE " + TABLE + " (" +
            ID_COLUMN + " INTEGER PRIMARY KEY, " +
            URL_COLUMN + " TEXT NON NULL UNIQUE, " +
            TILE_IMAGE_URL_COLUMN + " STRING, " +
            TILE_COLOR_COLUMN + " STRING, " +
            TOUCH_ICON_COLUMN + " STRING);";
        db.execSQL(create);
    }

    private void upgradeDatabaseFrom26To27(SQLiteDatabase db) {
        db.execSQL("ALTER TABLE " + TABLE +
                   " ADD COLUMN " + TOUCH_ICON_COLUMN + " STRING");
    }

    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        // This table was added in v21 of the db. Force its creation if we're coming from an earlier version
        if (newVersion >= 21 && oldVersion < 21) {
            onCreate(db);
            return;
        }

        // Removed the redundant metadata_url_idx index in version 26
        if (newVersion >= 26 && oldVersion < 26) {
            db.execSQL("DROP INDEX IF EXISTS metadata_url_idx");
        }
        if (newVersion >= 27 && oldVersion < 27) {
            upgradeDatabaseFrom26To27(db);
        }
    }

    @Override
    public Table.ContentProviderInfo[] getContentProviderInfo() {
        return new Table.ContentProviderInfo[] {
            new Table.ContentProviderInfo(TABLE_ID_NUMBER, TABLE)
        };
    }

    public int deleteUnused(final SQLiteDatabase db) {
        final String selection = URL_COLUMN + " NOT IN " +
                                 "(SELECT " + History.URL +
                                 " FROM " + History.TABLE_NAME +
                                 " WHERE " + History.IS_DELETED + " = 0" +
                                 " UNION " +
                                 " SELECT " + Bookmarks.URL +
                                 " FROM " + Bookmarks.TABLE_NAME +
                                 " WHERE " + Bookmarks.IS_DELETED + " = 0 " +
                                 " AND " + Bookmarks.URL + " IS NOT NULL)";

        return db.delete(getTable(), selection, null);
    }
}
