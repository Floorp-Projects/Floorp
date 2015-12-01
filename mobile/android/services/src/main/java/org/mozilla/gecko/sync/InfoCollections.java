/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;

import org.mozilla.gecko.background.common.log.Logger;

/**
 * Fetches the timestamp information in <code>info/collections</code> on the
 * Sync server. Provides access to those timestamps, along with logic to check
 * for whether a collection requires an update.
 */
public class InfoCollections {
  private static final String LOG_TAG = "InfoCollections";

  /**
   * Fields fetched from the server, or <code>null</code> if not yet fetched.
   * <p>
   * Rather than storing decimal/double timestamps, as provided by the server,
   * we convert immediately to milliseconds since epoch.
   */
  final Map<String, Long> timestamps;

  public InfoCollections() {
    this(new ExtendedJSONObject());
  }

  public InfoCollections(final ExtendedJSONObject record) {
    Logger.debug(LOG_TAG, "info/collections is " + record.toJSONString());
    HashMap<String, Long> map = new HashMap<String, Long>();

    for (Entry<String, Object> entry : record.entrySet()) {
      final String key = entry.getKey();
      final Object value = entry.getValue();

      // These objects are most likely going to be Doubles. Regardless, we
      // want to get them in a more sane time format.
      if (value instanceof Double) {
        map.put(key, Utils.decimalSecondsToMilliseconds((Double) value));
        continue;
      }
      if (value instanceof Long) {
        map.put(key, Utils.decimalSecondsToMilliseconds((Long) value));
        continue;
      }
      if (value instanceof Integer) {
        map.put(key, Utils.decimalSecondsToMilliseconds((Integer) value));
        continue;
      }
      Logger.warn(LOG_TAG, "Skipping info/collections entry for " + key);
    }

    this.timestamps = Collections.unmodifiableMap(map);
  }

  /**
   * Return the timestamp for the given collection, or null if the timestamps
   * have not been fetched or the given collection does not have a timestamp.
   *
   * @param collection
   *          The collection to inspect.
   * @return the timestamp in milliseconds since epoch.
   */
  public Long getTimestamp(String collection) {
    if (timestamps == null) {
      return null;
    }
    return timestamps.get(collection);
  }

  /**
   * Test if a given collection needs to be updated.
   *
   * @param collection
   *          The collection to test.
   * @param lastModified
   *          Timestamp when local record was last modified.
   */
  public boolean updateNeeded(String collection, long lastModified) {
    Logger.trace(LOG_TAG, "Testing " + collection + " for updateNeeded. Local last modified is " + lastModified + ".");

    // No local record of modification time? Need an update.
    if (lastModified <= 0) {
      return true;
    }

    // No meta/global on the server? We need an update. The server fetch will fail and
    // then we will upload a fresh meta/global.
    Long serverLastModified = getTimestamp(collection);
    if (serverLastModified == null) {
      return true;
    }

    // Otherwise, we need an update if our modification time is stale.
    return serverLastModified > lastModified;
  }
}
