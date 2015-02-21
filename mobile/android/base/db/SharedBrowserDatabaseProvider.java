/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.db.BrowserContract.CommonColumns;
import org.mozilla.gecko.db.BrowserContract.SyncColumns;
import org.mozilla.gecko.db.PerProfileDatabases.DatabaseHelperFactory;

import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;
import android.util.Log;

/**
 * A ContentProvider subclass that provides per-profile browser.db access
 * that can be safely shared between multiple providers.
 *
 * If multiple ContentProvider classes wish to share a database, it's
 * vitally important that they use the same SQLiteOpenHelpers for access.
 *
 * Failure to do so can cause accidental concurrent writes, with the result
 * being unexpected SQLITE_BUSY errors.
 *
 * This class provides a static {@link PerProfileDatabases} instance, lazily
 * initialized within {@link SharedBrowserDatabaseProvider#onCreate()}.
 */
public abstract class SharedBrowserDatabaseProvider extends AbstractPerProfileDatabaseProvider {
    private static final String LOGTAG = SharedBrowserDatabaseProvider.class.getSimpleName();

    private static PerProfileDatabases<BrowserDatabaseHelper> databases;

    @Override
    protected PerProfileDatabases<BrowserDatabaseHelper> getDatabases() {
        return databases;
    }

    // Can't mark as @Override. Added in API 11.
    public void shutdown() {
        synchronized (SharedBrowserDatabaseProvider.class) {
            databases.shutdown();
            databases = null;
        }
    }

    @Override
    public boolean onCreate() {
        // If necessary, do the shared DB work.
        synchronized (SharedBrowserDatabaseProvider.class) {
            if (databases != null) {
                return true;
            }

            final DatabaseHelperFactory<BrowserDatabaseHelper> helperFactory = new DatabaseHelperFactory<BrowserDatabaseHelper>() {
                @Override
                public BrowserDatabaseHelper makeDatabaseHelper(Context context, String databasePath) {
                    final BrowserDatabaseHelper helper = new BrowserDatabaseHelper(context, databasePath);
                    if (Versions.feature16Plus) {
                        helper.setWriteAheadLoggingEnabled(true);
                    }
                    return helper;
                }
            };

            databases = new PerProfileDatabases<BrowserDatabaseHelper>(getContext(), BrowserDatabaseHelper.DATABASE_NAME, helperFactory);
        }

        return true;
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
    protected void cleanUpSomeDeletedRecords(Uri fromUri, String tableName) {
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
        final String limit = Long.toString(DELETED_RECORDS_PURGE_LIMIT, 10);
        final Cursor cursor = db.query(tableName, new String[] { CommonColumns._ID }, selection, null, null, null, null, limit);
        final String inClause;
        try {
            inClause = DBUtils.computeSQLInClauseFromLongs(cursor, CommonColumns._ID);
        } finally {
            cursor.close();
        }

        db.delete(tableName, inClause, null);
    }
}
