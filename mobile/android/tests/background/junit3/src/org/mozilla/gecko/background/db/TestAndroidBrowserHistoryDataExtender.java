/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.db;

import java.io.IOException;
import java.util.ArrayList;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.json.simple.parser.ParseException;
import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.background.sync.helpers.HistoryHelpers;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonArrayJSONException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserHistoryDataExtender;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;

import android.database.Cursor;

public class TestAndroidBrowserHistoryDataExtender extends AndroidSyncTestCase {

  protected AndroidBrowserHistoryDataExtender extender;
  protected static final String LOG_TAG = "SyncHistoryVisitsTest";

  public void setUp() {
    extender = new AndroidBrowserHistoryDataExtender(getApplicationContext());
    extender.wipe();
  }

  public void tearDown() {
    extender.close();
  }

  public void testStoreFetch() throws NullCursorException, NonObjectJSONException, IOException, ParseException {
    String guid = Utils.generateGuid();
    extender.store(Utils.generateGuid(), null);
    extender.store(guid, null);
    extender.store(Utils.generateGuid(), null);

    Cursor cur = null;
    try {
      cur = extender.fetch(guid);
      assertEquals(1, cur.getCount());
      assertTrue(cur.moveToFirst());
      assertEquals(guid, cur.getString(0));
    } finally {
      if (cur != null) {
        cur.close();
      }
    }
  }

  public void testVisitsForGUID() throws NonArrayJSONException, NonObjectJSONException, IOException, ParseException, NullCursorException {
    String guid = Utils.generateGuid();
    JSONArray visits = new ExtendedJSONObject("{ \"visits\": [ { \"key\" : \"value\" } ] }").getArray("visits");

    extender.store(Utils.generateGuid(), null);
    extender.store(guid, visits);
    extender.store(Utils.generateGuid(), null);

    JSONArray fetchedVisits = extender.visitsForGUID(guid);
    assertEquals(1, fetchedVisits.size());
    assertEquals("value", ((JSONObject)fetchedVisits.get(0)).get("key"));
  }

  public void testDeleteHandlesBadGUIDs() {
    String evilGUID = "' or '1'='1";
    extender.store(Utils.generateGuid(), null);
    extender.store(Utils.generateGuid(), null);
    extender.store(evilGUID, null);
    extender.delete(evilGUID);

    Cursor cur = null;
    try {
      cur = extender.fetchAll();
      assertEquals(cur.getCount(), 2);
      assertTrue(cur.moveToFirst());
      while (!cur.isAfterLast()) {
        String guid = RepoUtils.getStringFromCursor(cur, AndroidBrowserHistoryDataExtender.COL_GUID);
        assertFalse(evilGUID.equals(guid));
        cur.moveToNext();
      }
    } catch (NullCursorException e) {
      e.printStackTrace();
      fail("Should not have null cursor.");
    } finally {
      if (cur != null) {
        cur.close();
      }
    }
  }

  public void testStoreFetchHandlesBadGUIDs() {
    String evilGUID = "' or '1'='1";
    extender.store(Utils.generateGuid(), null);
    extender.store(Utils.generateGuid(), null);
    extender.store(evilGUID, null);

    Cursor cur = null;
    try {
      cur = extender.fetch(evilGUID);
      assertEquals(1, cur.getCount());
      assertTrue(cur.moveToFirst());
      while (!cur.isAfterLast()) {
        String guid = RepoUtils.getStringFromCursor(cur, AndroidBrowserHistoryDataExtender.COL_GUID);
        assertEquals(evilGUID, guid);
        cur.moveToNext();
      }
    } catch (NullCursorException e) {
      e.printStackTrace();
      fail("Should not have null cursor.");
    } finally {
      if (cur != null) {
        cur.close();
      }
    }
  }

  public void testBulkInsert() throws NullCursorException {
    ArrayList<HistoryRecord> records = new ArrayList<HistoryRecord>();
    records.add(HistoryHelpers.createHistory1());
    records.add(HistoryHelpers.createHistory2());
    extender.bulkInsert(records);

    for (HistoryRecord record : records) {
      HistoryRecord toCompare = (HistoryRecord) record.copyWithIDs(record.guid, record.androidID);
      toCompare.visits = extender.visitsForGUID(record.guid);
      assertEquals(record.visits.size(), toCompare.visits.size());
      assertTrue(record.equals(toCompare));
    }

    // Now insert existing records, changing one, and add another record.
    records.get(0).title = "test";
    records.add(HistoryHelpers.createHistory3());
    extender.bulkInsert(records);

    for (HistoryRecord record : records) {
      HistoryRecord toCompare = (HistoryRecord) record.copyWithIDs(record.guid, record.androidID);
      toCompare.visits = extender.visitsForGUID(record.guid);
      assertEquals(record.visits.size(), toCompare.visits.size());
      assertTrue(record.equals(toCompare));
    }
  }
}
