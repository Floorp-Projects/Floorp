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
 * Portions created by the Initial Developer are Copyright (C) 2011-2012
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
import org.mozilla.gecko.sqlite.ByteBufferInputStream;
import org.mozilla.gecko.sqlite.SQLiteBridge;
import org.mozilla.gecko.sqlite.SQLiteBridgeException;

import android.content.ContentResolver;
import android.database.Cursor;
import android.database.SQLException;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.provider.Browser;
import android.util.Log;

import java.io.IOException;
import java.io.File;
import java.nio.ByteBuffer;
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

    /*
      Amount of Android history entries we will remember
      to prevent moving their last access date backwards.
    */
    private static final int MAX_HISTORY_TO_CHECK = 1000;

    /*
       These queries are derived from the low-level Places schema
       https://developer.mozilla.org/en/The_Places_database
    */
    private final String bookmarkQuery = "SELECT places.url AS a_url, "
        + "places.title AS a_title FROM "
        + "(moz_places as places JOIN moz_bookmarks as bookmarks ON "
        + "places.id = bookmarks.fk) WHERE places.hidden <> 1 "
        + "ORDER BY bookmarks.dateAdded";
    // Result column of relevant data
    private final String bookmarkUrl   = "a_url";
    private final String bookmarkTitle = "a_title";

    private final String historyQuery =
        "SELECT places.url AS a_url, places.title AS a_title, "
        + "history.visit_date AS a_date FROM "
        + "(moz_historyvisits AS history JOIN moz_places AS places ON "
        + "places.id = history.place_id) WHERE places.hidden <> 1 "
        + "ORDER BY history.visit_date DESC";
    private final String historyUrl   = "a_url";
    private final String historyTitle = "a_title";
    private final String historyDate  = "a_date";

    private final String faviconQuery =
        "SELECT places.url AS a_url, favicon.data AS a_data, "
        + "favicon.mime_type AS a_mime FROM (moz_places AS places JOIN "
        + "moz_favicons AS favicon ON places.favicon_id = favicon.id)";
    private final String faviconUrl  = "a_url";
    private final String faviconData = "a_data";
    private final String faviconMime = "a_mime";

    public ProfileMigrator(ContentResolver cr, File profileDir) {
        mProfileDir = profileDir;
        mCr = cr;
    }

    public void launchBackground() {
        // Work around http://code.google.com/p/android/issues/detail?id=11291
        // WebIconDatabase needs to be initialized within a looper thread.
        GeckoAppShell.getHandler().post(new PlacesTask());
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

        protected void migrateHistory(SQLiteBridge db) {
            Map<String, Long> androidHistory = gatherAndroidHistory();
            final ArrayList<String> placesHistory = new ArrayList<String>();

            try {
                ArrayList<Object[]> queryResult = db.query(historyQuery);
                final int urlCol = db.getColumnIndex(historyUrl);
                final int titleCol = db.getColumnIndex(historyTitle);
                final int dateCol = db.getColumnIndex(historyDate);

                for (Object[] resultRow: queryResult) {
                    String url = (String)resultRow[urlCol];
                    String title = (String)resultRow[titleCol];
                    long date = Long.parseLong((String)(resultRow[dateCol])) / (long)1000;
                    addHistory(androidHistory, url, title, date);
                    placesHistory.add(url);
                }
            } catch (SQLiteBridgeException e) {
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

        protected void migrateBookmarks(SQLiteBridge db) {
            try {
                ArrayList<Object[]> queryResult = db.query(bookmarkQuery);
                final int urlCol = db.getColumnIndex(bookmarkUrl);
                final int titleCol = db.getColumnIndex(bookmarkTitle);

                for (Object[] resultRow: queryResult) {
                    String url = (String)resultRow[urlCol];
                    String title = (String)resultRow[titleCol];
                    addBookmark(url, title);
                }
            } catch (SQLiteBridgeException e) {
                Log.i(LOGTAG, "Failed to get bookmarks: " + e.getMessage());
                return;
            }
        }

        protected void addFavicon(String url, String mime, ByteBuffer data) {
            ByteBufferInputStream byteStream = new ByteBufferInputStream(data);
            BitmapDrawable image = (BitmapDrawable) Drawable.createFromStream(byteStream, "src");
            if (image != null) {
                try {
                    BrowserDB.updateFaviconForUrl(mCr, url, image);
                } catch (SQLException e) {
                    Log.i(LOGTAG, "Migrating favicon failed: " + mime + " URL: " + url
                          + " error:" + e.getMessage());
                }
            }
        }

        protected void migrateFavicons(SQLiteBridge db) {
            try {
                ArrayList<Object[]> queryResult = db.query(faviconQuery);
                final int urlCol = db.getColumnIndex(faviconUrl);
                final int mimeCol = db.getColumnIndex(faviconMime);
                final int dataCol = db.getColumnIndex(faviconData);

                for (Object[] resultRow: queryResult) {
                    String url = (String)resultRow[urlCol];
                    String mime = (String)resultRow[mimeCol];
                    ByteBuffer dataBuff = (ByteBuffer)resultRow[dataCol];
                    addFavicon(url, mime, dataBuff);
                }
            } catch (SQLiteBridgeException e) {
                Log.i(LOGTAG, "Failed to get favicons: " + e.getMessage());
                return;
            }
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

            SQLiteBridge db = null;
            try {
                db = new SQLiteBridge(dbPath);
                migrateBookmarks(db);
                migrateHistory(db);
                migrateFavicons(db);
                db.close();

                // Clean up
                dbFile.delete();
                dbFileWal.delete();
                dbFileShm.delete();

                Log.i(LOGTAG, "Profile migration finished");
            } catch (SQLiteBridgeException e) {
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
