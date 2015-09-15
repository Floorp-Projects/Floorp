/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport.test;

import org.json.JSONObject;

import android.database.Cursor;
import android.util.SparseArray;

import org.mozilla.gecko.background.healthreport.Environment;
import org.mozilla.gecko.background.healthreport.HealthReportStorage;

public class HealthReportStorageStub implements HealthReportStorage {
  public void close() { throw new UnsupportedOperationException(); }

  public int getDay(long time) { throw new UnsupportedOperationException(); }
  public int getDay() { throw new UnsupportedOperationException(); }

  public Environment getEnvironment() { throw new UnsupportedOperationException(); }
  public SparseArray<String> getEnvironmentHashesByID() { throw new UnsupportedOperationException(); }
  public SparseArray<Environment> getEnvironmentRecordsByID() { throw new UnsupportedOperationException(); }
  public Cursor getEnvironmentRecordForID(int id) { throw new UnsupportedOperationException(); }

  public Field getField(String measurement, int measurementVersion, String fieldName) {
    throw new UnsupportedOperationException();
  }
  public SparseArray<Field> getFieldsByID() { throw new UnsupportedOperationException(); }

  public void recordDailyLast(int env, int day, int field, JSONObject value) { throw new UnsupportedOperationException(); }
  public void recordDailyLast(int env, int day, int field, String value) { throw new UnsupportedOperationException(); }
  public void recordDailyLast(int env, int day, int field, int value) { throw new UnsupportedOperationException(); }
  public void recordDailyDiscrete(int env, int day, int field, JSONObject value) { throw new UnsupportedOperationException(); }
  public void recordDailyDiscrete(int env, int day, int field, String value) { throw new UnsupportedOperationException(); }
  public void recordDailyDiscrete(int env, int day, int field, int value) { throw new UnsupportedOperationException(); }
  public void incrementDailyCount(int env, int day, int field, int by) { throw new UnsupportedOperationException(); }
  public void incrementDailyCount(int env, int day, int field) { throw new UnsupportedOperationException(); }

  public boolean hasEventSince(long time) { throw new UnsupportedOperationException(); }

  public Cursor getRawEventsSince(long time) { throw new UnsupportedOperationException(); }

  public Cursor getEventsSince(long time) { throw new UnsupportedOperationException(); }

  public void ensureMeasurementInitialized(String measurement, int version, MeasurementFields fields) {
    throw new UnsupportedOperationException();
  }
  public Cursor getMeasurementVersions() { throw new UnsupportedOperationException(); }
  public Cursor getFieldVersions() { throw new UnsupportedOperationException(); }
  public Cursor getFieldVersions(String measurement, int measurementVersion) { throw new UnsupportedOperationException(); }

  public void deleteEverything() { throw new UnsupportedOperationException(); }
  public void deleteEnvironments() { throw new UnsupportedOperationException(); }
  public void deleteMeasurements() { throw new UnsupportedOperationException(); }
  public int deleteDataBefore(final long time, final int curEnv) { throw new UnsupportedOperationException(); }

  public int getEventCount() { throw new UnsupportedOperationException(); }
  public int getEnvironmentCount() { throw new UnsupportedOperationException(); }

  public void pruneEvents(final int num) { throw new UnsupportedOperationException(); }
  public void pruneEnvironments(final int num) { throw new UnsupportedOperationException(); }

  public void enqueueOperation(Runnable runnable) { throw new UnsupportedOperationException(); }
}
