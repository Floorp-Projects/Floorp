/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Map.Entry;

import org.mozilla.gecko.background.healthreport.HealthReportDatabaseStorage.DatabaseEnvironment;
import org.mozilla.gecko.background.healthreport.HealthReportStorage.Field;
import org.mozilla.gecko.background.healthreport.HealthReportStorage.MeasurementFields;
import org.mozilla.gecko.background.healthreport.HealthReportStorage.MeasurementFields.FieldSpec;

import android.content.ContentProvider;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.UriMatcher;
import android.database.Cursor;
import android.net.Uri;

/**
 * This is a {@link ContentProvider} wrapper around a database-backed Health
 * Report storage layer.
 *
 * It stores environments, fields, and measurements, and events which refer to
 * each of these by integer ID.
 *
 * Insert = daily discrete.
 * content://org.mozilla.gecko.health/events/env/measurement/v/field
 *
 * Update = daily last or daily counter
 * content://org.mozilla.gecko.health/events/env/measurement/v/field/counter
 * content://org.mozilla.gecko.health/events/env/measurement/v/field/last
 *
 * Delete = drop today's row
 * content://org.mozilla.gecko.health/events/env/measurement/v/field/
 *
 * Query, of course: content://org.mozilla.gecko.health/events/?since
 *
 * Each operation accepts an optional `time` query parameter, formatted as
 * milliseconds since epoch. If omitted, it defaults to the current time.
 *
 * Each operation also accepts mandatory `profilePath` and `env` arguments.
 *
 * TODO: document measurements.
 */
public class HealthReportProvider extends ContentProvider {
  private HealthReportDatabases databases;
  private static final UriMatcher uriMatcher = new UriMatcher(UriMatcher.NO_MATCH);

  public static final String HEALTH_AUTHORITY = HealthReportConstants.HEALTH_AUTHORITY;

  // URI matches.
  private static final int ENVIRONMENTS_ROOT    = 10;
  private static final int EVENTS_ROOT          = 11;
  private static final int EVENTS_RAW_ROOT      = 12;
  private static final int FIELDS_ROOT          = 13;
  private static final int MEASUREMENTS_ROOT    = 14;

  private static final int EVENTS_FIELD_GENERIC = 20;
  private static final int EVENTS_FIELD_COUNTER = 21;
  private static final int EVENTS_FIELD_LAST    = 22;

  private static final int ENVIRONMENT_DETAILS  = 30;
  private static final int FIELDS_MEASUREMENT   = 31;

  static {
    uriMatcher.addURI(HEALTH_AUTHORITY, "environments/", ENVIRONMENTS_ROOT);
    uriMatcher.addURI(HEALTH_AUTHORITY, "events/", EVENTS_ROOT);
    uriMatcher.addURI(HEALTH_AUTHORITY, "rawevents/", EVENTS_RAW_ROOT);
    uriMatcher.addURI(HEALTH_AUTHORITY, "fields/", FIELDS_ROOT);
    uriMatcher.addURI(HEALTH_AUTHORITY, "measurements/", MEASUREMENTS_ROOT);

    uriMatcher.addURI(HEALTH_AUTHORITY, "events/#/*/#/*", EVENTS_FIELD_GENERIC);
    uriMatcher.addURI(HEALTH_AUTHORITY, "events/#/*/#/*/counter", EVENTS_FIELD_COUNTER);
    uriMatcher.addURI(HEALTH_AUTHORITY, "events/#/*/#/*/last", EVENTS_FIELD_LAST);

    uriMatcher.addURI(HEALTH_AUTHORITY, "environments/#", ENVIRONMENT_DETAILS);
    uriMatcher.addURI(HEALTH_AUTHORITY, "fields/*/#", FIELDS_MEASUREMENT);
  }

  /**
   * So we can bypass the ContentProvider layer.
   */
  public HealthReportDatabaseStorage getProfileStorage(final String profilePath) {
    if (profilePath == null) {
      throw new IllegalArgumentException("profilePath must be provided.");
    }
    return databases.getDatabaseHelperForProfile(new File(profilePath));
  }

  private HealthReportDatabaseStorage getProfileStorageForUri(Uri uri) {
    final String profilePath = uri.getQueryParameter("profilePath");
    return getProfileStorage(profilePath);
  }

  @Override
  public void onLowMemory() {
    super.onLowMemory();
    databases.closeDatabaseHelpers();
  }

  @Override
  public String getType(Uri uri) {
    return null;
  }

  @Override
  public boolean onCreate() {
    databases = new HealthReportDatabases(getContext());
    return true;
  }

  @Override
  public Uri insert(Uri uri, ContentValues values) {
    int match = uriMatcher.match(uri);
    HealthReportDatabaseStorage storage = getProfileStorageForUri(uri);
    switch (match) {
    case FIELDS_MEASUREMENT:
      // The keys of this ContentValues are field names.
      List<String> pathSegments = uri.getPathSegments();
      String measurement = pathSegments.get(1);
      int v = Integer.parseInt(pathSegments.get(2));
      storage.ensureMeasurementInitialized(measurement, v, getFieldSpecs(values));
      return uri;

    case ENVIRONMENTS_ROOT:
      DatabaseEnvironment environment = storage.getEnvironment();
      environment.init(values);
      return ContentUris.withAppendedId(uri, environment.register());

    case EVENTS_FIELD_GENERIC:
      long time = getTimeFromUri(uri);
      int day = storage.getDay(time);
      int env = getEnvironmentFromUri(uri);
      Field field = getFieldFromUri(storage, uri);

      if (!values.containsKey("value")) {
        throw new IllegalArgumentException("Must provide ContentValues including 'value' key.");
      }

      Object object = values.get("value");
      if (object instanceof Integer ||
          object instanceof Long) {
        storage.recordDailyDiscrete(env, day, field.getID(), ((Integer) object).intValue());
      } else if (object instanceof String) {
        storage.recordDailyDiscrete(env, day, field.getID(), (String) object);
      } else {
        storage.recordDailyDiscrete(env, day, field.getID(), object.toString());
      }

      // TODO: eventually we might want to return something more useful than
      // the input URI.
      return uri;
    default:
      throw new IllegalArgumentException("Unknown insert URI");
    }
  }

  @Override
  public int update(Uri uri, ContentValues values, String selection,
                    String[] selectionArgs) {

    int match = uriMatcher.match(uri);
    if (match != EVENTS_FIELD_COUNTER &&
        match != EVENTS_FIELD_LAST) {
      throw new IllegalArgumentException("Must provide operation for update.");
    }

    HealthReportStorage storage = getProfileStorageForUri(uri);
    long time = getTimeFromUri(uri);
    int day = storage.getDay(time);
    int env = getEnvironmentFromUri(uri);
    Field field = getFieldFromUri(storage, uri);

    switch (match) {
    case EVENTS_FIELD_COUNTER:
      int by = values.containsKey("value") ? values.getAsInteger("value") : 1;
      storage.incrementDailyCount(env, day, field.getID(), by);
      return 1;

    case EVENTS_FIELD_LAST:
      Object object = values.get("value");
      if (object instanceof Integer ||
          object instanceof Long) {
        storage.recordDailyLast(env, day, field.getID(), ((Integer) object).intValue());
      } else if (object instanceof String) {
        storage.recordDailyLast(env, day, field.getID(), (String) object);
      } else {
        storage.recordDailyLast(env, day, field.getID(), object.toString());
      }
      return 1;

    default:
        // javac's flow control analysis sucks.
        return 0;
    }
  }

  @Override
  public int delete(Uri uri, String selection, String[] selectionArgs) {
    int match = uriMatcher.match(uri);
    HealthReportStorage storage = getProfileStorageForUri(uri);
    switch (match) {
    case MEASUREMENTS_ROOT:
      storage.deleteMeasurements();
      return 1;
    case ENVIRONMENTS_ROOT:
      storage.deleteEnvironments();
      return 1;
    default:
      throw new IllegalArgumentException();
    }

    // TODO: more
  }

  @Override
  public Cursor query(Uri uri, String[] projection, String selection,
                      String[] selectionArgs, String sortOrder) {
    int match = uriMatcher.match(uri);

    HealthReportStorage storage = getProfileStorageForUri(uri);
    switch (match) {
    case EVENTS_ROOT:
      return storage.getEventsSince(getTimeFromUri(uri));
    case EVENTS_RAW_ROOT:
      return storage.getRawEventsSince(getTimeFromUri(uri));
    case MEASUREMENTS_ROOT:
      return storage.getMeasurementVersions();
    case FIELDS_ROOT:
      return storage.getFieldVersions();
    }
    List<String> pathSegments = uri.getPathSegments();
    switch (match) {
    case ENVIRONMENT_DETAILS:
      return storage.getEnvironmentRecordForID(Integer.parseInt(pathSegments.get(1), 10));
    case FIELDS_MEASUREMENT:
      String measurement = pathSegments.get(1);
      int v = Integer.parseInt(pathSegments.get(2));
      return storage.getFieldVersions(measurement, v);
    default:
    return null;
    }
  }

  private static long getTimeFromUri(final Uri uri) {
    String t = uri.getQueryParameter("time");
    if (t == null) {
      return System.currentTimeMillis();
    } else {
      return Long.parseLong(t, 10);
    }
  }

  private static int getEnvironmentFromUri(final Uri uri) {
    return Integer.parseInt(uri.getPathSegments().get(1), 10);
  }

  /**
   * Assumes a URI structured like:
   *
   * <code>content://org.mozilla.gecko.health/events/env/measurement/v/field</code>
   *
   * @param uri a URI formatted as expected.
   * @return a {@link Field} instance.
   */
  private static Field getFieldFromUri(HealthReportStorage storage, final Uri uri) {
    String measurement;
    String field;
    int measurementVersion;

    List<String> pathSegments = uri.getPathSegments();
    measurement = pathSegments.get(2);
    measurementVersion = Integer.parseInt(pathSegments.get(3), 10);
    field = pathSegments.get(4);

    return storage.getField(measurement, measurementVersion, field);
  }

  private MeasurementFields getFieldSpecs(ContentValues values) {
    final ArrayList<FieldSpec> specs = new ArrayList<FieldSpec>(values.size());
    for (Entry<String, Object> entry : values.valueSet()) {
      specs.add(new FieldSpec(entry.getKey(), ((Integer) entry.getValue()).intValue()));
    }

    return new MeasurementFields() {
      @Override
      public Iterable<FieldSpec> getFields() {
        return specs;
      }
    };
  }

}
