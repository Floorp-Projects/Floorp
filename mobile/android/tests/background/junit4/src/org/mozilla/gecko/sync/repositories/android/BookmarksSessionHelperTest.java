/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.android;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class BookmarksSessionHelperTest {
    /**
     * Trivial test that forbidden records such as pinned items
     * will be ignored if processed.
     */
    @Test
    public void testShouldIgnore() throws Exception {
        final long now = System.currentTimeMillis();
        final String bookmarksCollection = "bookmarks";

        final BookmarkRecord pinned = new BookmarkRecord("pinpinpinpin", "bookmarks", now - 1, false);
        final BookmarkRecord normal = new BookmarkRecord("baaaaaaaaaaa", "bookmarks", now - 2, false);

        final BookmarkRecord pinnedItems  = new BookmarkRecord(BrowserContract.Bookmarks.PINNED_FOLDER_GUID,
                bookmarksCollection, now - 4, false);

        normal.type = "bookmark";
        pinned.type = "bookmark";
        pinnedItems.type = "folder";

        pinned.parentID = BrowserContract.Bookmarks.PINNED_FOLDER_GUID;
        normal.parentID = BrowserContract.Bookmarks.TOOLBAR_FOLDER_GUID;

        pinnedItems.parentID = BrowserContract.Bookmarks.PLACES_FOLDER_GUID;

        assertTrue(BookmarksSessionHelper.shouldIgnoreStatic(pinned));
        assertTrue(BookmarksSessionHelper.shouldIgnoreStatic(pinnedItems));
        assertFalse(BookmarksSessionHelper.shouldIgnoreStatic(normal));
    }
}