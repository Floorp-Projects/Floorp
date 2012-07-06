/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import java.io.IOException;
import java.net.URISyntaxException;
import java.util.HashSet;
import java.util.Set;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.CollectionKeys;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.NoCollectionKeysSetException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.crypto.PersistedCrypto5Keys;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

public class EnsureCrypto5KeysStage
extends AbstractNonRepositorySyncStage
implements SyncStorageRequestDelegate {

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
    if (retrying || !infoCollections.updateNeeded(CRYPTO_COLLECTION, lastModified)) {
      // Try to use our local collection keys for this session.
      Logger.debug(LOG_TAG, "Trying to use persisted collection keys for this session.");
      CollectionKeys keys = pck.keys();
      if (keys != null) {
        Logger.trace(LOG_TAG, "Using persisted collection keys for this session.");
        session.config.setCollectionKeys(keys);
        session.advance();
        return;
      }
      Logger.trace(LOG_TAG, "Failed to use persisted collection keys for this session.");
    }

    // We need an update: fetch fresh keys.
    Logger.debug(LOG_TAG, "Fetching fresh collection keys for this session.");
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

  protected void setAndPersist(PersistedCrypto5Keys pck, CollectionKeys keys, long timestamp) {
    session.config.setCollectionKeys(keys);
    pck.persistKeys(keys);
    pck.persistLastModified(timestamp);
  }

  /**
   * Return collections where either the individual key has changed, or if the
   * new default key is not the same as the old default key, where the
   * collection is using the default key.
   */
  protected Set<String> collectionsToUpdate(CollectionKeys oldKeys, CollectionKeys newKeys) {
    // These keys have explicitly changed; they definitely need updating.
    Set<String> changedKeys = new HashSet<String>(CollectionKeys.differences(oldKeys, newKeys));

    boolean defaultKeyChanged = true; // Most pessimistic is to assume default key has changed.
    KeyBundle newDefaultKeyBundle = null;
    try {
      KeyBundle oldDefaultKeyBundle = oldKeys.defaultKeyBundle();
      newDefaultKeyBundle = newKeys.defaultKeyBundle();
      defaultKeyChanged = !oldDefaultKeyBundle.equals(newDefaultKeyBundle);
    } catch (NoCollectionKeysSetException e) {
      Logger.warn(LOG_TAG, "NoCollectionKeysSetException in EnsureCrypto5KeysStage.", e);
    }

    if (newDefaultKeyBundle == null) {
      Logger.trace(LOG_TAG, "New default key not provided; returning changed individual keys.");
      return changedKeys;
    }

    if (!defaultKeyChanged) {
      Logger.trace(LOG_TAG, "New default key is the same as old default key; returning changed individual keys.");
      return changedKeys;
    }

    // New keys have a different default/sync key; check known collections against the default key.
    Logger.debug(LOG_TAG, "New default key is not the same as old default key.");
    for (Stage stage : Stage.getNamedStages()) {
      String name = stage.getRepositoryName();
      if (!newKeys.keyBundleForCollectionIsNotDefault(name)) {
        // Default key has changed, so this collection has changed.
        changedKeys.add(name);
      }
    }

    return changedKeys;
  }

  @Override
  public void handleRequestSuccess(SyncStorageResponse response) {
    // Take the timestamp from the response since it is later than the timestamp from info/collections.
    long responseTimestamp = response.normalizedWeaveTimestamp();
    CollectionKeys keys = new CollectionKeys();
    try {
      ExtendedJSONObject body = response.jsonObjectBody();
      if (Logger.LOG_PERSONAL_INFORMATION) {
        Logger.pii(LOG_TAG, "Fetched keys: " + body.toJSONString());
      }
      keys.setKeyPairsFromWBO(CryptoRecord.fromJSONRecord(body), session.config.syncKeyBundle);
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

    PersistedCrypto5Keys pck = session.config.persistedCryptoKeys();
    if (!pck.persistedKeysExist()) {
      // New keys, and no old keys! Persist keys and server timestamp.
      Logger.trace(LOG_TAG, "Setting fetched keys for this session; persisting fetched keys and last modified.");
      setAndPersist(pck, keys, responseTimestamp);
      session.advance();
      return;
    }

    // New keys, but we had old keys.  Check for differences.
    CollectionKeys oldKeys = pck.keys();
    Set<String> changedCollections = collectionsToUpdate(oldKeys, keys);
    if (!changedCollections.isEmpty()) {
      // New keys, different from old keys.
      Logger.trace(LOG_TAG, "Fetched keys are not the same as persisted keys; " +
          "setting fetched keys for this session before resetting changed engines.");
      setAndPersist(pck, keys, responseTimestamp);
      session.resetStagesByName(changedCollections);
      session.abort(null, "crypto/keys changed on server.");
      return;
    }

    // New keys don't differ from old keys; persist timestamp and move on.
    Logger.trace(LOG_TAG, "Fetched keys are the same as persisted keys; persisting only last modified.");
    session.config.setCollectionKeys(oldKeys);
    pck.persistLastModified(response.normalizedWeaveTimestamp());
    session.advance();
  }

  @Override
  public void handleRequestFailure(SyncStorageResponse response) {
    if (retrying) {
      // Should happen very rarely -- this means we uploaded our crypto/keys
      // successfully, but failed to re-download.
      session.handleHTTPError(response, "Failure while re-downloading already uploaded keys.");
      return;
    }

    int statusCode = response.getStatusCode();
    if (statusCode == 404) {
      Logger.info(LOG_TAG, "Got 404 fetching keys.  Fresh starting since keys are missing on server.");
      session.freshStart();
      return;
    }
    session.handleHTTPError(response, "Failure fetching keys: got response status code " + statusCode);
  }

  @Override
  public void handleRequestError(Exception ex) {
    session.abort(ex, "Failure fetching keys.");
  }
}
