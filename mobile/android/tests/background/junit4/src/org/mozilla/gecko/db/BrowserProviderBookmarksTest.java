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

import static org.junit.Assert.assertEquals;
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

    private ShadowContentResolver contentResolver;
    private ContentProviderClient bookmarksClient;
    private Uri bookmarksTestUri;
    private BrowserProvider provider;

    @Before
    public void setUp() throws Exception {
        provider = new BrowserProvider();
        provider.onCreate();
        ShadowContentResolver.registerProvider(BrowserContract.AUTHORITY, new DelegatingTestContentProvider(provider));

        contentResolver = new ShadowContentResolver();
        bookmarksClient = contentResolver.acquireContentProviderClient(BrowserContractHelpers.BOOKMARKS_CONTENT_URI);

        bookmarksTestUri = testUri(BrowserContract.Bookmarks.CONTENT_URI);
    }

    @After
    public void tearDown() {
        bookmarksClient.release();
        provider.shutdown();
    }

    @Test
    public void testDeleteBookmarkFolder() throws RemoteException {
        final long rootId = getBookmarkIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);

        // Create a folder.
        final Uri parentUri = insertBookmark("parent", null, "parent", rootId,
                                             BrowserContract.Bookmarks.TYPE_FOLDER);
        final long parentId = Long.parseLong(parentUri.getLastPathSegment());

        // Insert 10 bookmarks into the parent
        for (int i = 0; i < 10; i++) {
            insertBookmark("bookmark-" + i, "https://www.mozilla-" + i + ".org", "guid-" + i,
                           parentId, BrowserContract.Bookmarks.TYPE_BOOKMARK);
        }

        final String selection = BrowserContract.Bookmarks._ID + " = ? OR " +
                                 BrowserContract.Bookmarks.PARENT + " = ?";
        final String[] selectionArgs = { String.valueOf(parentId),
                                         String.valueOf(parentId) };
        // Check insertions. We should have 11 records(1 parent and 10 children).
        final int inserted = getRowCount(selection, selectionArgs);
        assertEquals(11, inserted);

        final int deleted = bookmarksClient.delete(bookmarksTestUri,
                                                   BrowserContract.Bookmarks._ID + " = ?",
                                                   new String[] { String.valueOf(parentId) });
        // Check if parent and all its descendants are deleted.
        assertEquals(11, deleted);

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
        try {
            while (cursor.moveToNext()) {
                final int isDeleted = cursor.getInt(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.IS_DELETED));
                assertEquals(1, isDeleted);

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