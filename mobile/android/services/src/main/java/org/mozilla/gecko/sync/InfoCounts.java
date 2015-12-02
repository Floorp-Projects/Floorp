/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

import org.mozilla.gecko.background.common.log.Logger;

public class InfoCounts {
  static final String LOG_TAG = "InfoCounts";

  /**
   * Counts fetched from the server, or <code>null</code> if not yet fetched.
   */
  private Map<String, Integer> counts = null;

  @SuppressWarnings("unchecked")
  public InfoCounts(final ExtendedJSONObject record) {
    Logger.debug(LOG_TAG, "info/collection_counts is " + record.toJSONString());
    HashMap<String, Integer> map = new HashMap<String, Integer>();

    Set<Entry<String, Object>> entrySet = record.object.entrySet();

    String key;
    Object value;

    for (Entry<String, Object> entry : entrySet) {
      key = entry.getKey();
      value = entry.getValue();

      if (value instanceof Integer) {
        map.put(key, (Integer) value);
        continue;
      }

      if (value instanceof Long) {
        map.put(key, ((Long) value).intValue());
        continue;
      }

      Logger.warn(LOG_TAG, "Skipping info/collection_counts entry for " + key);
    }

    this.counts = Collections.unmodifiableMap(map);
  }

  /**
   * Return the server count for the given collection, or null if the counts
   * have not been fetched or the given collection does not have a count.
   *
   * @param collection
   *          The collection to inspect.
   * @return the number of elements in the named collection.
   */
  public Integer getCount(String collection) {
    if (counts == null) {
      return null;
    }
    return counts.get(collection);
  }
}
