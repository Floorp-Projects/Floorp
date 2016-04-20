/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.annotation.TargetApi;
import android.database.DatabaseUtils;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteStatement;
import android.os.Build;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteOpenHelper;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.Telemetry;

import java.util.Map;

public class DBUtils {
    private static final String LOGTAG = "GeckoDBUtils";

    public static final int SQLITE_MAX_VARIABLE_NUMBER = 999;

    public static final String qualifyColumn(String table, String column) {
        return table + "." + column;
    }

    // This is available in Android >= 11. Implemented locally to be
    // compatible with older versions.
    public static String concatenateWhere(String a, String b) {
        if (TextUtils.isEmpty(a)) {
            return b;
        }

        if (TextUtils.isEmpty(b)) {
            return a;
        }

        return "(" + a + ") AND (" + b + ")";
    }

    // This is available in Android >= 11. Implemented locally to be
    // compatible with older versions.
    public static String[] appendSelectionArgs(String[] originalValues, String[] newValues) {
        if (originalValues == null || originalValues.length == 0) {
            return newValues;
        }

        if (newValues == null || newValues.length == 0) {
            return originalValues;
        }

        String[] result = new String[originalValues.length + newValues.length];
        System.arraycopy(originalValues, 0, result, 0, originalValues.length);
        System.arraycopy(newValues, 0, result, originalValues.length, newValues.length);

        return result;
    }

    /**
     * Concatenate multiple lists of selection arguments. <code>values</code> may be <code>null</code>.
     */
    public static String[] concatenateSelectionArgs(String[]... values) {
        // Since we're most likely to be concatenating a few arrays of many values, it is most
        // efficient to iterate over the arrays once to obtain their lengths, allowing us to create one target array
        // (as opposed to copying arrays on every iteration, which would result in many more copies).
        int totalLength = 0;
        for (String[] v : values) {
            if (v != null) {
                totalLength += v.length;
            }
        }

        String[] result = new String[totalLength];

        int position = 0;
        for (String[] v: values) {
            if (v != null) {
                int currentLength = v.length;
                System.arraycopy(v, 0, result, position, currentLength);
                position += currentLength;
            }
        }

        return result;
    }

    public static void replaceKey(ContentValues aValues, String aOriginalKey,
                                  String aNewKey, String aDefault) {
        String value = aDefault;
        if (aOriginalKey != null && aValues.containsKey(aOriginalKey)) {
            value = aValues.get(aOriginalKey).toString();
            aValues.remove(aOriginalKey);
        }

        if (!aValues.containsKey(aNewKey)) {
            aValues.put(aNewKey, value);
        }
    }

    private static String HISTOGRAM_DATABASE_LOCKED = "DATABASE_LOCKED_EXCEPTION";
    private static String HISTOGRAM_DATABASE_UNLOCKED = "DATABASE_SUCCESSFUL_UNLOCK";
    public static void ensureDatabaseIsNotLocked(SQLiteOpenHelper dbHelper, String databasePath) {
        final int maxAttempts = 5;
        int attempt = 0;
        SQLiteDatabase db = null;
        for (; attempt < maxAttempts; attempt++) {
            try {
                // Try a simple test and exit the loop.
                db = dbHelper.getWritableDatabase();
                break;
            } catch (Exception e) {
                // We assume that this is a android.database.sqlite.SQLiteDatabaseLockedException.
                // That class is only available on API 11+.
                Telemetry.addToHistogram(HISTOGRAM_DATABASE_LOCKED, attempt);

                // Things could get very bad if we don't find a way to unlock the DB.
                Log.d(LOGTAG, "Database is locked, trying to kill any zombie processes: " + databasePath);
                GeckoAppShell.killAnyZombies();
                try {
                    Thread.sleep(attempt * 100);
                } catch (InterruptedException ie) {
                }
            }
        }

        if (db == null) {
            Log.w(LOGTAG, "Failed to unlock database.");
            GeckoAppShell.listOfOpenFiles();
            return;
        }

        // If we needed to retry, but we succeeded, report that in telemetry.
        // Failures are indicated by a lower frequency of UNLOCKED than LOCKED.
        if (attempt > 1) {
            Telemetry.addToHistogram(HISTOGRAM_DATABASE_UNLOCKED, attempt - 1);
        }
    }

    /**
     * Copies a table <b>between</b> database files.
     *
     * This method assumes that the source table and destination table already exist in the
     * source and destination databases, respectively.
     *
     * The table is copied row-by-row in a single transaction.
     *
     * @param source The source database that the table will be copied from.
     * @param sourceTableName The name of the source table.
     * @param destination The destination database that the table will be copied to.
     * @param destinationTableName The name of the destination table.
     * @return true if all rows were copied; false otherwise.
     */
    public static boolean copyTable(SQLiteDatabase source, String sourceTableName,
                                    SQLiteDatabase destination, String destinationTableName) {
        Cursor cursor = null;
        try {
            destination.beginTransaction();

            cursor = source.query(sourceTableName, null, null, null, null, null, null);
            Log.d(LOGTAG, "Trying to copy " + cursor.getCount() + " rows from " + sourceTableName + " to " + destinationTableName);

            final ContentValues contentValues = new ContentValues();
            while (cursor.moveToNext()) {
                contentValues.clear();
                DatabaseUtils.cursorRowToContentValues(cursor, contentValues);
                destination.insert(destinationTableName, null, contentValues);
            }

            destination.setTransactionSuccessful();
            Log.d(LOGTAG, "Successfully copied " + cursor.getCount() + " rows from " + sourceTableName + " to " + destinationTableName);
            return true;
        } catch (Exception e) {
            Log.w(LOGTAG, "Got exception copying rows from " + sourceTableName + " to " + destinationTableName + "; ignoring.", e);
            return false;
        } finally {
            destination.endTransaction();
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    /**
     * Verifies that 0-byte arrays aren't added as favicon or thumbnail data.
     * @param values        ContentValues of query
     * @param columnName    Name of data column to verify
     */
    public static void stripEmptyByteArray(ContentValues values, String columnName) {
        if (values.containsKey(columnName)) {
            byte[] data = values.getAsByteArray(columnName);
            if (data == null || data.length == 0) {
                Log.w(LOGTAG, "Tried to insert an empty or non-byte-array image. Ignoring.");
                values.putNull(columnName);
            }
        }
    }

    /**
     * Builds a selection string that searches for a list of arguments in a particular column.
     * For example URL in (?,?,?). Callers should pass the actual arguments into their query
     * as selection args.
     * @para columnName   The column to search in
     * @para size         The number of arguments to search for
     */
    public static String computeSQLInClause(int items, String field) {
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
    public static String computeSQLInClauseFromLongs(final Cursor cursor, String field) {
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

    public static Uri appendProfile(final String profile, final Uri uri) {
        return uri.buildUpon().appendQueryParameter(BrowserContract.PARAM_PROFILE, profile).build();
    }

    public static Uri appendProfileWithDefault(final String profile, final Uri uri) {
        if (profile == null) {
            return appendProfile(GeckoProfile.DEFAULT_PROFILE, uri);
        }
        return appendProfile(profile, uri);
    }

    /**
     * Use the following when no conflict action is specified.
     */
    private static final int CONFLICT_NONE = 0;
    private static final String[] CONFLICT_VALUES = new String[] {"", " OR ROLLBACK ", " OR ABORT ", " OR FAIL ", " OR IGNORE ", " OR REPLACE "};

    /**
     * Convenience method for updating rows in the database.
     *
     * @param table the table to update in
     * @param values a map from column names to new column values. null is a
     *            valid value that will be translated to NULL.
     * @param whereClause the optional WHERE clause to apply when updating.
     *            Passing null will update all rows.
     * @param whereArgs You may include ?s in the where clause, which
     *            will be replaced by the values from whereArgs. The values
     *            will be bound as Strings.
     * @return the number of rows affected
     */
    @RobocopTarget
    public static int updateArrays(SQLiteDatabase db, String table, ContentValues[] values, UpdateOperation[] ops, String whereClause, String[] whereArgs) {
        return updateArraysWithOnConflict(db, table, values, ops, whereClause, whereArgs, CONFLICT_NONE, true);
    }

    public static void updateArraysBlindly(SQLiteDatabase db, String table, ContentValues[] values, UpdateOperation[] ops, String whereClause, String[] whereArgs) {
        updateArraysWithOnConflict(db, table, values, ops, whereClause, whereArgs, CONFLICT_NONE, false);
    }

    @RobocopTarget
    public enum UpdateOperation {
        /**
         * ASSIGN is the usual update: replaces the value in the named column with the provided value.
         *
         * foo = ?
         */
        ASSIGN,

        /**
         * BITWISE_OR applies the provided value to the existing value with a bitwise OR. This is useful for adding to flags.
         *
         * foo |= ?
         */
        BITWISE_OR,

        /**
         * EXPRESSION is an end-run around the API: it allows callers to specify a fragment of SQL to splice into the
         * SET part of the query.
         *
         * foo = $value
         *
         * Be very careful not to use user input in this.
         */
        EXPRESSION,
    }

    /**
     * This is an evil reimplementation of SQLiteDatabase's methods to allow for
     * smarter updating.
     *
     * Each ContentValues has an associated enum that describes how to unify input values with the existing column values.
     */
    private static int updateArraysWithOnConflict(SQLiteDatabase db, String table,
                                          ContentValues[] values,
                                          UpdateOperation[] ops,
                                          String whereClause,
                                          String[] whereArgs,
                                          int conflictAlgorithm,
                                          boolean returnChangedRows) {
        if (values == null || values.length == 0) {
            throw new IllegalArgumentException("Empty values");
        }

        if (ops == null || ops.length != values.length) {
            throw new IllegalArgumentException("ops and values don't match");
        }

        StringBuilder sql = new StringBuilder(120);
        sql.append("UPDATE ");
        sql.append(CONFLICT_VALUES[conflictAlgorithm]);
        sql.append(table);
        sql.append(" SET ");

        // move all bind args to one array
        int setValuesSize = 0;
        for (int i = 0; i < values.length; i++) {
            // EXPRESSION types don't contribute any placeholders.
            if (ops[i] != UpdateOperation.EXPRESSION) {
                setValuesSize += values[i].size();
            }
        }

        int bindArgsSize = (whereArgs == null) ? setValuesSize : (setValuesSize + whereArgs.length);
        Object[] bindArgs = new Object[bindArgsSize];

        int arg = 0;
        for (int i = 0; i < values.length; i++) {
            final ContentValues v = values[i];
            final UpdateOperation op = ops[i];

            // Alas, code duplication.
            switch (op) {
                case ASSIGN:
                    for (Map.Entry<String, Object> entry : v.valueSet()) {
                        final String colName = entry.getKey();
                        sql.append((arg > 0) ? "," : "");
                        sql.append(colName);
                        bindArgs[arg++] = entry.getValue();
                        sql.append("= ?");
                    }
                    break;
                case BITWISE_OR:
                    for (Map.Entry<String, Object> entry : v.valueSet()) {
                        final String colName = entry.getKey();
                        sql.append((arg > 0) ? "," : "");
                        sql.append(colName);
                        bindArgs[arg++] = entry.getValue();
                        sql.append("= ? | ");
                        sql.append(colName);
                    }
                    break;
                case EXPRESSION:
                    // Treat each value as a literal SQL string.
                    for (Map.Entry<String, Object> entry : v.valueSet()) {
                        final String colName = entry.getKey();
                        sql.append((arg > 0) ? "," : "");
                        sql.append(colName);
                        sql.append(" = ");
                        sql.append(entry.getValue());
                    }
                    break;
            }
        }

        if (whereArgs != null) {
            for (arg = setValuesSize; arg < bindArgsSize; arg++) {
                bindArgs[arg] = whereArgs[arg - setValuesSize];
            }
        }
        if (!TextUtils.isEmpty(whereClause)) {
            sql.append(" WHERE ");
            sql.append(whereClause);
        }

        // What a huge pain in the ass, all because SQLiteDatabase doesn't expose .executeSql,
        // and we can't get a DB handle. Nor can we easily construct a statement with arguments
        // already bound.
        final SQLiteStatement statement = db.compileStatement(sql.toString());
        try {
            bindAllArgs(statement, bindArgs);
            if (!returnChangedRows) {
                statement.execute();
                return 0;
            }

            if (AppConstants.Versions.feature11Plus) {
                // This is a separate method so we can annotate it with @TargetApi.
                return executeStatementReturningChangedRows(statement);
            } else {
                statement.execute();
                final Cursor cursor = db.rawQuery("SELECT changes()", null);
                try {
                    cursor.moveToFirst();
                    return cursor.getInt(0);
                } finally {
                    cursor.close();
                }

            }
        } finally {
            statement.close();
        }
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    private static int executeStatementReturningChangedRows(SQLiteStatement statement) {
        return statement.executeUpdateDelete();
    }

    // All because {@link SQLiteProgram#bind(integer, Object)} is private.
    private static void bindAllArgs(SQLiteStatement statement, Object[] bindArgs) {
        if (bindArgs == null) {
            return;
        }
        for (int i = bindArgs.length; i != 0; i--) {
            Object v = bindArgs[i - 1];
            if (v == null) {
                statement.bindNull(i);
            } else if (v instanceof String) {
                statement.bindString(i, (String) v);
            } else if (v instanceof Double) {
                statement.bindDouble(i, (Double) v);
            } else if (v instanceof Float) {
                statement.bindDouble(i, (Float) v);
            } else if (v instanceof Long) {
                statement.bindLong(i, (Long) v);
            } else if (v instanceof Integer) {
                statement.bindLong(i, (Integer) v);
            } else if (v instanceof Byte) {
                statement.bindLong(i, (Byte) v);
            } else if (v instanceof byte[]) {
                statement.bindBlob(i, (byte[]) v);
            }
        }
    }
}
