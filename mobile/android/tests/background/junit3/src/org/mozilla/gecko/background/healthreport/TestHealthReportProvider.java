/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport;

import org.mozilla.gecko.background.helpers.DBHelpers;
import org.mozilla.gecko.background.helpers.DBProviderTestCase;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.test.mock.MockContentResolver;

public class TestHealthReportProvider extends DBProviderTestCase<HealthReportProvider> {
  protected static final int MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

  public TestHealthReportProvider() {
    super(HealthReportProvider.class, HealthReportProvider.HEALTH_AUTHORITY);
  }

  public TestHealthReportProvider(Class<HealthReportProvider> providerClass,
                                  String providerAuthority) {
    super(providerClass, providerAuthority);
  }

  private Uri getCompleteUri(String rest) {
    return Uri.parse("content://" + HealthReportProvider.HEALTH_AUTHORITY + rest +
                     (rest.indexOf('?') == -1 ? "?" : "&") +
                     "profilePath=" + Uri.encode(fakeProfileDirectory.getAbsolutePath()));
  }

  private void ensureCount(int expected, Uri uri) {
    final MockContentResolver resolver = getMockContentResolver();
    Cursor cursor = resolver.query(uri, null, null, null, null);
    assertNotNull(cursor);
    assertEquals(expected, cursor.getCount());
    cursor.close();
  }

  private void ensureMeasurementCount(int expected) {
    final Uri measurements = getCompleteUri("/measurements/");
    ensureCount(expected, measurements);
  }

  private void ensureFieldCount(int expected) {
    final Uri fields = getCompleteUri("/fields/");
    ensureCount(expected, fields);
  }

  public void testNonExistentMeasurement() {
    assertNotNull(getContext());
    Uri u = getCompleteUri("/events/" + 0 + "/" + "testm" + "/" + 3 + "/" + "testf");
    ContentValues v = new ContentValues();
    v.put("value", 5);
    ContentResolver r = getMockContentResolver();
    assertNotNull(r);
    try {
      r.insert(u, v);
      fail("Should throw.");
    } catch (IllegalStateException e) {
      assertTrue(e.getMessage().contains("No field with name testf"));
    }
  }

  public void testEnsureMeasurements() {
    ensureMeasurementCount(0);

    final MockContentResolver resolver = getMockContentResolver();

    // Note that we insert no fields. These are empty measurements.
    ContentValues values = new ContentValues();
    resolver.insert(getCompleteUri("/fields/testm1/1"), values);
    ensureMeasurementCount(1);

    resolver.insert(getCompleteUri("/fields/testm1/1"), values);
    ensureMeasurementCount(1);

    resolver.insert(getCompleteUri("/fields/testm1/3"), values);
    ensureMeasurementCount(2);

    resolver.insert(getCompleteUri("/fields/testm2/1"), values);
    ensureMeasurementCount(3);

    Cursor cursor = resolver.query(getCompleteUri("/measurements/"), null, null, null, null);

    assertTrue(cursor.moveToFirst());
    assertEquals("testm1", cursor.getString(1));    // 'id' is column 0.
    assertEquals(1,        cursor.getInt(2));

    assertTrue(cursor.moveToNext());
    assertEquals("testm1", cursor.getString(1));
    assertEquals(3,        cursor.getInt(2));

    assertTrue(cursor.moveToNext());
    assertEquals("testm2", cursor.getString(1));
    assertEquals(1,        cursor.getInt(2));
    assertFalse(cursor.moveToNext());

    cursor.close();

    resolver.delete(getCompleteUri("/measurements/"), null, null);
  }

  /**
   * Return true if the two times occur on the same UTC day.
   */
  private static boolean sameDay(long start, long end) {
   return Math.floor(start / MILLISECONDS_PER_DAY) ==
          Math.floor(end / MILLISECONDS_PER_DAY);
  }

  private static int getDay(long time) {
    return (int) Math.floor(time / MILLISECONDS_PER_DAY);
  }


  public void testRealData() {
    ensureMeasurementCount(0);
    long start = System.currentTimeMillis();
    int day = getDay(start);

    final MockContentResolver resolver = getMockContentResolver();

    // Register a provider with four fields.
    ContentValues values = new ContentValues();
    values.put("counter1", 1);
    values.put("counter2", 4);
    values.put("last1",    7);
    values.put("discrete1", 11);

    resolver.insert(getCompleteUri("/fields/testm1/1"), values);
    ensureMeasurementCount(1);
    ensureFieldCount(4);

    final Uri envURI = resolver.insert(getCompleteUri("/environments/"), getTestEnvContentValues());
    String envHash = null;
    Cursor envCursor = resolver.query(envURI, null, null, null, null);
    try {
      assertTrue(envCursor.moveToFirst());
      envHash = envCursor.getString(2);      // id, version, hash, ...
    } finally {
      envCursor.close();
    }

    final Uri eventURI = HealthReportUtils.getEventURI(envURI);

    Uri discrete1 = eventURI.buildUpon().appendEncodedPath("testm1/1/discrete1").build();
    Uri counter1 = eventURI.buildUpon().appendEncodedPath("testm1/1/counter1/counter").build();
    Uri counter2 = eventURI.buildUpon().appendEncodedPath("testm1/1/counter2/counter").build();
    Uri last1 = eventURI.buildUpon().appendEncodedPath("testm1/1/last1/last").build();

    ContentValues discreteS = new ContentValues();
    ContentValues discreteI = new ContentValues();

    discreteS.put("value", "Some string");
    discreteI.put("value", 9);
    resolver.insert(discrete1, discreteS);
    resolver.insert(discrete1, discreteI);

    ContentValues counter = new ContentValues();
    resolver.update(counter1, counter, null, null);   // Defaults to 1.
    resolver.update(counter2, counter, null, null);   // Defaults to 1.
    counter.put("value", 3);
    resolver.update(counter2, counter, null, null);   // Increment by 3.

    // Interleaving.
    discreteS.put("value", "Some other string");
    discreteI.put("value", 3);
    resolver.insert(discrete1, discreteS);
    resolver.insert(discrete1, discreteI);

    // Note that we explicitly do not support last-values transitioning between types.
    ContentValues last = new ContentValues();
    last.put("value", 123);
    resolver.update(last1, last, null, null);
    last.put("value", 245);
    resolver.update(last1, last, null, null);

    int expectedRows = 2 + 1 + 4;   // Two counters, one last, four entries for discrete.

    // Now let's see what comes up in the query!
    // We'll do "named" first -- the results include strings.
    Cursor cursor = resolver.query(getCompleteUri("/events/?time=" + start), null, null, null, null);
    assertEquals(expectedRows, cursor.getCount());
    assertTrue(cursor.moveToFirst());

    // Let's be safe in case someone runs this test at midnight.
    long end = System.currentTimeMillis();
    if (!sameDay(start, end)) {
      System.out.println("Aborting testAddData: spans midnight.");
      cursor.close();
      return;
    }

    // "date", "env", m, mv, f, f_flags, "value"
    Object[][] expected = {
      {day, envHash, "testm1", 1, "counter1", null, 1},
      {day, envHash, "testm1", 1, "counter2", null, 4},

      // Discrete values don't preserve order of insertion across types, but
      // this actually isn't really permitted -- fields have a single type.
      {day, envHash, "testm1", 1, "discrete1", null, 9},
      {day, envHash, "testm1", 1, "discrete1", null, 3},
      {day, envHash, "testm1", 1, "discrete1", null, "Some string"},
      {day, envHash, "testm1", 1, "discrete1", null, "Some other string"},
      {day, envHash, "testm1", 1, "last1", null, 245},
    };


    DBHelpers.assertCursorContains(expected, cursor);
    cursor.close();

    resolver.delete(getCompleteUri("/measurements/"), null, null);
    ensureMeasurementCount(0);
    ensureFieldCount(0);
  }

  private ContentValues getTestEnvContentValues() {
    ContentValues v = new ContentValues();
    v.put("profileCreation", 0);
    v.put("cpuCount", 0);
    v.put("memoryMB", 0);

    v.put("isBlocklistEnabled", 0);
    v.put("isTelemetryEnabled", 0);
    v.put("extensionCount", 0);
    v.put("pluginCount", 0);
    v.put("themeCount", 0);

    v.put("architecture", "");
    v.put("sysName", "");
    v.put("sysVersion", "");
    v.put("vendor", "");
    v.put("appName", "");
    v.put("appID", "");
    v.put("appVersion", "");
    v.put("appBuildID", "");
    v.put("platformVersion", "");
    v.put("platformBuildID", "");
    v.put("os", "");
    v.put("xpcomabi", "");
    v.put("updateChannel", "");

    // v2.
    v.put("distribution", "");
    v.put("osLocale", "en_us");
    v.put("appLocale", "en_us");
    v.put("acceptLangSet", 0);

    // v3.
    v.put("hasHardwareKeyboard", 0);
    v.put("uiMode", 0);
    v.put("uiType", "default");
    v.put("screenLayout", 0);
    v.put("screenXInMM", 100);
    v.put("screenYInMM", 140);

    return v;
  }
}
