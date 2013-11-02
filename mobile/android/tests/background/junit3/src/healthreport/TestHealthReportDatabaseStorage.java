/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport;

import java.util.ArrayList;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.healthreport.HealthReportStorage;
import org.mozilla.gecko.background.healthreport.HealthReportStorage.Field;
import org.mozilla.gecko.background.healthreport.HealthReportStorage.MeasurementFields;
import org.mozilla.gecko.background.healthreport.MockHealthReportDatabaseStorage.PrepopulatedMockHealthReportDatabaseStorage;
import org.mozilla.gecko.background.helpers.DBHelpers;
import org.mozilla.gecko.background.helpers.FakeProfileTestCase;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteConstraintException;
import android.database.sqlite.SQLiteDatabase;

public class TestHealthReportDatabaseStorage extends FakeProfileTestCase {
  private String[] TABLE_NAMES = {
    "addons",
    "environments",
    "measurements",
    "fields",
    "events_integer",
    "events_textual"
  };

  public static class MockMeasurementFields implements MeasurementFields {
    @Override
    public Iterable<FieldSpec> getFields() {
      ArrayList<FieldSpec> fields = new ArrayList<FieldSpec>();
      fields.add(new FieldSpec("testfield1", Field.TYPE_INTEGER_COUNTER));
      fields.add(new FieldSpec("testfield2", Field.TYPE_INTEGER_COUNTER));
      return fields;
    }
  }

  public void testInitializingProvider() {
    MockHealthReportDatabaseStorage storage = new MockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    storage.beginInitialization();

    // Two providers with the same measurement and field names. Shouldn't conflict.
    storage.ensureMeasurementInitialized("testpA.testm", 1, new MockMeasurementFields());
    storage.ensureMeasurementInitialized("testpB.testm", 2, new MockMeasurementFields());
    storage.finishInitialization();

    // Now make sure our stuff is in the DB.
    SQLiteDatabase db = storage.getDB();
    Cursor c = db.query("measurements", new String[] {"id", "name", "version"}, null, null, null, null, "name");
    assertTrue(c.moveToFirst());
    assertEquals(2, c.getCount());

    Object[][] expected = new Object[][] {
        {null, "testpA.testm", 1},
        {null, "testpB.testm", 2},
    };

    DBHelpers.assertCursorContains(expected, c);
    c.close();
  }

  private static final JSONObject EXAMPLE_ADDONS = safeJSONObject(
            "{ " +
            "\"amznUWL2@amazon.com\": { " +
            "  \"userDisabled\": false, " +
            "  \"appDisabled\": false, " +
            "  \"version\": \"1.10\", " +
            "  \"type\": \"extension\", " +
            "  \"scope\": 1, " +
            "  \"foreignInstall\": false, " +
            "  \"hasBinaryComponents\": false, " +
            "  \"installDay\": 15269, " +
            "  \"updateDay\": 15602 " +
            "}, " +
            "\"jid0-qBnIpLfDFa4LpdrjhAC6vBqN20Q@jetpack\": { " +
            "  \"userDisabled\": false, " +
            "  \"appDisabled\": false, " +
            "  \"version\": \"1.12.1\", " +
            "  \"type\": \"extension\", " +
            "  \"scope\": 1, " +
            "  \"foreignInstall\": false, " +
            "  \"hasBinaryComponents\": false, " +
            "  \"installDay\": 15062, " +
            "  \"updateDay\": 15580 " +
            "} " +
            "} ");

  private static JSONObject safeJSONObject(String s) {
    try {
      return new JSONObject(s);
    } catch (JSONException e) {
      return null;
    }
  }

  public void testEnvironmentsAndFields() throws Exception {
    MockHealthReportDatabaseStorage storage = new MockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    storage.beginInitialization();
    storage.ensureMeasurementInitialized("testpA.testm", 1, new MockMeasurementFields());
    storage.ensureMeasurementInitialized("testpB.testn", 1, new MockMeasurementFields());
    storage.finishInitialization();

    MockDatabaseEnvironment environmentA = storage.getEnvironment();
    environmentA.mockInit("v123");
    environmentA.setJSONForAddons(EXAMPLE_ADDONS);
    final int envA = environmentA.register();
    assertEquals(envA, environmentA.register());

    // getField memoizes.
    assertSame(storage.getField("foo", 2, "bar"),
               storage.getField("foo", 2, "bar"));

    // It throws if you refer to a non-existent field.
    try {
      storage.getField("foo", 2, "bar").getID();
      fail("Should throw.");
    } catch (IllegalStateException ex) {
      // Expected.
    }

    // It returns the field ID for a valid field.
    Field field = storage.getField("testpA.testm", 1, "testfield1");
    assertTrue(field.getID() >= 0);

    // These IDs are stable.
    assertEquals(field.getID(), field.getID());
    int fieldID = field.getID();

    // Before inserting, no events.
    assertFalse(storage.hasEventSince(0));
    assertFalse(storage.hasEventSince(storage.now));

    // Store some data for two environments across two days.
    storage.incrementDailyCount(envA, storage.getYesterday(), fieldID, 4);
    storage.incrementDailyCount(envA, storage.getYesterday(), fieldID, 1);
    storage.incrementDailyCount(envA, storage.getToday(), fieldID, 2);

    // After inserting, we have events.
    assertTrue(storage.hasEventSince(storage.now - GlobalConstants.MILLISECONDS_PER_DAY));
    assertTrue(storage.hasEventSince(storage.now));
    // But not in the future.
    assertFalse(storage.hasEventSince(storage.now + GlobalConstants.MILLISECONDS_PER_DAY));

    MockDatabaseEnvironment environmentB = storage.getEnvironment();
    environmentB.mockInit("v234");
    environmentB.setJSONForAddons(EXAMPLE_ADDONS);
    final int envB = environmentB.register();
    assertFalse(envA == envB);

    storage.incrementDailyCount(envB, storage.getToday(), fieldID, 6);
    storage.incrementDailyCount(envB, storage.getToday(), fieldID, 2);

    // Let's make sure everything's there.
    Cursor c = storage.getRawEventsSince(storage.getOneDayAgo());
    try {
      assertTrue(c.moveToFirst());
      assertTrue(assertRowEquals(c, storage.getYesterday(), envA, fieldID, 5));
      assertTrue(assertRowEquals(c, storage.getToday(), envA, fieldID, 2));
      assertFalse(assertRowEquals(c, storage.getToday(), envB, fieldID, 8));
    } finally {
      c.close();
    }

    // The stored environment has the provided JSON add-ons bundle.
    Cursor e = storage.getEnvironmentRecordForID(envA);
    e.moveToFirst();
    assertEquals(EXAMPLE_ADDONS.toString(), e.getString(e.getColumnIndex("addonsBody")));
    e.close();

    e = storage.getEnvironmentRecordForID(envB);
    e.moveToFirst();
    assertEquals(EXAMPLE_ADDONS.toString(), e.getString(e.getColumnIndex("addonsBody")));
    e.close();

    // There's only one add-ons bundle in the DB, despite having two environments.
    Cursor addons = storage.getDB().query("addons", null, null, null, null, null, null);
    assertEquals(1, addons.getCount());
    addons.close();
  }

  /**
   * Asserts validity for a storage cursor. Returns whether there is another row to process.
   */
  private static boolean assertRowEquals(Cursor c, int day, int env, int field, int value) {
    assertEquals(day,   c.getInt(0));
    assertEquals(env,   c.getInt(1));
    assertEquals(field, c.getInt(2));
    assertEquals(value, c.getLong(3));
    return c.moveToNext();
  }

  /**
   * Test robust insertions. This also acts as a test for the getPrepopulatedStorage method,
   * allowing faster debugging if this fails and other tests relying on getPrepopulatedStorage
   * also fail.
   */
  public void testInsertions() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    assertNotNull(storage);
  }

  public void testForeignKeyConstraints() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    final SQLiteDatabase db = storage.getDB();

    final int envID = storage.getEnvironment().register();
    final int counterFieldID = storage.getField(storage.measurementNames[0], storage.measurementVers[0],
        storage.fieldSpecContainers[0].counter.name).getID();
    final int discreteFieldID = storage.getField(storage.measurementNames[0], storage.measurementVers[0],
        storage.fieldSpecContainers[0].discrete.name).getID();

    final int nonExistentEnvID = DBHelpers.getNonExistentID(db, "environments");
    final int nonExistentFieldID = DBHelpers.getNonExistentID(db, "fields");
    final int nonExistentAddonID = DBHelpers.getNonExistentID(db, "addons");
    final int nonExistentMeasurementID = DBHelpers.getNonExistentID(db, "measurements");

    ContentValues v = new ContentValues();
    v.put("field", counterFieldID);
    v.put("env", nonExistentEnvID);
    try {
      db.insertOrThrow("events_integer", null, v);
      fail("Should throw - events_integer(env) is referencing non-existent environments(id)");
    } catch (SQLiteConstraintException e) { }
    v.put("field", discreteFieldID);
    try {
      db.insertOrThrow("events_textual", null, v);
      fail("Should throw - events_textual(env) is referencing non-existent environments(id)");
    } catch (SQLiteConstraintException e) { }

    v.put("field", nonExistentFieldID);
    v.put("env", envID);
    try {
      db.insertOrThrow("events_integer", null, v);
      fail("Should throw - events_integer(field) is referencing non-existent fields(id)");
    } catch (SQLiteConstraintException e) { }
    try {
      db.insertOrThrow("events_textual", null, v);
      fail("Should throw - events_textual(field) is referencing non-existent fields(id)");
    } catch (SQLiteConstraintException e) { }

    v = new ContentValues();
    v.put("addonsID", nonExistentAddonID);
    try {
      db.insertOrThrow("environments", null, v);
      fail("Should throw - environments(addonsID) is referencing non-existent addons(id).");
    } catch (SQLiteConstraintException e) { }

    v = new ContentValues();
    v.put("measurement", nonExistentMeasurementID);
    try {
      db.insertOrThrow("fields", null, v);
      fail("Should throw - fields(measurement) is referencing non-existent measurements(id).");
    } catch (SQLiteConstraintException e) { }
  }

  private int getTotalEventCount(HealthReportStorage storage) {
    final Cursor c = storage.getEventsSince(0);
    try {
      return c.getCount();
    } finally {
      c.close();
    }
  }

  public void testCascadingDeletions() throws Exception {
    PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    SQLiteDatabase db = storage.getDB();
    db.delete("environments", null, null);
    assertEquals(0, DBHelpers.getRowCount(db, "events_integer"));
    assertEquals(0, DBHelpers.getRowCount(db, "events_textual"));

    storage = new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    db = storage.getDB();
    db.delete("measurements", null, null);
    assertEquals(0, DBHelpers.getRowCount(db, "fields"));
    assertEquals(0, DBHelpers.getRowCount(db, "events_integer"));
    assertEquals(0, DBHelpers.getRowCount(db, "events_textual"));
  }

  public void testRestrictedDeletions() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    SQLiteDatabase db = storage.getDB();
    try {
      db.delete("addons", null, null);
      fail("Should throw - environment references addons and thus addons cannot be deleted.");
    } catch (SQLiteConstraintException e) { }
  }

  public void testDeleteEverything() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    storage.deleteEverything();

    final SQLiteDatabase db = storage.getDB();
    for (String table : TABLE_NAMES) {
      if (DBHelpers.getRowCount(db, table) != 0) {
        fail("Not everything has been deleted for table " + table + ".");
      }
    }
  }

  public void testMeasurementRecordingConstraintViolation() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    final SQLiteDatabase db = storage.getDB();

    final int envID = storage.getEnvironment().register();
    final int counterFieldID = storage.getField(storage.measurementNames[0], storage.measurementVers[0],
        storage.fieldSpecContainers[0].counter.name).getID();
    final int discreteFieldID = storage.getField(storage.measurementNames[0], storage.measurementVers[0],
        storage.fieldSpecContainers[0].discrete.name).getID();

    final int nonExistentEnvID = DBHelpers.getNonExistentID(db, "environments");
    final int nonExistentFieldID = DBHelpers.getNonExistentID(db, "fields");

    try {
      storage.incrementDailyCount(nonExistentEnvID, storage.getToday(), counterFieldID);
      fail("Should throw - event_integer(env) references environments(id), which is given as a non-existent value.");
    } catch (IllegalStateException e) { }
    try {
      storage.recordDailyDiscrete(nonExistentEnvID, storage.getToday(), discreteFieldID, "iu");
      fail("Should throw - event_textual(env) references environments(id), which is given as a non-existent value.");
    } catch (IllegalStateException e) { }
    try {
      storage.recordDailyLast(nonExistentEnvID, storage.getToday(), discreteFieldID, "iu");
      fail("Should throw - event_textual(env) references environments(id), which is given as a non-existent value.");
    } catch (IllegalStateException e) { }

    try {
      storage.incrementDailyCount(envID, storage.getToday(), nonExistentFieldID);
      fail("Should throw - event_integer(field) references fields(id), which is given as a non-existent value.");
    } catch (IllegalStateException e) { }
    try {
      storage.recordDailyDiscrete(envID, storage.getToday(), nonExistentFieldID, "iu");
      fail("Should throw - event_textual(field) references fields(id), which is given as a non-existent value.");
    } catch (IllegalStateException e) { }
    try {
      storage.recordDailyLast(envID, storage.getToday(), nonExistentFieldID, "iu");
      fail("Should throw - event_textual(field) references fields(id), which is given as a non-existent value.");
    } catch (IllegalStateException e) { }
  }

  // Largely taken from testDeleteEnvAndEventsBefore and testDeleteOrphanedAddons.
  public void testDeleteDataBefore() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    final SQLiteDatabase db = storage.getDB();

    // Insert (and delete) an environment not referenced by any events.
    ContentValues v = new ContentValues();
    v.put("hash", "I really hope this is a unique hash! ^_^");
    v.put("addonsID", DBHelpers.getExistentID(db, "addons"));
    db.insertOrThrow("environments", null, v);
    v.put("hash", "Another unique hash!");
    final int curEnv = (int) db.insertOrThrow("environments", null, v);
    final ContentValues addonV = new ContentValues();
    addonV.put("body", "addon1");
    db.insertOrThrow("addons", null, addonV);
    // 2 = 1 addon + 1 env.
    assertEquals(2, storage.deleteDataBefore(storage.getGivenDaysAgoMillis(8), curEnv));
    assertEquals(1, storage.deleteDataBefore(storage.getGivenDaysAgoMillis(8),
          DBHelpers.getNonExistentID(db, "environments")));
    assertEquals(1, DBHelpers.getRowCount(db, "addons"));

    // Insert (and delete) new environment and referencing events.
    final long envID = db.insertOrThrow("environments", null, v);
    v = new ContentValues();
    v.put("date", storage.getGivenDaysAgo(9));
    v.put("env", envID);
    v.put("field", DBHelpers.getExistentID(db, "fields"));
    db.insertOrThrow("events_integer", null, v);
    db.insertOrThrow("events_integer", null, v);
    assertEquals(16, getTotalEventCount(storage));
    final int nonExistentEnvID = DBHelpers.getNonExistentID(db, "environments");
    assertEquals(1, storage.deleteDataBefore(storage.getGivenDaysAgoMillis(8), nonExistentEnvID));
    assertEquals(14, getTotalEventCount(storage));

    // Assert only pre-populated storage is stored.
    assertEquals(1, DBHelpers.getRowCount(db, "environments"));

    assertEquals(0, storage.deleteDataBefore(storage.getGivenDaysAgoMillis(5), nonExistentEnvID));
    assertEquals(12, getTotalEventCount(storage));

    assertEquals(0, storage.deleteDataBefore(storage.getGivenDaysAgoMillis(4), nonExistentEnvID));
    assertEquals(10, getTotalEventCount(storage));

    assertEquals(0, storage.deleteDataBefore(storage.now, nonExistentEnvID));
    assertEquals(5, getTotalEventCount(storage));
    assertEquals(1, DBHelpers.getRowCount(db, "addons"));

    // 2 = 1 addon + 1 env.
    assertEquals(2, storage.deleteDataBefore(storage.now + GlobalConstants.MILLISECONDS_PER_DAY,
          nonExistentEnvID));
    assertEquals(0, getTotalEventCount(storage));
    assertEquals(0, DBHelpers.getRowCount(db, "addons"));
  }

  // Largely taken from testDeleteOrphanedEnv and testDeleteEventsBefore.
  public void testDeleteEnvAndEventsBefore() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    final SQLiteDatabase db = storage.getDB();

    // Insert (and delete) an environment not referenced by any events.
    ContentValues v = new ContentValues();
    v.put("hash", "I really hope this is a unique hash! ^_^");
    v.put("addonsID", DBHelpers.getExistentID(db, "addons"));
    db.insertOrThrow("environments", null, v);
    v.put("hash", "Another unique hash!");
    final int curEnv = (int) db.insertOrThrow("environments", null, v);
    assertEquals(1, storage.deleteEnvAndEventsBefore(storage.getGivenDaysAgoMillis(8), curEnv));
    assertEquals(1, storage.deleteEnvAndEventsBefore(storage.getGivenDaysAgoMillis(8),
          DBHelpers.getNonExistentID(db, "environments")));

    // Insert (and delete) new environment and referencing events.
    final long envID = db.insertOrThrow("environments", null, v);
    v = new ContentValues();
    v.put("date", storage.getGivenDaysAgo(9));
    v.put("env", envID);
    v.put("field", DBHelpers.getExistentID(db, "fields"));
    db.insertOrThrow("events_integer", null, v);
    db.insertOrThrow("events_integer", null, v);
    assertEquals(16, getTotalEventCount(storage));
    final int nonExistentEnvID = DBHelpers.getNonExistentID(db, "environments");
    assertEquals(1, storage.deleteEnvAndEventsBefore(storage.getGivenDaysAgoMillis(8), nonExistentEnvID));
    assertEquals(14, getTotalEventCount(storage));

    // Assert only pre-populated storage is stored.
    assertEquals(1, DBHelpers.getRowCount(db, "environments"));

    assertEquals(0, storage.deleteEnvAndEventsBefore(storage.getGivenDaysAgoMillis(5), nonExistentEnvID));
    assertEquals(12, getTotalEventCount(storage));

    assertEquals(0, storage.deleteEnvAndEventsBefore(storage.getGivenDaysAgoMillis(4), nonExistentEnvID));
    assertEquals(10, getTotalEventCount(storage));

    assertEquals(0, storage.deleteEnvAndEventsBefore(storage.now, nonExistentEnvID));
    assertEquals(5, getTotalEventCount(storage));

    assertEquals(1, storage.deleteEnvAndEventsBefore(storage.now + GlobalConstants.MILLISECONDS_PER_DAY,
          nonExistentEnvID));
    assertEquals(0, getTotalEventCount(storage));
  }

  public void testDeleteOrphanedEnv() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    final SQLiteDatabase db = storage.getDB();

    final ContentValues v = new ContentValues();
    v.put("addonsID", DBHelpers.getExistentID(db, "addons"));
    v.put("hash", "unique");
    final int envID = (int) db.insert("environments", null, v);

    assertEquals(0, storage.deleteOrphanedEnv(envID));
    assertEquals(1, storage.deleteOrphanedEnv(storage.env));
    this.deleteEvents(db);
    assertEquals(1, storage.deleteOrphanedEnv(envID));
  }

  private void deleteEvents(final SQLiteDatabase db) throws Exception {
    db.beginTransaction();
    try {
      db.delete("events_integer", null, null);
      db.delete("events_textual", null, null);
      db.setTransactionSuccessful();
    } finally {
      db.endTransaction();
    }
  }

  public void testDeleteEventsBefore() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    assertEquals(2, storage.deleteEventsBefore(Integer.toString(storage.getGivenDaysAgo(5))));
    assertEquals(12, getTotalEventCount(storage));

    assertEquals(2, storage.deleteEventsBefore(Integer.toString(storage.getGivenDaysAgo(4))));
    assertEquals(10, getTotalEventCount(storage));

    assertEquals(5, storage.deleteEventsBefore(Integer.toString(storage.getToday())));
    assertEquals(5, getTotalEventCount(storage));

    assertEquals(5, storage.deleteEventsBefore(Integer.toString(storage.getTomorrow())));
    assertEquals(0, getTotalEventCount(storage));
  }

  public void testDeleteOrphanedAddons() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    final SQLiteDatabase db = storage.getDB();

    final ArrayList<Integer> nonOrphanIDs = new ArrayList<Integer>();
    final Cursor c = db.query("addons", new String[] {"id"}, null, null, null, null, null);
    try {
      assertTrue(c.moveToFirst());
      do {
        nonOrphanIDs.add(c.getInt(0));
      } while (c.moveToNext());
    } finally {
      c.close();
    }

    // Ensure we don't delete non-orphans.
    assertEquals(0, storage.deleteOrphanedAddons());

    // Insert orphans.
    final long[] orphanIDs = new long[2];
    final ContentValues v = new ContentValues();
    v.put("body", "addon1");
    orphanIDs[0] = db.insertOrThrow("addons", null, v);
    v.put("body", "addon2");
    orphanIDs[1] = db.insertOrThrow("addons", null, v);
    assertEquals(2, storage.deleteOrphanedAddons());
    assertEquals(0, DBHelpers.getRowCount(db, "addons", "ID = ? OR ID = ?",
        new String[] {Long.toString(orphanIDs[0]), Long.toString(orphanIDs[1])}));

    // Orphan all addons.
    db.delete("environments", null, null);
    assertEquals(nonOrphanIDs.size(), storage.deleteOrphanedAddons());
    assertEquals(0, DBHelpers.getRowCount(db, "addons"));
  }

  public void testGetEventCount() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    assertEquals(14, storage.getEventCount());
    final SQLiteDatabase db = storage.getDB();
    this.deleteEvents(db);
    assertEquals(0, storage.getEventCount());
  }

  public void testGetEnvironmentCount() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    assertEquals(1, storage.getEnvironmentCount());
    final SQLiteDatabase db = storage.getDB();
    db.delete("environments", null, null);
    assertEquals(0, storage.getEnvironmentCount());
  }

  public void testPruneEnvironments() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory, 2);
    final SQLiteDatabase db = storage.getDB();
    assertEquals(5, DBHelpers.getRowCount(db, "environments"));
    storage.pruneEnvironments(1);
    assertTrue(!getEnvAppVersions(db).contains("v3"));
    storage.pruneEnvironments(2);
    assertTrue(!getEnvAppVersions(db).contains("v2"));
    assertTrue(!getEnvAppVersions(db).contains("v1"));
    storage.pruneEnvironments(1);
    assertTrue(!getEnvAppVersions(db).contains("v123"));
    storage.pruneEnvironments(1);
    assertTrue(!getEnvAppVersions(db).contains("v4"));
  }

  private ArrayList<String> getEnvAppVersions(final SQLiteDatabase db) {
    ArrayList<String> out = new ArrayList<String>();
    Cursor c = null;
    try {
      c = db.query(true, "environments", new String[] {"appVersion"}, null, null, null, null, null, null);
      while (c.moveToNext()) {
        out.add(c.getString(0));
      }
    } finally {
      if (c != null) {
        c.close();
      }
    }
    return out;
  }

  public void testPruneEvents() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    SQLiteDatabase db = storage.getDB();
    assertEquals(14, DBHelpers.getRowCount(db, "events"));
    storage.pruneEvents(1); // Delete < 7 days ago.
    assertEquals(14, DBHelpers.getRowCount(db, "events"));
    storage.pruneEvents(2); // Delete < 5 days ago.
    assertEquals(13, DBHelpers.getRowCount(db, "events"));
    storage.pruneEvents(5); // Delete < 2 days ago.
    assertEquals(9, DBHelpers.getRowCount(db, "events"));
    storage.pruneEvents(14); // Delete < today.
    assertEquals(5, DBHelpers.getRowCount(db, "events"));
  }

  public void testVacuum() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    final SQLiteDatabase db = storage.getDB();
    // Need to disable auto_vacuum to allow free page fragmentation. Note that the pragma changes
    // only after a vacuum command.
    db.execSQL("PRAGMA auto_vacuum=0");
    db.execSQL("vacuum");
    assertTrue(isAutoVacuumingDisabled(storage));

    createFreePages(storage);
    storage.vacuum();
    assertEquals(0, getFreelistCount(storage));
  }

  public long getFreelistCount(final MockHealthReportDatabaseStorage storage) {
    return storage.getIntFromQuery("PRAGMA freelist_count", null);
  }

  public boolean isAutoVacuumingDisabled(final MockHealthReportDatabaseStorage storage) {
    return storage.getIntFromQuery("PRAGMA auto_vacuum", null) == 0;
  }

  private void createFreePages(final PrepopulatedMockHealthReportDatabaseStorage storage) throws Exception {
    // Insert and delete until DB has free page fragmentation. The loop helps ensure that the
    // fragmentation will occur with minimal disk usage. The upper loop limits are arbitrary.
    final SQLiteDatabase db = storage.getDB();
    for (int i = 10; i <= 1250; i *= 5) {
      storage.insertTextualEvents(i);
      db.delete("events_textual", "date < ?", new String[] {Integer.toString(i / 2)});
      if (getFreelistCount(storage) > 0) {
        return;
      }
    }
    fail("Database free pages failed to fragment.");
  }

  public void testDisableAutoVacuuming() throws Exception {
    final PrepopulatedMockHealthReportDatabaseStorage storage =
        new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    final SQLiteDatabase db = storage.getDB();
    // The pragma changes only after a vacuum command.
    db.execSQL("PRAGMA auto_vacuum=1");
    db.execSQL("vacuum");
    assertEquals(1, storage.getIntFromQuery("PRAGMA auto_vacuum", null));
    storage.disableAutoVacuuming();
    db.execSQL("vacuum");
    assertTrue(isAutoVacuumingDisabled(storage));
  }
}
