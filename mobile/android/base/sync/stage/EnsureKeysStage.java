/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Richard Newman <rnewman@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.delegates.KeyUploadDelegate;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import android.util.Log;

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
      Log.i(LOG_TAG, "Fetched keys: " + body.toJSONString());
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

    Log.i("rnewman", "Setting keys. Yay!");
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
    Log.i(LOG_TAG, "Got " + statusCode + " fetching keys.");
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
    Log.i(LOG_TAG, "New keys uploaded. Starting stage again to fetch them.");
    try {
      this.execute(this.session);
    } catch (NoSuchStageException e) {
      session.abort(e, "No such stage.");
    }
  }

  @Override
  public void onKeyUploadFailed(Exception e) {
    Log.i(LOG_TAG, "Key upload failed. Aborting sync.");
    session.abort(e, "Key upload failed.");
  }

}
