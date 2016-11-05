/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.ActivityStreamBlocklist;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserContract.FaviconColumns;
import org.mozilla.gecko.db.BrowserContract.Favicons;
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
import android.database.MergeCursor;
import android.database.SQLException;
import android.database.sqlite.SQLiteCursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteQueryBuilder;
import android.net.Uri;
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

    static final int HIGHLIGHTS = 1300;

    static final int ACTIVITY_STREAM_BLOCKLIST = 1400;

    static final int PAGE_METADATA = 1500;

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
    static final Table[] sTables;

    static {
        sTables = new Table[] {
            // See awful shortcut assumption hack in getURLMetadataTable.
            new URLMetadataTable()
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

        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "highlights", HIGHLIGHTS);

        URI_MATCHER.addURI(BrowserContract.AUTHORITY, ActivityStreamBlocklist.TABLE_NAME, ACTIVITY_STREAM_BLOCKLIST);
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
    private URLMetadataTable getURLMetadataTable() {
        return (URLMetadataTable) sTables[0];
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
            sql = "DELETE FROM " + TABLE_HISTORY + " " +
                  "WHERE MAX(" + History.DATE_LAST_VISITED + ", " + History.DATE_MODIFIED + ") < " + keepAfter + " " +
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

                selection = DBUtils.concatenateWhere(selection, TABLE_BOOKMARKS + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case BOOKMARKS: {
                trace("Deleting bookmarks: " + uri);
                deleted = deleteBookmarks(uri, selection, selectionArgs);
                deleteUnusedImages(uri);
                break;
            }

            case HISTORY_ID:
                trace("Delete on HISTORY_ID: " + uri);

                selection = DBUtils.concatenateWhere(selection, TABLE_HISTORY + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
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

                selection = DBUtils.concatenateWhere(selection, TABLE_FAVICONS + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
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

                selection = DBUtils.concatenateWhere(selection, TABLE_THUMBNAILS + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
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
    public int updateInTransaction(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        trace("Calling update in transaction on URI: " + uri);

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
                updated = updateBookmarkParents(db, values, selection, selectionArgs);
                break;
            }

            case BOOKMARKS_ID:
                debug("Update on BOOKMARKS_ID: " + uri);

                selection = DBUtils.concatenateWhere(selection, TABLE_BOOKMARKS + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
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

                selection = DBUtils.concatenateWhere(selection, TABLE_HISTORY + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
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

    /**
     * Get topsites by themselves, without the inclusion of pinned sites. Suggested sites
     * will be appended (if necessary) to the end of the list in order to provide up to PARAM_LIMIT items.
     */
    private Cursor getPlainTopSites(final Uri uri) {
        final SQLiteDatabase db = getReadableDatabase(uri);

        final String limitParam = uri.getQueryParameter(BrowserContract.PARAM_LIMIT);
        final int limit;
        if (limitParam != null) {
            limit = Integer.parseInt(limitParam);
        } else {
            limit = 12;
        }

        // Filter out: unvisited pages (history_id == -1) pinned (and other special) sites, deleted sites,
        // and about: pages.
        final String ignoreForTopSitesWhereClause =
                "(" + Combined.HISTORY_ID + " IS NOT -1)" +
                " AND " +
                Combined.URL + " NOT IN (SELECT " +
                Bookmarks.URL + " FROM " + TABLE_BOOKMARKS + " WHERE " +
                DBUtils.qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " < " + Bookmarks.FIXED_ROOT_ID + " AND " +
                DBUtils.qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED) + " == 0)" +
                " AND " +
                "(" + Combined.URL + " NOT LIKE ?)";

        final String[] ignoreForTopSitesArgs = new String[] {
                AboutPages.URL_FILTER
        };

        final Cursor c = db.rawQuery("SELECT " +
                   Bookmarks._ID + ", " +
                   Combined.BOOKMARK_ID + ", " +
                   Combined.HISTORY_ID + ", " +
                   Bookmarks.URL + ", " +
                   Bookmarks.TITLE + ", " +
                   Combined.HISTORY_ID + ", " +
                   TopSites.TYPE_TOP + " AS " + TopSites.TYPE +
                   " FROM " + Combined.VIEW_NAME +
                   " WHERE " + ignoreForTopSitesWhereClause +
                   " ORDER BY " + BrowserContract.getCombinedFrecencySortOrder(true, false) +
                   " LIMIT " + limit,
                ignoreForTopSitesArgs);

        c.setNotificationUri(getContext().getContentResolver(),
                BrowserContract.AUTHORITY_URI);

        if (c.getCount() == limit) {
            return c;
        }

        // If we don't have enough data: get suggested sites too
        final SuggestedSites suggestedSites = BrowserDB.from(GeckoProfile.get(
                getContext(), uri.getQueryParameter(BrowserContract.PARAM_PROFILE))).getSuggestedSites();

        final Cursor suggestedSitesCursor = suggestedSites.get(limit - c.getCount());

        return new MergeCursor(new Cursor[]{
                c,
                suggestedSitesCursor
        });
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

        final String pinnedSitesFromClause = "FROM " + TABLE_BOOKMARKS + " WHERE " +
                                             Bookmarks.PARENT + " == " + Bookmarks.FIXED_PINNED_LIST_ID +
                                             " AND " + Bookmarks.IS_DELETED + " IS NOT 1";

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
        final String ignoreForTopSitesWhereClause =
                "(" + Combined.HISTORY_ID + " IS NOT -1)" +
                " AND " +
                Combined.URL + " NOT IN (SELECT " +
                Bookmarks.URL + " FROM bookmarks WHERE " +
                DBUtils.qualifyColumn("bookmarks", Bookmarks.PARENT) + " < " + Bookmarks.FIXED_ROOT_ID + " AND " +
                DBUtils.qualifyColumn("bookmarks", Bookmarks.IS_DELETED) + " == 0)" +
                " AND " +
                "(" + Combined.URL + " NOT LIKE ?)";

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

            suggestedSiteArgs = DBUtils.appendSelectionArgs(suggestedSiteArgs,
                                                            new String[] {
                                                                    suggestedSitesCursor.getString(idColumnIndex),
                                                                    suggestedSitesCursor.getString(urlColumnIndex),
                                                                    suggestedSitesCursor.getString(titleColumnIndex)
                                                            });
        }
        suggestedSitesCursor.close();

        boolean hasPreparedBlankTiles = false;

        // We can somewhat reduce the number of blanks we produce by eliminating suggested sites.
        // We do the actual limit calculation in SQL (since we need to take into account the number
        // of pinned sites too), but this might avoid producing 5 or so additional blank tiles
        // that would then need to be filtered out.
        final int maxBlanksNeeded = suggestedGridLimit - suggestedSitesCursor.getCount();

        final StringBuilder blanksBuilder = new StringBuilder();
        for (int i = 0; i < maxBlanksNeeded; i++) {
            if (hasPreparedBlankTiles) {
                blanksBuilder.append(" UNION ALL");
            } else {
                hasPreparedBlankTiles = true;
            }

            blanksBuilder.append(" SELECT" +
                                 " -1 AS " + Bookmarks._ID + "," +
                                 " '' AS " + Bookmarks.URL + "," +
                                 " '' AS " + Bookmarks.TITLE);
        }



        // To restrict suggested sites to the grid, we simply subtract the number of topsites (which have already had
        // the pinned sites filtered out), and the number of pinned sites.
        // SQLite completely ignores negative limits, hence we need to manually limit to 0 in this case.
        final String suggestedLimitClause = " LIMIT MAX(0, (" + suggestedGridLimit + " - (SELECT COUNT(*) FROM " + TABLE_TOPSITES + ") - (SELECT COUNT(*) " + pinnedSitesFromClause + "))) ";

        // Pinned site positions are zero indexed, but we need to get the maximum 1-indexed position.
        // Hence to correctly calculate the largest pinned position (which should be 0 if there are
        // no sites, or 1-6 if we have at least one pinned site), we coalesce the DB position (0-5)
        // with -1 to represent no-sites, which allows us to directly add 1 to obtain the expected value
        // regardless of whether a position was actually retrieved.
        final String blanksLimitClause = " LIMIT MAX(0, " +
                            "COALESCE((SELECT " + Bookmarks.POSITION + " " + pinnedSitesFromClause + "), -1) + 1" +
                            " - (SELECT COUNT(*) " + pinnedSitesFromClause + ")" +
                            " - (SELECT COUNT(*) FROM " + TABLE_TOPSITES + ")" +
                            ")";

        db.beginTransaction();
        try {
            db.execSQL("DROP TABLE IF EXISTS " + TABLE_TOPSITES);

            db.execSQL("CREATE TEMP TABLE " + TABLE_TOPSITES + " AS" +
                       " SELECT " +
                       Bookmarks._ID + ", " +
                       Combined.BOOKMARK_ID + ", " +
                       Combined.HISTORY_ID + ", " +
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
                           Bookmarks._ID + " AS " + Combined.BOOKMARK_ID + ", " +
                           " -1 AS " + Combined.HISTORY_ID + ", " +
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

            if (hasPreparedBlankTiles) {
                db.execSQL("INSERT INTO " + TABLE_TOPSITES +
                           // We need to LIMIT _after_ selecting the relevant suggested sites, which requires us to
                           // use an additional internal subquery, since we cannot LIMIT a subquery that is part of UNION ALL.
                           // Hence the weird SELECT * FROM (SELECT ...relevant suggested sites... LIMIT ?)
                           " SELECT * FROM (SELECT " +
                           Bookmarks._ID + ", " +
                           Bookmarks._ID + " AS " + Combined.BOOKMARK_ID + ", " +
                           " -1 AS " + Combined.HISTORY_ID + ", " +
                           Bookmarks.URL + ", " +
                           Bookmarks.TITLE + ", " +
                           "NULL AS " + Combined.HISTORY_ID + ", " +
                           TopSites.TYPE_BLANK + " as " + TopSites.TYPE +
                           " FROM ( " + blanksBuilder.toString() + " )" +
                           blanksLimitClause + " )");
            }

            // If we retrieve more topsites than we have free positions for in the freeIdSubquery,
            // we will have topsites that don't receive a position when joining TABLE_TOPSITES
            // with freeIdSubquery. Hence we need to coalesce the position with a generated position.
            // We know that the difference in positions will be at most suggestedGridLimit, hence we
            // can add that to the rowid to generate a safe position.
            // I.e. if we have 6 pinned sites then positions 0..5 are filled, the JOIN results in
            // the first N rows having positions 6..(N+6), so row N+1 should receive a position that is at
            // least N+1+6, which is equal to rowid + 6.
            final SQLiteCursor c = (SQLiteCursor) db.rawQuery(
                        "SELECT " +
                        Bookmarks._ID + ", " +
                        TopSites.BOOKMARK_ID + ", " +
                        TopSites.HISTORY_ID + ", " +
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
                        Bookmarks.URL + ", " +
                        Bookmarks.TITLE + ", " +
                        Bookmarks.POSITION + ", " +
                        "NULL AS " + Combined.HISTORY_ID + ", " +
                        TopSites.TYPE_PINNED + " as " + TopSites.TYPE +
                        " " + pinnedSitesFromClause +

                        " ORDER BY " + Bookmarks.POSITION,

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

    /**
     * Obtain a set of links for highlights (from bookmarks and history).
     *
     * Based on the query for Activity^ Stream (desktop):
     * https://github.com/mozilla/activity-stream/blob/9eb9f451b553bb62ae9b8d6b41a8ef94a2e020ea/addon/PlacesProvider.js#L578
     */
    public Cursor getHighlights(final SQLiteDatabase db, String limit) {
        final int totalLimit = limit == null ? 20 : Integer.parseInt(limit);

        final long threeDaysAgo = System.currentTimeMillis() - (1000 * 60 * 60 * 24 * 3);
        final long bookmarkLimit = 1;

        // Select recent bookmarks that have not been visited much
        final String bookmarksQuery = "SELECT * FROM (SELECT " +
                "-1 AS " + Combined.HISTORY_ID + ", " +
                DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks._ID) + " AS " + Combined.BOOKMARK_ID + ", " +
                DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.URL) + ", " +
                DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.TITLE) + ", " +
                DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.DATE_CREATED) + " AS " + Highlights.DATE + " " +
                "FROM " + Bookmarks.TABLE_NAME + " " +
                "LEFT JOIN " + History.TABLE_NAME + " ON " +
                    DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.URL) + " = " +
                    DBUtils.qualifyColumn(History.TABLE_NAME, History.URL) + " " +
                "WHERE " + DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.DATE_CREATED) + " > " + threeDaysAgo + " " +
                "AND (" + DBUtils.qualifyColumn(History.TABLE_NAME, History.VISITS) + " <= 3 " +
                  "OR " + DBUtils.qualifyColumn(History.TABLE_NAME, History.VISITS) + " IS NULL) " +
                "AND " + DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.IS_DELETED)  + " = 0 " +
                "AND " + DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.TYPE) + " = " + Bookmarks.TYPE_BOOKMARK + " " +
                "AND " + DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.URL) + " NOT IN (SELECT " + ActivityStreamBlocklist.URL + " FROM " + ActivityStreamBlocklist.TABLE_NAME + " )" +
                "ORDER BY " + DBUtils.qualifyColumn(Bookmarks.TABLE_NAME, Bookmarks.DATE_CREATED) + " DESC " +
                "LIMIT " + bookmarkLimit + ")";

        final long last30Minutes = System.currentTimeMillis() - (1000 * 60 * 30);
        final long historyLimit = totalLimit - bookmarkLimit;

        // Select recent history that has not been visited much.
        final String historyQuery = "SELECT * FROM (SELECT " +
                History._ID + " AS " + Combined.HISTORY_ID + ", " +
                "-1 AS " + Combined.BOOKMARK_ID + ", " +
                History.URL + ", " +
                History.TITLE + ", " +
                History.DATE_LAST_VISITED + " AS " + Highlights.DATE + " " +
                "FROM " + History.TABLE_NAME + " " +
                "WHERE " + History.DATE_LAST_VISITED + " < " + last30Minutes + " " +
                "AND " + History.VISITS + " <= 3 " +
                "AND " + History.TITLE + " NOT NULL AND " + History.TITLE + " != '' " +
                "AND " + History.IS_DELETED + " = 0 " +
                "AND " + History.URL + " NOT IN (SELECT " + ActivityStreamBlocklist.URL + " FROM " + ActivityStreamBlocklist.TABLE_NAME + " )" +
                // TODO: Implement domain black list (bug 1298786)
                // TODO: Group by host (bug 1298785)
                "ORDER BY " + History.DATE_LAST_VISITED + " DESC " +
                "LIMIT " + historyLimit + ")";

        final String query = "SELECT DISTINCT * " +
                "FROM (" + bookmarksQuery + " " +
                "UNION ALL " + historyQuery + ") " +
                "GROUP BY " + Combined.URL + ";";

        final Cursor cursor = db.rawQuery(query, null);

        cursor.setNotificationUri(getContext().getContentResolver(),
                BrowserContract.AUTHORITY_URI);

        return cursor;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder) {
        final int match = URI_MATCHER.match(uri);

        // Handle only queries requiring a writable DB connection here: most queries need only a readable
        // connection, hence we can get a readable DB once, and then handle most queries within a switch.
        // TopSites requires a writable connection (because of the temporary tables it uses), hence
        // we handle that separately, i.e. before retrieving a readable connection.
        if (match == TOPSITES) {
            if (uri.getBooleanQueryParameter(BrowserContract.PARAM_TOPSITES_DISABLE_PINNED, false)) {
                return getPlainTopSites(uri);
            } else {
                return getTopSites(uri);
            }
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
                    selection = DBUtils.concatenateWhere(selection, Bookmarks._ID + " = ?");
                    selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                            new String[] { Long.toString(ContentUris.parseId(uri)) });
                } else if (match == BOOKMARKS_FOLDER_ID) {
                    selection = DBUtils.concatenateWhere(selection, Bookmarks.PARENT + " = ?");
                    selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                            new String[] { Long.toString(ContentUris.parseId(uri)) });
                }

                if (!shouldShowDeleted(uri))
                    selection = DBUtils.concatenateWhere(Bookmarks.IS_DELETED + " = 0", selection);

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
                selection = DBUtils.concatenateWhere(selection, History._ID + " = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case HISTORY: {
                debug("Query is on history: " + uri);

                if (!shouldShowDeleted(uri))
                    selection = DBUtils.concatenateWhere(History.IS_DELETED + " = 0", selection);

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
                selection = DBUtils.concatenateWhere(selection, Favicons._ID + " = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case FAVICONS: {
                debug("Query is on favicons: " + uri);

                qb.setProjectionMap(FAVICONS_PROJECTION_MAP);
                qb.setTables(TABLE_FAVICONS);

                break;
            }

            case THUMBNAIL_ID:
                selection = DBUtils.concatenateWhere(selection, Thumbnails._ID + " = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
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

            case HIGHLIGHTS: {
                debug("Highlights query: " + uri);

                return getHighlights(db, limit);
            }

            case PAGE_METADATA: {
                debug("PageMetadata query: " + uri);

                qb.setProjectionMap(PAGE_METADATA_PROJECTION_MAP);
                qb.setTables(TABLE_PAGE_METADATA);
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
    private int updateBookmarkParents(SQLiteDatabase db, ContentValues values, String selection, String[] selectionArgs) {
        trace("Updating bookmark parents of " + selection + " (" + selectionArgs[0] + ")");
        String where = Bookmarks._ID + " IN (" +
                       " SELECT DISTINCT " + Bookmarks.PARENT +
                       " FROM " + TABLE_BOOKMARKS +
                       " WHERE " + selection + " )";
        return db.update(TABLE_BOOKMARKS, values, where, selectionArgs);
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

        debug("Inserting bookmark in database with URL: " + url);
        final SQLiteDatabase db = getWritableDatabase(uri);
        beginWrite(db);
        return db.insertOrThrow(TABLE_BOOKMARKS, Bookmarks.TITLE, values);
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
        return db.update(TABLE_BOOKMARKS, values, inClause, null);
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
        // For example, if there were 1200 GUIDs, this will perform 2 delete statements.
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

    private int deleteVisits(Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting visits for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);

        beginWrite(db);
        return db.delete(TABLE_VISITS, selection, selectionArgs);
    }

    private int deleteBookmarks(Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting bookmarks for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);

        if (isCallerSync(uri)) {
            beginWrite(db);
            return db.delete(TABLE_BOOKMARKS, selection, selectionArgs);
        }

        debug("Marking bookmarks as deleted for URI: " + uri);

        ContentValues values = new ContentValues();
        values.put(Bookmarks.IS_DELETED, 1);
        values.put(Bookmarks.POSITION, 0);
        values.putNull(Bookmarks.PARENT);
        values.putNull(Bookmarks.URL);
        values.putNull(Bookmarks.TITLE);
        values.putNull(Bookmarks.DESCRIPTION);
        values.putNull(Bookmarks.KEYWORD);
        values.putNull(Bookmarks.TAGS);
        values.putNull(Bookmarks.FAVICON_ID);

        // Doing this UPDATE (or the DELETE above) first ensures that the
        // first operation within this transaction is a write.
        // The cleanup call below will do a SELECT first, and thus would
        // require the transaction to be upgraded from a reader to a writer.
        final int updated = updateBookmarks(uri, values, selection, selectionArgs);
        try {
            cleanUpSomeDeletedRecords(uri, TABLE_BOOKMARKS);
        } catch (Exception e) {
            // We don't care.
            Log.e(LOGTAG, "Unable to clean up deleted bookmark records: ", e);
        }
        return updated;
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
               getURLMetadataTable().deleteUnused(getWritableDatabase(uri));
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
}
