/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.db;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteException;
import android.provider.Browser;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.db.DelegatingTestContentProvider;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.robolectric.shadows.ShadowContentResolver;

import java.util.ArrayList;
import java.util.UUID;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

@RunWith(TestRunner.class)
public class BrowserProviderGeneralTest {
    final static long INVALID_TIMESTAMP = -1;
    final static Uri bookmarksTestUri = withTest(BrowserContract.Bookmarks.CONTENT_URI);
    final static Uri bookmarksTestSyncUri = withSync(bookmarksTestUri);
    final static Uri getBookmarksTestSyncIncrementLocalVersionUri = bookmarksTestSyncUri
            .buildUpon()
            .appendQueryParameter(BrowserContract.PARAM_INCREMENT_LOCAL_VERSION_FROM_SYNC, "true")
            .build();

    private static final long INVALID_ID = -1;

    private BrowserProvider provider;
    private ContentProviderClient browserClient;

    @Before
    public void setUp() throws Exception {
        provider = new BrowserProvider();
        provider.onCreate();

        ShadowContentResolver.registerProvider(BrowserContract.AUTHORITY, new DelegatingTestContentProvider(provider));

        ShadowContentResolver contentResolver = new ShadowContentResolver();
        browserClient = contentResolver.acquireContentProviderClient(BrowserContractHelpers.BOOKMARKS_CONTENT_URI);
    }

    @After
    public void tearDown() throws Exception {
        browserClient.release();
        provider.shutdown();
    }

    @Test
    public void testUpdatingWithLocalVersionAssertion() throws Exception {
        // try bad calls first

        // missing guid, expected version, METHOD_PARAM_DATA
        try {
            browserClient.call(
                    BrowserContract.METHOD_UPDATE_BY_GUID_ASSERTING_LOCAL_VERSION,
                    bookmarksTestSyncUri.toString(),
                    new Bundle()
            );
            fail();
        } catch (IllegalArgumentException e) {}

        // missing expected version, METHOD_PARAM_DATA
        final Bundle data = new Bundle();
        data.putString(BrowserContract.SyncColumns.GUID, "guid");
        try {
            browserClient.call(
                    BrowserContract.METHOD_UPDATE_BY_GUID_ASSERTING_LOCAL_VERSION,
                    bookmarksTestSyncUri.toString(),
                    data
            );
            fail();
        } catch (IllegalArgumentException e) {}

        // missing METHOD_PARAM_DATA
        data.putInt(BrowserContract.VersionColumns.LOCAL_VERSION, 2);
        try {
            browserClient.call(
                    BrowserContract.METHOD_UPDATE_BY_GUID_ASSERTING_LOCAL_VERSION,
                    bookmarksTestSyncUri.toString(),
                    data
            );
            fail();
        } catch (IllegalArgumentException e) {}

        ContentValues cv = new ContentValues();
        cv.put(BrowserContract.Bookmarks.TITLE, "new title");

        // try updating a non-existent record
        data.putParcelable(BrowserContract.METHOD_PARAM_DATA, cv);
        data.putString(BrowserContract.Bookmarks.GUID, "bad-guid");
        data.putInt(BrowserContract.VersionColumns.LOCAL_VERSION, 2);

        Bundle result = browserClient.call(
                BrowserContract.METHOD_UPDATE_BY_GUID_ASSERTING_LOCAL_VERSION,
                bookmarksTestSyncUri.toString(),
                data
        );
        assertNotNull(result);
        assertTrue(result.containsKey(BrowserContract.METHOD_RESULT));
        assertEquals(false, result.getSerializable(BrowserContract.METHOD_RESULT));

        // insert a record
        long parentId = getBookmarkIdFromGuid(browserClient, BrowserContract.Bookmarks.MENU_FOLDER_GUID);
        insertBookmark(browserClient, "test", "http://mozilla-1.org", "guid-1", parentId, BrowserContract.Bookmarks.TYPE_BOOKMARK, null, null, true);

        // try incorrect version
        data.putString(BrowserContract.Bookmarks.GUID, "guid-1");
        data.putInt(BrowserContract.VersionColumns.LOCAL_VERSION, 2);

        result = browserClient.call(
                BrowserContract.METHOD_UPDATE_BY_GUID_ASSERTING_LOCAL_VERSION,
                bookmarksTestSyncUri.toString(),
                data
        );
        assertNotNull(result);
        assertTrue(result.containsKey(BrowserContract.METHOD_RESULT));
        assertEquals(false, result.getSerializable(BrowserContract.METHOD_RESULT));

        // try correct version
        data.putInt(BrowserContract.VersionColumns.LOCAL_VERSION, 1);
        result = browserClient.call(
                BrowserContract.METHOD_UPDATE_BY_GUID_ASSERTING_LOCAL_VERSION,
                bookmarksTestSyncUri.toString(),
                data
        );
        assertNotNull(result);
        assertTrue(result.containsKey(BrowserContract.METHOD_RESULT));
        assertEquals(true, result.getSerializable(BrowserContract.METHOD_RESULT));

        // try combining assertion and incrementing a version flag
        result = browserClient.call(
                BrowserContract.METHOD_UPDATE_BY_GUID_ASSERTING_LOCAL_VERSION,
                getBookmarksTestSyncIncrementLocalVersionUri.toString(),
                data
        );
        assertNotNull(result);
        assertTrue(result.containsKey(BrowserContract.METHOD_RESULT));
        assertEquals(true, result.getSerializable(BrowserContract.METHOD_RESULT));

        // try old version
        data.putInt(BrowserContract.VersionColumns.LOCAL_VERSION, 1);
        result = browserClient.call(
                BrowserContract.METHOD_UPDATE_BY_GUID_ASSERTING_LOCAL_VERSION,
                bookmarksTestSyncUri.toString(),
                data
        );
        assertNotNull(result);
        assertTrue(result.containsKey(BrowserContract.METHOD_RESULT));
        assertEquals(false, result.getSerializable(BrowserContract.METHOD_RESULT));

        // try new version
        data.putInt(BrowserContract.VersionColumns.LOCAL_VERSION, 2);
        result = browserClient.call(
                BrowserContract.METHOD_UPDATE_BY_GUID_ASSERTING_LOCAL_VERSION,
                bookmarksTestSyncUri.toString(),
                data
        );
        assertNotNull(result);
        assertTrue(result.containsKey(BrowserContract.METHOD_RESULT));
        assertEquals(true, result.getSerializable(BrowserContract.METHOD_RESULT));
    }

    @Test
    public void testRecordVersionReset() throws Exception {
        final Uri resetAllUri = withTest(BrowserContract.AUTHORITY_URI)
                .buildUpon().appendQueryParameter(BrowserContract.PARAM_RESET_VERSIONS_FOR_ALL_TYPES, "true").build();

        // Resetting versions is only allowed from sync.
        try {
            browserClient.call(
                    BrowserContract.METHOD_RESET_RECORD_VERSIONS,
                    resetAllUri.toString(),
                    null
            );
            browserClient.call(
                    BrowserContract.METHOD_RESET_RECORD_VERSIONS,
                    bookmarksTestUri.toString(),
                    null
            );
            fail();
        } catch (IllegalStateException e) {}

        // Test that we can call this for whatever default state of the database is.
        Bundle result = browserClient.call(
                BrowserContract.METHOD_RESET_RECORD_VERSIONS,
                withSync(resetAllUri).toString(),
                null
        );
        assertNotNull(result);
        // Currently, only bookmarks support versioning. There are 7 default bookmarks, so we expect
        // to see 7 changed records.
        assertEquals(7, result.getInt(BrowserContract.METHOD_RESULT));

        // Test that we can call this for whatever default state of the database is.
        result = browserClient.call(
                BrowserContract.METHOD_RESET_RECORD_VERSIONS,
                bookmarksTestSyncUri.toString(),
                null
        );
        assertNotNull(result);
        // Currently, only bookmarks support versioning. There are 7 default bookmarks, so we expect
        // to see 7 changed records.
        assertEquals(7, result.getInt(BrowserContract.METHOD_RESULT));

        long parentId = getBookmarkIdFromGuid(browserClient, BrowserContract.Bookmarks.MENU_FOLDER_GUID);

        // Insert a bookmark from sync (versions 1,1), reset versions, validate that bookmark was reset.
        insertBookmark(browserClient, "test", "http://mozilla-1.org", "guid-1", parentId, BrowserContract.Bookmarks.TYPE_BOOKMARK, null, null, true);
        assertVersionsForSelection(browserClient, bookmarksTestUri, BrowserContract.Bookmarks.GUID + " = ?", new String[]{"guid-1"}, 1, 1);

        result = browserClient.call(
                BrowserContract.METHOD_RESET_RECORD_VERSIONS,
                withSync(resetAllUri).toString(),
                null
        );
        assertNotNull(result);
        assertEquals(8, result.getInt(BrowserContract.METHOD_RESULT));
        assertVersionsForSelection(browserClient, bookmarksTestUri, BrowserContract.Bookmarks.GUID + " = ?", new String[]{"guid-1"}, 1, 0);

        // Insert a bookmark from sync (versions 1,1), reset versions "to synced", validate that new versions are (2,1)
        insertBookmark(browserClient, "test", "http://mozilla-2.org", "guid-2", parentId, BrowserContract.Bookmarks.TYPE_BOOKMARK, null, null, true);
        assertVersionsForSelection(browserClient, bookmarksTestUri, BrowserContract.Bookmarks.GUID + " = ?", new String[]{"guid-2"}, 1, 1);

        result = browserClient.call(
                BrowserContract.METHOD_RESET_RECORD_VERSIONS,
                withSync(bookmarksTestUri).buildUpon().appendQueryParameter(BrowserContract.PARAM_RESET_VERSIONS_TO_SYNCED, "true").build().toString(),
                null
        );
        assertNotNull(result);
        assertEquals(9, result.getInt(BrowserContract.METHOD_RESULT));
        assertVersionsForSelection(browserClient, bookmarksTestUri, BrowserContract.Bookmarks.GUID + " = ?", new String[]{"guid-2"}, 2, 1);

        // Insert many bookmarks. Reset versions, validate that all were reset.
        ArrayList<String> guids = new ArrayList<>();
        for (int i = 0; i < 2000; i++) {
            String guid = UUID.randomUUID().toString();
            insertBookmark(browserClient, "test", "http://mozilla-" + i + ".org", guid, parentId, BrowserContract.Bookmarks.TYPE_BOOKMARK, null, null, true);
            guids.add(guid);
        }

        result = browserClient.call(
                BrowserContract.METHOD_RESET_RECORD_VERSIONS,
                withSync(resetAllUri).toString(),
                null
        );
        assertNotNull(result);
        // 7 defaults, two we inserted above, and another 2000 we just inserted.
        assertEquals(2009, result.getInt(BrowserContract.METHOD_RESULT));

        for (String guid : guids) {
            assertVersionsForSelection(browserClient, bookmarksTestUri, BrowserContract.Bookmarks.GUID + " = ?", new String[]{guid}, 1, 0);
        }
    }

    public static Uri withTest(Uri baseUri) {
        return baseUri.buildUpon().appendQueryParameter(BrowserContract.PARAM_IS_TEST, "1").build();
    }

    public static Uri withDeleted(Uri baseUri) {
        return baseUri.buildUpon().appendQueryParameter(BrowserContract.PARAM_SHOW_DELETED, "true").build();
    }

    public static Uri withSync(Uri baseUri) {
        return baseUri.buildUpon().appendQueryParameter(BrowserContract.PARAM_IS_SYNC, "1").build();
    }

    public static void assertVersionsForSelection(ContentProviderClient client, Uri uri, String selection, String[] selectionArgs, int localVersion, int syncVersion) throws RemoteException {
        final Cursor cursor = client.query(withDeleted(uri),
                new String[] {BrowserContract.Bookmarks.LOCAL_VERSION, BrowserContract.Bookmarks.SYNC_VERSION},
                selection,
                selectionArgs,
                null);

        assertNotNull(cursor);

        try {
            assertEquals(1, cursor.getCount());

            final int localVersionCol = cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.LOCAL_VERSION);
            final int syncVersionCol = cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.SYNC_VERSION);

            assertTrue(cursor.moveToFirst());

            assertEquals(localVersion, cursor.getInt(localVersionCol));
            assertEquals(syncVersion, cursor.getInt(syncVersionCol));
        } finally {
            cursor.close();
        }
    }

    public static Uri insertBookmark(ContentProviderClient client, String title, String url, String guid, long parentId, int type, Integer localVersion, Integer syncVersion, boolean fromSync) throws RemoteException {
        return insertBookmark(client, title, url, guid, parentId, type, 0, localVersion, syncVersion, fromSync, false);
    }

    public static Uri insertBookmark(ContentProviderClient client, String title, String url, String guid, long parentId, int type, int position, Integer localVersion, Integer syncVersion, boolean fromSync, boolean insertAsModified) throws RemoteException {
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

        values.put(BrowserContract.Bookmarks.POSITION, position);

        if (localVersion != null) {
            values.put(BrowserContract.Bookmarks.LOCAL_VERSION, localVersion);
        }

        if (syncVersion != null) {
            values.put(BrowserContract.Bookmarks.SYNC_VERSION, syncVersion);
        }

        if (insertAsModified) {
            values.put(BrowserContract.Bookmarks.PARAM_INSERT_FROM_SYNC_AS_MODIFIED, true);
        }

        Uri insertionUri = bookmarksTestUri;
        if (fromSync) {
            insertionUri = bookmarksTestSyncUri;
        }

        return client.insert(insertionUri, values);
    }

    public static long getBookmarkIdFromGuid(ContentProviderClient client, String guid) throws RemoteException {
        Cursor cursor = client.query(BrowserContract.Bookmarks.CONTENT_URI,
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
}
