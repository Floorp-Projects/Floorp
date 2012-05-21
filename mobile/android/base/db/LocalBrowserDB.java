/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.io.ByteArrayOutputStream;
import java.util.HashMap;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.History;
import org.mozilla.gecko.db.BrowserContract.ImageColumns;
import org.mozilla.gecko.db.BrowserContract.Images;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserContract.URLColumns;
import org.mozilla.gecko.db.DBUtils;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.database.ContentObserver;
import android.database.Cursor;
import android.database.CursorWrapper;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.provider.Browser;
import android.util.Log;

public class LocalBrowserDB implements BrowserDB.BrowserDBIface {
    // Calculate these once, at initialization. isLoggable is too expensive to
    // have in-line in each log call.
    private static final String LOGTAG = "GeckoLocalBrowserDB";
    private static boolean logDebug = Log.isLoggable(LOGTAG, Log.DEBUG);
    protected static void debug(String message) {
        if (logDebug) {
            Log.d(LOGTAG, message);
        }
    }

    private final String mProfile;

    // Map of folder GUIDs to IDs. Used for caching.
    private HashMap<String, Long> mFolderIdMap;

    // Use wrapped Boolean so that we can have a null state
    private Boolean mDesktopBookmarksExist;

    private final Uri mBookmarksUriWithProfile;
    private final Uri mParentsUriWithProfile;
    private final Uri mHistoryUriWithProfile;
    private final Uri mImagesUriWithProfile;
    private final Uri mCombinedUriWithProfile;
    private final Uri mDeletedHistoryUriWithProfile;
    private final Uri mUpdateHistoryUriWithProfile;

    private static final String[] DEFAULT_BOOKMARK_COLUMNS =
            new String[] { Bookmarks._ID,
                           Bookmarks.GUID,
                           Bookmarks.URL,
                           Bookmarks.TITLE,
                           Bookmarks.TYPE,
                           Bookmarks.PARENT,
                           Bookmarks.KEYWORD,
                           Bookmarks.FAVICON }; 

    public LocalBrowserDB(String profile) {
        mProfile = profile;
        mFolderIdMap = new HashMap<String, Long>();
        mDesktopBookmarksExist = null;

        mBookmarksUriWithProfile = appendProfile(Bookmarks.CONTENT_URI);
        mParentsUriWithProfile = appendProfile(Bookmarks.PARENTS_CONTENT_URI);
        mHistoryUriWithProfile = appendProfile(History.CONTENT_URI);
        mImagesUriWithProfile = appendProfile(Images.CONTENT_URI);
        mCombinedUriWithProfile = appendProfile(Combined.CONTENT_URI);

        mDeletedHistoryUriWithProfile = mHistoryUriWithProfile.buildUpon().
            appendQueryParameter(BrowserContract.PARAM_SHOW_DELETED, "1").build();

        mUpdateHistoryUriWithProfile = mHistoryUriWithProfile.buildUpon().
            appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true").
            appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true").build();
    }

    // Invalidate cached data
    public void invalidateCachedState() {
        mDesktopBookmarksExist = null;
    }

    private Uri historyUriWithLimit(int limit) {
        return mHistoryUriWithProfile.buildUpon().appendQueryParameter(BrowserContract.PARAM_LIMIT,
                                                                       String.valueOf(limit)).build();
    }

    private Uri bookmarksUriWithLimit(int limit) {
        return mBookmarksUriWithProfile.buildUpon().appendQueryParameter(BrowserContract.PARAM_LIMIT,
                                                                         String.valueOf(limit)).build();
    }

    private Uri combinedUriWithLimit(int limit) {
        return mCombinedUriWithProfile.buildUpon().appendQueryParameter(BrowserContract.PARAM_LIMIT,
                String.valueOf(limit)).build();
    }

    private Uri appendProfile(Uri uri) {
        return uri.buildUpon().appendQueryParameter(BrowserContract.PARAM_PROFILE, mProfile).build();
    }

    private Cursor filterAllSites(ContentResolver cr, String[] projection, CharSequence constraint,
            int limit, CharSequence urlFilter) {
        String selection = "";
        String[] selectionArgs = null;

        // The combined history/bookmarks selection queries for sites with a url or title containing
        // the constraint string(s), treating space-separated words as separate constraints
        String[] constraintWords = constraint.toString().split(" ");
        for (int i = 0; i < constraintWords.length; i++) {
            selection = DBUtils.concatenateWhere(selection, "(" + Combined.URL + " LIKE ? OR " +
                                                                  Combined.TITLE + " LIKE ?)");
            String constraintWord =  "%" + constraintWords[i] + "%";
            selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                new String[] { constraintWord, constraintWord });
        }

        if (urlFilter != null) {
            selection = DBUtils.concatenateWhere(selection, "(" + Combined.URL + " NOT LIKE ?)");
            selectionArgs = DBUtils.appendSelectionArgs(selectionArgs, new String[] { urlFilter.toString() });
        }

        // Our version of frecency is computed by scaling the number of visits by a multiplier
        // that approximates Gaussian decay, based on how long ago the entry was last visited.
        // Since we're limited by the math we can do with sqlite, we're calculating this
        // approximation using the Cauchy distribution: multiplier = 15^2 / (age^2 + 15^2).
        // Using 15 as our scale parameter, we get a constant 15^2 = 225. Following this math,
        // frecencyScore = numVisits * max(1, 100 * 225 / (age*age + 225)). (See bug 704977)
        // We also give bookmarks an extra bonus boost by adding 100 points to their frecency score.
        final String age = "(" + Combined.DATE_LAST_VISITED + " - " + System.currentTimeMillis() + ") / 86400000";
        final String sortOrder = "(CASE WHEN " + Combined.BOOKMARK_ID + " > -1 THEN 100 ELSE 0 END) + " +
                                 Combined.VISITS + " * MAX(1, 100 * 225 / (" + age + "*" + age + " + 225)) DESC";

        Cursor c = cr.query(combinedUriWithLimit(limit),
                            projection,
                            selection,
                            selectionArgs,
                            sortOrder);

        return new LocalDBCursor(c);
    }

    public Cursor filter(ContentResolver cr, CharSequence constraint, int limit) {
        return filterAllSites(cr,
                              new String[] { Combined._ID,
                                             Combined.URL,
                                             Combined.TITLE,
                                             Combined.FAVICON,
                                             Combined.BOOKMARK_ID,
                                             Combined.HISTORY_ID },
                              constraint,
                              limit,
                              null);
    }

    public Cursor getTopSites(ContentResolver cr, int limit) {
        return filterAllSites(cr,
                              new String[] { Combined._ID,
                                             Combined.URL,
                                             Combined.TITLE,
                                             Combined.THUMBNAIL },
                              "",
                              limit,
                              BrowserDB.ABOUT_PAGES_URL_FILTER);
    }

    public void updateVisitedHistory(ContentResolver cr, String uri) {
        ContentValues values = new ContentValues();

        values.put(History.URL, uri);
        values.put(History.DATE_LAST_VISITED, System.currentTimeMillis());
        values.put(History.IS_DELETED, 0);

        // This will insert a new history entry if one for this URL
        // doesn't already exist
        cr.update(mUpdateHistoryUriWithProfile,
                  values,
                  History.URL + " = ?",
                  new String[] { uri });
    }

    public void updateHistoryTitle(ContentResolver cr, String uri, String title) {
        ContentValues values = new ContentValues();
        values.put(History.TITLE, title);

        cr.update(mHistoryUriWithProfile,
                  values,
                  History.URL + " = ?",
                  new String[] { uri });
    }

    public void updateHistoryEntry(ContentResolver cr, String uri, String title,
                                   long date, int visits) {
        int oldVisits = 0;
        Cursor cursor = null;
        try {
            cursor = cr.query(mHistoryUriWithProfile,
                              new String[] { History.VISITS },
                              History.URL + " = ?",
                              new String[] { uri },
                              null);

            if (cursor.moveToFirst()) {
                oldVisits = cursor.getInt(0);
            }
        } finally {
            if (cursor != null)
                cursor.close();
        }

        ContentValues values = new ContentValues();
        values.put(History.DATE_LAST_VISITED, date);
        values.put(History.VISITS, oldVisits + visits);
        if (title != null) {
            values.put(History.TITLE, title);
        }

        cr.update(mHistoryUriWithProfile,
                  values,
                  History.URL + " = ?",
                  new String[] { uri });
    }

    public Cursor getAllVisitedHistory(ContentResolver cr) {
        Cursor c = cr.query(mHistoryUriWithProfile,
                            new String[] { History.URL },
                            History.VISITS + " > 0",
                            null,
                            null);

        return new LocalDBCursor(c);
    }

    public Cursor getRecentHistory(ContentResolver cr, int limit) {
        Cursor c = cr.query(combinedUriWithLimit(limit),
                            new String[] { Combined._ID,
                                           Combined.BOOKMARK_ID,
                                           Combined.HISTORY_ID,
                                           Combined.URL,
                                           Combined.TITLE,
                                           Combined.FAVICON,
                                           Combined.DATE_LAST_VISITED,
                                           Combined.VISITS },
                            History.DATE_LAST_VISITED + " > 0",
                            null,
                            History.DATE_LAST_VISITED + " DESC");

        return new LocalDBCursor(c);
    }

    public void removeHistoryEntry(ContentResolver cr, int id) {
        cr.delete(mHistoryUriWithProfile,
                  History._ID + " = ?",
                  new String[] { String.valueOf(id) });
    }

    public void clearHistory(ContentResolver cr) {
        cr.delete(mHistoryUriWithProfile, null, null);
    }

    public Cursor getBookmarksInFolder(ContentResolver cr, long folderId) {
        Cursor c = null;
        boolean addDesktopFolder = false;

        // We always want to show mobile bookmarks in the root view.
        if (folderId == Bookmarks.FIXED_ROOT_ID) {
            folderId = getFolderIdFromGuid(cr, Bookmarks.MOBILE_FOLDER_GUID);

            // We'll add a fake "Desktop Bookmarks" folder to the root view if desktop 
            // bookmarks exist, so that the user can still access non-mobile bookmarks.
            addDesktopFolder = desktopBookmarksExist(cr);
        }

        if (folderId == Bookmarks.FAKE_DESKTOP_FOLDER_ID) {
            // Since the "Desktop Bookmarks" folder doesn't actually exist, we
            // just fake it by querying specifically certain known desktop folders.
            c = cr.query(mBookmarksUriWithProfile,
                         DEFAULT_BOOKMARK_COLUMNS,
                         Bookmarks.GUID + " = ? OR " +
                         Bookmarks.GUID + " = ? OR " +
                         Bookmarks.GUID + " = ?",
                         new String[] { Bookmarks.TOOLBAR_FOLDER_GUID,
                                        Bookmarks.MENU_FOLDER_GUID,
                                        Bookmarks.UNFILED_FOLDER_GUID },
                         null);
        } else {
            // Right now, we only support showing folder and bookmark type of
            // entries. We should add support for other types though (bug 737024)
            c = cr.query(mBookmarksUriWithProfile,
                         DEFAULT_BOOKMARK_COLUMNS,
                         Bookmarks.PARENT + " = ? AND " +
                         "(" + Bookmarks.TYPE + " = ? OR " + Bookmarks.TYPE + " = ?)",
                         new String[] { String.valueOf(folderId),
                                        String.valueOf(Bookmarks.TYPE_BOOKMARK),
                                        String.valueOf(Bookmarks.TYPE_FOLDER) },
                         null);
        }

        if (addDesktopFolder) {
            // Wrap cursor to add fake desktop bookmarks folder
            c = new DesktopBookmarksCursorWrapper(c);
        }

        return new LocalDBCursor(c);
    }

    // Returns true if any desktop bookmarks exist, which will be true if the user
    // has set up sync at one point, or done a profile migration from XUL fennec.
    private boolean desktopBookmarksExist(ContentResolver cr) {
        if (mDesktopBookmarksExist != null)
            return mDesktopBookmarksExist;

        Cursor c = null;
        int count = 0;
        try {
            // Check to see if there are any bookmarks in one of our three
            // fixed "Desktop Boomarks" folders.
            c = cr.query(bookmarksUriWithLimit(1),
                         new String[] { Bookmarks._ID },
                         Bookmarks.PARENT + " = ? OR " +
                         Bookmarks.PARENT + " = ? OR " +
                         Bookmarks.PARENT + " = ?",
                         new String[] { String.valueOf(getFolderIdFromGuid(cr, Bookmarks.TOOLBAR_FOLDER_GUID)),
                                        String.valueOf(getFolderIdFromGuid(cr, Bookmarks.MENU_FOLDER_GUID)),
                                        String.valueOf(getFolderIdFromGuid(cr, Bookmarks.UNFILED_FOLDER_GUID)) },
                         null);
            count = c.getCount();
        } finally {
            c.close();
        }

        // Cache result for future queries
        mDesktopBookmarksExist = (count == 1);
        return mDesktopBookmarksExist;
    }

    public boolean isBookmark(ContentResolver cr, String uri) {
        Cursor cursor = cr.query(mBookmarksUriWithProfile,
                                 new String[] { Bookmarks._ID },
                                 Bookmarks.URL + " = ?",
                                 new String[] { uri },
                                 Bookmarks.URL);

        int count = cursor.getCount();
        cursor.close();

        return (count == 1);
    }

    public String getUrlForKeyword(ContentResolver cr, String keyword) {
        Cursor cursor = cr.query(mBookmarksUriWithProfile,
                                 new String[] { Bookmarks.URL },
                                 Bookmarks.KEYWORD + " = ?",
                                 new String[] { keyword },
                                 null);

        if (!cursor.moveToFirst()) {
            cursor.close();
            return null;
        }

        String url = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.URL));
        cursor.close();

        return url;
    }

    private synchronized long getFolderIdFromGuid(ContentResolver cr, String guid) {
        if (mFolderIdMap.containsKey(guid))
          return mFolderIdMap.get(guid);

        long folderId = -1;
        Cursor c = null;

        try {
            c = cr.query(mBookmarksUriWithProfile,
                         new String[] { Bookmarks._ID },
                         Bookmarks.GUID + " = ?",
                         new String[] { guid },
                         null);

            if (c.moveToFirst())
                folderId = c.getLong(c.getColumnIndexOrThrow(Bookmarks._ID));
        } finally {
            if (c != null)
                c.close();
        }

        mFolderIdMap.put(guid, folderId);
        return folderId;
    }

    /**
     * Find parents of records that match the provided criteria, and bump their
     * modified timestamp.
     */
    protected void bumpParents(ContentResolver cr, String param, String value) {
        ContentValues values = new ContentValues();
        values.put(Bookmarks.DATE_MODIFIED, System.currentTimeMillis());

        String where  = param + " = ?";
        String[] args = new String[] { value };
        int updated  = cr.update(mParentsUriWithProfile, values, where, args);
        debug("Updated " + updated + " rows to new modified time.");
    }

    public void addBookmark(ContentResolver cr, String title, String uri) {
        long folderId = getFolderIdFromGuid(cr, Bookmarks.MOBILE_FOLDER_GUID);
        if (folderId < 0)
            return;

        final long now = System.currentTimeMillis();
        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.TITLE, title);
        values.put(Bookmarks.URL, uri);
        values.put(Bookmarks.PARENT, folderId);
        values.put(Bookmarks.DATE_MODIFIED, now);

        // Restore deleted record if possible
        values.put(Bookmarks.IS_DELETED, 0);

        Uri contentUri = mBookmarksUriWithProfile;
        int updated = cr.update(contentUri,
                                values,
                                Bookmarks.URL + " = ?",
                                new String[] { uri });

        if (updated == 0)
            cr.insert(contentUri, values);

        // Bump parent modified time using its ID.
        debug("Bumping parent modified time for addition to: " + folderId);
        final String where  = Bookmarks._ID + " = ?";
        final String[] args = new String[] { String.valueOf(folderId) };

        ContentValues bumped = new ContentValues();
        bumped.put(Bookmarks.DATE_MODIFIED, now);

        updated = cr.update(contentUri, bumped, where, args);
        debug("Updated " + updated + " rows to new modified time.");
    }

    public void removeBookmark(ContentResolver cr, int id) {
        Uri contentUri = mBookmarksUriWithProfile;

        // Do this now so that the item still exists!
        final String idString = String.valueOf(id);
        bumpParents(cr, Bookmarks._ID, idString);

        final String[] idArgs = new String[] { idString };
        final String idEquals = Bookmarks._ID + " = ?";
        cr.delete(contentUri, idEquals, idArgs);
    }

    public void removeBookmarksWithURL(ContentResolver cr, String uri) {
        Uri contentUri = mBookmarksUriWithProfile;

        // Do this now so that the items still exist!
        bumpParents(cr, Bookmarks.URL, uri);

        final String[] urlArgs = new String[] { uri };
        final String urlEquals = Bookmarks.URL + " = ?";
        cr.delete(contentUri, urlEquals, urlArgs);
    }

    public void registerBookmarkObserver(ContentResolver cr, ContentObserver observer) {
        cr.registerContentObserver(mBookmarksUriWithProfile, false, observer);
    }

    public void registerHistoryObserver(ContentResolver cr, ContentObserver observer) {
        cr.registerContentObserver(mHistoryUriWithProfile, false, observer);
    }

    public void updateBookmark(ContentResolver cr, int id, String uri, String title, String keyword) {
        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.TITLE, title);
        values.put(Bookmarks.URL, uri);
        values.put(Bookmarks.KEYWORD, keyword);

        cr.update(mBookmarksUriWithProfile,
                  values,
                  Bookmarks._ID + " = ?",
                  new String[] { String.valueOf(id) });
    }

    public BitmapDrawable getFaviconForUrl(ContentResolver cr, String uri) {
        Cursor c = cr.query(mImagesUriWithProfile,
                            new String[] { Images.FAVICON },
                            Images.URL + " = ?",
                            new String[] { uri },
                            null);

        if (!c.moveToFirst()) {
            c.close();
            return null;
        }

        int faviconIndex = c.getColumnIndexOrThrow(Images.FAVICON);

        byte[] b = c.getBlob(faviconIndex);
        c.close();

        if (b == null)
            return null;

        Bitmap bitmap = BitmapFactory.decodeByteArray(b, 0, b.length);
        return new BitmapDrawable(bitmap);
    }

    public void updateFaviconForUrl(ContentResolver cr, String uri,
            BitmapDrawable favicon) {
        Bitmap bitmap = favicon.getBitmap();
        if (bitmap == null)
            return;

        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);

        ContentValues values = new ContentValues();
        values.put(Images.FAVICON, stream.toByteArray());
        values.put(Images.URL, uri);

        // Restore deleted record if possible
        values.put(Images.IS_DELETED, 0);

        int updated = cr.update(mImagesUriWithProfile,
                                values,
                                Images.URL + " = ?",
                                new String[] { uri });

        if (updated == 0)
            cr.insert(mImagesUriWithProfile, values);
    }

    public void updateThumbnailForUrl(ContentResolver cr, String uri,
            BitmapDrawable thumbnail) {
        Bitmap bitmap = thumbnail.getBitmap();

        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 0, stream);

        ContentValues values = new ContentValues();
        values.put(Images.THUMBNAIL, stream.toByteArray());
        values.put(Images.URL, uri);

        // Restore deleted record if possible
        values.put(Images.IS_DELETED, 0);

        int updated = cr.update(mImagesUriWithProfile,
                                values,
                                Images.URL + " = ?",
                                new String[] { uri });

        if (updated == 0)
            cr.insert(mImagesUriWithProfile, values);
    }

    public byte[] getThumbnailForUrl(ContentResolver cr, String uri) {
        Cursor c = cr.query(mImagesUriWithProfile,
                            new String[] { Images.THUMBNAIL },
                            Images.URL + " = ?",
                            new String[] { uri },
                            null);

        if (!c.moveToFirst()) {
            c.close();
            return null;
        }

        int thumbnailIndex = c.getColumnIndexOrThrow(Images.THUMBNAIL);

        byte[] b = c.getBlob(thumbnailIndex);
        c.close();

        return b;
    }

    // This wrapper adds a fake "Desktop Bookmarks" folder entry to the
    // beginning of the cursor's data set.
    private static class DesktopBookmarksCursorWrapper extends CursorWrapper {
        private boolean mAtDesktopBookmarksPosition = false;

        public DesktopBookmarksCursorWrapper(Cursor c) {
            super(c);
        }

        @Override
        public int getCount() {
            return super.getCount() + 1;
        }

        @Override
        public boolean moveToPosition(int position) {
            if (position == 0) {
                mAtDesktopBookmarksPosition = true;
                return true;
            }

            mAtDesktopBookmarksPosition = false;
            return super.moveToPosition(position - 1);
        }

        @Override
        public long getLong(int columnIndex) {
            if (!mAtDesktopBookmarksPosition)
                return super.getLong(columnIndex);

            if (columnIndex == getColumnIndex(Bookmarks._ID))
                return Bookmarks.FAKE_DESKTOP_FOLDER_ID;
            if (columnIndex == getColumnIndex(Bookmarks.PARENT))
                return Bookmarks.FIXED_ROOT_ID;

            return -1;
        }

        @Override
        public int getInt(int columnIndex) {
            if (!mAtDesktopBookmarksPosition)
                return super.getInt(columnIndex);

            if (columnIndex == getColumnIndex(Bookmarks.TYPE))
                return Bookmarks.TYPE_FOLDER;

            return -1;
        }

        @Override
        public String getString(int columnIndex) {
            if (!mAtDesktopBookmarksPosition)
                return super.getString(columnIndex);

            if (columnIndex == getColumnIndex(Bookmarks.GUID))
                return Bookmarks.FAKE_DESKTOP_FOLDER_GUID;

            return "";
        }
    }

    private static class LocalDBCursor extends CursorWrapper {
        public LocalDBCursor(Cursor c) {
            super(c);
        }

        private String translateColumnName(String columnName) {
            if (columnName.equals(BrowserDB.URLColumns.URL)) {
                columnName = URLColumns.URL;
            } else if (columnName.equals(BrowserDB.URLColumns.TITLE)) {
                columnName = URLColumns.TITLE;
            } else if (columnName.equals(BrowserDB.URLColumns.FAVICON)) {
                columnName = ImageColumns.FAVICON;
            } else if (columnName.equals(BrowserDB.URLColumns.THUMBNAIL)) {
                columnName = ImageColumns.THUMBNAIL;
            } else if (columnName.equals(BrowserDB.URLColumns.DATE_LAST_VISITED)) {
                columnName = History.DATE_LAST_VISITED;
            } else if (columnName.equals(BrowserDB.URLColumns.VISITS)) {
                columnName = History.VISITS;
            }

            return columnName;
        }

        @Override
        public int getColumnIndex(String columnName) {
            return super.getColumnIndex(translateColumnName(columnName));
        }

        @Override
        public int getColumnIndexOrThrow(String columnName) {
            return super.getColumnIndexOrThrow(translateColumnName(columnName));
        }
    }
}
