/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.sqlite;

import org.mozilla.gecko.sqlite.SQLiteBridgeException;
import android.util.Log;

import java.lang.String;
import java.util.ArrayList;
import java.util.HashMap;

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

    public void close() {
        // nop, provided for API compatibility with SQLiteDatabase.
    }
}