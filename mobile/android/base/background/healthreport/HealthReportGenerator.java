/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.background.common.DateUtils.DateFormatter;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.healthreport.HealthReportStorage.Field;

import android.database.Cursor;
import android.util.SparseArray;

public class HealthReportGenerator {
  private static final int PAYLOAD_VERSION = 3;

  private static final String LOG_TAG = "GeckoHealthGen";

  private final HealthReportStorage storage;
  private final DateFormatter dateFormatter;

  public HealthReportGenerator(HealthReportStorage storage) {
    this.storage = storage;
    this.dateFormatter = new DateFormatter();
  }

  @SuppressWarnings("static-method")
  protected long now() {
    return System.currentTimeMillis();
  }

  /**
   * @return null if no environment could be computed, or else the resulting document.
   * @throws JSONException if there was an error adding environment data to the resulting document.
   */
  public JSONObject generateDocument(long since, long lastPingTime, String profilePath) throws JSONException {
    Logger.info(LOG_TAG, "Generating FHR document from " + since + "; last ping " + lastPingTime);
    Logger.pii(LOG_TAG, "Generating for profile " + profilePath);
    ProfileInformationCache cache = new ProfileInformationCache(profilePath);
    if (!cache.restoreUnlessInitialized()) {
      Logger.warn(LOG_TAG, "Not enough profile information to compute current environment.");
      return null;
    }
    Environment current = EnvironmentBuilder.getCurrentEnvironment(cache);
    return generateDocument(since, lastPingTime, current);
  }

  /**
   * The document consists of:
   *
   *<ul>
   *<li>Basic metadata: last ping time, current ping time, version.</li>
   *<li>A map of environments: <code>current</code> and others named by hash. <code>current</code> is fully specified,
   * and others are deltas from current.</li>
   *<li>A <code>data</code> object. This includes <code>last</code> and <code>days</code>.</li>
   *</ul>
   *
   * <code>days</code> is a map from date strings to <tt>{hash: {measurement: {_v: version, fields...}}}</tt>.
   * @throws JSONException if there was an error adding environment data to the resulting document.
   */
  public JSONObject generateDocument(long since, long lastPingTime, Environment currentEnvironment) throws JSONException {
    final String currentHash = currentEnvironment.getHash();

    Logger.debug(LOG_TAG, "Current environment hash: " + currentHash);
    if (currentHash == null) {
      Logger.warn(LOG_TAG, "Current hash is null; aborting.");
      return null;
    }

    // We want to map field IDs to some strings as we go.
    SparseArray<Environment> envs = storage.getEnvironmentRecordsByID();

    JSONObject document = new JSONObject();

    if (lastPingTime >= HealthReportConstants.EARLIEST_LAST_PING) {
      document.put("lastPingDate", dateFormatter.getDateString(lastPingTime));
    }

    document.put("thisPingDate", dateFormatter.getDateString(now()));
    document.put("version", PAYLOAD_VERSION);

    document.put("environments", getEnvironmentsJSON(currentEnvironment, envs));
    document.put("data", getDataJSON(currentEnvironment, envs, since));

    return document;
  }

  protected JSONObject getDataJSON(Environment currentEnvironment,
                                   SparseArray<Environment> envs, long since) throws JSONException {
    SparseArray<Field> fields = storage.getFieldsByID();

    JSONObject days = getDaysJSON(currentEnvironment, envs, fields, since);

    JSONObject last = new JSONObject();

    JSONObject data = new JSONObject();
    data.put("days", days);
    data.put("last", last);
    return data;
  }

  protected JSONObject getDaysJSON(Environment currentEnvironment, SparseArray<Environment> envs, SparseArray<Field> fields, long since) throws JSONException {
    if (Logger.shouldLogVerbose(LOG_TAG)) {
      for (int i = 0; i < envs.size(); ++i) {
        Logger.trace(LOG_TAG, "Days environment " + envs.keyAt(i) + ": " + envs.get(envs.keyAt(i)).getHash());
      }
    }

    JSONObject days = new JSONObject();
    Cursor cursor = storage.getRawEventsSince(since);
    try {
      if (!cursor.moveToFirst()) {
        return days;
      }

      // A classic walking partition.
      // Columns are "date", "env", "field", "value".
      // Note that we care about the type (integer, string) and kind
      // (last/counter, discrete) of each field.
      // Each field will be accessed once for each date/env pair, so
      // Field memoizes these facts.
      // We also care about which measurement contains each field.
      int lastDate  = -1;
      int lastEnv   = -1;
      JSONObject dateObject = null;
      JSONObject envObject = null;

      while (!cursor.isAfterLast()) {
        int cEnv = cursor.getInt(1);
        if (cEnv == -1 ||
            (cEnv != lastEnv &&
             envs.indexOfKey(cEnv) < 0)) {
          Logger.warn(LOG_TAG, "Invalid environment " + cEnv + " in cursor. Skipping.");
          cursor.moveToNext();
          continue;
        }

        int cDate  = cursor.getInt(0);
        int cField = cursor.getInt(2);

        Logger.trace(LOG_TAG, "Event row: " + cDate + ", " + cEnv + ", " + cField);
        boolean dateChanged = cDate != lastDate;
        boolean envChanged = cEnv != lastEnv;

        if (dateChanged) {
          if (dateObject != null) {
            days.put(dateFormatter.getDateStringForDay(lastDate), dateObject);
          }
          dateObject = new JSONObject();
          lastDate = cDate;
        }

        if (dateChanged || envChanged) {
          envObject = new JSONObject();
          // This is safe because we checked above that cEnv is valid.
          dateObject.put(envs.get(cEnv).getHash(), envObject);
          lastEnv = cEnv;
        }

        final Field field = fields.get(cField);
        JSONObject measurement = envObject.optJSONObject(field.measurementName);
        if (measurement == null) {
          // We will never have more than one measurement version within a
          // single environment -- to do so involves changing the build ID. And
          // even if we did, we have no way to represent it. So just build the
          // output object once.
          measurement = new JSONObject();
          measurement.put("_v", field.measurementVersion);
          envObject.put(field.measurementName, measurement);
        }

        // How we record depends on the type of the field, so we
        // break this out into a separate method for clarity.
        recordMeasurementFromCursor(field, measurement, cursor);

        cursor.moveToNext();
        continue;
      }
      days.put(dateFormatter.getDateStringForDay(lastDate), dateObject);
    } finally {
      cursor.close();
    }
    return days;
  }

  /**
   * Return the {@link JSONObject} parsed from the provided index of the given
   * cursor, or {@link JSONObject#NULL} if either SQL <code>NULL</code> or
   * string <code>"null"</code> is present at that index.
   */
  private static Object getJSONAtIndex(Cursor cursor, int index) throws JSONException {
    if (cursor.isNull(index)) {
      return JSONObject.NULL;
    }
    final String value = cursor.getString(index);
    if ("null".equals(value)) {
      return JSONObject.NULL;
    }
    return new JSONObject(value);
  }

  protected static void recordMeasurementFromCursor(final Field field,
                                             JSONObject measurement,
                                             Cursor cursor)
                                                           throws JSONException {
    if (field.isDiscreteField()) {
      // Discrete counted. Increment the named counter.
      if (field.isCountedField()) {
        if (!field.isStringField()) {
          throw new IllegalStateException("Unable to handle non-string counted types.");
        }
        HealthReportUtils.count(measurement, field.fieldName, cursor.getString(3));
        return;
      }

      // Discrete string or integer. Append it.
      if (field.isStringField()) {
        HealthReportUtils.append(measurement, field.fieldName, cursor.getString(3));
        return;
      }
      if (field.isJSONField()) {
        HealthReportUtils.append(measurement, field.fieldName, getJSONAtIndex(cursor, 3));
        return;
      }
      if (field.isIntegerField()) {
        HealthReportUtils.append(measurement, field.fieldName, cursor.getLong(3));
        return;
      }
      throw new IllegalStateException("Unknown field type: " + field.flags);
    }

    // Non-discrete -- must be LAST or COUNTER, so just accumulate the value.
    if (field.isStringField()) {
      measurement.put(field.fieldName, cursor.getString(3));
      return;
    }
    if (field.isJSONField()) {
      measurement.put(field.fieldName, getJSONAtIndex(cursor, 3));
      return;
    }
    measurement.put(field.fieldName, cursor.getLong(3));
  }

  public static JSONObject getEnvironmentsJSON(Environment currentEnvironment,
                                               SparseArray<Environment> envs) throws JSONException {
    JSONObject environments = new JSONObject();

    // Always do this, even if it hasn't recorded anything in the DB.
    environments.put("current", jsonify(currentEnvironment, null));

    String currentHash = currentEnvironment.getHash();
    for (int i = 0; i < envs.size(); i++) {
      Environment e = envs.valueAt(i);
      if (currentHash.equals(e.getHash())) {
        continue;
      }
      environments.put(e.getHash(), jsonify(e, currentEnvironment));
    }
    return environments;
  }

  public static JSONObject jsonify(Environment e, Environment current) throws JSONException {
    JSONObject age = getProfileAge(e, current);
    JSONObject sysinfo = getSysInfo(e, current);
    JSONObject gecko = getGeckoInfo(e, current);
    JSONObject appinfo = getAppInfo(e, current);
    JSONObject counts = getAddonCounts(e, current);

    JSONObject out = new JSONObject();
    if (age != null)
      out.put("org.mozilla.profile.age", age);
    if (sysinfo != null)
      out.put("org.mozilla.sysinfo.sysinfo", sysinfo);
    if (gecko != null)
      out.put("geckoAppInfo", gecko);
    if (appinfo != null)
      out.put("org.mozilla.appInfo.appinfo", appinfo);
    if (counts != null)
      out.put("org.mozilla.addons.counts", counts);

    JSONObject active = getActiveAddons(e, current);
    if (active != null)
      out.put("org.mozilla.addons.active", active);

    if (current == null) {
      out.put("hash", e.getHash());
    }
    return out;
  }

  private static JSONObject getProfileAge(Environment e, Environment current) throws JSONException {
    JSONObject age = new JSONObject();
    int changes = 0;
    if (current == null || current.profileCreation != e.profileCreation) {
      age.put("profileCreation", e.profileCreation);
      changes++;
    }
    if (current != null && changes == 0) {
      return null;
    }
    age.put("_v", 1);
    return age;
  }

  private static JSONObject getSysInfo(Environment e, Environment current) throws JSONException {
    JSONObject sysinfo = new JSONObject();
    int changes = 0;
    if (current == null || current.cpuCount != e.cpuCount) {
      sysinfo.put("cpuCount", e.cpuCount);
      changes++;
    }
    if (current == null || current.memoryMB != e.memoryMB) {
      sysinfo.put("memoryMB", e.memoryMB);
      changes++;
    }
    if (current == null || !current.architecture.equals(e.architecture)) {
      sysinfo.put("architecture", e.architecture);
      changes++;
    }
    if (current == null || !current.sysName.equals(e.sysName)) {
      sysinfo.put("name", e.sysName);
      changes++;
    }
    if (current == null || !current.sysVersion.equals(e.sysVersion)) {
      sysinfo.put("version", e.sysVersion);
      changes++;
    }
    if (current != null && changes == 0) {
      return null;
    }
    sysinfo.put("_v", 1);
    return sysinfo;
  }

  private static JSONObject getGeckoInfo(Environment e, Environment current) throws JSONException {
    JSONObject gecko = new JSONObject();
    int changes = 0;
    if (current == null || !current.vendor.equals(e.vendor)) {
      gecko.put("vendor", e.vendor);
      changes++;
    }
    if (current == null || !current.appName.equals(e.appName)) {
      gecko.put("name", e.appName);
      changes++;
    }
    if (current == null || !current.appID.equals(e.appID)) {
      gecko.put("id", e.appID);
      changes++;
    }
    if (current == null || !current.appVersion.equals(e.appVersion)) {
      gecko.put("version", e.appVersion);
      changes++;
    }
    if (current == null || !current.appBuildID.equals(e.appBuildID)) {
      gecko.put("appBuildID", e.appBuildID);
      changes++;
    }
    if (current == null || !current.platformVersion.equals(e.platformVersion)) {
      gecko.put("platformVersion", e.platformVersion);
      changes++;
    }
    if (current == null || !current.platformBuildID.equals(e.platformBuildID)) {
      gecko.put("platformBuildID", e.platformBuildID);
      changes++;
    }
    if (current == null || !current.os.equals(e.os)) {
      gecko.put("os", e.os);
      changes++;
    }
    if (current == null || !current.xpcomabi.equals(e.xpcomabi)) {
      gecko.put("xpcomabi", e.xpcomabi);
      changes++;
    }
    if (current == null || !current.updateChannel.equals(e.updateChannel)) {
      gecko.put("updateChannel", e.updateChannel);
      changes++;
    }
    if (current != null && changes == 0) {
      return null;
    }
    gecko.put("_v", 1);
    return gecko;
  }

  // Null-safe string comparison.
  private static boolean stringsDiffer(final String a, final String b) {
    if (a == null) {
      return b != null;
    }
    return !a.equals(b);
  }

  private static JSONObject getAppInfo(Environment e, Environment current) throws JSONException {
    JSONObject appinfo = new JSONObject();

    Logger.debug(LOG_TAG, "Generating appinfo for v" + e.version + " env " + e.hash);

    // Is the environment in question newer than the diff target, or is
    // there no diff target?
    final boolean outdated = current == null ||
                             e.version > current.version;

    // Is the environment in question a different version (lower or higher),
    // or is there no diff target?
    final boolean differ = outdated || current.version > e.version;

    // Always produce an output object if there's a version mismatch or this
    // isn't a diff. Otherwise, track as we go if there's any difference.
    boolean changed = differ;

    switch (e.version) {
    // There's a straightforward correspondence between environment versions
    // and appinfo versions.
    case 2:
      appinfo.put("_v", 3);
      break;
    case 1:
      appinfo.put("_v", 2);
      break;
    default:
      Logger.warn(LOG_TAG, "Unknown environment version: " + e.version);
      return appinfo;
    }

    switch (e.version) {
    case 2:
      if (populateAppInfoV2(appinfo, e, current, outdated)) {
        changed = true;
      }
      // Fall through.

    case 1:
      // There is no older version than v1, so don't check outdated.
      if (populateAppInfoV1(e, current, appinfo)) {
        changed = true;
      }
    }

    if (!changed) {
      return null;
    }

    return appinfo;
  }

  private static boolean populateAppInfoV1(Environment e,
                                           Environment current,
                                           JSONObject appinfo)
    throws JSONException {
    boolean changes = false;
    if (current == null || current.isBlocklistEnabled != e.isBlocklistEnabled) {
      appinfo.put("isBlocklistEnabled", e.isBlocklistEnabled);
      changes = true;
    }

    if (current == null || current.isTelemetryEnabled != e.isTelemetryEnabled) {
      appinfo.put("isTelemetryEnabled", e.isTelemetryEnabled);
      changes = true;
    }

    return changes;
  }

  private static boolean populateAppInfoV2(JSONObject appinfo,
                                           Environment e,
                                           Environment current,
                                           final boolean outdated)
    throws JSONException {
    boolean changes = false;
    if (outdated ||
        stringsDiffer(current.osLocale, e.osLocale)) {
      appinfo.put("osLocale", e.osLocale);
      changes = true;
    }

    if (outdated ||
        stringsDiffer(current.appLocale, e.appLocale)) {
      appinfo.put("appLocale", e.appLocale);
      changes = true;
    }

    if (outdated ||
        stringsDiffer(current.distribution, e.distribution)) {
      appinfo.put("distribution", e.distribution);
      changes = true;
    }

    if (outdated ||
        current.acceptLangSet != e.acceptLangSet) {
      appinfo.put("acceptLangIsUserSet", e.acceptLangSet);
      changes = true;
    }
    return changes;
  }

  private static JSONObject getAddonCounts(Environment e, Environment current) throws JSONException {
    JSONObject counts = new JSONObject();
    int changes = 0;
    if (current == null || current.extensionCount != e.extensionCount) {
      counts.put("extension", e.extensionCount);
      changes++;
    }
    if (current == null || current.pluginCount != e.pluginCount) {
      counts.put("plugin", e.pluginCount);
      changes++;
    }
    if (current == null || current.themeCount != e.themeCount) {
      counts.put("theme", e.themeCount);
      changes++;
    }
    if (current != null && changes == 0) {
      return null;
    }
    counts.put("_v", 1);
    return counts;
  }

  /**
   * Compute the *tree* difference set between the two objects. If the two
   * objects are identical, returns <code>null</code>. If <code>from</code> is
   * <code>null</code>, returns <code>to</code>. If <code>to</code> is
   * <code>null</code>, behaves as if <code>to</code> were an empty object.
   *
   * (Note that this method does not check for {@link JSONObject#NULL}, because
   * by definition it can't be provided as input to this method.)
   *
   * This behavior is intended to simplify life for callers: a missing object
   * can be viewed as (and behaves as) an empty map, to a useful extent, rather
   * than throwing an exception.
   *
   * @param from
   *          a JSONObject.
   * @param to
   *          a JSONObject.
   * @param includeNull
   *          if true, keys present in <code>from</code> but not in
   *          <code>to</code> are included as {@link JSONObject#NULL} in the
   *          output.
   *
   * @return a JSONObject, or null if the two objects are identical.
   * @throws JSONException
   *           should not occur, but...
   */
  public static JSONObject diff(JSONObject from,
                                JSONObject to,
                                boolean includeNull) throws JSONException {
    if (from == null) {
      return to;
    }

    if (to == null) {
      return diff(from, new JSONObject(), includeNull);
    }

    JSONObject out = new JSONObject();

    HashSet<String> toKeys = includeNull ? new HashSet<String>(to.length())
                                         : null;

    @SuppressWarnings("unchecked")
    Iterator<String> it = to.keys();
    while (it.hasNext()) {
      String key = it.next();

      // Track these as we go if we'll need them later.
      if (includeNull) {
        toKeys.add(key);
      }

      Object value = to.get(key);
      if (!from.has(key)) {
        // It must be new.
        out.put(key, value);
        continue;
      }

      // Not new? Then see if it changed.
      Object old = from.get(key);

      // Two JSONObjects should be diffed.
      if (old instanceof JSONObject && value instanceof JSONObject) {
        JSONObject innerDiff = diff(((JSONObject) old), ((JSONObject) value),
                                    includeNull);
        // No change? No output.
        if (innerDiff == null) {
          continue;
        }

        // Otherwise include the diff.
        out.put(key, innerDiff);
        continue;
      }

      // A regular value, or a type change. Only skip if they're the same.
      if (value.equals(old)) {
        continue;
      }
      out.put(key, value);
    }

    // Now -- if requested -- include any removed keys.
    if (includeNull) {
      Set<String> fromKeys = HealthReportUtils.keySet(from);
      fromKeys.removeAll(toKeys);
      for (String notPresent : fromKeys) {
        out.put(notPresent, JSONObject.NULL);
      }
    }

    if (out.length() == 0) {
      return null;
    }
    return out;
  }

  private static JSONObject getActiveAddons(Environment e, Environment current) throws JSONException {
    // Just return the current add-on set, with a version annotation.
    // To do so requires copying.
    if (current == null) {
      JSONObject out = e.getNonIgnoredAddons();
      if (out == null) {
        Logger.warn(LOG_TAG, "Null add-ons to return in FHR document. Returning {}.");
        out = new JSONObject();        // So that we always return something.
      }
      out.put("_v", 1);
      return out;
    }

    // Otherwise, return the diff.
    JSONObject diff = diff(current.getNonIgnoredAddons(), e.getNonIgnoredAddons(), true);
    if (diff == null) {
      return null;
    }
    if (diff == e.addons) {
      // Again, needs to copy.
      return getActiveAddons(e, null);
    }

    diff.put("_v", 1);
    return diff;
  }
}
