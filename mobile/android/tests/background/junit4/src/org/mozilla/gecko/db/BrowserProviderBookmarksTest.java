/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.db;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.db.DelegatingTestContentProvider;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.robolectric.shadows.ShadowContentResolver;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

/**
 * Testing direct interactions with bookmarks through BrowserProvider
 */
@RunWith(TestRunner.class)
public class BrowserProviderBookmarksTest {

    private static final long INVALID_ID = -1;
    private static final long INVALID_TIMESTAMP = -1;

    private ContentProviderClient bookmarksClient;
    private Uri bookmarksTestUri;
    private BrowserProvider provider;

    @Before
    public void setUp() throws Exception {
        provider = new BrowserProvider();
        provider.onCreate();
        ShadowContentResolver.registerProvider(BrowserContract.AUTHORITY, new DelegatingTestContentProvider(provider));

        ShadowContentResolver contentResolver = new ShadowContentResolver();
        bookmarksClient = contentResolver.acquireContentProviderClient(BrowserContractHelpers.BOOKMARKS_CONTENT_URI);

        bookmarksTestUri = testUri(BrowserContract.Bookmarks.CONTENT_URI);
    }

    @After
    public void tearDown() {
        bookmarksClient.release();
        provider.shutdown();
    }

    @Test
    public void testBookmarkChangedCountsOnUpdates() throws RemoteException {
        final long rootId = getBookmarkIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
        insertBookmark("bookmark-1", "https://www.mozilla-1.org", "guid-1",
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);

        ContentValues values = new ContentValues();
        values.put(BrowserContract.Bookmarks.TITLE, "bookmark-1-2");
        int changed = bookmarksClient.update(
                bookmarksTestUri, values, BrowserContract.Bookmarks.GUID + " = ?",
                new String[] { "guid-1" });

        // Only record itself is changed.
        assertEquals(1, changed);

        // Test that changing a parent of a record affects three records - record itself, new parent, old parent.
        insertBookmark("bookmark-1", null, "parent", rootId, BrowserContract.Bookmarks.TYPE_FOLDER);
        long parentId = getBookmarkIdFromGuid("parent");
        values = new ContentValues();
        values.put(BrowserContract.Bookmarks.PARENT, parentId);

        changed = bookmarksClient.update(
                bookmarksTestUri.buildUpon()
                        .appendQueryParameter(
                                BrowserContract.PARAM_OLD_BOOKMARK_PARENT,
                                String.valueOf(rootId)
                        ).build(),
                values,
                BrowserContract.Bookmarks.GUID + " = ?",
                new String[] { "guid-1" }
        );

        assertEquals(3, changed);
    }

    @Test
    public void testBookmarkChangedCountsOnDeletions() throws RemoteException {
        final long rootId = getBookmarkIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
        insertBookmark("bookmark-1", "https://www.mozilla-1.org", "guid-1",
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);

        int changed = bookmarksClient.delete(bookmarksTestUri,
                BrowserContract.Bookmarks.URL + " = ?",
                new String[] { "https://www.mozilla-1.org" });

        // Note that we also modify parent folder, and so changed counts must reflect that.
        assertEquals(2, changed);

        insertBookmark("bookmark-2", "https://www.mozilla-2.org", "guid-2",
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);
        insertBookmark("bookmark-3", "https://www.mozilla-3.org", "guid-3",
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);
        insertBookmark("bookmark-4", "https://www.mozilla-4.org", "guid-4",
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);

        changed = bookmarksClient.delete(bookmarksTestUri,
                BrowserContract.Bookmarks.PARENT + " = ?",
                new String[] { String.valueOf(rootId) });

        assertEquals(4, changed);

        // Test that we correctly count affected records during deletions of subtrees.
        final Uri parentUri = insertBookmark("parent", null, "parent", rootId,
                BrowserContract.Bookmarks.TYPE_FOLDER);
        final long parentId = Long.parseLong(parentUri.getLastPathSegment());

        insertBookmark("bookmark-5", "https://www.mozilla-2.org", "guid-5",
                parentId, BrowserContract.Bookmarks.TYPE_BOOKMARK);
        insertBookmark("bookmark-6", "https://www.mozilla-3.org", "guid-6",
                parentId, BrowserContract.Bookmarks.TYPE_BOOKMARK);
        // Directly under the root.
        insertBookmark("bookmark-7", "https://www.mozilla-4.org", "guid-7",
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);

        changed = bookmarksClient.delete(bookmarksTestUri,
                BrowserContract.Bookmarks.GUID + " = ?",
                new String[] { "parent" });

        // 4 = parent, its two children, mobile root.
        assertEquals(4, changed);
    }

    @Test
    public void testBookmarkFolderLastModifiedOnDeletion() throws RemoteException {
        final long rootId = getBookmarkIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);

        // root
        // -> sibling
        // -> parent -- timestamp must change
        // ---> child-1
        // ---> child-2 -- delete this one

        final Uri parentUri = insertBookmark("parent", null, "parent", rootId,
                                             BrowserContract.Bookmarks.TYPE_FOLDER);
        final Uri siblingUri = insertBookmark("sibling", null, "sibling", rootId,
                                              BrowserContract.Bookmarks.TYPE_FOLDER);

        final long parentId = Long.parseLong(parentUri.getLastPathSegment());
        final Uri child1Uri = insertBookmark("child-1", null, "child-1", parentId,
                                             BrowserContract.Bookmarks.TYPE_FOLDER);
        final Uri child2Uri = insertBookmark("child-2", null, "child-2", parentId,
                                             BrowserContract.Bookmarks.TYPE_FOLDER);

        final long parentLastModifiedBeforeDeletion = getLastModified(parentUri);
        final long siblingLastModifiedBeforeDeletion = getLastModified(siblingUri);

        final long child1LastModifiedBeforeDeletion = getLastModified(child1Uri);
        final long child2LastModifiedBeforeDeletion = getLastModified(child2Uri);

        bookmarksClient.delete(child2Uri, null, null);

        final long parentLastModifiedAfterDeletion = getLastModified(parentUri);
        final long siblingLastModifiedAfterDeletion = getLastModified(siblingUri);

        final long child1LastModifiedAfterDeletion = getLastModified(child1Uri);
        final long child2LastModifiedAfterDeletion = getLastModified(withDeleted(child2Uri));

        // Check last modified timestamp of parent and child-2 is increased.
        assertTrue(parentLastModifiedAfterDeletion > parentLastModifiedBeforeDeletion);
        assertTrue(child2LastModifiedAfterDeletion > child2LastModifiedBeforeDeletion);

        // Check last modified timestamp of sibling and child-1 is not changed.
        assertTrue(siblingLastModifiedBeforeDeletion == siblingLastModifiedAfterDeletion);
        assertTrue(child1LastModifiedBeforeDeletion == child1LastModifiedAfterDeletion);
    }

    @Test
    public void testDeleteBookmarkFolder() throws RemoteException {
        final long rootId = getBookmarkIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);

        // Create a folder.
        final Uri parentUri = insertBookmark("parent", null, "parent", rootId,
                                             BrowserContract.Bookmarks.TYPE_FOLDER);
        final long parentId = Long.parseLong(parentUri.getLastPathSegment());

        // Insert 10 bookmarks into the parent.
        for (int i = 0; i < 10; i++) {
            insertBookmark("bookmark-" + i, "https://www.mozilla-" + i + ".org", "guid-" + i,
                           parentId, BrowserContract.Bookmarks.TYPE_BOOKMARK);
        }

        final String selection = BrowserContract.Bookmarks._ID + " = ? OR " +
                                 BrowserContract.Bookmarks.PARENT + " = ?";
        final String[] selectionArgs = { String.valueOf(parentId),
                                         String.valueOf(parentId) };
        // Check insertions. We should have 11 records (1 parent and 10 children).
        final int inserted = getRowCount(selection, selectionArgs);
        assertEquals(11, inserted);

        final int changed = bookmarksClient.delete(bookmarksTestUri,
                                                   BrowserContract.Bookmarks._ID + " = ?",
                                                   new String[] { String.valueOf(parentId) });
        // Check if parent and all its descendants are deleted.
        // We also modify parent's parent, and so that is counted as well.
        assertEquals(12, changed);

        // Check records are marked as deleted and column are reset.
        final StringBuilder sb = new StringBuilder(BrowserContract.Bookmarks._ID + " = ? OR ");
        sb.append(BrowserContract.Bookmarks.GUID + " IN (");
        for (int i = 0; i < 10; i++) {
            if (i != 0) {
                sb.append(", ");
            }
            sb.append("'");
            sb.append("guid-").append(i);
            sb.append("'");
        }
        sb.append(")");
        final String where = sb.toString();

        final Uri withDeletedUri = withDeleted(bookmarksTestUri);
        final Cursor cursor = bookmarksClient.query(withDeletedUri, null,
                                                    where, new String[] { String.valueOf(parentId) }, null);
        assertNotNull(cursor);
        assertEquals(11, cursor.getCount());
        List<String> guids = Arrays.asList("parent", "guid-0", "guid-1", "guid-2", "guid-3", "guid-4", "guid-5", "guid-6", "guid-7", "guid-8", "guid-9");
        List<String> seenGuids = new ArrayList<>();
        try {
            while (cursor.moveToNext()) {
                final int isDeleted = cursor.getInt(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.IS_DELETED));
                assertEquals(1, isDeleted);

                final String guid = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.GUID));
                // Don't depend on iteration order while checking that every guid is correctly preserved.
                assertTrue(guids.contains(guid));
                assertFalse(seenGuids.contains(guid));
                seenGuids.add(guid);

                final int position = cursor.getInt(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.POSITION));
                assertEquals(0, position);

                final long parent = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.PARENT));
                assertEquals(0, parent);

                final String url = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.URL));
                assertNull(url);

                final String title = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TITLE));
                assertNull(title);

                final String description = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.DESCRIPTION));
                assertNull(description);

                final String keyword = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.KEYWORD));
                assertNull(keyword);

                final String tags = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TAGS));
                assertNull(tags);

                final int faviconId = cursor.getInt(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.FAVICON_ID));
                assertEquals(0, faviconId);
            }
        } finally {
            cursor.close();
        }
    }

    private Uri testUri(Uri baseUri) {
        return baseUri.buildUpon().appendQueryParameter(BrowserContract.PARAM_IS_TEST, "1").build();
    }

    private Uri withDeleted(Uri baseUri) {
        return baseUri.buildUpon().appendQueryParameter(BrowserContract.PARAM_SHOW_DELETED, "true").build();
    }

    private long getLastModified(final Uri uri) throws RemoteException {
        final Cursor cursor = bookmarksClient.query(uri,
                                                    new String[] { BrowserContract.Bookmarks.DATE_MODIFIED },
                                                    null,
                                                    null,
                                                    null);
        assertNotNull(cursor);

        long lastModified = INVALID_TIMESTAMP;
        try {
            assertTrue(cursor.moveToFirst());
            assertEquals(1, cursor.getCount());
            lastModified = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.DATE_MODIFIED));
        } finally {
            cursor.close();
        }
        assertNotEquals(lastModified, INVALID_TIMESTAMP);
        return lastModified;
    }

    private long getBookmarkIdFromGuid(String guid) throws RemoteException {
        Cursor cursor = bookmarksClient.query(BrowserContract.Bookmarks.CONTENT_URI,
                                              new String[] { BrowserContract.Bookmarks._ID },
                                              BrowserContract.Bookmarks.GUID + " = ?",
                                              new String[] { guid },
                                              null);
        assertNotNull(cursor);

        long id = INVALID_ID;
        try {
            assertTrue(cursor.moveToFirst());
            id = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks._ID));
        } finally {
            cursor.close();
        }
        assertNotEquals(id, INVALID_ID);
        return id;
    }

    private Uri insertBookmark(String title, String url, String guid, long parentId, int type) throws RemoteException {
        final ContentValues values = new ContentValues();
        if (title != null) {
            values.put(BrowserContract.Bookmarks.TITLE, title);
        }
        if (url != null) {
            values.put(BrowserContract.Bookmarks.URL, url);
        }
        if (guid != null) {
            values.put(BrowserContract.Bookmarks.GUID, guid);
        }
        values.put(BrowserContract.Bookmarks.PARENT, parentId);
        values.put(BrowserContract.Bookmarks.TYPE, type);

        final long now = System.currentTimeMillis();
        values.put(BrowserContract.Bookmarks.DATE_CREATED, now);
        values.put(BrowserContract.Bookmarks.DATE_MODIFIED, now);

        return bookmarksClient.insert(bookmarksTestUri, values);
    }

    private int getRowCount(String selection, String[] selectionArgs) throws RemoteException {
        final Cursor cursor = bookmarksClient.query(bookmarksTestUri,
                                                    new String[] { BrowserContract.Bookmarks._ID },
                                                    selection,
                                                    selectionArgs,
                                                    null);
        assertNotNull(cursor);
        try {
            return cursor.getCount();
        } finally {
            cursor.close();
        }
    }
}