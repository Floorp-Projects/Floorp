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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lucas Rocha <lucasr@mozilla.com>
 *   Richard Newman <rnewman@mozilla.com>
 *   Margaret Leibovic <margaret.leibovic@gmail.com>
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

package org.mozilla.gecko.db;

import java.io.ByteArrayOutputStream;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.History;
import org.mozilla.gecko.db.BrowserContract.ImageColumns;
import org.mozilla.gecko.db.BrowserContract.Images;
import org.mozilla.gecko.db.BrowserContract.URLColumns;

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
    // Same as android.provider.Browser for consistency.
    private static final int MAX_HISTORY_COUNT = 250;

    // Same as android.provider.Browser for consistency.
    public static final int TRUNCATE_N_OLDEST = 5;

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
    private long mMobileFolderId;
    private long mTagsFolderId;

    private final Uri mBookmarksUriWithProfile;
    private final Uri mParentsUriWithProfile;
    private final Uri mHistoryUriWithProfile;
    private final Uri mImagesUriWithProfile;
    private final Uri mDeletedHistoryUriWithProfile;

    private static final String[] DEFAULT_BOOKMARK_COLUMNS =
            new String[] { Bookmarks._ID,
                           Bookmarks.GUID,
                           Bookmarks.URL,
                           Bookmarks.TITLE,
                           Bookmarks.IS_FOLDER,
                           Bookmarks.PARENT,
                           Bookmarks.FAVICON }; 

    public LocalBrowserDB(String profile) {
        mProfile = profile;
        mMobileFolderId = -1;
        mTagsFolderId = -1;

        mBookmarksUriWithProfile = appendProfile(Bookmarks.CONTENT_URI);
        mParentsUriWithProfile = appendProfile(Bookmarks.PARENTS_CONTENT_URI);
        mHistoryUriWithProfile = appendProfile(History.CONTENT_URI);
        mImagesUriWithProfile = appendProfile(Images.CONTENT_URI);

        mDeletedHistoryUriWithProfile = mHistoryUriWithProfile.buildUpon().
            appendQueryParameter(BrowserContract.PARAM_SHOW_DELETED, "1").build();
    }

    private Uri historyUriWithLimit(int limit) {
        return mHistoryUriWithProfile.buildUpon().appendQueryParameter(BrowserContract.PARAM_LIMIT,
                                                                       String.valueOf(limit)).build();
    }

    private Uri appendProfile(Uri uri) {
        return uri.buildUpon().appendQueryParameter(BrowserContract.PARAM_PROFILE, mProfile).build();
    }

    private Cursor filterAllSites(ContentResolver cr, String[] projection, CharSequence constraint, int limit, CharSequence urlFilter) {
        Cursor c = cr.query(historyUriWithLimit(limit),
                            projection,
                            (urlFilter != null ? "(" + History.URL + " NOT LIKE ? ) AND " : "" ) + 
                            "(" + History.URL + " LIKE ? OR " + History.TITLE + " LIKE ?)",
                            urlFilter == null ? new String[] {"%" + constraint.toString() + "%", "%" + constraint.toString() + "%"} :
                            new String[] {urlFilter.toString(), "%" + constraint.toString() + "%", "%" + constraint.toString() + "%"},
                            // ORDER BY is number of visits times a multiplier from 1 - 120 of how recently the site
                            // was accessed with a site accessed today getting 120 and a site accessed 119 or more
                            // days ago getting 1
                            History.VISITS + " * MAX(1, (" +
                            History.DATE_LAST_VISITED + " - " + System.currentTimeMillis() + ") / 86400000 + 120) DESC");

        return new LocalDBCursor(c);
    }

    public Cursor filter(ContentResolver cr, CharSequence constraint, int limit) {
        return filterAllSites(cr,
                              new String[] { History._ID,
                                             History.URL,
                                             History.TITLE,
                                             History.FAVICON },
                              constraint,
                              limit,
                              null);
    }

    public Cursor getTopSites(ContentResolver cr, int limit) {
        return filterAllSites(cr,
                              new String[] { History._ID,
                                             History.URL,
                                             History.TITLE,
                                             History.THUMBNAIL },
                              "",
                              limit,
                              BrowserDB.ABOUT_PAGES_URL_FILTER);
    }

    private void truncateHistory(ContentResolver cr) {
        Cursor cursor = null;

        try {
            cursor = cr.query(mHistoryUriWithProfile,
                              new String[] { History._ID },
                              null,
                              null,
                              History.DATE_LAST_VISITED + " ASC");

            if (cursor.getCount() < MAX_HISTORY_COUNT)
                return;

            if (cursor.moveToFirst()) {
                for (int i = 0; i < TRUNCATE_N_OLDEST; i++) {
                    Uri historyUri = ContentUris.withAppendedId(History.CONTENT_URI, cursor.getLong(0)); 
                    cr.delete(appendProfile(historyUri), null, null);

                    if (!cursor.moveToNext())
                        break;
                }
            }
        } finally {
            if (cursor != null)
                cursor.close();
        }
    }

    public void updateVisitedHistory(ContentResolver cr, String uri) {
        long now = System.currentTimeMillis();
        Cursor cursor = null;

        try {
            final String[] projection = new String[] {
                    History._ID,    // 0
                    History.VISITS, // 1
            };

            cursor = cr.query(mDeletedHistoryUriWithProfile,
                              projection,
                              History.URL + " = ?",
                              new String[] { uri },
                              null);

            if (cursor.moveToFirst()) {
                ContentValues values = new ContentValues();

                values.put(History.VISITS, cursor.getInt(1) + 1);
                values.put(History.DATE_LAST_VISITED, now);

                // Restore deleted record if possible
                values.put(History.IS_DELETED, 0);

                Uri historyUri = ContentUris.withAppendedId(History.CONTENT_URI, cursor.getLong(0));
                cr.update(appendProfile(historyUri), values, null, null);
            } else {
                // Ensure we don't blow up our database with too
                // many history items.
                truncateHistory(cr);

                ContentValues values = new ContentValues();

                values.put(History.URL, uri);
                values.put(History.VISITS, 1);
                values.put(History.DATE_LAST_VISITED, now);
                values.put(History.TITLE, uri);

                cr.insert(mHistoryUriWithProfile, values);
            }
        } finally {
            if (cursor != null)
                cursor.close();
        }
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
        Cursor c = cr.query(historyUriWithLimit(limit),
                            new String[] { History._ID,
                                           History.URL,
                                           History.TITLE,
                                           History.FAVICON,
                                           History.DATE_LAST_VISITED,
                                           History.VISITS },
                            History.DATE_LAST_VISITED + " > 0",
                            null,
                            History.DATE_LAST_VISITED + " DESC");

        return new LocalDBCursor(c);
    }

    public int getMaxHistoryCount() {
        return MAX_HISTORY_COUNT;
    }

    public void clearHistory(ContentResolver cr) {
        cr.delete(mHistoryUriWithProfile, null, null);
    }

    // This method filters out the root folder and the tags folder, since we
    // don't want to see those in the UI
    public Cursor getBookmarksInFolder(ContentResolver cr, long folderId) {
        Cursor c = null;

        if (folderId == Bookmarks.FIXED_ROOT_ID) {
            // Because of sync, we can end up with some additional records under
            // the root node that we don't want to see. Since sync doesn't 
            // want to run into problems deleting these, we can just ignore them
            // by querying specifically for only the folders we care about.
            c = cr.query(mBookmarksUriWithProfile,
                         DEFAULT_BOOKMARK_COLUMNS,
                         Bookmarks.PARENT + " = ? AND (" +
                         Bookmarks.GUID + " = ? OR " +
                         Bookmarks.GUID + " = ? OR " +
                         Bookmarks.GUID + " = ? OR " +
                         Bookmarks.GUID + " = ?)",
                         new String[] { String.valueOf(folderId),
                                        Bookmarks.MOBILE_FOLDER_GUID,
                                        Bookmarks.TOOLBAR_FOLDER_GUID,
                                        Bookmarks.MENU_FOLDER_GUID,
                                        Bookmarks.UNFILED_FOLDER_GUID },
                         null);
        } else {
            c = cr.query(mBookmarksUriWithProfile,
                         DEFAULT_BOOKMARK_COLUMNS,
                         Bookmarks.PARENT + " = ? ",
                         new String[] { String.valueOf(folderId) },
                         null);
        }

        return new LocalDBCursor(c);
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

    private long getMobileBookmarksFolderId(ContentResolver cr) {
        if (mMobileFolderId >= 0)
            return mMobileFolderId;

        mMobileFolderId = getFolderIdFromGuid(cr, Bookmarks.MOBILE_FOLDER_GUID);
        return mMobileFolderId;
    }

    private long getFolderIdFromGuid(ContentResolver cr, String guid) {
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
        long folderId = getMobileBookmarksFolderId(cr);
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
        Uri uri = mBookmarksUriWithProfile;
        cr.registerContentObserver(uri, false, observer);
    }

    public void updateBookmark(ContentResolver cr, String oldUri, String uri, String title, String keyword) {
        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.TITLE, title);
        values.put(Bookmarks.URL, uri);
        values.put(Bookmarks.KEYWORD, keyword);

        cr.update(mBookmarksUriWithProfile,
                  values,
                  Bookmarks.URL + " = ?",
                  new String[] { oldUri });
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
