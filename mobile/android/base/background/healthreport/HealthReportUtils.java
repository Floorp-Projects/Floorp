/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import java.text.SimpleDateFormat;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Locale;
import java.util.Set;
import java.util.SortedSet;
import java.util.TimeZone;
import java.util.TreeSet;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONArray;
import org.mozilla.apache.commons.codec.digest.DigestUtils;

import android.content.ContentUris;
import android.net.Uri;

public class HealthReportUtils {
  public static int getDay(final long time) {
    return (int) Math.floor(time / HealthReportConstants.MILLISECONDS_PER_DAY);
  }

  public static String getEnvironmentHash(final String input) {
    return DigestUtils.shaHex(input);
  }

  public static String getDateStringForDay(long day) {
    return getDateString(HealthReportConstants.MILLISECONDS_PER_DAY * day);
  }

  public static String getDateString(long time) {
    final SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd", Locale.US);
    format.setTimeZone(TimeZone.getTimeZone("UTC"));
    return format.format(time);
  }

  /**
   * Take an environment URI (one that identifies an environment) and produce an
   * event URI.
   *
   * That this is needed is tragic.
   *
   * @param environmentURI
   *          the {@link Uri} returned by an environment operation.
   * @return a {@link Uri} to which insertions can be dispatched.
   */
  public static Uri getEventURI(Uri environmentURI) {
    return environmentURI.buildUpon().path("/events/" + ContentUris.parseId(environmentURI) + "/").build();
  }

  /**
   * Copy the keys from the provided {@link JSONObject} into the provided {@link Set}.
   */
  private static <T extends Set<String>> T intoKeySet(T keys, JSONObject o) {
    if (o == null || o == JSONObject.NULL) {
      return keys;
    }

    @SuppressWarnings("unchecked")
    Iterator<String> it = o.keys();
    while (it.hasNext()) {
      keys.add(it.next());
    }
    return keys;
  }

  /**
   * Produce a {@link SortedSet} containing the string keys of the provided
   * object.
   *
   * @param o a {@link JSONObject} with string keys.
   * @return a sorted set.
   */
  public static SortedSet<String> sortedKeySet(JSONObject o) {
    return intoKeySet(new TreeSet<String>(), o);
  }

  /**
   * Produce a {@link Set} containing the string keys of the provided object.
   * @param o a {@link JSONObject} with string keys.
   * @return an unsorted set.
   */
  public static Set<String> keySet(JSONObject o) {
    return intoKeySet(new HashSet<String>(), o);
  }

  /**
   * Just like {@link JSONObject#accumulate(String, Object)}, but doesn't do the wrong thing for single values.
   * @throws JSONException 
   */
  public static void append(JSONObject o, String key, Object value) throws JSONException {
    if (!o.has(key)) {
      JSONArray arr = new JSONArray();
      arr.put(value);
      o.put(key, arr);
      return;
    }
    Object dest = o.get(key);
    if (dest instanceof JSONArray) {
      ((JSONArray) dest).put(value);
      return;
    }
    JSONArray arr = new JSONArray();
    arr.put(dest);
    arr.put(value);
    o.put(key, arr);
  }

  /**
   * Accumulate counts for how often each provided value occurs.
   *
   * <code>
   *   HealthReportUtils.count(o, "foo", "bar");
   * </code>
   *
   * will change
   *
   * <pre>
   *   {"foo", {"bar": 1}}
   * </pre>
   *
   * into
   *
   * <pre>
   *   {"foo", {"bar": 2}}
   * </pre>
   *
   */
  public static void count(JSONObject o, String key,
                           String value) throws JSONException {
    if (!o.has(key)) {
      JSONObject counts = new JSONObject();
      counts.put(value, 1);
      o.put(key, counts);
      return;
    }
    JSONObject dest = o.getJSONObject(key);
    dest.put(value, dest.optInt(value, 0) + 1);
  }
}
