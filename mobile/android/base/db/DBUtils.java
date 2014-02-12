/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.GeckoAppShell;

import android.content.ContentValues;
import android.database.sqlite.SQLiteOpenHelper;
import android.text.TextUtils;
import android.util.Log;

public class DBUtils {
    private static final String LOGTAG = "GeckoDBUtils";

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

    public static void ensureDatabaseIsNotLocked(SQLiteOpenHelper dbHelper, String databasePath) {
        for (int retries = 0; retries < 5; retries++) {
            try {
                // Try a simple test and exit the loop
                dbHelper.getWritableDatabase();
                return;
            } catch (Exception e) {
                // Things could get very bad if we don't find a way to unlock the DB
                Log.d(LOGTAG, "Database is locked, trying to kill any zombie processes: " + databasePath);
                GeckoAppShell.killAnyZombies();
                try {
                    Thread.sleep(retries * 100);
                } catch (InterruptedException ie) { }
            }
        }
        Log.d(LOGTAG, "Failed to unlock database");
        GeckoAppShell.listOfOpenFiles();
    }
}
