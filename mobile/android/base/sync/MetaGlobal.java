/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.io.IOException;
import java.net.URISyntaxException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.MetaGlobalException.MetaGlobalMalformedSyncIDException;
import org.mozilla.gecko.sync.MetaGlobalException.MetaGlobalMalformedVersionException;
import org.mozilla.gecko.sync.delegates.MetaGlobalDelegate;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

public class MetaGlobal implements SyncStorageRequestDelegate {
  private static final String LOG_TAG = "MetaGlobal";
  protected String metaURL;

  // Fields.
  protected ExtendedJSONObject  engines;
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
      r.delegate = this;
      r.deferGet();
    } catch (URISyntaxException e) {
      this.callback.handleError(e);
    }
  }

  public void upload(MetaGlobalDelegate callback) {
    try {
      this.isUploading = true;
      SyncStorageRecordRequest r = new SyncStorageRecordRequest(this.metaURL);

      r.delegate = this;
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

  public void setFromRecord(CryptoRecord record) throws IllegalStateException, IOException, ParseException, NonObjectJSONException {
    if (record == null) {
      throw new IllegalArgumentException("Cannot set meta/global from null record");
    }
    Logger.debug(LOG_TAG, "meta/global is " + record.payload.toJSONString());
    this.storageVersion = (Long) record.payload.get("storageVersion");
    this.syncID = (String) record.payload.get("syncID");
    setEngines(record.payload.getObject("engines"));
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
          version.intValue() == 0) {
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

  @Override
  public AuthHeaderProvider getAuthHeaderProvider() {
    return authHeaderProvider;
  }

  public String ifUnmodifiedSince() {
    return null;
  }

  public void handleRequestSuccess(SyncStorageResponse response) {
    if (this.isUploading) {
      this.handleUploadSuccess(response);
    } else {
      this.handleDownloadSuccess(response);
    }
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

  public void handleRequestFailure(SyncStorageResponse response) {
    if (response.getStatusCode() == 404) {
      this.callback.handleMissing(this, response);
      return;
    }
    this.callback.handleFailure(response);
  }

  public void handleRequestError(Exception e) {
    this.callback.handleError(e);
  }
}
