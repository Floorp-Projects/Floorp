/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.io.IOException;
import java.net.URISyntaxException;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.MetaGlobalException.MetaGlobalMalformedSyncIDException;
import org.mozilla.gecko.sync.MetaGlobalException.MetaGlobalMalformedVersionException;
import org.mozilla.gecko.sync.MetaGlobalException.MetaGlobalStaleClientSyncIDException;
import org.mozilla.gecko.sync.MetaGlobalException.MetaGlobalStaleClientVersionException;
import org.mozilla.gecko.sync.delegates.MetaGlobalDelegate;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import android.util.Log;

public class MetaGlobal implements SyncStorageRequestDelegate {
  private static final String LOG_TAG = "MetaGlobal";
  protected String metaURL;
  protected String credentials;

  // Fields.
  protected ExtendedJSONObject  engines;
  protected Long                storageVersion;
  protected String              syncID;

  // Temporary location to store our callback.
  private MetaGlobalDelegate callback;

  // A little hack so we can use the same delegate implementation for upload and download.
  private boolean isUploading;

  public MetaGlobal(String metaURL, String credentials) {
    this.metaURL     = metaURL;
    this.credentials = credentials;
  }

  public void fetch(MetaGlobalDelegate callback) {
    this.callback = callback;
    this.doFetch();
  }

  private void doFetch() {
    try {
      this.isUploading = false;
      SyncStorageRecordRequest r = new SyncStorageRecordRequest(this.metaURL);
      r.delegate = this;
      r.deferGet();
    } catch (URISyntaxException e) {
      callback.handleError(e);
    }
  }

  public void upload(MetaGlobalDelegate callback) {
    try {
      this.isUploading = true;
      SyncStorageRecordRequest r = new SyncStorageRecordRequest(this.metaURL);

      // TODO: PUT! Body!
      r.delegate = this;
      r.deferPut(null);
    } catch (URISyntaxException e) {
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

  public CryptoRecord asCryptoRecord() {
    ExtendedJSONObject payload = this.asRecordContents();
    CryptoRecord record = new CryptoRecord(payload);
    record.collection = "meta";
    record.guid       = "global";
    record.deleted    = false;
    return record;
  }

  public void setFromRecord(CryptoRecord record) throws IllegalStateException, IOException, ParseException, NonObjectJSONException {
    Log.i(LOG_TAG, "meta/global is " + record.payload.toJSONString());
    this.storageVersion = (Long) record.payload.get("storageVersion");
    this.engines = record.payload.getObject("engines");
    this.syncID = (String) record.payload.get("syncID");
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
    this.engines = engines;
  }

  /**
   * Returns if the server settings and local settings match.
   * Throws a specific exception if that's not the case.
   */
  public static void verifyEngineSettings(ExtendedJSONObject engineEntry,
                                          EngineSettings engineSettings)
  throws MetaGlobalMalformedVersionException, MetaGlobalMalformedSyncIDException, MetaGlobalStaleClientVersionException, MetaGlobalStaleClientSyncIDException {

    if (engineEntry == null) {
      throw new IllegalArgumentException("engineEntry cannot be null.");
    }
    if (engineSettings == null) {
      throw new IllegalArgumentException("engineSettings cannot be null.");
    }
    try {
      Integer version = engineEntry.getIntegerSafely("version");
      if (version == null ||
          version.intValue() == 0) {
        // Invalid version. Wipe the server.
        throw new MetaGlobalException.MetaGlobalMalformedVersionException();
      }
      if (version > engineSettings.version) {
        // We're out of date.
        throw new MetaGlobalException.MetaGlobalStaleClientVersionException(version);
      }
      try {
        String syncID = engineEntry.getString("syncID");
        if (syncID == null) {
          // No syncID. This should never happen. Wipe the server.
          throw new MetaGlobalException.MetaGlobalMalformedSyncIDException();
        }
        if (!syncID.equals(engineSettings.syncID)) {
          // Our syncID is wrong. Reset client and take the server syncID.
          throw new MetaGlobalException.MetaGlobalStaleClientSyncIDException(syncID);
        }
        // Great!
        return;

      } catch (ClassCastException e) {
        // Malformed syncID on the server. Wipe the server.
        throw new MetaGlobalException.MetaGlobalMalformedSyncIDException();
      }
    } catch (NumberFormatException e) {
      // Invalid version. Wipe the server.
      throw new MetaGlobalException.MetaGlobalMalformedVersionException();
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
    return this.credentials;
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
    this.callback = null;
  }

  private void handleDownloadSuccess(SyncStorageResponse response) {
    if (response.wasSuccessful()) {
      try {
        CryptoRecord record = CryptoRecord.fromJSONRecord(response.jsonObjectBody());
        this.setFromRecord(record);
        this.callback.handleSuccess(this, response);
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
    if (response.getStatusCode() == 404) {
      this.callback.handleMissing(this, response);
      this.callback = null;
      return;
    }
    this.callback.handleFailure(response);
    this.callback = null;
  }

  public void handleRequestError(Exception e) {
    this.callback.handleError(e);
    this.callback = null;
  }
}
