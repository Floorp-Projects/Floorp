/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.auth;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;

import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.sync.GlobalConstants;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.SyncResourceDelegate;
import org.mozilla.gecko.sync.setup.Constants;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.message.BasicHeader;

public class AuthenticateAccountStage implements AuthenticatorStage {
  private final String LOG_TAG = "AuthAccountStage";
  private HttpRequestBase httpRequest = null;

  public interface AuthenticateAccountStageDelegate {
    public void handleSuccess(boolean isSuccess);
    public void handleFailure(HttpResponse response);
    public void handleError(Exception e);
  }

  @Override
  public void execute(final AccountAuthenticator aa) throws URISyntaxException, UnsupportedEncodingException {
    AuthenticateAccountStageDelegate callbackDelegate = new AuthenticateAccountStageDelegate() {

      @Override
      public void handleSuccess(boolean isSuccess) {
        aa.isSuccess = isSuccess;
        aa.runNextStage();
      }

      @Override
      public void handleFailure(HttpResponse response) {
        Logger.debug(LOG_TAG, "handleFailure");
        aa.abort(AuthenticationResult.FAILURE_OTHER, new Exception(response.getStatusLine().getStatusCode() + " error."));
        if (response.getEntity() == null) {
          // No cleanup necessary.
          return;
        }
        try {
          BufferedReader reader = new BufferedReader(new InputStreamReader(response.getEntity().getContent(), "UTF-8"));
          Logger.warn(LOG_TAG, "content: " + reader.readLine());
          BaseResource.consumeReader(reader);
        } catch (IllegalStateException e) {
          Logger.debug(LOG_TAG, "Error reading content.", e);
        } catch (RuntimeException e) {
          Logger.debug(LOG_TAG, "Unexpected exception.", e);
          if (httpRequest != null) {
            httpRequest.abort();
          }
        } catch (IOException e) {
          Logger.debug(LOG_TAG, "Error reading content.", e);
        }
      }

      @Override
      public void handleError(Exception e) {
        Logger.debug(LOG_TAG, "handleError", e);
        aa.abort(AuthenticationResult.FAILURE_OTHER, e);
      }
    };

    // Calculate BasicAuth hash of username/password.
    String authHeader = makeAuthHeader(aa.username, aa.password);
    String authRequestUrl = makeAuthRequestUrl(aa.authServer, aa.username);
    Logger.trace(LOG_TAG, "Making auth request to: " + authRequestUrl);
    authenticateAccount(callbackDelegate, authRequestUrl, authHeader);

  }

  /**
   * Makes an authentication request to the server and passes appropriate response back to callback.
   * @param callbackDelegate
   *        Delegate to deal with HTTP response.
   * @param authRequestUrl
   * @param authHeader
   * @throws URISyntaxException
   */
  // Made public for testing.
  public void authenticateAccount(final AuthenticateAccountStageDelegate callbackDelegate, final String authRequestUrl, final String authHeader) throws URISyntaxException {
    final BaseResource httpResource = new BaseResource(authRequestUrl);
    httpResource.delegate = new SyncResourceDelegate(httpResource) {

      @Override
      public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
        // Make reference to request, to abort if necessary.
        httpRequest = request;
        client.log.enableDebug(true);
        request.setHeader(new BasicHeader("User-Agent", GlobalConstants.USER_AGENT));
        // Host header is not set for some reason, so do it explicitly.
        try {
          URI authServerUri = new URI(authRequestUrl);
          request.setHeader(new BasicHeader("Host", authServerUri.getHost()));
        } catch (URISyntaxException e) {
          Logger.error(LOG_TAG, "Malformed uri, will be caught elsewhere.", e);
        }
        request.setHeader(new BasicHeader("Authorization", authHeader));
      }

      @Override
      public void handleHttpResponse(HttpResponse response) {
        int statusCode = response.getStatusLine().getStatusCode();
        try {
          switch (statusCode) {
          case 200:
            callbackDelegate.handleSuccess(true);
            break;
          case 401:
            callbackDelegate.handleSuccess(false);
            break;
          default:
            callbackDelegate.handleFailure(response);
          }
        } finally {
          BaseResource.consumeEntity(response.getEntity());
          Logger.info(LOG_TAG, "Released entity.");
        }
      }

      @Override
      public void handleHttpProtocolException(ClientProtocolException e) {
        Logger.error(LOG_TAG, "Client protocol error.", e);
        callbackDelegate.handleError(e);
      }

      @Override
      public void handleHttpIOException(IOException e) {
        Logger.error(LOG_TAG, "I/O exception.");
        callbackDelegate.handleError(e);
      }

      @Override
      public void handleTransportException(GeneralSecurityException e) {
        Logger.error(LOG_TAG, "Transport exception.");
        callbackDelegate.handleError(e);
      }
    };

    AccountAuthenticator.runOnThread(new Runnable() {
      @Override
      public void run() {
        httpResource.get();
      }
    });
  }

  public String makeAuthHeader(String usernameHash, String password) {
    try {
      return "Basic " + Base64.encodeBase64String((usernameHash + ":" + password).getBytes("UTF-8"));
    } catch (UnsupportedEncodingException e) {
      Logger.debug(LOG_TAG, "Unsupported encoding: UTF-8.");
      return null;
    }
  }

  public String makeAuthRequestUrl(String authServer, String usernameHash) {
    return authServer + Constants.AUTH_SERVER_VERSION + usernameHash + "/" + Constants.AUTH_SERVER_SUFFIX;
  }
}
