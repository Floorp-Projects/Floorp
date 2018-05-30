package com.leanplum.internal;

/*
 * Copyright 2017, Leanplum, Inc. All rights reserved.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

import android.content.ContentValues;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;

import android.database.DatabaseUtils;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

import com.leanplum.Leanplum;
import com.leanplum.utils.SharedPreferencesUtil;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.UUID;

/**
 * LeanplumEventDataManager class to work with SQLite.
 *
 * @author Anna Orlova
 */
public class LeanplumEventDataManager {
    private static final String DATABASE_NAME = "__leanplum.db";
    private static final int DATABASE_VERSION = 1;
    private static final String EVENT_TABLE_NAME = "event";
    private static final String COLUMN_DATA = "data";
    private static final String KEY_ROWID = "rowid";

    private static SQLiteDatabase database;
    private static LeanplumDataBaseManager databaseManager;
    private static ContentValues contentValues = new ContentValues();

    static boolean willSendErrorLog = false;

    /**
     * Creates connection to database, if database is not present, it will automatically create it.
     *
     * @param context Current context.
     */
    public static void init(Context context) {
        if (database != null) {
            Log.e("Database is already initialized.");
            return;
        }

        // Create database if needed.
        try {
            if (databaseManager == null) {
                databaseManager = new LeanplumDataBaseManager(context);
            }
            database = databaseManager.getWritableDatabase();
        } catch (Throwable t) {
            handleSQLiteError("Cannot create database.", t);
        }
    }

    /**
     * Inserts event to event table.
     *
     * @param event String with json of event.
     */
    static void insertEvent(String event) {
        if (database == null) {
            return;
        }
        contentValues.put(COLUMN_DATA, event);
        try {
            database.insert(EVENT_TABLE_NAME, null, contentValues);
            willSendErrorLog = false;
        } catch (Throwable t) {
            handleSQLiteError("Unable to insert event to database.", t);
        }
        contentValues.clear();
    }

    /**
     * Gets first count events from event table.
     *
     * @param count Number of events.
     * @return List of events.
     */
    static List<Map<String, Object>> getEvents(int count) {
        List<Map<String, Object>> events = new ArrayList<>();
        if (database == null) {
            return events;
        }
        Cursor cursor = null;
        try {
            cursor = database.query(EVENT_TABLE_NAME, new String[]{COLUMN_DATA}, null, null, null,
                    null, KEY_ROWID + " ASC", "" + count);
            willSendErrorLog = false;
            while (cursor.moveToNext()) {
                Map<String, Object> requestArgs = JsonConverter.mapFromJson(new JSONObject(
                        cursor.getString(cursor.getColumnIndex(COLUMN_DATA))));
                events.add(requestArgs);
            }
        } catch (Throwable t) {
            handleSQLiteError("Unable to get events from the table.", t);
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
        return events;
    }

    /**
     * Deletes first count elements from event table.
     *
     * @param count Number of event that need to be deleted.
     */
    static void deleteEvents(int count) {
        if (database == null) {
            return;
        }
        try {
            database.delete(EVENT_TABLE_NAME, KEY_ROWID + " in (select " + KEY_ROWID + " from " +
                    EVENT_TABLE_NAME + " ORDER BY " + KEY_ROWID + " ASC LIMIT " + count + ")", null);
            willSendErrorLog = false;
        } catch (Throwable t) {
            handleSQLiteError("Unable to delete events from the table.", t);
        }
    }

    /**
     * Gets number of rows in the event table.
     *
     * @return Number of rows in the event table.
     */
    static long getEventsCount() {
        long count = 0;
        if (database == null) {
            return count;
        }
        try {
            count = DatabaseUtils.queryNumEntries(database, EVENT_TABLE_NAME);
            willSendErrorLog = false;
        } catch (Throwable t) {
            handleSQLiteError("Unable to get a number of rows in the table.", t);
        }
        return count;
    }

    /**
     * Helper function that logs and sends errors to the server.
     */
    private static void handleSQLiteError(String log, Throwable t) {
        Log.e(log, t);
        // Send error log. Using willSendErrorLog to prevent infinte loop.
        if (!willSendErrorLog) {
            willSendErrorLog = true;
            Util.handleException(t);
        }
    }

    private static class LeanplumDataBaseManager extends SQLiteOpenHelper {
        LeanplumDataBaseManager(Context context) {
            super(context, DATABASE_NAME, null, DATABASE_VERSION);
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            // Create event table.
            db.execSQL("CREATE TABLE IF NOT EXISTS " + EVENT_TABLE_NAME + "(" + COLUMN_DATA +
                    " TEXT)");

            // Migrate old data from shared preferences.
            try {
                migrateFromSharedPreferences(db);
            } catch (Throwable t) {
                Log.e("Cannot move old data from shared preferences to SQLite table.", t);
                Util.handleException(t);
            }
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            // No used for now.
        }

        /**
         * Migrate data from shared preferences to SQLite.
         */
        private static void migrateFromSharedPreferences(SQLiteDatabase db) {
            synchronized (Request.class) {
                Context context = Leanplum.getContext();
                SharedPreferences preferences = context.getSharedPreferences(
                        Request.LEANPLUM, Context.MODE_PRIVATE);
                SharedPreferences.Editor editor = preferences.edit();
                int count = preferences.getInt(Constants.Defaults.COUNT_KEY, 0);
                if (count == 0) {
                    return;
                }

                List<Map<String, Object>> requestData = new ArrayList<>();
                for (int i = 0; i < count; i++) {
                    String itemKey = String.format(Locale.US, Constants.Defaults.ITEM_KEY, i);
                    Map<String, Object> requestArgs;
                    try {
                        requestArgs = JsonConverter.mapFromJson(new JSONObject(
                                preferences.getString(itemKey, "{}")));
                        requestData.add(requestArgs);
                    } catch (JSONException e) {
                        e.printStackTrace();
                    }
                    editor.remove(itemKey);
                }

                editor.remove(Constants.Defaults.COUNT_KEY);

                try {
                    String uuid = preferences.getString(Constants.Defaults.UUID_KEY, null);
                    if (uuid == null || count % Request.MAX_EVENTS_PER_API_CALL == 0) {
                        uuid = UUID.randomUUID().toString();
                        editor.putString(Constants.Defaults.UUID_KEY, uuid);
                    }
                    for (Map<String, Object> event : requestData) {
                        event.put(Request.UUID_KEY, uuid);
                        contentValues.put(COLUMN_DATA, JsonConverter.toJson(event));
                        db.insert(EVENT_TABLE_NAME, null, contentValues);
                        contentValues.clear();
                    }
                    SharedPreferencesUtil.commitChanges(editor);
                } catch (Throwable t) {
                    Log.e("Failed on migration data from shared preferences.", t);
                    Util.handleException(t);
                }
            }
        }
    }
}