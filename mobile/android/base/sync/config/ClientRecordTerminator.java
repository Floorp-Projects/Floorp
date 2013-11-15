/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.config;

import java.net.URI;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

/**
 * Bug 770785: when an Android Account is deleted, we need to (try to) delete
 * the associated client GUID from the server's clients table.
 * <p>
 * This class provides a static method to do that.
 */
public class ClientRecordTerminator {
  public static final String LOG_TAG = "ClientRecTerminator";

  protected ClientRecordTerminator() {
    super(); // Stop this class from being instantiated.
  }

  public static void deleteClientRecord(final String username,
      final String clusterURL,
      final String clientGuid,
      final AuthHeaderProvider authHeaderProvider)
    throws Exception {

    // Would prefer to delegate to SyncConfiguration, but that would proliferate static methods.
    final String collection = "clients";
    final URI wboURI = new URI(clusterURL + GlobalSession.API_VERSION + "/" + username + "/storage/" + collection + "/" + clientGuid);

    // Would prefer to break this out into a self-contained client library.
    final SyncStorageRecordRequest r = new SyncStorageRecordRequest(wboURI);
    r.delegate = new SyncStorageRequestDelegate() {
      @Override
      public AuthHeaderProvider getAuthHeaderProvider() {
        return authHeaderProvider;
      }

      @Override
      public String ifUnmodifiedSince() {
        return null;
      }

      @Override
      public void handleRequestSuccess(SyncStorageResponse response) {
        Logger.info(LOG_TAG, "Deleted client record with GUID " + clientGuid + " from server.");
        BaseResource.consumeEntity(response);
      }

      @Override
      public void handleRequestFailure(SyncStorageResponse response) {
        Logger.warn(LOG_TAG, "Failed to delete client record with GUID " + clientGuid + " from server.");
        try {
          Logger.warn(LOG_TAG, "Server error message was: " + response.getErrorMessage());
        } catch (Exception e) {
          // Do nothing.
        }
        BaseResource.consumeEntity(response);
      }

      @Override
      public void handleRequestError(Exception ex) {
        // It could be that we don't have network access when trying
        // to remove an Account; not much to be done in this situation.
        Logger.error(LOG_TAG, "Got exception trying to delete client record with GUID " + clientGuid + " from server; ignoring.", ex);
      }
    };

    r.delete();
  }
}
