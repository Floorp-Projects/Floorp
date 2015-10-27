/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport;

import org.mozilla.gecko.background.helpers.DBHelpers;
import org.mozilla.gecko.background.helpers.FakeProfileTestCase;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteException;

public class TestHealthReportSQLiteOpenHelper extends FakeProfileTestCase {
  private MockHealthReportSQLiteOpenHelper helper;

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    helper = null;
  }

  @Override
  protected void tearDown() throws Exception {
    if (helper != null) {
      helper.close();
      helper = null;
    }
    super.tearDown();
  }

  private MockHealthReportSQLiteOpenHelper createHelper(String name) {
    return new MockHealthReportSQLiteOpenHelper(context, fakeProfileDirectory, name);
  }

  private MockHealthReportSQLiteOpenHelper createHelper(String name, int version) {
    return new MockHealthReportSQLiteOpenHelper(context, fakeProfileDirectory, name, version);
  }

  public void testOpening() {
    helper = createHelper("health.db");
    SQLiteDatabase db = helper.getWritableDatabase();
    assertTrue(db.isOpen());
    db.beginTransaction();
    db.setTransactionSuccessful();
    db.endTransaction();
    helper.close();
    assertFalse(db.isOpen());
  }

  private void assertEmptyTable(SQLiteDatabase db, String table, String column) {
    Cursor c = db.query(table, new String[] { column },
                        null, null, null, null, null);
    assertNotNull(c);
    try {
      assertFalse(c.moveToFirst());
    } finally {
      c.close();
    }
  }

  public void testInit() {
    helper = createHelper("health-" + System.currentTimeMillis() + ".db");
    SQLiteDatabase db = helper.getWritableDatabase();
    assertTrue(db.isOpen());

    db.beginTransaction();
    try {
      // DB starts empty with correct tables.
      assertEmptyTable(db, "fields", "name");
      assertEmptyTable(db, "measurements", "name");
      assertEmptyTable(db, "events_textual", "field");
      assertEmptyTable(db, "events_integer", "field");
      assertEmptyTable(db, "events", "field");

      // Throws for tables that don't exist.
      try {
        assertEmptyTable(db, "foobarbaz", "name");
      } catch (SQLiteException e) {
        // Expected.
      }
      db.setTransactionSuccessful();
    } finally {
      db.endTransaction();
    }
  }

  public void testUpgradeDatabaseFrom4To5() throws Exception {
    final String dbName = "health-4To5.db";
    helper = createHelper(dbName, 4);
    SQLiteDatabase db = helper.getWritableDatabase();
    db.beginTransaction();
    try {
      db.execSQL("PRAGMA foreign_keys=OFF;");

      // Despite being referenced, this addon should be deleted because it is NULL.
      ContentValues v = new ContentValues();
      v.put("body", (String) null);
      final long orphanedAddonID = db.insert("addons", null, v);
      v.put("body", "addon");
      final long addonID = db.insert("addons", null, v);

      // environments -> addons
      v = new ContentValues();
      v.put("hash", "orphanedEnv");
      v.put("addonsID", orphanedAddonID);
      final long orphanedEnvID = db.insert("environments", null, v);
      v.put("hash", "env");
      v.put("addonsID", addonID);
      final long envID = db.insert("environments", null, v);

      v = new ContentValues();
      v.put("name", "measurement");
      v.put("version", 1);
      final long measurementID = db.insert("measurements", null, v);

      // fields -> measurements
      v = new ContentValues();
      v.put("name", "orphanedField");
      v.put("measurement", DBHelpers.getNonExistentID(db, "measurements"));
      final long orphanedFieldID = db.insert("fields", null, v);
      v.put("name", "field");
      v.put("measurement", measurementID);
      final long fieldID = db.insert("fields", null, v);

      // events -> environments, fields
      final String[] eventTables = {"events_integer", "events_textual"};
      for (String table : eventTables) {
        v = new ContentValues();
        v.put("env", envID);
        v.put("field", fieldID);
        db.insert(table, null, v);

        v.put("env", orphanedEnvID);
        v.put("field", fieldID);
        db.insert(table, null, v);

        v.put("env", envID);
        v.put("field", orphanedFieldID);
        db.insert(table, null, v);

        v.put("env", orphanedEnvID);
        v.put("field", orphanedFieldID);
        db.insert(table, null, v);
      }

      db.setTransactionSuccessful();
    } finally {
      db.endTransaction();
      helper.close();
    }

    // Upgrade.
    helper = createHelper(dbName, 5);
    // Despite only reading from it, open a writable database so we can better replicate what
    // might happen in production (most notably, this should enable foreign keys).
    db = helper.getWritableDatabase();

    assertEquals(1, DBHelpers.getRowCount(db, "addons"));
    assertEquals(1, DBHelpers.getRowCount(db, "measurements"));
    assertEquals(1, DBHelpers.getRowCount(db, "fields"));
    assertEquals(1, DBHelpers.getRowCount(db, "events_integer"));
    assertEquals(1, DBHelpers.getRowCount(db, "events_textual"));
  }
}
