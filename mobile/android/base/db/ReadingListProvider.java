/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.sync.Utils;

import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteQueryBuilder;
import android.net.Uri;
import android.text.TextUtils;

public class ReadingListProvider extends TransactionalProvider<BrowserDatabaseHelper> {
    private static final String LOGTAG = "GeckoReadingListProv";

    static final String TABLE_READING_LIST = ReadingListItems.TABLE_NAME;

    static final int ITEMS = 101;
    static final int ITEMS_ID = 102;
    static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

    static {
        URI_MATCHER.addURI(BrowserContract.READING_LIST_AUTHORITY, "items", ITEMS);
        URI_MATCHER.addURI(BrowserContract.READING_LIST_AUTHORITY, "items/#", ITEMS_ID);
    }

    /**
     * Updates items that match the selection criteria. If no such items is found
     * one is inserted with the attributes passed in. Returns 0 if no item updated.
     *
     * @return Number of items updated or inserted
     */
    public int updateOrInsertItem(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        int updated = updateItems(uri, values, selection, selectionArgs);
        if (updated <= 0) {
            updated = insertItem(uri, values) != -1 ? 1 : 0;
        }
        return updated;
    }

    /**
     * Updates items that match the selection criteria.
     *
     * @return Number of items updated or inserted
     */
    public int updateItems(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        trace("Updating ReadingListItems on URI: " + uri);
        final SQLiteDatabase db = getWritableDatabase(uri);
        if (!values.containsKey(ReadingListItems.DATE_MODIFIED)) {
            values.put(ReadingListItems.DATE_MODIFIED, System.currentTimeMillis());
        }
        return db.update(TABLE_READING_LIST, values, selection, selectionArgs);
    }

    /**
     * Inserts a new item into the DB. DATE_CREATED, DATE_MODIFIED
     * and GUID fields are generated if they are not specified.
     *
     * @return ID of the newly inserted item
     */
    long insertItem(Uri uri, ContentValues values) {
        long now = System.currentTimeMillis();
        if (!values.containsKey(ReadingListItems.DATE_CREATED)) {
            values.put(ReadingListItems.DATE_CREATED, now);
        }

        if (!values.containsKey(ReadingListItems.DATE_MODIFIED)) {
            values.put(ReadingListItems.DATE_MODIFIED, now);
        }

        if (!values.containsKey(ReadingListItems.GUID)) {
            values.put(ReadingListItems.GUID, Utils.generateGuid());
        }

        String url = values.getAsString(ReadingListItems.URL);
        debug("Inserting item in database with URL: " + url);
        return getWritableDatabase(uri)
                .insertOrThrow(TABLE_READING_LIST, null, values);
    }

    /**
     * Deletes items. Item is marked as 'deleted' so that sync can
     * detect the change.
     *
     * @return Number of deleted items
     */
    int deleteItems(Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting item entry for URI: " + uri);
        final SQLiteDatabase db = getWritableDatabase(uri);

        if (isCallerSync(uri)) {
            return db.delete(TABLE_READING_LIST, selection, selectionArgs);
        }

        debug("Marking item entry as deleted for URI: " + uri);
        ContentValues values = new ContentValues();
        values.put(ReadingListItems.IS_DELETED, 1);

        cleanupSomeDeletedRecords(uri, ReadingListItems.CONTENT_URI, TABLE_READING_LIST);
        return updateItems(uri, values, selection, selectionArgs);
    }

    @Override
    @SuppressWarnings("fallthrough")
    public int updateInTransaction(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        trace("Calling update in transaction on URI: " + uri);

        int updated = 0;
        int match = URI_MATCHER.match(uri);

        switch (match) {
            case ITEMS_ID:
                debug("Update on ITEMS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, TABLE_READING_LIST + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });

            case ITEMS: {
                debug("Updating ITEMS: " + uri);
                updated = shouldUpdateOrInsert(uri) ?
                          updateOrInsertItem(uri, values, selection, selectionArgs) :
                          updateItems(uri, values, selection, selectionArgs);
                break;
            }

            default:
                throw new UnsupportedOperationException("Unknown update URI " + uri);
        }

        debug("Updated " + updated + " rows for URI: " + uri);
        return updated;
    }


    @Override
    @SuppressWarnings("fallthrough")
    public int deleteInTransaction(Uri uri, String selection, String[] selectionArgs) {
        trace("Calling delete in transaction on URI: " + uri);

        int numDeleted = 0;
        int match = URI_MATCHER.match(uri);

        switch (match) {
            case ITEMS_ID:
                debug("Deleting on ITEMS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, TABLE_READING_LIST + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });

            case ITEMS:
                debug("Deleting ITEMS: " + uri);
                numDeleted = deleteItems(uri, selection, selectionArgs);
                break;

            default:
                throw new UnsupportedOperationException("Unknown update URI " + uri);
        }

        debug("Deleted " + numDeleted + " rows for URI: " + uri);
        return numDeleted;
    }

    @Override
    public Uri insertInTransaction(Uri uri, ContentValues values) {
        trace("Calling insert in transaction on URI: " + uri);
        long id = -1;
        int match = URI_MATCHER.match(uri);

        switch (match) {
            case ITEMS:
                trace("Insert on ITEMS: " + uri);
                id = insertItem(uri, values);
                break;

            default:
                throw new UnsupportedOperationException("Unknown insert URI " + uri);
        }

        debug("Inserted ID in database: " + id);

        if (id >= 0) {
            return ContentUris.withAppendedId(uri, id);
        }

        return null;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        String groupBy = null;
        SQLiteDatabase db = getReadableDatabase(uri);
        SQLiteQueryBuilder qb = new SQLiteQueryBuilder();
        String limit = uri.getQueryParameter(BrowserContract.PARAM_LIMIT);

        final int match = URI_MATCHER.match(uri);
        switch (match) {
            case ITEMS_ID:
                trace("Query on ITEMS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, ReadingListItems._ID + " = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });

            case ITEMS:
                trace("Query on ITEMS: " + uri);
                if (!shouldShowDeleted(uri))
                    selection = DBUtils.concatenateWhere(ReadingListItems.IS_DELETED + " = 0", selection);
                break;

            default:
                throw new UnsupportedOperationException("Unknown query URI " + uri);
        }

        if (TextUtils.isEmpty(sortOrder)) {
            sortOrder = ReadingListItems.DEFAULT_SORT_ORDER;
        }

        trace("Running built query.");
        qb.setTables(TABLE_READING_LIST);
        Cursor cursor = qb.query(db, projection, selection, selectionArgs, groupBy, null, sortOrder, limit);
        cursor.setNotificationUri(getContext().getContentResolver(), uri);

        return cursor;
    }

    @Override
    public String getType(Uri uri) {
        trace("Getting URI type: " + uri);

        final int match = URI_MATCHER.match(uri);
        switch (match) {
            case ITEMS:
                trace("URI is ITEMS: " + uri);
                return ReadingListItems.CONTENT_TYPE;

            case ITEMS_ID:
                trace("URI is ITEMS_ID: " + uri);
                return ReadingListItems.CONTENT_ITEM_TYPE;
        }

        debug("URI has unrecognized type: " + uri);
        return null;
    }

    @Override
    protected BrowserDatabaseHelper createDatabaseHelper(Context context,
            String databasePath) {
        return new BrowserDatabaseHelper(context, databasePath);
    }

    @Override
    protected String getDatabaseName() {
        return BrowserDatabaseHelper.DATABASE_NAME;
    }
}
