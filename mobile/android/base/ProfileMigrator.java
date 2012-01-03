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
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
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

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserDB;

import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteException;
import android.database.sqlite.SQLiteStatement;
import android.content.ContentResolver;
import android.database.Cursor;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.provider.Browser;
import android.util.Log;
import android.webkit.WebIconDatabase;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.File;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Map;
import java.util.HashMap;
import java.util.Date;
import java.util.List;
import java.util.Iterator;


public class ProfileMigrator {
    private static final String LOGTAG = "ProfMigr";
    private File mProfileDir;
    private ContentResolver mCr;
    private SQLiteDatabase mDb;

    /*
      Amount of Android history entries we will remember
      to prevent moving their last access date backwards.
    */
    private static final int MAX_HISTORY_TO_CHECK = 1000;

    /*
       These queries are derived from the low-level Places schema
       https://developer.mozilla.org/en/The_Places_database
    */
    final String bookmarkQuery = "SELECT places.url AS a_url, "
        + "places.title AS a_title FROM "
        + "(moz_places as places JOIN moz_bookmarks as bookmarks ON "
        + "places.id = bookmarks.fk) WHERE places.hidden <> 1 "
        + "ORDER BY bookmarks.dateAdded";
    // Don't ask why. Just curse along at the Android devs.
    final String bookmarkUrl = "a_url";
    final String bookmarkTitle = "a_title";

    final String historyQuery =
        "SELECT places.url AS a_url, places.title AS a_title, "
        + "history.visit_date AS a_date FROM "
        + "(moz_historyvisits AS history JOIN moz_places AS places ON "
        + "places.id = history.place_id) WHERE places.hidden <> 1 "
        + "ORDER BY history.visit_date DESC";
    final String historyUrl = "a_url";
    final String historyTitle = "a_title";
    final String historyDate = "a_date";

    final String faviconQuery =
        "SELECT places.url AS a_url, favicon.data AS a_data, "
        + "favicon.mime_type AS a_mime FROM (moz_places AS places JOIN "
        + "moz_favicons AS favicon ON places.favicon_id = favicon.id)";
    final String faviconUrl = "a_url";
    final String faviconData = "a_data";
    final String faviconMime = "a_mime";

    public ProfileMigrator(ContentResolver cr, File profileDir) {
        mProfileDir = profileDir;
        mCr = cr;
    }

    public void launchBackground() {
        // Work around http://code.google.com/p/android/issues/detail?id=11291
        // The WebIconDatabase needs to be initialized within the UI thread so
        // just request the instance here.
        WebIconDatabase.getInstance();

        PlacesTask placesTask = new PlacesTask();
        new Thread(placesTask).start();
    }

    private class PlacesTask implements Runnable {
        // Get a list of the last times an URL was accessed
        protected Map<String, Long> gatherAndroidHistory() {
            Map<String, Long> history = new HashMap<String, Long>();

            Cursor cursor = BrowserDB.getRecentHistory(mCr, MAX_HISTORY_TO_CHECK);
            final int urlCol =
                cursor.getColumnIndexOrThrow(BrowserDB.URLColumns.URL);
            final int dateCol =
                cursor.getColumnIndexOrThrow(BrowserDB.URLColumns.DATE_LAST_VISITED);

            cursor.moveToFirst();
            while (!cursor.isAfterLast()) {
                String url = cursor.getString(urlCol);
                Long date = cursor.getLong(dateCol);
                // getRecentHistory returns newest-to-oldest, which means
                // we remember the most recent access
                if (!history.containsKey(url)) {
                    history.put(url, date);
                }
                cursor.moveToNext();
            }
            cursor.close();

            return history;
        }

        protected void addHistory(Map<String, Long> androidHistory,
                                  String url, String title, long date) {
            boolean allowUpdate = false;

            if (!androidHistory.containsKey(url)) {
                // Android doesn't know the URL, allow it to be
                // inserted with places date
                allowUpdate = true;
            } else {
                long androidDate = androidHistory.get(url);
                if (androidDate < date) {
                    // Places URL hit is newer than Android,
                    // allow it to be updated with places date
                    allowUpdate = true;
                }
            }

            if (allowUpdate) {
                BrowserDB.updateVisitedHistory(mCr, url);
                BrowserDB.updateHistoryDate(mCr, url, date);
                if (title != null) {
                    BrowserDB.updateHistoryTitle(mCr, url, title);
                }
            }
        }

        protected void migrateHistory(SQLiteDatabase db) {
            Map<String, Long> androidHistory = gatherAndroidHistory();
            final ArrayList<String> placesHistory = new ArrayList<String>();

            Cursor cursor = null;
            try {
                cursor =
                    db.rawQuery(historyQuery, new String[] { });
                final int urlCol =
                    cursor.getColumnIndexOrThrow(historyUrl);
                final int titleCol =
                    cursor.getColumnIndexOrThrow(historyTitle);
                final int dateCol =
                    cursor.getColumnIndexOrThrow(historyDate);

                cursor.moveToFirst();
                while (!cursor.isAfterLast()) {
                    String url = cursor.getString(urlCol);
                    String title = cursor.getString(titleCol);
                    // Convert from us (Places) to ms (Java, Android)
                    long date = cursor.getLong(dateCol) / (long)1000;
                    addHistory(androidHistory, url, title, date);
                    placesHistory.add(url);
                    cursor.moveToNext();
                }

                cursor.close();
            } catch (SQLiteException e) {
                if (cursor != null) {
                    cursor.close();
                }
                Log.i(LOGTAG, "Failed to get bookmarks: " + e.getMessage());
                return;
            }
            // GlobalHistory access communicates with Gecko
            // and must run on its thread
            GeckoAppShell.getHandler().post(new Runnable() {
                    public void run() {
                        for (String url : placesHistory) {
                            GlobalHistory.getInstance().addToGeckoOnly(url);
                        }
                    }
             });
        }

        protected void addBookmark(String url, String title) {
            if (!BrowserDB.isBookmark(mCr, url)) {
                if (title == null) {
                    title = url;
                }
                BrowserDB.addBookmark(mCr, title, url);
            }
        }

        protected void migrateBookmarks(SQLiteDatabase db) {
            Cursor cursor = null;
            try {
                cursor = db.rawQuery(bookmarkQuery,
                                     new String[] {});
                if (cursor.getCount() > 0) {
                    final int urlCol =
                        cursor.getColumnIndexOrThrow(bookmarkUrl);
                    final int titleCol =
                        cursor.getColumnIndexOrThrow(bookmarkTitle);

                    cursor.moveToFirst();
                    while (!cursor.isAfterLast()) {
                        String url = cursor.getString(urlCol);
                        String title = cursor.getString(titleCol);
                        addBookmark(url, title);
                        cursor.moveToNext();
                    }
                }
                cursor.close();
            } catch (SQLiteException e) {
                if (cursor != null) {
                    cursor.close();
                }
                Log.i(LOGTAG, "Failed to get bookmarks: " + e.getMessage());
                return;
            }
        }

        protected void addFavicon(String url, String mime, byte[] data) {
            ByteArrayInputStream byteStream = new ByteArrayInputStream(data);
            BitmapDrawable image = (BitmapDrawable) Drawable.createFromStream(byteStream, "src");
            if (image != null) {
                try {
                    BrowserDB.updateFaviconForUrl(mCr, url, image);
                } catch (SQLiteException e) {
                    Log.i(LOGTAG, "Migrating favicon failed: " + mime + " URL: " + url
                          + " error:" + e.getMessage());
                }
            }
        }

        protected void migrateFavicons(SQLiteDatabase db) {
            Cursor cursor = null;
            try {
                cursor = db.rawQuery(faviconQuery,
                                     new String[] {});
                if (cursor.getCount() > 0) {
                    final int urlCol =
                        cursor.getColumnIndexOrThrow(faviconUrl);
                    final int dataCol =
                        cursor.getColumnIndexOrThrow(faviconData);
                    final int mimeCol =
                        cursor.getColumnIndexOrThrow(faviconMime);

                    cursor.moveToFirst();
                    while (!cursor.isAfterLast()) {
                        String url = cursor.getString(urlCol);
                        String mime = cursor.getString(mimeCol);
                        byte[] data = cursor.getBlob(dataCol);
                        addFavicon(url, mime, data);
                        cursor.moveToNext();
                    }
                }
                cursor.close();
            } catch (SQLiteException e) {
                if (cursor != null) {
                    cursor.close();
                }
                Log.i(LOGTAG, "Failed to get favicons: " + e.getMessage());
                return;
            }
        }

        SQLiteDatabase openPlaces(String dbPath) throws SQLiteException {
            /* http://stackoverflow.com/questions/2528489/no-such-table-android-metadata-whats-the-problem */
            SQLiteDatabase db = SQLiteDatabase.openDatabase(dbPath,
                                                            null,
                                                            SQLiteDatabase.OPEN_READONLY |
                                                            SQLiteDatabase.NO_LOCALIZED_COLLATORS);

            return db;
        }

        protected void migratePlaces(File aFile) {
            String dbPath = aFile.getPath() + "/places.sqlite";
            String dbPathWal = aFile.getPath() + "/places.sqlite-wal";
            String dbPathShm = aFile.getPath() + "/places.sqlite-shm";
            Log.i(LOGTAG, "Opening path: " + dbPath);

            File dbFile = new File(dbPath);
            if (!dbFile.exists()) {
                Log.i(LOGTAG, "No database");
                return;
            }
            File dbFileWal = new File(dbPathWal);
            File dbFileShm = new File(dbPathShm);

            SQLiteDatabase db = null;
            try {
                db = openPlaces(dbPath);
                migrateBookmarks(db);
                migrateHistory(db);
                migrateFavicons(db);
                db.close();

                // Clean up
                dbFile.delete();
                dbFileWal.delete();
                dbFileShm.delete();

                Log.i(LOGTAG, "Profile migration finished");
            } catch (SQLiteException e) {
                if (db != null) {
                    db.close();
                }
                Log.i(LOGTAG, "Error on places database:" + e.getMessage());
                return;
            }
        }

        protected void cleanupXULLibCache() {
            File cacheFile = GeckoAppShell.getCacheDir();
            File[] files = cacheFile.listFiles();
            if (files != null) {
                Iterator cacheFiles = Arrays.asList(files).iterator();
                while (cacheFiles.hasNext()) {
                    File libFile = (File)cacheFiles.next();
                    if (libFile.getName().endsWith(".so")) {
                        libFile.delete();
                    }
                }
            }
        }

        @Override
        public void run() {
            migratePlaces(mProfileDir);
            // XXX: Land dependent bugs first
            // cleanupXULLibCache();
        }
    }
}
