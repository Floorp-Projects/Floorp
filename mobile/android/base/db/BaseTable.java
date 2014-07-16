/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;
import android.util.Log;

// BaseTable provides a basic implementation of a Table for tables that don't require advanced operations during
// insert, delete, update, or query operations. Implementors must still provide onCreate and onUpgrade operations.
public abstract class BaseTable implements Table {
    private static final String LOGTAG = "GeckoBaseTable";

    private static final boolean DEBUG = false;

    protected static void log(String msg) {
        if (DEBUG) {
            Log.i(LOGTAG, msg);
        }
    }

    // Table implementation
    @Override
    public Table.ContentProviderInfo[] getContentProviderInfo() {
        return new Table.ContentProviderInfo[0];
    }

    // Table implementation
    @Override
    public abstract void onCreate(SQLiteDatabase db);

    // Table implementation
    @Override
    public abstract void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion);

    // Returns the name of the table to modify/query
    protected abstract String getTable();

    // Table implementation
    @Override
    public Cursor query(SQLiteDatabase db, Uri uri, int dbId, String[] columns, String selection, String[] selectionArgs, String sortOrder, String groupBy, String limit) {
        Cursor c = db.query(getTable(), columns, selection, selectionArgs, groupBy, null, sortOrder, limit);
        log("query " + columns + " in " + selection + " = " + c);
        return c;
    }

    @Override
    public int update(SQLiteDatabase db, Uri uri, int dbId, ContentValues values, String selection, String[] selectionArgs) {
        int updated = db.updateWithOnConflict(getTable(), values, selection, selectionArgs, SQLiteDatabase.CONFLICT_REPLACE);
        log("update " + values + " in " + selection + " = " + updated);
        return updated;
    }

    @Override
    public long insert(SQLiteDatabase db, Uri uri, int dbId, ContentValues values) {
        long inserted = db.insertOrThrow(getTable(), null, values);
        log("insert " + values + " = " + inserted);
        return inserted;
    }

    @Override
    public int delete(SQLiteDatabase db, Uri uri, int dbId, String selection, String[] selectionArgs) {
        int deleted = db.delete(getTable(), selection, selectionArgs);
        log("delete " + selection + " = " + deleted);
        return deleted;
    }
};
