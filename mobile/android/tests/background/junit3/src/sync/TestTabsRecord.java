/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync;

import org.mozilla.gecko.background.db.CursorDumper;
import org.mozilla.gecko.background.db.TestFennecTabsStorage;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.mozilla.gecko.sync.repositories.android.FennecTabsRepository;
import org.mozilla.gecko.sync.repositories.domain.TabsRecord;

import android.content.ContentProviderClient;
import android.database.Cursor;

public class TestTabsRecord extends TestFennecTabsStorage {
  public void testTabsRecordFromCursor() throws Exception {
    final ContentProviderClient tabsClient = getTabsClient();

    deleteAllTestTabs(tabsClient);
    insertSomeTestTabs(tabsClient);

    final String positionAscending = BrowserContract.Tabs.POSITION + " ASC";
    Cursor cursor = null;
    try {
      cursor = tabsClient.query(BrowserContractHelpers.TABS_CONTENT_URI, null, TABS_CLIENT_GUID_IS, new String[] { TEST_CLIENT_GUID }, positionAscending);
      assertEquals(3, cursor.getCount());

      cursor.moveToPosition(1);

      final TabsRecord tabsRecord = FennecTabsRepository.tabsRecordFromCursor(cursor, TEST_CLIENT_GUID, TEST_CLIENT_NAME);

      // Make sure we clean up after ourselves.
      assertEquals(1, cursor.getPosition());

      assertEquals(TEST_CLIENT_GUID, tabsRecord.guid);
      assertEquals(TEST_CLIENT_NAME, tabsRecord.clientName);

      assertEquals(3, tabsRecord.tabs.size());
      assertEquals(testTab1, tabsRecord.tabs.get(0));
      assertEquals(testTab2, tabsRecord.tabs.get(1));
      assertEquals(testTab3, tabsRecord.tabs.get(2));

      assertEquals(Math.max(Math.max(testTab1.lastUsed, testTab2.lastUsed), testTab3.lastUsed), tabsRecord.lastModified);
    } finally {
      cursor.close();
    }
  }

  // Verify that we can fetch a record when there are no local tabs at all.
  public void testEmptyTabsRecordFromCursor() throws Exception {
    final ContentProviderClient tabsClient = getTabsClient();

    deleteAllTestTabs(tabsClient);

    final String positionAscending = BrowserContract.Tabs.POSITION + " ASC";
    Cursor cursor = null;
    try {
      cursor = tabsClient.query(BrowserContractHelpers.TABS_CONTENT_URI, null, TABS_CLIENT_GUID_IS, new String[] { TEST_CLIENT_GUID }, positionAscending);
      assertEquals(0, cursor.getCount());

      final TabsRecord tabsRecord = FennecTabsRepository.tabsRecordFromCursor(cursor, TEST_CLIENT_GUID, TEST_CLIENT_NAME);

      assertEquals(TEST_CLIENT_GUID, tabsRecord.guid);
      assertEquals(TEST_CLIENT_NAME, tabsRecord.clientName);

      assertNotNull(tabsRecord.tabs);
      assertEquals(0, tabsRecord.tabs.size());

      assertEquals(0, tabsRecord.lastModified);
    } finally {
      cursor.close();
    }
  }

  // Not much of a test, but verifies the tabs record at least agrees with the
  // disk data and doubles as a database inspector.
  public void testLocalTabs() throws Exception {
    final ContentProviderClient tabsClient = getTabsClient();

    final String positionAscending = BrowserContract.Tabs.POSITION + " ASC";
    Cursor cursor = null;
    try {
      // Keep this in sync with the Fennec schema.
      cursor = tabsClient.query(BrowserContractHelpers.TABS_CONTENT_URI, null, BrowserContract.Tabs.CLIENT_GUID + " IS NULL", null, positionAscending);
      CursorDumper.dumpCursor(cursor);

      final TabsRecord tabsRecord = FennecTabsRepository.tabsRecordFromCursor(cursor, TEST_CLIENT_GUID, TEST_CLIENT_NAME);

      assertEquals(TEST_CLIENT_GUID, tabsRecord.guid);
      assertEquals(TEST_CLIENT_NAME, tabsRecord.clientName);

      assertNotNull(tabsRecord.tabs);
      assertEquals(cursor.getCount(), tabsRecord.tabs.size());
    } finally {
      cursor.close();
    }
  }
}
