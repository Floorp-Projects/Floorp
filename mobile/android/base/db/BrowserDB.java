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

import android.content.ContentResolver;
import android.database.Cursor;
import android.graphics.drawable.BitmapDrawable;

public class BrowserDB {
    public static interface URLColumns {
        public static String URL = "url";
        public static String TITLE = "title";
        public static String FAVICON = "favicon";
        public static String THUMBNAIL = "thumbnail";
        public static String DATE_LAST_VISITED = "date-last-visited";
    }

    private static BrowserDBIface sDb;

    public interface BrowserDBIface {
        public Cursor filter(ContentResolver cr, CharSequence constraint, int limit, CharSequence urlFilter);

        public void updateVisitedHistory(ContentResolver cr, String uri);

        public void updateHistoryTitle(ContentResolver cr, String uri, String title);

        public void updateHistoryDate(ContentResolver cr, String uri, long date);

        public Cursor getAllVisitedHistory(ContentResolver cr);

        public Cursor getRecentHistory(ContentResolver cr, int limit);

        public void clearHistory(ContentResolver cr);

        public Cursor getAllBookmarks(ContentResolver cr);

        public boolean isBookmark(ContentResolver cr, String uri);

        public void addBookmark(ContentResolver cr, String title, String uri);

        public void removeBookmark(ContentResolver cr, String uri);

        public BitmapDrawable getFaviconForUrl(ContentResolver cr, String uri);

        public void updateFaviconForUrl(ContentResolver cr, String uri, BitmapDrawable favicon);

        public void updateThumbnailForUrl(ContentResolver cr, String uri, BitmapDrawable thumbnail);
    }

    static {
        // FIXME: Still need to figure out how to use local or android
        // database here.
        sDb = new AndroidBrowserDB();
    }

    public static Cursor filter(ContentResolver cr, CharSequence constraint, int limit, CharSequence urlFilter) {
        return sDb.filter(cr, constraint, limit, urlFilter);
    }

    public static Cursor filter(ContentResolver cr, CharSequence constraint, int limit) {
        return sDb.filter(cr, constraint, limit, null);
    }

    public static void updateVisitedHistory(ContentResolver cr, String uri) {
        sDb.updateVisitedHistory(cr, uri);
    }

    public static void updateHistoryTitle(ContentResolver cr, String uri, String title) {
        sDb.updateHistoryTitle(cr, uri, title);
    }

    public static void updateHistoryDate(ContentResolver cr, String uri, long date) {
        sDb.updateHistoryDate(cr, uri, date);
    }

    public static Cursor getAllVisitedHistory(ContentResolver cr) {
        return sDb.getAllVisitedHistory(cr);
    }

    public static Cursor getRecentHistory(ContentResolver cr, int limit) {
        return sDb.getRecentHistory(cr, limit);
    }

    public static void clearHistory(ContentResolver cr) {
        sDb.clearHistory(cr);
    }

    public static Cursor getAllBookmarks(ContentResolver cr) {
        return sDb.getAllBookmarks(cr);
    }

    public static boolean isBookmark(ContentResolver cr, String uri) {
        return sDb.isBookmark(cr, uri);
    }

    public static void addBookmark(ContentResolver cr, String title, String uri) {
        sDb.addBookmark(cr, title, uri);
    }

    public static void removeBookmark(ContentResolver cr, String uri) {
        sDb.removeBookmark(cr, uri);
    }

    public static BitmapDrawable getFaviconForUrl(ContentResolver cr, String uri) {
        return sDb.getFaviconForUrl(cr, uri);
    }

    public static void updateFaviconForUrl(ContentResolver cr, String uri, BitmapDrawable favicon) {
        sDb.updateFaviconForUrl(cr, uri, favicon);
    }

    public static void updateThumbnailForUrl(ContentResolver cr, String uri, BitmapDrawable thumbnail) {
        sDb.updateThumbnailForUrl(cr, uri, thumbnail);
    }
}
