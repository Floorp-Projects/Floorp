/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sqlite;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteException;
import android.text.TextUtils;
import android.util.Log;

import java.util.ArrayList;
import java.util.Map.Entry;

/*
 * This class allows using the mozsqlite3 library included with Firefox
 * to read SQLite databases, instead of the Android SQLiteDataBase API,
 * which might use whatever outdated DB is present on the Android system.
 */
public class SQLiteBridge {
    private static final String LOGTAG = "SQLiteBridge";

    // Path to the database. If this database was not opened with openDatabase, we reopen it every query.
    private String mDb;
    // pointer to the database if it was opened with openDatabase
    protected long mDbPointer = 0;

    // Values remembered after a query.
    private long[] mQueryResults;

    private boolean mTransactionSuccess = false;
    private boolean mInTransaction = false;

    private static final int RESULT_INSERT_ROW_ID = 0;
    private static final int RESULT_ROWS_CHANGED = 1;

    // JNI code in $(topdir)/mozglue/android/..
    private static native MatrixBlobCursor sqliteCall(String aDb, String aQuery,
                                                      String[] aParams,
                                                      long[] aUpdateResult)
        throws SQLiteBridgeException;
    private static native MatrixBlobCursor sqliteCallWithDb(long aDb, String aQuery,
                                                            String[] aParams,
                                                            long[] aUpdateResult)
        throws SQLiteBridgeException;
    private static native long openDatabase(String aDb)
        throws SQLiteBridgeException;
    private static native void closeDatabase(long aDb);

    // Takes the path to the database we want to access.
    public SQLiteBridge(String aDb) throws SQLiteBridgeException {
        mDb = aDb;
    }

    // Executes a simple line of sql.
    public void execSQL(String sql)
                throws SQLiteBridgeException {
        internalQuery(sql, null);
    }

    // Executes a simple line of sql. Allow you to bind arguments
    public void execSQL(String sql, String[] bindArgs)
                throws SQLiteBridgeException {
        internalQuery(sql, bindArgs);
    }

    // Executes a DELETE statement on the database
    public int delete(String table, String whereClause, String[] whereArgs)
               throws SQLiteBridgeException {
        StringBuilder sb = new StringBuilder("DELETE from ");
        sb.append(table);
        if (whereClause != null) {
            sb.append(" WHERE " + whereClause);
        }

        internalQuery(sb.toString(), whereArgs);
        return (int)mQueryResults[RESULT_ROWS_CHANGED];
    }

    public Cursor query(String table,
                        String[] columns,
                        String selection,
                        String[] selectionArgs,
                        String groupBy,
                        String having,
                        String orderBy,
                        String limit)
               throws SQLiteBridgeException {
        StringBuilder sb = new StringBuilder("SELECT ");
        if (columns != null)
            sb.append(TextUtils.join(", ", columns));
        else
            sb.append(" * ");

        sb.append(" FROM ");
        sb.append(table);

        if (selection != null) {
            sb.append(" WHERE " + selection);
        }

        if (groupBy != null) {
            sb.append(" GROUP BY " + groupBy);
        }

        if (having != null) {
            sb.append(" HAVING " + having);
        }

        if (orderBy != null) {
            sb.append(" ORDER BY " + orderBy);
        }

        if (limit != null) {
            sb.append(" " + limit);
        }

        return rawQuery(sb.toString(), selectionArgs);
    }

    /* This method is referenced by Robocop via reflection. */
    public Cursor rawQuery(String sql, String[] selectionArgs)
        throws SQLiteBridgeException {
        return internalQuery(sql, selectionArgs);
    }

    public long insert(String table, String nullColumnHack, ContentValues values)
               throws SQLiteBridgeException {
        if (values == null)
            return 0;

        ArrayList<String> valueNames = new ArrayList<String>();
        ArrayList<String> valueBinds = new ArrayList<String>();
        ArrayList<String> keyNames = new ArrayList<String>();

        for (Entry<String, Object> value : values.valueSet()) {
            keyNames.add(value.getKey());

            Object val = value.getValue();
            if (val == null) {
                valueNames.add("NULL");
            } else {
                valueNames.add("?");
                valueBinds.add(val.toString());
            }
        }

        StringBuilder sb = new StringBuilder("INSERT into ");
        sb.append(table);

        sb.append(" (");
        sb.append(TextUtils.join(", ", keyNames));
        sb.append(")");

        // XXX - Do we need to bind these values?
        sb.append(" VALUES (");
        sb.append(TextUtils.join(", ", valueNames));
        sb.append(") ");

        String[] binds = new String[valueBinds.size()];
        valueBinds.toArray(binds);
        internalQuery(sb.toString(), binds);
        return mQueryResults[RESULT_INSERT_ROW_ID];
    }

    public int update(String table, ContentValues values, String whereClause, String[] whereArgs)
               throws SQLiteBridgeException {
        if (values == null)
            return 0;

        ArrayList<String> valueNames = new ArrayList<String>();

        StringBuilder sb = new StringBuilder("UPDATE ");
        sb.append(table);
        sb.append(" SET ");

        boolean isFirst = true;

        for (Entry<String, Object> value : values.valueSet()) {
            if (isFirst)
                isFirst = false;
            else
                sb.append(", ");

            sb.append(value.getKey());

            Object val = value.getValue();
            if (val == null) {
                sb.append(" = NULL");
            } else {
                sb.append(" = ?");
                valueNames.add(val.toString());
            }
        }

        if (!TextUtils.isEmpty(whereClause)) {
            sb.append(" WHERE ");
            sb.append(whereClause);
            for (int i = 0; i < whereArgs.length; i++) {
                valueNames.add(whereArgs[i]);
            }
        }

        String[] binds = new String[valueNames.size()];
        valueNames.toArray(binds);

        internalQuery(sb.toString(), binds);
        return (int)mQueryResults[RESULT_ROWS_CHANGED];
    }

    public int getVersion()
               throws SQLiteBridgeException {
        Cursor cursor = internalQuery("PRAGMA user_version", null);
        int ret = -1;
        if (cursor != null) {
            cursor.moveToFirst();
            String version = cursor.getString(0);
            ret = Integer.parseInt(version);
        }
        return ret;
    }

    // Do an SQL query, substituting the parameters in the query with the passed
    // parameters. The parameters are subsituded in order, so named parameters
    // are not supported.
    private Cursor internalQuery(String aQuery, String[] aParams)
        throws SQLiteBridgeException {

        mQueryResults = new long[2];
        if (isOpen()) {
            return sqliteCallWithDb(mDbPointer, aQuery, aParams, mQueryResults);
        }
        return sqliteCall(mDb, aQuery, aParams, mQueryResults);
    }

    /*
     * The second two parameters here are just provided for compatbility with SQLiteDatabase
     * Support for them is not currently implemented
    */
    public static SQLiteBridge openDatabase(String path, SQLiteDatabase.CursorFactory factory, int flags)
        throws SQLiteException {
        SQLiteBridge bridge = null;
        try {
            bridge = new SQLiteBridge(path);
            bridge.mDbPointer = SQLiteBridge.openDatabase(path);
        } catch(SQLiteBridgeException ex) {
            // catch and rethrow as a SQLiteException to match SQLiteDatabase
            throw new SQLiteException(ex.getMessage());
        }
        return bridge;
    }

    public void close() {
        if (isOpen()) {
          closeDatabase(mDbPointer);
        }
        mDbPointer = 0;
    }

    public boolean isOpen() {
        return mDbPointer > 0;
    }

    public void beginTransaction() throws SQLiteBridgeException {
        if (inTransaction()) {
            throw new SQLiteBridgeException("Nested transactions are not supported");
        }
        execSQL("BEGIN EXCLUSIVE");
        mTransactionSuccess = false;
        mInTransaction = true;
    }

    public void beginTransactionNonExclusive() throws SQLiteBridgeException {
        if (inTransaction()) {
            throw new SQLiteBridgeException("Nested transactions are not supported");
        }
        execSQL("BEGIN IMMEDIATE");
        mTransactionSuccess = false;
        mInTransaction = true;
    }

    public void endTransaction() {
        if (!inTransaction())
            return;

        try {
          if (mTransactionSuccess) {
              execSQL("COMMIT TRANSACTION");
          } else {
              execSQL("ROLLBACK TRANSACTION");
          }
        } catch(SQLiteBridgeException ex) {
            Log.e(LOGTAG, "Error ending transaction", ex);
        }
        mInTransaction = false;
        mTransactionSuccess = false;
    }

    public void setTransactionSuccessful() throws SQLiteBridgeException {
        if (!inTransaction()) {
            throw new SQLiteBridgeException("setTransactionSuccessful called outside a transaction");
        }
        mTransactionSuccess = true;
    }

    public boolean inTransaction() {
        return mInTransaction;
    }

    @Override
    public void finalize() {
        if (isOpen()) {
            Log.e(LOGTAG, "Bridge finalized without closing the database");
            close();
        }
    }
}
