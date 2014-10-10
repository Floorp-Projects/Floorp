/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.AppConstants.Versions;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.SQLException;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

/**
 * This abstract class exists to capture some of the transaction-handling
 * commonalities in Fennec's DB layer.
 *
 * In particular, this abstracts DB access, batching, and a particular
 * transaction approach.
 *
 * That approach is: subclasses implement the abstract methods
 * {@link #insertInTransaction(android.net.Uri, android.content.ContentValues)},
 * {@link #deleteInTransaction(android.net.Uri, String, String[])}, and
 * {@link #updateInTransaction(android.net.Uri, android.content.ContentValues, String, String[])}.
 *
 * These are all called expecting a transaction to be established, so failed
 * modifications can be rolled-back, and work batched.
 *
 * If no transaction is established, that's not a problem. Transaction nesting
 * can be avoided by using {@link #beginWrite(SQLiteDatabase)}.
 *
 * The decision of when to begin a transaction is left to the subclasses,
 * primarily to avoid the pattern of a transaction being begun, a read occurring,
 * and then a write being necessary. This lock upgrade can result in SQLITE_BUSY,
 * which we don't handle well. Better to avoid starting a transaction too soon!
 *
 * You are probably interested in some subclasses:
 *
 * * {@link AbstractPerProfileDatabaseProvider} provides a simple abstraction for
 *   querying databases that are stored in the user's profile directory.
 * * {@link PerProfileDatabaseProvider} is a simple version that only allows a
 *   single ContentProvider to access each per-profile database.
 * * {@link SharedBrowserDatabaseProvider} is an example of a per-profile provider
 *   that allows for multiple providers to safely work with the same databases.
 */
@SuppressWarnings("javadoc")
public abstract class AbstractTransactionalProvider extends ContentProvider {
    private static final String LOGTAG = "GeckoTransProvider";

    private static final boolean logDebug = Log.isLoggable(LOGTAG, Log.DEBUG);
    private static final boolean logVerbose = Log.isLoggable(LOGTAG, Log.VERBOSE);

    protected abstract SQLiteDatabase getReadableDatabase(Uri uri);
    protected abstract SQLiteDatabase getWritableDatabase(Uri uri);

    public abstract SQLiteDatabase getWritableDatabaseForTesting(Uri uri);

    protected abstract Uri insertInTransaction(Uri uri, ContentValues values);
    protected abstract int deleteInTransaction(Uri uri, String selection, String[] selectionArgs);
    protected abstract int updateInTransaction(Uri uri, ContentValues values, String selection, String[] selectionArgs);

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

    /**
     * Return true if OS version and database parallelism support indicates
     * that this provider should bundle writes into transactions.
     */
    @SuppressWarnings("static-method")
    protected boolean shouldUseTransactions() {
        return Versions.feature11Plus;
    }

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
     * @see PerProfileDatabaseProvider#markWriteSuccessful(SQLiteDatabase)
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
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
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
            getContext().getContentResolver().notifyChange(uri, null, shouldSyncToNetwork);
        }

        return successes;
    }

    /**
     * Indicates whether a query should include deleted fields
     * based on the URI.
     * @param uri query URI
     */
    protected static boolean shouldShowDeleted(Uri uri) {
        String showDeleted = uri.getQueryParameter(BrowserContract.PARAM_SHOW_DELETED);
        return !TextUtils.isEmpty(showDeleted);
    }

    /**
     * Indicates whether an insertion should be made if a record doesn't
     * exist, based on the URI.
     * @param uri query URI
     */
    protected static boolean shouldUpdateOrInsert(Uri uri) {
        String insertIfNeeded = uri.getQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED);
        return Boolean.parseBoolean(insertIfNeeded);
    }

    /**
     * Indicates whether query is a test based on the URI.
     * @param uri query URI
     */
    protected static boolean isTest(Uri uri) {
        if (uri == null) {
            return false;
        }
        String isTest = uri.getQueryParameter(BrowserContract.PARAM_IS_TEST);
        return !TextUtils.isEmpty(isTest);
    }

    /**
     * Return true of the query is from Firefox Sync.
     * @param uri query URI
     */
    protected static boolean isCallerSync(Uri uri) {
        String isSync = uri.getQueryParameter(BrowserContract.PARAM_IS_SYNC);
        return !TextUtils.isEmpty(isSync);
    }

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