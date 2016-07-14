/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.distribution;

import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.CursorWrapper;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.text.TextUtils;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.db.BrowserContract;

import java.util.HashSet;
import java.util.Set;

/**
 * A proxy for the partner bookmarks provider. Bookmark and folder ids of the partner bookmarks providers
 * will be transformed so that they do not overlap with the ids from the local database.
 *
 * Bookmarks in folder:
 *   content://{PACKAGE_ID}.partnerbookmarks/bookmarks/{folderId}
 * Icon of bookmark:
 *   content://{PACKAGE_ID}.partnerbookmarks/icons/{bookmarkId}
 */
public class PartnerBookmarksProviderProxy extends ContentProvider {
    /**
     * The contract between the partner bookmarks provider and applications. Contains the definition
     * for the supported URIs and columns.
     */
    public static class PartnerContract {
        public static final Uri CONTENT_URI = Uri.parse("content://com.android.partnerbookmarks/bookmarks");

        public static final int TYPE_BOOKMARK = 1;
        public static final int TYPE_FOLDER = 2;

        public static final int PARENT_ROOT_ID = 0;

        public static final String ID = "_id";
        public static final String TYPE = "type";
        public static final String URL = "url";
        public static final String TITLE = "title";
        public static final String FAVICON = "favicon";
        public static final String TOUCHICON = "touchicon";
        public static final String PARENT = "parent";
    }

    private static final String AUTHORITY_PREFIX = ".partnerbookmarks";

    private static final int URI_MATCH_BOOKMARKS = 1000;
    private static final int URI_MATCH_ICON = 1001;
    private static final int URI_MATCH_BOOKMARK = 1002;

    private static final String PREF_DELETED_PARTNER_BOOKMARKS = "distribution.partner.bookmark.deleted";

    /**
     * Cursor wrapper for filtering empty folders.
     */
    private static class FilteredCursor extends CursorWrapper {
        private HashSet<Integer> emptyFolderPositions;
        private int count;

        public FilteredCursor(PartnerBookmarksProviderProxy proxy, Cursor cursor) {
            super(cursor);

            emptyFolderPositions = new HashSet<>();
            count = cursor.getCount();

            for (int i = 0; i < cursor.getCount(); i++) {
                cursor.moveToPosition(i);

                final long id = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks._ID));
                final int type = cursor.getInt(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TYPE));

                if (type == BrowserContract.Bookmarks.TYPE_FOLDER && proxy.isFolderEmpty(id)) {
                    // We do not support deleting folders. So at least hide partner folders that are
                    // empty because all bookmarks inside it are deleted/hidden.
                    // Note that this will still show folders with empty folders in them. But multi-level
                    // partner bookmarks are very unlikely.

                    count--;
                    emptyFolderPositions.add(i);
                }
            }
        }

        @Override
        public int getCount() {
            return count;
        }

        @Override
        public boolean moveToPosition(int position) {
            final Cursor cursor = getWrappedCursor();
            final int actualCount = cursor.getCount();

            // Find the next position pointing to a bookmark or a non-empty folder
            while (position < actualCount && emptyFolderPositions.contains(position)) {
                position++;
            }

            return position < actualCount && cursor.moveToPosition(position);
        }
    }

    private static String getAuthority(Context context) {
        return context.getPackageName() + AUTHORITY_PREFIX;
    }

    public static Uri getUriForBookmarks(Context context, long folderId) {
        return new Uri.Builder()
                .scheme("content")
                .authority(getAuthority(context))
                .appendPath("bookmarks")
                .appendPath(String.valueOf(folderId))
                .build();
    }

    public static Uri getUriForIcon(Context context, long bookmarkId) {
        return new Uri.Builder()
                .scheme("content")
                .authority(getAuthority(context))
                .appendPath("icons")
                .appendPath(String.valueOf(bookmarkId))
                .build();
    }

    public static Uri getUriForBookmark(Context context, long bookmarkId) {
        return new Uri.Builder()
                .scheme("content")
                .authority(getAuthority(context))
                .appendPath("bookmark")
                .appendPath(String.valueOf(bookmarkId))
                .build();
    }

    private final UriMatcher uriMatcher = new UriMatcher(UriMatcher.NO_MATCH);

    @Override
    public boolean onCreate() {
        String authority = getAuthority(assertAndGetContext());

        uriMatcher.addURI(authority, "bookmarks/*", URI_MATCH_BOOKMARKS);
        uriMatcher.addURI(authority, "icons/*", URI_MATCH_ICON);
        uriMatcher.addURI(authority, "bookmark/*", URI_MATCH_BOOKMARK);

        return true;
    }

    @Override
    public Cursor query(@NonNull Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        final Context context = assertAndGetContext();
        final int match = uriMatcher.match(uri);

        final ContentResolver contentResolver = context.getContentResolver();

        switch (match) {
            case URI_MATCH_BOOKMARKS:
                final long bookmarkId = ContentUris.parseId(uri);
                if (bookmarkId == -1) {
                    throw new IllegalArgumentException("Bookmark id is not a number");
                }
                final Cursor cursor = getBookmarksInFolder(contentResolver, bookmarkId);
                cursor.setNotificationUri(context.getContentResolver(), uri);
                return new FilteredCursor(this, cursor);

            case URI_MATCH_ICON:
                return getIcon(contentResolver, ContentUris.parseId(uri));

            default:
                throw new UnsupportedOperationException("Unknown URI " + uri.toString());
        }
    }

    @Override
    public int delete(@NonNull Uri uri, String selection, String[] selectionArgs) {
        final int match = uriMatcher.match(uri);

        switch (match) {
            case URI_MATCH_BOOKMARK:
                rememberRemovedBookmark(ContentUris.parseId(uri));
                notifyBookmarkChange();
                return 1;

            default:
                throw new UnsupportedOperationException("Unknown URI " + uri.toString());
        }
    }

    private void notifyBookmarkChange() {
        final Context context = assertAndGetContext();

        context.getContentResolver().notifyChange(
                new Uri.Builder()
                        .scheme("content")
                        .authority(getAuthority(context))
                        .appendPath("bookmarks")
                        .build(),
                null);
    }

    private synchronized void rememberRemovedBookmark(long bookmarkId) {
        Set<String> deletedIds = getRemovedBookmarkIds();

        deletedIds.add(String.valueOf(bookmarkId));

        GeckoSharedPrefs.forProfile(assertAndGetContext())
                .edit()
                .putStringSet(PREF_DELETED_PARTNER_BOOKMARKS, deletedIds)
                .apply();
    }

    private synchronized Set<String> getRemovedBookmarkIds() {
        SharedPreferences preferences = GeckoSharedPrefs.forProfile(assertAndGetContext());
        return preferences.getStringSet(PREF_DELETED_PARTNER_BOOKMARKS, new HashSet<String>());
    }

    private Cursor getBookmarksInFolder(ContentResolver contentResolver, long folderId) {
        // Use root folder id or transform negative id into actual (positive) folder id.
        final long actualFolderId = folderId == BrowserContract.Bookmarks.FIXED_ROOT_ID
                ? PartnerContract.PARENT_ROOT_ID
                : BrowserContract.Bookmarks.FAKE_PARTNER_BOOKMARKS_START - folderId;

        final String removedBookmarkIds = TextUtils.join(",", getRemovedBookmarkIds());

        return contentResolver.query(
                PartnerContract.CONTENT_URI,
                new String[] {
                        // Transform ids into negative values starting with FAKE_PARTNER_BOOKMARKS_START.
                        "(" + BrowserContract.Bookmarks.FAKE_PARTNER_BOOKMARKS_START + " - " + PartnerContract.ID + ") as " + BrowserContract.Bookmarks._ID,
                        "(" + BrowserContract.Bookmarks.FAKE_PARTNER_BOOKMARKS_START + " - " + PartnerContract.ID + ") as " + BrowserContract.Combined.BOOKMARK_ID,
                        PartnerContract.TITLE + " as " + BrowserContract.Bookmarks.TITLE,
                        PartnerContract.URL +  " as " + BrowserContract.Bookmarks.URL,
                        // Transform parent ids to negative ids as well
                        "(" + BrowserContract.Bookmarks.FAKE_PARTNER_BOOKMARKS_START + " - " + PartnerContract.PARENT + ") as " + BrowserContract.Bookmarks.PARENT,
                        // Convert types (we use 0-1 and the partner provider 1-2)
                        "(2 - " + PartnerContract.TYPE + ") as " + BrowserContract.Bookmarks.TYPE,
                        // Use the ID of the entry as GUID
                        PartnerContract.ID + " as " + BrowserContract.Bookmarks.GUID
                },
                PartnerContract.PARENT + " = ?"
                        // We only want to read bookmarks or folders from the content provider
                        + " AND " + BrowserContract.Bookmarks.TYPE + " IN (?,?)"
                        // Only select entries with non empty title
                        + " AND " + BrowserContract.Bookmarks.TITLE + " <> ''"
                        // Filter all "deleted" ids
                        + " AND " + BrowserContract.Combined.BOOKMARK_ID + " NOT IN (" + removedBookmarkIds + ")",
                new String[] {
                        String.valueOf(actualFolderId),
                        String.valueOf(PartnerContract.TYPE_BOOKMARK),
                        String.valueOf(PartnerContract.TYPE_FOLDER)
                },
                // Same order we use in our content provider (without position)
                BrowserContract.Bookmarks.TYPE + " ASC, " + BrowserContract.Bookmarks._ID + " ASC");
    }

    private boolean isFolderEmpty(long folderId) {
        final Context context = assertAndGetContext();
        final Cursor cursor = getBookmarksInFolder(context.getContentResolver(), folderId);

        if (cursor == null) {
            return true;
        }

        try {
            return cursor.getCount() == 0;
        } finally {
            cursor.close();
        }
    }

    private Cursor getIcon(ContentResolver contentResolver, long bookmarkId) {
        final long actualId = BrowserContract.Bookmarks.FAKE_PARTNER_BOOKMARKS_START - bookmarkId;

        return contentResolver.query(
                PartnerContract.CONTENT_URI,
                new String[] {
                    PartnerContract.TOUCHICON,
                    PartnerContract.FAVICON
                },
                PartnerContract.ID + " = ?",
                new String[] {
                    String.valueOf(actualId)
                },
                null);
    }

    private Context assertAndGetContext() {
        final Context context = super.getContext();

        if (context == null) {
            throw new AssertionError("Context is null");
        }

        return context;
    }

    @Override
    public String getType(@NonNull Uri uri) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Uri insert(@NonNull Uri uri, ContentValues values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int update(@NonNull Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }
}
