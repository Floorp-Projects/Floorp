/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.distribution;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;

import org.mozilla.gecko.db.BrowserContract;

/**
 * Client for reading Android's PartnerBookmarksProvider.
 *
 * Note: This client is only invoked for distributions. Without a distribution the content provider
 *       will not be read and no bookmarks will be added to the UI.
 */
public class PartnerBookmarksProviderClient {
    /**
     * The contract between the partner bookmarks provider and applications. Contains the definition
     * for the supported URIs and columns.
     */
    private static class PartnerContract {
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

    public static Cursor getBookmarksInFolder(ContentResolver contentResolver, int folderId) {
        // Use root folder id or transform negative id into actual (positive) folder id.
        final long actualFolderId = folderId == BrowserContract.Bookmarks.FIXED_ROOT_ID
                ? PartnerContract.PARENT_ROOT_ID
                : BrowserContract.Bookmarks.FAKE_PARTNER_BOOKMARKS_START - folderId;

        return contentResolver.query(
                PartnerContract.CONTENT_URI,
                new String[] {
                        // Transform ids into negative values starting with FAKE_PARTNER_BOOKMARKS_START.
                        "(" + BrowserContract.Bookmarks.FAKE_PARTNER_BOOKMARKS_START + " - " + PartnerContract.ID + ") as " + BrowserContract.Bookmarks._ID,
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
                        // Only select entries with valid type
                        + " AND (" + BrowserContract.Bookmarks.TYPE + " = 1 OR " + BrowserContract.Bookmarks.TYPE + " = 2)"
                        // Only select entries with non empty title
                        + " AND " + BrowserContract.Bookmarks.TITLE + " <> ''",
                new String[] { String.valueOf(actualFolderId) },
                // Same order we use in our content provider (without position)
                BrowserContract.Bookmarks.TYPE + " ASC, " + BrowserContract.Bookmarks._ID + " ASC");
    }
}
