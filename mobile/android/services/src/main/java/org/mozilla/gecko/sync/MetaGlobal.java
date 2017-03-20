/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.io.IOException;
import java.net.URISyntaxException;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import org.json.simple.JSONArray;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.MetaGlobalException.MetaGlobalMalformedSyncIDException;
import org.mozilla.gecko.sync.MetaGlobalException.MetaGlobalMalformedVersionException;
import org.mozilla.gecko.sync.delegates.MetaGlobalDelegate;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

public class MetaGlobal {
  private static final String LOG_TAG = "MetaGlobal";
  protected String metaURL;

  // Fields.
  protected ExtendedJSONObject  engines;
  protected JSONArray           declined;
  protected Long                storageVersion;
  protected String              syncID;

  // Lookup tables.
  protected Map<String, String> syncIDs;
  protected Map<String, Integer> versions;
  protected Map<String, MetaGlobalException> exceptions;

  // Temporary location to store our callback.
  private MetaGlobalDelegate callback;

  // A little hack so we can use the same delegate implementation for upload and download.
  private boolean isUploading;
  protected final AuthHeaderProvider authHeaderProvider;

  public MetaGlobal(String metaURL, AuthHeaderProvider authHeaderProvider) {
    this.metaURL = metaURL;
    this.authHeaderProvider = authHeaderProvider;
  }

  public void fetch(MetaGlobalDelegate delegate) {
    this.callback = delegate;
    try {
      this.isUploading = false;
      SyncStorageRecordRequest r = new SyncStorageRecordRequest(this.metaURL);
      r.delegate = new MetaUploadDelegate(this, null);
      r.get();
    } catch (URISyntaxException e) {
      this.callback.handleError(e);
    }
  }

  public void upload(long lastModifiedTimestamp, MetaGlobalDelegate callback) {
    try {
      this.isUploading = true;
      SyncStorageRecordRequest r = new SyncStorageRecordRequest(this.metaURL);
      r.delegate = new MetaUploadDelegate(this, lastModifiedTimestamp);
      this.callback = callback;
      r.put(this.asCryptoRecord());
    } catch (Exception e) {
      callback.handleError(e);
    }
  }

  protected ExtendedJSONObject asRecordContents() {
    ExtendedJSONObject json = new ExtendedJSONObject();
    json.put("storageVersion", storageVersion);
    json.put("engines", engines);
    json.put("syncID", syncID);
    json.put("declined", declined);
    return json;
  }

  /**
   * Return a copy ready for upload.
   * @return an unencrypted <code>CryptoRecord</code>.
   */
  public CryptoRecord asCryptoRecord() {
    ExtendedJSONObject payload = this.asRecordContents();
    CryptoRecord record = new CryptoRecord(payload);
    record.collection = "meta";
    record.guid       = "global";
    record.deleted    = false;
    return record;
  }

  public void setFromRecord(CryptoRecord record) throws IllegalStateException, IOException, NonObjectJSONException, NonArrayJSONException {
    if (record == null) {
      throw new IllegalArgumentException("Cannot set meta/global from null record");
    }
    Logger.debug(LOG_TAG, "meta/global is " + record.payload.toJSONString());
    this.storageVersion = (Long) record.payload.get("storageVersion");
    this.syncID = (String) record.payload.get("syncID");

    setEngines(record.payload.getObject("engines"));

    // Accepts null -- declined can be missing.
    setDeclinedEngineNames(record.payload.getArray("declined"));
  }

  public Long getStorageVersion() {
    return this.storageVersion;
  }

  public void setStorageVersion(Long version) {
    this.storageVersion = version;
  }

  public ExtendedJSONObject getEngines() {
    return engines;
  }

  @SuppressWarnings("unchecked")
  public void declineEngine(String engine) {
    if (this.declined == null) {
      JSONArray replacement = new JSONArray();
      replacement.add(engine);
      setDeclinedEngineNames(replacement);
      return;
    }

    this.declined.add(engine);
  }

  @SuppressWarnings("unchecked")
  public void declineEngineNames(Collection<String> additional) {
    if (this.declined == null) {
      JSONArray replacement = new JSONArray();
      replacement.addAll(additional);
      setDeclinedEngineNames(replacement);
      return;
    }

    for (String engine : additional) {
      if (!this.declined.contains(engine)) {
        this.declined.add(engine);
      }
    }
  }

  public void setDeclinedEngineNames(JSONArray declined) {
    if (declined == null) {
      this.declined = new JSONArray();
      return;
    }
    this.declined = declined;
  }

  /**
   * Return the set of engines that we support (given as an argument)
   * but the user hasn't explicitly declined on another device.
   *
   * Can return the input if the user hasn't declined any engines.
   */
  public Set<String> getNonDeclinedEngineNames(Set<String> supported) {
    if (this.declined == null ||
        this.declined.isEmpty()) {
      return supported;
    }

    final Set<String> result = new HashSet<String>(supported);
    result.removeAll(this.declined);
    return result;
  }

  public void setEngines(ExtendedJSONObject engines) {
    if (engines == null) {
      engines = new ExtendedJSONObject();
    }
    this.engines = engines;
    final int count = engines.size();
    versions   = new HashMap<String, Integer>(count);
    syncIDs    = new HashMap<String, String>(count);
    exceptions = new HashMap<String, MetaGlobalException>(count);
    for (String engineName : engines.keySet()) {
      try {
        ExtendedJSONObject engineEntry = engines.getObject(engineName);
        recordEngineState(engineName, engineEntry);
      } catch (NonObjectJSONException e) {
        Logger.error(LOG_TAG, "Engine field for " + engineName + " in meta/global is not an object.");
        recordEngineState(engineName, new ExtendedJSONObject()); // Doesn't have a version or syncID, for example, so will be server wiped.
      }
    }
  }

  /**
   * Take a JSON object corresponding to the 'engines' field for the provided engine name,
   * updating {@link #syncIDs} and {@link #versions} accordingly.
   *
   * If the record is malformed, an entry is added to {@link #exceptions}, to be rethrown
   * during validation.
   */
  protected void recordEngineState(String engineName, ExtendedJSONObject engineEntry) {
    if (engineEntry == null) {
      throw new IllegalArgumentException("engineEntry cannot be null.");
    }

    // Record syncID first, so that engines with bad versions are recorded.
    try {
      String syncID = engineEntry.getString("syncID");
      if (syncID == null) {
        Logger.warn(LOG_TAG, "No syncID for " + engineName + ". Recording exception.");
        exceptions.put(engineName, new MetaGlobalMalformedSyncIDException());
      }
      syncIDs.put(engineName, syncID);
    } catch (ClassCastException e) {
      // Malformed syncID on the server. Wipe the server.
      Logger.warn(LOG_TAG, "Malformed syncID " + engineEntry.get("syncID") +
                           " for " + engineName + ". Recording exception.");
      exceptions.put(engineName, new MetaGlobalMalformedSyncIDException());
    }

    try {
      Integer version = engineEntry.getIntegerSafely("version");
      Logger.trace(LOG_TAG, "Engine " + engineName + " has server version " + version);
      if (version == null ||
          version == 0) {
        // Invalid version. Wipe the server.
        Logger.warn(LOG_TAG, "Malformed version " + version +
                             " for " + engineName + ". Recording exception.");
        exceptions.put(engineName, new MetaGlobalMalformedVersionException());
        return;
      }
      versions.put(engineName, version);
    } catch (NumberFormatException e) {
      // Invalid version. Wipe the server.
      Logger.warn(LOG_TAG, "Malformed version " + engineEntry.get("version") +
                           " for " + engineName + ". Recording exception.");
      exceptions.put(engineName, new MetaGlobalMalformedVersionException());
      return;
    }
  }

  /**
   * Get enabled engine names.
   *
   * @return a collection of engine names or <code>null</code> if meta/global
   *         was malformed.
   */
  public Set<String> getEnabledEngineNames() {
    if (engines == null) {
      return null;
    }
    return new HashSet<String>(engines.keySet());
  }

  @SuppressWarnings("unchecked")
  public Set<String> getDeclinedEngineNames() {
    if (declined == null) {
      return null;
    }
    return new HashSet<String>(declined);
  }

  /**
   * Returns if the server settings and local settings match.
   * Throws a specific MetaGlobalException if that's not the case.
   */
  public void verifyEngineSettings(String engineName, EngineSettings engineSettings)
  throws MetaGlobalException {

    // We use syncIDs as our canary.
    if (syncIDs == null) {
      throw new IllegalStateException("No meta/global record yet processed.");
    }

    if (engineSettings == null) {
      throw new IllegalArgumentException("engineSettings cannot be null.");
    }

    // First, see if we had a parsing problem.
    final MetaGlobalException exception = exceptions.get(engineName);
    if (exception != null) {
      throw exception;
    }

    final String syncID = syncIDs.get(engineName);
    if (syncID == null) {
      // We have checked engineName against enabled engine names before this, so
      // we should either have a syncID or an exception for this engine already.
      throw new IllegalArgumentException("Unknown engine " + engineName);
    }

    // Since we don't have an exception, and we do have a syncID, we should have a version.
    final Integer version = versions.get(engineName);
    if (version > engineSettings.version) {
      // We're out of date.
      throw new MetaGlobalException.MetaGlobalStaleClientVersionException(version);
    }

    if (!syncID.equals(engineSettings.syncID)) {
      // Our syncID is wrong. Reset client and take the server syncID.
      throw new MetaGlobalException.MetaGlobalStaleClientSyncIDException(syncID);
    }
  }

  public String getSyncID() {
    return syncID;
  }

  public void setSyncID(String syncID) {
    this.syncID = syncID;
  }

  // SyncStorageRequestDelegate methods for fetching.
  public String credentials() {
    return null;
  }

  private void handleUploadSuccess(SyncStorageResponse response) {
    this.callback.handleSuccess(this, response);
  }

  private void handleDownloadSuccess(SyncStorageResponse response) {
    if (response.wasSuccessful()) {
      try {
        CryptoRecord record = CryptoRecord.fromJSONRecord(response.jsonObjectBody());
        this.setFromRecord(record);
        this.callback.handleSuccess(this, response);
      } catch (Exception e) {
        this.callback.handleError(e);
      }
      return;
    }
    this.callback.handleFailure(response);
  }

  private static class MetaUploadDelegate implements SyncStorageRequestDelegate {
    private final MetaGlobal metaGlobal;
    private final Long ifUnmodifiedSinceTimestamp;

    /* package-local */ MetaUploadDelegate(final MetaGlobal metaGlobal, final Long ifUnmodifiedSinceTimestamp) {
      this.metaGlobal = metaGlobal;
      this.ifUnmodifiedSinceTimestamp = ifUnmodifiedSinceTimestamp;
    }

    @Override
    public AuthHeaderProvider getAuthHeaderProvider() {
      return metaGlobal.authHeaderProvider;
    }

    @Override
    public String ifUnmodifiedSince() {
      if (ifUnmodifiedSinceTimestamp == null) {
        return null;
      }
      return Utils.millisecondsToDecimalSecondsString(ifUnmodifiedSinceTimestamp);
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse response) {
      if (metaGlobal.isUploading) {
        metaGlobal.handleUploadSuccess(response);
      } else {
        metaGlobal.handleDownloadSuccess(response);
      }
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse response) {
      if (response.getStatusCode() == 404) {
        metaGlobal.callback.handleMissing(metaGlobal, response);
        return;
      }
      metaGlobal.callback.handleFailure(response);
    }

    @Override
    public void handleRequestError(Exception e) {
      metaGlobal.callback.handleError(e);
    }
  }
}
