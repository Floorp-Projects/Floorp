/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.io.File;
import java.io.UnsupportedEncodingException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.mozilla.apache.commons.codec.binary.Base32;
import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.db.BrowserContract.ActivityStreamBlocklist;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserContract.Favicons;
import org.mozilla.gecko.db.BrowserContract.History;
import org.mozilla.gecko.db.BrowserContract.Visits;
import org.mozilla.gecko.db.BrowserContract.PageMetadata;
import org.mozilla.gecko.db.BrowserContract.Numbers;
import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.db.BrowserContract.SearchHistory;
import org.mozilla.gecko.db.BrowserContract.Thumbnails;
import org.mozilla.gecko.db.BrowserContract.UrlAnnotations;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.reader.SavedReaderViewHelper;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;
import org.mozilla.gecko.util.FileUtils;

import static org.mozilla.gecko.db.DBUtils.qualifyColumn;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.database.SQLException;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteException;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteStatement;
import android.net.Uri;
import android.os.Build;
import android.util.Log;


// public for robocop testing
public final class BrowserDatabaseHelper extends SQLiteOpenHelper {
    private static final String LOGTAG = "GeckoBrowserDBHelper";

    // Replace the Bug number below with your Bug that is conducting a DB upgrade, as to force a merge conflict with any
    // other patches that require a DB upgrade.
    public static final int DATABASE_VERSION = 36; // Bug 1301717
    public static final String DATABASE_NAME = "browser.db";

    final protected Context mContext;

    static final String TABLE_BOOKMARKS = Bookmarks.TABLE_NAME;
    static final String TABLE_HISTORY = History.TABLE_NAME;
    static final String TABLE_VISITS = Visits.TABLE_NAME;
    static final String TABLE_PAGE_METADATA = PageMetadata.TABLE_NAME;
    static final String TABLE_FAVICONS = Favicons.TABLE_NAME;
    static final String TABLE_THUMBNAILS = Thumbnails.TABLE_NAME;
    static final String TABLE_READING_LIST = ReadingListItems.TABLE_NAME;
    static final String TABLE_TABS = TabsProvider.TABLE_TABS;
    static final String TABLE_CLIENTS = TabsProvider.TABLE_CLIENTS;
    static final String TABLE_LOGINS = BrowserContract.Logins.TABLE_LOGINS;
    static final String TABLE_DELETED_LOGINS = BrowserContract.DeletedLogins.TABLE_DELETED_LOGINS;
    static final String TABLE_DISABLED_HOSTS = BrowserContract.LoginsDisabledHosts.TABLE_DISABLED_HOSTS;
    static final String TABLE_ANNOTATIONS = UrlAnnotations.TABLE_NAME;

    static final String VIEW_COMBINED = Combined.VIEW_NAME;
    static final String VIEW_BOOKMARKS_WITH_FAVICONS = Bookmarks.VIEW_WITH_FAVICONS;
    static final String VIEW_BOOKMARKS_WITH_ANNOTATIONS = Bookmarks.VIEW_WITH_ANNOTATIONS;
    static final String VIEW_HISTORY_WITH_FAVICONS = History.VIEW_WITH_FAVICONS;
    static final String VIEW_COMBINED_WITH_FAVICONS = Combined.VIEW_WITH_FAVICONS;

    static final String TABLE_BOOKMARKS_JOIN_FAVICONS = TABLE_BOOKMARKS + " LEFT OUTER JOIN " +
            TABLE_FAVICONS + " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.FAVICON_ID) + " = " +
            qualifyColumn(TABLE_FAVICONS, Favicons._ID);

    static final String TABLE_BOOKMARKS_JOIN_ANNOTATIONS = TABLE_BOOKMARKS + " JOIN " +
            TABLE_ANNOTATIONS + " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " = " +
            qualifyColumn(TABLE_ANNOTATIONS, UrlAnnotations.URL);

    static final String TABLE_HISTORY_JOIN_FAVICONS = TABLE_HISTORY + " LEFT OUTER JOIN " +
            TABLE_FAVICONS + " ON " + qualifyColumn(TABLE_HISTORY, History.FAVICON_ID) + " = " +
            qualifyColumn(TABLE_FAVICONS, Favicons._ID);

    static final String TABLE_BOOKMARKS_TMP = TABLE_BOOKMARKS + "_tmp";
    static final String TABLE_HISTORY_TMP = TABLE_HISTORY + "_tmp";

    private static final String[] mobileIdColumns = new String[] { Bookmarks._ID };
    private static final String[] mobileIdSelectionArgs = new String[] { Bookmarks.MOBILE_FOLDER_GUID };

    private boolean didCreateTabsTable = false;
    private boolean didCreateCurrentReadingListTable = false;

    public BrowserDatabaseHelper(Context context, String databasePath) {
        super(context, databasePath, null, DATABASE_VERSION);
        mContext = context;
    }

    private void createBookmarksTable(SQLiteDatabase db) {
        debug("Creating " + TABLE_BOOKMARKS + " table");

        db.execSQL("CREATE TABLE " + TABLE_BOOKMARKS + "(" +
                Bookmarks._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                Bookmarks.TITLE + " TEXT," +
                Bookmarks.URL + " TEXT," +
                Bookmarks.TYPE + " INTEGER NOT NULL DEFAULT " + Bookmarks.TYPE_BOOKMARK + "," +
                Bookmarks.PARENT + " INTEGER," +
                Bookmarks.POSITION + " INTEGER NOT NULL," +
                Bookmarks.KEYWORD + " TEXT," +
                Bookmarks.DESCRIPTION + " TEXT," +
                Bookmarks.TAGS + " TEXT," +
                Bookmarks.FAVICON_ID + " INTEGER," +
                Bookmarks.DATE_CREATED + " INTEGER," +
                Bookmarks.DATE_MODIFIED + " INTEGER," +
                Bookmarks.GUID + " TEXT NOT NULL," +
                Bookmarks.IS_DELETED + " INTEGER NOT NULL DEFAULT 0, " +
                "FOREIGN KEY (" + Bookmarks.PARENT + ") REFERENCES " +
                TABLE_BOOKMARKS + "(" + Bookmarks._ID + ")" +
                ");");

        db.execSQL("CREATE INDEX bookmarks_url_index ON " + TABLE_BOOKMARKS + "("
                + Bookmarks.URL + ")");
        db.execSQL("CREATE INDEX bookmarks_type_deleted_index ON " + TABLE_BOOKMARKS + "("
                + Bookmarks.TYPE + ", " + Bookmarks.IS_DELETED + ")");
        db.execSQL("CREATE UNIQUE INDEX bookmarks_guid_index ON " + TABLE_BOOKMARKS + "("
                + Bookmarks.GUID + ")");
        db.execSQL("CREATE INDEX bookmarks_modified_index ON " + TABLE_BOOKMARKS + "("
                + Bookmarks.DATE_MODIFIED + ")");
    }

    private void createHistoryTable(SQLiteDatabase db) {
        debug("Creating " + TABLE_HISTORY + " table");
        db.execSQL("CREATE TABLE " + TABLE_HISTORY + "(" +
                History._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                History.TITLE + " TEXT," +
                History.URL + " TEXT NOT NULL," +
                // Can we drop VISITS count? Can we calculate it in the Combined view as a sum?
                // See Bug 1277329.
                History.VISITS + " INTEGER NOT NULL DEFAULT 0," +
                History.LOCAL_VISITS + " INTEGER NOT NULL DEFAULT 0," +
                History.REMOTE_VISITS + " INTEGER NOT NULL DEFAULT 0," +
                History.FAVICON_ID + " INTEGER," +
                History.DATE_LAST_VISITED + " INTEGER," +
                History.LOCAL_DATE_LAST_VISITED + " INTEGER NOT NULL DEFAULT 0," +
                History.REMOTE_DATE_LAST_VISITED + " INTEGER NOT NULL DEFAULT 0," +
                History.DATE_CREATED + " INTEGER," +
                History.DATE_MODIFIED + " INTEGER," +
                History.GUID + " TEXT NOT NULL," +
                History.IS_DELETED + " INTEGER NOT NULL DEFAULT 0" +
                ");");

        db.execSQL("CREATE INDEX history_url_index ON " + TABLE_HISTORY + '('
                + History.URL + ')');
        db.execSQL("CREATE UNIQUE INDEX history_guid_index ON " + TABLE_HISTORY + '('
                + History.GUID + ')');
        db.execSQL("CREATE INDEX history_modified_index ON " + TABLE_HISTORY + '('
                + History.DATE_MODIFIED + ')');
        db.execSQL("CREATE INDEX history_visited_index ON " + TABLE_HISTORY + '('
                + History.DATE_LAST_VISITED + ')');
    }

    private void createVisitsTable(SQLiteDatabase db) {
        debug("Creating " + TABLE_VISITS + " table");
        db.execSQL("CREATE TABLE " + TABLE_VISITS + "(" +
                Visits._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                Visits.HISTORY_GUID + " TEXT NOT NULL," +
                Visits.VISIT_TYPE + " TINYINT NOT NULL DEFAULT 1," +
                Visits.DATE_VISITED + " INTEGER NOT NULL, " +
                Visits.IS_LOCAL + " TINYINT NOT NULL DEFAULT 1, " +

                "FOREIGN KEY (" + Visits.HISTORY_GUID + ") REFERENCES " +
                TABLE_HISTORY + "(" + History.GUID + ") ON DELETE CASCADE ON UPDATE CASCADE" +
                ");");

        db.execSQL("CREATE UNIQUE INDEX visits_history_guid_and_date_visited_index ON " + TABLE_VISITS + "("
            + Visits.HISTORY_GUID + "," + Visits.DATE_VISITED + ")");
        db.execSQL("CREATE INDEX visits_history_guid_index ON " + TABLE_VISITS + "(" + Visits.HISTORY_GUID + ")");
    }

    private void createFaviconsTable(SQLiteDatabase db) {
        debug("Creating " + TABLE_FAVICONS + " table");
        db.execSQL("CREATE TABLE " + TABLE_FAVICONS + " (" +
                Favicons._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                Favicons.URL + " TEXT UNIQUE," +
                Favicons.DATA + " BLOB," +
                Favicons.DATE_CREATED + " INTEGER," +
                Favicons.DATE_MODIFIED + " INTEGER" +
                ");");

        db.execSQL("CREATE INDEX favicons_modified_index ON " + TABLE_FAVICONS + "("
                + Favicons.DATE_MODIFIED + ")");
    }

    private void createThumbnailsTable(SQLiteDatabase db) {
        debug("Creating " + TABLE_THUMBNAILS + " table");
        db.execSQL("CREATE TABLE " + TABLE_THUMBNAILS + " (" +
                Thumbnails._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                Thumbnails.URL + " TEXT UNIQUE," +
                Thumbnails.DATA + " BLOB" +
                ");");
    }

    private void createPageMetadataTable(SQLiteDatabase db) {
        debug("Creating " + TABLE_PAGE_METADATA + " table");
        db.execSQL("CREATE TABLE " + TABLE_PAGE_METADATA + "(" +
                PageMetadata._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                PageMetadata.HISTORY_GUID + " TEXT NOT NULL," +
                PageMetadata.DATE_CREATED + " INTEGER NOT NULL, " +
                PageMetadata.HAS_IMAGE + " TINYINT NOT NULL DEFAULT 0, " +
                PageMetadata.JSON + " TEXT NOT NULL, " +

                "FOREIGN KEY (" + Visits.HISTORY_GUID + ") REFERENCES " +
                TABLE_HISTORY + "(" + History.GUID + ") ON DELETE CASCADE ON UPDATE CASCADE" +
                ");");

        // Establish a 1-to-1 relationship with History table.
        db.execSQL("CREATE UNIQUE INDEX page_metadata_history_guid ON " + TABLE_PAGE_METADATA + "("
                + PageMetadata.HISTORY_GUID + ")");
        // Improve performance of commonly occurring selections.
        db.execSQL("CREATE INDEX page_metadata_history_guid_and_has_image ON " + TABLE_PAGE_METADATA + "("
                + PageMetadata.HISTORY_GUID + ", " + PageMetadata.HAS_IMAGE + ")");
    }

    private void createBookmarksWithFaviconsView(SQLiteDatabase db) {
        debug("Creating " + VIEW_BOOKMARKS_WITH_FAVICONS + " view");

        db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_BOOKMARKS_WITH_FAVICONS + " AS " +
                "SELECT " + qualifyColumn(TABLE_BOOKMARKS, "*") +
                ", " + qualifyColumn(TABLE_FAVICONS, Favicons.DATA) + " AS " + Bookmarks.FAVICON +
                ", " + qualifyColumn(TABLE_FAVICONS, Favicons.URL) + " AS " + Bookmarks.FAVICON_URL +
                " FROM " + TABLE_BOOKMARKS_JOIN_FAVICONS);
    }

    private void createBookmarksWithAnnotationsView(SQLiteDatabase db) {
        debug("Creating " + VIEW_BOOKMARKS_WITH_ANNOTATIONS + " view");

        db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_BOOKMARKS_WITH_ANNOTATIONS + " AS " +
                   "SELECT " + qualifyColumn(TABLE_BOOKMARKS, "*") +
                   ", " + qualifyColumn(TABLE_ANNOTATIONS, UrlAnnotations.KEY) + " AS " + Bookmarks.ANNOTATION_KEY +
                   ", " + qualifyColumn(TABLE_ANNOTATIONS, UrlAnnotations.VALUE) + " AS " + Bookmarks.ANNOTATION_VALUE +
                   " FROM " + TABLE_BOOKMARKS_JOIN_ANNOTATIONS);
    }

    private void createHistoryWithFaviconsView(SQLiteDatabase db) {
        debug("Creating " + VIEW_HISTORY_WITH_FAVICONS + " view");

        db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_HISTORY_WITH_FAVICONS + " AS " +
                "SELECT " + qualifyColumn(TABLE_HISTORY, "*") +
                ", " + qualifyColumn(TABLE_FAVICONS, Favicons.DATA) + " AS " + History.FAVICON +
                ", " + qualifyColumn(TABLE_FAVICONS, Favicons.URL) + " AS " + History.FAVICON_URL +
                " FROM " + TABLE_HISTORY_JOIN_FAVICONS);
    }

    private void createClientsTable(SQLiteDatabase db) {
        debug("Creating " + TABLE_CLIENTS + " table");

        // Table for client's name-guid mapping.
        db.execSQL("CREATE TABLE " + TABLE_CLIENTS + "(" +
                BrowserContract.Clients.GUID + " TEXT PRIMARY KEY," +
                BrowserContract.Clients.NAME + " TEXT," +
                BrowserContract.Clients.LAST_MODIFIED + " INTEGER," +
                BrowserContract.Clients.DEVICE_TYPE + " TEXT" +
                ");");
    }

    private void createTabsTable(SQLiteDatabase db, final String tableName) {
        debug("Creating tabs.db: " + db.getPath());
        debug("Creating " + tableName + " table");

        // Table for each tab on any client.
        db.execSQL("CREATE TABLE " + tableName + "(" +
                BrowserContract.Tabs._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                BrowserContract.Tabs.CLIENT_GUID + " TEXT," +
                BrowserContract.Tabs.TITLE + " TEXT," +
                BrowserContract.Tabs.URL + " TEXT," +
                BrowserContract.Tabs.HISTORY + " TEXT," +
                BrowserContract.Tabs.FAVICON + " TEXT," +
                BrowserContract.Tabs.LAST_USED + " INTEGER," +
                BrowserContract.Tabs.POSITION + " INTEGER, " +
                "FOREIGN KEY (" + BrowserContract.Tabs.CLIENT_GUID + ") REFERENCES " +
                TABLE_CLIENTS + "(" + BrowserContract.Clients.GUID + ") ON DELETE CASCADE" +
                ");");

        didCreateTabsTable = true;
    }

    private void createTabsTableIndices(SQLiteDatabase db, final String tableName) {
        // Indices on CLIENT_GUID and POSITION.
        db.execSQL("CREATE INDEX " + TabsProvider.INDEX_TABS_GUID +
                " ON " + tableName + "(" + BrowserContract.Tabs.CLIENT_GUID + ")");
        db.execSQL("CREATE INDEX " + TabsProvider.INDEX_TABS_POSITION +
                " ON " + tableName + "(" + BrowserContract.Tabs.POSITION + ")");
    }

    // Insert a client row for our local Fennec client.
    private void createLocalClient(SQLiteDatabase db) {
        debug("Inserting local Fennec client into " + TABLE_CLIENTS + " table");

        ContentValues values = new ContentValues();
        values.put(BrowserContract.Clients.LAST_MODIFIED, System.currentTimeMillis());
        db.insertOrThrow(TABLE_CLIENTS, null, values);
    }

    private void createCombinedViewOn19(SQLiteDatabase db) {
        /*
        The v19 combined view removes the redundant subquery from the v16
        combined view and reorders the columns as necessary to prevent this
        from breaking any code that might be referencing columns by index.

        The rows in the ensuing view are, in order:

            Combined.BOOKMARK_ID
            Combined.HISTORY_ID
            Combined._ID (always 0)
            Combined.URL
            Combined.TITLE
            Combined.VISITS
            Combined.DATE_LAST_VISITED
            Combined.FAVICON_ID

        We need to return an _id column because CursorAdapter requires it for its
        default implementation for the getItemId() method. However, since
        we're not using this feature in the parts of the UI using this view,
        we can just use 0 for all rows.
         */

        db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_COMBINED + " AS" +

                // Bookmarks without history.
                " SELECT " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) + " AS " + Combined.BOOKMARK_ID + "," +
                    "-1 AS " + Combined.HISTORY_ID + "," +
                    "0 AS " + Combined._ID + "," +
                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " AS " + Combined.URL + ", " +
                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + " AS " + Combined.TITLE + ", " +
                    "-1 AS " + Combined.VISITS + ", " +
                    "-1 AS " + Combined.DATE_LAST_VISITED + "," +
                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.FAVICON_ID) + " AS " + Combined.FAVICON_ID +
                " FROM " + TABLE_BOOKMARKS +
                " WHERE " +
                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + " AND " +
                    // Ignore pinned bookmarks.
                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT)  + " <> " + Bookmarks.FIXED_PINNED_LIST_ID + " AND " +
                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED)  + " = 0 AND " +
                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) +
                        " NOT IN (SELECT " + History.URL + " FROM " + TABLE_HISTORY + ")" +
                " UNION ALL" +

                    // History with and without bookmark.
                    " SELECT " +
                        "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED) +

                            // Give pinned bookmarks a NULL ID so that they're not treated as bookmarks. We can't
                            // completely ignore them here because they're joined with history entries we care about.
                            " WHEN 0 THEN " +
                                "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) +
                                    " WHEN " + Bookmarks.FIXED_PINNED_LIST_ID + " THEN " +
                                        "NULL " +
                                    "ELSE " +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) +
                                " END " +
                            "ELSE " +
                                "NULL " +
                        "END AS " + Combined.BOOKMARK_ID + "," +
                        qualifyColumn(TABLE_HISTORY, History._ID) + " AS " + Combined.HISTORY_ID + "," +
                        "0 AS " + Combined._ID + "," +
                        qualifyColumn(TABLE_HISTORY, History.URL) + " AS " + Combined.URL + "," +

                        // Prioritize bookmark titles over history titles, since the user may have
                        // customized the title for a bookmark.
                        "COALESCE(" + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + ", " +
                                      qualifyColumn(TABLE_HISTORY, History.TITLE) +
                                ") AS " + Combined.TITLE + "," +
                        qualifyColumn(TABLE_HISTORY, History.VISITS) + " AS " + Combined.VISITS + "," +
                        qualifyColumn(TABLE_HISTORY, History.DATE_LAST_VISITED) + " AS " + Combined.DATE_LAST_VISITED + "," +
                        qualifyColumn(TABLE_HISTORY, History.FAVICON_ID) + " AS " + Combined.FAVICON_ID +

                    // We really shouldn't be selecting deleted bookmarks, but oh well.
                    " FROM " + TABLE_HISTORY + " LEFT OUTER JOIN " + TABLE_BOOKMARKS +
                    " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " = " + qualifyColumn(TABLE_HISTORY, History.URL) +
                    " WHERE " +
                        qualifyColumn(TABLE_HISTORY, History.IS_DELETED) + " = 0 AND " +
                        "(" +
                            // The left outer join didn't match...
                            qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE) + " IS NULL OR " +

                            // ... or it's a bookmark. This is less efficient than filtering prior
                            // to the join if you have lots of folders.
                            qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE) + " = " + Bookmarks.TYPE_BOOKMARK +
                        ")"
        );

        debug("Creating " + VIEW_COMBINED_WITH_FAVICONS + " view");

        db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_COMBINED_WITH_FAVICONS + " AS" +
                " SELECT " + qualifyColumn(VIEW_COMBINED, "*") + ", " +
                    qualifyColumn(TABLE_FAVICONS, Favicons.URL) + " AS " + Combined.FAVICON_URL + ", " +
                    qualifyColumn(TABLE_FAVICONS, Favicons.DATA) + " AS " + Combined.FAVICON +
                " FROM " + VIEW_COMBINED + " LEFT OUTER JOIN " + TABLE_FAVICONS +
                    " ON " + Combined.FAVICON_ID + " = " + qualifyColumn(TABLE_FAVICONS, Favicons._ID));

    }

    private void createCombinedViewOn33(final SQLiteDatabase db) {
        /*
        Builds on top of v19 combined view, and adds the following aggregates:
        - Combined.LOCAL_DATE_LAST_VISITED - last date visited for all local visits
        - Combined.REMOTE_DATE_LAST_VISITED - last date visited for all remote visits
        - Combined.LOCAL_VISITS_COUNT - total number of local visits
        - Combined.REMOTE_VISITS_COUNT - total number of remote visits

        Any code written prior to v33 referencing columns by index directly remains intact
        (yet must die a fiery death), as new columns were added to the end of the list.

        The rows in the ensuing view are, in order:
            Combined.BOOKMARK_ID
            Combined.HISTORY_ID
            Combined._ID (always 0)
            Combined.URL
            Combined.TITLE
            Combined.VISITS
            Combined.DATE_LAST_VISITED
            Combined.FAVICON_ID
            Combined.LOCAL_DATE_LAST_VISITED
            Combined.REMOTE_DATE_LAST_VISITED
            Combined.LOCAL_VISITS_COUNT
            Combined.REMOTE_VISITS_COUNT
         */
        db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_COMBINED + " AS" +

                // Bookmarks without history.
                " SELECT " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) + " AS " + Combined.BOOKMARK_ID + "," +
                "-1 AS " + Combined.HISTORY_ID + "," +
                "0 AS " + Combined._ID + "," +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " AS " + Combined.URL + ", " +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + " AS " + Combined.TITLE + ", " +
                "-1 AS " + Combined.VISITS + ", " +
                "-1 AS " + Combined.DATE_LAST_VISITED + "," +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.FAVICON_ID) + " AS " + Combined.FAVICON_ID + "," +
                "0 AS " + Combined.LOCAL_DATE_LAST_VISITED + ", " +
                "0 AS " + Combined.REMOTE_DATE_LAST_VISITED + ", " +
                "0 AS " + Combined.LOCAL_VISITS_COUNT + ", " +
                "0 AS " + Combined.REMOTE_VISITS_COUNT +
                " FROM " + TABLE_BOOKMARKS +
                " WHERE " +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + " AND " +
                // Ignore pinned bookmarks.
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT)  + " <> " + Bookmarks.FIXED_PINNED_LIST_ID + " AND " +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED)  + " = 0 AND " +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) +
                " NOT IN (SELECT " + History.URL + " FROM " + TABLE_HISTORY + ")" +
                " UNION ALL" +

                // History with and without bookmark.
                " SELECT " +
                "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED) +

                // Give pinned bookmarks a NULL ID so that they're not treated as bookmarks. We can't
                // completely ignore them here because they're joined with history entries we care about.
                " WHEN 0 THEN " +
                "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) +
                " WHEN " + Bookmarks.FIXED_PINNED_LIST_ID + " THEN " +
                "NULL " +
                "ELSE " +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) +
                " END " +
                "ELSE " +
                "NULL " +
                "END AS " + Combined.BOOKMARK_ID + "," +
                qualifyColumn(TABLE_HISTORY, History._ID) + " AS " + Combined.HISTORY_ID + "," +
                "0 AS " + Combined._ID + "," +
                qualifyColumn(TABLE_HISTORY, History.URL) + " AS " + Combined.URL + "," +

                // Prioritize bookmark titles over history titles, since the user may have
                // customized the title for a bookmark.
                "COALESCE(" + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + ", " +
                qualifyColumn(TABLE_HISTORY, History.TITLE) +
                ") AS " + Combined.TITLE + "," +
                qualifyColumn(TABLE_HISTORY, History.VISITS) + " AS " + Combined.VISITS + "," +
                qualifyColumn(TABLE_HISTORY, History.DATE_LAST_VISITED) + " AS " + Combined.DATE_LAST_VISITED + "," +
                qualifyColumn(TABLE_HISTORY, History.FAVICON_ID) + " AS " + Combined.FAVICON_ID + "," +

                // Figure out "last visited" days using MAX values for visit timestamps.
                // We use CASE statements here to separate local from remote visits.
                "COALESCE(MAX(CASE " + qualifyColumn(TABLE_VISITS, Visits.IS_LOCAL) + " " +
                    "WHEN 1 THEN " + qualifyColumn(TABLE_VISITS, Visits.DATE_VISITED) + " " +
                    "ELSE 0 END" +
                "), 0) AS " + Combined.LOCAL_DATE_LAST_VISITED + ", " +

                "COALESCE(MAX(CASE " + qualifyColumn(TABLE_VISITS, Visits.IS_LOCAL) + " " +
                    "WHEN 0 THEN " + qualifyColumn(TABLE_VISITS, Visits.DATE_VISITED) + " " +
                    "ELSE 0 END" +
                "), 0) AS " + Combined.REMOTE_DATE_LAST_VISITED + ", " +

                // Sum up visit counts for local and remote visit types. Again, use CASE to separate the two.
                "COALESCE(SUM(" + qualifyColumn(TABLE_VISITS, Visits.IS_LOCAL) + "), 0) AS " + Combined.LOCAL_VISITS_COUNT + ", " +
                "COALESCE(SUM(CASE " + qualifyColumn(TABLE_VISITS, Visits.IS_LOCAL) + " WHEN 0 THEN 1 ELSE 0 END), 0) AS " + Combined.REMOTE_VISITS_COUNT +

                // We need to JOIN on Visits in order to compute visit counts
                " FROM " + TABLE_HISTORY + " " +
                "LEFT OUTER JOIN " + TABLE_VISITS +
                " ON " + qualifyColumn(TABLE_HISTORY, History.GUID) + " = " + qualifyColumn(TABLE_VISITS, Visits.HISTORY_GUID) + " " +

                // We really shouldn't be selecting deleted bookmarks, but oh well.
                "LEFT OUTER JOIN " + TABLE_BOOKMARKS +
                " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " = " + qualifyColumn(TABLE_HISTORY, History.URL) +
                " WHERE " +
                qualifyColumn(TABLE_HISTORY, History.IS_DELETED) + " = 0 AND " +
                "(" +
                // The left outer join didn't match...
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE) + " IS NULL OR " +

                // ... or it's a bookmark. This is less efficient than filtering prior
                // to the join if you have lots of folders.
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE) + " = " + Bookmarks.TYPE_BOOKMARK +

                ") GROUP BY " + qualifyColumn(TABLE_HISTORY, History.GUID)
        );

        debug("Creating " + VIEW_COMBINED_WITH_FAVICONS + " view");

        db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_COMBINED_WITH_FAVICONS + " AS" +
                " SELECT " + qualifyColumn(VIEW_COMBINED, "*") + ", " +
                qualifyColumn(TABLE_FAVICONS, Favicons.URL) + " AS " + Combined.FAVICON_URL + ", " +
                qualifyColumn(TABLE_FAVICONS, Favicons.DATA) + " AS " + Combined.FAVICON +
                " FROM " + VIEW_COMBINED + " LEFT OUTER JOIN " + TABLE_FAVICONS +
                " ON " + Combined.FAVICON_ID + " = " + qualifyColumn(TABLE_FAVICONS, Favicons._ID));
    }

    private void createCombinedViewOn34(final SQLiteDatabase db) {
        /*
        Builds on top of v33 combined view, and instead of calculating the following aggregates, gets them
        from the history table:
        - Combined.LOCAL_DATE_LAST_VISITED - last date visited for all local visits
        - Combined.REMOTE_DATE_LAST_VISITED - last date visited for all remote visits
        - Combined.LOCAL_VISITS_COUNT - total number of local visits
        - Combined.REMOTE_VISITS_COUNT - total number of remote visits

        Any code written prior to v33 referencing columns by index directly remains intact
        (yet must die a fiery death), as new columns were added to the end of the list.

        The rows in the ensuing view are, in order:
            Combined.BOOKMARK_ID
            Combined.HISTORY_ID
            Combined._ID (always 0)
            Combined.URL
            Combined.TITLE
            Combined.VISITS
            Combined.DATE_LAST_VISITED
            Combined.FAVICON_ID
            Combined.LOCAL_DATE_LAST_VISITED
            Combined.REMOTE_DATE_LAST_VISITED
            Combined.LOCAL_VISITS_COUNT
            Combined.REMOTE_VISITS_COUNT
         */
        db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_COMBINED + " AS" +

                // Bookmarks without history.
                " SELECT " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) + " AS " + Combined.BOOKMARK_ID + "," +
                "-1 AS " + Combined.HISTORY_ID + "," +
                "0 AS " + Combined._ID + "," +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " AS " + Combined.URL + ", " +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + " AS " + Combined.TITLE + ", " +
                "-1 AS " + Combined.VISITS + ", " +
                "-1 AS " + Combined.DATE_LAST_VISITED + "," +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.FAVICON_ID) + " AS " + Combined.FAVICON_ID + "," +
                "0 AS " + Combined.LOCAL_DATE_LAST_VISITED + ", " +
                "0 AS " + Combined.REMOTE_DATE_LAST_VISITED + ", " +
                "0 AS " + Combined.LOCAL_VISITS_COUNT + ", " +
                "0 AS " + Combined.REMOTE_VISITS_COUNT +
                " FROM " + TABLE_BOOKMARKS +
                " WHERE " +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + " AND " +
                // Ignore pinned bookmarks.
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT)  + " <> " + Bookmarks.FIXED_PINNED_LIST_ID + " AND " +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED)  + " = 0 AND " +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) +
                " NOT IN (SELECT " + History.URL + " FROM " + TABLE_HISTORY + ")" +
                " UNION ALL" +

                // History with and without bookmark.
                " SELECT " +
                "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED) +

                // Give pinned bookmarks a NULL ID so that they're not treated as bookmarks. We can't
                // completely ignore them here because they're joined with history entries we care about.
                " WHEN 0 THEN " +
                "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) +
                " WHEN " + Bookmarks.FIXED_PINNED_LIST_ID + " THEN " +
                "NULL " +
                "ELSE " +
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) +
                " END " +
                "ELSE " +
                "NULL " +
                "END AS " + Combined.BOOKMARK_ID + "," +
                qualifyColumn(TABLE_HISTORY, History._ID) + " AS " + Combined.HISTORY_ID + "," +
                "0 AS " + Combined._ID + "," +
                qualifyColumn(TABLE_HISTORY, History.URL) + " AS " + Combined.URL + "," +

                // Prioritize bookmark titles over history titles, since the user may have
                // customized the title for a bookmark.
                "COALESCE(" + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + ", " +
                qualifyColumn(TABLE_HISTORY, History.TITLE) +
                ") AS " + Combined.TITLE + "," +
                qualifyColumn(TABLE_HISTORY, History.VISITS) + " AS " + Combined.VISITS + "," +
                qualifyColumn(TABLE_HISTORY, History.DATE_LAST_VISITED) + " AS " + Combined.DATE_LAST_VISITED + "," +
                qualifyColumn(TABLE_HISTORY, History.FAVICON_ID) + " AS " + Combined.FAVICON_ID + "," +

                qualifyColumn(TABLE_HISTORY, History.LOCAL_DATE_LAST_VISITED) + " AS " + Combined.LOCAL_DATE_LAST_VISITED + "," +
                qualifyColumn(TABLE_HISTORY, History.REMOTE_DATE_LAST_VISITED) + " AS " + Combined.REMOTE_DATE_LAST_VISITED + "," +
                qualifyColumn(TABLE_HISTORY, History.LOCAL_VISITS) + " AS " + Combined.LOCAL_VISITS_COUNT + "," +
                qualifyColumn(TABLE_HISTORY, History.REMOTE_VISITS) + " AS " + Combined.REMOTE_VISITS_COUNT +

                // We need to JOIN on Visits in order to compute visit counts
                " FROM " + TABLE_HISTORY + " " +

                // We really shouldn't be selecting deleted bookmarks, but oh well.
                "LEFT OUTER JOIN " + TABLE_BOOKMARKS +
                " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " = " + qualifyColumn(TABLE_HISTORY, History.URL) +
                " WHERE " +
                qualifyColumn(TABLE_HISTORY, History.IS_DELETED) + " = 0 AND " +
                "(" +
                // The left outer join didn't match...
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE) + " IS NULL OR " +

                // ... or it's a bookmark. This is less efficient than filtering prior
                // to the join if you have lots of folders.
                qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE) + " = " + Bookmarks.TYPE_BOOKMARK + ")"
        );

        debug("Creating " + VIEW_COMBINED_WITH_FAVICONS + " view");

        db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_COMBINED_WITH_FAVICONS + " AS" +
                " SELECT " + qualifyColumn(VIEW_COMBINED, "*") + ", " +
                qualifyColumn(TABLE_FAVICONS, Favicons.URL) + " AS " + Combined.FAVICON_URL + ", " +
                qualifyColumn(TABLE_FAVICONS, Favicons.DATA) + " AS " + Combined.FAVICON +
                " FROM " + VIEW_COMBINED + " LEFT OUTER JOIN " + TABLE_FAVICONS +
                " ON " + Combined.FAVICON_ID + " = " + qualifyColumn(TABLE_FAVICONS, Favicons._ID));
    }

    private void createLoginsTable(SQLiteDatabase db, final String tableName) {
        debug("Creating logins.db: " + db.getPath());
        debug("Creating " + tableName + " table");

        // Table for each login.
        db.execSQL("CREATE TABLE " + tableName + "(" +
                BrowserContract.Logins._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                BrowserContract.Logins.HOSTNAME + " TEXT NOT NULL," +
                BrowserContract.Logins.HTTP_REALM + " TEXT," +
                BrowserContract.Logins.FORM_SUBMIT_URL + " TEXT," +
                BrowserContract.Logins.USERNAME_FIELD + " TEXT NOT NULL," +
                BrowserContract.Logins.PASSWORD_FIELD + " TEXT NOT NULL," +
                BrowserContract.Logins.ENCRYPTED_USERNAME + " TEXT NOT NULL," +
                BrowserContract.Logins.ENCRYPTED_PASSWORD + " TEXT NOT NULL," +
                BrowserContract.Logins.GUID + " TEXT UNIQUE NOT NULL," +
                BrowserContract.Logins.ENC_TYPE + " INTEGER NOT NULL, " +
                BrowserContract.Logins.TIME_CREATED + " INTEGER," +
                BrowserContract.Logins.TIME_LAST_USED + " INTEGER," +
                BrowserContract.Logins.TIME_PASSWORD_CHANGED + " INTEGER," +
                BrowserContract.Logins.TIMES_USED + " INTEGER" +
                ");");
    }

    private void createLoginsTableIndices(SQLiteDatabase db, final String tableName) {
        // No need to create an index on GUID, it is an unique column.
        db.execSQL("CREATE INDEX " + LoginsProvider.INDEX_LOGINS_HOSTNAME +
                " ON " + tableName + "(" + BrowserContract.Logins.HOSTNAME + ")");
        db.execSQL("CREATE INDEX " + LoginsProvider.INDEX_LOGINS_HOSTNAME_FORM_SUBMIT_URL +
                " ON " + tableName + "(" + BrowserContract.Logins.HOSTNAME + "," + BrowserContract.Logins.FORM_SUBMIT_URL + ")");
        db.execSQL("CREATE INDEX " + LoginsProvider.INDEX_LOGINS_HOSTNAME_HTTP_REALM +
                " ON " + tableName + "(" + BrowserContract.Logins.HOSTNAME + "," + BrowserContract.Logins.HTTP_REALM + ")");
    }

    private void createDeletedLoginsTable(SQLiteDatabase db, final String tableName) {
        debug("Creating deleted_logins.db: " + db.getPath());
        debug("Creating " + tableName + " table");

        // Table for each deleted login.
        db.execSQL("CREATE TABLE " + tableName + "(" +
                BrowserContract.DeletedLogins._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                BrowserContract.DeletedLogins.GUID + " TEXT UNIQUE NOT NULL," +
                BrowserContract.DeletedLogins.TIME_DELETED + " INTEGER NOT NULL" +
                ");");
    }

    private void createDisabledHostsTable(SQLiteDatabase db, final String tableName) {
        debug("Creating disabled_hosts.db: " + db.getPath());
        debug("Creating " + tableName + " table");

        // Table for each disabled host.
        db.execSQL("CREATE TABLE " + tableName + "(" +
                BrowserContract.LoginsDisabledHosts._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                BrowserContract.LoginsDisabledHosts.HOSTNAME + " TEXT UNIQUE NOT NULL ON CONFLICT REPLACE" +
                ");");
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
        debug("Creating browser.db: " + db.getPath());

        for (Table table : BrowserProvider.sTables) {
            table.onCreate(db);
        }

        createBookmarksTable(db);
        createHistoryTable(db);
        createFaviconsTable(db);
        createThumbnailsTable(db);
        createClientsTable(db);
        createLocalClient(db);
        createTabsTable(db, TABLE_TABS);
        createTabsTableIndices(db, TABLE_TABS);


        createBookmarksWithFaviconsView(db);
        createHistoryWithFaviconsView(db);

        createOrUpdateSpecialFolder(db, Bookmarks.PLACES_FOLDER_GUID,
            R.string.bookmarks_folder_places, 0);

        createOrUpdateAllSpecialFolders(db);
        createSearchHistoryTable(db);
        createUrlAnnotationsTable(db);
        createNumbersTable(db);

        createDeletedLoginsTable(db, TABLE_DELETED_LOGINS);
        createDisabledHostsTable(db, TABLE_DISABLED_HOSTS);
        createLoginsTable(db, TABLE_LOGINS);
        createLoginsTableIndices(db, TABLE_LOGINS);

        createBookmarksWithAnnotationsView(db);

        createVisitsTable(db);
        createCombinedViewOn34(db);

        createActivityStreamBlocklistTable(db);

        createPageMetadataTable(db);
    }

    /**
     * Copies the tabs and clients tables out of the given tabs.db file and into the destinationDB.
     *
     * @param tabsDBFile Path to existing tabs.db.
     * @param destinationDB The destination database.
     */
    public void copyTabsDB(File tabsDBFile, SQLiteDatabase destinationDB) {
        createClientsTable(destinationDB);
        createTabsTable(destinationDB, TABLE_TABS);
        createTabsTableIndices(destinationDB, TABLE_TABS);

        SQLiteDatabase oldTabsDB = null;
        try {
            oldTabsDB = SQLiteDatabase.openDatabase(tabsDBFile.getPath(), null, SQLiteDatabase.OPEN_READONLY);

            if (!DBUtils.copyTable(oldTabsDB, TABLE_CLIENTS, destinationDB, TABLE_CLIENTS)) {
                Log.e(LOGTAG, "Failed to migrate table clients; ignoring.");
            }
            if (!DBUtils.copyTable(oldTabsDB, TABLE_TABS, destinationDB, TABLE_TABS)) {
                Log.e(LOGTAG, "Failed to migrate table tabs; ignoring.");
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception occurred while trying to copy from " + tabsDBFile.getPath() +
                    " to " + destinationDB.getPath() + "; ignoring.", e);
        } finally {
            if (oldTabsDB != null) {
                oldTabsDB.close();
            }
        }
    }

    /**
     * We used to have a separate history extensions database which was used by Sync to store arrays
     * of visits for individual History GUIDs. It was only used by Sync.
     * This function migrates contents of that database over to the Visits table.
     *
     * Warning to callers: this method might throw IllegalStateException if we fail to allocate a
     * cursor to read HistoryExtensionsDB data for whatever reason. See Bug 1280409.
     *
     * @param historyExtensionDb Source History Extensions database
     * @param db Destination database
     */
    private void copyHistoryExtensionDataToVisitsTable(final SQLiteDatabase historyExtensionDb, final SQLiteDatabase db) {
        final String historyExtensionTable = "HistoryExtension";
        final String columnGuid = "guid";
        final String columnVisits = "visits";

        final Cursor historyExtensionCursor = historyExtensionDb.query(historyExtensionTable,
                new String[] {columnGuid, columnVisits},
                null, null, null, null, null);
        // Ignore null or empty cursor, we can't (or have nothing to) copy at this point.
        if (historyExtensionCursor == null) {
            return;
        }
        try {
            if (!historyExtensionCursor.moveToFirst()) {
                return;
            }

            final int guidCol = historyExtensionCursor.getColumnIndexOrThrow(columnGuid);

            // Use prepared (aka "compiled") SQL statements because they are much faster when we're inserting
            // lots of data. We avoid GC churn and recompilation of SQL statements on every insert.
            // NB #1: OR IGNORE clause applies to UNIQUE, NOT NULL, CHECK, and PRIMARY KEY constraints.
            // It does not apply to Foreign Key constraints, but in our case, at this point in time, foreign key
            // constraints are disabled anyway.
            // We care about OR IGNORE because we want to ensure that in case of (GUID,DATE)
            // clash (the UNIQUE constraint), we will not fail the transaction, and just skip conflicting row.
            // Clash might occur if visits array we got from Sync has duplicate (guid,date) records.
            // NB #2: IS_LOCAL is always 0, since we consider all visits coming from Sync to be remote.
            final String insertSqlStatement = "INSERT OR IGNORE INTO " + Visits.TABLE_NAME + " (" +
                    Visits.DATE_VISITED + "," +
                    Visits.VISIT_TYPE + "," +
                    Visits.HISTORY_GUID + "," +
                    Visits.IS_LOCAL + ") VALUES (?, ?, ?, " + Visits.VISIT_IS_REMOTE + ")";
            final SQLiteStatement compiledInsertStatement = db.compileStatement(insertSqlStatement);

            do {
                final String guid = historyExtensionCursor.getString(guidCol);

                // Sanity check, let's not risk a bad incoming GUID.
                if (guid == null || guid.isEmpty()) {
                    continue;
                }

                // First, check if history with given GUID exists in the History table.
                // We might have a lot of entries in the HistoryExtensionDatabase whose GUID doesn't
                // match one in the History table. Let's avoid doing unnecessary work by first checking if
                // GUID exists locally.
                // Note that we don't have foreign key constraints enabled at this point.
                // See Bug 1266232 for details.
                if (!isGUIDPresentInHistoryTable(db, guid)) {
                    continue;
                }

                final JSONArray visitsInHistoryExtensionDB = RepoUtils.getJSONArrayFromCursor(historyExtensionCursor, columnVisits);

                if (visitsInHistoryExtensionDB == null) {
                    continue;
                }

                final int histExtVisitCount = visitsInHistoryExtensionDB.size();

                debug("Inserting " + histExtVisitCount + " visits from history extension db for GUID: " + guid);
                for (int i = 0; i < histExtVisitCount; i++) {
                    final JSONObject visit = (JSONObject) visitsInHistoryExtensionDB.get(i);

                    // Sanity check.
                    if (visit == null) {
                        continue;
                    }

                    // Let's not rely on underlying data being correct, and guard against casting failures.
                    // Since we can't recover from this (other than ignoring this visit), let's not fail user's migration.
                    final Long date;
                    final Long visitType;
                    try {
                        date = (Long) visit.get("date");
                        visitType = (Long) visit.get("type");
                    } catch (ClassCastException e) {
                        continue;
                    }
                    // Sanity check our incoming data.
                    if (date == null || visitType == null) {
                        continue;
                    }

                    // Bind parameters use a 1-based index.
                    compiledInsertStatement.clearBindings();
                    compiledInsertStatement.bindLong(1, date);
                    compiledInsertStatement.bindLong(2, visitType);
                    compiledInsertStatement.bindString(3, guid);
                    compiledInsertStatement.executeInsert();
                }
            } while (historyExtensionCursor.moveToNext());
        } finally {
            // We return on a null cursor, so don't have to check it here.
            historyExtensionCursor.close();
        }
    }

    private boolean isGUIDPresentInHistoryTable(final SQLiteDatabase db, String guid) {
        final Cursor historyCursor = db.query(
                History.TABLE_NAME,
                new String[] {History.GUID}, History.GUID + " = ?", new String[] {guid},
                null, null, null);
        if (historyCursor == null) {
            return false;
        }
        try {
            // No history record found for given GUID
            if (!historyCursor.moveToFirst()) {
                return false;
            }
        } finally {
            historyCursor.close();
        }

        return true;
    }

    private void createSearchHistoryTable(SQLiteDatabase db) {
        debug("Creating " + SearchHistory.TABLE_NAME + " table");

        db.execSQL("CREATE TABLE " + SearchHistory.TABLE_NAME + "(" +
                    SearchHistory._ID + " INTEGER PRIMARY KEY AUTOINCREMENT, " +
                    SearchHistory.QUERY + " TEXT UNIQUE NOT NULL, " +
                    SearchHistory.DATE_LAST_VISITED + " INTEGER, " +
                    SearchHistory.VISITS + " INTEGER ) ");

        db.execSQL("CREATE INDEX idx_search_history_last_visited ON " +
                SearchHistory.TABLE_NAME + "(" + SearchHistory.DATE_LAST_VISITED + ")");
    }

    private void createActivityStreamBlocklistTable(final SQLiteDatabase db) {
        debug("Creating " + ActivityStreamBlocklist.TABLE_NAME + " table");

        db.execSQL("CREATE TABLE " + ActivityStreamBlocklist.TABLE_NAME + "(" +
                   ActivityStreamBlocklist._ID + " INTEGER PRIMARY KEY AUTOINCREMENT, " +
                   ActivityStreamBlocklist.URL + " TEXT UNIQUE NOT NULL, " +
                   ActivityStreamBlocklist.CREATED + " INTEGER NOT NULL)");
    }

    private void createReadingListTable(final SQLiteDatabase db, final String tableName) {
        debug("Creating " + TABLE_READING_LIST + " table");

        db.execSQL("CREATE TABLE " + tableName + "(" +
                   ReadingListItems._ID + " INTEGER PRIMARY KEY AUTOINCREMENT, " +
                   ReadingListItems.GUID + " TEXT UNIQUE, " +                          // Server-assigned.

                   ReadingListItems.CONTENT_STATUS + " TINYINT NOT NULL DEFAULT " + ReadingListItems.STATUS_UNFETCHED + ", " +
                   ReadingListItems.SYNC_STATUS + " TINYINT NOT NULL DEFAULT " + ReadingListItems.SYNC_STATUS_NEW + ", " +
                   ReadingListItems.SYNC_CHANGE_FLAGS + " TINYINT NOT NULL DEFAULT " + ReadingListItems.SYNC_CHANGE_NONE + ", " +

                   ReadingListItems.CLIENT_LAST_MODIFIED + " INTEGER NOT NULL, " +     // Client time.
                   ReadingListItems.SERVER_LAST_MODIFIED + " INTEGER, " +              // Server-assigned.

                   // Server-assigned.
                   ReadingListItems.SERVER_STORED_ON + " INTEGER, " +
                   ReadingListItems.ADDED_ON + " INTEGER, " +                   // Client time. Shouldn't be null, but not enforced. Formerly DATE_CREATED.
                   ReadingListItems.MARKED_READ_ON + " INTEGER, " +

                   // These boolean flags represent the server 'status', 'unread', 'is_article', and 'favorite' fields.
                   ReadingListItems.IS_DELETED + " TINYINT NOT NULL DEFAULT 0, " +
                   ReadingListItems.IS_ARCHIVED + " TINYINT NOT NULL DEFAULT 0, " +
                   ReadingListItems.IS_UNREAD + " TINYINT NOT NULL DEFAULT 1, " +
                   ReadingListItems.IS_ARTICLE + " TINYINT NOT NULL DEFAULT 0, " +
                   ReadingListItems.IS_FAVORITE + " TINYINT NOT NULL DEFAULT 0, " +

                   ReadingListItems.URL + " TEXT NOT NULL, " +
                   ReadingListItems.TITLE + " TEXT, " +
                   ReadingListItems.RESOLVED_URL + " TEXT, " +
                   ReadingListItems.RESOLVED_TITLE + " TEXT, " +

                   ReadingListItems.EXCERPT + " TEXT, " +

                   ReadingListItems.ADDED_BY + " TEXT, " +
                   ReadingListItems.MARKED_READ_BY + " TEXT, " +

                   ReadingListItems.WORD_COUNT + " INTEGER DEFAULT 0, " +
                   ReadingListItems.READ_POSITION + " INTEGER DEFAULT 0 " +
                "); ");

        didCreateCurrentReadingListTable = true;      // Mostly correct, in the absence of transactions.
    }

    private void createReadingListIndices(final SQLiteDatabase db, final String tableName) {
        // No need to create an index on GUID; it's a UNIQUE column.
        db.execSQL("CREATE INDEX reading_list_url ON " + tableName + "("
                           + ReadingListItems.URL + ")");
        db.execSQL("CREATE INDEX reading_list_content_status ON " + tableName + "("
                           + ReadingListItems.CONTENT_STATUS + ")");
    }

    private void createUrlAnnotationsTable(final SQLiteDatabase db) {
        debug("Creating " + UrlAnnotations.TABLE_NAME + " table");

        db.execSQL("CREATE TABLE " + UrlAnnotations.TABLE_NAME + "(" +
                UrlAnnotations._ID + " INTEGER PRIMARY KEY AUTOINCREMENT, " +
                UrlAnnotations.URL + " TEXT NOT NULL, " +
                UrlAnnotations.KEY + " TEXT NOT NULL, " +
                UrlAnnotations.VALUE + " TEXT, " +
                UrlAnnotations.DATE_CREATED + " INTEGER NOT NULL, " +
                UrlAnnotations.DATE_MODIFIED + " INTEGER NOT NULL, " +
                UrlAnnotations.SYNC_STATUS + " TINYINT NOT NULL DEFAULT " + UrlAnnotations.SyncStatus.NEW.getDBValue() +
                " );");

        db.execSQL("CREATE INDEX idx_url_annotations_url_key ON " +
                UrlAnnotations.TABLE_NAME + "(" + UrlAnnotations.URL + ", " + UrlAnnotations.KEY + ")");
    }

    private void createOrUpdateAllSpecialFolders(SQLiteDatabase db) {
        createOrUpdateSpecialFolder(db, Bookmarks.MOBILE_FOLDER_GUID,
            R.string.bookmarks_folder_mobile, 0);
        createOrUpdateSpecialFolder(db, Bookmarks.TOOLBAR_FOLDER_GUID,
            R.string.bookmarks_folder_toolbar, 1);
        createOrUpdateSpecialFolder(db, Bookmarks.MENU_FOLDER_GUID,
            R.string.bookmarks_folder_menu, 2);
        createOrUpdateSpecialFolder(db, Bookmarks.TAGS_FOLDER_GUID,
            R.string.bookmarks_folder_tags, 3);
        createOrUpdateSpecialFolder(db, Bookmarks.UNFILED_FOLDER_GUID,
            R.string.bookmarks_folder_unfiled, 4);
        createOrUpdateSpecialFolder(db, Bookmarks.PINNED_FOLDER_GUID,
            R.string.bookmarks_folder_pinned, 5);
    }

    private void createOrUpdateSpecialFolder(SQLiteDatabase db,
            String guid, int titleId, int position) {
        ContentValues values = new ContentValues();
        values.put(Bookmarks.GUID, guid);
        values.put(Bookmarks.TYPE, Bookmarks.TYPE_FOLDER);
        values.put(Bookmarks.POSITION, position);

        if (guid.equals(Bookmarks.PLACES_FOLDER_GUID)) {
            values.put(Bookmarks._ID, Bookmarks.FIXED_ROOT_ID);
        } else if (guid.equals(Bookmarks.PINNED_FOLDER_GUID)) {
            values.put(Bookmarks._ID, Bookmarks.FIXED_PINNED_LIST_ID);
        }

        // Set the parent to 0, which sync assumes is the root
        values.put(Bookmarks.PARENT, Bookmarks.FIXED_ROOT_ID);

        String title = mContext.getResources().getString(titleId);
        values.put(Bookmarks.TITLE, title);

        long now = System.currentTimeMillis();
        values.put(Bookmarks.DATE_CREATED, now);
        values.put(Bookmarks.DATE_MODIFIED, now);

        int updated = db.update(TABLE_BOOKMARKS, values,
                                Bookmarks.GUID + " = ?",
                                new String[] { guid });

        if (updated == 0) {
            db.insert(TABLE_BOOKMARKS, Bookmarks.GUID, values);
            debug("Inserted special folder: " + guid);
        } else {
            debug("Updated special folder: " + guid);
        }
    }

    private void createNumbersTable(SQLiteDatabase db) {
        db.execSQL("CREATE TABLE " + Numbers.TABLE_NAME + " (" + Numbers.POSITION + " INTEGER PRIMARY KEY AUTOINCREMENT)");

        if (db.getVersion() >= 3007011) { // SQLite 3.7.11
            // This is only available in SQLite >= 3.7.11, see release notes:
            // "Enhance the INSERT syntax to allow multiple rows to be inserted via the VALUES clause"
            final String numbers = "(0),(1),(2),(3),(4),(5),(6),(7),(8),(9)," +
                    "(10),(11),(12),(13),(14),(15),(16),(17),(18),(19)," +
                    "(20),(21),(22),(23),(24),(25),(26),(27),(28),(29)," +
                    "(30),(31),(32),(33),(34),(35),(36),(37),(38),(39)," +
                    "(40),(41),(42),(43),(44),(45),(46),(47),(48),(49)," +
                    "(50)";

            db.execSQL("INSERT INTO " + Numbers.TABLE_NAME + " (" + Numbers.POSITION + ") VALUES " + numbers);
        } else {
            final SQLiteStatement statement = db.compileStatement("INSERT INTO " + Numbers.TABLE_NAME + " (" + Numbers.POSITION + ") VALUES (?)");

            for (int i = 0; i <= Numbers.MAX_VALUE; i++) {
                statement.bindLong(1, i);
                statement.executeInsert();
            }
        }
    }

    private boolean isSpecialFolder(ContentValues values) {
        String guid = values.getAsString(Bookmarks.GUID);
        if (guid == null) {
            return false;
        }

        return guid.equals(Bookmarks.MOBILE_FOLDER_GUID) ||
               guid.equals(Bookmarks.MENU_FOLDER_GUID) ||
               guid.equals(Bookmarks.TOOLBAR_FOLDER_GUID) ||
               guid.equals(Bookmarks.UNFILED_FOLDER_GUID) ||
               guid.equals(Bookmarks.TAGS_FOLDER_GUID);
    }

    private void migrateBookmarkFolder(SQLiteDatabase db, int folderId,
            BookmarkMigrator migrator) {
        Cursor c = null;

        debug("Migrating bookmark folder with id = " + folderId);

        String selection = Bookmarks.PARENT + " = " + folderId;
        String[] selectionArgs = null;

        boolean isRootFolder = (folderId == Bookmarks.FIXED_ROOT_ID);

        // If we're loading the root folder, we have to account for
        // any previously created special folder that was created without
        // setting a parent id (e.g. mobile folder) and making sure we're
        // not adding any infinite recursion as root's parent is root itself.
        if (isRootFolder) {
            selection = Bookmarks.GUID + " != ?" + " AND (" +
                        selection + " OR " + Bookmarks.PARENT + " = NULL)";
            selectionArgs = new String[] { Bookmarks.PLACES_FOLDER_GUID };
        }

        List<Integer> subFolders = new ArrayList<Integer>();
        List<ContentValues> invalidSpecialEntries = new ArrayList<ContentValues>();

        try {
            c = db.query(TABLE_BOOKMARKS_TMP,
                         null,
                         selection,
                         selectionArgs,
                         null, null, null);

            // The key point here is that bookmarks should be added in
            // parent order to avoid any problems with the foreign key
            // in Bookmarks.PARENT.
            while (c.moveToNext()) {
                ContentValues values = new ContentValues();

                // We're using a null projection in the query which
                // means we're getting all columns from the table.
                // It's safe to simply transform the row into the
                // values to be inserted on the new table.
                DatabaseUtils.cursorRowToContentValues(c, values);

                boolean isSpecialFolder = isSpecialFolder(values);

                // The mobile folder used to be created with PARENT = NULL.
                // We want fix that here.
                if (values.getAsLong(Bookmarks.PARENT) == null && isSpecialFolder)
                    values.put(Bookmarks.PARENT, Bookmarks.FIXED_ROOT_ID);

                if (isRootFolder && !isSpecialFolder) {
                    invalidSpecialEntries.add(values);
                    continue;
                }

                if (migrator != null)
                    migrator.updateForNewTable(values);

                debug("Migrating bookmark: " + values.getAsString(Bookmarks.TITLE));
                db.insert(TABLE_BOOKMARKS, Bookmarks.URL, values);

                Integer type = values.getAsInteger(Bookmarks.TYPE);
                if (type != null && type == Bookmarks.TYPE_FOLDER)
                    subFolders.add(values.getAsInteger(Bookmarks._ID));
            }
        } finally {
            if (c != null)
                c.close();
        }

        // At this point is safe to assume that the mobile folder is
        // in the new table given that we've always created it on
        // database creation time.
        final int nInvalidSpecialEntries = invalidSpecialEntries.size();
        if (nInvalidSpecialEntries > 0) {
            Integer mobileFolderId = getMobileFolderId(db);
            if (mobileFolderId == null) {
                Log.e(LOGTAG, "Error migrating invalid special folder entries: mobile folder id is null");
                return;
            }

            debug("Found " + nInvalidSpecialEntries + " invalid special folder entries");
            for (int i = 0; i < nInvalidSpecialEntries; i++) {
                ContentValues values = invalidSpecialEntries.get(i);
                values.put(Bookmarks.PARENT, mobileFolderId);

                db.insert(TABLE_BOOKMARKS, Bookmarks.URL, values);
            }
        }

        final int nSubFolders = subFolders.size();
        for (int i = 0; i < nSubFolders; i++) {
            int subFolderId = subFolders.get(i);
            migrateBookmarkFolder(db, subFolderId, migrator);
        }
    }

    private void migrateBookmarksTable(SQLiteDatabase db) {
        migrateBookmarksTable(db, null);
    }

    private void migrateBookmarksTable(SQLiteDatabase db, BookmarkMigrator migrator) {
        debug("Renaming bookmarks table to " + TABLE_BOOKMARKS_TMP);
        db.execSQL("ALTER TABLE " + TABLE_BOOKMARKS +
                   " RENAME TO " + TABLE_BOOKMARKS_TMP);

        debug("Dropping views and indexes related to " + TABLE_BOOKMARKS);

        db.execSQL("DROP INDEX IF EXISTS bookmarks_url_index");
        db.execSQL("DROP INDEX IF EXISTS bookmarks_type_deleted_index");
        db.execSQL("DROP INDEX IF EXISTS bookmarks_guid_index");
        db.execSQL("DROP INDEX IF EXISTS bookmarks_modified_index");

        createBookmarksTable(db);

        createOrUpdateSpecialFolder(db, Bookmarks.PLACES_FOLDER_GUID,
            R.string.bookmarks_folder_places, 0);

        migrateBookmarkFolder(db, Bookmarks.FIXED_ROOT_ID, migrator);

        // Ensure all special folders exist and have the
        // right folder hierarchy.
        createOrUpdateAllSpecialFolders(db);

        debug("Dropping bookmarks temporary table");
        db.execSQL("DROP TABLE IF EXISTS " + TABLE_BOOKMARKS_TMP);
    }

    /**
     * Migrate a history table from some old version to the newest one by creating the new table and
     * copying all the data over.
     */
    private void migrateHistoryTable(SQLiteDatabase db) {
        debug("Renaming history table to " + TABLE_HISTORY_TMP);
        db.execSQL("ALTER TABLE " + TABLE_HISTORY +
                   " RENAME TO " + TABLE_HISTORY_TMP);

        debug("Dropping views and indexes related to " + TABLE_HISTORY);

        db.execSQL("DROP INDEX IF EXISTS history_url_index");
        db.execSQL("DROP INDEX IF EXISTS history_guid_index");
        db.execSQL("DROP INDEX IF EXISTS history_modified_index");
        db.execSQL("DROP INDEX IF EXISTS history_visited_index");

        createHistoryTable(db);

        db.execSQL("INSERT INTO " + TABLE_HISTORY + " SELECT * FROM " + TABLE_HISTORY_TMP);

        debug("Dropping history temporary table");
        db.execSQL("DROP TABLE IF EXISTS " + TABLE_HISTORY_TMP);
    }

    private void upgradeDatabaseFrom3to4(SQLiteDatabase db) {
        migrateBookmarksTable(db, new BookmarkMigrator3to4());
    }

    private void upgradeDatabaseFrom6to7(SQLiteDatabase db) {
        debug("Removing history visits with NULL GUIDs");
        db.execSQL("DELETE FROM " + TABLE_HISTORY + " WHERE " + History.GUID + " IS NULL");

        migrateBookmarksTable(db);
        migrateHistoryTable(db);
    }

    private void upgradeDatabaseFrom7to8(SQLiteDatabase db) {
        debug("Combining history entries with the same URL");

        final String TABLE_DUPES = "duped_urls";
        final String TOTAL = "total";
        final String LATEST = "latest";
        final String WINNER = "winner";

        db.execSQL("CREATE TEMP TABLE " + TABLE_DUPES + " AS" +
                  " SELECT " + History.URL + ", " +
                              "SUM(" + History.VISITS + ") AS " + TOTAL + ", " +
                              "MAX(" + History.DATE_MODIFIED + ") AS " + LATEST + ", " +
                              "MAX(" + History._ID + ") AS " + WINNER +
                  " FROM " + TABLE_HISTORY +
                  " GROUP BY " + History.URL +
                  " HAVING count(" + History.URL + ") > 1");

        db.execSQL("CREATE UNIQUE INDEX " + TABLE_DUPES + "_url_index ON " +
                   TABLE_DUPES + " (" + History.URL + ")");

        final String fromClause = " FROM " + TABLE_DUPES + " WHERE " +
                                  qualifyColumn(TABLE_DUPES, History.URL) + " = " +
                                  qualifyColumn(TABLE_HISTORY, History.URL);

        db.execSQL("UPDATE " + TABLE_HISTORY +
                  " SET " + History.VISITS + " = (SELECT " + TOTAL + fromClause + "), " +
                            History.DATE_MODIFIED + " = (SELECT " + LATEST + fromClause + "), " +
                            History.IS_DELETED + " = " +
                                "(" + History._ID + " <> (SELECT " + WINNER + fromClause + "))" +
                  " WHERE " + History.URL + " IN (SELECT " + History.URL + " FROM " + TABLE_DUPES + ")");

        db.execSQL("DROP TABLE " + TABLE_DUPES);
    }

    private void upgradeDatabaseFrom10to11(SQLiteDatabase db) {
        db.execSQL("CREATE INDEX bookmarks_type_deleted_index ON " + TABLE_BOOKMARKS + "("
                + Bookmarks.TYPE + ", " + Bookmarks.IS_DELETED + ")");
    }

    private void upgradeDatabaseFrom12to13(SQLiteDatabase db) {
        createFaviconsTable(db);

        // Add favicon_id column to the history/bookmarks tables. We wrap this in a try-catch
        // because the column *may* already exist at this point (depending on how many upgrade
        // steps have been performed in this operation). In which case these queries will throw,
        // but we don't care.
        try {
            db.execSQL("ALTER TABLE " + TABLE_HISTORY +
                                    " ADD COLUMN " + History.FAVICON_ID + " INTEGER");
            db.execSQL("ALTER TABLE " + TABLE_BOOKMARKS +
                               " ADD COLUMN " + Bookmarks.FAVICON_ID + " INTEGER");
        } catch (SQLException e) {
            // Don't care.
            debug("Exception adding favicon_id column. We're probably fine." + e);
        }

        createThumbnailsTable(db);

        db.execSQL("DROP VIEW IF EXISTS bookmarks_with_images");
        db.execSQL("DROP VIEW IF EXISTS history_with_images");
        db.execSQL("DROP VIEW IF EXISTS combined_with_images");

        createBookmarksWithFaviconsView(db);
        createHistoryWithFaviconsView(db);

        db.execSQL("DROP TABLE IF EXISTS images");
    }

    private void upgradeDatabaseFrom13to14(SQLiteDatabase db) {
        createOrUpdateSpecialFolder(db, Bookmarks.PINNED_FOLDER_GUID,
            R.string.bookmarks_folder_pinned, 6);
    }

    private void upgradeDatabaseFrom14to15(SQLiteDatabase db) {
        Cursor c = null;
        try {
            // Get all the pinned bookmarks
            c = db.query(TABLE_BOOKMARKS,
                         new String[] { Bookmarks._ID, Bookmarks.URL },
                         Bookmarks.PARENT + " = ?",
                         new String[] { Integer.toString(Bookmarks.FIXED_PINNED_LIST_ID) },
                         null, null, null);

            while (c.moveToNext()) {
                // Check if this URL can be parsed as a URI with a valid scheme.
                String url = c.getString(c.getColumnIndexOrThrow(Bookmarks.URL));
                if (Uri.parse(url).getScheme() != null) {
                    continue;
                }

                // If it can't, update the URL to be an encoded "user-entered" value.
                ContentValues values = new ContentValues(1);
                String newUrl = Uri.fromParts("user-entered", url, null).toString();
                values.put(Bookmarks.URL, newUrl);
                db.update(TABLE_BOOKMARKS, values, Bookmarks._ID + " = ?",
                          new String[] { Integer.toString(c.getInt(c.getColumnIndexOrThrow(Bookmarks._ID))) });
            }
        } finally {
            if (c != null) {
                c.close();
            }
        }
    }

    private void upgradeDatabaseFrom15to16(SQLiteDatabase db) {
        // No harm in creating the v19 combined view here: means we don't need two almost-identical
        // functions to define both the v16 and v19 ones. The upgrade path will redundantly drop
        // and recreate the view again. *shrug*
        createV19CombinedView(db);
    }

    private void upgradeDatabaseFrom16to17(SQLiteDatabase db) {
        // Purge any 0-byte favicons/thumbnails
        try {
            db.execSQL("DELETE FROM " + TABLE_FAVICONS +
                    " WHERE length(" + Favicons.DATA + ") = 0");
            db.execSQL("DELETE FROM " + TABLE_THUMBNAILS +
                    " WHERE length(" + Thumbnails.DATA + ") = 0");
        } catch (SQLException e) {
            Log.e(LOGTAG, "Error purging invalid favicons or thumbnails", e);
        }
    }

    /*
     * Moves reading list items from 'bookmarks' table to 'reading_list' table.
     */
    private void upgradeDatabaseFrom17to18(SQLiteDatabase db) {
        debug("Moving reading list items from 'bookmarks' table to 'reading_list' table");

        final String selection = Bookmarks.PARENT + " = ? AND " + Bookmarks.IS_DELETED + " = ? ";
        final String[] selectionArgs = { String.valueOf(Bookmarks.FIXED_READING_LIST_ID), "0" };
        final String[] projection = {   Bookmarks._ID,
                                        Bookmarks.GUID,
                                        Bookmarks.URL,
                                        Bookmarks.DATE_MODIFIED,
                                        Bookmarks.DATE_CREATED,
                                        Bookmarks.TITLE };

        try {
            db.beginTransaction();

            // Create 'reading_list' table.
            createReadingListTable(db, TABLE_READING_LIST);

            // Get all the reading list items from bookmarks table.
            final Cursor cursor = db.query(TABLE_BOOKMARKS, projection, selection, selectionArgs, null, null, null);

            if (cursor == null) {
                // This should never happen.
                db.setTransactionSuccessful();
                return;
            }

            try {
                // Insert reading list items into reading_list table.
                while (cursor.moveToNext()) {
                    debug(DatabaseUtils.dumpCurrentRowToString(cursor));
                    final ContentValues values = new ContentValues();

                    // We don't preserve bookmark GUIDs.
                    DatabaseUtils.cursorStringToContentValues(cursor, Bookmarks.URL, values, ReadingListItems.URL);
                    DatabaseUtils.cursorStringToContentValues(cursor, Bookmarks.TITLE, values, ReadingListItems.TITLE);
                    DatabaseUtils.cursorLongToContentValues(cursor, Bookmarks.DATE_CREATED, values, ReadingListItems.ADDED_ON);
                    DatabaseUtils.cursorLongToContentValues(cursor, Bookmarks.DATE_MODIFIED, values, ReadingListItems.CLIENT_LAST_MODIFIED);

                    db.insertOrThrow(TABLE_READING_LIST, null, values);
                }
            } finally {
                cursor.close();
            }

            // Delete reading list items from bookmarks table.
            db.delete(TABLE_BOOKMARKS,
                      Bookmarks.PARENT + " = ? ",
                      new String[] { String.valueOf(Bookmarks.FIXED_READING_LIST_ID) });

            // Delete reading list special folder.
            db.delete(TABLE_BOOKMARKS,
                      Bookmarks._ID + " = ? ",
                      new String[] { String.valueOf(Bookmarks.FIXED_READING_LIST_ID) });

            // Create indices.
            createReadingListIndices(db, TABLE_READING_LIST);

            // Done.
            db.setTransactionSuccessful();
        } catch (SQLException e) {
            Log.e(LOGTAG, "Error migrating reading list items", e);
        } finally {
            db.endTransaction();
        }
    }

    private void upgradeDatabaseFrom18to19(SQLiteDatabase db) {
        // Redefine the "combined" view...
        createV19CombinedView(db);

        // Kill any history entries with NULL URL. This ostensibly can't happen...
        db.execSQL("DELETE FROM " + TABLE_HISTORY + " WHERE " + History.URL + " IS NULL");

        // Similar for bookmark types. Replaces logic from the combined view, also shouldn't happen.
        db.execSQL("UPDATE " + TABLE_BOOKMARKS + " SET " +
                   Bookmarks.TYPE + " = " + Bookmarks.TYPE_BOOKMARK +
                   " WHERE " + Bookmarks.TYPE + " IS NULL");
    }

    private void upgradeDatabaseFrom19to20(SQLiteDatabase db) {
        createSearchHistoryTable(db);
    }

    private void upgradeDatabaseFrom21to22(SQLiteDatabase db) {
        if (didCreateCurrentReadingListTable) {
            debug("No need to add CONTENT_STATUS to reading list; we just created with the current schema.");
            return;
        }

        debug("Adding CONTENT_STATUS column to reading list table.");

        try {
            db.execSQL("ALTER TABLE " + TABLE_READING_LIST +
                       " ADD COLUMN " + ReadingListItems.CONTENT_STATUS +
                       " TINYINT DEFAULT " + ReadingListItems.STATUS_UNFETCHED);

            db.execSQL("CREATE INDEX reading_list_content_status ON " + TABLE_READING_LIST + "("
                    + ReadingListItems.CONTENT_STATUS + ")");
        } catch (SQLiteException e) {
            // We're betting that an error here means that the table already has the column,
            // so we're failing due to the duplicate column name.
            Log.e(LOGTAG, "Error upgrading database from 21 to 22", e);
        }
    }

    private void upgradeDatabaseFrom22to23(SQLiteDatabase db) {
        if (didCreateCurrentReadingListTable) {
            // If we just created this table it is already in the expected >= 23 schema. Trying
            // to run this migration will crash because columns that were in the <= 22 schema
            // no longer exist.
            debug("No need to rev reading list schema; we just created with the current schema.");
            return;
        }

        debug("Rewriting reading list table.");
        createReadingListTable(db, "tmp_rl");

        // Remove indexes. We don't need them now, and we'll be throwing away the table.
        db.execSQL("DROP INDEX IF EXISTS reading_list_url");
        db.execSQL("DROP INDEX IF EXISTS reading_list_guid");
        db.execSQL("DROP INDEX IF EXISTS reading_list_content_status");

        // This used to be a part of the no longer existing ReadingListProvider, since we're deleting
        // this table later in the second migration, and since sync for this table never existed,
        // we don't care about the device name here.
        final String thisDevice = "_fake_device_name_that_will_be_discarded_in_the_next_migration_";
        db.execSQL("INSERT INTO tmp_rl (" +
                   // Here are the columns we can preserve.
                   ReadingListItems._ID + ", " +
                   ReadingListItems.URL + ", " +
                   ReadingListItems.TITLE + ", " +
                   ReadingListItems.RESOLVED_TITLE + ", " +       // = TITLE (if CONTENT_STATUS = STATUS_FETCHED_ARTICLE)
                   ReadingListItems.RESOLVED_URL + ", " +         // = URL (if CONTENT_STATUS = STATUS_FETCHED_ARTICLE)
                   ReadingListItems.EXCERPT + ", " +
                   ReadingListItems.IS_UNREAD + ", " +            // = !READ
                   ReadingListItems.IS_DELETED + ", " +           // = 0
                   ReadingListItems.GUID + ", " +                 // = NULL
                   ReadingListItems.CLIENT_LAST_MODIFIED + ", " + // = DATE_MODIFIED
                   ReadingListItems.ADDED_ON + ", " +             // = DATE_CREATED
                   ReadingListItems.CONTENT_STATUS + ", " +
                   ReadingListItems.MARKED_READ_BY + ", " +       // if READ + ", = this device
                   ReadingListItems.ADDED_BY +                    // = this device
                   ") " +
                   "SELECT " +
                   "_id, url, title, " +
                   "CASE content_status WHEN " + ReadingListItems.STATUS_FETCHED_ARTICLE + " THEN title ELSE NULL END, " +   // RESOLVED_TITLE.
                   "CASE content_status WHEN " + ReadingListItems.STATUS_FETCHED_ARTICLE + " THEN url ELSE NULL END, " +     // RESOLVED_URL.
                   "excerpt, " +
                   "CASE read WHEN 1 THEN 0 ELSE 1 END, " +            // IS_UNREAD.
                   "0, " +                                             // IS_DELETED.
                   "NULL, modified, created, content_status, " +
                   "CASE read WHEN 1 THEN ? ELSE NULL END, " +         // MARKED_READ_BY.
                   "?" +                                               // ADDED_BY.
                   " FROM " + TABLE_READING_LIST +
                   " WHERE deleted = 0",
                   new String[] {thisDevice, thisDevice});

        // Now switch these tables over and recreate the indices.
        db.execSQL("DROP TABLE " + TABLE_READING_LIST);
        db.execSQL("ALTER TABLE tmp_rl RENAME TO " + TABLE_READING_LIST);

        createReadingListIndices(db, TABLE_READING_LIST);
    }

    private void upgradeDatabaseFrom23to24(SQLiteDatabase db) {
        // Version 24 consolidates the tabs and clients table into browser.db.  Before, they lived in tabs.db.
        // It's easier to copy the existing data than to arrange for Sync to re-populate it.
        try {
            final File oldTabsDBFile = new File(GeckoProfile.get(mContext).getDir(), "tabs.db");
            copyTabsDB(oldTabsDBFile, db);
        } catch (Exception e) {
            Log.e(LOGTAG, "Got exception copying tabs and clients data from tabs.db to browser.db; ignoring.", e);
        }

        // Delete the database, the shared memory, and the log.
        for (String filename : new String[] { "tabs.db", "tabs.db-shm", "tabs.db-wal" }) {
            final File file = new File(GeckoProfile.get(mContext).getDir(), filename);
            try {
                FileUtils.delete(file);
            } catch (Exception e) {
                Log.e(LOGTAG, "Exception occurred while trying to delete " + file.getPath() + "; ignoring.", e);
            }
        }
    }

    private void upgradeDatabaseFrom24to25(SQLiteDatabase db) {
        if (didCreateTabsTable) {
            // This migration adds a foreign key constraint (the table scheme stays identical, except
            // for the new constraint) - hence it is safe to run this migration on a newly created tabs
            // table - but it's unnecessary hence we should avoid doing so.
            debug("No need to rev tabs schema; foreign key constraint exists.");
            return;
        }

        debug("Rewriting tabs table.");
        createTabsTable(db, "tmp_tabs");

        // Remove indexes. We don't need them now, and we'll be throwing away the table.
        db.execSQL("DROP INDEX IF EXISTS " + TabsProvider.INDEX_TABS_GUID);
        db.execSQL("DROP INDEX IF EXISTS " + TabsProvider.INDEX_TABS_POSITION);

        db.execSQL("INSERT INTO tmp_tabs (" +
                        // Here are the columns we can preserve.
                        BrowserContract.Tabs._ID + ", " +
                        BrowserContract.Tabs.CLIENT_GUID + ", " +
                        BrowserContract.Tabs.TITLE + ", " +
                        BrowserContract.Tabs.URL + ", " +
                        BrowserContract.Tabs.HISTORY + ", " +
                        BrowserContract.Tabs.FAVICON + ", " +
                        BrowserContract.Tabs.LAST_USED + ", " +
                        BrowserContract.Tabs.POSITION +
                        ") " +
                        "SELECT " +
                        "_id, client_guid, title, url, history, favicon, last_used, position" +
                        " FROM " + TABLE_TABS);

        // Now switch these tables over and recreate the indices.
        db.execSQL("DROP TABLE " + TABLE_TABS);
        db.execSQL("ALTER TABLE tmp_tabs RENAME TO " + TABLE_TABS);
        createTabsTableIndices(db, TABLE_TABS);
        didCreateTabsTable = true;
    }

    private void upgradeDatabaseFrom25to26(SQLiteDatabase db) {
        debug("Dropping unnecessary indices");
        db.execSQL("DROP INDEX IF EXISTS clients_guid_index");
        db.execSQL("DROP INDEX IF EXISTS thumbnails_url_index");
        db.execSQL("DROP INDEX IF EXISTS favicons_url_index");
    }

    private void upgradeDatabaseFrom27to28(final SQLiteDatabase db) {
        debug("Adding url annotations table");
        createUrlAnnotationsTable(db);
    }

    private void upgradeDatabaseFrom28to29(SQLiteDatabase db) {
        debug("Adding numbers table");
        createNumbersTable(db);
    }

    private void upgradeDatabaseFrom29to30(final SQLiteDatabase db) {
        debug("creating logins table");
        createDeletedLoginsTable(db, TABLE_DELETED_LOGINS);
        createDisabledHostsTable(db, TABLE_DISABLED_HOSTS);
        createLoginsTable(db, TABLE_LOGINS);
        createLoginsTableIndices(db, TABLE_LOGINS);
    }

    // Get the cache path for a URL, based on the storage format in place during the 27to28 transition.
    // This is a reimplementation of _toHashedPath from ReaderMode.jsm - given that we're likely
    // to migrate the SavedReaderViewHelper implementation at some point, it seems safest to have a local
    // implementation here - moreover this is probably faster than calling into JS.
    // This is public only to allow for testing.
    @RobocopTarget
    public static String getReaderCacheFileNameForURL(String url) {
        try {
            // On KitKat and above we can use java.nio.charset.StandardCharsets.UTF_8 in place of "UTF8"
            // which avoids having to handle UnsupportedCodingException
            byte[] utf8 = url.getBytes("UTF8");

            final MessageDigest digester = MessageDigest.getInstance("MD5");
            byte[] hash = digester.digest(utf8);

            final String hashString = new Base32().encodeAsString(hash);
            return hashString.substring(0, hashString.indexOf('=')) + ".json";
        } catch (UnsupportedEncodingException e) {
            // This should never happen
            throw new IllegalStateException("UTF8 encoding not available - can't process readercache filename");
        } catch (NoSuchAlgorithmException e) {
            // This should also never happen
            throw new IllegalStateException("MD5 digester unavailable - can't process readercache filename");
        }
    }

    /*
     * Moves reading list items from the 'reading_list' table back into the 'bookmarks' table. This time the
     * reading list items are placed into a "Reading List" folder, which is a subfolder of the mobile-bookmarks table.
     */
    private void upgradeDatabaseFrom30to31(SQLiteDatabase db) {
        // We only need to do the migration if reading-list items already exist. We could do a query of count(*) on
        // TABLE_READING_LIST, however if we are doing the migration, we'll need to query all items in the reading-list,
        // hence we might as well just query all items, and proceed with the migration if cursor.count > 0.

        // We try to retain the original ordering below. Our LocalReadingListAccessor actually coalesced
        // SERVER_STORED_ON with ADDED_ON to determine positioning, however reading list syncing was never
        // implemented hence SERVER_STORED will have always been null.
        final Cursor readingListCursor = db.query(TABLE_READING_LIST,
                                     new String[] {
                                             ReadingListItems.URL,
                                             ReadingListItems.TITLE,
                                             ReadingListItems.ADDED_ON,
                                             ReadingListItems.CLIENT_LAST_MODIFIED
                                     },
                                     ReadingListItems.IS_DELETED + " = 0",
                                     null,
                                     null,
                                     null,
                                     ReadingListItems.ADDED_ON + " DESC");

        // We'll want to walk the cache directory, so that we can (A) bookkeep readercache items
        // that we want and (B) delete unneeded readercache items. (B) shouldn't actually happen, but
        // is possible if there were bugs in our reader-caching code.
        // We need to construct this here since we populate this map while walking the DB cursor,
        // and use the map later when walking the cache.
        final Map<String, String> fileToURLMap = new HashMap<>();


        try {
            if (!readingListCursor.moveToFirst()) {
                return;
            }

            final Integer mobileBookmarksID = getMobileFolderId(db);

            if (mobileBookmarksID == null) {
                // This folder is created either on DB creation or during the 3-4 or 6-7 migrations.
                throw new IllegalStateException("mobile bookmarks folder must already exist");
            }

            final long now = System.currentTimeMillis();

            // We try to retain the same order as the reading-list would show. We should hopefully be reading the
            // items in the order they are displayed on screen (final param of db.query above), by providing
            // a position we should obtain the same ordering in the bookmark folder.
            long position = 0;

            final int titleColumnID = readingListCursor.getColumnIndexOrThrow(ReadingListItems.TITLE);
            final int createdColumnID = readingListCursor.getColumnIndexOrThrow(ReadingListItems.ADDED_ON);

            // This isn't the most efficient implementation, but the migration is one-off, and this
            // also more maintainable than the SQL equivalent (generating the guids correctly is
            // difficult in SQLite).
            do {
                final ContentValues readingListItemValues = new ContentValues();

                final String url = readingListCursor.getString(readingListCursor.getColumnIndexOrThrow(ReadingListItems.URL));

                readingListItemValues.put(Bookmarks.PARENT, mobileBookmarksID);
                readingListItemValues.put(Bookmarks.GUID, Utils.generateGuid());
                readingListItemValues.put(Bookmarks.URL, url);
                // Title may be null, however we're expecting a String - we can generate an empty string if needed:
                if (!readingListCursor.isNull(titleColumnID)) {
                    readingListItemValues.put(Bookmarks.TITLE, readingListCursor.getString(titleColumnID));
                } else {
                    readingListItemValues.put(Bookmarks.TITLE, "");
                }
                readingListItemValues.put(Bookmarks.DATE_CREATED, readingListCursor.getLong(createdColumnID));
                readingListItemValues.put(Bookmarks.DATE_MODIFIED, now);
                readingListItemValues.put(Bookmarks.POSITION, position);

                db.insert(TABLE_BOOKMARKS,
                          null,
                          readingListItemValues);

                final String cacheFileName = getReaderCacheFileNameForURL(url);
                fileToURLMap.put(cacheFileName, url);

                position++;
            } while (readingListCursor.moveToNext());

        } finally {
            readingListCursor.close();
            // We need to do this work here since we might be returning (we return early if the
            // reading-list table is empty).
            db.execSQL("DROP TABLE IF EXISTS " + TABLE_READING_LIST);
            createBookmarksWithAnnotationsView(db);
        }

        final File profileDir = GeckoProfile.get(mContext).getDir();
        final File cacheDir = new File(profileDir, "readercache");

        // At the time of this migration the SavedReaderViewHelper becomes a 1:1 mirror of reader view
        // url-annotations. This may change in future implementations, however currently we only need to care
        // about standard bookmarks (untouched during this migration) and bookmarks with a reader
        // view annotation (which we're creating here, and which are guaranteed to be saved offline).
        //
        // This is why we have to migrate the cache items (instead of cleaning the cache
        // and rebuilding it). We simply don't support uncached reader view bookmarks, and we would
        // break existing reading list items (they would convert into plain bookmarks without
        // reader view). This helps ensure that offline content isn't lost during the migration.
        if (cacheDir.exists() && cacheDir.isDirectory()) {
            SavedReaderViewHelper savedReaderViewHelper = SavedReaderViewHelper.getSavedReaderViewHelper(mContext);

            // Usually we initialise the helper during onOpen(). However onUpgrade() is run before
            // onOpen() hence we need to manually initialise it at this stage.
            savedReaderViewHelper.loadItems();

            for (File cacheFile : cacheDir.listFiles()) {
                if (fileToURLMap.containsKey(cacheFile.getName())) {
                    final String url = fileToURLMap.get(cacheFile.getName());
                    final String path = cacheFile.getAbsolutePath();
                    long size = cacheFile.length();

                    savedReaderViewHelper.put(url, path, size);
                } else {
                    // This should never happen, but we don't actually know whether or not orphaned
                    // items happened in the wild.
                    boolean deleted = cacheFile.delete();

                    if (!deleted) {
                        Log.w(LOGTAG, "Failed to delete orphaned saved reader view file.");
                    }
                }
            }
        }
    }

    private void upgradeDatabaseFrom31to32(final SQLiteDatabase db) {
        debug("Adding visits table");
        createVisitsTable(db);

        debug("Migrating visits from history extension db into visits table");
        String historyExtensionDbName = "history_extension_database";

        SQLiteDatabase historyExtensionDb = null;
        final File historyExtensionsDatabase = mContext.getDatabasePath(historyExtensionDbName);

        // Primary goal of this migration is to improve Top Sites experience by distinguishing between
        // local and remote visits. If Sync is enabled, we rely on visit data from Sync and treat it as remote.
        // However, if Sync is disabled but we detect evidence that it was enabled at some point (HistoryExtensionsDB is present)
        // then we synthesize visits from the History table, but we mark them all as "remote". This will ensure
        // that once user starts browsing around, their Top Sites will reflect their local browsing history.
        // Otherwise, we risk overwhelming their Top Sites with remote history, just as we did before this migration.
        try {
            // If FxAccount exists (Sync is enabled) then port data over to the Visits table.
            if (FirefoxAccounts.firefoxAccountsExist(mContext)) {
                try {
                    historyExtensionDb = SQLiteDatabase.openDatabase(historyExtensionsDatabase.getPath(), null,
                            SQLiteDatabase.OPEN_READONLY);

                    if (historyExtensionDb != null) {
                        copyHistoryExtensionDataToVisitsTable(historyExtensionDb, db);
                    }

                // If we fail to open HistoryExtensionDatabase, then synthesize visits marking them as remote
                } catch (SQLiteException e) {
                    Log.w(LOGTAG, "Couldn't open history extension database; synthesizing visits instead", e);
                    synthesizeAndInsertVisits(db, false);

                // It's possible that we might fail to copy over visit data from the HistoryExtensionsDB,
                // so let's synthesize visits marking them as remote. See Bug 1280409.
                } catch (IllegalStateException e) {
                    Log.w(LOGTAG, "Couldn't copy over history extension data; synthesizing visits instead", e);
                    synthesizeAndInsertVisits(db, false);
                }

            // FxAccount doesn't exist, but there's evidence Sync was enabled at some point.
            // Synthesize visits from History table marking them all as remote.
            } else if (historyExtensionsDatabase.exists()) {
                synthesizeAndInsertVisits(db, false);

            // FxAccount doesn't exist and there's no evidence sync was ever enabled.
            // Synthesize visits from History table marking them all as local.
            } else {
                synthesizeAndInsertVisits(db, true);
            }
        } finally {
            if (historyExtensionDb != null) {
                historyExtensionDb.close();
            }
        }

        // Delete history extensions database if it's present.
        if (historyExtensionsDatabase.exists()) {
            if (!mContext.deleteDatabase(historyExtensionDbName)) {
                Log.e(LOGTAG, "Couldn't remove history extension database");
            }
        }
    }

    private void synthesizeAndInsertVisits(final SQLiteDatabase db, boolean markAsLocal) {
        final Cursor cursor = db.query(
                History.TABLE_NAME,
                new String[] {History.GUID, History.VISITS, History.DATE_LAST_VISITED},
                null, null, null, null, null);
        if (cursor == null) {
            Log.e(LOGTAG, "Null cursor while selecting all history records");
            return;
        }

        try {
            if (!cursor.moveToFirst()) {
                Log.e(LOGTAG, "No history records to synthesize visits for.");
                return;
            }

            int guidCol = cursor.getColumnIndexOrThrow(History.GUID);
            int visitsCol = cursor.getColumnIndexOrThrow(History.VISITS);
            int dateCol = cursor.getColumnIndexOrThrow(History.DATE_LAST_VISITED);

            // Re-use compiled SQL statements for faster inserts.
            // Visit Type is going to be 1, which is the column's default value.
            final String insertSqlStatement = "INSERT OR IGNORE INTO " + Visits.TABLE_NAME + "(" +
                    Visits.DATE_VISITED + "," +
                    Visits.HISTORY_GUID + "," +
                    Visits.IS_LOCAL +
                    ") VALUES (?, ?, ?)";
            final SQLiteStatement compiledInsertStatement = db.compileStatement(insertSqlStatement);

            // For each history record, insert as many visits as there are recorded in the VISITS column.
            do {
                final int numberOfVisits = cursor.getInt(visitsCol);
                final String guid = cursor.getString(guidCol);
                final long lastVisitedDate = cursor.getLong(dateCol);

                // Sanity check.
                if (guid == null) {
                    continue;
                }

                // In a strange case that lastVisitedDate is a very low number, let's not introduce
                // negative timestamps into our data.
                if (lastVisitedDate - numberOfVisits < 0) {
                    continue;
                }

                for (int i = 0; i < numberOfVisits; i++) {
                    final long offsetVisitedDate = lastVisitedDate - i;
                    compiledInsertStatement.clearBindings();
                    compiledInsertStatement.bindLong(1, offsetVisitedDate);
                    compiledInsertStatement.bindString(2, guid);
                    // Very old school, 1 is true and 0 is false :)
                    if (markAsLocal) {
                        compiledInsertStatement.bindLong(3, Visits.VISIT_IS_LOCAL);
                    } else {
                        compiledInsertStatement.bindLong(3, Visits.VISIT_IS_REMOTE);
                    }
                    compiledInsertStatement.executeInsert();
                }
            } while (cursor.moveToNext());
        } catch (Exception e) {
            Log.e(LOGTAG, "Error while synthesizing visits for history record", e);
        } finally {
            cursor.close();
        }
    }

    private void updateHistoryTableAddVisitAggregates(final SQLiteDatabase db) {
        db.execSQL("ALTER TABLE " + TABLE_HISTORY +
                " ADD COLUMN " + History.LOCAL_VISITS + " INTEGER NOT NULL DEFAULT 0");
        db.execSQL("ALTER TABLE " + TABLE_HISTORY +
                " ADD COLUMN " + History.REMOTE_VISITS + " INTEGER NOT NULL DEFAULT 0");
        db.execSQL("ALTER TABLE " + TABLE_HISTORY +
                " ADD COLUMN " + History.LOCAL_DATE_LAST_VISITED + " INTEGER NOT NULL DEFAULT 0");
        db.execSQL("ALTER TABLE " + TABLE_HISTORY +
                " ADD COLUMN " + History.REMOTE_DATE_LAST_VISITED + " INTEGER NOT NULL DEFAULT 0");
    }

    private void calculateHistoryTableVisitAggregates(final SQLiteDatabase db) {
        // Note that we convert from microseconds (timestamps in the visits table) to milliseconds
        // (timestamps in the history table). Sync works in microseconds, so for visits Fennec stores
        // timestamps in microseconds as well - but the rest of the timestamps are stored in milliseconds.
        db.execSQL("UPDATE " + TABLE_HISTORY + " SET " +
                History.LOCAL_VISITS + " = (" +
                    "SELECT COALESCE(SUM(" + qualifyColumn(TABLE_VISITS, Visits.IS_LOCAL) + "), 0)" +
                        " FROM " + TABLE_VISITS +
                        " WHERE " + qualifyColumn(TABLE_VISITS, Visits.HISTORY_GUID) + " = " + qualifyColumn(TABLE_HISTORY, History.GUID) +
                "), " +
                History.REMOTE_VISITS + " = (" +
                    "SELECT COALESCE(SUM(CASE " + Visits.IS_LOCAL + " WHEN 0 THEN 1 ELSE 0 END), 0)" +
                        " FROM " + TABLE_VISITS +
                        " WHERE " + qualifyColumn(TABLE_VISITS, Visits.HISTORY_GUID) + " = " + qualifyColumn(TABLE_HISTORY, History.GUID) +
                "), " +
                History.LOCAL_DATE_LAST_VISITED + " = (" +
                    "SELECT COALESCE(MAX(CASE " + Visits.IS_LOCAL + " WHEN 1 THEN " + Visits.DATE_VISITED + " ELSE 0 END), 0) / 1000" +
                        " FROM " + TABLE_VISITS +
                        " WHERE " + qualifyColumn(TABLE_VISITS, Visits.HISTORY_GUID) + " = " + qualifyColumn(TABLE_HISTORY, History.GUID) +
                "), " +
                History.REMOTE_DATE_LAST_VISITED + " = (" +
                    "SELECT COALESCE(MAX(CASE " + Visits.IS_LOCAL + " WHEN 0 THEN " + Visits.DATE_VISITED + " ELSE 0 END), 0) / 1000" +
                        " FROM " + TABLE_VISITS +
                        " WHERE " + qualifyColumn(TABLE_VISITS, Visits.HISTORY_GUID) + " = " + qualifyColumn(TABLE_HISTORY, History.GUID) +
                ") " +
                "WHERE EXISTS " +
                    "(SELECT " + Visits._ID +
                        " FROM " + TABLE_VISITS +
                        " WHERE " + qualifyColumn(TABLE_VISITS, Visits.HISTORY_GUID) + " = " + qualifyColumn(TABLE_HISTORY, History.GUID) + ")"
        );
    }

    private void upgradeDatabaseFrom32to33(final SQLiteDatabase db) {
        createV33CombinedView(db);
    }

    private void upgradeDatabaseFrom33to34(final SQLiteDatabase db) {
        updateHistoryTableAddVisitAggregates(db);
        calculateHistoryTableVisitAggregates(db);
        createV34CombinedView(db);
    }

    private void upgradeDatabaseFrom34to35(final SQLiteDatabase db) {
        createActivityStreamBlocklistTable(db);
    }

    private void upgradeDatabaseFrom35to36(final SQLiteDatabase db) {
        createPageMetadataTable(db);
    }

    private void createV33CombinedView(final SQLiteDatabase db) {
        db.execSQL("DROP VIEW IF EXISTS " + VIEW_COMBINED);
        db.execSQL("DROP VIEW IF EXISTS " + VIEW_COMBINED_WITH_FAVICONS);

        createCombinedViewOn33(db);
    }

    private void createV34CombinedView(final SQLiteDatabase db) {
        db.execSQL("DROP VIEW IF EXISTS " + VIEW_COMBINED);
        db.execSQL("DROP VIEW IF EXISTS " + VIEW_COMBINED_WITH_FAVICONS);

        createCombinedViewOn34(db);
    }

    private void createV19CombinedView(SQLiteDatabase db) {
        db.execSQL("DROP VIEW IF EXISTS " + VIEW_COMBINED);
        db.execSQL("DROP VIEW IF EXISTS " + VIEW_COMBINED_WITH_FAVICONS);

        createCombinedViewOn19(db);
    }

    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        debug("Upgrading browser.db: " + db.getPath() + " from " +
                oldVersion + " to " + newVersion);

        // We have to do incremental upgrades until we reach the current
        // database schema version.
        for (int v = oldVersion + 1; v <= newVersion; v++) {
            switch (v) {
                case 4:
                    upgradeDatabaseFrom3to4(db);
                    break;

                case 7:
                    upgradeDatabaseFrom6to7(db);
                    break;

                case 8:
                    upgradeDatabaseFrom7to8(db);
                    break;

                case 11:
                    upgradeDatabaseFrom10to11(db);
                    break;

                case 13:
                    upgradeDatabaseFrom12to13(db);
                    break;

                case 14:
                    upgradeDatabaseFrom13to14(db);
                    break;

                case 15:
                    upgradeDatabaseFrom14to15(db);
                    break;

                case 16:
                    upgradeDatabaseFrom15to16(db);
                    break;

                case 17:
                    upgradeDatabaseFrom16to17(db);
                    break;

                case 18:
                    upgradeDatabaseFrom17to18(db);
                    break;

                case 19:
                    upgradeDatabaseFrom18to19(db);
                    break;

                case 20:
                    upgradeDatabaseFrom19to20(db);
                    break;

                case 22:
                    upgradeDatabaseFrom21to22(db);
                    break;

                case 23:
                    upgradeDatabaseFrom22to23(db);
                    break;

                case 24:
                    upgradeDatabaseFrom23to24(db);
                    break;

                case 25:
                    upgradeDatabaseFrom24to25(db);
                    break;

                case 26:
                    upgradeDatabaseFrom25to26(db);
                    break;

                // case 27 occurs in UrlMetadataTable.onUpgrade

                case 28:
                    upgradeDatabaseFrom27to28(db);
                    break;

                case 29:
                    upgradeDatabaseFrom28to29(db);
                    break;

                case 30:
                    upgradeDatabaseFrom29to30(db);
                    break;

                case 31:
                    upgradeDatabaseFrom30to31(db);
                    break;

                case 32:
                    upgradeDatabaseFrom31to32(db);
                    break;

                case 33:
                    upgradeDatabaseFrom32to33(db);
                    break;

                case 34:
                    upgradeDatabaseFrom33to34(db);
                    break;

                case 35:
                    upgradeDatabaseFrom34to35(db);
                    break;

                case 36:
                    upgradeDatabaseFrom35to36(db);
                    break;
            }
        }

        for (Table table : BrowserProvider.sTables) {
            table.onUpgrade(db, oldVersion, newVersion);
        }

        // Delete the obsolete favicon database after all other upgrades complete.
        // This can probably equivalently be moved into upgradeDatabaseFrom12to13.
        if (oldVersion < 13 && newVersion >= 13) {
            if (mContext.getDatabasePath("favicon_urls.db").exists()) {
                mContext.deleteDatabase("favicon_urls.db");
            }
        }
    }

    @Override
    public void onOpen(SQLiteDatabase db) {
        debug("Opening browser.db: " + db.getPath());

        // Force explicit readercache loading - we won't access readercache state for bookmarks
        // until we actually know what our bookmarks are. Bookmarks are stored in the DB, hence
        // it is sufficient to ensure that the readercache is loaded before the DB can be accessed.
        // Note, this takes ~4-6ms to load on an N4 (compared to 20-50ms for most DB queries), and
        // is only done once, hence this shouldn't have noticeable impact on performance. Moreover
        // this is run on a background thread and therefore won't block UI code during startup.
        SavedReaderViewHelper.getSavedReaderViewHelper(mContext).loadItems();

        Cursor cursor = null;
        try {
            cursor = db.rawQuery("PRAGMA foreign_keys=ON", null);
        } finally {
            if (cursor != null)
                cursor.close();
        }
        cursor = null;
        try {
            cursor = db.rawQuery("PRAGMA synchronous=NORMAL", null);
        } finally {
            if (cursor != null)
                cursor.close();
        }

        // From Honeycomb on, it's possible to run several db
        // commands in parallel using multiple connections.
        if (Build.VERSION.SDK_INT >= 11) {
            // Modern Android allows WAL to be enabled through a mode flag.
            if (Build.VERSION.SDK_INT < 16) {
                db.enableWriteAheadLogging();

                // This does nothing on 16+.
                db.setLockingEnabled(false);
            }
        } else {
            // Pre-Honeycomb, we can do some lesser optimizations.
            cursor = null;
            try {
                cursor = db.rawQuery("PRAGMA journal_mode=PERSIST", null);
            } finally {
                if (cursor != null)
                    cursor.close();
            }
        }
    }

    // Calculate these once, at initialization. isLoggable is too expensive to
    // have in-line in each log call.
    private static final boolean logDebug   = Log.isLoggable(LOGTAG, Log.DEBUG);
    private static final boolean logVerbose = Log.isLoggable(LOGTAG, Log.VERBOSE);
    protected static void trace(String message) {
        if (logVerbose) {
            Log.v(LOGTAG, message);
        }
    }

    protected static void debug(String message) {
        if (logDebug) {
            Log.d(LOGTAG, message);
        }
    }

    private Integer getMobileFolderId(SQLiteDatabase db) {
        Cursor c = null;

        try {
            c = db.query(TABLE_BOOKMARKS,
                         mobileIdColumns,
                         Bookmarks.GUID + " = ?",
                         mobileIdSelectionArgs,
                         null, null, null);

            if (c == null || !c.moveToFirst())
                return null;

            return c.getInt(c.getColumnIndex(Bookmarks._ID));
        } finally {
            if (c != null)
                c.close();
        }
    }

    private interface BookmarkMigrator {
        public void updateForNewTable(ContentValues bookmark);
    }

    private class BookmarkMigrator3to4 implements BookmarkMigrator {
        @Override
        public void updateForNewTable(ContentValues bookmark) {
            Integer isFolder = bookmark.getAsInteger("folder");
            if (isFolder == null || isFolder != 1) {
                bookmark.put(Bookmarks.TYPE, Bookmarks.TYPE_BOOKMARK);
            } else {
                bookmark.put(Bookmarks.TYPE, Bookmarks.TYPE_FOLDER);
            }

            bookmark.remove("folder");
        }
    }
}

