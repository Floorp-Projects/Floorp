/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.db.BrowserContract.CommonColumns;
import org.mozilla.gecko.db.BrowserContract.SyncColumns;
import org.mozilla.gecko.db.PerProfileDatabases.DatabaseHelperFactory;
import org.mozilla.gecko.mozglue.RobocopTarget;

import android.content.ContentProvider;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
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
     * @param  databasePath  path to the DB  file
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
     * @param uri            Query URI.
     * @param selection      An optional filter to match rows to delete.
     * @param selectionArgs  An array of arguments to substitute into the selection.
     *
     * @return number of rows impacted by the deletion.
     */
    abstract protected int deleteInTransaction(Uri uri, String selection, String[] selectionArgs);

    /*
     * Updates the database within a DB transaction.
     *
     * @param uri            Query URI.
     * @param values         A set of column_name/value pairs to add to the database.
     * @param selection      An optional filter to match rows to update.
     * @param selectionArgs  An array of arguments to substitute into the selection.
     *
     * @return               number of rows impacted by the update.
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

    /**
     * Public version of {@link #getWritableDatabase(Uri) getWritableDatabase}.
     * This method should ONLY be used for testing purposes.
     *
     * @param uri content URI optionally indicating the profile of the user
     * @return    instance of a writeable SQLiteDatabase
     */
    @RobocopTarget
    public SQLiteDatabase getWritableDatabaseForTesting(Uri uri) {
        return getWritableDatabase(uri);
    }

    /**
     * Return true of the query is from Firefox Sync.
     * @param uri query URI
     */
    public static boolean isCallerSync(Uri uri) {
        String isSync = uri.getQueryParameter(BrowserContract.PARAM_IS_SYNC);
        return !TextUtils.isEmpty(isSync);
    }

    /**
     * Indicates whether a query should include deleted fields
     * based on the URI.
     * @param uri query URI
     */
    public static boolean shouldShowDeleted(Uri uri) {
        String showDeleted = uri.getQueryParameter(BrowserContract.PARAM_SHOW_DELETED);
        return !TextUtils.isEmpty(showDeleted);
    }

    /**
     * Indicates whether an insertion should be made if a record doesn't
     * exist, based on the URI.
     * @param uri query URI
     */
    public static boolean shouldUpdateOrInsert(Uri uri) {
        String insertIfNeeded = uri.getQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED);
        return Boolean.parseBoolean(insertIfNeeded);
    }

    /**
     * Indicates whether query is a test based on the URI.
     * @param uri query URI
     */
    public static boolean isTest(Uri uri) {
        String isTest = uri.getQueryParameter(BrowserContract.PARAM_IS_TEST);
        return !TextUtils.isEmpty(isTest);
    }

    protected SQLiteDatabase getWritableDatabaseForProfile(String profile, boolean isTest) {
        return mDatabases.getDatabaseHelperForProfile(profile, isTest).getWritableDatabase();
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

    /**
     * Return true if OS version and database parallelism support indicates
     * that this provider should bundle writes into transactions.
     */
    @SuppressWarnings("static-method")
    protected boolean shouldUseTransactions() {
        return Build.VERSION.SDK_INT >= 11;
    }

    /**
     * Track whether we're in a batch operation.
     *
     * When we're in a batch operation, individual write steps won't even try
     * to start a transaction... and neither will they attempt to finish one.
     *
     * Set this to <code>Boolean.TRUE</code> when you're entering a batch --
     * a section of code in which {@link ContentProvider} methods will be
     * called, but nested transactions should not be started. Callers are
     * responsible for beginning and ending the enclosing transaction, and
     * for setting this to <code>Boolean.FALSE</code> when done.
     *
     * This is a ThreadLocal separate from `db.inTransaction` because batched
     * operations start transactions independent of individual ContentProvider
     * operations. This doesn't work well with the entire concept of this
     * abstract class -- that is, automatically beginning and ending transactions
     * for each insert/delete/update operation -- and doing so without
     * causing arbitrary nesting requires external tracking.
     *
     * Note that beginWrite takes a DB argument, but we don't differentiate
     * between databases in this tracking flag. If your ContentProvider manages
     * multiple database transactions within the same thread, you'll need to
     * amend this scheme -- but then, you're already doing some serious wizardry,
     * so rock on.
     */
    final ThreadLocal<Boolean> isInBatchOperation = new ThreadLocal<Boolean>();

    private boolean isInBatch() {
        final Boolean isInBatch = isInBatchOperation.get();
        if (isInBatch == null) {
            return false;
        }
        return isInBatch.booleanValue();
    }

    /**
     * If we're not currently in a transaction, and we should be, start one.
     */
    protected void beginWrite(final SQLiteDatabase db) {
        if (isInBatch()) {
            trace("Not bothering with an intermediate write transaction: inside batch operation.");
            return;
        }

        if (shouldUseTransactions() && !db.inTransaction()) {
            trace("beginWrite: beginning transaction.");
            db.beginTransaction();
        }
    }

    /**
     * If we're not in a batch, but we are in a write transaction, mark it as
     * successful.
     */
    protected void markWriteSuccessful(final SQLiteDatabase db) {
        if (isInBatch()) {
            trace("Not marking write successful: inside batch operation.");
            return;
        }

        if (shouldUseTransactions() && db.inTransaction()) {
            trace("Marking write transaction successful.");
            db.setTransactionSuccessful();
        }
    }

    /**
     * If we're not in a batch, but we are in a write transaction,
     * end it.
     *
     * @see TransactionalProvider#markWriteSuccessful(SQLiteDatabase)
     */
    protected void endWrite(final SQLiteDatabase db) {
        if (isInBatch()) {
            trace("Not ending write: inside batch operation.");
            return;
        }

        if (shouldUseTransactions() && db.inTransaction()) {
            trace("endWrite: ending transaction.");
            db.endTransaction();
        }
    }

    protected void beginBatch(final SQLiteDatabase db) {
        trace("Beginning batch.");
        isInBatchOperation.set(Boolean.TRUE);
        db.beginTransaction();
    }

    protected void markBatchSuccessful(final SQLiteDatabase db) {
        if (isInBatch()) {
            trace("Marking batch successful.");
            db.setTransactionSuccessful();
            return;
        }
        Log.w(LOGTAG, "Unexpectedly asked to mark batch successful, but not in batch!");
        throw new IllegalStateException("Not in batch.");
    }

    protected void endBatch(final SQLiteDatabase db) {
        trace("Ending batch.");
        db.endTransaction();
        isInBatchOperation.set(Boolean.FALSE);
    }

    /*
     * This utility is replicated from RepoUtils, which is managed by android-sync.
     */
    protected static String computeSQLInClause(int items, String field) {
        final StringBuilder builder = new StringBuilder(field);
        builder.append(" IN (");
        int i = 0;
        for (; i < items - 1; ++i) {
            builder.append("?, ");
        }
        if (i < items) {
            builder.append("?");
        }
        builder.append(")");
        return builder.toString();
    }

    /**
     * Turn a single-column cursor of longs into a single SQL "IN" clause.
     * We can do this without using selection arguments because Long isn't
     * vulnerable to injection.
     */
    protected static String computeSQLInClauseFromLongs(final Cursor cursor, String field) {
        final StringBuilder builder = new StringBuilder(field);
        builder.append(" IN (");
        final int commaLimit = cursor.getCount() - 1;
        int i = 0;
        while (cursor.moveToNext()) {
            builder.append(cursor.getLong(0));
            if (i++ < commaLimit) {
                builder.append(", ");
            }
        }
        builder.append(")");
        return builder.toString();
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        trace("Calling delete on URI: " + uri + ", " + selection + ", " + selectionArgs);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int deleted = 0;

        try {
            deleted = deleteInTransaction(uri, selection, selectionArgs);
            markWriteSuccessful(db);
        } finally {
            endWrite(db);
        }

        if (deleted > 0) {
            final boolean shouldSyncToNetwork = !isCallerSync(uri);
            getContext().getContentResolver().notifyChange(uri, null, shouldSyncToNetwork);
        }

        return deleted;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        trace("Calling insert on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        Uri result = null;
        try {
            result = insertInTransaction(uri, values);
            markWriteSuccessful(db);
        } catch (SQLException sqle) {
            Log.e(LOGTAG, "exception in DB operation", sqle);
        } catch (UnsupportedOperationException uoe) {
            Log.e(LOGTAG, "don't know how to perform that insert", uoe);
        } finally {
            endWrite(db);
        }

        if (result != null) {
            final boolean shouldSyncToNetwork = !isCallerSync(uri);
            getContext().getContentResolver().notifyChange(uri, null, shouldSyncToNetwork);
        }

        return result;
    }


    @Override
    public int update(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        trace("Calling update on URI: " + uri + ", " + selection + ", " + selectionArgs);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int updated = 0;

        try {
            updated = updateInTransaction(uri, values, selection,
                                          selectionArgs);
            markWriteSuccessful(db);
        } finally {
            endWrite(db);
        }

        if (updated > 0) {
            final boolean shouldSyncToNetwork = !isCallerSync(uri);
            getContext().getContentResolver().notifyChange(uri, null, shouldSyncToNetwork);
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

        final SQLiteDatabase db = getWritableDatabase(uri);

        debug("bulkInsert: explicitly starting transaction.");
        beginBatch(db);

        try {
            for (int i = 0; i < numValues; i++) {
                insertInTransaction(uri, values[i]);
                successes++;
            }
            trace("Flushing DB bulkinsert...");
            markBatchSuccessful(db);
        } finally {
            debug("bulkInsert: explicitly ending transaction.");
            endBatch(db);
        }

        if (successes > 0) {
            final boolean shouldSyncToNetwork = !isCallerSync(uri);
            mContext.getContentResolver().notifyChange(uri, null, shouldSyncToNetwork);
        }

        return successes;
    }

    /**
     * Clean up some deleted records from the specified table.
     *
     * If called in an existing transaction, it is the caller's responsibility
     * to ensure that the transaction is already upgraded to a writer, because
     * this method issues a read followed by a write, and thus is potentially
     * vulnerable to an unhandled SQLITE_BUSY failure during the upgrade.
     *
     * If not called in an existing transaction, no new explicit transaction
     * will be begun.
     */
    protected void cleanupSomeDeletedRecords(Uri fromUri, Uri targetUri, String tableName) {
        Log.d(LOGTAG, "Cleaning up deleted records from " + tableName);

        // We clean up records marked as deleted that are older than a
        // predefined max age. It's important not be too greedy here and
        // remove only a few old deleted records at a time.

        // we cleanup records marked as deleted that are older than a
        // predefined max age. It's important not be too greedy here and
        // remove only a few old deleted records at a time.

        // Maximum age of deleted records to be cleaned up (20 days in ms)
        final long MAX_AGE_OF_DELETED_RECORDS = 86400000 * 20;

        // Number of records marked as deleted to be removed
        final long DELETED_RECORDS_PURGE_LIMIT = 5;

        // Android SQLite doesn't have LIMIT on DELETE. Instead, query for the
        // IDs of matching rows, then delete them in one go.
        final long now = System.currentTimeMillis();
        final String selection = SyncColumns.IS_DELETED + " = 1 AND " +
                                 SyncColumns.DATE_MODIFIED + " <= " +
                                 (now - MAX_AGE_OF_DELETED_RECORDS);

        final String profile = fromUri.getQueryParameter(BrowserContract.PARAM_PROFILE);
        final SQLiteDatabase db = getWritableDatabaseForProfile(profile, isTest(fromUri));
        final String[] ids;
        final String limit = Long.toString(DELETED_RECORDS_PURGE_LIMIT, 10);
        final Cursor cursor = db.query(tableName, new String[] { CommonColumns._ID }, selection, null, null, null, null, limit);
        try {
            ids = new String[cursor.getCount()];
            int i = 0;
            while (cursor.moveToNext()) {
                ids[i++] = Long.toString(cursor.getLong(0), 10);
            }
        } finally {
            cursor.close();
        }

        final String inClause = computeSQLInClause(ids.length,
                                                   CommonColumns._ID);
        db.delete(tableName, inClause, ids);
    }

    // Calculate these once, at initialization. isLoggable is too expensive to
    // have in-line in each log call.
    private static boolean logDebug  = Log.isLoggable(LOGTAG, Log.DEBUG);
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
