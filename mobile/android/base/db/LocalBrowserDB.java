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
import java.util.Date;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.History;
import org.mozilla.gecko.db.BrowserContract.ImageColumns;
import org.mozilla.gecko.db.BrowserContract.Images;
import org.mozilla.gecko.db.BrowserContract.URLColumns;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.CursorWrapper;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.provider.Browser;

public class LocalBrowserDB implements BrowserDB.BrowserDBIface {
    // Same as android.provider.Browser for consistency
    private static final int MAX_HISTORY_COUNT = 250;

    // Same as android.provider.Browser for consistency
    public static final int TRUNCATE_N_OLDEST = 5;

    private final String mProfile;
    private long mMobileFolderId;

    public LocalBrowserDB(String profile) {
        mProfile = profile;
        mMobileFolderId = -1;
    }

    private Uri appendProfileAndLimit(Uri uri, int limit) {
        return uri.buildUpon().appendQueryParameter(BrowserContract.PARAM_PROFILE, mProfile).
                appendQueryParameter(BrowserContract.PARAM_LIMIT, String.valueOf(limit)).build();
    }

    private Uri appendProfile(Uri uri) {
        return uri.buildUpon().appendQueryParameter(BrowserContract.PARAM_PROFILE, mProfile).build();
    }

    public Cursor filter(ContentResolver cr, CharSequence constraint, int limit) {
        Cursor c = cr.query(appendProfileAndLimit(History.CONTENT_URI, limit),
                            new String[] { History._ID,
                                           History.URL,
                                           History.TITLE,
                                           History.FAVICON,
                                           History.THUMBNAIL },
                            "(" + History.URL + " LIKE ? OR " + History.TITLE + " LIKE ?)",
                            new String[] {"%" + constraint.toString() + "%", "%" + constraint.toString() + "%"},
                            // ORDER BY is number of visits times a multiplier from 1 - 120 of how recently the site
                            // was accessed with a site accessed today getting 120 and a site accessed 119 or more
                            // days ago getting 1
                            History.VISITS + " * MAX(1, (" +
                            History.DATE_LAST_VISITED + " - " + new Date().getTime() + ") / 86400000 + 120) DESC");

        return new LocalDBCursor(c);
    }

    private void truncateHistory(ContentResolver cr) {
        Cursor cursor = null;

        try {
            cursor = cr.query(appendProfile(History.CONTENT_URI),
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

            cursor = cr.query(appendProfile(History.CONTENT_URI),
                              projection,
                              History.URL + " = ?",
                              new String[] { uri },
                              null);

            if (cursor.moveToFirst()) {
                ContentValues values = new ContentValues();

                values.put(History.VISITS, cursor.getInt(1) + 1);
                values.put(History.DATE_LAST_VISITED, now);

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

                cr.insert(appendProfile(History.CONTENT_URI), values);
            }
        } finally {
            if (cursor != null)
                cursor.close();
        }
    }

    public void updateHistoryTitle(ContentResolver cr, String uri, String title) {
        ContentValues values = new ContentValues();
        values.put(History.TITLE, title);

        cr.update(appendProfile(History.CONTENT_URI),
                  values,
                  History.URL + " = ?",
                  new String[] { uri });
    }

    public Cursor getAllVisitedHistory(ContentResolver cr) {
        Cursor c = cr.query(appendProfile(History.CONTENT_URI),
                            new String[] { History.URL },
                            History.VISITS + " > 0",
                            null,
                            null);

        return new LocalDBCursor(c);
    }

    public Cursor getRecentHistory(ContentResolver cr, int limit) {
        Cursor c = cr.query(appendProfileAndLimit(History.CONTENT_URI, limit),
                            new String[] { History._ID,
                                           History.URL,
                                           History.TITLE,
                                           History.FAVICON,
                                           History.DATE_LAST_VISITED },
                            History.DATE_LAST_VISITED + " > 0",
                            null,
                            History.DATE_LAST_VISITED + " DESC");

        return new LocalDBCursor(c);
    }

    public void clearHistory(ContentResolver cr) {
        cr.delete(appendProfile(History.CONTENT_URI), null, null);
    }

    public Cursor getAllBookmarks(ContentResolver cr) {
        Cursor c = cr.query(appendProfile(Bookmarks.CONTENT_URI),
                            new String[] { Bookmarks._ID,
                                           Bookmarks.URL,
                                           Bookmarks.TITLE,
                                           Bookmarks.FAVICON },
                            Bookmarks.IS_FOLDER + " = 0",
                            null,
                            Bookmarks.TITLE + " ASC");

        return new LocalDBCursor(c);
    }

    public boolean isBookmark(ContentResolver cr, String uri) {
        Cursor cursor = cr.query(appendProfile(Bookmarks.CONTENT_URI),
                                 new String[] { Bookmarks._ID },
                                 Bookmarks.URL + " = ?",
                                 new String[] { uri },
                                 Bookmarks.URL);

        int count = cursor.getCount();
        cursor.close();

        return (count == 1);
    }

    private long getMobileBookmarksFolderId(ContentResolver cr) {
        if (mMobileFolderId >= 0)
            return mMobileFolderId;

        Cursor c = null;

        try {
            c = cr.query(appendProfile(Bookmarks.CONTENT_URI),
                         new String[] { Bookmarks._ID },
                         Bookmarks.GUID + " = ?",
                         new String[] { Bookmarks.MOBILE_FOLDER_GUID },
                         null);

            if (c.moveToFirst())
                mMobileFolderId = c.getLong(c.getColumnIndexOrThrow(Bookmarks._ID));
        } finally {
            if (c != null)
                c.close();
        }

        return mMobileFolderId;
    }

    public void addBookmark(ContentResolver cr, String title, String uri) {
        long folderId = getMobileBookmarksFolderId(cr);
        if (folderId < 0)
            return;

        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.TITLE, title);
        values.put(Bookmarks.URL, uri);
        values.put(Bookmarks.PARENT, folderId);

        // Restore deleted record if possible
        values.put(Bookmarks.IS_DELETED, 0);

        int updated = cr.update(appendProfile(Bookmarks.CONTENT_URI),
                                values,
                                Bookmarks.URL + " = ?",
                                new String[] { uri });

        if (updated == 0)
            cr.insert(appendProfile(Bookmarks.CONTENT_URI), values);
    }

    public void removeBookmark(ContentResolver cr, String uri) {
        cr.delete(appendProfile(Bookmarks.CONTENT_URI),
                  Bookmarks.URL + " = ?",
                  new String[] { uri });
    }

    public BitmapDrawable getFaviconForUrl(ContentResolver cr, String uri) {
        Cursor c = cr.query(appendProfile(Images.CONTENT_URI),
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

        int updated = cr.update(appendProfile(Images.CONTENT_URI),
                                values,
                                Images.URL + " = ?",
                                new String[] { uri });

        if (updated == 0)
            cr.insert(appendProfile(Images.CONTENT_URI), values);
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

        int updated = cr.update(appendProfile(Images.CONTENT_URI),
                                values,
                                Images.URL + " = ?",
                                new String[] { uri });

        if (updated == 0)
            cr.insert(appendProfile(Images.CONTENT_URI), values);
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
