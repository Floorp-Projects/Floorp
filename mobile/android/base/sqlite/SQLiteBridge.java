/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sqlite;

import org.mozilla.gecko.sqlite.SQLiteBridgeException;
import org.mozilla.gecko.sqlite.MatrixBlobCursor;
import android.content.ContentValues;
import android.database.Cursor;
import android.text.TextUtils;
import android.util.Log;

import java.lang.String;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map.Entry;
import java.util.Set;

/*
 * This class allows using the mozsqlite3 library included with Firefox
 * to read SQLite databases, instead of the Android SQLiteDataBase API,
 * which might use whatever outdated DB is present on the Android system.
 */
public class SQLiteBridge {
    private static final String LOGTAG = "SQLiteBridge";

    // Path to the database. We reopen it every query.
    private String mDb;

    // Values remembered after a query.
    private long[] mQueryResults;

    private static final int RESULT_INSERT_ROW_ID = 0;
    private static final int RESULT_ROWS_CHANGED = 1;

    // JNI code in $(topdir)/mozglue/android/..
    private static native MatrixBlobCursor sqliteCall(String aDb, String aQuery,
                                                      String[] aParams,
                                                      long[] aUpdateResult)
        throws SQLiteBridgeException;

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

    public Cursor rawQuery(String sql, String[] selectionArgs)
        throws SQLiteBridgeException {
        return internalQuery(sql, selectionArgs);
    }

    public long insert(String table, String nullColumnHack, ContentValues values)
               throws SQLiteBridgeException {
        Set<Entry<String, Object>> valueSet = values.valueSet();
        Iterator<Entry<String, Object>> valueIterator = valueSet.iterator();
        ArrayList<String> valueNames = new ArrayList<String>();
        ArrayList<String> valueBinds = new ArrayList<String>();
        ArrayList<String> keyNames = new ArrayList<String>();

        while(valueIterator.hasNext()) {
            Entry<String, Object> value = valueIterator.next();
            keyNames.add(value.getKey());
            valueNames.add("?");
            valueBinds.add(value.getValue().toString());
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
        Set<Entry<String, Object>> valueSet = values.valueSet();
        Iterator<Entry<String, Object>> valueIterator = valueSet.iterator();
        ArrayList<String> valueNames = new ArrayList<String>();

        StringBuilder sb = new StringBuilder("UPDATE ");
        sb.append(table);

        sb.append(" SET ");
        while(valueIterator.hasNext()) {
            Entry<String, Object> value = valueIterator.next();
            sb.append(value.getKey() + " = ?");
            valueNames.add(value.getValue().toString());
            if (valueIterator.hasNext())
                sb.append(", ");
            else
                sb.append(" ");
        }

        if (whereClause != null) {
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
        return sqliteCall(mDb, aQuery, aParams, mQueryResults);
    }

    // nop, provided for API compatibility with SQLiteDatabase.
    public void close() { }
}
