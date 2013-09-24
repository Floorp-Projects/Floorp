/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport;

import java.io.File;

import org.mozilla.gecko.background.healthreport.HealthReportDatabaseStorage.HealthReportSQLiteOpenHelper;

import android.content.Context;
import android.database.sqlite.SQLiteDatabase;

public class MockHealthReportSQLiteOpenHelper extends HealthReportSQLiteOpenHelper {
  private int version;

  public MockHealthReportSQLiteOpenHelper(Context context, File fakeProfileDirectory, String name) {
    super(context, fakeProfileDirectory, name);
    version = HealthReportSQLiteOpenHelper.CURRENT_VERSION;
  }

  public MockHealthReportSQLiteOpenHelper(Context context, File fakeProfileDirectory, String name, int version) {
    super(context, fakeProfileDirectory, name, version);
    this.version = version;
  }

  @Override
  public void onCreate(SQLiteDatabase db) {
    if (version == HealthReportSQLiteOpenHelper.CURRENT_VERSION) {
      super.onCreate(db);
    } else if (version == 4) {
      onCreateSchemaVersion4(db);
    } else {
      throw new IllegalStateException("Unknown version number, " + version + ".");
    }
  }

  // Copy-pasta from HealthReportDatabaseStorage.onCreate from v4.
  public void onCreateSchemaVersion4(SQLiteDatabase db) {
    db.beginTransaction();
    try {
      db.execSQL("CREATE TABLE addons (id INTEGER PRIMARY KEY AUTOINCREMENT, " +
                 "                     body TEXT, " +
                 "                     UNIQUE (body) " +
                 ")");

      db.execSQL("CREATE TABLE environments (id INTEGER PRIMARY KEY AUTOINCREMENT, " +
                 "                           hash TEXT, " +
                 "                           profileCreation INTEGER, " +
                 "                           cpuCount        INTEGER, " +
                 "                           memoryMB        INTEGER, " +
                 "                           isBlocklistEnabled INTEGER, " +
                 "                           isTelemetryEnabled INTEGER, " +
                 "                           extensionCount     INTEGER, " +
                 "                           pluginCount        INTEGER, " +
                 "                           themeCount         INTEGER, " +
                 "                           architecture    TEXT, " +
                 "                           sysName         TEXT, " +
                 "                           sysVersion      TEXT, " +
                 "                           vendor          TEXT, " +
                 "                           appName         TEXT, " +
                 "                           appID           TEXT, " +
                 "                           appVersion      TEXT, " +
                 "                           appBuildID      TEXT, " +
                 "                           platformVersion TEXT, " +
                 "                           platformBuildID TEXT, " +
                 "                           os              TEXT, " +
                 "                           xpcomabi        TEXT, " +
                 "                           updateChannel   TEXT, " +
                 "                           addonsID        INTEGER, " +
                 "                           FOREIGN KEY (addonsID) REFERENCES addons(id) ON DELETE RESTRICT, " +
                 "                           UNIQUE (hash) " +
                 ")");

      db.execSQL("CREATE TABLE measurements (id INTEGER PRIMARY KEY AUTOINCREMENT, " +
                 "                           name TEXT, " +
                 "                           version INTEGER, " +
                 "                           UNIQUE (name, version) " +
                 ")");

      db.execSQL("CREATE TABLE fields (id INTEGER PRIMARY KEY AUTOINCREMENT, " +
                 "                     measurement INTEGER, " +
                 "                     name TEXT, " +
                 "                     flags INTEGER, " +
                 "                     FOREIGN KEY (measurement) REFERENCES measurements(id) ON DELETE CASCADE, " +
                 "                     UNIQUE (measurement, name)" +
                 ")");

      db.execSQL("CREATE TABLE events_integer (" +
                 "                 date  INTEGER, " +
                 "                 env   INTEGER, " +
                 "                 field INTEGER, " +
                 "                 value INTEGER, " +
                 "                 FOREIGN KEY (field) REFERENCES fields(id) ON DELETE CASCADE, " +
                 "                 FOREIGN KEY (env) REFERENCES environments(id) ON DELETE CASCADE" +
                 ")");

      db.execSQL("CREATE TABLE events_textual (" +
                 "                 date  INTEGER, " +
                 "                 env   INTEGER, " +
                 "                 field INTEGER, " +
                 "                 value TEXT, " +
                 "                 FOREIGN KEY (field) REFERENCES fields(id) ON DELETE CASCADE, " +
                 "                 FOREIGN KEY (env) REFERENCES environments(id) ON DELETE CASCADE" +
                 ")");

      db.execSQL("CREATE INDEX idx_events_integer_date_env_field ON events_integer (date, env, field)");
      db.execSQL("CREATE INDEX idx_events_textual_date_env_field ON events_textual (date, env, field)");

      db.execSQL("CREATE VIEW events AS " +
                 "SELECT date, env, field, value FROM events_integer " +
                 "UNION ALL " +
                 "SELECT date, env, field, value FROM events_textual");

      db.execSQL("CREATE VIEW named_events AS " +
                 "SELECT date, " +
                 "       environments.hash AS environment, " +
                 "       measurements.name AS measurement_name, " +
                 "       measurements.version AS measurement_version, " +
                 "       fields.name AS field_name, " +
                 "       fields.flags AS field_flags, " +
                 "       value FROM " +
                 "events JOIN environments ON events.env = environments.id " +
                 "       JOIN fields ON events.field = fields.id " +
                 "       JOIN measurements ON fields.measurement = measurements.id");

      db.execSQL("CREATE VIEW named_fields AS " +
                 "SELECT measurements.name AS measurement_name, " +
                 "       measurements.id AS measurement_id, " +
                 "       measurements.version AS measurement_version, " +
                 "       fields.name AS field_name, " +
                 "       fields.id AS field_id, " +
                 "       fields.flags AS field_flags " +
                 "FROM fields JOIN measurements ON fields.measurement = measurements.id");

      db.execSQL("CREATE VIEW current_measurements AS " +
                 "SELECT name, MAX(version) AS version FROM measurements GROUP BY name");

      // createAddonsEnvironmentsView(db):
      db.execSQL("CREATE VIEW environments_with_addons AS " +
          "SELECT e.id AS id, " +
          "       e.hash AS hash, " +
          "       e.profileCreation AS profileCreation, " +
          "       e.cpuCount AS cpuCount, " +
          "       e.memoryMB AS memoryMB, " +
          "       e.isBlocklistEnabled AS isBlocklistEnabled, " +
          "       e.isTelemetryEnabled AS isTelemetryEnabled, " +
          "       e.extensionCount AS extensionCount, " +
          "       e.pluginCount AS pluginCount, " +
          "       e.themeCount AS themeCount, " +
          "       e.architecture AS architecture, " +
          "       e.sysName AS sysName, " +
          "       e.sysVersion AS sysVersion, " +
          "       e.vendor AS vendor, " +
          "       e.appName AS appName, " +
          "       e.appID AS appID, " +
          "       e.appVersion AS appVersion, " +
          "       e.appBuildID AS appBuildID, " +
          "       e.platformVersion AS platformVersion, " +
          "       e.platformBuildID AS platformBuildID, " +
          "       e.os AS os, " +
          "       e.xpcomabi AS xpcomabi, " +
          "       e.updateChannel AS updateChannel, " +
          "       addons.body AS addonsBody " +
          "FROM environments AS e, addons " +
          "WHERE e.addonsID = addons.id");

      db.setTransactionSuccessful();
    } finally {
      db.endTransaction();
    }
  }
}
