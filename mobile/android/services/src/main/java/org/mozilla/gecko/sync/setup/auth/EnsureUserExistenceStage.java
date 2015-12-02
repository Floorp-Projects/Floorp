/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.auth;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.SyncConstants;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;

public class EnsureUserExistenceStage implements AuthenticatorStage {
  private final String LOG_TAG = "EnsureUserExistence";

  public interface EnsureUserExistenceStageDelegate {
    public void handleSuccess();
    public void handleFailure(AuthenticationResult result);
    public void handleError(Exception e);
  }
  @Override
  public void execute(final AccountAuthenticator aa) throws URISyntaxException,
      UnsupportedEncodingException {
    final EnsureUserExistenceStageDelegate callbackDelegate = new EnsureUserExistenceStageDelegate() {

      @Override
      public void handleSuccess() {
        // User exists; now determine auth node.
        Logger.debug(LOG_TAG, "handleSuccess()");
        aa.runNextStage();
      }

      @Override
      public void handleFailure(AuthenticationResult result) {
        aa.abort(result, new Exception("Failure in EnsureUser"));
      }

      @Override
      public void handleError(Exception e) {
        Logger.info(LOG_TAG, "Error checking for user existence.");
        aa.abort(AuthenticationResult.FAILURE_SERVER, e);
      }

    };

    // This is not the same as Utils.nodeWeaveURL: it's missing the trailing node/weave.
    String userRequestUrl = aa.nodeServer + "user/1.0/" + aa.username;
    final BaseResource httpResource = new BaseResource(userRequestUrl);
    httpResource.delegate = new BaseResourceDelegate(httpResource) {
      @Override
      public String getUserAgent() {
        return SyncConstants.USER_AGENT;
      }

      @Override
      public void handleHttpResponse(HttpResponse response) {
        int statusCode = response.getStatusLine().getStatusCode();
        switch(statusCode) {
        case 200:
          try {
            InputStream content = response.getEntity().getContent();
            BufferedReader reader = new BufferedReader(new InputStreamReader(content, "UTF-8"), 1024);
            String inUse = reader.readLine();
            BaseResource.consumeReader(reader);
            reader.close();
            // Removed Logger.debug inUse, because stalling.
            if (inUse.equals("1")) { // Username exists.
              callbackDelegate.handleSuccess();
            } else { // User does not exist.
              Logger.info(LOG_TAG, "No such user.");
              callbackDelegate.handleFailure(AuthenticationResult.FAILURE_USERNAME);
            }
          } catch (Exception e) {
            Logger.error(LOG_TAG, "Failure in content parsing.", e);
            callbackDelegate.handleFailure(AuthenticationResult.FAILURE_OTHER);
          }
          break;
        default: // No other response is acceptable.
          callbackDelegate.handleFailure(AuthenticationResult.FAILURE_OTHER);
        }
        Logger.debug(LOG_TAG, "Consuming entity.");
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
    // Make request.
    AccountAuthenticator.runOnThread(new Runnable() {

      @Override
      public void run() {
        httpResource.get();
      }
    });
  }

}
