/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.io.Serializable;
import java.lang.ref.WeakReference;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.ActivityStreamBlocklist;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserContract.FaviconColumns;
import org.mozilla.gecko.db.BrowserContract.Favicons;
import org.mozilla.gecko.db.BrowserContract.RemoteDevices;
import org.mozilla.gecko.db.BrowserContract.Highlights;
import org.mozilla.gecko.db.BrowserContract.History;
import org.mozilla.gecko.db.BrowserContract.Visits;
import org.mozilla.gecko.db.BrowserContract.Schema;
import org.mozilla.gecko.db.BrowserContract.Tabs;
import org.mozilla.gecko.db.BrowserContract.Thumbnails;
import org.mozilla.gecko.db.BrowserContract.TopSites;
import org.mozilla.gecko.db.BrowserContract.UrlAnnotations;
import org.mozilla.gecko.db.BrowserContract.PageMetadata;
import org.mozilla.gecko.db.DBUtils.UpdateOperation;
import org.mozilla.gecko.icons.IconsHelper;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.BroadcastReceiver;
import android.content.ContentProviderOperation;
import android.content.ContentProviderResult;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.OperationApplicationException;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.database.MatrixCursor;
import android.database.SQLException;
import android.database.sqlite.SQLiteConstraintException;
import android.database.sqlite.SQLiteCursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteQueryBuilder;
import android.database.sqlite.SQLiteStatement;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.util.Log;

public class BrowserProvider extends SharedBrowserDatabaseProvider {
    public static final String ACTION_SHRINK_MEMORY = "org.mozilla.gecko.db.intent.action.SHRINK_MEMORY";

    private static final String LOGTAG = "GeckoBrowserProvider";

    // How many records to reposition in a single query.
    // This should be less than the SQLite maximum number of query variables
    // (currently 999) divided by the number of variables used per positioning
    // query (currently 3).
    static final int MAX_POSITION_UPDATES_PER_QUERY = 100;

    // Minimum number of records to keep when expiring history.
    static final int DEFAULT_EXPIRY_RETAIN_COUNT = 2000;
    static final int AGGRESSIVE_EXPIRY_RETAIN_COUNT = 500;

    // Factor used to determine the minimum number of records to keep when expiring the activity stream blocklist
    static final int ACTIVITYSTREAM_BLOCKLIST_EXPIRY_FACTOR = 5;

    // Minimum duration to keep when expiring.
    static final long DEFAULT_EXPIRY_PRESERVE_WINDOW = 1000L * 60L * 60L * 24L * 28L;     // Four weeks.
    // Minimum number of thumbnails to keep around.
    static final int DEFAULT_EXPIRY_THUMBNAIL_COUNT = 15;

    static final String TABLE_BOOKMARKS = Bookmarks.TABLE_NAME;
    static final String TABLE_HISTORY = History.TABLE_NAME;
    static final String TABLE_VISITS = Visits.TABLE_NAME;
    static final String TABLE_FAVICONS = Favicons.TABLE_NAME;
    static final String TABLE_THUMBNAILS = Thumbnails.TABLE_NAME;
    static final String TABLE_TABS = Tabs.TABLE_NAME;
    static final String TABLE_URL_ANNOTATIONS = UrlAnnotations.TABLE_NAME;
    static final String TABLE_ACTIVITY_STREAM_BLOCKLIST = ActivityStreamBlocklist.TABLE_NAME;
    static final String TABLE_PAGE_METADATA = PageMetadata.TABLE_NAME;
    static final String TABLE_REMOTE_DEVICES = RemoteDevices.TABLE_NAME;

    static final String VIEW_COMBINED = Combined.VIEW_NAME;
    static final String VIEW_BOOKMARKS_WITH_FAVICONS = Bookmarks.VIEW_WITH_FAVICONS;
    static final String VIEW_BOOKMARKS_WITH_ANNOTATIONS = Bookmarks.VIEW_WITH_ANNOTATIONS;
    static final String VIEW_HISTORY_WITH_FAVICONS = History.VIEW_WITH_FAVICONS;
    static final String VIEW_COMBINED_WITH_FAVICONS = Combined.VIEW_WITH_FAVICONS;

    // Bookmark matches
    static final int BOOKMARKS = 100;
    static final int BOOKMARKS_ID = 101;
    static final int BOOKMARKS_FOLDER_ID = 102;
    static final int BOOKMARKS_PARENT = 103;
    static final int BOOKMARKS_POSITIONS = 104;

    // History matches
    static final int HISTORY = 200;
    static final int HISTORY_ID = 201;
    static final int HISTORY_OLD = 202;

    // Favicon matches
    static final int FAVICONS = 300;
    static final int FAVICON_ID = 301;

    // Schema matches
    static final int SCHEMA = 400;

    // Combined bookmarks and history matches
    static final int COMBINED = 500;

    // Control matches
    static final int CONTROL = 600;

    // Search Suggest matches. Obsolete.
    static final int SEARCH_SUGGEST = 700;

    // Thumbnail matches
    static final int THUMBNAILS = 800;
    static final int THUMBNAIL_ID = 801;

    static final int URL_ANNOTATIONS = 900;

    static final int TOPSITES = 1000;

    static final int VISITS = 1100;

    static final int METADATA = 1200;

    static final int HIGHLIGHT_CANDIDATES = 1300;

    static final int ACTIVITY_STREAM_BLOCKLIST = 1400;

    static final int PAGE_METADATA = 1500;

    static final int REMOTE_DEVICES = 1600;
    static final int REMOTE_DEVICES_ID = 1601;

    static final String DEFAULT_BOOKMARKS_SORT_ORDER = Bookmarks.TYPE
            + " ASC, " + Bookmarks.POSITION + " ASC, " + Bookmarks._ID
            + " ASC";

    static final String DEFAULT_HISTORY_SORT_ORDER = History.DATE_LAST_VISITED + " DESC";
    static final String DEFAULT_VISITS_SORT_ORDER = Visits.DATE_VISITED + " DESC";

    static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

    static final Map<String, String> BOOKMARKS_PROJECTION_MAP;
    static final Map<String, String> HISTORY_PROJECTION_MAP;
    static final Map<String, String> COMBINED_PROJECTION_MAP;
    static final Map<String, String> SCHEMA_PROJECTION_MAP;
    static final Map<String, String> FAVICONS_PROJECTION_MAP;
    static final Map<String, String> THUMBNAILS_PROJECTION_MAP;
    static final Map<String, String> URL_ANNOTATIONS_PROJECTION_MAP;
    static final Map<String, String> VISIT_PROJECTION_MAP;
    static final Map<String, String> PAGE_METADATA_PROJECTION_MAP;
    static final Map<String, String> REMOTE_DEVICES_PROJECTION_MAP;
    static final Table[] sTables;

    static {
        sTables = new Table[] {
            // See awful shortcut assumption hack in getURLImageDataTable.
            new URLImageDataTable()
        };
        // We will reuse this.
        HashMap<String, String> map;

        // Bookmarks
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "bookmarks", BOOKMARKS);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "bookmarks/#", BOOKMARKS_ID);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "bookmarks/parents", BOOKMARKS_PARENT);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "bookmarks/positions", BOOKMARKS_POSITIONS);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "bookmarks/folder/#", BOOKMARKS_FOLDER_ID);

        map = new HashMap<String, String>();
        map.put(Bookmarks._ID, Bookmarks._ID);
        map.put(Bookmarks.TITLE, Bookmarks.TITLE);
        map.put(Bookmarks.URL, Bookmarks.URL);
        map.put(Bookmarks.FAVICON, Bookmarks.FAVICON);
        map.put(Bookmarks.FAVICON_ID, Bookmarks.FAVICON_ID);
        map.put(Bookmarks.FAVICON_URL, Bookmarks.FAVICON_URL);
        map.put(Bookmarks.TYPE, Bookmarks.TYPE);
        map.put(Bookmarks.PARENT, Bookmarks.PARENT);
        map.put(Bookmarks.POSITION, Bookmarks.POSITION);
        map.put(Bookmarks.TAGS, Bookmarks.TAGS);
        map.put(Bookmarks.DESCRIPTION, Bookmarks.DESCRIPTION);
        map.put(Bookmarks.KEYWORD, Bookmarks.KEYWORD);
        map.put(Bookmarks.DATE_CREATED, Bookmarks.DATE_CREATED);
        map.put(Bookmarks.DATE_MODIFIED, Bookmarks.DATE_MODIFIED);
        map.put(Bookmarks.GUID, Bookmarks.GUID);
        map.put(Bookmarks.IS_DELETED, Bookmarks.IS_DELETED);
        map.put(Bookmarks.LOCAL_VERSION, Bookmarks.LOCAL_VERSION);
        map.put(Bookmarks.SYNC_VERSION, Bookmarks.SYNC_VERSION);
        BOOKMARKS_PROJECTION_MAP = Collections.unmodifiableMap(map);

        // History
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "history", HISTORY);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "history/#", HISTORY_ID);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "history/old", HISTORY_OLD);

        map = new HashMap<String, String>();
        map.put(History._ID, History._ID);
        map.put(History.TITLE, History.TITLE);
        map.put(History.URL, History.URL);
        map.put(History.FAVICON, History.FAVICON);
        map.put(History.FAVICON_ID, History.FAVICON_ID);
        map.put(History.FAVICON_URL, History.FAVICON_URL);
        map.put(History.VISITS, History.VISITS);
        map.put(History.LOCAL_VISITS, History.LOCAL_VISITS);
        map.put(History.REMOTE_VISITS, History.REMOTE_VISITS);
        map.put(History.DATE_LAST_VISITED, History.DATE_LAST_VISITED);
        map.put(History.LOCAL_DATE_LAST_VISITED, History.LOCAL_DATE_LAST_VISITED);
        map.put(History.REMOTE_DATE_LAST_VISITED, History.REMOTE_DATE_LAST_VISITED);
        map.put(History.DATE_CREATED, History.DATE_CREATED);
        map.put(History.DATE_MODIFIED, History.DATE_MODIFIED);
        map.put(History.GUID, History.GUID);
        map.put(History.IS_DELETED, History.IS_DELETED);
        HISTORY_PROJECTION_MAP = Collections.unmodifiableMap(map);

        // Visits
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "visits", VISITS);

        map = new HashMap<String, String>();
        map.put(Visits._ID, Visits._ID);
        map.put(Visits.HISTORY_GUID, Visits.HISTORY_GUID);
        map.put(Visits.VISIT_TYPE, Visits.VISIT_TYPE);
        map.put(Visits.DATE_VISITED, Visits.DATE_VISITED);
        map.put(Visits.IS_LOCAL, Visits.IS_LOCAL);
        VISIT_PROJECTION_MAP = Collections.unmodifiableMap(map);

        // Favicons
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "favicons", FAVICONS);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "favicons/#", FAVICON_ID);

        map = new HashMap<String, String>();
        map.put(Favicons._ID, Favicons._ID);
        map.put(Favicons.URL, Favicons.URL);
        map.put(Favicons.DATA, Favicons.DATA);
        map.put(Favicons.DATE_CREATED, Favicons.DATE_CREATED);
        map.put(Favicons.DATE_MODIFIED, Favicons.DATE_MODIFIED);
        FAVICONS_PROJECTION_MAP = Collections.unmodifiableMap(map);

        // Thumbnails
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "thumbnails", THUMBNAILS);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "thumbnails/#", THUMBNAIL_ID);

        map = new HashMap<String, String>();
        map.put(Thumbnails._ID, Thumbnails._ID);
        map.put(Thumbnails.URL, Thumbnails.URL);
        map.put(Thumbnails.DATA, Thumbnails.DATA);
        THUMBNAILS_PROJECTION_MAP = Collections.unmodifiableMap(map);

        // Url annotations
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, TABLE_URL_ANNOTATIONS, URL_ANNOTATIONS);

        map = new HashMap<>();
        map.put(UrlAnnotations._ID, UrlAnnotations._ID);
        map.put(UrlAnnotations.URL, UrlAnnotations.URL);
        map.put(UrlAnnotations.KEY, UrlAnnotations.KEY);
        map.put(UrlAnnotations.VALUE, UrlAnnotations.VALUE);
        map.put(UrlAnnotations.DATE_CREATED, UrlAnnotations.DATE_CREATED);
        map.put(UrlAnnotations.DATE_MODIFIED, UrlAnnotations.DATE_MODIFIED);
        map.put(UrlAnnotations.SYNC_STATUS, UrlAnnotations.SYNC_STATUS);
        URL_ANNOTATIONS_PROJECTION_MAP = Collections.unmodifiableMap(map);

        // Combined bookmarks and history
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "combined", COMBINED);

        map = new HashMap<String, String>();
        map.put(Combined._ID, Combined._ID);
        map.put(Combined.BOOKMARK_ID, Combined.BOOKMARK_ID);
        map.put(Combined.HISTORY_ID, Combined.HISTORY_ID);
        map.put(Combined.URL, Combined.URL);
        map.put(Combined.TITLE, Combined.TITLE);
        map.put(Combined.VISITS, Combined.VISITS);
        map.put(Combined.DATE_LAST_VISITED, Combined.DATE_LAST_VISITED);
        map.put(Combined.FAVICON, Combined.FAVICON);
        map.put(Combined.FAVICON_ID, Combined.FAVICON_ID);
        map.put(Combined.FAVICON_URL, Combined.FAVICON_URL);
        map.put(Combined.LOCAL_DATE_LAST_VISITED, Combined.LOCAL_DATE_LAST_VISITED);
        map.put(Combined.REMOTE_DATE_LAST_VISITED, Combined.REMOTE_DATE_LAST_VISITED);
        map.put(Combined.LOCAL_VISITS_COUNT, Combined.LOCAL_VISITS_COUNT);
        map.put(Combined.REMOTE_VISITS_COUNT, Combined.REMOTE_VISITS_COUNT);
        COMBINED_PROJECTION_MAP = Collections.unmodifiableMap(map);

        map = new HashMap<>();
        map.put(PageMetadata._ID, PageMetadata._ID);
        map.put(PageMetadata.HISTORY_GUID, PageMetadata.HISTORY_GUID);
        map.put(PageMetadata.DATE_CREATED, PageMetadata.DATE_CREATED);
        map.put(PageMetadata.HAS_IMAGE, PageMetadata.HAS_IMAGE);
        map.put(PageMetadata.JSON, PageMetadata.JSON);
        PAGE_METADATA_PROJECTION_MAP = Collections.unmodifiableMap(map);

        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "page_metadata", PAGE_METADATA);

        // Schema
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "schema", SCHEMA);

        map = new HashMap<String, String>();
        map.put(Schema.VERSION, Schema.VERSION);
        SCHEMA_PROJECTION_MAP = Collections.unmodifiableMap(map);


        // Control
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "control", CONTROL);

        for (Table table : sTables) {
            for (Table.ContentProviderInfo type : table.getContentProviderInfo()) {
                URI_MATCHER.addURI(BrowserContract.AUTHORITY, type.name, type.id);
            }
        }

        // Combined pinned sites, top visited sites, and suggested sites
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "topsites", TOPSITES);

        URI_MATCHER.addURI(BrowserContract.AUTHORITY, ActivityStreamBlocklist.TABLE_NAME, ACTIVITY_STREAM_BLOCKLIST);

        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "highlight_candidates", HIGHLIGHT_CANDIDATES);

        // FxA Devices
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "remote_devices", REMOTE_DEVICES);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "remote_devices/#", REMOTE_DEVICES_ID);

        map = new HashMap<>();
        map.put(RemoteDevices._ID, RemoteDevices._ID);
        map.put(RemoteDevices.GUID, RemoteDevices.GUID);
        map.put(RemoteDevices.NAME, RemoteDevices.NAME);
        map.put(RemoteDevices.TYPE, RemoteDevices.TYPE);
        map.put(RemoteDevices.IS_CURRENT_DEVICE, RemoteDevices.IS_CURRENT_DEVICE);
        map.put(RemoteDevices.DATE_CREATED, RemoteDevices.DATE_CREATED);
        map.put(RemoteDevices.DATE_MODIFIED, RemoteDevices.DATE_MODIFIED);
        map.put(RemoteDevices.LAST_ACCESS_TIME, RemoteDevices.LAST_ACCESS_TIME);
        REMOTE_DEVICES_PROJECTION_MAP = Collections.unmodifiableMap(map);
    }

    private static class ShrinkMemoryReceiver extends BroadcastReceiver {
        private final WeakReference<BrowserProvider> mBrowserProviderWeakReference;

        public ShrinkMemoryReceiver(final BrowserProvider browserProvider) {
            mBrowserProviderWeakReference = new WeakReference<>(browserProvider);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            final BrowserProvider browserProvider = mBrowserProviderWeakReference.get();
            if (browserProvider == null) {
                return;
            }
            final PerProfileDatabases<BrowserDatabaseHelper> databases = browserProvider.getDatabases();
            if (databases == null) {
                return;
            }
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    databases.shrinkMemory();
                }
            });
        }
    }

    private final ShrinkMemoryReceiver mShrinkMemoryReceiver = new ShrinkMemoryReceiver(this);

    @Override
    public boolean onCreate() {
        if (!super.onCreate()) {
            return false;
        }

        LocalBroadcastManager.getInstance(getContext()).registerReceiver(mShrinkMemoryReceiver,
                new IntentFilter(ACTION_SHRINK_MEMORY));

        return true;
    }

    @Override
    public void shutdown() {
        LocalBroadcastManager.getInstance(getContext()).unregisterReceiver(mShrinkMemoryReceiver);

        super.shutdown();
    }

    // Convenience accessor.
    // Assumes structure of sTables!
    private URLImageDataTable getURLImageDataTable() {
        return (URLImageDataTable) sTables[0];
    }

    private static boolean hasFaviconsInProjection(String[] projection) {
        if (projection == null) return true;
        for (int i = 0; i < projection.length; ++i) {
            if (projection[i].equals(FaviconColumns.FAVICON) ||
                projection[i].equals(FaviconColumns.FAVICON_URL))
                return true;
        }

        return false;
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

    /**
     * Remove enough activity stream blocklist items to bring the database count below <code>retain</code>.
     *
     * Items will be removed according to their creation date, oldest being removed first.
     */
    private void expireActivityStreamBlocklist(final SQLiteDatabase db, final int retain) {
        Log.d(LOGTAG, "Expiring highlights blocklist.");
        final long rows = DatabaseUtils.queryNumEntries(db, TABLE_ACTIVITY_STREAM_BLOCKLIST);

        if (retain >= rows) {
            debug("Not expiring highlights blocklist: only have " + rows + " rows.");
            return;
        }

        final long toRemove = rows - retain;

        final String statement = "DELETE FROM " + TABLE_ACTIVITY_STREAM_BLOCKLIST + " WHERE " + ActivityStreamBlocklist._ID + " IN " +
                " ( SELECT " + ActivityStreamBlocklist._ID + " FROM " + TABLE_ACTIVITY_STREAM_BLOCKLIST + " " +
                "ORDER BY " + ActivityStreamBlocklist.CREATED + " ASC LIMIT " + toRemove + ")";

        beginWrite(db);
        db.execSQL(statement);
    }

    /**
     * Remove enough history items to bring the database count below <code>retain</code>,
     * removing no items with a modified time after <code>keepAfter</code>.
     *
     * Provide <code>keepAfter</code> less than or equal to zero to skip that check.
     *
     * Items will be removed according to last visited date.
     */
    private void expireHistory(final SQLiteDatabase db, final int retain, final long keepAfter) {
        Log.d(LOGTAG, "Expiring history.");
        final long rows = DatabaseUtils.queryNumEntries(db, TABLE_HISTORY);

        if (retain >= rows) {
            debug("Not expiring history: only have " + rows + " rows.");
            return;
        }

        final long toRemove = rows - retain;
        debug("Expiring at most " + toRemove + " rows earlier than " + keepAfter + ".");

        final String sql;
        if (keepAfter > 0) {
            // See Bug 1428165: 'modified' value might be missing; assume 0 if that's the case.
            sql = "DELETE FROM " + TABLE_HISTORY + " " +
                  "WHERE MAX("
                    + History.DATE_LAST_VISITED + ", " +
                    "COALESCE(" + History.DATE_MODIFIED + ", 0))" +
                  " < " + keepAfter + " " +
                  " AND " + History._ID + " IN ( SELECT " +
                    History._ID + " FROM " + TABLE_HISTORY + " " +
                    "ORDER BY " + History.DATE_LAST_VISITED + " ASC LIMIT " + toRemove +
                  ")";
        } else {
            sql = "DELETE FROM " + TABLE_HISTORY + " WHERE " + History._ID + " " +
                  "IN ( SELECT " + History._ID + " FROM " + TABLE_HISTORY + " " +
                  "ORDER BY " + History.DATE_LAST_VISITED + " ASC LIMIT " + toRemove + ")";
        }
        trace("Deleting using query: " + sql);

        beginWrite(db);
        db.execSQL(sql);
    }

    /**
     * Remove any thumbnails that for sites that aren't likely to be ever shown.
     * Items will be removed according to a frecency calculation and only if they are not pinned
     *
     * Call this method within a transaction.
     */
    private void expireThumbnails(final SQLiteDatabase db) {
        Log.d(LOGTAG, "Expiring thumbnails.");
        final String sortOrder = BrowserContract.getCombinedFrecencySortOrder(true, false);
        final String sql = "DELETE FROM " + TABLE_THUMBNAILS +
                           " WHERE " + Thumbnails.URL + " NOT IN ( " +
                             " SELECT " + Combined.URL +
                             " FROM " + Combined.VIEW_NAME +
                             " ORDER BY " + sortOrder +
                             " LIMIT " + DEFAULT_EXPIRY_THUMBNAIL_COUNT +
                           ") AND " + Thumbnails.URL + " NOT IN ( " +
                             " SELECT " + Bookmarks.URL +
                             " FROM " + TABLE_BOOKMARKS +
                             " WHERE " + Bookmarks.PARENT + " = " + Bookmarks.FIXED_PINNED_LIST_ID +
                           ") AND " + Thumbnails.URL + " NOT IN ( " +
                             " SELECT " + Tabs.URL +
                             " FROM " + TABLE_TABS +
                           ")";
        trace("Clear thumbs using query: " + sql);
        db.execSQL(sql);
    }

    private boolean shouldIncrementVisits(Uri uri) {
        String incrementVisits = uri.getQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS);
        return Boolean.parseBoolean(incrementVisits);
    }

    private boolean shouldIncrementRemoteAggregates(Uri uri) {
        final String incrementRemoteAggregates = uri.getQueryParameter(BrowserContract.PARAM_INCREMENT_REMOTE_AGGREGATES);
        return Boolean.parseBoolean(incrementRemoteAggregates);
    }

    @Override
    public String getType(Uri uri) {
        final int match = URI_MATCHER.match(uri);

        trace("Getting URI type: " + uri);

        switch (match) {
            case BOOKMARKS:
                trace("URI is BOOKMARKS: " + uri);
                return Bookmarks.CONTENT_TYPE;
            case BOOKMARKS_ID:
                trace("URI is BOOKMARKS_ID: " + uri);
                return Bookmarks.CONTENT_ITEM_TYPE;
            case HISTORY:
                trace("URI is HISTORY: " + uri);
                return History.CONTENT_TYPE;
            case HISTORY_ID:
                trace("URI is HISTORY_ID: " + uri);
                return History.CONTENT_ITEM_TYPE;
            default:
                String type = getContentItemType(match);
                if (type != null) {
                    trace("URI is " + type);
                    return type;
                }

                debug("URI has unrecognized type: " + uri);
                return null;
        }
    }

    @SuppressWarnings("fallthrough")
    @Override
    public int deleteInTransaction(Uri uri, String selection, String[] selectionArgs) {
        trace("Calling delete in transaction on URI: " + uri);
        final SQLiteDatabase db = getWritableDatabase(uri);

        final int match = URI_MATCHER.match(uri);
        int deleted = 0;

        switch (match) {
            case BOOKMARKS_ID:
                trace("Delete on BOOKMARKS_ID: " + uri);

                selection = DatabaseUtils.concatenateWhere(selection, TABLE_BOOKMARKS + "._id = ?");
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case BOOKMARKS: {
                trace("Deleting bookmarks: " + uri);
                // Since we touch multiple records for most deletions, 'deleted' here really means 'affected'.
                deleted = deleteBookmarks(uri, selection, selectionArgs);
                deleteUnusedImages(uri);
                break;
            }

            case HISTORY_ID:
                trace("Delete on HISTORY_ID: " + uri);

                selection = DatabaseUtils.concatenateWhere(selection, TABLE_HISTORY + "._id = ?");
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case HISTORY: {
                trace("Deleting history: " + uri);
                beginWrite(db);
                /**
                 * Deletes from Sync are actual DELETE statements, which will cascade delete relevant visits.
                 * Fennec's deletes mark records as deleted and wipe out all information (except for GUID).
                 * Eventually, Fennec will purge history records that were marked as deleted for longer than some
                 * period of time (e.g. 20 days).
                 * See {@link SharedBrowserDatabaseProvider#cleanUpSomeDeletedRecords(Uri, String)}.
                 */
                final ArrayList<String> historyGUIDs = getHistoryGUIDsFromSelection(db, uri, selection, selectionArgs);

                if (!isCallerSync(uri)) {
                    deleteVisitsForHistory(db, historyGUIDs);
                }
                deletePageMetadataForHistory(db, historyGUIDs);
                deleted = deleteHistory(db, uri, selection, selectionArgs);
                deleteUnusedImages(uri);
                break;
            }

            case VISITS:
                trace("Deleting visits: " + uri);
                beginWrite(db);
                deleted = deleteVisits(uri, selection, selectionArgs);
                break;

            case HISTORY_OLD: {
                String priority = uri.getQueryParameter(BrowserContract.PARAM_EXPIRE_PRIORITY);
                long keepAfter = System.currentTimeMillis() - DEFAULT_EXPIRY_PRESERVE_WINDOW;
                int retainCount = DEFAULT_EXPIRY_RETAIN_COUNT;

                if (BrowserContract.ExpirePriority.AGGRESSIVE.toString().equals(priority)) {
                    keepAfter = 0;
                    retainCount = AGGRESSIVE_EXPIRY_RETAIN_COUNT;
                }
                expireHistory(db, retainCount, keepAfter);
                expireActivityStreamBlocklist(db, retainCount / ACTIVITYSTREAM_BLOCKLIST_EXPIRY_FACTOR);
                expireThumbnails(db);
                deleteUnusedImages(uri);
                break;
            }

            case FAVICON_ID:
                debug("Delete on FAVICON_ID: " + uri);

                selection = DatabaseUtils.concatenateWhere(selection, TABLE_FAVICONS + "._id = ?");
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case FAVICONS: {
                trace("Deleting favicons: " + uri);
                beginWrite(db);
                deleted = deleteFavicons(uri, selection, selectionArgs);
                break;
            }

            case THUMBNAIL_ID:
                debug("Delete on THUMBNAIL_ID: " + uri);

                selection = DatabaseUtils.concatenateWhere(selection, TABLE_THUMBNAILS + "._id = ?");
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case THUMBNAILS: {
                trace("Deleting thumbnails: " + uri);
                beginWrite(db);
                deleted = deleteThumbnails(uri, selection, selectionArgs);
                break;
            }

            case URL_ANNOTATIONS:
                trace("Delete on URL_ANNOTATIONS: " + uri);
                deleteUrlAnnotation(uri, selection, selectionArgs);
                break;

            case PAGE_METADATA:
                trace("Delete on PAGE_METADATA: " + uri);
                deleted = deletePageMetadata(uri, selection, selectionArgs);
                break;

            case REMOTE_DEVICES_ID:
                debug("Delete on REMOTE_DEVICES_ID: " + uri);

                selection = DatabaseUtils.concatenateWhere(selection, TABLE_REMOTE_DEVICES + "._ID = ?");
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case REMOTE_DEVICES: {
                trace("Deleting FxA devices: " + uri);
                beginWrite(db);
                deleted = deleteRemoteDevices(uri, selection, selectionArgs);
                break;
            }

            default: {
                Table table = findTableFor(match);
                if (table == null) {
                    throw new UnsupportedOperationException("Unknown delete URI " + uri);
                }
                trace("Deleting TABLE: " + uri);
                beginWrite(db);
                deleted = table.delete(db, uri, match, selection, selectionArgs);
            }
        }

        debug("Deleted " + deleted + " rows for URI: " + uri);

        return deleted;
    }

    @Override
    public Uri insertInTransaction(Uri uri, ContentValues values) {
        trace("Calling insert in transaction on URI: " + uri);

        int match = URI_MATCHER.match(uri);
        long id = -1;

        if (values != null
                && (values.containsKey(BrowserContract.VersionColumns.LOCAL_VERSION)
                || values.containsKey(BrowserContract.VersionColumns.SYNC_VERSION))) {
            throw new IllegalArgumentException("Can not manually set record versions.");
        }

        switch (match) {
            case BOOKMARKS: {
                trace("Insert on BOOKMARKS: " + uri);
                id = insertBookmark(uri, values);
                break;
            }

            case HISTORY: {
                trace("Insert on HISTORY: " + uri);
                id = insertHistory(uri, values);
                break;
            }

            case VISITS: {
                trace("Insert on VISITS: " + uri);
                id = insertVisit(uri, values);
                break;
            }

            case FAVICONS: {
                trace("Insert on FAVICONS: " + uri);
                id = insertFavicon(uri, values);
                break;
            }

            case THUMBNAILS: {
                trace("Insert on THUMBNAILS: " + uri);
                id = insertThumbnail(uri, values);
                break;
            }

            case URL_ANNOTATIONS: {
                trace("Insert on URL_ANNOTATIONS: " + uri);
                id = insertUrlAnnotation(uri, values);
                break;
            }

            case ACTIVITY_STREAM_BLOCKLIST: {
                trace("Insert on ACTIVITY_STREAM_BLOCKLIST: " + uri);
                id = insertActivityStreamBlocklistSite(uri, values);
                break;
            }

            case PAGE_METADATA: {
                trace("Insert on PAGE_METADATA: " + uri);
                id = insertPageMetadata(uri, values);
                break;
            }

            case REMOTE_DEVICES: {
                trace("Insert on REMOTE_DEVICES: " + uri);
                id = insertFxADevice(uri, values);
                break;
            }

            default: {
                Table table = findTableFor(match);
                if (table == null) {
                    throw new UnsupportedOperationException("Unknown insert URI " + uri);
                }

                trace("Insert on TABLE: " + uri);
                final SQLiteDatabase db = getWritableDatabase(uri);
                beginWrite(db);
                id = table.insert(db, uri, match, values);
            }
        }

        debug("Inserted ID in database: " + id);

        if (id >= 0)
            return ContentUris.withAppendedId(uri, id);

        return null;
    }

    @SuppressWarnings("fallthrough")
    @Override
    public int updateInTransaction(Uri uri, @Nullable ContentValues values, @Nullable String selection,
            String[] selectionArgs) {
        trace("Calling update in transaction on URI: " + uri);

        if (values != null
                && (values.containsKey(BrowserContract.VersionColumns.LOCAL_VERSION)
                || values.containsKey(BrowserContract.VersionColumns.SYNC_VERSION))) {
            throw new IllegalArgumentException("Can not manually update record versions.");
        }

        int match = URI_MATCHER.match(uri);
        int updated = 0;

        final SQLiteDatabase db = getWritableDatabase(uri);
        switch (match) {
            // We provide a dedicated (hacky) API for callers to bulk-update the positions of
            // folder children by passing an array of GUID strings as `selectionArgs`.
            // Each child will have its position column set to its index in the provided array.
            //
            // This avoids callers having to issue a large number of UPDATE queries through
            // the usual channels. See Bug 728783.
            //
            // Note that this is decidedly not a general-purpose API; use at your own risk.
            // `values` and `selection` are ignored.
            case BOOKMARKS_POSITIONS: {
                debug("Update on BOOKMARKS_POSITIONS: " + uri);

                // This already starts and finishes its own transaction.
                updated = updateBookmarkPositions(uri, selectionArgs);
                break;
            }

            case BOOKMARKS_PARENT: {
                debug("Update on BOOKMARKS_PARENT: " + uri);
                beginWrite(db);
                updated = updateBookmarkParents(db, uri, values, selection, selectionArgs);
                break;
            }

            case BOOKMARKS_ID:
                debug("Update on BOOKMARKS_ID: " + uri);

                selection = DatabaseUtils.concatenateWhere(selection, TABLE_BOOKMARKS + "._id = ?");
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case BOOKMARKS: {
                debug("Updating bookmark: " + uri);
                if (shouldUpdateOrInsert(uri)) {
                    updated = updateOrInsertBookmark(uri, values, selection, selectionArgs);
                } else {
                    updated = updateBookmarks(uri, values, selection, selectionArgs);
                }
                break;
            }

            case HISTORY_ID:
                debug("Update on HISTORY_ID: " + uri);

                selection = DatabaseUtils.concatenateWhere(selection, TABLE_HISTORY + "._id = ?");
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case HISTORY: {
                debug("Updating history: " + uri);
                if (shouldUpdateOrInsert(uri)) {
                    updated = updateOrInsertHistory(uri, values, selection, selectionArgs);
                } else {
                    updated = updateHistory(uri, values, selection, selectionArgs);
                }
                if (shouldIncrementVisits(uri)) {
                    insertVisitForHistory(uri, values, selection, selectionArgs);
                }
                break;
            }

            case FAVICONS: {
                debug("Update on FAVICONS: " + uri);

                String url = values.getAsString(Favicons.URL);
                String faviconSelection = null;
                String[] faviconSelectionArgs = null;

                if (!TextUtils.isEmpty(url)) {
                    faviconSelection = Favicons.URL + " = ?";
                    faviconSelectionArgs = new String[] { url };
                }

                if (shouldUpdateOrInsert(uri)) {
                    updated = updateOrInsertFavicon(uri, values, faviconSelection, faviconSelectionArgs);
                } else {
                    updated = updateExistingFavicon(uri, values, faviconSelection, faviconSelectionArgs);
                }
                break;
            }

            case THUMBNAILS: {
                debug("Update on THUMBNAILS: " + uri);

                String url = values.getAsString(Thumbnails.URL);

                // if no URL is provided, update all of the entries
                if (TextUtils.isEmpty(values.getAsString(Thumbnails.URL))) {
                    updated = updateExistingThumbnail(uri, values, null, null);
                } else if (shouldUpdateOrInsert(uri)) {
                    updated = updateOrInsertThumbnail(uri, values, Thumbnails.URL + " = ?",
                                                      new String[] { url });
                } else {
                    updated = updateExistingThumbnail(uri, values, Thumbnails.URL + " = ?",
                                                      new String[] { url });
                }
                break;
            }

            case URL_ANNOTATIONS:
                updateUrlAnnotation(uri, values, selection, selectionArgs);
                break;

            default: {
                Table table = findTableFor(match);
                if (table == null) {
                    throw new UnsupportedOperationException("Unknown update URI " + uri);
                }
                trace("Update TABLE: " + uri);

                beginWrite(db);
                updated = table.update(db, uri, match, values, selection, selectionArgs);
                if (shouldUpdateOrInsert(uri) && updated == 0) {
                    trace("No update, inserting for URL: " + uri);
                    table.insert(db, uri, match, values);
                    updated = 1;
                }
            }
        }

        debug("Updated " + updated + " rows for URI: " + uri);
        return updated;
    }

    private Cursor getTopSites(final Uri uri) {
        // In order to correctly merge the top and pinned sites we:
        //
        // 1. Generate a list of free ids for topsites - this is the positions that are NOT used by pinned sites.
        //    We do this using a subquery with a self-join in order to generate rowids, that allow us to join with
        //    the list of topsites.
        // 2. Generate the list of topsites in order of frecency.
        // 3. Join these, so that each topsite is given its resulting position
        // 4. UNION all with the pinned sites, and order by position
        //
        // Suggested sites are placed after the topsites, but might still be interspersed with the suggested sites,
        // hence we append these to the topsite list, and treat these identically to topsites from this point on.
        //
        // We require rowids to join the two lists, however subqueries aren't given rowids - hence we use two different
        // tricks to generate these:
        // 1. The list of free ids is small, hence we can do a self-join to generate rowids.
        // 2. The topsites list is larger, hence we use a temporary table, which automatically provides rowids.

        final SQLiteDatabase db = getWritableDatabase(uri);

        final String TABLE_TOPSITES = "topsites";

        final String limitParam = uri.getQueryParameter(BrowserContract.PARAM_LIMIT);
        final String gridLimitParam = uri.getQueryParameter(BrowserContract.PARAM_SUGGESTEDSITES_LIMIT);
        final boolean excludeRemoteOnly = Boolean.parseBoolean(
                uri.getQueryParameter(BrowserContract.PARAM_TOPSITES_EXCLUDE_REMOTE_ONLY));
        final String nonPositionedPins = uri.getQueryParameter(BrowserContract.PARAM_NON_POSITIONED_PINS);

        final int totalLimit;
        final int suggestedGridLimit;

        if (limitParam == null) {
            totalLimit = 50;
        } else {
            totalLimit = Integer.parseInt(limitParam, 10);
        }

        if (gridLimitParam == null) {
            suggestedGridLimit = getContext().getResources().getInteger(R.integer.number_of_top_sites);
        } else {
            suggestedGridLimit = Integer.parseInt(gridLimitParam, 10);
        }

        // We have two types of pinned sites, positioned and non-positioned. Positioned pins are used
        // by regular Top Sites, where position in the grid is of importance.
        // Activity Stream Top Sites pins are displayed in front of other top site items - they inherit
        // the Top Sites positioned pins, and new pins are created as non-positioned.
        // Since we will be permanently moving to Activity Stream, if a pin is modified in Activity Stream, it is
        // lost from (legacy) Top Sites.
        String pinnedSitesFromClause = "FROM " + TABLE_BOOKMARKS + " WHERE " +
                Bookmarks.PARENT + " = " + Bookmarks.FIXED_PINNED_LIST_ID +
                " AND " + Bookmarks.IS_DELETED + " IS NOT 1";
        if (nonPositionedPins == null) {
            pinnedSitesFromClause += " AND " + Bookmarks.POSITION + " != " + Bookmarks.FIXED_AS_PIN_POSITION;
        }

        // Ideally we'd use a recursive CTE to generate our sequence, e.g. something like this worked at one point:
        // " WITH RECURSIVE" +
        // " cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 6)" +
        // However that requires SQLite >= 3.8.3 (available on Android >= 5.0), so in the meantime
        // we use a temporary numbers table.
        // Note: SQLite rowids are 1-indexed, whereas we're expecting 0-indexed values for the position. Our numbers
        // table starts at position = 0, which ensures the correct results here.
        final String freeIDSubquery =
                " SELECT count(free_ids.position) + 1 AS rowid, numbers.position AS " + Bookmarks.POSITION +
                " FROM (SELECT position FROM numbers WHERE position NOT IN (SELECT " + Bookmarks.POSITION + " " + pinnedSitesFromClause + ")) AS numbers" +
                " LEFT OUTER JOIN " +
                " (SELECT position FROM numbers WHERE position NOT IN (SELECT " + Bookmarks.POSITION + " " + pinnedSitesFromClause + ")) AS free_ids" +
                " ON numbers.position > free_ids.position" +
                " GROUP BY numbers.position" +
                " ORDER BY numbers.position ASC" +
                " LIMIT " + suggestedGridLimit;

        // Filter out: unvisited pages (history_id == -1) pinned (and other special) sites, deleted sites,
        // and about: pages.
        String ignoreForTopSitesWhereClause =
                "(" + Combined.HISTORY_ID + " IS NOT -1)" +
                " AND " +
                Combined.URL + " NOT IN (SELECT " +
                Bookmarks.URL + " FROM bookmarks WHERE " +
                DBUtils.qualifyColumn("bookmarks", Bookmarks.PARENT) + " < " + Bookmarks.FIXED_ROOT_ID + " AND " +
                DBUtils.qualifyColumn("bookmarks", Bookmarks.IS_DELETED) + " == 0)" +
                " AND " +
                "(" + Combined.URL + " NOT LIKE ?)";

        if (excludeRemoteOnly) {
            ignoreForTopSitesWhereClause += " AND (" + Combined.LOCAL_VISITS_COUNT + " > 0)";
        }

        final String[] ignoreForTopSitesArgs = new String[] {
                AboutPages.URL_FILTER
        };

        // Stuff the suggested sites into SQL: this allows us to filter pinned and topsites out of the suggested
        // sites list as part of the final query (as opposed to walking cursors in java)
        final SuggestedSites suggestedSites = BrowserDB.from(GeckoProfile.get(
                getContext(), uri.getQueryParameter(BrowserContract.PARAM_PROFILE))).getSuggestedSites();

        StringBuilder suggestedSitesBuilder = new StringBuilder();
        // We could access the underlying data here, however SuggestedSites also performs filtering on the suggested
        // sites list, which means we'd need to process the lists within SuggestedSites in any case. If we're doing
        // that processing, there is little real between us using a MatrixCursor, or a Map (or List) instead of the
        // MatrixCursor.
        final Cursor suggestedSitesCursor = suggestedSites.get(suggestedGridLimit);

        String[] suggestedSiteArgs = new String[0];

        boolean hasProcessedAnySuggestedSites = false;

        final int idColumnIndex = suggestedSitesCursor.getColumnIndexOrThrow(Bookmarks._ID);
        final int urlColumnIndex = suggestedSitesCursor.getColumnIndexOrThrow(Bookmarks.URL);
        final int titleColumnIndex = suggestedSitesCursor.getColumnIndexOrThrow(Bookmarks.TITLE);

        while (suggestedSitesCursor.moveToNext()) {
            // We'll be using this as a subquery, hence we need to avoid the preceding UNION ALL
            if (hasProcessedAnySuggestedSites) {
                suggestedSitesBuilder.append(" UNION ALL");
            } else {
                hasProcessedAnySuggestedSites = true;
            }
            suggestedSitesBuilder.append(" SELECT" +
                                         " ? AS " + Bookmarks._ID + "," +
                                         " ? AS " + Bookmarks.URL + "," +
                                         " ? AS " + Bookmarks.TITLE);

            suggestedSiteArgs = DatabaseUtils.appendSelectionArgs(suggestedSiteArgs,
                                                                  new String[] {
                                                                          suggestedSitesCursor.getString(idColumnIndex),
                                                                          suggestedSitesCursor.getString(urlColumnIndex),
                                                                          suggestedSitesCursor.getString(titleColumnIndex)
                                                                  });
        }
        suggestedSitesCursor.close();

        // To restrict suggested sites to the grid, we simply subtract the number of topsites (which have already had
        // the pinned sites filtered out), and the number of pinned sites.
        // SQLite completely ignores negative limits, hence we need to manually limit to 0 in this case.
        final String suggestedLimitClause = " LIMIT MAX(0, (" + suggestedGridLimit + " - (SELECT COUNT(*) FROM " + TABLE_TOPSITES + ") - (SELECT COUNT(*) " + pinnedSitesFromClause + "))) ";

        db.beginTransaction();
        try {
            db.execSQL("DROP TABLE IF EXISTS " + TABLE_TOPSITES);

            db.execSQL("CREATE TEMP TABLE " + TABLE_TOPSITES + " AS" +
                       " SELECT " +
                       Bookmarks._ID + ", " +
                       Combined.BOOKMARK_ID + ", " +
                       Combined.HISTORY_ID + ", " +
                       Combined.HISTORY_GUID + ", " +
                       Bookmarks.URL + ", " +
                       Bookmarks.TITLE + ", " +
                       Combined.HISTORY_ID + ", " +
                       TopSites.TYPE_TOP + " AS " + TopSites.TYPE +
                       " FROM " + Combined.VIEW_NAME +
                       " WHERE " + ignoreForTopSitesWhereClause +
                       " ORDER BY " + BrowserContract.getCombinedFrecencySortOrder(true, false) +
                       " LIMIT " + totalLimit,

                       ignoreForTopSitesArgs);

            if (hasProcessedAnySuggestedSites) {
                db.execSQL("INSERT INTO " + TABLE_TOPSITES +
                           // We need to LIMIT _after_ selecting the relevant suggested sites, which requires us to
                           // use an additional internal subquery, since we cannot LIMIT a subquery that is part of UNION ALL.
                           // Hence the weird SELECT * FROM (SELECT ...relevant suggested sites... LIMIT ?)
                           " SELECT * FROM (SELECT " +
                           Bookmarks._ID + ", " +
                           " NULL " + " AS " + Combined.BOOKMARK_ID + ", " +
                           " -1 AS " + Combined.HISTORY_ID + ", " +
                           " NULL AS " + Combined.HISTORY_GUID + ", " +
                           Bookmarks.URL + ", " +
                           Bookmarks.TITLE + ", " +
                           "NULL AS " + Combined.HISTORY_ID + ", " +
                           TopSites.TYPE_SUGGESTED + " as " + TopSites.TYPE +
                           " FROM ( " + suggestedSitesBuilder.toString() + " )" +
                           " WHERE " +
                           Bookmarks.URL + " NOT IN (SELECT url FROM " + TABLE_TOPSITES + ")" +
                           " AND " +
                           Bookmarks.URL + " NOT IN (SELECT url " + pinnedSitesFromClause + ")" +
                           suggestedLimitClause + " )",

                           suggestedSiteArgs);
            }

            // If we retrieve more topsites than we have free positions for in the freeIdSubquery,
            // we will have topsites that don't receive a position when joining TABLE_TOPSITES
            // with freeIdSubquery. Hence we need to coalesce the position with a generated position.
            // We know that the difference in positions will be at most suggestedGridLimit, hence we
            // can add that to the rowid to generate a safe position.
            // I.e. if we have 6 pinned sites then positions 0..5 are filled, the JOIN results in
            // the first N rows having positions 6..(N+6), so row N+1 should receive a position that is at
            // least N+1+6, which is equal to rowid + 6.
            final String selectTopSites =
                    "SELECT " +
                    Bookmarks._ID + ", " +
                    TopSites.BOOKMARK_ID + ", " +
                    TopSites.HISTORY_ID + ", " +
                    Combined.HISTORY_GUID + ", " +
                    Bookmarks.URL + ", " +
                    Bookmarks.TITLE + ", " +
                    "COALESCE(" + Bookmarks.POSITION + ", " +
                    DBUtils.qualifyColumn(TABLE_TOPSITES, "rowid") + " + " + suggestedGridLimit +
                    ")" + " AS " + Bookmarks.POSITION + ", " +
                    Combined.HISTORY_ID + ", " +
                    TopSites.TYPE +
                    " FROM " + TABLE_TOPSITES +
                    " LEFT OUTER JOIN " + // TABLE_IDS +
                    "(" + freeIDSubquery + ") AS id_results" +
                    " ON " + DBUtils.qualifyColumn(TABLE_TOPSITES, "rowid") +
                    " = " + DBUtils.qualifyColumn("id_results", "rowid") +

                    " UNION ALL " +

                    "SELECT " +
                    Bookmarks._ID + ", " +
                    Bookmarks._ID + " AS " + TopSites.BOOKMARK_ID + ", " +
                    " -1 AS " + TopSites.HISTORY_ID + ", " +
                    " NULL AS " + Combined.HISTORY_GUID + ", " +
                    Bookmarks.URL + ", " +
                    Bookmarks.TITLE + ", " +
                    Bookmarks.POSITION + ", " +
                    "NULL AS " + Combined.HISTORY_ID + ", " +
                    TopSites.TYPE_PINNED + " as " + TopSites.TYPE +
                    " " + pinnedSitesFromClause;

            // In order to join the PageMetadata with our `SELECT ... UNION ALL SELECT ...` for top sites, the top sites
            // SELECT must be a subquery (see https://stackoverflow.com/a/19110809/2219998).
            final SQLiteCursor c = (SQLiteCursor) db.rawQuery(
                    // Specify a projection so we don't take the whole PageMetadata table, or the joining columns, with us.
                    "SELECT " +
                    DBUtils.qualifyColumn(TABLE_TOPSITES, Bookmarks._ID) + ", " +
                    DBUtils.qualifyColumn(TABLE_TOPSITES, TopSites.BOOKMARK_ID) + ", " +
                    DBUtils.qualifyColumn(TABLE_TOPSITES, TopSites.HISTORY_ID) + ", " +
                    DBUtils.qualifyColumn(TABLE_TOPSITES, Bookmarks.URL) + ", " +
                    DBUtils.qualifyColumn(TABLE_TOPSITES, Bookmarks.TITLE) + ", " +
                    DBUtils.qualifyColumn(TABLE_TOPSITES, Bookmarks.POSITION) + ", " +
                    DBUtils.qualifyColumn(TABLE_TOPSITES, TopSites.TYPE) + ", " +
                    PageMetadata.JSON + " AS " + TopSites.PAGE_METADATA_JSON +

                    " FROM (" + selectTopSites + ") AS " + TABLE_TOPSITES +

                    " LEFT OUTER JOIN " + TABLE_PAGE_METADATA + " ON " +
                            DBUtils.qualifyColumn(TABLE_TOPSITES, Combined.HISTORY_GUID) + " = " +
                            DBUtils.qualifyColumn(TABLE_PAGE_METADATA, PageMetadata.HISTORY_GUID) +

                    " GROUP BY " + DBUtils.qualifyColumn(TABLE_TOPSITES, Bookmarks.URL) + // remove duplicates.

                    // In case position is non-unique (as in Activity Stream pins, whose position
                    // is always zero), we need to ensure we get stable ordering.
                    " ORDER BY " + Bookmarks.POSITION + ", " + Bookmarks.URL,

                    null);

            c.setNotificationUri(getContext().getContentResolver(),
                                 BrowserContract.AUTHORITY_URI);

            // Force the cursor to be compiled and the cursor-window filled now:
            // (A) without compiling the cursor now we won't have access to the TEMP table which
            // is removed as soon as we close our connection.
            // (B) this might also mitigate the situation causing this crash where we're accessing
            // a cursor and crashing in fillWindow.
            c.moveToFirst();

            db.setTransactionSuccessful();
            return c;
        } finally {
            db.endTransaction();
        }
    }

    public Cursor getHighlightCandidates(final SQLiteDatabase db, String limit) {
        final String query = "SELECT " +
                DBUtils.qualifyColumn(History.TABLE_NAME, History.URL) + " AS " + Highlights.URL + ", " +
                DBUtils.qualifyColumn(History.TABLE_NAME, History.VISITS) + ", " +
                DBUtils.qualifyColumn(History.TABLE_NAME, History.DATE_LAST_VISITED) + ", " +
                DBUtils.qualifyColumn(PageMetadata.TABLE_NAME, PageMetadata.JSON) + " AS " + Highlights.METADATA + ", " +
                DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.DATE_CREATED) + ", " +

                DBUtils.qualifyColumn(History.TABLE_NAME, History.TITLE) + " AS " + Highlights.TITLE + ", " +
                DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.POSITION) + " AS " + Highlights.POSITION + ", " +
                DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.PARENT) + " AS " + Highlights.PARENT + ", " +
                DBUtils.qualifyColumn(History.TABLE_NAME, History._ID) + " AS " + Highlights.HISTORY_ID + ", " +

                /**
                 * NB: Highlights filtering depends on BOOKMARK_ID, so changes to this logic should also update
                 * {@link org.mozilla.gecko.activitystream.ranking.HighlightsRanking#filterOutItemsPreffedOff(List, boolean, boolean)}
                 */
                "CASE WHEN " + DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks._ID) + " IS NOT NULL "
                + "AND " + DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.PARENT) + " IS NOT " + Bookmarks.FIXED_PINNED_LIST_ID + " "
                + "THEN " + DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks._ID) + " "
                + "ELSE -1 "
                + "END AS " + Highlights.BOOKMARK_ID + ", " +

                "CASE WHEN " + DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.DATE_CREATED) + " IS NOT NULL "
                    + "THEN " + DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.DATE_CREATED) + " "
                    + "ELSE " + DBUtils.qualifyColumn(History.TABLE_NAME, History.DATE_LAST_VISITED) + " "
                    + "END AS " + Highlights.DATE + " " +

                "FROM " + History.TABLE_NAME + " " +
                "LEFT JOIN " + Bookmarks.TABLE_NAME + " ON " +
                    DBUtils.qualifyColumn(History.TABLE_NAME, History.URL) + " = " +
                    DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.URL) + " " +
                // 1:1 relationship (Metadata is added via INSERT OR REPLACE)
                "LEFT JOIN " + PageMetadata.TABLE_NAME + " ON " +
                    DBUtils.qualifyColumn(History.TABLE_NAME, History.GUID) + " = " +
                    DBUtils.qualifyColumn(PageMetadata.TABLE_NAME, PageMetadata.HISTORY_GUID) + " " +
                "WHERE " + DBUtils.qualifyColumn(History.TABLE_NAME, History.URL) + " NOT IN (SELECT " + ActivityStreamBlocklist.URL + " FROM " + ActivityStreamBlocklist.TABLE_NAME + " ) " +
                "AND " + DBUtils.qualifyColumn(History.TABLE_NAME, History.IS_DELETED) + " IS NOT 1 " +
                "ORDER BY " + Highlights.DATE + " DESC " +
                "LIMIT " + limit;

        final Cursor cursor = db.rawQuery(query, null);
        final Context context = getContext();

        if (cursor != null && context != null) {
            cursor.setNotificationUri(context.getContentResolver(), BrowserContract.AUTHORITY_URI);
        }

        return cursor;
    }

    @Override
    @SuppressWarnings("fallthrough")
    public Cursor query(Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder) {
        final int match = URI_MATCHER.match(uri);

        // Handle only queries requiring a writable DB connection here: most queries need only a readable
        // connection, hence we can get a readable DB once, and then handle most queries within a switch.
        // TopSites requires a writable connection (because of the temporary tables it uses), hence
        // we handle that separately, i.e. before retrieving a readable connection.
        if (match == TOPSITES) {
            return getTopSites(uri);
        }

        SQLiteDatabase db = getReadableDatabase(uri);

        SQLiteQueryBuilder qb = new SQLiteQueryBuilder();
        String limit = uri.getQueryParameter(BrowserContract.PARAM_LIMIT);
        String groupBy = null;

        switch (match) {
            case BOOKMARKS_FOLDER_ID:
            case BOOKMARKS_ID:
            case BOOKMARKS: {
                debug("Query is on bookmarks: " + uri);

                if (match == BOOKMARKS_ID) {
                    selection = DatabaseUtils.concatenateWhere(selection, Bookmarks._ID + " = ?");
                    selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                            new String[] { Long.toString(ContentUris.parseId(uri)) });
                } else if (match == BOOKMARKS_FOLDER_ID) {
                    selection = DatabaseUtils.concatenateWhere(selection, Bookmarks.PARENT + " = ?");
                    selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                            new String[] { Long.toString(ContentUris.parseId(uri)) });
                }

                if (!shouldShowDeleted(uri))
                    selection = DatabaseUtils.concatenateWhere(Bookmarks.IS_DELETED + " = 0", selection);

                if (TextUtils.isEmpty(sortOrder)) {
                    sortOrder = DEFAULT_BOOKMARKS_SORT_ORDER;
                } else {
                    debug("Using sort order " + sortOrder + ".");
                }

                qb.setProjectionMap(BOOKMARKS_PROJECTION_MAP);

                if (hasFaviconsInProjection(projection)) {
                    qb.setTables(VIEW_BOOKMARKS_WITH_FAVICONS);
                } else if (selection != null && selection.contains(Bookmarks.ANNOTATION_KEY)) {
                    qb.setTables(VIEW_BOOKMARKS_WITH_ANNOTATIONS);

                    groupBy = uri.getQueryParameter(BrowserContract.PARAM_GROUP_BY);
                } else {
                    qb.setTables(TABLE_BOOKMARKS);
                }

                break;
            }

            case HISTORY_ID:
                selection = DatabaseUtils.concatenateWhere(selection, History._ID + " = ?");
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case HISTORY: {
                debug("Query is on history: " + uri);

                if (!shouldShowDeleted(uri))
                    selection = DatabaseUtils.concatenateWhere(History.IS_DELETED + " = 0", selection);

                if (TextUtils.isEmpty(sortOrder))
                    sortOrder = DEFAULT_HISTORY_SORT_ORDER;

                qb.setProjectionMap(HISTORY_PROJECTION_MAP);

                if (hasFaviconsInProjection(projection))
                    qb.setTables(VIEW_HISTORY_WITH_FAVICONS);
                else
                    qb.setTables(TABLE_HISTORY);

                break;
            }

            case VISITS:
                debug("Query is on visits: " + uri);
                qb.setProjectionMap(VISIT_PROJECTION_MAP);
                qb.setTables(TABLE_VISITS);

                if (TextUtils.isEmpty(sortOrder)) {
                    sortOrder = DEFAULT_VISITS_SORT_ORDER;
                }
                break;

            case FAVICON_ID:
                selection = DatabaseUtils.concatenateWhere(selection, Favicons._ID + " = ?");
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case FAVICONS: {
                debug("Query is on favicons: " + uri);

                qb.setProjectionMap(FAVICONS_PROJECTION_MAP);
                qb.setTables(TABLE_FAVICONS);

                break;
            }

            case THUMBNAIL_ID:
                selection = DatabaseUtils.concatenateWhere(selection, Thumbnails._ID + " = ?");
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case THUMBNAILS: {
                debug("Query is on thumbnails: " + uri);

                qb.setProjectionMap(THUMBNAILS_PROJECTION_MAP);
                qb.setTables(TABLE_THUMBNAILS);

                break;
            }

            case URL_ANNOTATIONS:
                debug("Query is on url annotations: " + uri);

                qb.setProjectionMap(URL_ANNOTATIONS_PROJECTION_MAP);
                qb.setTables(TABLE_URL_ANNOTATIONS);
                break;

            case SCHEMA: {
                debug("Query is on schema.");
                MatrixCursor schemaCursor = new MatrixCursor(new String[] { Schema.VERSION });
                schemaCursor.newRow().add(BrowserDatabaseHelper.DATABASE_VERSION);

                return schemaCursor;
            }

            case COMBINED: {
                debug("Query is on combined: " + uri);

                if (TextUtils.isEmpty(sortOrder))
                    sortOrder = DEFAULT_HISTORY_SORT_ORDER;

                // This will avoid duplicate entries in the awesomebar
                // results when a history entry has multiple bookmarks.
                groupBy = Combined.URL;

                qb.setProjectionMap(COMBINED_PROJECTION_MAP);

                if (hasFaviconsInProjection(projection))
                    qb.setTables(VIEW_COMBINED_WITH_FAVICONS);
                else
                    qb.setTables(Combined.VIEW_NAME);

                break;
            }

            case HIGHLIGHT_CANDIDATES:
                debug("Highlight candidates query: " + uri);
                return getHighlightCandidates(db, limit);

            case PAGE_METADATA: {
                debug("PageMetadata query: " + uri);

                qb.setProjectionMap(PAGE_METADATA_PROJECTION_MAP);
                qb.setTables(TABLE_PAGE_METADATA);
                break;
            }

            case REMOTE_DEVICES_ID:
                selection = DatabaseUtils.concatenateWhere(selection, RemoteDevices._ID + " = ?");
                selectionArgs = DatabaseUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case REMOTE_DEVICES: {
                debug("FxA devices query: " + uri);

                qb.setProjectionMap(REMOTE_DEVICES_PROJECTION_MAP);
                qb.setTables(TABLE_REMOTE_DEVICES);

                break;
            }

            default: {
                Table table = findTableFor(match);
                if (table == null) {
                    throw new UnsupportedOperationException("Unknown query URI " + uri);
                }
                trace("Update TABLE: " + uri);
                return table.query(db, uri, match, projection, selection, selectionArgs, sortOrder, groupBy, limit);
            }
        }

        trace("Running built query.");
        Cursor cursor = qb.query(db, projection, selection, selectionArgs, groupBy,
                null, sortOrder, limit);
        cursor.setNotificationUri(getContext().getContentResolver(),
                BrowserContract.AUTHORITY_URI);

        return cursor;
    }

    /**
     * Update the positions of bookmarks in batches.
     *
     * Begins and ends its own transactions.
     *
     * @see #updateBookmarkPositionsInTransaction(SQLiteDatabase, String[], int, int)
     */
    private int updateBookmarkPositions(Uri uri, String[] guids) {
        if (guids == null) {
            return 0;
        }

        int guidsCount = guids.length;
        if (guidsCount == 0) {
            return 0;
        }

        int offset = 0;
        int updated = 0;

        final SQLiteDatabase db = getWritableDatabase(uri);
        db.beginTransaction();

        while (offset < guidsCount) {
            try {
                updated += updateBookmarkPositionsInTransaction(db, guids, offset,
                                                                MAX_POSITION_UPDATES_PER_QUERY);
            } catch (SQLException e) {
                Log.e(LOGTAG, "Got SQLite exception updating bookmark positions at offset " + offset, e);

                // Need to restart the transaction.
                // The only way a caller knows that anything failed is that the
                // returned update count will be smaller than the requested
                // number of records.
                db.setTransactionSuccessful();
                db.endTransaction();

                db.beginTransaction();
            }

            offset += MAX_POSITION_UPDATES_PER_QUERY;
        }

        db.setTransactionSuccessful();
        db.endTransaction();

        return updated;
    }

    /**
     * Construct and execute an update expression that will modify the positions
     * of records in-place.
     */
    private static int updateBookmarkPositionsInTransaction(final SQLiteDatabase db, final String[] guids,
                                                            final int offset, final int max) {
        int guidsCount = guids.length;
        int processCount = Math.min(max, guidsCount - offset);

        // Each must appear twice: once in a CASE, and once in the IN clause.
        String[] args = new String[processCount * 2];
        System.arraycopy(guids, offset, args, 0, processCount);
        System.arraycopy(guids, offset, args, processCount, processCount);

        StringBuilder b = new StringBuilder("UPDATE " + TABLE_BOOKMARKS +
                                            " SET " + Bookmarks.POSITION +
                                            " = CASE guid");

        // Build the CASE statement body for GUID/index pairs from offset up to
        // the computed limit.
        final int end = offset + processCount;
        int i = offset;
        for (; i < end; ++i) {
            if (guids[i] == null) {
                // We don't want to issue the query if not every GUID is specified.
                debug("updateBookmarkPositions called with null GUID at index " + i);
                return 0;
            }
            b.append(" WHEN ? THEN " + i);
        }

        b.append(" END WHERE " + DBUtils.computeSQLInClause(processCount, Bookmarks.GUID));
        db.execSQL(b.toString(), args);

        // We can't easily get a modified count without calling something like changes().
        return processCount;
    }

    /**
     * Construct an update expression that will modify the parents of any records
     * that match.
     */
    private int updateBookmarkParents(SQLiteDatabase db, Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        if (selectionArgs != null) {
            trace("Updating bookmark parents of " + selection + " (" + selectionArgs[0] + ")");
        } else {
            trace("Updating bookmark parents of " + selection);
        }

        String where = Bookmarks._ID + " IN (" +
                       " SELECT DISTINCT " + Bookmarks.PARENT +
                       " FROM " + TABLE_BOOKMARKS +
                       " WHERE " + selection + " )";

        final int changed;
        if (!isCallerSync(uri)) {
            changed = updateAndIncrementLocalVersion(db, uri, TABLE_BOOKMARKS, values, where, selectionArgs);
        } else {
            changed = db.update(TABLE_BOOKMARKS, values, where, selectionArgs);
        }

        return changed;
    }

    private long insertBookmark(Uri uri, ContentValues values) {
        // Generate values if not specified. Don't overwrite
        // if specified by caller.
        long now = System.currentTimeMillis();
        if (!values.containsKey(Bookmarks.DATE_CREATED)) {
            values.put(Bookmarks.DATE_CREATED, now);
        }

        if (!values.containsKey(Bookmarks.DATE_MODIFIED)) {
            values.put(Bookmarks.DATE_MODIFIED, now);
        }

        if (!values.containsKey(Bookmarks.GUID)) {
            values.put(Bookmarks.GUID, Utils.generateGuid());
        }

        if (!values.containsKey(Bookmarks.POSITION)) {
            debug("Inserting bookmark with no position for URI");
            values.put(Bookmarks.POSITION,
                       Long.toString(BrowserContract.Bookmarks.DEFAULT_POSITION));
        }

        if (!values.containsKey(Bookmarks.TITLE)) {
            // Desktop Places barfs on insertion of a bookmark with no title,
            // so we don't store them that way.
            values.put(Bookmarks.TITLE, "");
        }

        String url = values.getAsString(Bookmarks.URL);

        // It's important that insertions from sync do not trigger another sync to start; we mark
        // records inserted from sync as "synced", except if explicitly given a starting localVersion.
        if (isCallerSync(uri)) {
            // Sync might ask to insert record as modified if it modified incoming record and needs
            // it to be re-uploaded.
            if (values.containsKey(Bookmarks.PARAM_INSERT_FROM_SYNC_AS_MODIFIED)
                    && values.getAsBoolean(Bookmarks.PARAM_INSERT_FROM_SYNC_AS_MODIFIED)) {
                values.put(Bookmarks.LOCAL_VERSION, 2);
            } else {
                values.put(Bookmarks.LOCAL_VERSION, 1);
            }
            values.put(Bookmarks.SYNC_VERSION, 1);

            values.remove(Bookmarks.PARAM_INSERT_FROM_SYNC_AS_MODIFIED);

         // Insertions from Fennec are always set as (1,0) to trigger an upload of these records.
        } else {
            values.put(Bookmarks.LOCAL_VERSION, 1);
            values.put(Bookmarks.SYNC_VERSION, 0);
        }

        debug("Inserting bookmark in database with URL: " + url);
        final SQLiteDatabase db = getWritableDatabase(uri);
        beginWrite(db);
        final long insertedId = db.insertOrThrow(TABLE_BOOKMARKS, Bookmarks.TITLE, values);

        if (insertedId == -1) {
            Log.e(LOGTAG, "Unable to insert bookmark in database with URL: " + url);
            return insertedId;
        }

        if (isCallerSync(uri)) {
            // Sync will handle timestamps on its own, so we don't perform the update here.
            return insertedId;
        }

        // Bump parent's lastModified timestamp.
        final long lastModified = values.getAsLong(Bookmarks.DATE_MODIFIED);
        final ContentValues parentValues = new ContentValues();
        parentValues.put(Bookmarks.DATE_MODIFIED, lastModified);

        // The ContentValues should have parentId, or the insertion above would fail because of
        // database schema foreign key constraint.
        final long parentId = values.getAsLong(Bookmarks.PARENT);
        final String parentSelection = Bookmarks._ID + " = ?";
        final String[] parentSelectionArgs = new String[] { String.valueOf(parentId) };
        updateAndIncrementLocalVersion(db, uri, TABLE_BOOKMARKS, parentValues, parentSelection, parentSelectionArgs);

        return insertedId;
    }

    private int updateOrInsertBookmark(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        int updated = updateBookmarks(uri, values, selection, selectionArgs);
        if (updated > 0) {
            return updated;
        }

        // Transaction already begun by updateBookmarks.
        if (0 <= insertBookmark(uri, values)) {
            // We 'updated' one row.
            return 1;
        }

        // If something went wrong, then we updated zero rows.
        return 0;
    }

    private int updateBookmarks(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        trace("Updating bookmarks on URI: " + uri);

        final String[] bookmarksProjection = new String[] {
                Bookmarks._ID, // 0
        };

        if (!values.containsKey(Bookmarks.DATE_MODIFIED)) {
            values.put(Bookmarks.DATE_MODIFIED, System.currentTimeMillis());
        }

        trace("Querying bookmarks to update on URI: " + uri);
        final SQLiteDatabase db = getWritableDatabase(uri);

        // Compute matching IDs.
        final Cursor cursor = db.query(TABLE_BOOKMARKS, bookmarksProjection,
                selection, selectionArgs, null, null, null);

        // Now that we're done reading, open a transaction.
        final String inClause;
        try {
            inClause = DBUtils.computeSQLInClauseFromLongs(cursor, Bookmarks._ID);
        } finally {
            cursor.close();
        }

        beginWrite(db);

        int updated;
        // If the update is coming from Sync, check if it explicitly asked to increment localVersion.
        if (!isCallerSync(uri) || shouldIncrementLocalVersionFromSync(uri)) {
            updated = updateAndIncrementLocalVersion(db, uri, TABLE_BOOKMARKS, values, inClause, null);
        } else {
            updated = db.update(TABLE_BOOKMARKS, values, inClause, null);
        }

        if (updated == 0) {
            trace("No update on URI: " + uri);
            return updated;
        }

        // If the update is coming from Sync, we're done.
        // Sync will handle timestamps of parent records on its own.
        if (isCallerSync(uri)) {
            return updated;
        }

        final long oldParentId = getOldParentIdIfParentChanged(uri);
        if (oldParentId == -1) {
            // Parent isn't changed, don't bump its timestamps.
            return updated;
        }

        final long newParentId = values.getAsLong(Bookmarks.PARENT);
        final long lastModified = values.getAsLong(Bookmarks.DATE_MODIFIED);
        final ContentValues parentValues = new ContentValues();
        parentValues.put(Bookmarks.DATE_MODIFIED, lastModified);

        // Bump old/new parent's lastModified timestamps.
        final String parentSelection = Bookmarks._ID + " IN (?, ?)";
        final String[] parentSelectionArgs = new String[] { String.valueOf(oldParentId), String.valueOf(newParentId) };
        updated += updateAndIncrementLocalVersion(db, uri, TABLE_BOOKMARKS, parentValues, parentSelection, parentSelectionArgs);

        return updated;
    }

    /**
     * Use the query key {@link BrowserContract#PARAM_OLD_BOOKMARK_PARENT} to check if parent is changed or not.
     *
     * @return old parent id if uri has the key, or -1 otherwise.
     */
    private long getOldParentIdIfParentChanged(Uri uri) {
        final String oldParentId = uri.getQueryParameter(BrowserContract.PARAM_OLD_BOOKMARK_PARENT);
        if (TextUtils.isEmpty(oldParentId)) {
            return -1;
        }

        try {
            return Long.parseLong(oldParentId);
        } catch (NumberFormatException ignored) {
            return -1;
        }
    }

    private long insertHistory(Uri uri, ContentValues values) {
        final long now = System.currentTimeMillis();
        values.put(History.DATE_CREATED, now);
        values.put(History.DATE_MODIFIED, now);

        // Generate GUID for new history entry. Don't override specified GUIDs.
        if (!values.containsKey(History.GUID)) {
          values.put(History.GUID, Utils.generateGuid());
        }

        String url = values.getAsString(History.URL);

        debug("Inserting history in database with URL: " + url);
        final SQLiteDatabase db = getWritableDatabase(uri);
        beginWrite(db);
        return db.insertOrThrow(TABLE_HISTORY, History.VISITS, values);
    }

    private int updateOrInsertHistory(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        final int updated = updateHistory(uri, values, selection, selectionArgs);
        if (updated > 0) {
            return updated;
        }

        // Insert a new entry if necessary, setting visit and date aggregate values.
        if (!values.containsKey(History.VISITS)) {
            values.put(History.VISITS, 1);
            values.put(History.LOCAL_VISITS, 1);
        } else {
            values.put(History.LOCAL_VISITS, values.getAsInteger(History.VISITS));
        }
        if (values.containsKey(History.DATE_LAST_VISITED)) {
            values.put(History.LOCAL_DATE_LAST_VISITED, values.getAsLong(History.DATE_LAST_VISITED));
        }
        if (!values.containsKey(History.TITLE)) {
            values.put(History.TITLE, values.getAsString(History.URL));
        }

        if (0 <= insertHistory(uri, values)) {
            return 1;
        }

        return 0;
    }

    private int updateHistory(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        trace("Updating history on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);

        if (!values.containsKey(History.DATE_MODIFIED)) {
            values.put(History.DATE_MODIFIED, System.currentTimeMillis());
        }

        // Use the simple code path for easy updates.
        if (!shouldIncrementVisits(uri) && !shouldIncrementRemoteAggregates(uri)) {
            trace("Updating history meta data only");
            return db.update(TABLE_HISTORY, values, selection, selectionArgs);
        }

        trace("Updating history meta data and incrementing visits");

        if (values.containsKey(History.DATE_LAST_VISITED)) {
            values.put(History.LOCAL_DATE_LAST_VISITED, values.getAsLong(History.DATE_LAST_VISITED));
        }

        // Create a separate set of values that will be updated as an expression.
        final ContentValues visits = new ContentValues();
        if (shouldIncrementVisits(uri)) {
            // Update data and increment visits by 1.
            final long incVisits = 1;

            visits.put(History.VISITS, History.VISITS + " + " + incVisits);
            visits.put(History.LOCAL_VISITS, History.LOCAL_VISITS + " + " + incVisits);
        }

        if (shouldIncrementRemoteAggregates(uri)) {
            // Let's fail loudly instead of trying to assume what users of this API meant to do.
            if (!values.containsKey(History.REMOTE_VISITS)) {
                throw new IllegalArgumentException(
                        "Tried incrementing History.REMOTE_VISITS by unknown value");
            }
            visits.put(
                    History.REMOTE_VISITS,
                    History.REMOTE_VISITS + " + " + values.getAsInteger(History.REMOTE_VISITS)
            );
            // Need to remove passed in value, so that we increment REMOTE_VISITS, and not just set it.
            values.remove(History.REMOTE_VISITS);
        }

        final ContentValues[] valuesAndVisits = { values,  visits };
        final UpdateOperation[] ops = { UpdateOperation.ASSIGN, UpdateOperation.EXPRESSION };

        return DBUtils.updateArrays(db, TABLE_HISTORY, valuesAndVisits, ops, selection, selectionArgs);
    }

    private long insertVisitForHistory(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        trace("Inserting visit for history on URI: " + uri);

        final SQLiteDatabase db = getReadableDatabase(uri);

        final Cursor cursor = db.query(
                History.TABLE_NAME, new String[] {History.GUID}, selection, selectionArgs,
                null, null, null);
        if (cursor == null) {
            Log.e(LOGTAG, "Null cursor while trying to insert visit for history URI: " + uri);
            return 0;
        }
        final ContentValues[] visitValues;
        try {
            visitValues = new ContentValues[cursor.getCount()];

            if (!cursor.moveToFirst()) {
                Log.e(LOGTAG, "No history records found while inserting visit(s) for history URI: " + uri);
                return 0;
            }

            // Sync works in microseconds, so we store visit timestamps in microseconds as well.
            // History timestamps are in milliseconds.
            // This is the conversion point for locally generated visits.
            final long visitDate;
            if (values.containsKey(History.DATE_LAST_VISITED)) {
                visitDate = values.getAsLong(History.DATE_LAST_VISITED) * 1000;
            } else {
                visitDate = System.currentTimeMillis() * 1000;
            }

            final int guidColumn = cursor.getColumnIndexOrThrow(History.GUID);
            while (!cursor.isAfterLast()) {
                final ContentValues visit = new ContentValues();
                visit.put(Visits.HISTORY_GUID, cursor.getString(guidColumn));
                visit.put(Visits.DATE_VISITED, visitDate);
                visitValues[cursor.getPosition()] = visit;
                cursor.moveToNext();
            }
        } finally {
            cursor.close();
        }

        if (visitValues.length == 1) {
            return insertVisit(Visits.CONTENT_URI, visitValues[0]);
        } else {
            return bulkInsert(Visits.CONTENT_URI, visitValues);
        }
    }

    private long insertVisit(Uri uri, ContentValues values) {
        final SQLiteDatabase db = getWritableDatabase(uri);

        debug("Inserting history in database with URL: " + uri);
        beginWrite(db);

        // We ignore insert conflicts here to simplify inserting visits records coming in from Sync.
        // Visits table has a unique index on (history_guid,date), so a conflict might arise when we're
        // trying to insert history record visits coming in from sync which are already present locally
        // as a result of previous sync operations.
        // An alternative to doing this is to filter out already present records when we're doing history inserts
        // from Sync, which is a costly operation to do en masse.
        return db.insertWithOnConflict(
                TABLE_VISITS, null, values, SQLiteDatabase.CONFLICT_IGNORE);
    }

    private void updateFaviconIdsForUrl(SQLiteDatabase db, String pageUrl, Long faviconId) {
        ContentValues updateValues = new ContentValues(1);
        updateValues.put(FaviconColumns.FAVICON_ID, faviconId);
        db.update(TABLE_HISTORY,
                  updateValues,
                  History.URL + " = ?",
                  new String[] { pageUrl });
        db.update(TABLE_BOOKMARKS,
                  updateValues,
                  Bookmarks.URL + " = ?",
                  new String[] { pageUrl });
    }

    private long insertFavicon(Uri uri, ContentValues values) {
        return insertFavicon(getWritableDatabase(uri), values);
    }

    private long insertFavicon(SQLiteDatabase db, ContentValues values) {
        String faviconUrl = values.getAsString(Favicons.URL);
        String pageUrl = null;

        trace("Inserting favicon for URL: " + faviconUrl);

        DBUtils.stripEmptyByteArray(values, Favicons.DATA);

        // Extract the page URL from the ContentValues
        if (values.containsKey(Favicons.PAGE_URL)) {
            pageUrl = values.getAsString(Favicons.PAGE_URL);
            values.remove(Favicons.PAGE_URL);
        }

        // If no URL is provided, insert using the default one.
        if (TextUtils.isEmpty(faviconUrl) && !TextUtils.isEmpty(pageUrl)) {
            values.put(Favicons.URL, IconsHelper.guessDefaultFaviconURL(pageUrl));
        }

        final long now = System.currentTimeMillis();
        values.put(Favicons.DATE_CREATED, now);
        values.put(Favicons.DATE_MODIFIED, now);

        beginWrite(db);
        final long faviconId = db.insertOrThrow(TABLE_FAVICONS, null, values);

        if (pageUrl != null) {
            updateFaviconIdsForUrl(db, pageUrl, faviconId);
        }
        return faviconId;
    }

    private int updateOrInsertFavicon(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        return updateFavicon(uri, values, selection, selectionArgs,
                true /* insert if needed */);
    }

    private int updateExistingFavicon(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        return updateFavicon(uri, values, selection, selectionArgs,
                false /* only update, no insert */);
    }

    private int updateFavicon(Uri uri, ContentValues values, String selection,
            String[] selectionArgs, boolean insertIfNeeded) {
        String faviconUrl = values.getAsString(Favicons.URL);
        String pageUrl = null;
        int updated = 0;
        Long faviconId = null;
        long now = System.currentTimeMillis();

        trace("Updating favicon for URL: " + faviconUrl);

        DBUtils.stripEmptyByteArray(values, Favicons.DATA);

        // Extract the page URL from the ContentValues
        if (values.containsKey(Favicons.PAGE_URL)) {
            pageUrl = values.getAsString(Favicons.PAGE_URL);
            values.remove(Favicons.PAGE_URL);
        }

        values.put(Favicons.DATE_MODIFIED, now);

        final SQLiteDatabase db = getWritableDatabase(uri);

        // If there's no favicon URL given and we're inserting if needed, skip
        // the update and only do an insert (otherwise all rows would be
        // updated).
        if (!(insertIfNeeded && (faviconUrl == null))) {
            updated = db.update(TABLE_FAVICONS, values, selection, selectionArgs);
        }

        if (updated > 0) {
            if ((faviconUrl != null) && (pageUrl != null)) {
                final Cursor cursor = db.query(TABLE_FAVICONS,
                                               new String[] { Favicons._ID },
                                               Favicons.URL + " = ?",
                                               new String[] { faviconUrl },
                                               null, null, null);
                try {
                    if (cursor.moveToFirst()) {
                        faviconId = cursor.getLong(cursor.getColumnIndexOrThrow(Favicons._ID));
                    }
                } finally {
                    cursor.close();
                }
            }
            if (pageUrl != null) {
                beginWrite(db);
            }
        } else if (insertIfNeeded) {
            values.put(Favicons.DATE_CREATED, now);

            trace("No update, inserting favicon for URL: " + faviconUrl);
            beginWrite(db);
            faviconId = db.insert(TABLE_FAVICONS, null, values);
            updated = 1;
        }

        if (pageUrl != null) {
            updateFaviconIdsForUrl(db, pageUrl, faviconId);
        }

        return updated;
    }

    private long insertThumbnail(Uri uri, ContentValues values) {
        final String url = values.getAsString(Thumbnails.URL);

        trace("Inserting thumbnail for URL: " + url);

        DBUtils.stripEmptyByteArray(values, Thumbnails.DATA);

        final SQLiteDatabase db = getWritableDatabase(uri);
        beginWrite(db);
        return db.insertOrThrow(TABLE_THUMBNAILS, null, values);
    }

    private long insertActivityStreamBlocklistSite(final Uri uri, final ContentValues values) {
        final String url = values.getAsString(ActivityStreamBlocklist.URL);
        trace("Inserting url into highlights blocklist, URL: " + url);

        final SQLiteDatabase db = getWritableDatabase(uri);
        values.put(ActivityStreamBlocklist.CREATED, System.currentTimeMillis());

        beginWrite(db);
        return db.insertOrThrow(TABLE_ACTIVITY_STREAM_BLOCKLIST, null, values);
    }

    private long insertPageMetadata(final Uri uri, final ContentValues values) {
        final SQLiteDatabase db = getWritableDatabase(uri);

        if (!values.containsKey(PageMetadata.DATE_CREATED)) {
            values.put(PageMetadata.DATE_CREATED, System.currentTimeMillis());
        }

        beginWrite(db);

        // Perform INSERT OR REPLACE, there might be page metadata present and we want to replace it.
        // Depends on a conflict arising from unique foreign key (history_guid) constraint violation.
        return db.insertWithOnConflict(
                TABLE_PAGE_METADATA, null, values, SQLiteDatabase.CONFLICT_REPLACE);
    }

    private long insertFxADevice(final Uri uri, final ContentValues values) {
        final SQLiteDatabase db = getWritableDatabase(uri);

        beginWrite(db);
        return db.insertOrThrow(TABLE_REMOTE_DEVICES, null, values);
    }

    private long insertUrlAnnotation(final Uri uri, final ContentValues values) {
        final String url = values.getAsString(UrlAnnotations.URL);
        trace("Inserting url annotations for URL: " + url);

        final SQLiteDatabase db = getWritableDatabase(uri);
        beginWrite(db);
        return db.insertOrThrow(TABLE_URL_ANNOTATIONS, null, values);
    }

    private void deleteUrlAnnotation(final Uri uri, final String selection, final String[] selectionArgs) {
        trace("Deleting url annotation for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        db.delete(TABLE_URL_ANNOTATIONS, selection, selectionArgs);
    }

    private int deletePageMetadata(final Uri uri, final String selection, final String[] selectionArgs) {
        trace("Deleting page metadata for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        return db.delete(TABLE_PAGE_METADATA, selection, selectionArgs);
    }

    private int deleteRemoteDevices(final Uri uri, final String selection, final String[] selectionArgs) {
        trace("Deleting FxA Devices for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        return db.delete(TABLE_REMOTE_DEVICES, selection, selectionArgs);
    }

    private void updateUrlAnnotation(final Uri uri, final ContentValues values, final String selection, final String[] selectionArgs) {
        trace("Updating url annotation for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        db.update(TABLE_URL_ANNOTATIONS, values, selection, selectionArgs);
    }

    private int updateOrInsertThumbnail(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        return updateThumbnail(uri, values, selection, selectionArgs,
                true /* insert if needed */);
    }

    private int updateExistingThumbnail(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        return updateThumbnail(uri, values, selection, selectionArgs,
                false /* only update, no insert */);
    }

    private int updateThumbnail(Uri uri, ContentValues values, String selection,
            String[] selectionArgs, boolean insertIfNeeded) {
        final String url = values.getAsString(Thumbnails.URL);
        DBUtils.stripEmptyByteArray(values, Thumbnails.DATA);

        trace("Updating thumbnail for URL: " + url);

        final SQLiteDatabase db = getWritableDatabase(uri);
        beginWrite(db);
        int updated = db.update(TABLE_THUMBNAILS, values, selection, selectionArgs);

        if (updated == 0 && insertIfNeeded) {
            trace("No update, inserting thumbnail for URL: " + url);
            db.insert(TABLE_THUMBNAILS, null, values);
            updated = 1;
        }

        return updated;
    }

    /**
     * This method does not create a new transaction. Its first operation is
     * guaranteed to be a write, which in the case of a new enclosing
     * transaction will guarantee that a read does not need to be upgraded to
     * a write.
     */
    private int deleteHistory(SQLiteDatabase db, Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting history entry for URI: " + uri);

        if (isCallerSync(uri)) {
            return db.delete(TABLE_HISTORY, selection, selectionArgs);
        }

        debug("Marking history entry as deleted for URI: " + uri);

        ContentValues values = new ContentValues();
        values.put(History.IS_DELETED, 1);

        // Wipe sensitive data.
        values.putNull(History.TITLE);
        values.put(History.URL, "");          // Column is NOT NULL.
        values.put(History.DATE_CREATED, 0);
        values.put(History.DATE_LAST_VISITED, 0);
        values.put(History.VISITS, 0);
        values.put(History.DATE_MODIFIED, System.currentTimeMillis());

        // Doing this UPDATE (or the DELETE above) first ensures that the
        // first operation within a new enclosing transaction is a write.
        // The cleanup call below will do a SELECT first, and thus would
        // require the transaction to be upgraded from a reader to a writer.
        // In some cases that upgrade can fail (SQLITE_BUSY), so we avoid
        // it if we can.
        final int updated = db.update(TABLE_HISTORY, values, selection, selectionArgs);
        try {
            cleanUpSomeDeletedRecords(uri, TABLE_HISTORY);
        } catch (Exception e) {
            // We don't care.
            Log.e(LOGTAG, "Unable to clean up deleted history records: ", e);
        }
        return updated;
    }

    private ArrayList<String> getHistoryGUIDsFromSelection(SQLiteDatabase db, Uri uri, String selection, String[] selectionArgs) {
        final ArrayList<String> historyGUIDs = new ArrayList<>();

        final Cursor cursor = db.query(
                History.TABLE_NAME, new String[] {History.GUID}, selection, selectionArgs,
                null, null, null);
        if (cursor == null) {
            Log.e(LOGTAG, "Null cursor while trying to delete visits for history URI: " + uri);
            return historyGUIDs;
        }

        try {
            if (!cursor.moveToFirst()) {
                trace("No history items for which to remove visits matched for URI: " + uri);
                return historyGUIDs;
            }
            final int historyColumn = cursor.getColumnIndexOrThrow(History.GUID);
            while (!cursor.isAfterLast()) {
                historyGUIDs.add(cursor.getString(historyColumn));
                cursor.moveToNext();
            }
        } finally {
            cursor.close();
        }

        return historyGUIDs;
    }

    private int deletePageMetadataForHistory(SQLiteDatabase db, ArrayList<String> historyGUIDs) {
        return bulkDeleteByHistoryGUID(db, historyGUIDs, PageMetadata.TABLE_NAME, PageMetadata.HISTORY_GUID);
    }

    private int deleteVisitsForHistory(SQLiteDatabase db, ArrayList<String> historyGUIDs) {
        return bulkDeleteByHistoryGUID(db, historyGUIDs, Visits.TABLE_NAME, Visits.HISTORY_GUID);
    }

    private int bulkDeleteByHistoryGUID(SQLiteDatabase db, ArrayList<String> historyGUIDs, String table, String historyGUIDColumn) {
        // Due to SQLite's maximum variable limitation, we need to chunk our delete statements.
        // For example, if there were 1200 GUIDs, this will perform 2 delete statements if SQLITE_MAX_VARIABLE_NUMBER is 999.
        int deleted = 0;
        for (int chunk = 0; chunk <= historyGUIDs.size() / DBUtils.SQLITE_MAX_VARIABLE_NUMBER; chunk++) {
            final int chunkStart = chunk * DBUtils.SQLITE_MAX_VARIABLE_NUMBER;
            int chunkEnd = (chunk + 1) * DBUtils.SQLITE_MAX_VARIABLE_NUMBER;
            if (chunkEnd > historyGUIDs.size()) {
                chunkEnd = historyGUIDs.size();
            }
            final List<String> chunkGUIDs = historyGUIDs.subList(chunkStart, chunkEnd);
            deleted += db.delete(
                    table,
                    DBUtils.computeSQLInClause(chunkGUIDs.size(), historyGUIDColumn),
                    chunkGUIDs.toArray(new String[chunkGUIDs.size()])
            );
        }

        return deleted;
    }

    /**
     * Chunk our deletes around {@link DBUtils#SQLITE_MAX_VARIABLE_NUMBER} so that we don't stumble
     * into 'too many SQL variables' error.
     */
    private int bulkDeleteByBookmarkGUIDs(SQLiteDatabase db, Uri uri, List<String> bookmarkGUIDs) {
        // Due to SQLite's maximum variable limitation, we need to chunk our update statements.
        // For example, if there were 1200 GUIDs, this will perform 2 update statements.
        int updated = 0;

        // To be sync-friendly, we mark records as deleted while nulling-out actual values.
        // In Desktop Sync parlance, this is akin to storing a tombstone.
        final ContentValues values = new ContentValues();
        values.put(Bookmarks.IS_DELETED, 1);
        values.put(Bookmarks.POSITION, 0);
        values.putNull(Bookmarks.PARENT);
        values.putNull(Bookmarks.URL);
        values.putNull(Bookmarks.TITLE);
        values.putNull(Bookmarks.DESCRIPTION);
        values.putNull(Bookmarks.KEYWORD);
        values.putNull(Bookmarks.TAGS);
        values.putNull(Bookmarks.FAVICON_ID);

        values.put(Bookmarks.DATE_MODIFIED, System.currentTimeMillis());

        // Leave space for variables in values.
        final int maxVariableNumber = DBUtils.SQLITE_MAX_VARIABLE_NUMBER - values.size();

        for (int chunk = 0; chunk <= bookmarkGUIDs.size() / maxVariableNumber; chunk++) {
            final int chunkStart = chunk * maxVariableNumber;
            int chunkEnd = (chunk + 1) * maxVariableNumber;
            if (chunkEnd > bookmarkGUIDs.size()) {
                chunkEnd = bookmarkGUIDs.size();
            }
            final List<String> chunkGUIDs = bookmarkGUIDs.subList(chunkStart, chunkEnd);
            final String selection = DBUtils.computeSQLInClause(chunkGUIDs.size(), Bookmarks.GUID);
            final String[] selectionArgs = chunkGUIDs.toArray(new String[chunkGUIDs.size()]);
            updated += updateAndIncrementLocalVersion(db, uri, TABLE_BOOKMARKS, values, selection, selectionArgs);
        }

        return updated;
    }

    private int deleteVisits(Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting visits for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);

        beginWrite(db);
        return db.delete(TABLE_VISITS, selection, selectionArgs);
    }

    private int deleteBookmarks(Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting bookmarks for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        beginWrite(db);

        if (isCallerSync(uri)) {
            return db.delete(TABLE_BOOKMARKS, selection, selectionArgs);
        }

        debug("Marking bookmarks as deleted for URI: " + uri);

        // Deletions of bookmarks almost always affect more than one record, and so we keep track of
        // number of records changed as opposed to number of records deleted. It's highly unusual,
        // but not impossible, for a "delete" operation to modify some of the records without
        // actually marking any as "deleted".
        // We do this to ensure we correctly fire 'notifyChanged' events whenever > 0 records are touched.
        int changed = 0;

        // First, bump parents' lastModified timestamp. Running this 'update' query first ensures our
        // transaction will start off as a 'writer'. Deletion code below performs a SELECT first,
        // requiring transaction to be upgraded from a reader to a writer, which might result in SQL_BUSY.
        // NB: this code allows for multi-parent bookmarks.
        final ContentValues parentValues = new ContentValues();
        parentValues.put(Bookmarks.DATE_MODIFIED, System.currentTimeMillis());
        changed += updateBookmarkParents(db, uri, parentValues, selection, selectionArgs);

        // Finally, delete everything that needs to be deleted all at once.
        // We need to compute list of all bookmarks and their descendants first.
        // We calculate our deletion tree based on 'selection', and so above queries do not null-out
        // any of the bookmark fields. This will be done in `bulkDeleteByBookmarkGUIDs`.
        final List<String> guids = getBookmarkDescendantGUIDs(db, selection, selectionArgs);
        changed += bulkDeleteByBookmarkGUIDs(db, uri, guids);

        return changed;
    }

    /**
     * Get bookmark descendant IDs with conditions.
     * @return A list of bookmark GUID.
     */
    private List<String> getBookmarkDescendantGUIDs(SQLiteDatabase db, String selection, String[] selectionArgs) {
        // Get GUIDs from selection filter.
        final Cursor cursor = db.query(TABLE_BOOKMARKS,
                                       new String[] { Bookmarks._ID, Bookmarks.TYPE, Bookmarks.GUID },
                                       selection,
                                       selectionArgs,
                                       null, null, null);
        if (cursor == null) {
            return Collections.emptyList();
        }

        final List<String> guids = new ArrayList<>();
        final ArrayDeque<Long> folderQueue = new ArrayDeque<>();
        try {
            while (cursor.moveToNext()) {
                final String guid = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.GUID));
                guids.add(guid);

                final int type = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE));
                if (type == Bookmarks.TYPE_FOLDER) {
                    final long id = cursor.getLong(cursor.getColumnIndexOrThrow(Bookmarks._ID));
                    folderQueue.add(id);
                }
            }
        } finally {
            cursor.close();
        }

        // Keep finding descendant GUIDs from parent IDs.
        while (!folderQueue.isEmpty()) {
            // Store all parent IDs in a in clause, and can query their children at once.
            final String[] inClauseArgs = new String[folderQueue.size()];
            int count = 0;
            while (folderQueue.peek() != null) {
                final long id = folderQueue.poll();
                inClauseArgs[count++] = String.valueOf(id);
            }

            final String inClause = DBUtils.computeSQLInClause(count, Bookmarks.PARENT);
            // We only select distinct parent IDs.
            final Cursor c = db.query(true, TABLE_BOOKMARKS,
                                      new String[] { Bookmarks._ID, Bookmarks.TYPE, Bookmarks.GUID },
                                      inClause, inClauseArgs, null, null, null, null);
            if (c == null) {
                continue;
            }
            try {
                while (c.moveToNext()) {
                    final int type = c.getInt(c.getColumnIndexOrThrow(Bookmarks.TYPE));
                    if (type == Bookmarks.TYPE_FOLDER) {
                        final long id = c.getLong(c.getColumnIndexOrThrow(Bookmarks._ID));
                        folderQueue.add(id);
                    }

                    final String guid = c.getString(c.getColumnIndexOrThrow(Bookmarks.GUID));
                    guids.add(guid);
                }
            } finally {
                c.close();
            }
        }
        return guids;
    }

    private int deleteFavicons(Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting favicons for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);

        return db.delete(TABLE_FAVICONS, selection, selectionArgs);
    }

    private int deleteThumbnails(Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting thumbnails for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);

        return db.delete(TABLE_THUMBNAILS, selection, selectionArgs);
    }

    private int deleteUnusedImages(Uri uri) {
        debug("Deleting all unused favicons and thumbnails for URI: " + uri);

        String faviconSelection = Favicons._ID + " NOT IN "
                + "(SELECT " + History.FAVICON_ID
                + " FROM " + TABLE_HISTORY
                + " WHERE " + History.IS_DELETED + " = 0"
                + " AND " + History.FAVICON_ID + " IS NOT NULL"
                + " UNION ALL SELECT " + Bookmarks.FAVICON_ID
                + " FROM " + TABLE_BOOKMARKS
                + " WHERE " + Bookmarks.IS_DELETED + " = 0"
                + " AND " + Bookmarks.FAVICON_ID + " IS NOT NULL)";

        String thumbnailSelection = Thumbnails.URL + " NOT IN "
                + "(SELECT " + History.URL
                + " FROM " + TABLE_HISTORY
                + " WHERE " + History.IS_DELETED + " = 0"
                + " AND " + History.URL + " IS NOT NULL"
                + " UNION ALL SELECT " + Bookmarks.URL
                + " FROM " + TABLE_BOOKMARKS
                + " WHERE " + Bookmarks.IS_DELETED + " = 0"
                + " AND " + Bookmarks.URL + " IS NOT NULL)";

        return deleteFavicons(uri, faviconSelection, null) +
               deleteThumbnails(uri, thumbnailSelection, null) +
               getURLImageDataTable().deleteUnused(getWritableDatabase(uri));
    }

    @Nullable
    @Override
    public Bundle call(@NonNull String method, String uriArg, Bundle extras) {
        if (uriArg == null) {
            throw new IllegalArgumentException("Missing required Uri argument.");
        }
        final Bundle result = new Bundle();
        switch (method) {
            case BrowserContract.METHOD_INSERT_HISTORY_WITH_VISITS_FROM_SYNC:
                try {
                    final Uri uri = Uri.parse(uriArg);
                    final SQLiteDatabase db = getWritableDatabase(uri);
                    bulkInsertHistoryWithVisits(db, extras);
                    result.putSerializable(BrowserContract.METHOD_RESULT, null);

                // If anything went wrong during insertion, we know that changes were rolled back.
                // Inform our caller that we have failed.
                } catch (RuntimeException e) {
                    throw e;
                } catch (Exception e) {
                    Log.e(LOGTAG, "Unexpected error while bulk inserting history", e);
                    result.putSerializable(BrowserContract.METHOD_RESULT, e);
                }
                break;
            case BrowserContract.METHOD_REPLACE_REMOTE_CLIENTS:
                try {
                    final Uri uri = Uri.parse(uriArg);
                    bulkReplaceRemoteDevices(uri, extras);
                    result.putSerializable(BrowserContract.METHOD_RESULT, null);

                    // If anything went wrong during insertion, we know that changes were rolled back.
                    // Inform our caller that we have failed.
                } catch (RuntimeException e) {
                    throw e;
                } catch (Exception e) {
                    Log.e(LOGTAG, "Unexpected error while bulk inserting remote clients", e);
                    result.putSerializable(BrowserContract.METHOD_RESULT, e);
                }
                break;
            case BrowserContract.METHOD_UPDATE_SYNC_VERSIONS:
                try {
                    final Uri uri = Uri.parse(uriArg);
                    final SQLiteDatabase db = getWritableDatabase(uri);
                    final int changed = bulkUpdateSyncVersions(db, uri, extras);
                    result.putSerializable(BrowserContract.METHOD_RESULT, changed);
                    // If anything went wrong during bulk operation, let our caller know.
                } catch (RuntimeException e) {
                    throw e;
                } catch (Exception e) {
                    Log.e(LOGTAG, "Unexpected error while bulk updating sync versions", e);
                    result.putSerializable(BrowserContract.METHOD_RESULT, e);
                }
                break;
            case BrowserContract.METHOD_UPDATE_BY_GUID_ASSERTING_LOCAL_VERSION:
                try {
                    final Uri uri = Uri.parse(uriArg);
                    final SQLiteDatabase db = getWritableDatabase(uri);
                    final boolean didUpdate = updateBookmarkByGuidAssertingLocalVersion(uri, db, extras);
                    result.putSerializable(BrowserContract.METHOD_RESULT, didUpdate);
                } catch (RuntimeException e) {
                    throw e;
                } catch (Exception e) {
                    Log.e(LOGTAG, "Unexpected error while resetting record versioning", e);
                    result.putSerializable(BrowserContract.METHOD_RESULT, e);
                }
                break;
            case BrowserContract.METHOD_RESET_RECORD_VERSIONS:
                try {
                    final Uri uri = Uri.parse(uriArg);
                    final SQLiteDatabase db = getWritableDatabase(uri);
                    final int changed = bulkResetRecordVersions(uri, db);
                    result.putSerializable(BrowserContract.METHOD_RESULT, changed);
                } catch (RuntimeException e) {
                    throw e;
                } catch (Exception e) {
                    Log.e(LOGTAG, "Unexpected error while resetting record versioning", e);
                    result.putSerializable(BrowserContract.METHOD_RESULT, e);
                }
                break;
            default:
                throw new IllegalArgumentException("Unknown method call: " + method);
        }

        return result;
    }

    private int bulkResetRecordVersions(Uri uri, SQLiteDatabase db) {
        // Whenever sync is disconnected, we need to reset our record versioning to ensure that
        // upon connecting sync in the future, all versioned records will be processed correctly.

        // We reset versions to a basic "sync needed and record doesn't exist elsewhere" state:
        // - localVersion=1
        // - syncVersion=0
        // This allows Fennec to DELETE tombstones from the database whenever it runs cleanup logic.

        // Whenever we encounter node-reassignment, or otherwise need to re-upload our records, records
        // are reset to a "sync needed and record exists elsewhere" state:
        // - localVersion=2
        // - syncVersion=1

        // syncVersion>0 indicates that a record was synced with the current Firefox Account at some point.
        // This is important for tombstones: we don't want to DELETE them from the database if a record
        // might exist elsewhere in the Sync constellation.
        // It's not possible to sign into another Firefox Account without first explicitly signing out
        // of the current one, and so when user changes accounts, we'll reset versions to (1,0), allowing
        // tombstones to be dropped from the database.

        // Our operational requirement for relationship between versions is:
        // localVersion >= syncVersion && localVersion != 0

        // We want to signify that this operation is very much tied to Sync. We force clients to
        // explicitly call out that they're sync-related, and crash for any non-sync callers (i.e. Fennec proper).
        if (!isCallerSync(uri)) {
            throw new IllegalStateException("Attempted resetting sync versions outside of Sync context");
        }

        // Resetting versions is currently supported only either explicitly for bookmarks, or wild-card
        // style for all record types, where what "all" means is defined by this method.
        final int match = URI_MATCHER.match(uri);
        if (match != BOOKMARKS
                && TextUtils.isEmpty(uri.getQueryParameter(BrowserContract.PARAM_RESET_VERSIONS_FOR_ALL_TYPES))) {
            throw new IllegalStateException("Attempting resetting sync versions for non-versioned record types");
        }

        final ContentValues resetVersionsValues = new ContentValues();

        if (!TextUtils.isEmpty(uri.getQueryParameter(BrowserContract.PARAM_RESET_VERSIONS_TO_SYNCED))) {
            resetVersionsValues.put(BrowserContract.VersionColumns.LOCAL_VERSION, 2);
            resetVersionsValues.put(BrowserContract.VersionColumns.SYNC_VERSION, 1);
        } else {
            resetVersionsValues.put(BrowserContract.VersionColumns.LOCAL_VERSION, 1);
            resetVersionsValues.put(BrowserContract.VersionColumns.SYNC_VERSION, 0);
        }

        // Reset versions for data types which support versioning. Currently that's just bookmarks.
        // See Bug 1383894.
        return db.update(TABLE_BOOKMARKS, resetVersionsValues, null, null);
    }

    private boolean updateBookmarkByGuidAssertingLocalVersion(Uri uri, SQLiteDatabase db, Bundle extras) {
        final int match = URI_MATCHER.match(uri);
        switch (match) {
            case BOOKMARKS:
                break;
            default:
                throw new IllegalStateException("Attempted to update sync versions for a non-versioned repository: " + uri);
        }

        final String table = TABLE_BOOKMARKS;
        final String guid = extras.getString(BrowserContract.SyncColumns.GUID);
        final int expectedLocalVersion = extras.getInt(BrowserContract.VersionColumns.LOCAL_VERSION, -1);

        if (guid == null || expectedLocalVersion == -1) {
            throw new IllegalArgumentException("Missing guid or expectedLocalVersion.");
        }

        final ContentValues values = extras.getParcelable(BrowserContract.METHOD_PARAM_DATA);
        if (values == null) {
            throw new IllegalArgumentException("Missing update values for a record in " + table);
        }

        // We want SQL's BEGIN IMMEDIATE, and to obtain a RESERVED lock and block others
        // from writing.
        // This guarantees that our record won't change between us reading its current version
        // and making changes.
        // We don't care if others are reading the database at the same time.
        // We'll still implicitly acquire an EXCLUSIVE lock right when we'll be executing
        // an UPDATE, but at least that time window will be less than if we acquired it
        // for running SELECT as well.
        // Note that SELECTs overlapping with UPDATES have undefined behaviour as far as
        // data visibility goes across transactions on the same connection, and so this might lead
        // to strange behaviour elsewhere in rare circumstances. Obtaining an EXCLUSIVE lock would
        // prevent this at a cost of concurrency - overlapping queries will get SQL_BUSY; since
        // we'll be running this query quite a bit during a sync that cost might be great, and so we
        // prefer the "optimistic" approach.
        db.beginTransactionNonExclusive();
        try {
            final Cursor c = db.query(table, new String[] {BrowserContract.VersionColumns.LOCAL_VERSION}, BrowserContract.SyncColumns.GUID + " = ?", new String[]{guid}, null, null, null);
            final int localVersionCol = c.getColumnIndexOrThrow(BrowserContract.VersionColumns.LOCAL_VERSION);
            try {
                // Missing record. Let the caller know we failed to update!
                if (!c.moveToFirst()) {
                    return false;
                }

                if (c.isNull(localVersionCol)) {
                    throw new IllegalArgumentException("Missing localVersion for a record in " + table);
                }
                final int localVersion = c.getInt(localVersionCol);
                // Versions don't match, meaning that assertion fails and we can't proceed
                // with an update. Let the caller know what the new localVersion is.
                if (expectedLocalVersion != localVersion) {
                    return false;
                }

                if (c.moveToNext()) {
                    // Somehow we got more than one record, which is problematic when we're
                    // trying to compare versions. Callers are supposed to be checking against this, so just throw.
                    throw new IllegalArgumentException("Got more than 1 record matching provided guid in table " + table);
                }
            } finally {
                c.close();
            }

            // Version assertion passed, we may proceed to update the record.
            final int changed = updateBookmarks(uri, values, Bookmarks.GUID + " = ?", new String[] {guid});
            if (changed != 1) {
                // We expected only one record to be updated, and this is getting stranger still!
                // This indicates that multiple records matched our selection criteria, but we should
                // have checked against this above and thrown already.
                // "This should never happen."
                throw new IllegalStateException("Expected to modify 1, but modified " + changed + " records in " + table);
            }
            db.setTransactionSuccessful();
            return true;
        } finally {
            db.endTransaction();
        }
    }

    private int bulkUpdateSyncVersions(SQLiteDatabase db, Uri uri, Bundle data) {
        if (!isCallerSync(uri)) {
            throw new IllegalStateException("Attempted updating sync versions outside of Sync");
        }

        final int match = URI_MATCHER.match(uri);
        switch (match) {
            case BOOKMARKS:
                break;
            default:
                throw new IllegalStateException("Attempted to update sync versions for a non-versioned repository: " + uri);
        }

        final String table = TABLE_BOOKMARKS;

        // The fact that this is a ConcurrentHashMap is a by-product of how VersionedMiddlewareRepository
        // works. We own both sides of this interface, and so it's not really worth the effort to
        // re-wrap data in, say, a list of ContentValues.
        final ConcurrentHashMap<String, Integer> syncVersionsForGuids = uncheckedCastSerializableToHashMap(
                data.getSerializable(BrowserContract.METHOD_PARAM_DATA)
        );

        final String updateSqlStatement = "UPDATE " + table +
                " SET " + BrowserContract.VersionColumns.SYNC_VERSION +
                " = ?" +
                " WHERE " + BrowserContract.SyncColumns.GUID + " = ?";
        final SQLiteStatement compiledStatement = db.compileStatement(updateSqlStatement);

        beginWrite(db);

        int changed = 0;
        try {
            for (String guid : syncVersionsForGuids.keySet()) {
                final int syncVersion = syncVersionsForGuids.get(guid);

                compiledStatement.clearBindings();
                compiledStatement.bindLong(1, syncVersion); // NB: 1-based index.
                compiledStatement.bindString(2, guid);

                // We expect this to be 1.
                final int didUpdate = compiledStatement.executeUpdateDelete();

                // These strong assertions are here to help figure out root cause of Bug 1392078.
                // This will throw if there are duplicate GUIDs present.
                if (didUpdate > 1) {
                    throw new IllegalStateException("Modified more than a single GUID during syncVersion update");
                }

                // This will throw if the requested GUID is missing from the database.
                if (didUpdate == 0) {
                    throw new IllegalStateException("Expected to modify syncVersion for a guid, but did not");
                }

                changed += didUpdate;
            }

            markWriteSuccessful(db);

        } finally {
            endWrite(db);
        }

        return changed;
    }

    private void bulkReplaceRemoteDevices(final Uri uri, @NonNull Bundle dataBundle) {
        final ContentValues[] values = (ContentValues[]) dataBundle.getParcelableArray(BrowserContract.METHOD_PARAM_DATA);

        if (values == null) {
            throw new IllegalArgumentException("Received null recordBundle while bulk inserting remote clients.");
        }

        final SQLiteDatabase db = getWritableDatabase(uri);

        // Wrap everything in a transaction.
        beginBatch(db);

        try {
            // First purge our list of remote devices.
            // We pass "1" to get a count of the affected rows (see SQLiteDatabase#delete)
            int count = deleteInTransaction(uri, "1", null);
            Log.i(LOGTAG, "Deleted " + count + " remote devices.");

            // Then insert the new ones.
            for (int i = 0; i < values.length; i++) {
                try {
                    insertFxADevice(uri, values[i]);
                } catch (Exception e) {
                    Log.e(LOGTAG, "Could not insert device with ID " + values[i].getAsString(RemoteDevices.GUID) + ": " + e);
                }
            }
            markBatchSuccessful(db);
        } finally {
            endBatch(db);
        }

        getContext().getContentResolver().notifyChange(uri, null, false);
    }

    private void bulkInsertHistoryWithVisits(final SQLiteDatabase db, @NonNull Bundle dataBundle) {
        // NB: dataBundle structure:
        // Key METHOD_PARAM_DATA=[Bundle,...]
        // Each Bundle has keys METHOD_PARAM_OBJECT=ContentValues{HistoryRecord}, VISITS=ContentValues[]{visits}
        final Bundle[] recordBundles = (Bundle[]) dataBundle.getSerializable(BrowserContract.METHOD_PARAM_DATA);

        if (recordBundles == null) {
            throw new IllegalArgumentException("Received null recordBundle while bulk inserting history.");
        }

        if (recordBundles.length == 0) {
            return;
        }

        final ContentValues[][] visitsValueSet = new ContentValues[recordBundles.length][];
        final ContentValues[] historyValueSet = new ContentValues[recordBundles.length];
        for (int i = 0; i < recordBundles.length; i++) {
            historyValueSet[i] = recordBundles[i].getParcelable(BrowserContract.METHOD_PARAM_OBJECT);
            visitsValueSet[i] = (ContentValues[]) recordBundles[i].getSerializable(History.VISITS);
        }

        // Wrap the whole operation in a transaction.
        beginBatch(db);

        final int historyInserted;
        try {
            // First, insert history records.
            historyInserted = bulkInsertHistory(db, historyValueSet);
            if (historyInserted != recordBundles.length) {
                Log.w(LOGTAG, "Expected to insert " + recordBundles.length + " history records, " +
                        "but actually inserted " + historyInserted);
            }

            // Second, insert visit records.
            bulkInsertVisits(db, visitsValueSet);

            // Finally, commit all of the insertions we just made.
            markBatchSuccessful(db);

        // We're done with our database operations.
        } finally {
            endBatch(db);
        }

        // Notify listeners that we've just inserted new history records.
        if (historyInserted > 0) {
            getContext().getContentResolver().notifyChange(
                    BrowserContractHelpers.HISTORY_CONTENT_URI, null,
                    // Do not sync these changes.
                    false
            );
        }
    }

    private int bulkInsertHistory(final SQLiteDatabase db, ContentValues[] values) {
        int inserted = 0;
        // Set 'modified' and 'created' timestamps to current wall time.
        // 'modified' specifically is used by Sync for change tracking, and so we must ensure it's
        // set to our own clock (as opposed to record's modified timestamp as record by the server).
        final long now = System.currentTimeMillis();
        final String fullInsertSqlStatement = "INSERT INTO " + History.TABLE_NAME + " (" +
                History.GUID + "," +
                History.TITLE + "," +
                History.URL + "," +
                History.DATE_LAST_VISITED + "," +
                History.REMOTE_DATE_LAST_VISITED + "," +
                History.VISITS + "," +
                History.REMOTE_VISITS + "," +
                History.DATE_MODIFIED + "," +
                History.DATE_CREATED + ") VALUES (?, ?, ?, ?, ?, ?, ?, " + now + "," + now + ")";
        final String shortInsertSqlStatement = "INSERT INTO " + History.TABLE_NAME + " (" +
                History.GUID + "," +
                History.TITLE + "," +
                History.URL + "," +
                History.DATE_MODIFIED + "," +
                History.DATE_CREATED + ") VALUES (?, ?, ?, " + now + "," + now + ")";
        final SQLiteStatement compiledFullStatement = db.compileStatement(fullInsertSqlStatement);
        final SQLiteStatement compiledShortStatement = db.compileStatement(shortInsertSqlStatement);
        SQLiteStatement statementToExec;

        beginWrite(db);
        try {
            for (ContentValues cv : values) {
                final String guid = cv.getAsString(History.GUID);
                final String title = cv.getAsString(History.TITLE);
                final String url = cv.getAsString(History.URL);
                final Long dateLastVisited = cv.getAsLong(History.DATE_LAST_VISITED);
                final Long remoteDateLastVisited = cv.getAsLong(History.REMOTE_DATE_LAST_VISITED);
                final Integer visits = cv.getAsInteger(History.VISITS);

                // If dateLastVisited is null, so will be remoteDateLastVisited and visits.
                // We will use the short compiled statement in this case.
                // See implementation in HistoryDataAccessor#getContentValues.
                if (dateLastVisited == null) {
                    statementToExec = compiledShortStatement;
                } else {
                    statementToExec = compiledFullStatement;
                }

                statementToExec.clearBindings();
                statementToExec.bindString(1, guid);
                // Title is allowed to be null.
                if (title != null) {
                    statementToExec.bindString(2, title);
                } else {
                    statementToExec.bindNull(2);
                }
                statementToExec.bindString(3, url);
                if (dateLastVisited != null) {
                    statementToExec.bindLong(4, dateLastVisited);
                    statementToExec.bindLong(5, remoteDateLastVisited);

                    // NB:
                    // Both of these count values might be slightly off unless we recalculate them
                    // from data in the visits table at some point.
                    // See note about visit insertion failures below in the bulkInsertVisits method.

                    // Visit count
                    statementToExec.bindLong(6, visits);
                    // Remote visit count.
                    statementToExec.bindLong(7, visits);
                }

                try {
                    if (statementToExec.executeInsert() != -1) {
                        inserted += 1;
                    }

                // NB: Constraint violation might occur if we're trying to insert a duplicate GUID.
                // This should not happen but it does in practice, possibly due to reconciliation bugs.
                // For now we catch and log the error without failing the whole bulk insert.
                } catch (SQLiteConstraintException e) {
                    Log.w(LOGTAG, "Unexpected constraint violation while inserting history with GUID " + guid, e);
                }
            }
            markWriteSuccessful(db);
        } finally {
            endWrite(db);
        }

        if (inserted != values.length) {
            Log.w(LOGTAG, "Failed to insert some of the history. " +
                    "Expected: " + values.length + ", actual: " + inserted);
        }

        return inserted;
    }

    private int bulkInsertVisits(SQLiteDatabase db, ContentValues[][] valueSets) {
        final String insertSqlStatement = "INSERT INTO " + Visits.TABLE_NAME + " (" +
                Visits.DATE_VISITED + "," +
                Visits.VISIT_TYPE + "," +
                Visits.HISTORY_GUID + "," +
                Visits.IS_LOCAL + ") VALUES (?, ?, ?, ?)";
        final SQLiteStatement compiledInsertStatement = db.compileStatement(insertSqlStatement);

        int totalInserted = 0;
        beginWrite(db);
        try {
            for (ContentValues[] valueSet : valueSets) {
                int inserted = 0;
                for (ContentValues values : valueSet) {
                    final long date = values.getAsLong(Visits.DATE_VISITED);
                    final long visitType = values.getAsLong(Visits.VISIT_TYPE);
                    final String guid = values.getAsString(Visits.HISTORY_GUID);
                    final Integer isLocal = values.getAsInteger(Visits.IS_LOCAL);

                    // Bind parameters use a 1-based index.
                    compiledInsertStatement.clearBindings();
                    compiledInsertStatement.bindLong(1, date);
                    compiledInsertStatement.bindLong(2, visitType);
                    compiledInsertStatement.bindString(3, guid);
                    compiledInsertStatement.bindLong(4, isLocal);

                    try {
                        if (compiledInsertStatement.executeInsert() != -1) {
                            inserted++;
                        }

                    // NB:
                    // Constraint exception will be thrown if we try to insert a visit violating
                    // unique(guid, date) constraint. We don't expect to do that, but our incoming
                    // data might not be clean - either due to duplicate entries in the sync data,
                    // or, less likely, due to record reconciliation bugs at the RepositorySession
                    // level.
                    } catch (SQLiteConstraintException e) {
                        // Don't log this, it'll just cause memory churn.
                    }
                }
                if (inserted != valueSet.length) {
                    Log.w(LOGTAG, "Failed to insert some of the visits. " +
                            "Expected: " + valueSet.length + ", actual: " + inserted);
                }
                totalInserted += inserted;
            }
            markWriteSuccessful(db);
        } finally {
            endWrite(db);
        }

        return totalInserted;
    }

    @SuppressWarnings("unchecked")
    private ConcurrentHashMap<String, Integer> uncheckedCastSerializableToHashMap(Serializable serializable) {
        return (ConcurrentHashMap<String, Integer>) serializable;
    }

    @Override
    public ContentProviderResult[] applyBatch (ArrayList<ContentProviderOperation> operations)
        throws OperationApplicationException {
        final int numOperations = operations.size();
        final ContentProviderResult[] results = new ContentProviderResult[numOperations];

        if (numOperations < 1) {
            debug("applyBatch: no operations; returning immediately.");
            // The original Android implementation returns a zero-length
            // array in this case. We do the same.
            return results;
        }

        boolean failures = false;

        // We only have 1 database for all Uris that we can get.
        SQLiteDatabase db = getWritableDatabase(operations.get(0).getUri());

        // Note that the apply() call may cause us to generate
        // additional transactions for the individual operations.
        // But Android's wrapper for SQLite supports nested transactions,
        // so this will do the right thing.
        //
        // Note further that in some circumstances this can result in
        // exceptions: if this transaction is first involved in reading,
        // and then (naturally) tries to perform writes, SQLITE_BUSY can
        // be raised. See Bug 947939 and friends.
        beginBatch(db);

        for (int i = 0; i < numOperations; i++) {
            try {
                final ContentProviderOperation operation = operations.get(i);
                results[i] = operation.apply(this, results, i);
            } catch (SQLException e) {
                Log.w(LOGTAG, "SQLite Exception during applyBatch.", e);
                // The Android API makes it implementation-defined whether
                // the failure of a single operation makes all others abort
                // or not. For our use cases, best-effort operation makes
                // more sense. Rolling back and forcing the caller to retry
                // after it figures out what went wrong isn't very convenient
                // anyway.
                // Signal failed operation back, so the caller knows what
                // went through and what didn't.
                results[i] = new ContentProviderResult(0);
                failures = true;
                // http://www.sqlite.org/lang_conflict.html
                // Note that we need a new transaction, subsequent operations
                // on this one will fail (we're in ABORT by default, which
                // isn't IGNORE). We still need to set it as successful to let
                // everything before the failed op go through.
                // We can't set conflict resolution on API level < 8, and even
                // above 8 it requires splitting the call per operation
                // (insert/update/delete).
                db.setTransactionSuccessful();
                db.endTransaction();
                db.beginTransaction();
            } catch (OperationApplicationException e) {
                // Repeat of above.
                results[i] = new ContentProviderResult(0);
                failures = true;
                db.setTransactionSuccessful();
                db.endTransaction();
                db.beginTransaction();
            }
        }

        trace("Flushing DB applyBatch...");
        markBatchSuccessful(db);
        endBatch(db);

        if (failures) {
            throw new OperationApplicationException();
        }

        return results;
    }

    private static Table findTableFor(int id) {
        for (Table table : sTables) {
            for (Table.ContentProviderInfo type : table.getContentProviderInfo()) {
                if (type.id == id) {
                    return table;
                }
            }
        }
        return null;
    }

    private static void addTablesToMatcher(Table[] tables, final UriMatcher matcher) {
    }

    private static String getContentItemType(final int match) {
        for (Table table : sTables) {
            for (Table.ContentProviderInfo type : table.getContentProviderInfo()) {
                if (type.id == match) {
                    return "vnd.android.cursor.item/" + type.name;
                }
            }
        }

        return null;
    }

    /**
     * A note on record version tracking, applicable to repositories that support it.
     * (bookmarks as of Bug 1364644; same logic must apply to any record types in the future)
     * - localVersion is always incremented by 1. No other change is allowed.
     * - any modifications from Fennec increment localVersion.
     * - modifications from Sync do not increment localVersion, unless an explicit flag is passed in.
     * - syncVersion is updated "in bulk", and set to some value that's expected to be <= localVersion.
     * - localVersion and syncVersion may jump backwards to (1,0) - always on reset, which happens
     *   on sync disconnect, or to (2,1) - always on "reset to synced", when local repositories are
     *   reset after node-reassignment, syncID change, etc.
     */
    private static int updateAndIncrementLocalVersion(SQLiteDatabase db, Uri uri, String table, ContentValues values, String selection, String[] selectionArgs) {
        // Strongly assert that this operation is happening outside of Sync or was explicitly asked for.
        // We prefer to crash rather than risk introducing sync loops.
        if (isCallerSync(uri) && !shouldIncrementLocalVersionFromSync(uri)) {
            throw new IllegalStateException("Attempted to increment change counter from within a Sync");
        }

        // Strongly assert that our caller isn't trying to set a localVersion themselves!
        if (values.containsKey(BrowserContract.VersionColumns.LOCAL_VERSION)) {
            throw new IllegalStateException("Attempted manually setting local version");
        }

        final ContentValues incrementLocalVersion = new ContentValues();
        incrementLocalVersion.put(BrowserContract.VersionColumns.LOCAL_VERSION, BrowserContract.VersionColumns.LOCAL_VERSION + " + 1");

        final ContentValues[] valuesAndVisits = { values,  incrementLocalVersion };
        UpdateOperation[] ops = new UpdateOperation[]{ UpdateOperation.ASSIGN, UpdateOperation.EXPRESSION };

        return DBUtils.updateArrays(db, table, valuesAndVisits, ops, selection, selectionArgs);
    }

}
