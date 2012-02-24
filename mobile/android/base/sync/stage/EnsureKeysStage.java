/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URISyntaxException;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.CollectionKeys;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.delegates.KeyUploadDelegate;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

public class EnsureKeysStage implements GlobalSyncStage, SyncStorageRequestDelegate, KeyUploadDelegate {
  private static final String LOG_TAG = "EnsureKeysStage";
  private GlobalSession session;
  private boolean retrying = false;

  @Override
  public void execute(GlobalSession session) throws NoSuchStageException {
    this.session = session;

    // TODO: decide whether we need to do this work.
    try {
      SyncStorageRecordRequest request = new SyncStorageRecordRequest(session.wboURI("crypto", "keys"));
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

    Logger.trace(LOG_TAG, "Setting keys.");
    session.setCollectionKeys(k);
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
      this.execute(this.session);
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
