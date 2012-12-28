/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.db.BrowserContract.ExpirePriority;

import android.content.ContentResolver;
import android.database.ContentObserver;
import android.database.Cursor;
import android.database.CursorWrapper;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

import java.util.List;

public class BrowserDB {
    public static String ABOUT_PAGES_URL_FILTER = "about:%";

    public static interface URLColumns {
        public static String URL = "url";
        public static String TITLE = "title";
        public static String FAVICON = "favicon";
        public static String THUMBNAIL = "thumbnail";
        public static String DATE_LAST_VISITED = "date-last-visited";
        public static String VISITS = "visits";
        public static String KEYWORD = "keyword";
    }

    private static BrowserDBIface sDb = null;

    public interface BrowserDBIface {
        public void invalidateCachedState();

        public Cursor filter(ContentResolver cr, CharSequence constraint, int limit);

        public Cursor getTopSites(ContentResolver cr, int limit);

        public void updateVisitedHistory(ContentResolver cr, String uri);

        public void updateHistoryTitle(ContentResolver cr, String uri, String title);

        public void updateHistoryEntry(ContentResolver cr, String uri, String title,
                                       long date, int visits);

        public Cursor getAllVisitedHistory(ContentResolver cr);

        public Cursor getRecentHistory(ContentResolver cr, int limit);

        public void expireHistory(ContentResolver cr, ExpirePriority priority);

        public void removeHistoryEntry(ContentResolver cr, int id);

        public void clearHistory(ContentResolver cr);

        public Cursor getBookmarksInFolder(ContentResolver cr, long folderId);

        public boolean isBookmark(ContentResolver cr, String uri);

        public boolean isReadingListItem(ContentResolver cr, String uri);

        public String getUrlForKeyword(ContentResolver cr, String keyword);

        public void addBookmark(ContentResolver cr, String title, String uri);

        public void removeBookmark(ContentResolver cr, int id);

        public void removeBookmarksWithURL(ContentResolver cr, String uri);

        public void updateBookmark(ContentResolver cr, int id, String uri, String title, String keyword);

        public void addReadingListItem(ContentResolver cr, String title, String uri);

        public void removeReadingListItemWithURL(ContentResolver cr, String uri);

        public Bitmap getFaviconForUrl(ContentResolver cr, String uri);

        public Cursor getFaviconsForUrls(ContentResolver cr, List<String> urls);

        public String getFaviconUrlForHistoryUrl(ContentResolver cr, String url);

        public void updateFaviconForUrl(ContentResolver cr, String pageUri, Bitmap favicon, String faviconUri);

        public void updateThumbnailForUrl(ContentResolver cr, String uri, BitmapDrawable thumbnail);

        public byte[] getThumbnailForUrl(ContentResolver cr, String uri);

        public Cursor getThumbnailsForUrls(ContentResolver cr, List<String> urls);

        public void removeThumbnails(ContentResolver cr);

        public void registerBookmarkObserver(ContentResolver cr, ContentObserver observer);

        public void registerHistoryObserver(ContentResolver cr, ContentObserver observer);

        public int getCount(ContentResolver cr, String database);

        public void pinSite(ContentResolver cr, String url, String title, int position);

        public void unpinSite(ContentResolver cr, int position);

        public void unpinAllSites(ContentResolver cr);

        public Cursor getPinnedSites(ContentResolver cr, int limit);
    }

    static {
        // Forcing local DB no option to switch to Android DB for now
        sDb = null;
    }

    public static void initialize(String profile) {
        sDb = new LocalBrowserDB(profile);
    }

    public static void invalidateCachedState() {
        sDb.invalidateCachedState();
    }

    public static Cursor filter(ContentResolver cr, CharSequence constraint, int limit) {
        return sDb.filter(cr, constraint, limit);
    }

    public static Cursor getTopSites(ContentResolver cr, int limit) {
        return new TopSitesCursorWrapper(sDb.getTopSites(cr, limit), limit);
    }

    public static void updateVisitedHistory(ContentResolver cr, String uri) {
        sDb.updateVisitedHistory(cr, uri);
    }

    public static void updateHistoryTitle(ContentResolver cr, String uri, String title) {
        sDb.updateHistoryTitle(cr, uri, title);
    }

    public static void updateHistoryEntry(ContentResolver cr, String uri, String title,
                                          long date, int visits) {
        sDb.updateHistoryEntry(cr, uri, title, date, visits);
    }

    public static Cursor getAllVisitedHistory(ContentResolver cr) {
        return sDb.getAllVisitedHistory(cr);
    }

    public static Cursor getRecentHistory(ContentResolver cr, int limit) {
        return sDb.getRecentHistory(cr, limit);
    }

    public static void expireHistory(ContentResolver cr, ExpirePriority priority) {
        if (sDb == null)
            return;

        if (priority == null)
            priority = ExpirePriority.NORMAL;
        sDb.expireHistory(cr, priority);
    }

    public static void removeHistoryEntry(ContentResolver cr, int id) {
        sDb.removeHistoryEntry(cr, id);
    }

    public static void clearHistory(ContentResolver cr) {
        sDb.clearHistory(cr);
    }

    public static Cursor getBookmarksInFolder(ContentResolver cr, long folderId) {
        return sDb.getBookmarksInFolder(cr, folderId);
    }

    public static String getUrlForKeyword(ContentResolver cr, String keyword) {
        return sDb.getUrlForKeyword(cr, keyword);
    }
    
    public static boolean isBookmark(ContentResolver cr, String uri) {
        return sDb.isBookmark(cr, uri);
    }

    public static boolean isReadingListItem(ContentResolver cr, String uri) {
        return sDb.isReadingListItem(cr, uri);
    }

    public static void addBookmark(ContentResolver cr, String title, String uri) {
        sDb.addBookmark(cr, title, uri);
    }

    public static void removeBookmark(ContentResolver cr, int id) {
        sDb.removeBookmark(cr, id);
    }

    public static void removeBookmarksWithURL(ContentResolver cr, String uri) {
        sDb.removeBookmarksWithURL(cr, uri);
    }

    public static void updateBookmark(ContentResolver cr, int id, String uri, String title, String keyword) {
        sDb.updateBookmark(cr, id, uri, title, keyword);
    }

    public static void addReadingListItem(ContentResolver cr, String title, String uri) {
        sDb.addReadingListItem(cr, title, uri);
    }

    public static void removeReadingListItemWithURL(ContentResolver cr, String uri) {
        sDb.removeReadingListItemWithURL(cr, uri);
    }

    public static Bitmap getFaviconForUrl(ContentResolver cr, String uri) {
        return sDb.getFaviconForUrl(cr, uri);
    }

    public static Cursor getFaviconsForUrls(ContentResolver cr, List<String> urls) {
        return sDb.getFaviconsForUrls(cr, urls);
    }

    public static String getFaviconUrlForHistoryUrl(ContentResolver cr, String url) {
        return sDb.getFaviconUrlForHistoryUrl(cr, url);
    }

    public static void updateFaviconForUrl(ContentResolver cr, String pageUri, Bitmap favicon, String faviconUri) {
        sDb.updateFaviconForUrl(cr, pageUri, favicon, faviconUri);
    }

    public static void updateThumbnailForUrl(ContentResolver cr, String uri, BitmapDrawable thumbnail) {
        sDb.updateThumbnailForUrl(cr, uri, thumbnail);
    }

    public static byte[] getThumbnailForUrl(ContentResolver cr, String uri) {
        return sDb.getThumbnailForUrl(cr, uri);
    }

    public static Cursor getThumbnailsForUrls(ContentResolver cr, List<String> urls) {
        return sDb.getThumbnailsForUrls(cr, urls);
    }

    public static void removeThumbnails(ContentResolver cr) {
        sDb.removeThumbnails(cr);
    }

    public static void registerBookmarkObserver(ContentResolver cr, ContentObserver observer) {
        sDb.registerBookmarkObserver(cr, observer);
    }

    public static void registerHistoryObserver(ContentResolver cr, ContentObserver observer) {
        sDb.registerHistoryObserver(cr, observer);
    }

    public static void unregisterContentObserver(ContentResolver cr, ContentObserver observer) {
        cr.unregisterContentObserver(observer);
    }

    public static int getCount(ContentResolver cr, String database) {
        return sDb.getCount(cr, database);
    }

    public static void pinSite(ContentResolver cr, String url, String title, int position) {
        sDb.pinSite(cr, url, title, position);
    }

    public static void unpinSite(ContentResolver cr, int position) {
        sDb.unpinSite(cr, position);
    }

    public static void unpinAllSites(ContentResolver cr) {
        sDb.unpinAllSites(cr);
    }

    public static Cursor getPinnedSites(ContentResolver cr, int limit) {
        return sDb.getPinnedSites(cr, limit);
    }

    /* Cursor wrapper that forces top sites to contain at least
     * mNumberOfTopSites entries. For rows outside the wrapped cursor
     * will return empty strings and zero.
     */
    static public class TopSitesCursorWrapper extends CursorWrapper {
        int mIndex = -1;
        Cursor mCursor = null;
        int mSize = 0;
        
        public TopSitesCursorWrapper(Cursor cursor, int size) {
            super(cursor);
            mCursor = cursor;
            mSize = size;
        }

        public int getPosition() { return mIndex; }
        public int getCount() { return mSize; }
        public boolean isAfterLast() { return mIndex >= mSize; }
        public boolean isBeforeFirst() { return mIndex < 0; }
        public boolean isLast() { return mIndex == mSize - 1; }
        public boolean moveToNext() { return moveToPosition(mIndex + 1); }
        public boolean moveToPrevious() { return moveToPosition(mIndex - 1); }

        public boolean moveToPosition(int position) {
            mIndex = position;
            if (position > -1 && position < mCursor.getCount())
                super.moveToPosition(position);
            return !(isBeforeFirst() || isAfterLast());
        }

        public long getLong(int columnIndex) {
            if (mIndex > -1 && mIndex < mCursor.getCount())
                return super.getLong(columnIndex);
            return 0;
        }

        public String getString(int columnIndex) {
            if (mIndex > -1 && mIndex < mCursor.getCount())
                return super.getString(columnIndex);
            return "";
        }

        public boolean move(int offset) {
            return moveToPosition(mIndex + offset);
        }

        public boolean moveToFirst() {
            return moveToPosition(0);
        }

        public boolean moveToLast() {
            return moveToPosition(mSize-1);
        }
    }
}
