/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.db;

import android.content.ContentProviderClient;
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

        if (localVersion != null) {
            values.put(BrowserContract.Bookmarks.LOCAL_VERSION, localVersion);
        }

        if (syncVersion != null) {
            values.put(BrowserContract.Bookmarks.SYNC_VERSION, syncVersion);
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
