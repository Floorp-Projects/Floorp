/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import org.json.JSONObject;

import android.database.Cursor;
import android.util.SparseArray;

/**
 * Abstraction over storage for Firefox Health Report on Android.
 */
public interface HealthReportStorage {
  // Right now we only care about the name of the field.
  public interface MeasurementFields {
    public class FieldSpec {
      public final String name;
      public final int type;
      public FieldSpec(String name, int type) {
        this.name = name;
        this.type = type;
      }
    }
    Iterable<FieldSpec> getFields();
  }

  public abstract class Field {
    protected static final int UNKNOWN_TYPE_OR_FIELD_ID = -1;

    protected static final int FLAG_INTEGER  = 1 << 0;
    protected static final int FLAG_STRING   = 1 << 1;
    protected static final int FLAG_JSON     = 1 << 2;

    protected static final int FLAG_DISCRETE = 1 << 8;
    protected static final int FLAG_LAST     = 1 << 9;
    protected static final int FLAG_COUNTER  = 1 << 10;

    protected static final int FLAG_COUNTED  = 1 << 14;

    public static final int TYPE_INTEGER_DISCRETE = FLAG_INTEGER | FLAG_DISCRETE;
    public static final int TYPE_INTEGER_LAST     = FLAG_INTEGER | FLAG_LAST;
    public static final int TYPE_INTEGER_COUNTER  = FLAG_INTEGER | FLAG_COUNTER;

    public static final int TYPE_STRING_DISCRETE  = FLAG_STRING | FLAG_DISCRETE;
    public static final int TYPE_STRING_LAST      = FLAG_STRING | FLAG_LAST;

    public static final int TYPE_JSON_DISCRETE    = FLAG_JSON | FLAG_DISCRETE;
    public static final int TYPE_JSON_LAST        = FLAG_JSON | FLAG_LAST;

    public static final int TYPE_COUNTED_STRING_DISCRETE = FLAG_COUNTED | TYPE_STRING_DISCRETE;

    protected int fieldID = UNKNOWN_TYPE_OR_FIELD_ID;
    protected int flags;

    protected final String measurementName;
    protected final String measurementVersion;
    protected final String fieldName;

    public Field(String mName, int mVersion, String fieldName, int type) {
      this.measurementName = mName;
      this.measurementVersion = Integer.toString(mVersion, 10);
      this.fieldName = fieldName;
      this.flags = type;
    }

    /**
     * @return the ID for this <code>Field</code>
     * @throws IllegalStateException if this field is not found in storage
     */
    public abstract int getID() throws IllegalStateException;

    public boolean isIntegerField() {
      return (this.flags & FLAG_INTEGER) > 0;
    }

    public boolean isStringField() {
      return (this.flags & FLAG_STRING) > 0;
    }

    public boolean isJSONField() {
      return (this.flags & FLAG_JSON) > 0;
    }

    public boolean isStoredAsString() {
      return (this.flags & (FLAG_JSON | FLAG_STRING)) > 0;
    }

    public boolean isDiscreteField() {
      return (this.flags & FLAG_DISCRETE) > 0;
    }

    /**
     * True if the accrued values are intended to be bucket-counted. For strings,
     * each discrete value will name a bucket, with the number of instances per
     * day being the value in the bucket.
     */
    public boolean isCountedField() {
      return (this.flags & FLAG_COUNTED) > 0;
    }
  }

  /**
   * Close open storage handles and otherwise finish up.
   */
  public void close();

  /**
   * Return the day integer corresponding to the provided time.
   *
   * @param time
   *          milliseconds since Unix epoch.
   * @return an integer day.
   */
  public int getDay(long time);

  /**
   * Return the day integer corresponding to the current time.
   *
   * @return an integer day.
   */
  public int getDay();

  /**
   * Return a new {@link Environment}, suitable for being populated, hashed, and
   * registered.
   *
   * @return a new {@link Environment} instance.
   */
  public Environment getEnvironment();

  /**
   * @return a mapping from environment IDs to hashes, suitable for use in
   *         payload generation.
   */
  public SparseArray<String> getEnvironmentHashesByID();

  /**
   * @return a mapping from environment IDs to registered {@link Environment}
   *         records, suitable for use in payload generation.
   */
  public SparseArray<Environment> getEnvironmentRecordsByID();

  /**
   * @param id
   *          the environment ID, as returned by {@link Environment#register()}.
   * @return a cursor for the record.
   */
  public Cursor getEnvironmentRecordForID(int id);

  /**
   * @param measurement
   *          the name of a measurement, such as "org.mozilla.appInfo.appInfo".
   * @param measurementVersion
   *          the version of a measurement, such as '3'.
   * @param fieldName
   *          the name of a field, such as "platformVersion".
   *
   * @return a {@link Field} instance corresponding to the provided values.
   */
  public Field getField(String measurement, int measurementVersion,
                        String fieldName);

  /**
   * @return a mapping from field IDs to {@link Field} instances, suitable for
   *         use in payload generation.
   */
  public SparseArray<Field> getFieldsByID();

  public void recordDailyLast(int env, int day, int field, JSONObject value);
  public void recordDailyLast(int env, int day, int field, String value);
  public void recordDailyLast(int env, int day, int field, int value);
  public void recordDailyDiscrete(int env, int day, int field, JSONObject value);
  public void recordDailyDiscrete(int env, int day, int field, String value);
  public void recordDailyDiscrete(int env, int day, int field, int value);
  public void incrementDailyCount(int env, int day, int field, int by);
  public void incrementDailyCount(int env, int day, int field);

  /**
   * Return true if events exist that were recorded on or after <code>time</code>.
   */
  boolean hasEventSince(long time);

  /**
   * Obtain a cursor over events that were recorded since <code>time</code>.
   * This cursor exposes 'raw' events, with integer identifiers for values.
   */
  public Cursor getRawEventsSince(long time);

  /**
   * Obtain a cursor over events that were recorded since <code>time</code>.
   *
   * This cursor exposes 'friendly' events, with string names and full
   * measurement metadata.
   */
  public Cursor getEventsSince(long time);

  /**
   * Ensure that a measurement and all of its fields are registered with the DB.
   * No fields will be processed if the measurement exists with the specified
   * version.
   *
   * @param measurement
   *          a measurement name, such as "org.mozila.appInfo.appInfo".
   * @param version
   *          a version number, such as '3'.
   * @param fields
   *          a {@link MeasurementFields} instance, consisting of a collection
   *          of field names.
   */
  public void ensureMeasurementInitialized(String measurement,
                                           int version,
                                           MeasurementFields fields);
  public Cursor getMeasurementVersions();
  public Cursor getFieldVersions();
  public Cursor getFieldVersions(String measurement, int measurementVersion);

  public void deleteEverything();
  public void deleteEnvironments();
  public void deleteMeasurements();
  /**
   * Deletes all environments, addons, and events from the database before the given time.
   *
   * @param time milliseconds since epoch.
   * @param curEnv The ID of the current environment.
   * @return The number of environments and addon entries deleted.
   */
  public int deleteDataBefore(final long time, final int curEnv);

  public int getEventCount();
  public int getEnvironmentCount();

  public void pruneEvents(final int num);
  public void pruneEnvironments(final int num);

  public void enqueueOperation(Runnable runnable);
}
