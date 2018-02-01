/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko;

import android.content.ContentProvider;
import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.database.Cursor;
import android.os.RemoteException;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.db.DelegatingTestContentProvider;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.PageMetadata;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.LocalBrowserDB;
import org.robolectric.shadows.ShadowContentResolver;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class GlobalPageMetadataTest {
    @Test
    public void testQueueing() throws Exception {
        BrowserDB db = new LocalBrowserDB("default");

        final ContentProvider provider = DelegatingTestContentProvider.createDelegatingBrowserProvider();
        try {
            ShadowContentResolver cr = new ShadowContentResolver();
            ContentProviderClient pageMetadataClient = cr.acquireContentProviderClient(PageMetadata.CONTENT_URI);

            assertEquals(0, GlobalPageMetadata.getInstance().getMetadataQueueSize());

            // There's not history record for this uri, so test that queueing works.
            GlobalPageMetadata.getInstance().doAddOrQueue(db, pageMetadataClient, "https://mozilla.org", false, "{type: 'article'}");

            assertPageMetadataCountForGUID(0, "guid1", pageMetadataClient);
            assertEquals(1, GlobalPageMetadata.getInstance().getMetadataQueueSize());

            // Test that queue doesn't duplicate metadata for the same history item.
            GlobalPageMetadata.getInstance().doAddOrQueue(db, pageMetadataClient, "https://mozilla.org", false, "{type: 'article'}");
            assertEquals(1, GlobalPageMetadata.getInstance().getMetadataQueueSize());

            // Test that queue is limited to 15 metadata items.
            for (int i = 0; i < 20; i++) {
                GlobalPageMetadata.getInstance().doAddOrQueue(db, pageMetadataClient, "https://mozilla.org/" + i, false, "{type: 'article'}");
            }
            assertEquals(15, GlobalPageMetadata.getInstance().getMetadataQueueSize());
        } finally {
            provider.shutdown();
        }
    }

    @Test
    public void testInsertingMetadata() throws Exception {
        BrowserDB db = new LocalBrowserDB("default");

        // Start listening for events.
        GlobalPageMetadata.getInstance().init();

        final ContentProvider provider = DelegatingTestContentProvider.createDelegatingBrowserProvider();
        try {
            ShadowContentResolver cr = new ShadowContentResolver();
            ContentProviderClient historyClient = cr.acquireContentProviderClient(BrowserContract.History.CONTENT_URI);
            ContentProviderClient pageMetadataClient = cr.acquireContentProviderClient(PageMetadata.CONTENT_URI);

            // Insert required history item...
            ContentValues cv = new ContentValues();
            cv.put(BrowserContract.History.GUID, "guid1");
            cv.put(BrowserContract.History.URL, "https://mozilla.org");
            historyClient.insert(BrowserContract.History.CONTENT_URI, cv);

            // TODO: Main test runner thread finishes before EventDispatcher events are processed...
            // Fire off a message saying that history has been inserted.
            // Bundle message = new Bundle();
            // message.putString(GlobalHistory.EVENT_PARAM_URI, "https://mozilla.org");
            // EventDispatcher.getInstance().dispatch(GlobalHistory.EVENT_URI_AVAILABLE_IN_HISTORY, message);

            // For now, let's just try inserting again.
            GlobalPageMetadata.getInstance().doAddOrQueue(db, pageMetadataClient, "https://mozilla.org", false, "{type: 'article', description: 'test article'}");

            assertPageMetadataCountForGUID(1, "guid1", pageMetadataClient);
            assertPageMetadataValues(pageMetadataClient, "guid1", false, "{\"type\":\"article\",\"description\":\"test article\"}");

            // Test that inserting empty metadata deletes existing metadata record.
            GlobalPageMetadata.getInstance().doAddOrQueue(db, pageMetadataClient, "https://mozilla.org", false, "{}");
            assertPageMetadataCountForGUID(0, "guid1", pageMetadataClient);

            // Test that inserting new metadata overrides existing metadata record.
            GlobalPageMetadata.getInstance().doAddOrQueue(db, pageMetadataClient, "https://mozilla.org", true, "{type: 'article', description: 'test article', image_url: 'https://example.com/test.png'}");
            assertPageMetadataValues(pageMetadataClient, "guid1", true, "{\"type\":\"article\",\"description\":\"test article\",\"image_url\":\"https:\\/\\/example.com\\/test.png\"}");

            // Insert another history item...
            cv = new ContentValues();
            cv.put(BrowserContract.History.GUID, "guid2");
            cv.put(BrowserContract.History.URL, "https://planet.mozilla.org");
            historyClient.insert(BrowserContract.History.CONTENT_URI, cv);
            // Test that empty metadata doesn't get inserted for a new history.
            GlobalPageMetadata.getInstance().doAddOrQueue(db, pageMetadataClient, "https://planet.mozilla.org", false, "{}");

            assertPageMetadataCountForGUID(0, "guid2", pageMetadataClient);

        } finally {
            provider.shutdown();
        }
    }

    /**
     * Expects cursor to be at the correct position.
     */
    private void assertCursorValues(Cursor cursor, String json, int hasImage, String guid) {
        assertNotNull(cursor);
        assertEquals(json, cursor.getString(cursor.getColumnIndexOrThrow(PageMetadata.JSON)));
        assertEquals(hasImage, cursor.getInt(cursor.getColumnIndexOrThrow(PageMetadata.HAS_IMAGE)));
        assertEquals(guid, cursor.getString(cursor.getColumnIndexOrThrow(PageMetadata.HISTORY_GUID)));
    }

    private void assertPageMetadataValues(ContentProviderClient client, String guid, boolean hasImage, String json) {
        final Cursor cursor;

        try {
            cursor = client.query(PageMetadata.CONTENT_URI, new String[]{
                    PageMetadata.HISTORY_GUID,
                    PageMetadata.HAS_IMAGE,
                    PageMetadata.JSON,
                    PageMetadata.DATE_CREATED
            }, PageMetadata.HISTORY_GUID + " = ?", new String[]{guid}, null);
        } catch (RemoteException e) {
            fail();
            return;
        }

        assertNotNull(cursor);
        try {
            assertEquals(1, cursor.getCount());
            assertTrue(cursor.moveToFirst());
            assertCursorValues(cursor, json, hasImage ? 1 : 0, guid);
        } finally {
            cursor.close();
        }
    }

    private void assertPageMetadataCountForGUID(int expected, String guid, ContentProviderClient client) {
        final Cursor cursor;

        try {
            cursor = client.query(PageMetadata.CONTENT_URI, new String[]{
                    PageMetadata.HISTORY_GUID,
                    PageMetadata.HAS_IMAGE,
                    PageMetadata.JSON,
                    PageMetadata.DATE_CREATED
            }, PageMetadata.HISTORY_GUID + " = ?", new String[]{guid}, null);
        } catch (RemoteException e) {
            fail();
            return;
        }

        assertNotNull(cursor);
        try {
            assertEquals(expected, cursor.getCount());
        } finally {
            cursor.close();
        }
    }
}
