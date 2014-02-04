/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport;

import java.io.File;
import java.util.ArrayList;
import java.util.concurrent.ConcurrentHashMap;

import org.json.JSONObject;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.healthreport.HealthReportDatabaseStorage;
import org.mozilla.gecko.background.healthreport.HealthReportStorage.MeasurementFields.FieldSpec;

import android.content.ContentValues;
import android.content.Context;
import android.database.sqlite.SQLiteDatabase;

public class MockHealthReportDatabaseStorage extends HealthReportDatabaseStorage {
  public long now = System.currentTimeMillis();

  public long getOneDayAgo() {
    return now - GlobalConstants.MILLISECONDS_PER_DAY;
  }

  public int getYesterday() {
    return super.getDay(this.getOneDayAgo());
  }

  public int getToday() {
    return super.getDay(now);
  }

  public int getTomorrow() {
    return super.getDay(now + GlobalConstants.MILLISECONDS_PER_DAY);
  }

  public int getGivenDaysAgo(int numDays) {
    return super.getDay(this.getGivenDaysAgoMillis(numDays));
  }

  public long getGivenDaysAgoMillis(int numDays) {
    return now - numDays * GlobalConstants.MILLISECONDS_PER_DAY;
  }

  public ConcurrentHashMap<String, Integer> getEnvironmentCache() {
    return this.envs;
  }

  public MockHealthReportDatabaseStorage(Context context, File fakeProfileDirectory) {
    super(context, fakeProfileDirectory);
  }

  public SQLiteDatabase getDB() {
    return this.helper.getWritableDatabase();
  }

  @Override
  public MockDatabaseEnvironment getEnvironment() {
    return new MockDatabaseEnvironment(this);
  }

  @Override
  public int deleteEnvAndEventsBefore(long time, int curEnv) {
    return super.deleteEnvAndEventsBefore(time, curEnv);
  }

  @Override
  public int deleteOrphanedEnv(int curEnv) {
    return super.deleteOrphanedEnv(curEnv);
  }

  @Override
  public int deleteEventsBefore(String dayString) {
    return super.deleteEventsBefore(dayString);
  }

  @Override
  public int deleteOrphanedAddons() {
    return super.deleteOrphanedAddons();
  }

  @Override
  public int getIntFromQuery(final String sql, final String[] selectionArgs) {
    return super.getIntFromQuery(sql, selectionArgs);
  }

  /**
   * A storage instance prepopulated with dummy data to be used for testing.
   *
   * Modifying this data directly will cause tests relying on it to fail so use the versioned
   * constructor to change the data if it's the desired version. Example:
   * <pre>
   *  if (version >= 3) {
   *    addVersion3Stuff();
   *  }
   *  if (version >= 2) {
   *    addVersion2Stuff();
   *  }
   *  addVersion1Stuff();
   * </pre>
   *
   * Don't forget to increment the {@link MAX_VERSION_USED} constant.
   *
   * Note that all instances of this class use the same underlying database and so each newly
   * created instance will share the same data.
   */
  public static class PrepopulatedMockHealthReportDatabaseStorage extends MockHealthReportDatabaseStorage {
    // A constant to enforce which version constructor is the maximum used so far.
    private int MAX_VERSION_USED = 2;

    public String[] measurementNames;
    public int[] measurementVers;
    public FieldSpecContainer[] fieldSpecContainers;
    public int env;
    private final JSONObject addonJSON = new JSONObject(
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

    public static class FieldSpecContainer {
      public final FieldSpec counter;
      public final FieldSpec discrete;
      public final FieldSpec last;

      public FieldSpecContainer(FieldSpec counter, FieldSpec discrete, FieldSpec last) {
        this.counter = counter;
        this.discrete = discrete;
        this.last = last;
      }

      public ArrayList<FieldSpec> asList() {
        final ArrayList<FieldSpec> out = new ArrayList<FieldSpec>(3);
        out.add(counter);
        out.add(discrete);
        out.add(last);
        return out;
      }
    }

    public PrepopulatedMockHealthReportDatabaseStorage(Context context, File fakeProfileDirectory) throws Exception {
      this(context, fakeProfileDirectory, 1);
    }

    public PrepopulatedMockHealthReportDatabaseStorage(Context context, File fakeProfileDirectory, int version) throws Exception {
      super(context, fakeProfileDirectory);

      if (version > MAX_VERSION_USED || version < 1) {
        throw new IllegalStateException("Invalid version number! Check " +
            "PrepopulatedMockHealthReportDatabaseStorage.MAX_VERSION_USED!");
      }

      measurementNames = new String[2];
      measurementNames[0] = "a_string_measurement";
      measurementNames[1] = "b_integer_measurement";

      measurementVers = new int[2];
      measurementVers[0] = 1;
      measurementVers[1] = 2;

      fieldSpecContainers = new FieldSpecContainer[2];
      fieldSpecContainers[0] = new FieldSpecContainer(
          new FieldSpec("a_counter_integer_field", Field.TYPE_INTEGER_COUNTER),
          new FieldSpec("a_discrete_string_field", Field.TYPE_STRING_DISCRETE),
          new FieldSpec("a_last_string_field", Field.TYPE_STRING_LAST));
      fieldSpecContainers[1] = new FieldSpecContainer(
          new FieldSpec("b_counter_integer_field", Field.TYPE_INTEGER_COUNTER),
          new FieldSpec("b_discrete_integer_field", Field.TYPE_INTEGER_DISCRETE),
          new FieldSpec("b_last_integer_field", Field.TYPE_INTEGER_LAST));

      final MeasurementFields[] measurementFields =
          new MeasurementFields[fieldSpecContainers.length];
      for (int i = 0; i < fieldSpecContainers.length; i++) {
        final FieldSpecContainer fieldSpecContainer = fieldSpecContainers[i];
        measurementFields[i] = new MeasurementFields() {
          @Override
          public Iterable<FieldSpec> getFields() {
            return fieldSpecContainer.asList();
          }
        };
      }

      this.beginInitialization();
      for (int i = 0; i < measurementNames.length; i++) {
        this.ensureMeasurementInitialized(measurementNames[i], measurementVers[i],
            measurementFields[i]);
      }
      this.finishInitialization();

      MockDatabaseEnvironment environment = this.getEnvironment();
      environment.mockInit("v123");
      environment.setJSONForAddons(addonJSON);
      env = environment.register();

      String mName = measurementNames[0];
      int mVer = measurementVers[0];
      FieldSpecContainer fieldSpecCont = fieldSpecContainers[0];
      int fieldID = this.getField(mName, mVer, fieldSpecCont.counter.name).getID();
      this.incrementDailyCount(env, this.getGivenDaysAgo(7), fieldID, 1);
      this.incrementDailyCount(env, this.getGivenDaysAgo(4), fieldID, 2);
      this.incrementDailyCount(env, this.getToday(), fieldID, 3);
      fieldID = this.getField(mName, mVer, fieldSpecCont.discrete.name).getID();
      this.recordDailyDiscrete(env, this.getGivenDaysAgo(5), fieldID, "five");
      this.recordDailyDiscrete(env, this.getGivenDaysAgo(5), fieldID, "five-two");
      this.recordDailyDiscrete(env, this.getGivenDaysAgo(2), fieldID, "two");
      this.recordDailyDiscrete(env, this.getToday(), fieldID, "zero");
      fieldID = this.getField(mName, mVer, fieldSpecCont.last.name).getID();
      this.recordDailyLast(env, this.getGivenDaysAgo(6), fieldID, "six");
      this.recordDailyLast(env, this.getGivenDaysAgo(3), fieldID, "three");
      this.recordDailyLast(env, this.getToday(), fieldID, "zero");

      mName = measurementNames[1];
      mVer = measurementVers[1];
      fieldSpecCont = fieldSpecContainers[1];
      fieldID = this.getField(mName, mVer, fieldSpecCont.counter.name).getID();
      this.incrementDailyCount(env, this.getGivenDaysAgo(2), fieldID, 2);
      fieldID = this.getField(mName, mVer, fieldSpecCont.discrete.name).getID();
      this.recordDailyDiscrete(env, this.getToday(), fieldID, 0);
      this.recordDailyDiscrete(env, this.getToday(), fieldID, 1);
      fieldID = this.getField(mName, mVer, fieldSpecCont.last.name).getID();
      this.recordDailyLast(env, this.getYesterday(), fieldID, 1);

      if (version >= 2) {
        // Insert more diverse environments.
        for (int i = 1; i <= 3; i++) {
          environment = this.getEnvironment();
          environment.mockInit("v" + i);
          env = environment.register();
          this.recordDailyLast(env, this.getGivenDaysAgo(7 * i + 1), fieldID, 13);
        }
        environment = this.getEnvironment();
        environment.mockInit("v4");
        env = environment.register();
        this.recordDailyLast(env, this.getGivenDaysAgo(1000), fieldID, 14);
        this.recordDailyLast(env, this.getToday(), fieldID, 15);
      }
    }

    public void insertTextualEvents(final int count) {
      final ContentValues v = new ContentValues();
      v.put("env", env);
      final int fieldID = this.getField(measurementNames[0], measurementVers[0],
          fieldSpecContainers[0].discrete.name).getID();
      v.put("field", fieldID);
      v.put("value", "data");
      final SQLiteDatabase db = this.helper.getWritableDatabase();
      db.beginTransaction();
      try {
        for (int i = 1; i <= count; i++) {
          v.put("date", i);
          db.insertOrThrow("events_textual", null, v);
        }
        db.setTransactionSuccessful();
      } finally {
        db.endTransaction();
      }
    }
  }
}
