/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.db.BrowserContract.Clients;
import org.mozilla.gecko.db.BrowserContract.Tabs;

import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

public class TabsProvider extends PerProfileDatabaseProvider<TabsProvider.TabsDatabaseHelper> {
    static final String DATABASE_NAME = "tabs.db";

    static final int DATABASE_VERSION = 3;

    static final String TABLE_TABS = "tabs";
    static final String TABLE_CLIENTS = "clients";

    static final int TABS = 600;
    static final int TABS_ID = 601;
    static final int CLIENTS = 602;
    static final int CLIENTS_ID = 603;

    static final String DEFAULT_TABS_SORT_ORDER = Clients.LAST_MODIFIED + " DESC, " + Tabs.LAST_USED + " DESC";
    static final String DEFAULT_CLIENTS_SORT_ORDER = Clients.LAST_MODIFIED + " DESC";

    static final String INDEX_TABS_GUID = "tabs_guid_index";
    static final String INDEX_TABS_POSITION = "tabs_position_index";
    static final String INDEX_CLIENTS_GUID = "clients_guid_index";

    static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

    static final Map<String, String> TABS_PROJECTION_MAP;
    static final Map<String, String> CLIENTS_PROJECTION_MAP;

    static {
        URI_MATCHER.addURI(BrowserContract.TABS_AUTHORITY, "tabs", TABS);
        URI_MATCHER.addURI(BrowserContract.TABS_AUTHORITY, "tabs/#", TABS_ID);
        URI_MATCHER.addURI(BrowserContract.TABS_AUTHORITY, "clients", CLIENTS);
        URI_MATCHER.addURI(BrowserContract.TABS_AUTHORITY, "clients/#", CLIENTS_ID);

        HashMap<String, String> map;

        map = new HashMap<String, String>();
        map.put(Tabs._ID, Tabs._ID);
        map.put(Tabs.TITLE, Tabs.TITLE);
        map.put(Tabs.URL, Tabs.URL);
        map.put(Tabs.HISTORY, Tabs.HISTORY);
        map.put(Tabs.FAVICON, Tabs.FAVICON);
        map.put(Tabs.LAST_USED, Tabs.LAST_USED);
        map.put(Tabs.POSITION, Tabs.POSITION);
        map.put(Clients.GUID, Clients.GUID);
        map.put(Clients.NAME, Clients.NAME);
        map.put(Clients.LAST_MODIFIED, Clients.LAST_MODIFIED);
        map.put(Clients.DEVICE_TYPE, Clients.DEVICE_TYPE);
        TABS_PROJECTION_MAP = Collections.unmodifiableMap(map);

        map = new HashMap<String, String>();
        map.put(Clients.GUID, Clients.GUID);
        map.put(Clients.NAME, Clients.NAME);
        map.put(Clients.LAST_MODIFIED, Clients.LAST_MODIFIED);
        map.put(Clients.DEVICE_TYPE, Clients.DEVICE_TYPE);
        CLIENTS_PROJECTION_MAP = Collections.unmodifiableMap(map);
    }

    private static final String selectColumn(String table, String column) {
        return table + "." + column + " = ?";
    }

    final class TabsDatabaseHelper extends SQLiteOpenHelper {
        public TabsDatabaseHelper(Context context, String databasePath) {
            super(context, databasePath, null, DATABASE_VERSION);
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            debug("Creating tabs.db: " + db.getPath());
            debug("Creating " + TABLE_TABS + " table");

            // Table for each tab on any client.
            db.execSQL("CREATE TABLE " + TABLE_TABS + "(" +
                       Tabs._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                       Tabs.CLIENT_GUID + " TEXT," +
                       Tabs.TITLE + " TEXT," +
                       Tabs.URL + " TEXT," +
                       Tabs.HISTORY + " TEXT," +
                       Tabs.FAVICON + " TEXT," +
                       Tabs.LAST_USED + " INTEGER," +
                       Tabs.POSITION + " INTEGER" +
                       ");");

            // Indices on CLIENT_GUID and POSITION.
            db.execSQL("CREATE INDEX " + INDEX_TABS_GUID +
                       " ON " + TABLE_TABS + "(" + Tabs.CLIENT_GUID + ")");
            db.execSQL("CREATE INDEX " + INDEX_TABS_POSITION +
                       " ON " + TABLE_TABS + "(" + Tabs.POSITION + ")");

            debug("Creating " + TABLE_CLIENTS + " table");

            // Table for client's name-guid mapping.
            db.execSQL("CREATE TABLE " + TABLE_CLIENTS + "(" +
                       Clients.GUID + " TEXT PRIMARY KEY," +
                       Clients.NAME + " TEXT," +
                       Clients.LAST_MODIFIED + " INTEGER," +
                       Clients.DEVICE_TYPE + " TEXT" +
                       ");");

            // Index on GUID.
            db.execSQL("CREATE INDEX " + INDEX_CLIENTS_GUID +
                       " ON " + TABLE_CLIENTS + "(" + Clients.GUID + ")");

            createLocalClient(db);
        }

        // Insert a client row for our local Fennec client.
        private void createLocalClient(SQLiteDatabase db) {
            debug("Inserting local Fennec client into " + TABLE_CLIENTS + " table");

            ContentValues values = new ContentValues();
            values.put(BrowserContract.Clients.LAST_MODIFIED, System.currentTimeMillis());
            db.insertOrThrow(TABLE_CLIENTS, null, values);
        }

        protected void upgradeDatabaseFrom2to3(SQLiteDatabase db) {
            debug("Setting remote client device types to 'mobile' in " + TABLE_CLIENTS + " table");

            // Add type to client, defaulting to mobile. This is correct for our
            // local client; all remote clients will be updated by Sync.
            db.execSQL("ALTER TABLE " + TABLE_CLIENTS + " ADD COLUMN " + BrowserContract.Clients.DEVICE_TYPE + " TEXT DEFAULT 'mobile'");
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            debug("Upgrading tabs.db: " + db.getPath() + " from " +
                  oldVersion + " to " + newVersion);

            // We have to do incremental upgrades until we reach the current
            // database schema version.
            for (int v = oldVersion + 1; v <= newVersion; v++) {
                switch(v) {
                    case 2:
                        createLocalClient(db);
                        break;

                    case 3:
                        upgradeDatabaseFrom2to3(db);
                        break;
                 }
             }
        }

        @Override
        public void onOpen(SQLiteDatabase db) {
            debug("Opening tabs.db: " + db.getPath());
            db.rawQuery("PRAGMA synchronous=OFF", null).close();

            if (shouldUseTransactions()) {
                // Modern Android allows WAL to be enabled through a mode flag.
                if (Versions.preJB) {
                    db.enableWriteAheadLogging();
                }
                db.setLockingEnabled(false);
                return;
            }

            // If we're not using transactions (in particular, prior to
            // Honeycomb), then we can do some lesser optimizations.
            db.rawQuery("PRAGMA journal_mode=PERSIST", null).close();
        }
    }

    @Override
    public String getType(Uri uri) {
        final int match = URI_MATCHER.match(uri);

        trace("Getting URI type: " + uri);

        switch (match) {
            case TABS:
                trace("URI is TABS: " + uri);
                return Tabs.CONTENT_TYPE;

            case TABS_ID:
                trace("URI is TABS_ID: " + uri);
                return Tabs.CONTENT_ITEM_TYPE;

            case CLIENTS:
                trace("URI is CLIENTS: " + uri);
                return Clients.CONTENT_TYPE;

            case CLIENTS_ID:
                trace("URI is CLIENTS_ID: " + uri);
                return Clients.CONTENT_ITEM_TYPE;
        }

        debug("URI has unrecognized type: " + uri);

        return null;
    }

    @Override
    @SuppressWarnings("fallthrough")
    public int deleteInTransaction(Uri uri, String selection, String[] selectionArgs) {
        trace("Calling delete in transaction on URI: " + uri);

        final int match = URI_MATCHER.match(uri);
        int deleted = 0;

        switch (match) {
            case CLIENTS_ID:
                trace("Delete on CLIENTS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, selectColumn(TABLE_CLIENTS, Clients.ROWID));
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case CLIENTS:
                trace("Delete on CLIENTS: " + uri);
                // Delete from both TABLE_TABS and TABLE_CLIENTS.
                deleteValues(uri, selection, selectionArgs, TABLE_TABS);
                deleted = deleteValues(uri, selection, selectionArgs, TABLE_CLIENTS);
                break;

            case TABS_ID:
                trace("Delete on TABS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, selectColumn(TABLE_TABS, Tabs._ID));
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case TABS:
                trace("Deleting on TABS: " + uri);
                deleted = deleteValues(uri, selection, selectionArgs, TABLE_TABS);
                break;

            default:
                throw new UnsupportedOperationException("Unknown delete URI " + uri);
        }

        debug("Deleted " + deleted + " rows for URI: " + uri);

        return deleted;
    }

    @Override
    public Uri insertInTransaction(Uri uri, ContentValues values) {
        trace("Calling insert in transaction on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int match = URI_MATCHER.match(uri);
        long id = -1;

        switch (match) {
            case CLIENTS:
                String guid = values.getAsString(Clients.GUID);
                debug("Inserting client in database with GUID: " + guid);
                id = db.insertOrThrow(TABLE_CLIENTS, Clients.GUID, values);
                break;

            case TABS:
                String url = values.getAsString(Tabs.URL);
                debug("Inserting tab in database with URL: " + url);
                id = db.insertOrThrow(TABLE_TABS, Tabs.TITLE, values);
                break;

            default:
                throw new UnsupportedOperationException("Unknown insert URI " + uri);
        }

        debug("Inserted ID in database: " + id);

        if (id >= 0)
            return ContentUris.withAppendedId(uri, id);

        return null;
    }

    @Override
    public int updateInTransaction(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        trace("Calling update in transaction on URI: " + uri);

        int match = URI_MATCHER.match(uri);
        int updated = 0;

        switch (match) {
            case CLIENTS_ID:
                trace("Update on CLIENTS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, selectColumn(TABLE_CLIENTS, Clients.ROWID));
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case CLIENTS:
                trace("Update on CLIENTS: " + uri);
                updated = updateValues(uri, values, selection, selectionArgs, TABLE_CLIENTS);
                break;

            case TABS_ID:
                trace("Update on TABS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, selectColumn(TABLE_TABS, Tabs._ID));
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case TABS:
                trace("Update on TABS: " + uri);
                updated = updateValues(uri, values, selection, selectionArgs, TABLE_TABS);
                break;

            default:
                throw new UnsupportedOperationException("Unknown update URI " + uri);
        }

        debug("Updated " + updated + " rows for URI: " + uri);

        return updated;
    }

    @Override
    @SuppressWarnings("fallthrough")
    public Cursor query(Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder) {
        SQLiteDatabase db = getReadableDatabase(uri);
        final int match = URI_MATCHER.match(uri);

        SQLiteQueryBuilder qb = new SQLiteQueryBuilder();
        String limit = uri.getQueryParameter(BrowserContract.PARAM_LIMIT);

        switch (match) {
            case TABS_ID:
                trace("Query is on TABS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, selectColumn(TABLE_TABS, Tabs._ID));
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case TABS:
                trace("Query is on TABS: " + uri);
                if (TextUtils.isEmpty(sortOrder)) {
                    sortOrder = DEFAULT_TABS_SORT_ORDER;
                } else {
                    debug("Using sort order " + sortOrder + ".");
                }

                qb.setProjectionMap(TABS_PROJECTION_MAP);
                qb.setTables(TABLE_TABS + " LEFT OUTER JOIN " + TABLE_CLIENTS + " ON (" + TABLE_TABS + "." + Tabs.CLIENT_GUID + " = " + TABLE_CLIENTS + "." + Clients.GUID + ")");
                break;

            case CLIENTS_ID:
                trace("Query is on CLIENTS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, selectColumn(TABLE_CLIENTS, Clients.ROWID));
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case CLIENTS:
                trace("Query is on CLIENTS: " + uri);
                if (TextUtils.isEmpty(sortOrder)) {
                    sortOrder = DEFAULT_CLIENTS_SORT_ORDER;
                } else {
                    debug("Using sort order " + sortOrder + ".");
                }

                qb.setProjectionMap(CLIENTS_PROJECTION_MAP);
                qb.setTables(TABLE_CLIENTS);
                break;

            default:
                throw new UnsupportedOperationException("Unknown query URI " + uri);
        }

        trace("Running built query.");
        final Cursor cursor = qb.query(db, projection, selection, selectionArgs, null, null, sortOrder, limit);
        cursor.setNotificationUri(getContext().getContentResolver(), BrowserContract.TABS_AUTHORITY_URI);

        return cursor;
    }

    int updateValues(Uri uri, ContentValues values, String selection, String[] selectionArgs, String table) {
        trace("Updating tabs on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        beginWrite(db);
        return db.update(table, values, selection, selectionArgs);
    }

    int deleteValues(Uri uri, String selection, String[] selectionArgs, String table) {
        debug("Deleting tabs for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        beginWrite(db);
        return db.delete(table, selection, selectionArgs);
    }

    @Override
    protected TabsDatabaseHelper createDatabaseHelper(Context context, String databasePath) {
        return new TabsDatabaseHelper(context, databasePath);
    }

    @Override
    protected String getDatabaseName() {
        return DATABASE_NAME;
    }
}
