/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.db;

import android.content.ContentProviderClient;
import android.content.ContentUris;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteException;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.db.DelegatingTestContentProvider;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.robolectric.shadows.ShadowContentResolver;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Random;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mozilla.gecko.db.BrowserProviderGeneralTest.INVALID_TIMESTAMP;
import static org.mozilla.gecko.db.BrowserProviderGeneralTest.assertVersionsForSelection;
import static org.mozilla.gecko.db.BrowserProviderGeneralTest.bookmarksTestSyncUri;
import static org.mozilla.gecko.db.BrowserProviderGeneralTest.bookmarksTestUri;
import static org.mozilla.gecko.db.BrowserProviderGeneralTest.getBookmarksTestSyncIncrementLocalVersionUri;
import static org.mozilla.gecko.db.BrowserProviderGeneralTest.getBookmarkIdFromGuid;
import static org.mozilla.gecko.db.BrowserProviderGeneralTest.insertBookmark;
import static org.mozilla.gecko.db.BrowserProviderGeneralTest.withDeleted;
import static org.mozilla.gecko.db.BrowserProviderGeneralTest.withSync;

/**
 * Testing direct interactions with bookmarks through BrowserProvider
 */
@RunWith(TestRunner.class)
public class BrowserProviderBookmarksTest {
    private ContentProviderClient bookmarksClient;
    private BrowserProvider provider;

    @Before
    public void setUp() throws Exception {
        provider = new BrowserProvider();
        provider.onCreate();
        ShadowContentResolver.registerProvider(BrowserContract.AUTHORITY, new DelegatingTestContentProvider(provider));

        ShadowContentResolver contentResolver = new ShadowContentResolver();
        bookmarksClient = contentResolver.acquireContentProviderClient(BrowserContractHelpers.BOOKMARKS_CONTENT_URI);
    }

    @After
    public void tearDown() {
        bookmarksClient.release();
        provider.shutdown();
    }

    @Test
    public void testBookmarkChangedCountsOnUpdates() throws RemoteException {
        final long rootId = getIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
        insert("bookmark-1", "https://www.mozilla-1.org", "guid-1",
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);

        ContentValues values = new ContentValues();
        values.put(BrowserContract.Bookmarks.TITLE, "bookmark-1-2");
        int changed = bookmarksClient.update(
                bookmarksTestUri, values, BrowserContract.Bookmarks.GUID + " = ?",
                new String[] { "guid-1" });

        // Only record itself is changed.
        assertEquals(1, changed);

        // Test that changing a parent of a record affects three records - record itself, new parent, old parent.
        insert("bookmark-1", null, "parent", rootId, BrowserContract.Bookmarks.TYPE_FOLDER);
        long parentId = getIdFromGuid("parent");
        values = new ContentValues();
        values.put(BrowserContract.Bookmarks.PARENT, parentId);

        Uri uriWithOldParent = bookmarksTestUri.buildUpon()
                .appendQueryParameter(BrowserContract.PARAM_OLD_BOOKMARK_PARENT, String.valueOf(rootId))
                .build();
        changed = bookmarksClient.update(
                uriWithOldParent,
                values,
                BrowserContract.Bookmarks.GUID + " = ?",
                new String[] { "guid-1" }
        );

        assertEquals(3, changed);

        // Test that changing a parent of a record from sync affects only the record itself.
        changed = bookmarksClient.update(
                withSync(uriWithOldParent),
                values,
                BrowserContract.Bookmarks.GUID + " = ?",
                new String[] { "guid-1" }
        );

        assertEquals(1, changed);
    }

    @Test
    public void testBookmarkChangedCountsOnDeletions() throws RemoteException {
        final long rootId = getIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
        insert("bookmark-1", "https://www.mozilla-1.org", "guid-1",
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);

        int changed = bookmarksClient.delete(bookmarksTestUri,
                BrowserContract.Bookmarks.URL + " = ?",
                new String[] { "https://www.mozilla-1.org" });

        // Note that we also modify parent folder, and so changed counts must reflect that.
        assertEquals(2, changed);

        insert("bookmark-2", "https://www.mozilla-2.org", "guid-2",
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);
        insert("bookmark-3", "https://www.mozilla-3.org", "guid-3",
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);
        insert("bookmark-4", "https://www.mozilla-4.org", "guid-4",
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);

        changed = bookmarksClient.delete(bookmarksTestUri,
                BrowserContract.Bookmarks.PARENT + " = ?",
                new String[] { String.valueOf(rootId) });

        assertEquals(4, changed);

        // Test that we correctly count affected records during deletions of subtrees.
        final Uri parentUri = insert("parent", null, "parent", rootId,
                BrowserContract.Bookmarks.TYPE_FOLDER);
        final long parentId = Long.parseLong(parentUri.getLastPathSegment());

        insert("bookmark-5", "https://www.mozilla-2.org", "guid-5",
                parentId, BrowserContract.Bookmarks.TYPE_BOOKMARK);
        insert("bookmark-6", "https://www.mozilla-3.org", "guid-6",
                parentId, BrowserContract.Bookmarks.TYPE_BOOKMARK);
        // Directly under the root.
        insert("bookmark-7", "https://www.mozilla-4.org", "guid-7",
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);

        changed = bookmarksClient.delete(bookmarksTestUri,
                BrowserContract.Bookmarks.GUID + " = ?",
                new String[] { "parent" });

        // 4 = parent, its two children, mobile root.
        assertEquals(4, changed);
    }

    @Test
    public void testBookmarkFolderLastModifiedOnDeletion() throws RemoteException {
        final long rootId = getIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);

        // root
        // -> sibling
        // -> parent -- timestamp must change
        // ---> child-1
        // ---> child-2 -- delete this one

        final Uri parentUri = insert("parent", null, "parent", rootId,
                                             BrowserContract.Bookmarks.TYPE_FOLDER);
        final Uri siblingUri = insert("sibling", null, "sibling", rootId,
                                              BrowserContract.Bookmarks.TYPE_FOLDER);

        final long parentId = Long.parseLong(parentUri.getLastPathSegment());
        final Uri child1Uri = insert("child-1", null, "child-1", parentId,
                                             BrowserContract.Bookmarks.TYPE_FOLDER);
        // Insert record "from sync", so that we set syncVersion=1, avoiding clean-up logic from
        // straight up deleting "unsynced" record later in the test from the database, as opposed
        // to just marking it as deleted.
        final Uri child2Uri = insert("child-2", null, "child-2", parentId,
                                             BrowserContract.Bookmarks.TYPE_FOLDER, true);

        final long parentLastModifiedBeforeDeletion = getLastModified(parentUri);
        final long siblingLastModifiedBeforeDeletion = getLastModified(siblingUri);

        final long child1LastModifiedBeforeDeletion = getLastModified(child1Uri);
        final long child2LastModifiedBeforeDeletion = getLastModified(child2Uri);

        bookmarksClient.delete(bookmarksTestUri, "guid = ?", new String[] {"child-2"});

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
        final long rootId = getIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);

        // First, create some bookmarks for our test. Insert them "from sync", so that we leave
        // tombstone records around after deletion, so that we may verify they're being "deleted"
        // correctly.

        // Create a folder.
        final Uri parentUri = insert("parent", null, "parent", rootId,
                                             BrowserContract.Bookmarks.TYPE_FOLDER, true);
        final long parentId = Long.parseLong(parentUri.getLastPathSegment());

        // Insert 10 bookmarks into the parent.
        for (int i = 0; i < 10; i++) {
            insert("bookmark-" + i, "https://www.mozilla-" + i + ".org", "guid-" + i,
                           parentId, BrowserContract.Bookmarks.TYPE_BOOKMARK, true);
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

        // Check that records are marked as deleted and columns are reset.
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

    @Test
    public void testBookmarkVersioning() throws Exception {
        // Any CRUD operations on bookmarks performed "from Fennec" must bump its local version, and
        // not touch its sync version.

        // Any CRUD operations "from sync" must not touch any of the versions.
        // Sync must explicitly perform bulk update of sync versions (see testBulkUpdatingSyncVersion).

        final long rootId = getIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);

        assertVersionsForGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID, 1, 0);

        // insert bookmark from fennec
        String guid1 = UUID.randomUUID().toString();
        String guid2 = UUID.randomUUID().toString();
        String guid3 = UUID.randomUUID().toString();

        assertEquals(7, getTotalNumberOfBookmarks());

        insert("bookmark-1", "https://www.mozilla-1.org", guid1,
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);
        assertVersionsForGuid(guid1, 1, 0);
        assertVersionsForGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID, 2, 0);

        assertEquals(8, getTotalNumberOfBookmarks());

        // insert bookmark from sync
        insert("bookmark-2", "https://www.mozilla-2.org", guid2,
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK, true);
        assertVersionsForGuid(guid2, 1, 1);
        // Inserting a bookmark from sync should not touch its parent's localVersion
        assertVersionsForGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID, 2, 0);

        // insert bookmark from sync, asking that it starts off as "modified"
        insert("bookmark-3", "https://www.mozilla-3.org", guid3,
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK, true, true);
        assertVersionsForGuid(guid3, 2, 1);

        assertEquals(10, getTotalNumberOfBookmarks());

        // Updates to bookmark, either from fennec or from sync, should not touch its parent's localVersion

        // modify bookmark from fennec
        updateBookmarkTitleByGuid(bookmarksTestUri, guid1, "new title");
        assertVersionsForGuid(guid1, 2, 0);

        assertVersionsForGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID, 2, 0);

        updateBookmarkTitleByGuid(bookmarksTestUri, guid2, "another title");
        assertVersionsForGuid(guid2, 2, 1);

        assertVersionsForGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID, 2, 0);

        // modify bookmark from sync
        updateBookmarkTitleByGuid(bookmarksTestSyncUri, guid1, "another title");
        assertVersionsForGuid(guid1, 2, 0);

        assertVersionsForGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID, 2, 0);

        updateBookmarkTitleByGuid(bookmarksTestSyncUri, guid2, "just title");
        assertVersionsForGuid(guid2, 2, 1);

        // Test that explicitely asking to update localVersion from sync does increment it.
        updateBookmarkTitleByGuid(getBookmarksTestSyncIncrementLocalVersionUri, guid2, "again, title!");
        assertVersionsForGuid(guid2, 3, 1);

        assertVersionsForGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID, 2, 0);

        // delete bookmark from fennec, expect bookmark tombstones to persist for unsynced records
        deleteByGuid(bookmarksTestUri, guid1);
        assertEquals(1, getRowCount(Bookmarks.GUID + " = ?", new String[]{guid1}));

        assertEquals(10, getTotalNumberOfBookmarks());

        assertVersionsForGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID, 3, 0);

        // insert a bookmark from sync, and delete it from fennec. this should bump its localVersion
        String guid4 = UUID.randomUUID().toString();
        insert("bookmark-4", "https://www.mozilla-4.org", guid4,
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK, true);

        assertEquals(11, getTotalNumberOfBookmarks());

        // inserting bookmark from sync should not touch its parent's localVersion
        assertVersionsForGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID, 3, 0);

        assertVersionsForGuid(guid4, 1, 1);
        deleteByGuid(bookmarksTestUri, guid4);

        // deleting bookmark from fennec _must_ increment its parent's localVersion
        assertVersionsForGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID, 4, 0);

        // sanity check our total number of bookmarks, it shouldn't have changed because of the delete
        assertEquals(11, getTotalNumberOfBookmarks());

        // assert that bookmark is still present after deletion, and its local version has been bumped
        assertEquals(1, getRowCount(Bookmarks.GUID + " = ?", new String[]{guid4}));
        assertVersionsForGuid(guid4, 2, 1);

        // delete bookmark from sync, will just DELETE it from the database entirely
        deleteByGuid(bookmarksTestSyncUri, guid2);
        assertEquals(10, getTotalNumberOfBookmarks());
        assertEquals(0, getRowCount(Bookmarks.GUID + " = ?", new String[]{guid2}));

        // deleting bookmark from sync should not touch its parent's localVersion
        assertVersionsForGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID, 4, 0);

        // check that deleting a folder from fennec bumps localVersions of its entire subtree
        // root  -- localVersion bumped
        // -> sibling
        // -> folder-1
        // -> parent -- delete this record
        // ---> child-1 -- deleted, localVersion bumped
        // ---> child-2 -- deleted, localVersion bumped
        // ---> folder-2  -- deleted, localVersion bumped
        // -----> child-3 -- deleted, localVersion bumped

        insert("folder-1", null, "folder-1", rootId,
                Bookmarks.TYPE_FOLDER);
        insert("sibling", "http://www.mozilla-1.org", "sibling", rootId,
                Bookmarks.TYPE_BOOKMARK);

        final Uri parentUri = insert("parent", null, "parent", rootId,
                Bookmarks.TYPE_FOLDER, true);
        final long parentId = Long.parseLong(parentUri.getLastPathSegment());
        insert("child-1", null, "child-1", parentId,
                Bookmarks.TYPE_FOLDER, true);
        // Insert record "from sync", so that we set syncVersion=1, avoiding clean-up logic from
        // straight up deleting "unsynced" record later in the test from the database, as opposed
        // to just marking it as deleted.
        insert("child-2", "http://www.mozilla-2.org", "child-2", parentId,
                Bookmarks.TYPE_BOOKMARK, true);
        final Uri folder2Uri = insert("folder-2", null, "folder-2", parentId,
                Bookmarks.TYPE_FOLDER, true);
        final long folder2Id = Long.parseLong(folder2Uri.getLastPathSegment());

        insert("child-3", "http://www.mozilla-4.org", "child-3", folder2Id,
                Bookmarks.TYPE_BOOKMARK, true);

        assertVersionsForGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID, 6, 0);
        assertVersionsForGuid("sibling", 1, 0);
        assertVersionsForGuid("folder-1", 1, 0);
        assertVersionsForGuid("parent", 1, 1);
        assertVersionsForGuid("child-1", 1, 1);
        assertVersionsForGuid("child-2", 1, 1);
        assertVersionsForGuid("folder-2", 1, 1);
        assertVersionsForGuid("child-3", 1, 1);

        deleteByGuid(bookmarksTestUri, "parent");

        assertVersionsForGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID, 7, 0);
        assertVersionsForGuid("sibling", 1, 0);
        assertVersionsForGuid("folder-1", 1, 0);
        assertVersionsForGuid("parent", 2, 1);
        assertVersionsForGuid("child-1", 2, 1);
        assertVersionsForGuid("child-2", 2, 1);
        assertVersionsForGuid("folder-2", 2, 1);
        assertVersionsForGuid("child-3", 2, 1);
    }

    @Test
    public void testBulkUpdatingSyncVersions() throws Exception {
        final long rootId = getIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);

        ConcurrentHashMap<String, Integer> guidsToVersionMap = new ConcurrentHashMap<>();

        for (int i = 0; i < 1500; i++) {
            String guid = UUID.randomUUID().toString();
            insert("bookmark-" + i, "https://www.mozilla-" + i + ".org", guid,
                    rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK);
            assertVersionsForGuid(guid, 1, 0);
            guidsToVersionMap.put(guid, 1);
        }

        final Bundle data = new Bundle();
        data.putSerializable(BrowserContract.METHOD_PARAM_DATA, guidsToVersionMap);

        // Bulk sync version updates are only allowed for bookmarks.
        ShadowContentResolver contentResolver = new ShadowContentResolver();
        ContentProviderClient client = contentResolver.acquireContentProviderClient(BrowserContractHelpers.HISTORY_CONTENT_URI);
        try {
            client.call(
                    BrowserContract.METHOD_UPDATE_SYNC_VERSIONS,
                    BrowserContractHelpers.HISTORY_CONTENT_URI.toString(),
                    data
            );
            fail();
        } catch (IllegalStateException e) {
        } finally {
            client.release();
        }

        // Bulk sync version updates are only allowed from sync.
        try {
            bookmarksClient.call(
                    BrowserContract.METHOD_UPDATE_SYNC_VERSIONS,
                    bookmarksTestUri.toString(),
                    data
            );
            fail();
        } catch (IllegalStateException e) {
        }

        final Bundle emptyData = new Bundle();
        emptyData.putSerializable(BrowserContract.METHOD_PARAM_DATA, new ConcurrentHashMap<String, Integer>());

        Bundle result = bookmarksClient.call(
                BrowserContract.METHOD_UPDATE_SYNC_VERSIONS,
                bookmarksTestSyncUri.toString(),
                emptyData
        );
        assertNotNull(result);
        int changed = (int) result.getSerializable(BrowserContract.METHOD_RESULT);
        assertEquals(0, changed);

        result = bookmarksClient.call(
                BrowserContract.METHOD_UPDATE_SYNC_VERSIONS,
                bookmarksTestSyncUri.toString(),
                data
        );
        assertNotNull(result);
        changed = (int) result.getSerializable(BrowserContract.METHOD_RESULT);
        assertEquals(guidsToVersionMap.size(), changed);

        for (String guid : guidsToVersionMap.keySet()) {
            assertVersionsForGuid(guid, 1, 1);
        }
    }

    @Test
    public void testThatVersionsCanNotBeSetManually() throws Exception {
        final long rootId = getIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);

        // First, try from sync
        try {
            insert("bookmark-1", "https://www.mozilla-1.org", "guid-1",
                    rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK, 1, 1, true);
            fail();
        } catch (IllegalArgumentException e) {}
        try {
            insert("bookmark-1", "https://www.mozilla-1.org", "guid-1",
                    rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK, 1, null, true);
            fail();
        } catch (IllegalArgumentException e) {}
        try {
            insert("bookmark-1", "https://www.mozilla-1.org", "guid-1",
                    rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK, null, 1, true);
            fail();
        } catch (IllegalArgumentException e) {}

        // Now, try from fennec
        try {
            insert("bookmark-1", "https://www.mozilla-1.org", "guid-1",
                    rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK, 1, 1, false);
            fail();
        } catch (IllegalArgumentException e) {}
        try {
            insert("bookmark-1", "https://www.mozilla-1.org", "guid-1",
                    rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK, 1, null, false);
            fail();
        } catch (IllegalArgumentException e) {}
        try {
            insert("bookmark-1", "https://www.mozilla-1.org", "guid-1",
                    rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK, null, 1, false);
            fail();
        } catch (IllegalArgumentException e) {}
    }

    @Test
    public void testThatVersionsCanNotBeModifiedManually() throws Exception {
        final long rootId = getIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);

        insert("bookmark-1", "https://www.mozilla-1.org", "guid-1",
                rootId, BrowserContract.Bookmarks.TYPE_BOOKMARK, false);
        assertVersionsForGuid("guid-1", 1, 0);

        // Try from sync
        try {
            updateBookmarkVersionsByGuid(bookmarksTestSyncUri, "guid-1", 2, 2);
            fail();
        } catch (IllegalArgumentException e) {}
        try {
            updateBookmarkVersionsByGuid(bookmarksTestSyncUri, "guid-1", 2, null);
            fail();
        } catch (IllegalArgumentException e) {}
        try {
            updateBookmarkVersionsByGuid(bookmarksTestSyncUri, "guid-1", null, 2);
            fail();
        } catch (IllegalArgumentException e) {}

        // Try from fennec
        try {
            updateBookmarkVersionsByGuid(bookmarksTestUri, "guid-1", 2, 2);
            fail();
        } catch (IllegalArgumentException e) {}
        try {
            updateBookmarkVersionsByGuid(bookmarksTestUri, "guid-1", 2, null);
            fail();
        } catch (IllegalArgumentException e) {}
        try {
            updateBookmarkVersionsByGuid(bookmarksTestUri, "guid-1", null, 2);
            fail();
        } catch (IllegalArgumentException e) {}

        assertVersionsForGuid("guid-1", 1, 0);
    }

    @Test
    public void testBookmarkBulkPositionChange() throws Exception {
        // We have a hacky ContentProvider API which allows for bulk-changing of positions.
        // This test creates a folder with a lot of bookmarks, shuffles their positions via the API,
        // and ensures that the shuffled order has been persisted.
        final int NUMBER_OF_CHILDREN = 1001;
        final long rootId = getIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
        long folderId = ContentUris.parseId(insert("FolderFolder", "", "folderfolder",
                rootId, Bookmarks.TYPE_FOLDER));

        // Create the children.
        String[] items = new String[NUMBER_OF_CHILDREN];

        for (int i = 0; i < NUMBER_OF_CHILDREN; ++i) {
            String guid = Utils.generateGuid();
            items[i] = guid;
            assertNotNull(insert(
                    "Test Bookmark" + guid,
                    "http://example.com" + guid,
                    guid, folderId, Bookmarks.TYPE_FOLDER, i, false)
            );
        }

        assertPositionsForParent(folderId, items);

        // Now permute the items array.
        Random rand = new Random();
        for (int i = 0; i < NUMBER_OF_CHILDREN; ++i) {
            final int newPosition = rand.nextInt(NUMBER_OF_CHILDREN);
            final String switched = items[newPosition];
            items[newPosition] = items[i];
            items[i] = switched;
        }

        // Impose the shuffled positions.
        long updated = bookmarksClient.update(BrowserContract.Bookmarks.POSITIONS_CONTENT_URI, null, null, items);
        // Our schema
        assertEquals(NUMBER_OF_CHILDREN, updated);
        assertPositionsForParent(folderId, items);
    }

    private void assertPositionsForParent(long parentId, String[] guidPositions) throws RemoteException {
        Cursor cursor = bookmarksClient.query(
                bookmarksTestSyncUri,
                new String[] {Bookmarks.GUID, Bookmarks.POSITION},
                Bookmarks.PARENT + " = ?",
                new String[] {String.valueOf(parentId)},
                null
        );
        assertNotNull(cursor);
        assertEquals(guidPositions.length, cursor.getCount());
        assertTrue(cursor.moveToFirst());
        final int guidCol = cursor.getColumnIndexOrThrow(Bookmarks.GUID);
        final int positionCol = cursor.getColumnIndexOrThrow(Bookmarks.POSITION);
        try {
            for (int i = 0; i < guidPositions.length; i++) {
                // guid: guidPositions[i]
                // position: i
                assertEquals(guidPositions[i], cursor.getString(guidCol));
                assertEquals(i, cursor.getInt(positionCol));
                cursor.moveToNext();
            }
        } finally {
            cursor.close();
        }
    }

    private void deleteByGuid(Uri uri, String guid) throws RemoteException {
        bookmarksClient.delete(uri,
                Bookmarks.GUID + " = ?",
                new String[] { guid });
    }

    private void updateBookmarkTitleByGuid(Uri uri, String guid, String newTitle) throws RemoteException {
        ContentValues values = new ContentValues();
        values.put(Bookmarks.TITLE, newTitle);
        bookmarksClient.update(uri, values, Bookmarks.GUID + " = ?", new String[] {guid});
    }

    private void updateBookmarkVersionsByGuid(Uri uri, String guid, Integer localVersion, Integer syncVersion) throws RemoteException {
        ContentValues values = new ContentValues();
        values.put(Bookmarks.LOCAL_VERSION, localVersion);
        values.put(Bookmarks.SYNC_VERSION, syncVersion);
        bookmarksClient.update(uri, values, Bookmarks.GUID + " = ?", new String[] {guid});
    }

    private void assertVersionsForGuid(String guid, int localVersion, int syncVersion) throws RemoteException {
        assertVersionsForSelection(bookmarksClient, bookmarksTestUri, Bookmarks.GUID + " = ?", new String[] {guid}, localVersion, syncVersion);
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

    private long getIdFromGuid(String guid) throws RemoteException {
        return getBookmarkIdFromGuid(bookmarksClient, guid);
    }

    private Uri insert(String title, String url, String guid, long parentId, int type) throws RemoteException {
        return insert(title, url, guid, parentId, type, 0, false);
    }

    private Uri insert(String title, String url, String guid, long parentId, int type, boolean fromSync) throws RemoteException {
        return insert(title, url, guid, parentId, type, 0, null, null, fromSync);
    }

    private Uri insert(String title, String url, String guid, long parentId, int type, int position, boolean fromSync) throws RemoteException {
        return insert(title, url, guid, parentId, type, position, null, null, fromSync);
    }

    private Uri insert(String title, String url, String guid, long parentId, int type, boolean fromSync, boolean insertAsModified) throws RemoteException {
        return insert(title, url, guid, parentId, type, 0, null, null, fromSync, insertAsModified);
    }

    private Uri insert(String title, String url, String guid, long parentId, int type, Integer localVersion, Integer syncVersion, boolean fromSync) throws RemoteException {
        return insert(title, url, guid, parentId, type, 0, localVersion, syncVersion, fromSync, false);
    }

    private Uri insert(String title, String url, String guid, long parentId, int type, int position, Integer localVersion, Integer syncVersion, boolean fromSync) throws RemoteException {
        return insert(title, url, guid, parentId, type, position, localVersion, syncVersion, fromSync, false);
    }

    private Uri insert(String title, String url, String guid, long parentId, int type, int position, Integer localVersion, Integer syncVersion, boolean fromSync, boolean insertAsModified) throws RemoteException {
        return insertBookmark(bookmarksClient, title, url, guid, parentId, type, position, localVersion, syncVersion, fromSync, insertAsModified);
    }

    private int getTotalNumberOfBookmarks() throws RemoteException {
        return getRowCount(null, null);
    }

    private int getRowCount(String selection, String[] selectionArgs) throws RemoteException {
        final Cursor cursor = bookmarksClient.query(withDeleted(bookmarksTestUri),
                                                    new String[] { BrowserContract.Bookmarks._ID, Bookmarks.GUID },
                                                    selection,
                                                    selectionArgs,
                                                    null);
        assertNotNull(cursor);
        try {
            while (cursor.moveToNext()) {
                String guid = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.GUID));
                int id = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));
            }
            return cursor.getCount();
        } finally {
            cursor.close();
        }
    }
}