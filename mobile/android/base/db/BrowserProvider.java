/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserContract.FaviconColumns;
import org.mozilla.gecko.db.BrowserContract.Favicons;
import org.mozilla.gecko.db.BrowserContract.History;
import org.mozilla.gecko.db.BrowserContract.Schema;
import org.mozilla.gecko.db.BrowserContract.Thumbnails;
import org.mozilla.gecko.sync.Utils;

import android.app.SearchManager;
import android.content.ContentProviderOperation;
import android.content.ContentProviderResult;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.OperationApplicationException;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.database.MatrixCursor;
import android.database.SQLException;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteQueryBuilder;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

public class BrowserProvider extends SharedBrowserDatabaseProvider {
    private static final String LOGTAG = "GeckoBrowserProvider";

    // How many records to reposition in a single query.
    // This should be less than the SQLite maximum number of query variables
    // (currently 999) divided by the number of variables used per positioning
    // query (currently 3).
    static final int MAX_POSITION_UPDATES_PER_QUERY = 100;

    // Minimum number of records to keep when expiring history.
    static final int DEFAULT_EXPIRY_RETAIN_COUNT = 2000;
    static final int AGGRESSIVE_EXPIRY_RETAIN_COUNT = 500;

    // Minimum duration to keep when expiring.
    static final long DEFAULT_EXPIRY_PRESERVE_WINDOW = 1000L * 60L * 60L * 24L * 28L;     // Four weeks.
    // Minimum number of thumbnails to keep around.
    static final int DEFAULT_EXPIRY_THUMBNAIL_COUNT = 15;

    static final String TABLE_BOOKMARKS = Bookmarks.TABLE_NAME;
    static final String TABLE_HISTORY = History.TABLE_NAME;
    static final String TABLE_FAVICONS = Favicons.TABLE_NAME;
    static final String TABLE_THUMBNAILS = Thumbnails.TABLE_NAME;

    static final String VIEW_COMBINED = Combined.VIEW_NAME;
    static final String VIEW_BOOKMARKS_WITH_FAVICONS = Bookmarks.VIEW_WITH_FAVICONS;
    static final String VIEW_HISTORY_WITH_FAVICONS = History.VIEW_WITH_FAVICONS;
    static final String VIEW_COMBINED_WITH_FAVICONS = Combined.VIEW_WITH_FAVICONS;

    static final String VIEW_FLAGS = "flags";

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

    // Search Suggest matches
    static final int SEARCH_SUGGEST = 700;

    // Thumbnail matches
    static final int THUMBNAILS = 800;
    static final int THUMBNAIL_ID = 801;

    static final int FLAGS = 900;

    static final String DEFAULT_BOOKMARKS_SORT_ORDER = Bookmarks.TYPE
            + " ASC, " + Bookmarks.POSITION + " ASC, " + Bookmarks._ID
            + " ASC";

    static final String DEFAULT_HISTORY_SORT_ORDER = History.DATE_LAST_VISITED + " DESC";

    static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

    static final Map<String, String> BOOKMARKS_PROJECTION_MAP;
    static final Map<String, String> HISTORY_PROJECTION_MAP;
    static final Map<String, String> COMBINED_PROJECTION_MAP;
    static final Map<String, String> SCHEMA_PROJECTION_MAP;
    static final Map<String, String> SEARCH_SUGGEST_PROJECTION_MAP;
    static final Map<String, String> FAVICONS_PROJECTION_MAP;
    static final Map<String, String> THUMBNAILS_PROJECTION_MAP;

    static {
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
        map.put(History.DATE_LAST_VISITED, History.DATE_LAST_VISITED);
        map.put(History.DATE_CREATED, History.DATE_CREATED);
        map.put(History.DATE_MODIFIED, History.DATE_MODIFIED);
        map.put(History.GUID, History.GUID);
        map.put(History.IS_DELETED, History.IS_DELETED);
        HISTORY_PROJECTION_MAP = Collections.unmodifiableMap(map);

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
        COMBINED_PROJECTION_MAP = Collections.unmodifiableMap(map);

        // Schema
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "schema", SCHEMA);

        map = new HashMap<String, String>();
        map.put(Schema.VERSION, Schema.VERSION);
        SCHEMA_PROJECTION_MAP = Collections.unmodifiableMap(map);


        // Control
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "control", CONTROL);

        // Search Suggest
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, SearchManager.SUGGEST_URI_PATH_QUERY + "/*", SEARCH_SUGGEST);

        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "flags", FLAGS);

        map = new HashMap<String, String>();
        map.put(SearchManager.SUGGEST_COLUMN_TEXT_1,
                Combined.TITLE + " AS " + SearchManager.SUGGEST_COLUMN_TEXT_1);
        map.put(SearchManager.SUGGEST_COLUMN_TEXT_2_URL,
                Combined.URL + " AS " + SearchManager.SUGGEST_COLUMN_TEXT_2_URL);
        map.put(SearchManager.SUGGEST_COLUMN_INTENT_DATA,
                Combined.URL + " AS " + SearchManager.SUGGEST_COLUMN_INTENT_DATA);
        SEARCH_SUGGEST_PROJECTION_MAP = Collections.unmodifiableMap(map);
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
    private static boolean logDebug   = Log.isLoggable(LOGTAG, Log.DEBUG);
    private static boolean logVerbose = Log.isLoggable(LOGTAG, Log.VERBOSE);
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
     * Remove enough history items to bring the database count below <code>retain</code>,
     * removing no items with a modified time after <code>keepAfter</code>.
     *
     * Provide <code>keepAfter</code> less than or equal to zero to skip that check.
     *
     * Items will be removed according to an approximate frecency calculation.
     */
    private void expireHistory(final SQLiteDatabase db, final int retain, final long keepAfter) {
        Log.d(LOGTAG, "Expiring history.");
        final long rows = DatabaseUtils.queryNumEntries(db, TABLE_HISTORY);

        if (retain >= rows) {
            debug("Not expiring history: only have " + rows + " rows.");
            return;
        }

        final String sortOrder = BrowserContract.getFrecencySortOrder(false, true);
        final long toRemove = rows - retain;
        debug("Expiring at most " + toRemove + " rows earlier than " + keepAfter + ".");

        final String sql;
        if (keepAfter > 0) {
            sql = "DELETE FROM " + TABLE_HISTORY + " " +
                  "WHERE MAX(" + History.DATE_LAST_VISITED + ", " + History.DATE_MODIFIED +") < " + keepAfter + " " +
                  " AND " + History._ID + " IN ( SELECT " +
                    History._ID + " FROM " + TABLE_HISTORY + " " +
                    "ORDER BY " + sortOrder + " LIMIT " + toRemove +
                  ")";
        } else {
            sql = "DELETE FROM " + TABLE_HISTORY + " WHERE " + History._ID + " " +
                  "IN ( SELECT " + History._ID + " FROM " + TABLE_HISTORY + " " +
                  "ORDER BY " + sortOrder + " LIMIT " + toRemove + ")";
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
        final String sortOrder = BrowserContract.getFrecencySortOrder(true, false);
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
                           ")";
        trace("Clear thumbs using query: " + sql);
        db.execSQL(sql);
    }

    private boolean shouldIncrementVisits(Uri uri) {
        String incrementVisits = uri.getQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS);
        return Boolean.parseBoolean(incrementVisits);
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
            case SEARCH_SUGGEST:
                trace("URI is SEARCH_SUGGEST: " + uri);
                return SearchManager.SUGGEST_MIME_TYPE;
            case FLAGS:
                trace("URI is FLAGS.");
                return Bookmarks.CONTENT_ITEM_TYPE;
        }

        debug("URI has unrecognized type: " + uri);

        return null;
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
                deleted = deleteHistory(uri, selection, selectionArgs);
                deleteUnusedImages(uri);
                break;
            }

            case HISTORY_OLD: {
                String priority = uri.getQueryParameter(BrowserContract.PARAM_EXPIRE_PRIORITY);
                long keepAfter = System.currentTimeMillis() - DEFAULT_EXPIRY_PRESERVE_WINDOW;
                int retainCount = DEFAULT_EXPIRY_RETAIN_COUNT;

                if (BrowserContract.ExpirePriority.AGGRESSIVE.toString().equals(priority)) {
                    keepAfter = 0;
                    retainCount = AGGRESSIVE_EXPIRY_RETAIN_COUNT;
                }
                expireHistory(db, retainCount, keepAfter);
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

            default:
                throw new UnsupportedOperationException("Unknown delete URI " + uri);
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

            default:
                throw new UnsupportedOperationException("Unknown insert URI " + uri);
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

            default:
                throw new UnsupportedOperationException("Unknown update URI " + uri);
        }

        debug("Updated " + updated + " rows for URI: " + uri);
        return updated;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder) {
        SQLiteDatabase db = getReadableDatabase(uri);
        final int match = URI_MATCHER.match(uri);

        // The first selectionArgs value is the URI for which to query.
        if (match == FLAGS) {
            // We don't need the QB below for this.
            //
            // There are three possible kinds of bookmarks:
            // * Regular bookmarks
            // * Bookmarks whose parent is FIXED_READING_LIST_ID (reading list items)
            // * Bookmarks whose parent is FIXED_PINNED_LIST_ID (pinned items).
            //
            // Although SQLite doesn't have an aggregate operator for bitwise-OR, we're
            // using disjoint flags, so we can simply use SUM and DISTINCT to get the
            // flags we need.
            // We turn parents into flags according to the three kinds, above.
            //
            // When this query is extended to support queries across multiple tables, simply
            // extend it to look like
            //
            // SELECT COALESCE((SELECT ...), 0) | COALESCE(...) | ...

            final boolean includeDeleted = shouldShowDeleted(uri);
            final String query = "SELECT COALESCE(SUM(flag), 0) AS flags " +
                "FROM ( SELECT DISTINCT CASE" +
                " WHEN " + Bookmarks.PARENT + " = " + Bookmarks.FIXED_READING_LIST_ID +
                " THEN " + Bookmarks.FLAG_READING +

                " WHEN " + Bookmarks.PARENT + " = " + Bookmarks.FIXED_PINNED_LIST_ID +
                " THEN " + Bookmarks.FLAG_PINNED +

                " ELSE " + Bookmarks.FLAG_BOOKMARK +
                " END flag " +
                "FROM " + TABLE_BOOKMARKS + " WHERE " +
                Bookmarks.URL + " = ? " +
                (includeDeleted ? "" : ("AND " + Bookmarks.IS_DELETED + " = 0")) +
                ")";

            return db.rawQuery(query, selectionArgs);
        }

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

                if (hasFaviconsInProjection(projection))
                    qb.setTables(VIEW_BOOKMARKS_WITH_FAVICONS);
                else
                    qb.setTables(TABLE_BOOKMARKS);

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

            case SEARCH_SUGGEST: {
                debug("Query is on search suggest: " + uri);
                selection = DBUtils.concatenateWhere(selection, "(" + Combined.URL + " LIKE ? OR " +
                                                                      Combined.TITLE + " LIKE ?)");

                String keyword = uri.getLastPathSegment();
                if (keyword == null)
                    keyword = "";

                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { "%" + keyword + "%",
                                       "%" + keyword + "%" });

                if (TextUtils.isEmpty(sortOrder))
                    sortOrder = DEFAULT_HISTORY_SORT_ORDER;

                qb.setProjectionMap(SEARCH_SUGGEST_PROJECTION_MAP);
                qb.setTables(VIEW_COMBINED_WITH_FAVICONS);

                break;
            }

            default:
                throw new UnsupportedOperationException("Unknown query URI " + uri);
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

        // TODO: use computeSQLInClause
        b.append(" END WHERE " + Bookmarks.GUID + " IN (");
        i = 1;
        while (i++ < processCount) {
            b.append("?, ");
        }
        b.append("?)");
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
            inClause = computeSQLInClauseFromLongs(cursor, Bookmarks._ID);
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

        // Insert a new entry if necessary
        if (!values.containsKey(History.VISITS)) {
            values.put(History.VISITS, 1);
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

        int updated = 0;

        final String[] historyProjection = new String[] {
            History._ID,   // 0
            History.URL,   // 1
            History.VISITS // 2
        };

        final SQLiteDatabase db = getWritableDatabase(uri);
        final Cursor cursor = db.query(TABLE_HISTORY, historyProjection, selection,
                                       selectionArgs, null, null, null);

        try {
            if (!values.containsKey(Bookmarks.DATE_MODIFIED)) {
                values.put(Bookmarks.DATE_MODIFIED,  System.currentTimeMillis());
            }

            while (cursor.moveToNext()) {
                long id = cursor.getLong(0);

                trace("Updating history entry with ID: " + id);

                if (shouldIncrementVisits(uri)) {
                    long existing = cursor.getLong(2);
                    Long additional = values.getAsLong(History.VISITS);

                    // Increment visit count by a specified amount, or default to increment by 1
                    values.put(History.VISITS, existing + ((additional != null) ? additional.longValue() : 1));
                }

                updated += db.update(TABLE_HISTORY, values, "_id = ?",
                                     new String[] { Long.toString(id) });
            }
        } finally {
            cursor.close();
        }

        return updated;
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
            values.put(Favicons.URL, org.mozilla.gecko.favicons.Favicons.guessDefaultFaviconURL(pageUrl));
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
    private int deleteHistory(Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting history entry for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);

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
               deleteThumbnails(uri, thumbnailSelection, null);
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
}
