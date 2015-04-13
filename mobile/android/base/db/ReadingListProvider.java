/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentUris;
import android.content.ContentValues;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.SQLException;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteQueryBuilder;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;
import org.mozilla.gecko.db.DBUtils.UpdateOperation;

import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.*;

public class ReadingListProvider extends SharedBrowserDatabaseProvider {
    private static final String LOGTAG = "GeckoRLProvider";

    static final String TABLE_READING_LIST = TABLE_NAME;

    static final int ITEMS = 101;
    static final int ITEMS_ID = 102;
    static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

    public static final String PLACEHOLDER_THIS_DEVICE = "$local";

    static {
        URI_MATCHER.addURI(BrowserContract.READING_LIST_AUTHORITY, "items", ITEMS);
        URI_MATCHER.addURI(BrowserContract.READING_LIST_AUTHORITY, "items/#", ITEMS_ID);
    }

    /**
     * Updates items that match the selection criteria. If no such items is found
     * one is inserted with the attributes passed in. Returns 0 if no item updated.
     *
     * Only use this method for callers, not internally -- it futzes with the provided
     * values to set syncing flags.
     *
     * @return Number of items updated or inserted
     */
    public int updateOrInsertItem(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        if (!values.containsKey(CLIENT_LAST_MODIFIED)) {
            values.put(CLIENT_LAST_MODIFIED, System.currentTimeMillis());
        }

        if (isCallerSync(uri)) {
            int updated = updateItemsWithFlags(uri, values, null, selection, selectionArgs);
            if (updated > 0) {
                return updated;
            }
            return insertItem(uri, values) != -1 ? 1 : 0;
        }

        // Assume updated.
        final ContentValues flags = processChangeValues(values);

        int updated = updateItemsWithFlags(uri, values, flags, selection, selectionArgs);
        if (updated <= 0) {
            // Must be an insertion. Let's make sure we're NEW and discard any update flags.
            values.put(SYNC_STATUS, SYNC_STATUS_NEW);
            values.put(SYNC_CHANGE_FLAGS, SYNC_CHANGE_NONE);
            updated = insertItem(uri, values) != -1 ? 1 : 0;
        }
        return updated;
    }

    /**
     * This method does two things:
     * * Based on the values provided, it computes and returns an incremental status change
     *   that can be applied to the database to track changes for syncing. This should be
     *   applied with {@link UpdateOperation#BITWISE_OR}.
     * * It mutates the provided values to mark absolute field changes.
     *
     * @return null if no values were provided, or no change needs to be recorded.
     */
    private ContentValues processChangeValues(ContentValues values) {
        if (values == null || values.size() == 0) {
            return null;
        }

        final ContentValues out = new ContentValues();
        int flag = 0;
        if (values.containsKey(MARKED_READ_BY) ||
            values.containsKey(MARKED_READ_ON) ||
            values.containsKey(IS_UNREAD)) {
            flag |= SYNC_CHANGE_UNREAD_CHANGED;
        }

        if (values.containsKey(IS_FAVORITE)) {
            flag |= SYNC_CHANGE_FAVORITE_CHANGED;
        }

        if (values.containsKey(RESOLVED_URL) ||
            values.containsKey(RESOLVED_TITLE) ||
            values.containsKey(EXCERPT)) {
            flag |= SYNC_CHANGE_RESOLVED;
        }

        if (flag == 0) {
            return null;
        }

        out.put(SYNC_CHANGE_FLAGS, flag);
        return out;
    }

    /**
     * Updates items that match the selection criteria.
     *
     * @return Number of items updated or inserted
     */
    public int updateItemsWithFlags(Uri uri, ContentValues values, ContentValues flags, String selection, String[] selectionArgs) {
        trace("Updating ReadingListItems on URI: " + uri);
        final SQLiteDatabase db = getWritableDatabase(uri);
        if (!values.containsKey(CLIENT_LAST_MODIFIED)) {
            values.put(CLIENT_LAST_MODIFIED, System.currentTimeMillis());
        }

        if (flags == null) {
            // This code path is used by Sync. Bypass metadata changes.
            return db.update(TABLE_READING_LIST, values, selection, selectionArgs);
        }

        // Set synced items to MODIFIED; otherwise, leave the sync status untouched.
        final ContentValues setModified = new ContentValues();
        setModified.put(SYNC_STATUS, "CASE " + SYNC_STATUS +
                                     " WHEN " + SYNC_STATUS_SYNCED +
                                     " THEN " + SYNC_STATUS_MODIFIED +
                                     " ELSE " + SYNC_STATUS +
                                     " END");

        final ContentValues[] valuesAndFlags = {values, flags, setModified};
        final UpdateOperation[] ops = {UpdateOperation.ASSIGN, UpdateOperation.BITWISE_OR, UpdateOperation.EXPRESSION};

        return DBUtils.updateArrays(db, TABLE_READING_LIST, valuesAndFlags, ops, selection, selectionArgs);
    }

    /**
     * Inserts a new item into the DB. CLIENT_LAST_MODIFIED is generated if it is not specified.
     *
     * Non-Sync callers will have ADDED_ON and ADDED_BY set appropriately if they are missing;
     * the assumption is that this is a new item added on this device.
     *
     * @return ID of the newly inserted item
     */
    private long insertItem(Uri uri, ContentValues values) {
        if (!values.containsKey(CLIENT_LAST_MODIFIED)) {
            values.put(CLIENT_LAST_MODIFIED, System.currentTimeMillis());
        }

        // We trust the syncing code to specify SYNC_STATUS_SYNCED.
        if (!isCallerSync(uri)) {
            values.put(SYNC_STATUS, SYNC_STATUS_NEW);
            if (!values.containsKey(ADDED_ON)) {
                values.put(ADDED_ON, System.currentTimeMillis());
            }
            if (!values.containsKey(ADDED_BY)) {
                values.put(ADDED_BY, PLACEHOLDER_THIS_DEVICE);
            }
        }

        final String url = values.getAsString(URL);
        debug("Inserting item in database with URL: " + url);
        try {
            return getWritableDatabase(uri).insertOrThrow(TABLE_READING_LIST, null, values);
        } catch (SQLException e) {
            Log.e(LOGTAG, "Insert failed.", e);
            throw e;
        }
    }

    private static final ContentValues DELETED_VALUES;
    static {
        final ContentValues values = new ContentValues();
        values.put(IS_DELETED, 1);

        values.put(URL, "");             // Non-null column.
        values.putNull(RESOLVED_URL);
        values.putNull(RESOLVED_TITLE);
        values.putNull(TITLE);
        values.putNull(EXCERPT);
        values.putNull(ADDED_BY);
        values.putNull(MARKED_READ_BY);

        // Mark it as deleted for sync purposes.
        values.put(SYNC_STATUS, SYNC_STATUS_DELETED);
        values.put(SYNC_CHANGE_FLAGS, SYNC_CHANGE_NONE);
        DELETED_VALUES = values;
    }

    /**
     * Deletes items. Item is marked as 'deleted' so that sync can
     * detect the change.
     *
     * It's the caller's responsibility to handle both original and resolved URLs.
     * @return Number of deleted items
     */
    int deleteItems(final Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting item entry for URI: " + uri);
        final SQLiteDatabase db = getWritableDatabase(uri);

        // TODO: also ensure that we delete affected items from the disk cache. Bug 1133158.
        if (isCallerSync(uri)) {
            debug("Directly deleting from reading list.");
            return db.delete(TABLE_READING_LIST, selection, selectionArgs);
        }

        // If we don't have a GUID for this item, then it hasn't made it
        // to the server. Just delete it.
        // If we do have a GUID, blank the row and mark it as deleted.
        int total = 0;
        final String whereNullGUID = DBUtils.concatenateWhere(selection, GUID + " IS NULL");
        final String whereNotNullGUID = DBUtils.concatenateWhere(selection, GUID + " IS NOT NULL");

        total += db.delete(TABLE_READING_LIST, whereNullGUID, selectionArgs);
        total += updateItemsWithFlags(uri, DELETED_VALUES, null, whereNotNullGUID, selectionArgs);

        return total;
    }

    int deleteItemByID(final Uri uri, long id) {
        debug("Deleting item entry for ID: " + id);
        final SQLiteDatabase db = getWritableDatabase(uri);

        // TODO: also ensure that we delete affected items from the disk cache. Bug 1133158.
        if (isCallerSync(uri)) {
            debug("Directly deleting from reading list.");
            final String selection = _ID + " = " + id;
            return db.delete(TABLE_READING_LIST, selection, null);
        }

        // If we don't have a GUID for this item, then it hasn't made it
        // to the server. Just delete it.
        final String whereNullGUID = _ID + " = " + id + " AND " + GUID + " IS NULL";
        final int raw = db.delete(TABLE_READING_LIST, whereNullGUID, null);
        if (raw > 0) {
            // _ID is unique, so this should only ever be 1, but it definitely means
            // we don't need to try the second part.
            return raw;
        }

        // If we do have a GUID, blank the row and mark it as deleted.
        final String whereNotNullGUID = _ID + " = " + id + " AND " + GUID + " IS NOT NULL";
        final ContentValues values = new ContentValues(DELETED_VALUES);
        values.put(CLIENT_LAST_MODIFIED, System.currentTimeMillis());
        return updateItemsWithFlags(uri, values, null, whereNotNullGUID, null);
    }

    @Override
    @SuppressWarnings("fallthrough")
    public int updateInTransaction(final Uri uri, ContentValues values, String selection, String[] selectionArgs) {
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
                if (shouldUpdateOrInsert(uri)) {
                    // updateOrInsertItem handles change flags for us.
                    updated = updateOrInsertItem(uri, values, selection, selectionArgs);
                } else {
                    // Don't use flags if we're inserting from sync.
                    ContentValues flags = isCallerSync(uri) ? null : processChangeValues(values);
                    updated = updateItemsWithFlags(uri, values, flags, selection, selectionArgs);
                }
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

        // This will never clean up any items that we're about to delete, so we
        // might as well run it first!
        cleanUpSomeDeletedRecords(uri, TABLE_READING_LIST);

        int numDeleted = 0;
        int match = URI_MATCHER.match(uri);

        switch (match) {
            case ITEMS_ID:
                debug("Deleting on ITEMS_ID: " + uri);
                numDeleted = deleteItemByID(uri, ContentUris.parseId(uri));
                break;

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
                // Log here because we typically insert in a batch, and that will muffle.
                Log.e(LOGTAG, "Unknown insert URI " + uri);
                throw new UnsupportedOperationException("Unknown insert URI " + uri);
        }

        debug("Inserted ID in database: " + id);

        if (id >= 0) {
            return ContentUris.withAppendedId(uri, id);
        }

        Log.e(LOGTAG, "Got to end of insertInTransaction without returning an id!");
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
                selection = DBUtils.concatenateWhere(selection, _ID + " = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });

            case ITEMS:
                trace("Query on ITEMS: " + uri);
                if (!shouldShowDeleted(uri)) {
                    selection = DBUtils.concatenateWhere(IS_DELETED + " = 0", selection);
                }
                break;

            default:
                throw new UnsupportedOperationException("Unknown query URI " + uri);
        }

        if (TextUtils.isEmpty(sortOrder)) {
            sortOrder = DEFAULT_SORT_ORDER;
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
                return CONTENT_TYPE;

            case ITEMS_ID:
                trace("URI is ITEMS_ID: " + uri);
                return CONTENT_ITEM_TYPE;
        }

        debug("URI has unrecognized type: " + uri);
        return null;
    }

    @Override
    protected String getDeletedItemSelection(long earlierThan) {
        if (earlierThan == -1L) {
            return IS_DELETED + " = 1";
        }
        return IS_DELETED + " = 1 AND " + CLIENT_LAST_MODIFIED + " <= " + earlierThan;
    }
}
