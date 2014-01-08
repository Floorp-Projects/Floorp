/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tokenserver;

import java.io.IOException;
import java.net.URI;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;

import org.json.simple.JSONObject;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonArrayJSONException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.UnexpectedJSONException.BadRequiredFieldJSONException;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;
import org.mozilla.gecko.sync.net.BrowserIDAuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.tokenserver.TokenServerException.TokenServerConditionsRequiredException;
import org.mozilla.gecko.tokenserver.TokenServerException.TokenServerInvalidCredentialsException;
import org.mozilla.gecko.tokenserver.TokenServerException.TokenServerMalformedRequestException;
import org.mozilla.gecko.tokenserver.TokenServerException.TokenServerMalformedResponseException;
import org.mozilla.gecko.tokenserver.TokenServerException.TokenServerUnknownServiceException;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.message.BasicHeader;

/**
 * HTTP client for interacting with the Mozilla Services Token Server API v1.0,
 * as documented at
 * <a href="http://docs.services.mozilla.com/token/apis.html">http://docs.services.mozilla.com/token/apis.html</a>.
 * <p>
 * A token server accepts some authorization credential and returns a different
 * authorization credential. Usually, it used to exchange a public-key
 * authorization token that is expensive to validate for a symmetric-key
 * authorization that is cheap to validate. For example, we might exchange a
 * BrowserID assertion for a HAWK id and key pair.
 */
public class TokenServerClient {
  protected static final String LOG_TAG = "TokenServerClient";

  public static final String JSON_KEY_API_ENDPOINT = "api_endpoint";
  public static final String JSON_KEY_CONDITION_URLS = "condition_urls";
  public static final String JSON_KEY_DURATION = "duration";
  public static final String JSON_KEY_ERRORS = "errors";
  public static final String JSON_KEY_ID = "id";
  public static final String JSON_KEY_KEY = "key";
  public static final String JSON_KEY_UID = "uid";

  protected final Executor executor;
  protected final URI uri;

  public TokenServerClient(URI uri, Executor executor) {
    if (uri == null) {
      throw new IllegalArgumentException("uri must not be null");
    }
    if (executor == null) {
      throw new IllegalArgumentException("executor must not be null");
    }
    this.uri = uri;
    this.executor = executor;
  }

  protected void invokeHandleSuccess(final TokenServerClientDelegate delegate, final TokenServerToken token) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        delegate.handleSuccess(token);
      }
    });
  }

  protected void invokeHandleFailure(final TokenServerClientDelegate delegate, final TokenServerException e) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        delegate.handleFailure(e);
      }
    });
  }

  protected void invokeHandleError(final TokenServerClientDelegate delegate, final Exception e) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        delegate.handleError(e);
      }
    });
  }

  public TokenServerToken processResponse(HttpResponse response)
      throws TokenServerException {
    SyncResponse res = new SyncResponse(response);
    int statusCode = res.getStatusCode();

    Logger.debug(LOG_TAG, "Got token response with status code " + statusCode + ".");

    // Responses should *always* be JSON, even in the case of 4xx and 5xx
    // errors. If we don't see JSON, the server is likely very unhappy.
    String contentType = response.getEntity().getContentType().getValue();
    if (contentType != "application/json" && !contentType.startsWith("application/json;")) {
      Logger.warn(LOG_TAG, "Got non-JSON response with Content-Type " +
          contentType + ". Misconfigured server?");
      throw new TokenServerMalformedResponseException(null, "Non-JSON response Content-Type.");
    }

    // Responses should *always* be a valid JSON object.
    ExtendedJSONObject result;
    try {
      result = res.jsonObjectBody();
    } catch (Exception e) {
      Logger.debug(LOG_TAG, "Malformed token response.", e);
      throw new TokenServerMalformedResponseException(null, e);
    }

    // The service shouldn't have any 3xx, so we don't need to handle those.
    if (res.getStatusCode() != 200) {
      // We should have a (Cornice) error report in the JSON. We log that to
      // help with debugging.
      List<ExtendedJSONObject> errorList = new ArrayList<ExtendedJSONObject>();

      if (result.containsKey(JSON_KEY_ERRORS)) {
        try {
          for (Object error : result.getArray(JSON_KEY_ERRORS)) {
            Logger.warn(LOG_TAG, "" + error);

            if (error instanceof JSONObject) {
              errorList.add(new ExtendedJSONObject((JSONObject) error));
            }
          }
        } catch (NonArrayJSONException e) {
          Logger.warn(LOG_TAG, "Got non-JSON array '" + result.getString(JSON_KEY_ERRORS) + "'.", e);
        }
      }

      if (statusCode == 400) {
        throw new TokenServerMalformedRequestException(errorList, result.toJSONString());
      }

      if (statusCode == 401) {
        throw new TokenServerInvalidCredentialsException(errorList, result.toJSONString());
      }

      // 403 should represent a "condition acceptance needed" response.
      //
      // The extra validation of "urls" is important. We don't want to signal
      // conditions required unless we are absolutely sure that is what the
      // server is asking for.
      if (statusCode == 403) {
        // Bug 792674 and Bug 783598: make this testing simpler. For now, we
        // check that errors is an array, and take any condition_urls from the
        // first element.

        try {
          if (errorList == null || errorList.isEmpty()) {
            throw new TokenServerMalformedResponseException(errorList, "403 response without proper fields.");
          }

          ExtendedJSONObject error = errorList.get(0);

          ExtendedJSONObject condition_urls = error.getObject(JSON_KEY_CONDITION_URLS);
          if (condition_urls != null) {
            throw new TokenServerConditionsRequiredException(condition_urls);
          }
        } catch (NonObjectJSONException e) {
          Logger.warn(LOG_TAG, "Got non-JSON error object.");
        }

        throw new TokenServerMalformedResponseException(errorList, "403 response without proper fields.");
      }

      if (statusCode == 404) {
        throw new TokenServerUnknownServiceException(errorList);
      }

      // We shouldn't ever get here...
      throw new TokenServerException(errorList);
    }

    try {
      result.throwIfFieldsMissingOrMisTyped(new String[] { JSON_KEY_ID, JSON_KEY_KEY, JSON_KEY_API_ENDPOINT }, String.class);
      result.throwIfFieldsMissingOrMisTyped(new String[] { JSON_KEY_UID }, Long.class);
    } catch (BadRequiredFieldJSONException e ) {
      throw new TokenServerMalformedResponseException(null, e);
    }

    Logger.debug(LOG_TAG, "Successful token response: " + result.getString(JSON_KEY_ID));

    return new TokenServerToken(result.getString(JSON_KEY_ID),
        result.getString(JSON_KEY_KEY),
        result.get(JSON_KEY_UID).toString(),
        result.getString(JSON_KEY_API_ENDPOINT));
  }

  public void getTokenFromBrowserIDAssertion(final String assertion, final boolean conditionsAccepted,
      final TokenServerClientDelegate delegate) {
    BaseResource r = new BaseResource(uri);

    r.delegate = new BaseResourceDelegate(r) {
      @Override
      public void handleHttpResponse(HttpResponse response) {
        try {
          TokenServerToken token = processResponse(response);
          invokeHandleSuccess(delegate, token);
        } catch (TokenServerException e) {
          invokeHandleFailure(delegate, e);
        }
      }

      @Override
      public void handleTransportException(GeneralSecurityException e) {
        invokeHandleError(delegate, e);
      }

      @Override
      public void handleHttpProtocolException(ClientProtocolException e) {
        invokeHandleError(delegate, e);
      }

      @Override
      public void handleHttpIOException(IOException e) {
        invokeHandleError(delegate, e);
      }

      @Override
      public AuthHeaderProvider getAuthHeaderProvider() {
        return new BrowserIDAuthHeaderProvider(assertion);
      }

      @Override
      public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
        String host = request.getURI().getHost();
        request.setHeader(new BasicHeader("Host", host));

        if (conditionsAccepted) {
          request.addHeader("X-Conditions-Accepted", "1");
        }
      }
    };

    r.get();
  }
}
