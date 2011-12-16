/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sqlite;

import org.mozilla.gecko.sqlite.SQLiteBridgeException;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
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
    // Remember column names from last query result.
    private ArrayList<String> mColumns;

    // JNI code in $(topdir)/mozglue/android/..
    private static native void sqliteCall(String aDb, String aQuery,
                                          String[] aParams,
                                          ArrayList<String> aColumns,
                                          ArrayList<Object[]> aRes)
        throws SQLiteBridgeException;

    // Takes the path to the database we want to access.
    public SQLiteBridge(String aDb) throws SQLiteBridgeException {
        mDb = aDb;
    }

    // Executes a simple line of sql.
    public void execSQL(String sql)
                throws SQLiteBridgeException {
        try {
            query(sql, null);
        } catch(SQLiteBridgeException ex) {
            throw ex;
        }
    }

    // Executes a simple line of sql. Allow you to bind arguments
    public void execSQL(String sql, String[] bindArgs)
                throws SQLiteBridgeException {
        try {
            query(sql, bindArgs);
        } catch(SQLiteBridgeException ex) {
            throw ex;
        }
    }

    // Executes a DELETE statement on the database
    public int delete(String table, String whereClause, String[] whereArgs)
               throws SQLiteBridgeException {
        StringBuilder sb = new StringBuilder("DELETE from ");
        sb.append(table);
        if (whereClause != null) {
            sb.append(" WHERE " + whereClause);
        }

        try {
            return getIntResult(sb.toString(), whereArgs, 1);
        } catch(SQLiteBridgeException ex) {
            throw ex;
        }
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

        ArrayList<Object[]> results;
        try {
            mColumns = null;

            results = query(sb.toString(), selectionArgs);

        } catch(SQLiteBridgeException ex) {
            throw ex;
        }

        MatrixCursor cursor = new MatrixCursor(mColumns.toArray(new String[0]));
        try {
            for (Object resultRow: results) {
                Object[] resultColumns = (Object[])resultRow;
                if (resultColumns.length == mColumns.size())
                    cursor.addRow(resultColumns);
            }
        } catch(IllegalArgumentException ex) {
            Log.e(LOGTAG, "Error getting rows", ex);
        }

        return cursor;
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
        try {
            return getIntResult(sb.toString(), binds, 0);
        } catch (SQLiteBridgeException ex) {
            throw ex;
        }
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
        try {
            return getIntResult(sb.toString(), binds, 1);
        } catch (SQLiteBridgeException ex) {
            throw ex;
        }
    }

    private int getIntResult(String query, String[] params, int resultIndex)
               throws SQLiteBridgeException {
        ArrayList<Object[]> results = null;
        try {
            mColumns = null;
            results = query(query, params);
        } catch(SQLiteBridgeException ex) {
            throw ex;
        }

        if (results != null) {
            for (Object resultRow: results) {
                Object[] resultColumns = (Object[])resultRow;
                return ((Number)resultColumns[resultIndex]).intValue();
            }
        }
        return -1;
    }

    public int getVersion()
               throws SQLiteBridgeException {
        ArrayList<Object[]> results = null;
        try {
            mColumns = null;
            results = query("PRAGMA user_version");
        } catch(SQLiteBridgeException ex) {
            throw ex;
        }
        int ret = -1;
        if (results != null) {
            for (Object resultRow: results) {
                Object[] resultColumns = (Object[])resultRow;
                String version = (String)resultColumns[0];
                ret = Integer.parseInt(version);
            }
        }
        return ret;
    }


    // Do an SQL query without parameters
    public ArrayList<Object[]> query(String aQuery) throws SQLiteBridgeException {
        String[] params = new String[0];
        return query(aQuery, params);
    }

    // Do an SQL query, substituting the parameters in the query with the passed
    // parameters. The parameters are subsituded in order, so named parameters
    // are not supported.
    // The result is returned as an ArrayList<Object[]>, with each
    // row being an entry in the ArrayList, and each column being one Object
    // in the Object[] array. The columns are of type null,
    // direct ByteBuffer (BLOB), or String (everything else).
    public ArrayList<Object[]> query(String aQuery, String[] aParams)
        throws SQLiteBridgeException {
        ArrayList<Object[]> result = new ArrayList<Object[]>();
        mColumns = new ArrayList<String>();
        sqliteCall(mDb, aQuery, aParams, mColumns, result);
        return result;
    }

    // Gets the index in the row Object[] for the given column name.
    // Returns -1 if not found.
    public int getColumnIndex(String aColumnName) {
        return mColumns.lastIndexOf(aColumnName);
    }

    // nop, provided for API compatibility with SQLiteDatabase.
    public void close() { }
}