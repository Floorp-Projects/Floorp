/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import org.mozilla.gecko.db.BrowserContract.Clients;
import org.mozilla.gecko.db.BrowserContract.Tabs;

import android.content.ContentUris;
import android.content.ContentValues;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteQueryBuilder;
import android.net.Uri;
import android.text.TextUtils;

public class TabsProvider extends SharedBrowserDatabaseProvider {
    private static final long ONE_DAY_IN_MILLISECONDS = 1000 * 60 * 60 * 24;
    private static final long ONE_WEEK_IN_MILLISECONDS = 7 * ONE_DAY_IN_MILLISECONDS;
    private static final long THREE_WEEKS_IN_MILLISECONDS = 3 * ONE_WEEK_IN_MILLISECONDS;

    static final String TABLE_TABS = "tabs";
    static final String TABLE_CLIENTS = "clients";

    static final int TABS = 600;
    static final int TABS_ID = 601;
    static final int CLIENTS = 602;
    static final int CLIENTS_ID = 603;
    static final int CLIENTS_NO_STALE_SORTED = 604;

    // Exclude clients that are more than three weeks old and also any duplicates that are older than one week old.
    static final String EXCLUDE_STALE_CLIENTS_SUBQUERY =
    "(SELECT " + Clients.GUID +
    ", " + Clients.NAME +
    ", " + Clients.LAST_MODIFIED +
    ", " + Clients.DEVICE_TYPE +
    "  FROM " + TABLE_CLIENTS +
    "  WHERE " + Clients.LAST_MODIFIED + " > %1$s " +
    " GROUP BY " + Clients.NAME +
    " UNION ALL " +
    " SELECT c." +  Clients.GUID + " AS " + Clients.GUID +
    ", c." + Clients.NAME + " AS " + Clients.NAME +
    ", c." + Clients.LAST_MODIFIED + " AS " + Clients.LAST_MODIFIED +
    ", c." + Clients.DEVICE_TYPE + " AS " + Clients.DEVICE_TYPE +
    " FROM " + TABLE_CLIENTS + " AS c " +
    " JOIN (" +
        " SELECT " + Clients.GUID +
        ", " + "MAX( " + Clients.LAST_MODIFIED + ") AS " + Clients.LAST_MODIFIED +
        " FROM " + TABLE_CLIENTS +
        " WHERE (" + Clients.LAST_MODIFIED + " < %1$s" + " AND " + Clients.LAST_MODIFIED + " > %2$s) AND " +
        Clients.NAME + " NOT IN " + "( SELECT " + Clients.NAME + " FROM " + TABLE_CLIENTS + " WHERE " + Clients.LAST_MODIFIED + " > %1$s)" +
        " GROUP BY " + Clients.NAME +
    ") AS c2" +
    " ON c." + Clients.GUID + " = c2." + Clients.GUID + ")";

    static final String DEFAULT_TABS_SORT_ORDER = Clients.LAST_MODIFIED + " DESC, " + Tabs.LAST_USED + " DESC";
    static final String DEFAULT_CLIENTS_SORT_ORDER = Clients.LAST_MODIFIED + " DESC";
    static final String DEFAULT_CLIENTS_NAME_ORDER = Clients.NAME + " COLLATE NOCASE ASC";

    static final String INDEX_TABS_GUID = "tabs_guid_index";
    static final String INDEX_TABS_POSITION = "tabs_position_index";

    static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

    static final Map<String, String> TABS_PROJECTION_MAP;
    static final Map<String, String> CLIENTS_PROJECTION_MAP;
    static final Map<String, String> CLIENTS_RECENCY_PROJECTION_MAP;

    static {
        URI_MATCHER.addURI(BrowserContract.TABS_AUTHORITY, "tabs", TABS);
        URI_MATCHER.addURI(BrowserContract.TABS_AUTHORITY, "tabs/#", TABS_ID);
        URI_MATCHER.addURI(BrowserContract.TABS_AUTHORITY, "clients", CLIENTS);
        URI_MATCHER.addURI(BrowserContract.TABS_AUTHORITY, "clients/#", CLIENTS_ID);
        URI_MATCHER.addURI(BrowserContract.TABS_AUTHORITY, "clients_no_stale_sorted", CLIENTS_NO_STALE_SORTED);

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

        map = new HashMap<>();
        map.put(Clients.GUID, projectColumn(TABLE_CLIENTS, Clients.GUID) + " AS guid");
        map.put(Clients.NAME, projectColumn(TABLE_CLIENTS, Clients.NAME) + " AS name");
        map.put(Clients.LAST_MODIFIED, projectColumn(TABLE_CLIENTS, Clients.LAST_MODIFIED) + " AS last_modified");
        map.put(Clients.DEVICE_TYPE, projectColumn(TABLE_CLIENTS, Clients.DEVICE_TYPE) + " AS device_type");
        // last_used is the max of the tab last_used times, or if there are no tabs,
        // the client's last_modified time.
        map.put(Tabs.LAST_USED, "COALESCE(MAX(" + projectColumn(TABLE_TABS, Tabs.LAST_USED) + "), " + projectColumn(TABLE_CLIENTS, Clients.LAST_MODIFIED) + ") AS last_used");
        CLIENTS_RECENCY_PROJECTION_MAP = Collections.unmodifiableMap(map);
    }

    private static final String projectColumn(String table, String column) {
        return table + "." + column;
    }

    private static final String selectColumn(String table, String column) {
        return projectColumn(table, column) + " = ?";
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
                selection = DatabaseUtils.concatenateWhere(selection, selectColumn(TABLE_CLIENTS, Clients._ID));
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case CLIENTS:
                trace("Delete on CLIENTS: " + uri);
                deleted = deleteValues(uri, selection, selectionArgs, TABLE_CLIENTS);
                break;

            case TABS_ID:
                trace("Delete on TABS_ID: " + uri);
                selection = DatabaseUtils.concatenateWhere(selection, selectColumn(TABLE_TABS, Tabs._ID));
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
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
    @SuppressWarnings("fallthrough")
    public int updateInTransaction(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        trace("Calling update in transaction on URI: " + uri);

        int match = URI_MATCHER.match(uri);
        int updated = 0;

        switch (match) {
            case CLIENTS_ID:
                trace("Update on CLIENTS_ID: " + uri);
                selection = DatabaseUtils.concatenateWhere(selection, selectColumn(TABLE_CLIENTS, Clients._ID));
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case CLIENTS:
                trace("Update on CLIENTS: " + uri);
                updated = updateValues(uri, values, selection, selectionArgs, TABLE_CLIENTS);
                break;

            case TABS_ID:
                trace("Update on TABS_ID: " + uri);
                selection = DatabaseUtils.concatenateWhere(selection, selectColumn(TABLE_TABS, Tabs._ID));
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
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

        String groupBy = null;
        SQLiteQueryBuilder qb = new SQLiteQueryBuilder();
        String limit = uri.getQueryParameter(BrowserContract.PARAM_LIMIT);

        switch (match) {
            case TABS_ID:
                trace("Query is on TABS_ID: " + uri);
                selection = DatabaseUtils.concatenateWhere(selection, selectColumn(TABLE_TABS, Tabs._ID));
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
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
                qb.setTables(TABLE_TABS + " LEFT OUTER JOIN " + TABLE_CLIENTS + " ON (" + TABLE_TABS + "." + Tabs.CLIENT_GUID + " = " + TABLE_CLIENTS + "." + Clients.GUID +
                             " OR (" + TABLE_TABS + "." + Tabs.CLIENT_GUID + " IS NULL AND " + TABLE_CLIENTS + "." + Clients.GUID + " IS NULL))");
                // 2nd OR clause is because the local client GUID is NULL and we can't assume that SQLite will compare NULL values with the equal operator
                // More info here: http://stackoverflow.com/questions/5423751/how-do-the-sql-is-and-operators-differ/5424558#5424558
                break;

            case CLIENTS_ID:
                trace("Query is on CLIENTS_ID: " + uri);
                selection = DatabaseUtils.concatenateWhere(selection, selectColumn(TABLE_CLIENTS, Clients._ID));
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
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

            case CLIENTS_NO_STALE_SORTED:
                trace("Query is on CLIENTS_NO_STALE_SORTED: " + uri);
                if (TextUtils.isEmpty(sortOrder)) {
                    sortOrder = DEFAULT_CLIENTS_NAME_ORDER;
                } else {
                    debug("Using sort order " + sortOrder + ".");
                }

                final long oneWeekAgo = System.currentTimeMillis() - ONE_WEEK_IN_MILLISECONDS;
                final long threeWeeksAgo = System.currentTimeMillis() - THREE_WEEKS_IN_MILLISECONDS;

                final String excludeStaleClientsTable = String.format(EXCLUDE_STALE_CLIENTS_SUBQUERY, oneWeekAgo, threeWeeksAgo);

                qb.setProjectionMap(CLIENTS_RECENCY_PROJECTION_MAP);

                // Use a subquery to quietly exclude stale duplicate client records.
                qb.setTables(excludeStaleClientsTable + " AS " + TABLE_CLIENTS + " LEFT OUTER JOIN " + TABLE_TABS +
                        " ON (" + projectColumn(TABLE_CLIENTS, Clients.GUID) +
                        " = " + projectColumn(TABLE_TABS, Tabs.CLIENT_GUID) + ")");
                groupBy = projectColumn(TABLE_CLIENTS, Clients.GUID);
                break;

            default:
                throw new UnsupportedOperationException("Unknown query URI " + uri);
        }

        trace("Running built query.");
        final Cursor cursor = qb.query(db, projection, selection, selectionArgs, groupBy, null, sortOrder, limit);
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
}
