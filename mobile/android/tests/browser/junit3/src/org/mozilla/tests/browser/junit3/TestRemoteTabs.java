/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.tests.browser.junit3;

import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.test.InstrumentationTestCase;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.background.db.CursorDumper;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.LocalTabsAccessor;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;

import java.util.List;

public class TestRemoteTabs extends InstrumentationTestCase {
    private static final long ONE_DAY_IN_MILLISECONDS = 1000 * 60 * 60 * 24;
    private static final long ONE_WEEK_IN_MILLISECONDS = 7 * ONE_DAY_IN_MILLISECONDS;
    private static final long THREE_WEEKS_IN_MILLISECONDS = 3 * ONE_WEEK_IN_MILLISECONDS;

    public void testGetClientsWithoutTabsByRecencyFromCursor() throws Exception {
        final Uri uri = BrowserContractHelpers.CLIENTS_CONTENT_URI;
        final ContentResolver cr = getInstrumentation().getTargetContext().getContentResolver();
        final ContentProviderClient cpc = cr.acquireContentProviderClient(uri);
        final LocalTabsAccessor accessor = new LocalTabsAccessor("test"); // The profile name given doesn't matter.

        try {
            // Delete all tabs to begin with.
            cpc.delete(uri, null, null);
            Cursor allClients = cpc.query(uri, null, null, null, null);
            try {
                assertEquals(0, allClients.getCount());
            } finally {
                allClients.close();
            }

            // Insert a local and remote1 client record, neither with tabs.
            final long now = System.currentTimeMillis();
            // Local client has GUID = null.
            final ContentValues local = new ContentValues();
            local.put(BrowserContract.Clients.NAME, "local");
            local.put(BrowserContract.Clients.LAST_MODIFIED, now + 1);
            // Remote clients have GUID != null.
            final ContentValues remote1 = new ContentValues();
            remote1.put(BrowserContract.Clients.GUID, "guid1");
            remote1.put(BrowserContract.Clients.NAME, "remote1");
            remote1.put(BrowserContract.Clients.LAST_MODIFIED, now + 2);

            final ContentValues remote2 = new ContentValues();
            remote2.put(BrowserContract.Clients.GUID, "guid2");
            remote2.put(BrowserContract.Clients.NAME, "remote2");
            remote2.put(BrowserContract.Clients.LAST_MODIFIED, now + 3);

            ContentValues[] values = new ContentValues[]{local, remote1, remote2};
            int inserted = cpc.bulkInsert(uri, values);
            assertEquals(3, inserted);

            allClients = cpc.query(BrowserContract.Clients.CONTENT_RECENCY_URI, null, null, null, null);
            try {
                CursorDumper.dumpCursor(allClients);
                // The local client is not ignored.
                assertEquals(3, allClients.getCount());
                final List<RemoteClient> clients = accessor.getClientsWithoutTabsByRecencyFromCursor(allClients);
                assertEquals(3, clients.size());
                for (RemoteClient client : clients) {
                    // Each client should not have any tabs.
                    assertNotNull(client.tabs);
                    assertEquals(0, client.tabs.size());
                }
                // Since there are no tabs, the order should be based on last_modified.
                assertEquals("guid2", clients.get(0).guid);
                assertEquals("guid1", clients.get(1).guid);
                assertEquals(null, clients.get(2).guid);
            } finally {
                allClients.close();
            }

            // Now let's add a few tabs to one client.  The times are chosen so that one tab's
            // last used is not relevant, and the other tab is the most recent used.
            final ContentValues remoteTab1 = new ContentValues();
            remoteTab1.put(BrowserContract.Tabs.CLIENT_GUID, "guid1");
            remoteTab1.put(BrowserContract.Tabs.TITLE, "title1");
            remoteTab1.put(BrowserContract.Tabs.URL, "http://test.com/test1");
            remoteTab1.put(BrowserContract.Tabs.HISTORY, "[\"http://test.com/test1\"]");
            remoteTab1.put(BrowserContract.Tabs.LAST_USED, now);
            remoteTab1.put(BrowserContract.Tabs.POSITION, 0);

            final ContentValues remoteTab2 = new ContentValues();
            remoteTab2.put(BrowserContract.Tabs.CLIENT_GUID, "guid1");
            remoteTab2.put(BrowserContract.Tabs.TITLE, "title2");
            remoteTab2.put(BrowserContract.Tabs.URL, "http://test.com/test2");
            remoteTab2.put(BrowserContract.Tabs.HISTORY, "[\"http://test.com/test2\"]");
            remoteTab2.put(BrowserContract.Tabs.LAST_USED, now + 5);
            remoteTab2.put(BrowserContract.Tabs.POSITION, 1);

            values = new ContentValues[]{remoteTab1, remoteTab2};
            inserted = cpc.bulkInsert(BrowserContract.Tabs.CONTENT_URI, values);
            assertEquals(2, inserted);

            allClients = cpc.query(BrowserContract.Clients.CONTENT_RECENCY_URI, null, BrowserContract.Clients.GUID + " IS NOT NULL", null, null);
            try {
                CursorDumper.dumpCursor(allClients);
                // The local client is ignored.
                assertEquals(2, allClients.getCount());
                final List<RemoteClient> clients = accessor.getClientsWithoutTabsByRecencyFromCursor(allClients);
                assertEquals(2, clients.size());
                for (RemoteClient client : clients) {
                    // Each client should be remote and should not have any tabs.
                    assertNotNull(client.guid);
                    assertNotNull(client.tabs);
                    assertEquals(0, client.tabs.size());
                }
                // Since now there is a tab attached to the remote2 client more recent than the
                // remote1 client modified time, it should be first.
                assertEquals("guid1", clients.get(0).guid);
                assertEquals("guid2", clients.get(1).guid);
            } finally {
                allClients.close();
            }
        } finally {
            cpc.release();
        }
    }

    public void testGetRecentRemoteClientsUpToOneWeekOld() throws Exception {
        final Uri uri = BrowserContractHelpers.CLIENTS_CONTENT_URI;
        final Context context = getInstrumentation().getTargetContext();
        final String profileName = GeckoProfile.get(context).getName();
        final ContentResolver cr = context.getContentResolver();
        final ContentProviderClient cpc = cr.acquireContentProviderClient(uri);
        final LocalTabsAccessor accessor = new LocalTabsAccessor(profileName);

        try {
            // Start Clean
            cpc.delete(uri, null, null);
            final Cursor allClients = cpc.query(uri, null, null, null, null);
            try {
                assertEquals(0, allClients.getCount());
            } finally {
                allClients.close();
            }

            // Insert a local and remote1 client record, neither with tabs.
            final long now = System.currentTimeMillis();
            // Local client has GUID = null.
            final ContentValues local = new ContentValues();
            local.put(BrowserContract.Clients.NAME, "local");
            local.put(BrowserContract.Clients.LAST_MODIFIED, now + 1);
            // Remote clients have GUID != null.
            final ContentValues remote1 = new ContentValues();
            remote1.put(BrowserContract.Clients.GUID, "guid1");
            remote1.put(BrowserContract.Clients.NAME, "remote1");
            remote1.put(BrowserContract.Clients.LAST_MODIFIED, now + 2);

            // Insert a Remote Client that is 6 days old.
            final ContentValues remote2 = new ContentValues();
            remote2.put(BrowserContract.Clients.GUID, "guid2");
            remote2.put(BrowserContract.Clients.NAME, "remote2");
            remote2.put(BrowserContract.Clients.LAST_MODIFIED, now - ONE_WEEK_IN_MILLISECONDS + ONE_DAY_IN_MILLISECONDS);

            // Insert a Remote Client with the same name as previous but with more than 3 weeks old
            final ContentValues remote3 = new ContentValues();
            remote3.put(BrowserContract.Clients.GUID, "guid21");
            remote3.put(BrowserContract.Clients.NAME, "remote2");
            remote3.put(BrowserContract.Clients.LAST_MODIFIED, now - THREE_WEEKS_IN_MILLISECONDS - ONE_DAY_IN_MILLISECONDS);

            // Insert another remote client with the same name as previous but with 3 weeks - 1 day old.
            final ContentValues remote4 = new ContentValues();
            remote4.put(BrowserContract.Clients.GUID, "guid22");
            remote4.put(BrowserContract.Clients.NAME, "remote2");
            remote4.put(BrowserContract.Clients.LAST_MODIFIED, now - THREE_WEEKS_IN_MILLISECONDS + ONE_DAY_IN_MILLISECONDS);

            // Insert a Remote Client that is exactly one week old.
            final ContentValues remote5 = new ContentValues();
            remote5.put(BrowserContract.Clients.GUID, "guid3");
            remote5.put(BrowserContract.Clients.NAME, "remote3");
            remote5.put(BrowserContract.Clients.LAST_MODIFIED, now - ONE_WEEK_IN_MILLISECONDS);

            ContentValues[] values = new ContentValues[]{local, remote1, remote2, remote3, remote4, remote5};
            int inserted = cpc.bulkInsert(uri, values);
            assertEquals(values.length, inserted);

            final Cursor remoteClients =
                    accessor.getRemoteClientsByRecencyCursor(context);

            try {
                CursorDumper.dumpCursor(remoteClients);
                // Local client is not included.
                // (remote1, guid1), (remote2, guid2), (remote3, guid3) are expected.
                assertEquals(3, remoteClients.getCount());

                // Check the inner data, according to recency.
                List<RemoteClient> recentRemoteClientsList =
                        accessor.getClientsWithoutTabsByRecencyFromCursor(remoteClients);
                assertEquals(3, recentRemoteClientsList.size());
                assertEquals("remote1", recentRemoteClientsList.get(0).name);
                assertEquals("guid1", recentRemoteClientsList.get(0).guid);
                assertEquals("remote2", recentRemoteClientsList.get(1).name);
                assertEquals("guid2", recentRemoteClientsList.get(1).guid);
                assertEquals("remote3", recentRemoteClientsList.get(2).name);
                assertEquals("guid3", recentRemoteClientsList.get(2).guid);
            } finally {
                remoteClients.close();
            }
        } finally {
            cpc.release();
        }
    }
}
