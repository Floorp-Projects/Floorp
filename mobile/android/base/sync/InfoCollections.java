/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.delegates.InfoCollectionsDelegate;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

public class InfoCollections implements SyncStorageRequestDelegate {
  private static final String LOG_TAG = "InfoCollections";
  protected String infoURL;
  protected String credentials;

  /**
   * Fields fetched from the server, or <code>null</code> if not yet fetched.
   * <p>
   * Rather than storing decimal/double timestamps, as provided by the server,
   * we convert immediately to milliseconds since epoch.
   */
  private Map<String, Long> timestamps = null;

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
    return (serverLastModified.longValue() > lastModified);
  }

  // Temporary location to store our callback.
  private InfoCollectionsDelegate callback;

  public InfoCollections(String metaURL, String credentials) {
    this.infoURL     = metaURL;
    this.credentials = credentials;
  }

  public void fetch(InfoCollectionsDelegate callback) {
    this.callback = callback;
    this.doFetch();
  }

  private void doFetch() {
    try {
      final SyncStorageRecordRequest r = new SyncStorageRecordRequest(this.infoURL);
      r.delegate = this;
      // TODO: it might be nice to make Resource include its
      // own thread pool, and automatically run asynchronously.
      ThreadPool.run(new Runnable() {
        @Override
        public void run() {
          try {
            r.get();
          } catch (Exception e) {
            callback.handleError(e);
          }
        }});
    } catch (Exception e) {
      callback.handleError(e);
    }
  }

  @SuppressWarnings("unchecked")
  public void setFromRecord(ExtendedJSONObject record) throws IllegalStateException, IOException, ParseException, NonObjectJSONException {
    Logger.debug(LOG_TAG, "info/collections is " + record.toJSONString());
    HashMap<String, Long> map = new HashMap<String, Long>();

    Set<Entry<String, Object>> entrySet = record.object.entrySet();
    for (Entry<String, Object> entry : entrySet) {
      // These objects are most likely going to be Doubles. Regardless, we
      // want to get them in a more sane time format.
      String key = entry.getKey();
      Object value = entry.getValue();
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
    this.timestamps = map;
  }

  // SyncStorageRequestDelegate methods for fetching.
  public String credentials() {
    return this.credentials;
  }

  public String ifUnmodifiedSince() {
    return null;
  }

  public void handleRequestSuccess(SyncStorageResponse response) {
    if (response.wasSuccessful()) {
      try {
        this.setFromRecord(response.jsonObjectBody());
        this.callback.handleSuccess(this);
        this.callback = null;
      } catch (Exception e) {
        this.callback.handleError(e);
        this.callback = null;
      }
      return;
    }
    this.callback.handleFailure(response);
    this.callback = null;
  }

  public void handleRequestFailure(SyncStorageResponse response) {
    this.callback.handleFailure(response);
    this.callback = null;
  }

  public void handleRequestError(Exception e) {
    this.callback.handleError(e);
    this.callback = null;
  }
}
