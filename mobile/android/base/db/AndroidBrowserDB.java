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

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.ContentObserver;
import android.database.Cursor;
import android.database.CursorWrapper;
import android.database.sqlite.SQLiteConstraintException;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.provider.Browser.BookmarkColumns;
import android.util.Log;

public class AndroidBrowserDB implements BrowserDB.BrowserDBIface {
    private static final String LOGTAG = "AndroidBrowserDB";
    private static final String URL_COLUMN_ID = "_id";
    private static final String URL_COLUMN_THUMBNAIL = "thumbnail";

    // Only available on Android >= 11
    private static final String URL_COLUMN_DELETED = "deleted";

    private static final Uri BOOKMARKS_CONTENT_URI_POST_11 = Uri.parse("content://com.android.browser/bookmarks");

    public void invalidateCachedState() {
        // Do nothing
    }

    private Cursor filterAllSites(ContentResolver cr, String[] projection, CharSequence constraint, int limit, CharSequence urlFilter) {
        Cursor c = cr.query(Browser.BOOKMARKS_URI,
                            projection,
                            // The length restriction on URL is for the same reason as in the general bookmark query
                            // (see comment earlier in this file).
                            (urlFilter != null ? "(" + Browser.BookmarkColumns.URL + " NOT LIKE ? ) AND " : "" ) + 
                            "(" + Browser.BookmarkColumns.URL + " LIKE ? OR " + Browser.BookmarkColumns.TITLE + " LIKE ?)"
                            + " AND LENGTH(" + Browser.BookmarkColumns.URL + ") > 0",
                            urlFilter == null ? new String[] {"%" + constraint.toString() + "%", "%" + constraint.toString() + "%"} :
                            new String[] {urlFilter.toString(), "%" + constraint.toString() + "%", "%" + constraint.toString() + "%"},
                            // ORDER BY is number of visits times a multiplier from 1 - 120 of how recently the site
                            // was accessed with a site accessed today getting 120 and a site accessed 119 or more
                            // days ago getting 1
                            Browser.BookmarkColumns.VISITS + " * MAX(1, (" +
                            Browser.BookmarkColumns.DATE + " - " + System.currentTimeMillis() + ") / 86400000 + 120) DESC LIMIT " + limit);

        return new AndroidDBCursor(c);
    }

    public Cursor filter(ContentResolver cr, CharSequence constraint, int limit) {
        return filterAllSites(cr,
                              new String[] { URL_COLUMN_ID,
                                             BookmarkColumns.URL,
                                             BookmarkColumns.TITLE,
                                             BookmarkColumns.FAVICON },
                              constraint,
                              limit,
                              null);
    }

    public Cursor getTopSites(ContentResolver cr, int limit) {
        return filterAllSites(cr,
                              new String[] { URL_COLUMN_ID,
                                             BookmarkColumns.URL,
                                             BookmarkColumns.TITLE,
                                             URL_COLUMN_THUMBNAIL },
                              "",
                              limit,
                              BrowserDB.ABOUT_PAGES_URL_FILTER);
    }

    public void updateVisitedHistory(ContentResolver cr, String uri) {
        Browser.updateVisitedHistory(cr, uri, true);
    }

    public void updateHistoryTitle(ContentResolver cr, String uri, String title) {
        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.TITLE, title);

        cr.update(Browser.BOOKMARKS_URI,
                  values,
                  Browser.BookmarkColumns.URL + " = ?",
                  new String[] { uri });
    }

    public void updateHistoryEntry(ContentResolver cr, String uri, String title,
                                   long date, int visits) {
        int oldVisits = 0;
        Cursor cursor = null;
        try {
            cursor = cr.query(Browser.BOOKMARKS_URI,
                              new String[] { Browser.BookmarkColumns.VISITS },
                              Browser.BookmarkColumns.URL + " = ?",
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
        values.put(Browser.BookmarkColumns.DATE, date);
        values.put(Browser.BookmarkColumns.VISITS, oldVisits + visits);
        if (title != null) {
            values.put(Browser.BookmarkColumns.TITLE, title);
        }

        cr.update(Browser.BOOKMARKS_URI,
                  values,
                  Browser.BookmarkColumns.URL + " = ?",
                  new String[] { uri });
    }

    public Cursor getAllVisitedHistory(ContentResolver cr) {
        Cursor c = cr.query(Browser.BOOKMARKS_URI,
                            new String[] { Browser.BookmarkColumns.URL },
                            Browser.BookmarkColumns.BOOKMARK + " = 0 AND " +
                            Browser.BookmarkColumns.VISITS + " > 0",
                            null,
                            null);

        return new AndroidDBCursor(c);
    }

    public Cursor getRecentHistory(ContentResolver cr, int limit) {
        Cursor c = cr.query(Browser.BOOKMARKS_URI,
                            new String[] { URL_COLUMN_ID,
                                           BookmarkColumns.URL,
                                           BookmarkColumns.TITLE,
                                           BookmarkColumns.FAVICON,
                                           BookmarkColumns.DATE,
                                           BookmarkColumns.VISITS },
                            // Bookmarks that have not been visited have a date value
                            // of 0, so don't pick them up in the history view.
                            Browser.BookmarkColumns.DATE + " > 0",
                            null,
                            Browser.BookmarkColumns.DATE + " DESC LIMIT " + limit);

        return new AndroidDBCursor(c);
    }

    public void clearHistory(ContentResolver cr) {
        Browser.clearHistory(cr);
    }

    public Cursor getBookmarksInFolder(ContentResolver cr, long folderId) {
        Cursor c = cr.query(null, null, null, null, null);
        return new AndroidDBCursor(c);
    }

    public Cursor isBookmarkQueryPre11(ContentResolver cr, String uri) {
        return cr.query(Browser.BOOKMARKS_URI,
                        new String[] { BookmarkColumns.URL },
                        Browser.BookmarkColumns.URL + " = ? and " + Browser.BookmarkColumns.BOOKMARK + " = ?",
                        new String[] { uri, "1" },
                        Browser.BookmarkColumns.URL);
    }

    public Cursor isBookmarkQueryPost11(ContentResolver cr, String uri) {
        return cr.query(BOOKMARKS_CONTENT_URI_POST_11,
                        new String[] { BookmarkColumns.URL },
                        Browser.BookmarkColumns.URL + " = ?",
                        new String[] { uri },
                        Browser.BookmarkColumns.URL);
    }

    public boolean isBookmark(ContentResolver cr, String uri) {
        Cursor cursor;

        if (Build.VERSION.SDK_INT >= 11)
            cursor = isBookmarkQueryPost11(cr, uri);
        else
            cursor = isBookmarkQueryPre11(cr, uri);

        int count = cursor.getCount();
        cursor.close();

        return (count == 1);
    }

    public String getUrlForKeyword(ContentResolver cr, String keyword) {
        return null;
    }

    public void addBookmarkPre11(ContentResolver cr, String title, String uri) {
        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.BOOKMARK, "1");
        values.put(Browser.BookmarkColumns.TITLE, title);
        values.put(Browser.BookmarkColumns.URL, uri);

        int updated = cr.update(Browser.BOOKMARKS_URI,
                                values,
                                Browser.BookmarkColumns.URL + " = ?",
                                new String[] { uri });

        if (updated == 0)
            cr.insert(Browser.BOOKMARKS_URI, values);
    }

    public void addBookmarkPost11(ContentResolver cr, String title, String uri) {
        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.TITLE, title);
        values.put(Browser.BookmarkColumns.URL, uri);
        values.put(URL_COLUMN_DELETED, "0");

        int updated = cr.update(BOOKMARKS_CONTENT_URI_POST_11,
                                values,
                                Browser.BookmarkColumns.URL + " = ?",
                                new String[] { uri });

        if (updated == 0)
            cr.insert(BOOKMARKS_CONTENT_URI_POST_11, values);
    }

    public void addBookmark(ContentResolver cr, String title, String uri) {
        if (Build.VERSION.SDK_INT >= 11)
            addBookmarkPost11(cr, title, uri);
        else
            addBookmarkPre11(cr, title, uri);
    }

    public void updateBookmarkPre11(ContentResolver cr, String oldUri, String uri, String title) {
        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.TITLE, title);
        values.put(Browser.BookmarkColumns.URL, uri);

        cr.update(Browser.BOOKMARKS_URI,
                  values,
                  Browser.BookmarkColumns.URL + " = ?",
                  new String[] { oldUri });
    }

    public void updateBookmarkPost11(ContentResolver cr, String oldUri, String uri, String title) {
        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.TITLE, title);
        values.put(Browser.BookmarkColumns.URL, uri);

        cr.update(BOOKMARKS_CONTENT_URI_POST_11,
                  values,
                  Browser.BookmarkColumns.URL + " = ?",
                  new String[] { oldUri });
    }

    public void updateBookmark(ContentResolver cr, String oldUri, String uri, String title, String keyword) {
        if (Build.VERSION.SDK_INT >= 11)
            updateBookmarkPost11(cr, oldUri, uri, title);
        else
            updateBookmarkPre11(cr, oldUri, uri, title);
    }

    public void removeBookmarkPre11(ContentResolver cr, String uri) {
        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.BOOKMARK, "0");

        cr.update(Browser.BOOKMARKS_URI,
                  values,
                  Browser.BookmarkColumns.URL + " = ?",
                  new String[] { uri });
    }

    public void removeBookmarkPost11(ContentResolver cr, String uri) {
        cr.delete(BOOKMARKS_CONTENT_URI_POST_11,
                  Browser.BookmarkColumns.URL + " = ?",
                  new String[] { uri });
    }

    public void removeBookmark(ContentResolver cr, int id) {
        // Not implemented
    }

    public void removeBookmarksWithURL(ContentResolver cr, String uri) {
        if (Build.VERSION.SDK_INT >= 11)
            removeBookmarkPost11(cr, uri);
        else
            removeBookmarkPre11(cr, uri);
    }

    public void registerBookmarkObserverPre11(ContentResolver cr, ContentObserver observer) {
        cr.registerContentObserver(Browser.BOOKMARKS_URI, false, observer);
    }

    public void registerBookmarkObserverPost11(ContentResolver cr, ContentObserver observer) {
        cr.registerContentObserver(BOOKMARKS_CONTENT_URI_POST_11, false, observer);
    }

    public void registerBookmarkObserver(ContentResolver cr, ContentObserver observer) {
        if (Build.VERSION.SDK_INT >= 11)
            registerBookmarkObserverPost11(cr, observer);
        else
            registerBookmarkObserverPre11(cr, observer);
    }

    public BitmapDrawable getFaviconForUrl(ContentResolver cr, String uri) {
        Cursor c = cr.query(Browser.BOOKMARKS_URI,
                            new String[] { Browser.BookmarkColumns.FAVICON },
                            Browser.BookmarkColumns.URL + " = ?",
                            new String[] { uri },
                            null);

        if (!c.moveToFirst()) {
            c.close();
            return null;
        }

        int faviconIndex = c.getColumnIndexOrThrow(Browser.BookmarkColumns.FAVICON);

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
        values.put(Browser.BookmarkColumns.FAVICON, stream.toByteArray());
        values.put(Browser.BookmarkColumns.URL, uri);

        int updated = cr.update(Browser.BOOKMARKS_URI,
                                values,
                                Browser.BookmarkColumns.URL + " = ?",
                                new String[] { uri });

        if (updated == 0) {
            try {
                cr.insert(Browser.BOOKMARKS_URI, values);
            } catch (SQLiteConstraintException e) {
                // insert() mysteriously and intermittently fails with "error
                // code 19: constraint failed" on some Honeycomb and ICS
                // devices. Bookmark favicons are not a critical feature, so
                // we can ignore this error for now. bug 711977; bug 712791
                Log.e(LOGTAG,
                      String.format("Inserting favicon for \"%s\" failed with SQLiteConstraintException: %s",
                                    uri, e.getMessage()));
            }
        }
    }

    public void updateThumbnailForUrl(ContentResolver cr, String uri,
            BitmapDrawable thumbnail) {
        Bitmap bitmap = thumbnail.getBitmap();

        ContentValues values = new ContentValues();
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 0, bos);

        values.put(URL_COLUMN_THUMBNAIL, bos.toByteArray());
        values.put(Browser.BookmarkColumns.URL, uri);

        int updated = cr.update(Browser.BOOKMARKS_URI,
                                values,
                                Browser.BookmarkColumns.URL + " = ?",
                                new String[] { uri });

        if (updated == 0)
            cr.insert(Browser.BOOKMARKS_URI, values);
    }

    public byte[] getThumbnailForUrl(ContentResolver cr, String uri) {
        Cursor c = cr.query(Browser.BOOKMARKS_URI,
                            new String[] { URL_COLUMN_THUMBNAIL },
                            Browser.BookmarkColumns.URL + " = ?",
                            new String[] { uri },
                            null);

        if (!c.moveToFirst()) {
            c.close();
            return null;
        }

        int thumbnailIndex = c.getColumnIndexOrThrow(URL_COLUMN_THUMBNAIL);

        byte[] b = c.getBlob(thumbnailIndex);
        c.close();

        return b;
    }

    private static class AndroidDBCursor extends CursorWrapper {
        public AndroidDBCursor(Cursor c) {
            super(c);
        }

        private String translateColumnName(String columnName) {
            if (columnName.equals(BrowserDB.URLColumns.URL)) {
                columnName = Browser.BookmarkColumns.URL;
            } else if (columnName.equals(BrowserDB.URLColumns.TITLE)) {
                columnName = Browser.BookmarkColumns.TITLE;
            } else if (columnName.equals(BrowserDB.URLColumns.FAVICON)) {
                columnName = Browser.BookmarkColumns.FAVICON;
            } else if (columnName.equals(BrowserDB.URLColumns.THUMBNAIL)) {
                columnName = URL_COLUMN_THUMBNAIL;
            } else if (columnName.equals(BrowserDB.URLColumns.DATE_LAST_VISITED)) {
                columnName = Browser.BookmarkColumns.DATE;
            } else if (columnName.equals(BrowserDB.URLColumns.VISITS)) {
                columnName = Browser.BookmarkColumns.VISITS;
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
