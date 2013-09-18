/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import java.io.File;
import java.util.Collection;
import java.util.HashMap;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import org.json.JSONObject;
import org.mozilla.gecko.background.common.DateUtils;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.healthreport.HealthReportStorage.MeasurementFields.FieldSpec;

import android.content.ContentValues;
import android.content.Context;
import android.content.ContextWrapper;
import android.database.Cursor;
import android.database.SQLException;
import android.database.sqlite.SQLiteConstraintException;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.os.Build;
import android.util.SparseArray;

/**
 * <code>HealthReportDatabaseStorage</code> provides an interface on top of
 * SQLite storage for Health Report data. It exposes methods for management of
 * environments, measurements, fields, and values, and a cursor-based API for
 * querying.
 *
 * Health Report data is structured as follows.
 *
 * Records are primarily broken down by date, at day granularity. Each day's data
 * is then split according to environment. An environment is a collection of
 * constant attributes, such as version and processor; if one of these attributes
 * changes, a new environment becomes active.
 *
 * Environments are identified by a stable hash of their attributes.
 *
 * The database includes a persistent numeric identifier for each environment. Create
 * or fetch this identifier via:
 *
 * <pre>
 *  final Environment environment = storage.getEnvironment();
 *  // Init the environment now.
 *  String envHash = environment.getHash();
 *  int env = environment.register();
 * </pre>
 *
 * You can safely cache this environment identifier for the life of the database.
 *
 * Orthogonal to environments are measurements. Each measurement is a named and
 * versioned scope for a collection of fields. It is assumed that if a measurement
 * with the given name and version is known to the database, then all of its fields
 * are also known; if you change the collection of fields in the measurement, the
 * measurement's version must be incremented, too.
 *
 * As with environments, measurements have an internal numeric identifier.
 *
 * Calling code should initialize its measurements as follows:
 *
 * <pre>
 *   public static class FooFieldsV1 implements MeasurementFields {
 *     {@literal @}Override
 *     public Iterable<String> getFields() {
 *       ArrayList<String> fields = new ArrayList<String>();
 *       fields.add("bar");
 *       fields.add("baz");
 *       return fields;
 *     }
 *   }
 *
 *   storage.beginInitialization();
 *
 *   try {
 *     storage.ensureMeasurementInitialized("org.mozilla.fooProvider.fooMeasurement",
 *                                          1, new FooFieldsV1());
 *     storage.finishInitialization();
 *   } catch (Exception e) {
 *     storage.abortInitialization();
 *   }
 * </pre>
 *
 * Measurements have fields. Fields can conceptually be divided into "daily last"
 * (we only care about the last value), "daily counter" (increments per day),
 * "daily discrete" (multiple records per day). Simply call the correct method for each.
 *
 * To do so you need a field ID, to avoid constant costly string lookups. You can get
 * this value from storage:
 *
 * <pre>
 *   Field field = storage.getField("org.mozilla.fooProvider.fooMeasurement", 1, "bar");
 *   int fieldID = field.getID();
 * </pre>
 *
 * This lookup is cached, and so is relatively inexpensive.
 *
 * You can then do something like the following:
 *
 * <pre>
 *   storage.recordDailyLast(storage.getDay(), env, fieldID, "last value");
 * </pre>
 *
 * or equivalently for numeric values, discrete or counters, etc.
 *
 * To retrieve values, use {@link #getRawEventsSince(long)}.
 *
 * For safety, perform operations on the storage executor thread:
 *
 * <pre>
 *   storage.enqueueOperation(runnable);
 * </pre>
 */
public class HealthReportDatabaseStorage implements HealthReportStorage {

  private static final String WHERE_DATE_AND_ENV_AND_FIELD = "date = ? AND env = ? AND field = ?";

  public static final String[] COLUMNS_HASH = new String[] {"hash"};
  public static final String[] COLUMNS_DATE_ENV_FIELD_VALUE = new String[] {"date", "env", "field", "value"};
  public static final String[] COLUMNS_DATE_ENVSTR_M_MV_F_VALUE = new String[] {
    "date", "environment", "measurement_name", "measurement_version",
    "field_name", "field_flags", "value"
  };

  private static final String[] COLUMNS_ENVIRONMENT_DETAILS = new String[] {
      "id", "hash",
      "profileCreation", "cpuCount", "memoryMB",

      "isBlocklistEnabled", "isTelemetryEnabled", "extensionCount",
      "pluginCount", "themeCount",

      "architecture", "sysName", "sysVersion", "vendor", "appName", "appID",
      "appVersion", "appBuildID", "platformVersion", "platformBuildID", "os",
      "xpcomabi", "updateChannel",

      // Joined to the add-ons table.
      "addonsBody"
  };

  public static final String[] COLUMNS_MEASUREMENT_DETAILS = new String[] {"id", "name", "version"};
  public static final String[] COLUMNS_MEASUREMENT_AND_FIELD_DETAILS =
      new String[] {"measurement_name", "measurement_id", "measurement_version",
                    "field_name", "field_id", "field_flags"};

  private static final String[] COLUMNS_VALUE = new String[] {"value"};
  private static final String[] COLUMNS_ID = new String[] {"id"};

  private static final String EVENTS_TEXTUAL = "events_textual";
  private static final String EVENTS_INTEGER = "events_integer";

  protected static final String DB_NAME = "health.db";

  private static final String LOG_TAG = "HealthReportStorage";

  private final Executor executor = Executors.newSingleThreadExecutor();

  @Override
  public void enqueueOperation(Runnable runnable) {
    executor.execute(runnable);
  }

  public HealthReportDatabaseStorage(final Context context,
                                     final File profileDirectory) {
    this.helper = new HealthReportSQLiteOpenHelper(context, profileDirectory,
                                                   DB_NAME);
    executor.execute(new Runnable() {
      @Override
      public void run() {
        Logger.setThreadLogTag(HealthReportConstants.GLOBAL_LOG_TAG);
        Logger.debug(LOG_TAG, "Creating HealthReportDatabaseStorage.");
      }
    });
  }

  @Override
  public void close() {
    this.helper.close();
    this.fields.clear();
    this.envs.clear();
    this.measurementVersions.clear();
  }

  protected final HealthReportSQLiteOpenHelper helper;

  public static class HealthReportSQLiteOpenHelper extends SQLiteOpenHelper {
    public static final int CURRENT_VERSION = 5;
    public static final String LOG_TAG = "HealthReportSQL";

    /**
     * A little helper to avoid SQLiteOpenHelper misbehaving on Android 2.1.
     * Partly cribbed from
     * <http://stackoverflow.com/questions/5332328/sqliteopenhelper-problem-with-fully-qualified-db-path-name>.
     */
    public static class AbsolutePathContext extends ContextWrapper {
      private final File parent;

      public AbsolutePathContext(Context base, File parent) {
        super(base);
        this.parent = parent;
      }

      @Override
      public File getDatabasePath(String name) {
        return new File(getAbsolutePath(parent, name));
      }

      // Won't be called after API v11, but we can't override the version that
      // *is* called and still support v8.
      // Instead we check the version code in the HealthReportSQLiteOpenHelper
      // constructor, and only use this workaround if we need to.
      @Override
      public SQLiteDatabase openOrCreateDatabase(String name,
                                                 int mode,
                                                 SQLiteDatabase.CursorFactory factory) {
        final File path = getDatabasePath(name);
        Logger.pii(LOG_TAG, "Opening database through absolute path " + path.getAbsolutePath());
        return SQLiteDatabase.openOrCreateDatabase(path, null);
      }
    }

    public static String getAbsolutePath(File parent, String name) {
      return parent.getAbsolutePath() + File.separator + name;
    }

    public static boolean CAN_USE_ABSOLUTE_DB_PATH = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.FROYO);
    public HealthReportSQLiteOpenHelper(Context context, File profileDirectory, String name) {
      this(context, profileDirectory, name, CURRENT_VERSION);
    }

    // For testing DBs of different versions.
    public HealthReportSQLiteOpenHelper(Context context, File profileDirectory, String name, int version) {
      super(
          (CAN_USE_ABSOLUTE_DB_PATH ? context : new AbsolutePathContext(context, profileDirectory)),
          (CAN_USE_ABSOLUTE_DB_PATH ? getAbsolutePath(profileDirectory, name) : name),
          null,
          version);

      if (CAN_USE_ABSOLUTE_DB_PATH) {
        Logger.pii(LOG_TAG, "Opening: " + getAbsolutePath(profileDirectory, name));
      }
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
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

        db.execSQL("CREATE TABLE " + EVENTS_INTEGER + "(" +
                   "                 date  INTEGER, " +
                   "                 env   INTEGER, " +
                   "                 field INTEGER, " +
                   "                 value INTEGER, " +
                   "                 FOREIGN KEY (field) REFERENCES fields(id) ON DELETE CASCADE, " +
                   "                 FOREIGN KEY (env) REFERENCES environments(id) ON DELETE CASCADE" +
                   ")");

        db.execSQL("CREATE TABLE " + EVENTS_TEXTUAL + "(" +
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
                   "SELECT date, env, field, value FROM " + EVENTS_INTEGER + " " +
                   "UNION ALL " +
                   "SELECT date, env, field, value FROM " + EVENTS_TEXTUAL);

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

        createAddonsEnvironmentsView(db);

        db.setTransactionSuccessful();
      } finally {
        db.endTransaction();
      }
    }

    @Override
    public void onOpen(SQLiteDatabase db) {
      if (!db.isReadOnly()) {
        db.execSQL("PRAGMA foreign_keys=ON;");
      }
    }

    private void createAddonsEnvironmentsView(SQLiteDatabase db) {
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
    }

    private void upgradeDatabaseFrom2To3(SQLiteDatabase db) {
      db.execSQL("CREATE TABLE addons (id INTEGER PRIMARY KEY AUTOINCREMENT, " +
                 "                     body TEXT, " +
                 "                     UNIQUE (body) " +
                 ")");

      db.execSQL("ALTER TABLE environments ADD COLUMN addonsID INTEGER REFERENCES addons(id) ON DELETE RESTRICT");

      createAddonsEnvironmentsView(db);
    }

    private void upgradeDatabaseFrom3To4(SQLiteDatabase db) {
      // Update search measurements to use a different type.
      db.execSQL("UPDATE OR IGNORE fields SET flags = " + Field.TYPE_COUNTED_STRING_DISCRETE +
                 " WHERE measurement IN (SELECT id FROM measurements WHERE name = 'org.mozilla.searches.counts')");
    }

    private void upgradeDatabaseFrom4to5(SQLiteDatabase db) {
      // Delete NULL in addons.body, which appeared as a result of Bug 886156. Note that the
      // foreign key constraint, "ON DELETE RESTRICT", may be violated, but since onOpen() is
      // called after this method, foreign keys are not yet enabled and constraints can be broken.
      db.delete("addons", "body IS NULL", null);

      // Purge any data inconsistent with foreign key references (which may have appeared before
      // foreign keys were enabled in Bug 900289).
      db.delete("fields", "measurement NOT IN (SELECT id FROM measurements)", null);
      db.delete("environments", "addonsID NOT IN (SELECT id from addons)", null);
      db.delete(EVENTS_INTEGER, "env NOT IN (SELECT id FROM environments)", null);
      db.delete(EVENTS_TEXTUAL, "env NOT IN (SELECT id FROM environments)", null);
      db.delete(EVENTS_INTEGER, "field NOT IN (SELECT id FROM fields)", null);
      db.delete(EVENTS_TEXTUAL, "field NOT IN (SELECT id FROM fields)", null);
    }

    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
      if (oldVersion >= newVersion) {
        return;
      }

      Logger.info(LOG_TAG, "onUpgrade: from " + oldVersion + " to " + newVersion + ".");
      try {
        db.beginTransaction();
        switch (oldVersion) {
        case 2:
          upgradeDatabaseFrom2To3(db);
        case 3:
          upgradeDatabaseFrom3To4(db);
        case 4:
          upgradeDatabaseFrom4to5(db);
        }
        db.setTransactionSuccessful();
      } catch (Exception e) {
        Logger.error(LOG_TAG, "Failure in onUpgrade.", e);
        throw new RuntimeException(e);
      } finally {
        db.endTransaction();
      }
   }

    public void deleteEverything() {
      final SQLiteDatabase db = this.getWritableDatabase();

      Logger.info(LOG_TAG, "Deleting everything.");
      db.beginTransaction();
      try {
        // Cascade will clear the rest.
        db.delete("measurements", null, null);
        db.delete("environments", null, null);
        db.delete("addons", null, null);
        db.setTransactionSuccessful();
        Logger.info(LOG_TAG, "Deletion successful.");
      } finally {
        db.endTransaction();
      }
    }
  }

  public class DatabaseField extends Field {
    public DatabaseField(String mName, int mVersion, String fieldName) {
      this(mName, mVersion, fieldName, UNKNOWN_TYPE_OR_FIELD_ID, UNKNOWN_TYPE_OR_FIELD_ID);
    }

    public DatabaseField(String mName, int mVersion, String fieldName, int flags) {
      this(mName, mVersion, fieldName, UNKNOWN_TYPE_OR_FIELD_ID, flags);
    }

    public DatabaseField(String mName, int mVersion, String fieldName, int fieldID, int flags) {
      super(mName, mVersion, fieldName, flags);
      this.fieldID = fieldID;
    }

    private void loadFlags() {
      if (this.flags == UNKNOWN_TYPE_OR_FIELD_ID) {
        if (this.fieldID == UNKNOWN_TYPE_OR_FIELD_ID) {
          this.getID();
        }
        this.flags = integerQuery("fields", "flags", "id = ?", new String[] { Integer.toString(this.fieldID, 10) }, -1);
      }
    }

    @Override
    public synchronized boolean isIntegerField() {
      loadFlags();
      return super.isIntegerField();
    }

    @Override
    public synchronized boolean isStringField() {
      loadFlags();
      return super.isStringField();
    }

    @Override
    public synchronized boolean isDiscreteField() {
      loadFlags();
      return super.isDiscreteField();
    }

    @Override
    public synchronized int getID() throws IllegalStateException {
      if (this.fieldID == UNKNOWN_TYPE_OR_FIELD_ID) {
        this.fieldID = integerQuery("named_fields", "field_id",
                                    "measurement_name = ? AND measurement_version = ? AND field_name = ?",
                                    new String[] {measurementName, measurementVersion, fieldName},
                                    -1);
        if (this.fieldID == UNKNOWN_TYPE_OR_FIELD_ID) {
          throw new IllegalStateException("No field with name " + fieldName +
                                          " (" + measurementName + ", " + measurementVersion + ")");
        }
      }
      return this.fieldID;
    }
  }

  // `envs` and `fields` look similar, but they are touched differently and
  // store differently stable kinds of data, hence type difference.
  // Note that we don't pre-populate the environment cache. We'll typically only
  // handle one per session.
  private final ConcurrentHashMap<String, Integer> envs = new ConcurrentHashMap<String, Integer>();

  /**
   * An {@link Environment} that knows how to persist to and from our database.
   */
  public static class DatabaseEnvironment extends Environment {
    protected final HealthReportDatabaseStorage storage;

    @Override
    public int register() {
      final String h = getHash();
      if (storage.envs.containsKey(h)) {
        this.id = storage.envs.get(h);
        return this.id;
      }

      // Otherwise, add data and hash to the DB.
      ContentValues v = new ContentValues();
      v.put("hash", h);
      v.put("profileCreation", profileCreation);
      v.put("cpuCount", cpuCount);
      v.put("memoryMB", memoryMB);
      v.put("isBlocklistEnabled", isBlocklistEnabled);
      v.put("isTelemetryEnabled", isTelemetryEnabled);
      v.put("extensionCount", extensionCount);
      v.put("pluginCount", pluginCount);
      v.put("themeCount", themeCount);
      v.put("architecture", architecture);
      v.put("sysName", sysName);
      v.put("sysVersion", sysVersion);
      v.put("vendor", vendor);
      v.put("appName", appName);
      v.put("appID", appID);
      v.put("appVersion", appVersion);
      v.put("appBuildID", appBuildID);
      v.put("platformVersion", platformVersion);
      v.put("platformBuildID", platformBuildID);
      v.put("os", os);
      v.put("xpcomabi", xpcomabi);
      v.put("updateChannel", updateChannel);

      final SQLiteDatabase db = storage.helper.getWritableDatabase();

      // If we're not already, we want all of our inserts to be in a transaction.
      boolean newTransaction = !db.inTransaction();

      // Insert, with a little error handling to populate the cache in case of
      // omission and consequent collision.
      //
      // We would like to hang a trigger off a view here, and just use that for
      // inserts. But triggers don't seem to correctly update the last inserted
      // ID, so Android's insertOrThrow method returns -1.
      //
      // Instead, we go without the trigger, simply running the inserts ourselves.
      //
      // insertWithOnConflict doesn't work as documented: <http://stackoverflow.com/questions/11328877/android-sqllite-on-conflict-ignore-is-ignored-in-ics/11424150>.
      // So we do this the hard way.
      // We presume that almost every get will hit the cache (except for the first, obviously), so we
      // bias here towards inserts for the environments.
      // For add-ons we assume that different environments will share add-ons, so we query first.

      final String addonsJSON = getNormalizedAddonsJSON();
      if (newTransaction) {
        db.beginTransaction();
      }

      try {
        int addonsID = ensureAddons(db, addonsJSON);
        v.put("addonsID", addonsID);

        try {
          int inserted = (int) db.insertOrThrow("environments", null, v);
          Logger.debug(LOG_TAG, "Inserted ID: " + inserted + " for hash " + h);
          if (inserted == -1) {
            throw new SQLException("Insert returned -1!");
          }
          this.id = inserted;
          storage.envs.put(h, this.id);
          if (newTransaction) {
            db.setTransactionSuccessful();
          }
          return inserted;
        } catch (SQLException e) {
          // The inserter should take care of updating `envs`. But if it
          // doesn't...
          Cursor c = db.query("environments", COLUMNS_ID, "hash = ?",
                              new String[] { h }, null, null, null);
          try {
            if (!c.moveToFirst()) {
              throw e;
            }
            this.id = (int) c.getLong(0);
            Logger.debug(LOG_TAG, "Found " + this.id + " for hash " + h);
            storage.envs.put(h, this.id);
            if (newTransaction) {
              db.setTransactionSuccessful();
            }
            return this.id;
          } finally {
            c.close();
          }
        }
      } finally {
        if (newTransaction) {
          db.endTransaction();
        }
      }
    }

    protected static int ensureAddons(SQLiteDatabase db, String json) {
      Cursor c = db.query("addons", COLUMNS_ID, "body = ?",
                          new String[] { (json == null) ? "null" : json }, null, null, null);
      try {
        if (c.moveToFirst()) {
          return c.getInt(0);
        }
        ContentValues values = new ContentValues();
        values.put("body", json);
        return (int) db.insert("addons", null, values);
      } finally {
        c.close();
      }
    }

    public void init(ContentValues v) {
      profileCreation = v.getAsInteger("profileCreation");
      cpuCount        = v.getAsInteger("cpuCount");
      memoryMB        = v.getAsInteger("memoryMB");

      isBlocklistEnabled = v.getAsInteger("isBlocklistEnabled");
      isTelemetryEnabled = v.getAsInteger("isTelemetryEnabled");
      extensionCount     = v.getAsInteger("extensionCount");
      pluginCount        = v.getAsInteger("pluginCount");
      themeCount         = v.getAsInteger("themeCount");

      architecture    = v.getAsString("architecture");
      sysName         = v.getAsString("sysName");
      sysVersion      = v.getAsString("sysVersion");
      vendor          = v.getAsString("vendor");
      appName         = v.getAsString("appName");
      appID           = v.getAsString("appID");
      appVersion      = v.getAsString("appVersion");
      appBuildID      = v.getAsString("appBuildID");
      platformVersion = v.getAsString("platformVersion");
      platformBuildID = v.getAsString("platformBuildID");
      os              = v.getAsString("os");
      xpcomabi        = v.getAsString("xpcomabi");
      updateChannel   = v.getAsString("updateChannel");

      try {
        setJSONForAddons(v.getAsString("addonsBody"));
      } catch (Exception e) {
        // Nothing we can do.
      }

      this.hash = null;
      this.id = -1;
    }

    /**
     * Fill ourselves with data from the DB, then advance the cursor.
     *
     * @param cursor a {@link Cursor} pointing at a record to load.
     * @return true if the cursor was successfully advanced.
     */
    public boolean init(Cursor cursor) {
      int i = 0;
      this.id         = cursor.getInt(i++);
      this.hash       = cursor.getString(i++);

      profileCreation = cursor.getInt(i++);
      cpuCount        = cursor.getInt(i++);
      memoryMB        = cursor.getInt(i++);

      isBlocklistEnabled = cursor.getInt(i++);
      isTelemetryEnabled = cursor.getInt(i++);
      extensionCount     = cursor.getInt(i++);
      pluginCount        = cursor.getInt(i++);
      themeCount         = cursor.getInt(i++);

      architecture    = cursor.getString(i++);
      sysName         = cursor.getString(i++);
      sysVersion      = cursor.getString(i++);
      vendor          = cursor.getString(i++);
      appName         = cursor.getString(i++);
      appID           = cursor.getString(i++);
      appVersion      = cursor.getString(i++);
      appBuildID      = cursor.getString(i++);
      platformVersion = cursor.getString(i++);
      platformBuildID = cursor.getString(i++);
      os              = cursor.getString(i++);
      xpcomabi        = cursor.getString(i++);
      updateChannel   = cursor.getString(i++);

      try {
        setJSONForAddons(cursor.getBlob(i++));
      } catch (Exception e) {
        // Nothing we can do.
      }

      return cursor.moveToNext();
    }

    public DatabaseEnvironment(HealthReportDatabaseStorage storage, Class<? extends EnvironmentAppender> appender) {
      super(appender);
      this.storage = storage;
    }

    public DatabaseEnvironment(HealthReportDatabaseStorage storage) {
      this.storage = storage;
    }
  }

  /**
   * Factory method. Returns a new {@link Environment} that callers can
   * populate and then register.
   */
  @Override
  public DatabaseEnvironment getEnvironment() {
    return new DatabaseEnvironment(this);
  }

  @Override
  public SparseArray<Environment> getEnvironmentRecordsByID() {
    final SQLiteDatabase db = this.helper.getReadableDatabase();
    Cursor c = db.query("environments_with_addons", COLUMNS_ENVIRONMENT_DETAILS, null, null, null, null, null);
    try {
      SparseArray<Environment> results = new SparseArray<Environment>();
      if (!c.moveToFirst()) {
        return results;
      }

      DatabaseEnvironment e = getEnvironment();
      while (e.init(c)) {
        results.put(e.id, e);
        e = getEnvironment();
      }
      results.put(e.id, e);
      return results;
    } finally {
      c.close();
    }
  }

  /**
   * Reverse lookup for an env. Only really used for tests: document generation
   * fetches all environments at once, and insertion only uses the integer key
   * that's returned during insertion.
   *
   * @param id
   *          the identifier for the environment.
   * @return a cursor over its details.
   */
  @Override
  public Cursor getEnvironmentRecordForID(int id) {
    final SQLiteDatabase db = this.helper.getReadableDatabase();
    return db.query("environments_with_addons", COLUMNS_ENVIRONMENT_DETAILS, "id = " + id, null, null, null, null);
  }

  @Override
  public SparseArray<String> getEnvironmentHashesByID() {
    final SQLiteDatabase db = this.helper.getReadableDatabase();
    Cursor c = db.query("environments", new String[] {"id", "hash"}, null, null, null, null, null);
    try {
      SparseArray<String> results = new SparseArray<String>();
      if (!c.moveToFirst()) {
        return results;
      }

      while (!c.isAfterLast()) {
        results.put(c.getInt(0), c.getString(1));
        c.moveToNext();
      }
      return results;
    } finally {
      c.close();
    }
  }

  /**
   * Cache the lookup from measurement and field specifier to field instance.
   * This allows us to memoize the field ID lookup, too.
   */
  private HashMap<String, Field> fields = new HashMap<String, Field>();
  private boolean fieldsCacheUpdated = false;

  private String getFieldKey(String mName, int mVersion, String fieldName) {
    return mVersion + "." + mName + "/" + fieldName;
  }

  @Override
  public Field getField(String mName, int mVersion, String fieldName) {
    final String key = getFieldKey(mName, mVersion, fieldName);
    synchronized (fields) {
      if (fields.containsKey(key)) {
        return fields.get(key);
      }
      Field f = new DatabaseField(mName, mVersion, fieldName);
      fields.put(key, f);
      return f;
    }
  }

  private void populateFieldCache() {
    synchronized (fields) {
      if (fieldsCacheUpdated) {
        return;
      }

      fields.clear();
      Cursor c = getFieldVersions();
      try {
        if (!c.moveToFirst()) {
          return;
        }
        do {
          // We don't use the measurement ID here, so column 1 is unused.
          final String mName = c.getString(0);
          final int mVersion = c.getInt(2);
          final String fieldName = c.getString(3);
          final int fieldID = c.getInt(4);
          final int flags = c.getInt(5);
          final String key = getFieldKey(mName, mVersion, fieldName);

          Field f = new DatabaseField(mName, mVersion, fieldName, fieldID, flags);
          fields.put(key, f);
        } while (c.moveToNext());
        fieldsCacheUpdated = true;
      } finally {
        c.close();
      }
    }
  }

  /**
   * Return mappings from field ID to Field instance. Do so by looking in the DB.
   */
  @Override
  public SparseArray<Field> getFieldsByID() {
    final SparseArray<Field> out = new SparseArray<Field>();
    synchronized (fields) {
      populateFieldCache();
      Collection<Field> values = fields.values();
      for (Field field : values) {
        // Cache is up-to-date at this point, so we don't need to hit the DB.
        out.put(field.getID(), field);
      }
    }
    return out;
  }

  private final HashMap<String, Integer> measurementVersions = new HashMap<String, Integer>();

  private void populateMeasurementVersionsCache(SQLiteDatabase db) {
    HashMap<String, Integer> results = getIntegers(db, "current_measurements", "name", "version");
    if (results == null) {
      measurementVersions.clear();
      return;
    }
    synchronized (measurementVersions) {
      measurementVersions.clear();
      measurementVersions.putAll(results);
    }
  }

  /**
   * Return the version of the measurement for which the DB is currently configured, or
   * 0 if unknown.
   * @param measurement String measurement identifier.
   * @return Current version.
   */
  private int getMeasurementVersion(String measurement) {
    synchronized (measurementVersions) {
      if (measurementVersions.containsKey(measurement)) {
        return measurementVersions.get(measurement);
      }

      // This should never be necessary, unless the measurement does not exist.
      int value = integerQuery("measurements", "version", "name = ?", new String[] {measurement}, 0);
      measurementVersions.put(measurement, value);
      return value;
    }
  }

  /**
   * Inform the storage layer that fields for the given measurement have been updated
   * to this version.
   *
   * This should be one of the final calls in a configuration transaction.
   * Always call this inside a transaction.
   */
  private void notifyMeasurementVersionUpdated(String measurement, int version) {
    Logger.info(LOG_TAG, "Measurement " + measurement + " now at " + version);

    final SQLiteDatabase db = this.helper.getWritableDatabase();
    final ContentValues values = new ContentValues();
    values.put("name", measurement);
    values.put("version", version);

    synchronized (measurementVersions) {
      measurementVersions.put(measurement, version);
    }

    db.insertWithOnConflict("measurements", null, values, SQLiteDatabase.CONFLICT_IGNORE);
  }

  /**
   * Call in a transaction.
   * This method could race with other accesses, but (a) it's within a transaction,
   * (b) configuration should be single-threaded, (c) we initialize the cache up-front.
   */
  @Override
  public void ensureMeasurementInitialized(String measurement, int version, MeasurementFields fields) {
    final int currentVersion = getMeasurementVersion(measurement);
    Logger.info(LOG_TAG, "Initializing measurement " + measurement + " to " +
                         version + " (current " + currentVersion + ")");

    if (currentVersion == version) {
      Logger.info(LOG_TAG, "Measurement " + measurement + " already at v" + version);
      return;
    }

    final SQLiteDatabase db = this.helper.getWritableDatabase();
    if (!db.inTransaction()) {
      Logger.warn(LOG_TAG, "ensureMeasurementInitialized should be called within a transaction.");
    }

    final ContentValues mv = new ContentValues();
    mv.put("name", measurement);
    mv.put("version", version);

    final int measurementID = (int) db.insert("measurements", null, mv);

    final ContentValues v = new ContentValues();
    v.put("measurement", measurementID);
    for (FieldSpec field : fields.getFields()) {
      v.put("name", field.name);
      v.put("flags", field.type);
      Logger.debug(LOG_TAG, "M: " + measurementID + " F: " + field.name + " (" + field.type + ")");
      db.insert("fields", null, v);
    }

    notifyMeasurementVersionUpdated(measurement, version);

    // Let's be easy for now.
    synchronized (fields) {
      fieldsCacheUpdated = false;
    }
  }

  /**
   * Return a cursor over the measurements and fields in the DB.
   * Columns are {@link HealthReportDatabaseStorage#COLUMNS_MEASUREMENT_AND_FIELD_DETAILS}.
   */
  @Override
  public Cursor getFieldVersions() {
    final SQLiteDatabase db = this.helper.getReadableDatabase();
    return db.query("named_fields", COLUMNS_MEASUREMENT_AND_FIELD_DETAILS,
                    null, null, null, null, "measurement_name, measurement_version, field_name");
  }

  @Override
  public Cursor getFieldVersions(String measurement, int measurementVersion) {
    final SQLiteDatabase db = this.helper.getReadableDatabase();
    return db.query("named_fields", COLUMNS_MEASUREMENT_AND_FIELD_DETAILS,
                    "measurement_name = ? AND measurement_version = ?",
                    new String[] {measurement, Integer.toString(measurementVersion)},
                    null, null, "field_name");
  }

  @Override
  public Cursor getMeasurementVersions() {
    final SQLiteDatabase db = this.helper.getReadableDatabase();
    return db.query("measurements", COLUMNS_MEASUREMENT_DETAILS,
                    null, null, null, null, "name, version");
  }

  /**
   * A thin wrapper around the database transactional semantics. Clients can
   * use this to more efficiently ensure that measurements are initialized.
   *
   * Note that caches are also initialized here.
   */
  public void beginInitialization() {
    SQLiteDatabase db = this.helper.getWritableDatabase();
    db.beginTransaction();
    populateMeasurementVersionsCache(db);
  }

  public void finishInitialization() {
    SQLiteDatabase db = this.helper.getWritableDatabase();
    db.setTransactionSuccessful();
    db.endTransaction();
  }

  public void abortInitialization() {
    this.helper.getWritableDatabase().endTransaction();
  }

  @Override
  public int getDay(long time) {
    return DateUtils.getDay(time);
  }

  @Override
  public int getDay() {
    return this.getDay(System.currentTimeMillis());
  }

  private void recordDailyLast(int env, int day, int field, Object value, String table) {
    if (env == -1) {
      Logger.warn(LOG_TAG, "Refusing to record with environment = -1.");
      return;
    }

    final SQLiteDatabase db = this.helper.getWritableDatabase();

    final String envString = Integer.toString(env);
    final String fieldIDString = Integer.toString(field, 10);
    final String dayString = Integer.toString(day, 10);

    // Java, your capacity for abstraction leaves me wanting.
    final ContentValues v = new ContentValues();
    putValue(v, value);

    // If we used a separate table, such that we could have a
    // UNIQUE(env, field, day) constraint for daily-last values, then we could
    // use INSERT OR REPLACE.
    final int updated = db.update(table, v, WHERE_DATE_AND_ENV_AND_FIELD,
                                  new String[] {dayString, envString, fieldIDString});
    if (0 == updated) {
      v.put("env", env);
      v.put("field", field);
      v.put("date", day);
      try {
        db.insertOrThrow(table, null, v);
      } catch (SQLiteConstraintException e) {
        throw new IllegalStateException("Event did not reference existing an environment or field.", e);
      }
    }
  }

  @Override
  public void recordDailyLast(int env, int day, int field, JSONObject value) {
    this.recordDailyLast(env, day, field, value == null ? "null" : value.toString(), EVENTS_TEXTUAL);
  }

  @Override
  public void recordDailyLast(int env, int day, int field, String value) {
    this.recordDailyLast(env, day, field, value, EVENTS_TEXTUAL);
  }

  @Override
  public void recordDailyLast(int env, int day, int field, int value) {
    this.recordDailyLast(env, day, field, Integer.valueOf(value), EVENTS_INTEGER);
  }

  private void recordDailyDiscrete(int env, int day, int field, Object value, String table) {
    if (env == -1) {
      Logger.warn(LOG_TAG, "Refusing to record with environment = -1.");
      return;
    }

    final ContentValues v = new ContentValues();
    v.put("env", env);
    v.put("field", field);
    v.put("date", day);

    final SQLiteDatabase db = this.helper.getWritableDatabase();
    putValue(v, value);
    try {
      db.insertOrThrow(table, null, v);
    } catch (SQLiteConstraintException e) {
      throw new IllegalStateException("Event did not reference existing an environment or field.", e);
    }
  }

  @Override
  public void recordDailyDiscrete(int env, int day, int field, JSONObject value) {
    this.recordDailyDiscrete(env, day, field, value == null ? "null" : value.toString(), EVENTS_TEXTUAL);
  }

  @Override
  public void recordDailyDiscrete(int env, int day, int field, String value) {
    this.recordDailyDiscrete(env, day, field, value, EVENTS_TEXTUAL);
  }

  @Override
  public void recordDailyDiscrete(int env, int day, int field, int value) {
    this.recordDailyDiscrete(env, day, field, value, EVENTS_INTEGER);
  }

  /**
   * Increment the specified field value by the specified amount. Counts start
   * at zero.
   *
   * Note that this method can misbehave or throw if not executed within a
   * transaction, because correct behavior involves querying then
   * insert-or-update, and a race condition can otherwise occur.
   *
   * @param env the environment ID
   * @param day the current day, in days since epoch
   * @param field the field ID
   * @param by how much to increment the counter.
   */
  @Override
  public void incrementDailyCount(int env, int day, int field, int by) {
    if (env == -1) {
      Logger.warn(LOG_TAG, "Refusing to record with environment = -1.");
      return;
    }

    final SQLiteDatabase db = this.helper.getWritableDatabase();
    final String envString = Integer.toString(env);
    final String fieldIDString = Integer.toString(field, 10);
    final String dayString = Integer.toString(day, 10);

    // Can't run a complex UPDATE and get the number of changed rows, so we'll
    // do this the hard way.
    // This relies on being called within a transaction.
    final String[] args = new String[] {dayString, envString, fieldIDString};
    final Cursor c = db.query(EVENTS_INTEGER,
                              COLUMNS_VALUE,
                              WHERE_DATE_AND_ENV_AND_FIELD,
                              args, null, null, null, "1");

    boolean present = false;
    try {
      present = c.moveToFirst();
    } finally {
      c.close();
    }

    if (present) {
      // It's an int, so safe to concatenate. Avoids us having to mess with args.
      db.execSQL("UPDATE " + EVENTS_INTEGER + " SET value = value + " + by + " WHERE " +
                 WHERE_DATE_AND_ENV_AND_FIELD,
                 args);
    } else {
      final ContentValues v = new ContentValues();
      v.put("env", env);
      v.put("value", by);
      v.put("field", field);
      v.put("date", day);
      try {
        db.insertOrThrow(EVENTS_INTEGER, null, v);
      } catch (SQLiteConstraintException e) {
        throw new IllegalStateException("Event did not reference existing an environment or field.", e);
      }
    }
  }

  @Override
  public void incrementDailyCount(int env, int day, int field) {
    this.incrementDailyCount(env, day, field, 1);
  }

  /**
   * Are there events recorded on or after <code>time</code>?
   *
   * @param time milliseconds since epoch. Will be converted by {@link #getDay(long)}.
   * @return true if such events exist, false otherwise.
   */
  @Override
  public boolean hasEventSince(long time) {
    final int start = this.getDay(time);
    final SQLiteDatabase db = this.helper.getReadableDatabase();
    final String dayString = Integer.toString(start, 10);
    Cursor cur = db.query("events", COLUMNS_DATE_ENV_FIELD_VALUE,
        "date >= ?", new String[] {dayString}, null, null, null, "1");
    if (cur == null) {
      // Something is horribly wrong; let the caller who actually reads the
      // events deal with it.
      return true;
    }
    try {
      return cur.getCount() > 0;
    } finally {
      cur.close();
    }
  }

  /**
   * Returns a cursor over field events in the database. The results will be
   * strictly ordered first by date, then by environment, and finally by field.
   *
   * Each row includes columns in {@link #COLUMNS_DATE_ENV_FIELD_VALUE}:
   * "date", "env", "field", "value".
   *
   * @param time milliseconds since epoch. Will be converted by {@link #getDay(long)}.
   * @return a cursor. The caller is responsible for closing this.
   */
  @Override
  public Cursor getRawEventsSince(long time) {
    final int start = this.getDay(time);
    final SQLiteDatabase db = this.helper.getReadableDatabase();
    final String dayString = Integer.toString(start, 10);
    return db.query("events", COLUMNS_DATE_ENV_FIELD_VALUE,
                    "date >= ?", new String[] {dayString}, null, null, "date, env, field");
  }

  /**
   * Returns a cursor over field events in the database. The results will be
   * strictly ordered first by date, then by environment, and finally by field.
   *
   * Each row includes columns in {@link #COLUMNS_DATE_ENVSTR_M_MV_F_VALUE}:
   * "date", "environment" (as a String), "measurement_name", "measurement_version",
   * "field_name", "field_flags", "value".
   *
   * @param time milliseconds since epoch. Will be converted by {@link #getDay(long)}.
   * @return a cursor. The caller is responsible for closing this.
   */
  @Override
  public Cursor getEventsSince(long time) {
    final int start = this.getDay(time);
    final SQLiteDatabase db = this.helper.getReadableDatabase();
    final String dayString = Integer.toString(start, 10);
    return db.query("named_events", COLUMNS_DATE_ENVSTR_M_MV_F_VALUE,
                    "date >= ?", new String[] {dayString}, null, null,
                    "date, environment, measurement_name, measurement_version, field_name");
  }

  /**
   * Retrieve a mapping from a table. Keys should be unique; only one key-value
   * pair will be returned for each key.
   */
  private static HashMap<String, Integer> getIntegers(SQLiteDatabase db, String table, String columnA, String columnB) {
    Cursor c = db.query(table, new String[] {columnA, columnB}, null, null, null, null, null);
    try {
      if (!c.moveToFirst()) {
        return null;
      }

      HashMap<String, Integer> results = new HashMap<String, Integer>();
      while (!c.isAfterLast()) {
        results.put(c.getString(0), c.getInt(1));
        c.moveToNext();
      }
      return results;
    } finally {
      c.close();
    }
  }

  /**
   * Retrieve a single value from a mapping table.
   */
  private int integerQuery(String table, String column, String where, String[] args, int defaultValue) {
    final SQLiteDatabase db = this.helper.getReadableDatabase();
    Cursor c = db.query(table, new String[] {column}, where, args, null, null, column + " DESC", "1");
    try {
      if (!c.moveToFirst()) {
        return defaultValue;
      }
      return c.getInt(0);
    } finally {
      c.close();
    }
  }

  /**
   * Helper to allow us to avoid excessive code duplication.
   *
   * @param v
   *          the destination <code>ContentValues</code>.
   * @param value
   *          either a <code>String</code> or an <code>Integer</code>. No type
   *          checking is performed.
   */
  private static final void putValue(final ContentValues v, Object value) {
    if (value instanceof String) {
      v.put("value", (String) value);
    } else {
      v.put("value", (Integer) value);
    }
  }

  @Override
  public void deleteEverything() {
    this.helper.deleteEverything();
  }

  @Override
  public void deleteEnvironments() {
    final SQLiteDatabase db = this.helper.getWritableDatabase();
    db.beginTransaction();
    try {
      // Cascade will clear the rest.
      db.delete("environments", null, null);
      db.setTransactionSuccessful();
    } finally {
      db.endTransaction();
    }
  }

  @Override
  public void deleteMeasurements() {
    final SQLiteDatabase db = this.helper.getWritableDatabase();
    db.beginTransaction();
    try {
      // Cascade will clear the rest.
      db.delete("measurements", null, null);
      db.setTransactionSuccessful();
    } finally {
      db.endTransaction();
    }
  }
}
