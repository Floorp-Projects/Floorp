/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.auth;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;
import org.mozilla.gecko.sync.setup.Constants;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;

public class FetchUserNodeStage implements AuthenticatorStage {
  private final String LOG_TAG = "FetchUserNodeStage";

  public interface FetchNodeStageDelegate {
    public void handleSuccess(String url);
    public void handleFailure(HttpResponse response);
    public void handleError(Exception e);
  }

  @Override
  public void execute(final AccountAuthenticator aa) throws URISyntaxException {

    FetchNodeStageDelegate callbackDelegate = new FetchNodeStageDelegate() {

      @Override
      public void handleSuccess(String server) {
        if (server == null) { // No separate auth node; use server url.
          Logger.debug(LOG_TAG, "Using server as auth node.");
          aa.authServer = aa.nodeServer;
          aa.runNextStage();
          return;
        }
        if (!server.endsWith("/")) {
          server += "/";
        }
        aa.authServer = server;
        aa.runNextStage();
      }

      @Override
      public void handleFailure(HttpResponse response) {
        int statusCode = response.getStatusLine().getStatusCode();
        Logger.debug(LOG_TAG, "Failed to fetch user node, with status " + statusCode);
        aa.abort(AuthenticationResult.FAILURE_OTHER, new Exception("HTTP " + statusCode + " error."));
      }

      @Override
      public void handleError(Exception e) {
        Logger.debug(LOG_TAG, "Error in fetching node.");
        aa.abort(AuthenticationResult.FAILURE_OTHER, e);
      }
    };
    String nodeRequestUrl = aa.nodeServer + Constants.AUTH_NODE_PATHNAME + Constants.AUTH_NODE_VERSION + aa.username + "/" + Constants.AUTH_NODE_SUFFIX;
    // Might contain a plaintext username in the case of old Sync accounts.
    Logger.pii(LOG_TAG, "NodeUrl: " + nodeRequestUrl);
    final BaseResource httpResource = makeFetchNodeRequest(callbackDelegate, nodeRequestUrl);
    // Make request on separate thread.
    AccountAuthenticator.runOnThread(new Runnable() {
      @Override
      public void run() {
        httpResource.get();
      }
    });
  }

  private BaseResource makeFetchNodeRequest(final FetchNodeStageDelegate callbackDelegate, String fetchNodeUrl) throws URISyntaxException {
    // Fetch node containing user.
    final BaseResource httpResource = new BaseResource(fetchNodeUrl);
    httpResource.delegate = new BaseResourceDelegate(httpResource) {

      @Override
      public void handleHttpResponse(HttpResponse response) {
        int statusCode = response.getStatusLine().getStatusCode();
        switch(statusCode) {
        case 200:
          try {
            InputStream content = response.getEntity().getContent();
            BufferedReader reader = new BufferedReader(new InputStreamReader(content, "UTF-8"), 1024);
            String server = reader.readLine();
            callbackDelegate.handleSuccess(server);
            BaseResource.consumeReader(reader);
            reader.close();
          } catch (IllegalStateException e) {
            callbackDelegate.handleError(e);
          } catch (IOException e) {
            callbackDelegate.handleError(e);
          }
          break;
        case 404: // Does not support auth nodes, use server instead.
          callbackDelegate.handleSuccess(null);
          break;
        default:
          // No other acceptable states.
          callbackDelegate.handleFailure(response);
        }
        BaseResource.consumeEntity(response.getEntity());
      }

      @Override
      public void handleHttpProtocolException(ClientProtocolException e) {
        callbackDelegate.handleError(e);
      }

      @Override
      public void handleHttpIOException(IOException e) {
        callbackDelegate.handleError(e);
      }

      @Override
      public void handleTransportException(GeneralSecurityException e) {
        callbackDelegate.handleError(e);
      }
    };
    return httpResource;
  }
}