/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;

// Tables provide a basic wrapper around ContentProvider methods to make it simpler to add new tables into storage.
// If you create a new Table type, make sure to add it to the sTables list in BrowserProvider to ensure it is queried.
interface Table {
    // Provides information to BrowserProvider about the type of URIs this Table can handle.
    public static class ContentProviderInfo {
        public final int id; // A number of ID for this table. Used by the UriMatcher in BrowserProvider
        public final String name; // A name for this table. Will be appended onto uris querying this table
                                  // This is also used to define the mimetype of data returned from this db, i.e.
                                  // BrowserProvider will return "vnd.android.cursor.item/" + name

        public ContentProviderInfo(int id, String name) {
            if (name == null) {
                throw new IllegalArgumentException("Content provider info must specify a name");
            }
            this.id = id;
            this.name = name;
        }
    }

    // Return a list of Info about the ContentProvider URIs this will match
    ContentProviderInfo[] getContentProviderInfo();

    // Called by BrowserDBHelper whenever the database is created or upgraded.
    // Order in which tables are created/upgraded isn't guaranteed (yet), so be careful if your Table depends on something in a
    // separate table.
    void onCreate(SQLiteDatabase db);
    void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion);

    // Called by BrowserProvider when this database queried/modified
    // The dbId here should match the dbId's you returned in your getContentProviderInfo() call
    Cursor query(SQLiteDatabase db, Uri uri, int dbId, String[] projection, String selection, String[] selectionArgs, String sortOrder, String groupBy, String limit);
    int update(SQLiteDatabase db, Uri uri, int dbId, ContentValues values, String selection, String[] selectionArgs);
    long insert(SQLiteDatabase db, Uri uri, int dbId, ContentValues values);
    int delete(SQLiteDatabase db, Uri uri, int dbId, String selection, String[] selectionArgs);
};
