/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.autocomplete;

import android.app.Activity;
import android.content.res.Resources;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.database.sqlite.SQLiteStatement;
import android.util.Log;
import android.widget.Toast;

import org.mozilla.search.Constants;
import org.mozilla.search.R;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

/**
 * Helper to search a word dictionary.
 * From: https://developer.android.com/training/search/search.html
 */
class AutoCompleteWordListAgent {

    public static final String COL_WORD = "WORD";
    private static final String TAG = "DictionaryDatabase";
    private static final String DATABASE_NAME = "DICTIONARY";
    private static final String FTS_VIRTUAL_TABLE = "FTS";
    private static final int DATABASE_VERSION = 1;

    private final DatabaseOpenHelper databaseOpenHelper;

    public AutoCompleteWordListAgent(Activity activity) {
        databaseOpenHelper = new DatabaseOpenHelper(activity);
        // DB helper uses lazy initialization, so this forces the db helper to start indexing the
        // wordlist
        databaseOpenHelper.getReadableDatabase();
    }

    public Cursor getWordMatches(String query) {
        String selection = COL_WORD + " MATCH ?";
        String[] selectionArgs = new String[]{query + "*"};
        return query(selection, selectionArgs);
    }

    private Cursor query(String selection, String[] selectionArgs) {
        SQLiteQueryBuilder builder = new SQLiteQueryBuilder();
        builder.setTables(FTS_VIRTUAL_TABLE);

        Cursor cursor = builder.query(databaseOpenHelper.getReadableDatabase(), null, selection,
                selectionArgs, null, null, null, Constants.AUTOCOMPLETE_ROW_LIMIT);

        if (cursor == null) {
            return null;
        } else if (!cursor.moveToFirst()) {
            cursor.close();
            return null;
        }
        return cursor;
    }

    private static class DatabaseOpenHelper extends SQLiteOpenHelper {

        private final Activity activity;

        private SQLiteDatabase database;

        private static final String FTS_TABLE_CREATE =
                "CREATE VIRTUAL TABLE " + FTS_VIRTUAL_TABLE + " USING fts3 (" + COL_WORD + ")";

        DatabaseOpenHelper(Activity activity) {
            super(activity, DATABASE_NAME, null, DATABASE_VERSION);
            this.activity = activity;
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            database = db;
            database.execSQL(FTS_TABLE_CREATE);

            loadDictionary();
        }

        private void loadDictionary() {
            new Thread(new Runnable() {
                public void run() {
                    try {

                        activity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                Toast.makeText(activity, "Starting post-install indexing",
                                        Toast.LENGTH_SHORT).show();
                                Toast.makeText(activity,
                                        "Don't worry; Mark & Ian we'll figure out a way around " +
                                                "this :)", Toast.LENGTH_SHORT
                                ).show();
                            }
                        });
                        loadWords();
                        activity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                Toast.makeText(activity, "All done!", Toast.LENGTH_SHORT).show();
                            }
                        });
                    } catch (IOException e) {
                        throw new RuntimeException(e);
                    }
                }
            }).start();
        }

        private void loadWords() throws IOException {
            final Resources resources = activity.getResources();
            InputStream inputStream = resources.openRawResource(R.raw.en_us);
            BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream));


            String sql = "INSERT INTO " + FTS_VIRTUAL_TABLE + " VALUES (?);";
            SQLiteStatement statement = database.compileStatement(sql);
            database.beginTransaction();

            try {
                String line;
                while (null != (line = reader.readLine())) {
                    statement.clearBindings();
                    statement.bindString(1, line.trim());
                    statement.execute();
                }
            } finally {
                database.setTransactionSuccessful();
                database.endTransaction();
            }
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            Log.w(TAG, "Upgrading database from version " + oldVersion + " to " + newVersion +
                    ", which will destroy all old data");
            db.execSQL("DROP TABLE IF EXISTS " + FTS_VIRTUAL_TABLE);
            onCreate(db);
        }


    }
}