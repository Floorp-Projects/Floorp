/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.db.PerProfileDatabases.DatabaseHelperFactory;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.SQLException;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;

/*
 * Abstract class containing methods needed to make a SQLite-based content provider with a
 * database helper of type T. Abstract methods insertInTransaction, deleteInTransaction and
 * updateInTransaction all called within a DB transaction so failed modifications can be rolled-back.
 */
public abstract class TransactionalProvider<T extends SQLiteOpenHelper> extends ContentProvider {
    private static final String LOGTAG = "GeckoTransProvider";
    protected Context mContext;
    protected PerProfileDatabases<T> mDatabases;

    /*
     * Returns the name of the database file. Used to get a path
     * to the DB file.
     *
     * @return name of the database file
     */
    abstract protected String getDatabaseName();

    /*
     * Creates and returns an instance of a DB helper. Given a
     * context and a path to the DB file
     *
     * @param  context       to use to create the database helper
     * @param  databasePath  path to the DB file
     * @return               instance of the database helper
     */
    abstract protected T createDatabaseHelper(Context context, String databasePath);

    /*
     * Inserts an item into the database within a DB transaction.
     *
     * @param uri    query URI
     * @param values column values to be inserted
     * @return       a URI for the newly inserted item
     */
    abstract protected Uri insertInTransaction(Uri uri, ContentValues values);

    /*
     * Deletes items from the database within a DB transaction.
     *
     * @param uri            query URI
     * @param selection      An optional filter to match rows to update.
     * @param selectionArgs  arguments for the selection
     * @return               number of rows impacted by the deletion
     */
    abstract protected int deleteInTransaction(Uri uri, String selection, String[] selectionArgs);

    /*
     * Updates the database within a DB transaction.
     *
     * @param uri            Query URI
     * @param values         A set of column_name/value pairs to add to the database.
     * @param selection      An optional filter to match rows to update.
     * @param selectionArgs  Arguments for the selection
     * @return               number of rows impacted by the update
     */
    abstract protected int updateInTransaction(Uri uri, ContentValues values, String selection, String[] selectionArgs);

    /*
     * Fetches a readable database based on the profile indicated in the
     * passed URI. If the URI does not contain a profile param, the default profile
     * is used.
     *
     * @param uri content URI optionally indicating the profile of the user
     * @return    instance of a readable SQLiteDatabase
     */
    protected SQLiteDatabase getReadableDatabase(Uri uri) {
        String profile = null;
        if (uri != null) {
            profile = uri.getQueryParameter(BrowserContract.PARAM_PROFILE);
        }

        return mDatabases.getDatabaseHelperForProfile(profile, isTest(uri)).getReadableDatabase();
    }

    /*
     * Fetches a writeable database based on the profile indicated in the
     * passed URI. If the URI does not contain a profile param, the default profile
     * is used
     *
     * @param uri content URI optionally indicating the profile of the user
     * @return    instance of a writeable SQLiteDatabase
     */
    protected SQLiteDatabase getWritableDatabase(Uri uri) {
        String profile = null;
        if (uri != null) {
            profile = uri.getQueryParameter(BrowserContract.PARAM_PROFILE);
        }

        return mDatabases.getDatabaseHelperForProfile(profile, isTest(uri)).getWritableDatabase();
    }

    @Override
    public boolean onCreate() {
        synchronized (this) {
            mContext = getContext();
            mDatabases = new PerProfileDatabases<T>(
                getContext(), getDatabaseName(), new DatabaseHelperFactory<T>() {
                    @Override
                    public T makeDatabaseHelper(Context context, String databasePath) {
                        return createDatabaseHelper(context, databasePath);
                    }
                });
        }

        return true;
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

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        trace("Calling insert on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        Uri result = null;
        try {
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
        } catch (SQLException sqle) {
            Log.e(LOGTAG, "exception in DB operation", sqle);
        } catch (UnsupportedOperationException uoe) {
            Log.e(LOGTAG, "don't know how to perform that insert", uoe);
        }

        if (result != null) {
            getContext().getContentResolver().notifyChange(uri, null);
        }

        return result;
    }


    @Override
    public int update(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
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

    @Override
    public int bulkInsert(Uri uri, ContentValues[] values) {
        if (values == null) {
            return 0;
        }

        int numValues = values.length;
        int successes = 0;

        final boolean hasTransactions = Build.VERSION.SDK_INT >= 11;
        final SQLiteDatabase db = getWritableDatabase(uri);

        if (hasTransactions) {
            db.beginTransaction();
        }

        try {
            for (int i = 0; i < numValues; i++) {
                insertInTransaction(uri, values[i]);
                successes++;
            }
            if (hasTransactions) {
                trace("Flushing DB bulkinsert...");
                db.setTransactionSuccessful();
            }
        } finally {
            if (hasTransactions) {
                db.endTransaction();
            }
        }

        if (successes > 0) {
            mContext.getContentResolver().notifyChange(uri, null);
        }

        return successes;
    }

    protected boolean isTest(Uri uri) {
        String isTest = uri.getQueryParameter(BrowserContract.PARAM_IS_TEST);
        return !TextUtils.isEmpty(isTest);
    }

    // Calculate these once, at initialization. isLoggable is too expensive to
    // have in-line in each log call.
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
}
