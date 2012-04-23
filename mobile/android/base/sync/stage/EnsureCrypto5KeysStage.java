/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URISyntaxException;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.CollectionKeys;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.PersistedCrypto5Keys;
import org.mozilla.gecko.sync.delegates.KeyUploadDelegate;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

public class EnsureCrypto5KeysStage
extends AbstractNonRepositorySyncStage
implements SyncStorageRequestDelegate, KeyUploadDelegate {

  public EnsureCrypto5KeysStage(GlobalSession session) {
    super(session);
  }

  private static final String LOG_TAG = "EnsureC5KeysStage";
  private static final String CRYPTO_COLLECTION = "crypto";
  protected boolean retrying = false;

  @Override
  public void execute() throws NoSuchStageException {
    InfoCollections infoCollections = session.config.infoCollections;
    if (infoCollections == null) {
      session.abort(null, "No info/collections set in EnsureCrypto5KeysStage.");
      return;
    }

    PersistedCrypto5Keys pck = session.config.persistedCryptoKeys();
    long lastModified = pck.lastModified();
    if (!infoCollections.updateNeeded(CRYPTO_COLLECTION, lastModified)) {
      // Try to use our local collection keys for this session.
      Logger.info(LOG_TAG, "Trying to use persisted collection keys for this session.");
      CollectionKeys keys = pck.keys();
      if (keys != null) {
        Logger.info(LOG_TAG, "Using persisted collection keys for this session.");
        session.config.setCollectionKeys(keys);
        session.advance();
        return;
      }
      Logger.info(LOG_TAG, "Failed to use persisted collection keys for this session.");
    }

    // We need an update: fetch or upload keys as necessary.
    Logger.info(LOG_TAG, "Fetching fresh collection keys for this session.");
    try {
      SyncStorageRecordRequest request = new SyncStorageRecordRequest(session.wboURI(CRYPTO_COLLECTION, "keys"));
      request.delegate = this;
      request.get();
    } catch (URISyntaxException e) {
      session.abort(e, "Invalid URI.");
    }
  }

  @Override
  public String credentials() {
    return session.credentials();
  }

  @Override
  public String ifUnmodifiedSince() {
    // TODO: last key time!
    return null;
  }

  @Override
  public void handleRequestSuccess(SyncStorageResponse response) {
    CollectionKeys k = new CollectionKeys();
    try {
      ExtendedJSONObject body = response.jsonObjectBody();
      if (Logger.LOG_PERSONAL_INFORMATION) {
        Logger.pii(LOG_TAG, "Fetched keys: " + body.toJSONString());
      }
      k.setKeyPairsFromWBO(CryptoRecord.fromJSONRecord(body), session.config.syncKeyBundle);

    } catch (IllegalStateException e) {
      session.abort(e, "Invalid keys WBO.");
      return;
    } catch (ParseException e) {
      session.abort(e, "Invalid keys WBO.");
      return;
    } catch (NonObjectJSONException e) {
      session.abort(e, "Invalid keys WBO.");
      return;
    } catch (IOException e) {
      // Some kind of lower-level error.
      session.abort(e, "IOException fetching keys.");
      return;
    } catch (CryptoException e) {
      session.abort(e, "CryptoException handling keys WBO.");
      return;
    }

    // New keys! Persist keys and server timestamp.
    Logger.info(LOG_TAG, "Setting fetched keys for this session.");
    session.config.setCollectionKeys(k);
    Logger.trace(LOG_TAG, "Persisting fetched keys and last modified.");
    PersistedCrypto5Keys pck = session.config.persistedCryptoKeys();
    pck.persistKeys(k);
    // Take the timestamp from the response since it is later than the timestamp from info/collections.
    pck.persistLastModified(response.normalizedWeaveTimestamp());

    session.advance();
  }

  @Override
  public void handleRequestFailure(SyncStorageResponse response) {
    if (retrying) {
      session.handleHTTPError(response, "Failure in refetching uploaded keys.");
      return;
    }

    int statusCode = response.getStatusCode();
    Logger.debug(LOG_TAG, "Got " + statusCode + " fetching keys.");
    if (statusCode == 404) {
      // No keys. Generate and upload, then refetch.
      CryptoRecord keysWBO;
      try {
        keysWBO = CollectionKeys.generateCollectionKeysRecord();
      } catch (CryptoException e) {
        session.abort(e, "Couldn't generate new key bundle.");
        return;
      }
      keysWBO.keyBundle = session.config.syncKeyBundle;
      try {
        keysWBO.encrypt();
      } catch (UnsupportedEncodingException e) {
        // Shouldn't occur, so let's not waste too much time on niceties. TODO
        session.abort(e, "Couldn't encrypt new key bundle: unsupported encoding.");
        return;
      } catch (CryptoException e) {
        session.abort(e, "Couldn't encrypt new key bundle.");
        return;
      }
      session.uploadKeys(keysWBO, this);
      return;
    }
    session.handleHTTPError(response, "Failure fetching keys.");
  }

  @Override
  public void handleRequestError(Exception ex) {
    session.abort(ex, "Failure fetching keys.");
  }

  @Override
  public void onKeysUploaded() {
    Logger.debug(LOG_TAG, "New keys uploaded. Starting stage again to fetch them.");
    try {
      retrying = true;
      this.execute();
    } catch (NoSuchStageException e) {
      session.abort(e, "No such stage.");
    }
  }

  @Override
  public void onKeyUploadFailed(Exception e) {
    Logger.warn(LOG_TAG, "Key upload failed. Aborting sync.");
    session.abort(e, "Key upload failed.");
  }
}
