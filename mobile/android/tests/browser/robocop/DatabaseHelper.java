/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.util.ArrayList;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Assert;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.db.BrowserDB;

import android.app.Activity;
import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;

class DatabaseHelper {
    protected enum BrowserDataType {BOOKMARKS, HISTORY};
    private final Activity mActivity;
    private final Assert mAsserter;

    public DatabaseHelper(Activity activity, Assert asserter) {
        mActivity = activity;
        mAsserter = asserter;
    }
    /**
    * This method can be used to check if an URL is present in the bookmarks database
    */
    protected boolean isBookmark(String url) {
        final ContentResolver resolver = mActivity.getContentResolver();
        return getProfileDB().isBookmark(resolver, url);
    }

    protected Uri buildUri(BrowserDataType dataType) {
        Uri uri = null;
        if (dataType == BrowserDataType.BOOKMARKS || dataType == BrowserDataType.HISTORY) {
            uri = Uri.parse("content://" + AppConstants.ANDROID_PACKAGE_NAME + ".db.browser/" + dataType.toString().toLowerCase());
        } else {
           mAsserter.ok(false, "The wrong data type has been provided = " + dataType.toString(), "Please provide the correct data type");
        }
        uri = uri.buildUpon().appendQueryParameter("profile", GeckoProfile.DEFAULT_PROFILE)
                             .appendQueryParameter("sync", "true").build();
        return uri;
    }

    /**
     * Adds a bookmark.
     */
    protected void addMobileBookmark(String title, String url) {
        final ContentResolver resolver = mActivity.getContentResolver();
        getProfileDB().addBookmark(resolver, title, url);
        mAsserter.ok(true, "Inserting a new bookmark", "Inserting the bookmark with the title = " + title + " and the url = " + url);
    }

    /**
     * Updates the title and keyword of a bookmark with the given URL.
     *
     * Warning: This method assumes that there's only one bookmark with the given URL.
     */
    protected void updateBookmark(String url, String title, String keyword) {
        final ContentResolver resolver = mActivity.getContentResolver();
        // Get the id for the bookmark with the given URL.
        Cursor c = null;
        try {
            c = getProfileDB().getBookmarkForUrl(resolver, url);
            if (!c.moveToFirst()) {
                mAsserter.ok(false, "Getting bookmark with url", "Couldn't find bookmark with url = " + url);
                return;
            }

            int id = c.getInt(c.getColumnIndexOrThrow("_id"));
            getProfileDB().updateBookmark(resolver, id, url, title, keyword);

            mAsserter.ok(true, "Updating bookmark", "Updating bookmark with url = " + url);
        } finally {
            if (c != null) {
                c.close();
            }
        }
    }

    protected void deleteBookmark(String url) {
        final ContentResolver resolver = mActivity.getContentResolver();
        getProfileDB().removeBookmarksWithURL(resolver, url);
    }

    protected void deleteHistoryItem(String url) {
        final ContentResolver resolver = mActivity.getContentResolver();
        getProfileDB().removeHistoryEntry(resolver, url);
    }

    // About the same implementation as getFolderIdFromGuid from LocalBrowserDB because it is declared private and we can't use reflections to access it
    protected long getFolderIdFromGuid(String guid) {
        final ContentResolver resolver = mActivity.getContentResolver();
        long folderId = -1L;
        final Uri bookmarksUri = buildUri(BrowserDataType.BOOKMARKS);

        Cursor c = null;
        try {
            c = resolver.query(bookmarksUri,
                               new String[] { "_id" },
                               "guid = ?",
                               new String[] { guid },
                               null);
            if (c.moveToFirst()) {
                folderId = c.getLong(c.getColumnIndexOrThrow("_id"));
            }

            if (folderId == -1) {
                mAsserter.ok(false, "Trying to get the folder id" ,"We did not get the correct folder id");
            }
        } finally {
            if (c != null) {
                c.close();
            }
        }
        return folderId;
    }

    /**
     * Returns all of the bookmarks or history entries in a database.
     *
     * @return an ArrayList of the urls in the Firefox for Android Bookmarks or History databases.
     */
    protected ArrayList<String> getBrowserDBUrls(BrowserDataType dataType) {
        final ArrayList<String> browserData = new ArrayList<String>();
        final ContentResolver resolver = mActivity.getContentResolver();

        Cursor cursor = null;
        final BrowserDB db = getProfileDB();
        if (dataType == BrowserDataType.HISTORY) {
            cursor = db.getAllVisitedHistory(resolver);
        } else if (dataType == BrowserDataType.BOOKMARKS) {
            cursor = db.getBookmarksInFolder(resolver, getFolderIdFromGuid("mobile"));
        }

        if (cursor == null) {
            mAsserter.ok(false, "We could not retrieve any data from the database", "The cursor was null");
            return browserData;
        }

        try {
            if (!cursor.moveToFirst()) {
                // Nothing here, but that's OK -- maybe there are zero results. The calling test will fail.
                return browserData;
            }

            do {
                // The URL field may be null for folders in the structure of the Bookmarks table for Firefox. Eliminate those.
                if (cursor.getString(cursor.getColumnIndex("url")) != null) {
                    browserData.add(cursor.getString(cursor.getColumnIndex("url")));
                }
            } while (cursor.moveToNext());

            return browserData;
        } finally {
            cursor.close();
        }
    }

    protected BrowserDB getProfileDB() {
        return GeckoProfile.get(mActivity).getDB();
    }
}
