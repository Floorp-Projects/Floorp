/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.db.BrowserContract.HomeListItems;
import org.mozilla.gecko.db.PerProfileDatabases.DatabaseHelperFactory;
import org.mozilla.gecko.mozglue.RobocopTarget;

import android.content.ContentProvider;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.database.MatrixCursor.RowBuilder;
import android.database.SQLException;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;

public class HomeListsProvider extends ContentProvider {
    private static final String LOGTAG = "GeckoHomeListsProvider";

    // Flag to disable DB usage. This prevents us from creating (and later needing to upgrade) a DB.
    private static final boolean DB_DISABLED = true;

    private PerProfileDatabases<HomeListsDatabaseHelper> mDatabases;

    // Endpoint to return static fake data.
    static final int ITEMS_FAKE = 100;

    static final int ITEMS = 101;
    static final int ITEMS_ID = 102;

    static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

    static {
        URI_MATCHER.addURI(BrowserContract.HOME_LISTS_AUTHORITY, "items/fake", ITEMS_FAKE);

        URI_MATCHER.addURI(BrowserContract.HOME_LISTS_AUTHORITY, "items", ITEMS);
        URI_MATCHER.addURI(BrowserContract.HOME_LISTS_AUTHORITY, "items/#", ITEMS_ID);
    }

    private static boolean logDebug   = Log.isLoggable(LOGTAG, Log.DEBUG);
    private static boolean logVerbose = Log.isLoggable(LOGTAG, Log.VERBOSE);
    protected static void trace(String message) {
        if (logVerbose) {
            Log.v(LOGTAG, message);
        }
    }

    protected static void debug(String message) {
        if (logDebug) {
            Log.d(LOGTAG, message);
        }
    }

    private boolean isTest(Uri uri) {
        String isTest = uri.getQueryParameter(BrowserContract.PARAM_IS_TEST);
        return !TextUtils.isEmpty(isTest);
    }

    private SQLiteDatabase getReadableDatabase(Uri uri) {
        if (DB_DISABLED) {
            throw new UnsupportedOperationException("Database operations are disabled!");
        }

        String profile = null;
        if (uri != null) {
            profile = uri.getQueryParameter(BrowserContract.PARAM_PROFILE);
        }
        return mDatabases.getDatabaseHelperForProfile(profile, isTest(uri)).getReadableDatabase();
    }

    private SQLiteDatabase getWritableDatabase(Uri uri) {
        if (DB_DISABLED) {
            throw new UnsupportedOperationException("Database operations are disabled!");
        }

        String profile = null;
        if (uri != null) {
            profile = uri.getQueryParameter(BrowserContract.PARAM_PROFILE);
        }
        return mDatabases.getDatabaseHelperForProfile(profile, isTest(uri)).getWritableDatabase();
    }

    @Override
    public boolean onCreate() {
        if (DB_DISABLED) {
            return true;
        }
        synchronized (this) {
            mDatabases = new PerProfileDatabases<HomeListsDatabaseHelper>(getContext(), HomeListsDatabaseHelper.DATABASE_NAME,
                new DatabaseHelperFactory<HomeListsDatabaseHelper>() {
                    @Override
                    public HomeListsDatabaseHelper makeDatabaseHelper(Context context, String databasePath) {
                        return new HomeListsDatabaseHelper(context, databasePath);
                    }
                });
        }
        return true;
    }

    @Override
    public String getType(Uri uri) {
        trace("Getting URI type: " + uri);

        final int match = URI_MATCHER.match(uri);
        switch (match) {
            case ITEMS_FAKE: {
                trace("URI is ITEMS_FAKE: " + uri);
                return HomeListItems.CONTENT_TYPE;
            }
            case ITEMS: {
                trace("URI is ITEMS: " + uri);
                return HomeListItems.CONTENT_TYPE;
            }
            case ITEMS_ID: {
                trace("URI is ITEMS_ID: " + uri);
                return HomeListItems.CONTENT_ITEM_TYPE;
            }
        }

        debug("URI has unrecognized type: " + uri);
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        trace("Calling delete on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int deleted = 0;

        if (Build.VERSION.SDK_INT >= 11) {
            trace("Beginning delete transaction: " + uri);
            db.beginTransaction();
            try {
                deleted = deleteInTransaction(uri, selection, selectionArgs);
                db.setTransactionSuccessful();
                trace("Successful delete transaction: " + uri);
            } finally {
                db.endTransaction();
            }
        } else {
            deleted = deleteInTransaction(uri, selection, selectionArgs);
        }

        if (deleted > 0) {
            getContext().getContentResolver().notifyChange(uri, null);
        }
        return deleted;
    }

    public int deleteInTransaction(Uri uri, String selection, String[] selectionArgs) {
        trace("Calling delete in transaction on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int deleted = 0;

        final int match = URI_MATCHER.match(uri);
        switch (match) {
            case ITEMS: {
                trace("Delete on ITEMS: " + uri);
                deleted = db.delete(HomeListsDatabaseHelper.TABLE_ITEMS, selection, selectionArgs);
                break;
            }
            case ITEMS_ID: {
                trace("Delete on ITEMS: " + uri);
                // Not implemented
                break;
            }
            default:
                throw new UnsupportedOperationException("Unknown delete URI " + uri);
        }

        debug("Deleted " + deleted + " rows for URI: " + uri);
        return deleted;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        trace("Calling insert on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        Uri result = null;

        if (Build.VERSION.SDK_INT >= 11) {
            trace("Beginning insert transaction: " + uri);
            db.beginTransaction();
            try {
                result = insertInTransaction(uri, values);
                db.setTransactionSuccessful();
                trace("Successful insert transaction: " + uri);
            } finally {
                db.endTransaction();
            }
        } else {
            result = insertInTransaction(uri, values);
        }

        if (result != null) {
            getContext().getContentResolver().notifyChange(uri, null);
        }
        return result;
    }

    @Override
    public int bulkInsert(Uri uri, ContentValues[] values) {
        if (values == null) {
            return 0;
        }

        final SQLiteDatabase db = getWritableDatabase(uri);
        final int numValues = values.length;

        int successes = 0;

        db.beginTransaction();
        try {
            for (int i = 0; i < numValues; i++) {
                try {
                    insertInTransaction(uri, values[i]);
                    successes++;
                } catch (SQLException e) {
                    Log.e(LOGTAG, "SQLException in bulkInsert", e);

                    // Restart the transaction to continue insertions.
                    db.setTransactionSuccessful();
                    db.endTransaction();
                    db.beginTransaction();
                }
            }
            trace("Flushing DB bulkinsert...");
            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
        }

        if (successes > 0) {
            getContext().getContentResolver().notifyChange(uri, null);
        }
        return successes;
    }

    public Uri insertInTransaction(Uri uri, ContentValues values) {
        trace("Calling insert in transaction on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        long id = -1;

        final int match = URI_MATCHER.match(uri);
        switch (match) {
            case ITEMS: {
                trace("Insert on ITEMS: " + uri);
                id = db.insertOrThrow(HomeListsDatabaseHelper.TABLE_ITEMS, null, values);
                break;
            }
            case ITEMS_ID: {
                trace("Insert on ITEMS_ID: " + uri);
                // Not implemented
                break;
            }
            default:
                throw new UnsupportedOperationException("Unknown insert URI " + uri);
        }

        if (id >= 0) {
            debug("Inserted ID in database: " + id);
            return ContentUris.withAppendedId(uri, id);
        }
        return null;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        trace("Calling update on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int updated = 0;

        if (Build.VERSION.SDK_INT >= 11) {
            trace("Beginning update transaction: " + uri);
            db.beginTransaction();
            try {
                updated = updateInTransaction(uri, values, selection, selectionArgs);
                db.setTransactionSuccessful();
                trace("Successful update transaction: " + uri);
            } finally {
                db.endTransaction();
            }
        } else {
            updated = updateInTransaction(uri, values, selection, selectionArgs);
        }

        if (updated > 0) {
            getContext().getContentResolver().notifyChange(uri, null);
        }
        return updated;
    }


    public int updateInTransaction(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        trace("Calling update in transaction on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int updated = 0;

        final int match = URI_MATCHER.match(uri);
        switch (match) {
            case ITEMS: {
                trace("Update on ITEMS: " + uri);
                updated = db.update(HomeListsDatabaseHelper.TABLE_ITEMS, values, selection, selectionArgs);
                break;
            }
            case ITEMS_ID: {
                trace("Update on ITEMS_ID: " + uri);
                // Not implemented
                break;
            }
            default:
                throw new UnsupportedOperationException("Unknown update URI " + uri);
        }

        debug("Updated " + updated + " rows for URI: " + uri);
        return updated;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        final int match = URI_MATCHER.match(uri);

        // If we're querying the fake items, don't try to get the database.
        if (match == ITEMS_FAKE) {
            trace("Query on ITEMS_FAKE: " + uri);
            return queryFakeItems(uri, projection, selection, selectionArgs, sortOrder);
        }

        final SQLiteDatabase db = getReadableDatabase(uri);
        Cursor cursor = null;

        switch (match) {
            case ITEMS: {
                trace("Query on ITEMS: " + uri);
                cursor = db.query(HomeListsDatabaseHelper.TABLE_ITEMS, projection, selection, selectionArgs, null, null, sortOrder, null);
                break;
            }
            case ITEMS_ID: {
                trace("Query on ITEMS_ID: " + uri);
                // Not implemented
                break;
            }
            default:
                throw new UnsupportedOperationException("Unknown query URI " + uri);
        }
        return cursor;
    }

    /**
     * Returns a cursor populated with static fake data.
     */
    private Cursor queryFakeItems(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        final String[] itemsColumns = new String[] {
            HomeListItems._ID,
            HomeListItems.PROVIDER_ID,
            HomeListItems.URL,
            HomeListItems.TITLE
        };

        // XXX: Return more items (from JSON file?) and filter fake items by provider specified in selection
        final MatrixCursor c = new MatrixCursor(itemsColumns);
        c.addRow(new Object[] { 1, "fake-provider", "http://example.com", "Example" });
        c.addRow(new Object[] { 2, "fake-provider", "http://mozilla.org", "Mozilla" });
        return c;
    }
}
