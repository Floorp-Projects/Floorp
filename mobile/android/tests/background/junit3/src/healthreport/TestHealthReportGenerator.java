/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport;

import java.io.File;
import java.util.ArrayList;
import java.util.Iterator;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.background.common.DateUtils;
import org.mozilla.gecko.background.healthreport.HealthReportStorage.Field;
import org.mozilla.gecko.background.healthreport.HealthReportStorage.MeasurementFields;
import org.mozilla.gecko.background.helpers.FakeProfileTestCase;

import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.util.SparseArray;

public class TestHealthReportGenerator extends FakeProfileTestCase {
  @SuppressWarnings("static-method")
  public void testOptObject() throws JSONException {
    JSONObject o = new JSONObject();
    o.put("foo", JSONObject.NULL);
    assertEquals(null, o.optJSONObject("foo"));
  }

  @SuppressWarnings("static-method")
  public void testAppend() throws JSONException {
    JSONObject o = new JSONObject();
    HealthReportUtils.append(o, "yyy", 5);
    assertNotNull(o.getJSONArray("yyy"));
    assertEquals(5, o.getJSONArray("yyy").getInt(0));

    o.put("foo", "noo");
    HealthReportUtils.append(o, "foo", "bar");
    assertNotNull(o.getJSONArray("foo"));
    assertEquals("noo", o.getJSONArray("foo").getString(0));
    assertEquals("bar", o.getJSONArray("foo").getString(1));
  }

  @SuppressWarnings("static-method")
  public void testCount() throws JSONException {
    JSONObject o = new JSONObject();
    HealthReportUtils.count(o, "foo", "a");
    HealthReportUtils.count(o, "foo", "b");
    HealthReportUtils.count(o, "foo", "a");
    HealthReportUtils.count(o, "foo", "c");
    HealthReportUtils.count(o, "bar", "a");
    HealthReportUtils.count(o, "bar", "d");
    JSONObject foo = o.getJSONObject("foo");
    JSONObject bar = o.getJSONObject("bar");
    assertEquals(2, foo.getInt("a"));
    assertEquals(1, foo.getInt("b"));
    assertEquals(1, foo.getInt("c"));
    assertFalse(foo.has("d"));
    assertEquals(1, bar.getInt("a"));
    assertEquals(1, bar.getInt("d"));
    assertFalse(bar.has("b"));
  }

  // We don't initialize the env in testHashing, so these are just the default
  // values for the Java types, in order.
  private static final String EXPECTED_MOCK_BASE_HASH = "000nullnullnullnullnullnullnull"
                                                        + "nullnullnullnullnullnull00000";

  // v2 fields.
  private static final String EXPECTED_MOCK_BASE_HASH_SUFFIX = "null" + "null" + 0 + "null";

  public void testHashing() throws JSONException {
    MockHealthReportDatabaseStorage storage = new MockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    MockDatabaseEnvironment env = new MockDatabaseEnvironment(storage, MockDatabaseEnvironment.MockEnvironmentAppender.class);
    env.addons = new JSONObject();

    String addonAHash = "{addonA}={appDisabled==falseforeignInstall==false"
        + "hasBinaryComponents==falseinstallDay==15269scope==1"
        + "type==extensionupdateDay==15602userDisabled==false"
        + "version==1.10}";

    JSONObject addonA1 = new JSONObject("{" +
        "\"userDisabled\": false, " +
        "\"appDisabled\": false, " +
        "\"version\": \"1.10\", " +
        "\"type\": \"extension\", " +
        "\"scope\": 1, " +
        "\"foreignInstall\": false, " +
        "\"hasBinaryComponents\": false, " +
        "\"installDay\": 15269, " +
        "\"updateDay\": 15602 " +
    "}");

    // A reordered but otherwise equivalent object.
    JSONObject addonA1rev = new JSONObject("{" +
        "\"userDisabled\": false, " +
        "\"foreignInstall\": false, " +
        "\"hasBinaryComponents\": false, " +
        "\"installDay\": 15269, " +
        "\"type\": \"extension\", " +
        "\"scope\": 1, " +
        "\"appDisabled\": false, " +
        "\"version\": \"1.10\", " +
        "\"updateDay\": 15602 " +
    "}");
    env.addons.put("{addonA}", addonA1);

    assertEquals(EXPECTED_MOCK_BASE_HASH + addonAHash + EXPECTED_MOCK_BASE_HASH_SUFFIX, env.getHash());

    env.addons.put("{addonA}", addonA1rev);
    assertEquals(EXPECTED_MOCK_BASE_HASH + addonAHash + EXPECTED_MOCK_BASE_HASH_SUFFIX, env.getHash());
  }

  private void assertJSONDiff(JSONObject source, JSONObject diff) throws JSONException {
    assertEquals(source.get("a"), diff.get("a"));
    assertFalse(diff.has("b"));
    assertEquals(source.get("c"), diff.get("c"));
    JSONObject diffD = diff.getJSONObject("d");
    assertFalse(diffD.has("aa"));
    assertEquals(1, diffD.getJSONArray("bb").getInt(0));
    JSONObject diffCC = diffD.getJSONObject("cc");
    assertEquals(1, diffCC.length());
    assertEquals(1, diffCC.getInt("---"));
  }

  private static void assertJSONEquals(JSONObject one, JSONObject two) throws JSONException {
    if (one == null || two == null) {
      assertEquals(two, one);
    }
    assertEquals(one.length(), two.length());
    @SuppressWarnings("unchecked")
    Iterator<String> it = one.keys();
    while (it.hasNext()) {
      String key = it.next();
      Object v1 = one.get(key);
      Object v2 = two.get(key);
      if (v1 instanceof JSONObject) {
        assertTrue(v2 instanceof JSONObject);
        assertJSONEquals((JSONObject) v1, (JSONObject) v2);
      } else {
        assertEquals(v1, v2);
      }
    }
  }

  @SuppressWarnings("static-method")
  public void testNulls() {
    assertTrue(JSONObject.NULL.equals(null));
    assertTrue(JSONObject.NULL.equals(JSONObject.NULL));
    assertFalse(JSONObject.NULL.equals(new JSONObject()));
    assertFalse(null == JSONObject.NULL);
  }

  public void testJSONDiffing() throws JSONException {
    String one = "{\"a\": 1, \"b\": 2, \"c\": [1, 2, 3], \"d\": {\"aa\": 5, \"bb\": [], \"cc\": {\"aaa\": null}}, \"e\": {}}";
    String two = "{\"a\": 2, \"b\": 2, \"c\": [1, null, 3], \"d\": {\"aa\": 5, \"bb\": [1], \"cc\": {\"---\": 1, \"aaa\": null}}}";
    JSONObject jOne = new JSONObject(one);
    JSONObject jTwo = new JSONObject(two);
    JSONObject diffNull = HealthReportGenerator.diff(jOne, jTwo, true);
    JSONObject diffNoNull = HealthReportGenerator.diff(jOne, jTwo, false);
    assertJSONDiff(jTwo, diffNull);
    assertJSONDiff(jTwo, diffNoNull);
    assertTrue(diffNull.isNull("e"));
    assertFalse(diffNoNull.has("e"));

    // Diffing to null returns the negation object: all the same keys but all null values.
    JSONObject negated = new JSONObject("{\"a\": null, \"b\": null, \"c\": null, \"d\": null, \"e\": null}");
    JSONObject toNull = HealthReportGenerator.diff(jOne, null, true);
    assertJSONEquals(toNull, negated);

    // Diffing from null returns the destination object.
    JSONObject fromNull = HealthReportGenerator.diff(null, jOne, true);
    assertJSONEquals(fromNull, jOne);
  }

  public void testAddonDiffing() throws JSONException {
    MockHealthReportDatabaseStorage storage = new MockHealthReportDatabaseStorage(
                                                                                  context,
                                                                                  fakeProfileDirectory);

    final MockDatabaseEnvironment env1 = storage.getEnvironment();
    env1.mockInit("23");
    final MockDatabaseEnvironment env2 = storage.getEnvironment();
    env2.mockInit("23");

    env1.addons = new JSONObject();
    env2.addons = new JSONObject();

    JSONObject addonA1 = new JSONObject("{" + "\"userDisabled\": false, "
                                        + "\"appDisabled\": false, "
                                        + "\"version\": \"1.10\", "
                                        + "\"type\": \"extension\", "
                                        + "\"scope\": 1, "
                                        + "\"foreignInstall\": false, "
                                        + "\"hasBinaryComponents\": false, "
                                        + "\"installDay\": 15269, "
                                        + "\"updateDay\": 15602 " + "}");
    JSONObject addonA2 = new JSONObject("{" + "\"userDisabled\": false, "
                                        + "\"appDisabled\": false, "
                                        + "\"version\": \"1.20\", "
                                        + "\"type\": \"extension\", "
                                        + "\"scope\": 1, "
                                        + "\"foreignInstall\": false, "
                                        + "\"hasBinaryComponents\": false, "
                                        + "\"installDay\": 15269, "
                                        + "\"updateDay\": 17602 " + "}");
    JSONObject addonB1 = new JSONObject("{" + "\"userDisabled\": false, "
                                        + "\"appDisabled\": false, "
                                        + "\"version\": \"1.0\", "
                                        + "\"type\": \"theme\", "
                                        + "\"scope\": 1, "
                                        + "\"foreignInstall\": false, "
                                        + "\"hasBinaryComponents\": false, "
                                        + "\"installDay\": 10269, "
                                        + "\"updateDay\": 10002 " + "}");
    JSONObject addonC1 = new JSONObject("{" + "\"userDisabled\": true, "
                                        + "\"appDisabled\": false, "
                                        + "\"version\": \"1.50\", "
                                        + "\"type\": \"plugin\", "
                                        + "\"scope\": 1, "
                                        + "\"foreignInstall\": false, "
                                        + "\"hasBinaryComponents\": true, "
                                        + "\"installDay\": 12269, "
                                        + "\"updateDay\": 12602 " + "}");
    env1.addons.put("{addonA}", addonA1);
    env1.addons.put("{addonB}", addonB1);
    env2.addons.put("{addonA}", addonA2);
    env2.addons.put("{addonB}", addonB1);
    env2.addons.put("{addonC}", addonC1);

    JSONObject env2JSON = HealthReportGenerator.jsonify(env2, env1);
    JSONObject addons = env2JSON.getJSONObject("org.mozilla.addons.active");
    assertTrue(addons.has("{addonA}"));
    assertFalse(addons.has("{addonB}")); // Because it's unchanged.
    assertTrue(addons.has("{addonC}"));
    JSONObject aJSON = addons.getJSONObject("{addonA}");
    assertEquals(2, aJSON.length());
    assertEquals("1.20", aJSON.getString("version"));
    assertEquals(17602, aJSON.getInt("updateDay"));
    JSONObject cJSON = addons.getJSONObject("{addonC}");
    assertEquals(9, cJSON.length());
  }

  public void testEnvironments() throws JSONException {
    // Hard-coded so you need to update tests!
    // If this is the only thing you need to change when revving a version, you
    // need more test coverage.
    final int expectedVersion = 3;

    MockHealthReportDatabaseStorage storage = new MockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    HealthReportGenerator gen = new HealthReportGenerator(storage);

    final MockDatabaseEnvironment env1 = storage.getEnvironment();
    env1.mockInit("23");
    final String env1Hash = env1.getHash();

    long now = System.currentTimeMillis();
    JSONObject document = gen.generateDocument(0, 0, env1);
    String today = new DateUtils.DateFormatter().getDateString(now);

    assertFalse(document.has("lastPingDate"));
    document = gen.generateDocument(0, HealthReportConstants.EARLIEST_LAST_PING, env1);
    assertEquals("2013-05-02", document.get("lastPingDate"));

    // True unless test spans midnight...
    assertEquals(today, document.get("thisPingDate"));
    assertEquals(expectedVersion, document.get("version"));

    JSONObject environments = document.getJSONObject("environments");
    JSONObject current = environments.getJSONObject("current");
    assertTrue(current.has("org.mozilla.profile.age"));
    assertTrue(current.has("org.mozilla.sysinfo.sysinfo"));
    assertTrue(current.has("org.mozilla.appInfo.appinfo"));
    assertTrue(current.has("geckoAppInfo"));
    assertTrue(current.has("org.mozilla.addons.active"));
    assertTrue(current.has("org.mozilla.addons.counts"));

    // Make sure we don't get duplicate environments when an environment has
    // been used, and that we get deltas between them.
    env1.register();
    final MockDatabaseEnvironment env2 = storage.getEnvironment();
    env2.mockInit("24");
    final String env2Hash = env2.getHash();
    assertFalse(env2Hash.equals(env1Hash));
    env2.register();
    assertEquals(env2Hash, env2.getHash());

    assertEquals("2013-05-02", document.get("lastPingDate"));

    // True unless test spans midnight...
    assertEquals(today, document.get("thisPingDate"));
    assertEquals(expectedVersion, document.get("version"));
    document = gen.generateDocument(0, HealthReportConstants.EARLIEST_LAST_PING, env2);
    environments = document.getJSONObject("environments");

    // Now we have two: env1, and env2 (as 'current').
    assertTrue(environments.has(env1.getHash()));
    assertTrue(environments.has("current"));
    assertEquals(2, environments.length());

    current = environments.getJSONObject("current");
    assertTrue(current.has("org.mozilla.profile.age"));
    assertTrue(current.has("org.mozilla.sysinfo.sysinfo"));
    assertTrue(current.has("org.mozilla.appInfo.appinfo"));
    assertTrue(current.has("geckoAppInfo"));
    assertTrue(current.has("org.mozilla.addons.active"));
    assertTrue(current.has("org.mozilla.addons.counts"));

    // The diff only contains the changed measurement and fields.
    JSONObject previous = environments.getJSONObject(env1.getHash());
    assertTrue(previous.has("geckoAppInfo"));
    final JSONObject previousAppInfo = previous.getJSONObject("geckoAppInfo");
    assertEquals(2, previousAppInfo.length());
    assertEquals("23", previousAppInfo.getString("version"));
    assertEquals(Integer.valueOf(1), (Integer) previousAppInfo.get("_v"));

    assertFalse(previous.has("org.mozilla.profile.age"));
    assertFalse(previous.has("org.mozilla.sysinfo.sysinfo"));
    assertFalse(previous.has("org.mozilla.appInfo.appinfo"));
    assertFalse(previous.has("org.mozilla.addons.active"));
    assertFalse(previous.has("org.mozilla.addons.counts"));
  }

  public void testInsertedData() throws JSONException {
    MockHealthReportDatabaseStorage storage = new MockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    HealthReportGenerator gen = new HealthReportGenerator(storage);

    storage.beginInitialization();

    final MockDatabaseEnvironment environment = storage.getEnvironment();
    String envHash = environment.getHash();
    int env = environment.mockInit("23").register();

    storage.ensureMeasurementInitialized("org.mozilla.testm5", 1, new MeasurementFields() {
      @Override
      public Iterable<FieldSpec> getFields() {
        ArrayList<FieldSpec> out = new ArrayList<FieldSpec>();
        out.add(new FieldSpec("counter", Field.TYPE_INTEGER_COUNTER));
        out.add(new FieldSpec("discrete_int", Field.TYPE_INTEGER_DISCRETE));
        out.add(new FieldSpec("discrete_str", Field.TYPE_STRING_DISCRETE));
        out.add(new FieldSpec("last_int", Field.TYPE_INTEGER_LAST));
        out.add(new FieldSpec("last_str", Field.TYPE_STRING_LAST));
        out.add(new FieldSpec("counted_str", Field.TYPE_COUNTED_STRING_DISCRETE));
        out.add(new FieldSpec("discrete_json", Field.TYPE_JSON_DISCRETE));
        return out;
      }
    });

    storage.finishInitialization();

    long now = System.currentTimeMillis();
    int day = storage.getDay(now);
    final String todayString = new DateUtils.DateFormatter().getDateString(now);

    int counter = storage.getField("org.mozilla.testm5", 1, "counter").getID();
    int discrete_int = storage.getField("org.mozilla.testm5", 1, "discrete_int").getID();
    int discrete_str = storage.getField("org.mozilla.testm5", 1, "discrete_str").getID();
    int last_int = storage.getField("org.mozilla.testm5", 1, "last_int").getID();
    int last_str = storage.getField("org.mozilla.testm5", 1, "last_str").getID();
    int counted_str = storage.getField("org.mozilla.testm5", 1, "counted_str").getID();
    int discrete_json = storage.getField("org.mozilla.testm5", 1, "discrete_json").getID();

    storage.incrementDailyCount(env, day, counter, 2);
    storage.incrementDailyCount(env, day, counter, 3);
    storage.recordDailyLast(env, day, last_int, 2);
    storage.recordDailyLast(env, day, last_str, "a");
    storage.recordDailyLast(env, day, last_int, 3);
    storage.recordDailyLast(env, day, last_str, "b");
    storage.recordDailyDiscrete(env, day, discrete_str, "a");
    storage.recordDailyDiscrete(env, day, discrete_str, "b");
    storage.recordDailyDiscrete(env, day, discrete_int, 2);
    storage.recordDailyDiscrete(env, day, discrete_int, 1);
    storage.recordDailyDiscrete(env, day, discrete_int, 3);
    storage.recordDailyDiscrete(env, day, counted_str, "aaa");
    storage.recordDailyDiscrete(env, day, counted_str, "ccc");
    storage.recordDailyDiscrete(env, day, counted_str, "bbb");
    storage.recordDailyDiscrete(env, day, counted_str, "aaa");

    JSONObject objA = new JSONObject();
    objA.put("foo", "bar");
    storage.recordDailyDiscrete(env, day, discrete_json, (JSONObject) null);
    storage.recordDailyDiscrete(env, day, discrete_json, "null");              // Still works because JSON is a string internally.
    storage.recordDailyDiscrete(env, day, discrete_json, objA);

    JSONObject document = gen.generateDocument(0, HealthReportConstants.EARLIEST_LAST_PING, environment);
    JSONObject today = document.getJSONObject("data").getJSONObject("days").getJSONObject(todayString);
    assertEquals(1, today.length());
    JSONObject measurement = today.getJSONObject(envHash).getJSONObject("org.mozilla.testm5");
    assertEquals(1, measurement.getInt("_v"));
    assertEquals(5, measurement.getInt("counter"));
    assertEquals(3, measurement.getInt("last_int"));
    assertEquals("b", measurement.getString("last_str"));
    JSONArray discreteInts = measurement.getJSONArray("discrete_int");
    JSONArray discreteStrs = measurement.getJSONArray("discrete_str");
    assertEquals(3, discreteInts.length());
    assertEquals(2, discreteStrs.length());
    assertEquals("a", discreteStrs.get(0));
    assertEquals("b", discreteStrs.get(1));
    assertEquals(Long.valueOf(2), discreteInts.get(0));
    assertEquals(Long.valueOf(1), discreteInts.get(1));
    assertEquals(Long.valueOf(3), discreteInts.get(2));
    JSONObject counted = measurement.getJSONObject("counted_str");
    assertEquals(2, counted.getInt("aaa"));
    assertEquals(1, counted.getInt("bbb"));
    assertEquals(1, counted.getInt("ccc"));
    assertFalse(counted.has("ddd"));
    JSONArray discreteJSON = measurement.getJSONArray("discrete_json");
    assertEquals(3, discreteJSON.length());
    assertEquals(JSONObject.NULL, discreteJSON.get(0));
    assertEquals(JSONObject.NULL, discreteJSON.get(1));
    assertEquals("bar", discreteJSON.getJSONObject(2).getString("foo"));
  }

  @Override
  protected String getCacheSuffix() {
    return File.separator + "health-" + System.currentTimeMillis() + ".profile";
  }

  public void testEnvironmentDiffing() throws JSONException {
    // Manually insert a v1 environment.
    final MockHealthReportDatabaseStorage storage = new MockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    final SQLiteDatabase db = storage.getDB();
    storage.deleteEverything();
    final MockDatabaseEnvironment v1env = storage.getEnvironment();
    v1env.mockInit("27.0a1");
    v1env.version = 1;
    v1env.appLocale = "";
    v1env.osLocale  = "";
    v1env.distribution = "";
    v1env.acceptLangSet = 0;
    final int v1ID = v1env.register();

    // Verify.
    final String[] cols = new String[] {
      "id", "version", "hash",
      "osLocale", "acceptLangSet", "appLocale", "distribution"
    };

    final Cursor c1 = db.query("environments", cols, "id = " + v1ID, null, null, null, null);
    String v1envHash;
    try {
      assertTrue(c1.moveToFirst());
      assertEquals(1, c1.getCount());

      assertEquals(v1ID, c1.getInt(0));
      assertEquals(1,    c1.getInt(1));

      v1envHash = c1.getString(2);
      assertNotNull(v1envHash);
      assertEquals("", c1.getString(3));
      assertEquals(0,  c1.getInt(4));
      assertEquals("", c1.getString(5));
      assertEquals("", c1.getString(6));
    } finally {
      c1.close();
    }

    // Insert a v2 environment.
    final MockDatabaseEnvironment v2env = storage.getEnvironment();
    v2env.mockInit("27.0a1");
    v2env.appLocale = v2env.osLocale = "en_us";
    v2env.acceptLangSet = 1;

    final int v2ID = v2env.register();
    assertFalse(v1ID == v2ID);
    final Cursor c2 = db.query("environments", cols, "id = " + v2ID, null, null, null, null);
    String v2envHash;
    try {
      assertTrue(c2.moveToFirst());
      assertEquals(1, c2.getCount());

      assertEquals(v2ID, c2.getInt(0));
      assertEquals(2,    c2.getInt(1));

      v2envHash = c2.getString(2);
      assertNotNull(v2envHash);
      assertEquals("en_us", c2.getString(3));
      assertEquals(1,       c2.getInt(4));
      assertEquals("en_us", c2.getString(5));
      assertEquals("",      c2.getString(6));
    } finally {
      c2.close();
    }

    assertFalse(v1envHash.equals(v2envHash));

    // Now let's diff based on DB contents.
    SparseArray<Environment> envs = storage.getEnvironmentRecordsByID();

    JSONObject oldEnv = HealthReportGenerator.jsonify(envs.get(v1ID), null).getJSONObject("org.mozilla.appInfo.appinfo");
    JSONObject newEnv = HealthReportGenerator.jsonify(envs.get(v2ID), null).getJSONObject("org.mozilla.appInfo.appinfo");

    // Generate the new env as if the old were the current. This should rarely happen in practice.
    // Fields supported by the new env but not the old will appear, even if the 'default' for the
    // old implementation is equal to the new env's value.
    JSONObject newVsOld = HealthReportGenerator.jsonify(envs.get(v2ID), envs.get(v1ID)).getJSONObject("org.mozilla.appInfo.appinfo");

    // Generate the old env as if the new were the current. This is normal. Fields not supported by the old
    // environment version should not appear in the output.
    JSONObject oldVsNew = HealthReportGenerator.jsonify(envs.get(v1ID), envs.get(v2ID)).getJSONObject("org.mozilla.appInfo.appinfo");
    assertEquals(2, oldEnv.getInt("_v"));
    assertEquals(3, newEnv.getInt("_v"));
    assertEquals(2, oldVsNew.getInt("_v"));
    assertEquals(3, newVsOld.getInt("_v"));

    assertFalse(oldVsNew.has("osLocale"));
    assertFalse(oldVsNew.has("appLocale"));
    assertFalse(oldVsNew.has("distribution"));
    assertFalse(oldVsNew.has("acceptLangIsUserSet"));

    assertTrue(newVsOld.has("osLocale"));
    assertTrue(newVsOld.has("appLocale"));
    assertTrue(newVsOld.has("distribution"));
    assertTrue(newVsOld.has("acceptLangIsUserSet"));
  }
}
