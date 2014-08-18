/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa.oauth;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.concurrent.Executor;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.net.BaseResource;

import ch.boye.httpclientandroidlib.HttpResponse;

/**
 * Talk to an fxa-oauth-server to get "implicitly granted" OAuth tokens.
 * <p>
 * To use this client, you will need a pre-allocated fxa-oauth-server
 * "client_id" with special "implicit grant" permissions.
 * <p>
 * This client was written against the API documented at <a href="https://github.com/mozilla/fxa-oauth-server/blob/41538990df9e91158558ae5a8115194383ac3b05/docs/api.md">https://github.com/mozilla/fxa-oauth-server/blob/41538990df9e91158558ae5a8115194383ac3b05/docs/api.md</a>.
 */
public class FxAccountOAuthClient10 extends FxAccountAbstractClient {
  protected static final String LOG_TAG = FxAccountOAuthClient10.class.getSimpleName();

  protected static final String AUTHORIZATION_RESPONSE_TYPE = "token";

  protected static final String JSON_KEY_ACCESS_TOKEN = "access_token";
  protected static final String JSON_KEY_ASSERTION = "assertion";
  protected static final String JSON_KEY_CLIENT_ID = "client_id";
  protected static final String JSON_KEY_RESPONSE_TYPE = "response_type";
  protected static final String JSON_KEY_SCOPE = "scope";
  protected static final String JSON_KEY_STATE = "state";
  protected static final String JSON_KEY_TOKEN_TYPE = "token_type";

  // access_token: A string that can be used for authorized requests to service providers.
  // scope: A string of space-separated permissions that this token has. May differ from requested scopes, since user can deny permissions.
  // token_type: A string representing the token type. Currently will always be "bearer".
  protected static final String[] AUTHORIZATION_RESPONSE_REQUIRED_STRING_FIELDS = new String[] { JSON_KEY_ACCESS_TOKEN, JSON_KEY_SCOPE, JSON_KEY_TOKEN_TYPE };

  public FxAccountOAuthClient10(String serverURI, Executor executor) {
    super(serverURI, executor);
  }

  /**
   * Thin container for an authorization response.
   */
  public static class AuthorizationResponse {
    public final String access_token;
    public final String token_type;
    public final String scope;

    public AuthorizationResponse(String access_token, String token_type, String scope) {
      this.access_token = access_token;
      this.token_type = token_type;
      this.scope = scope;
    }
  }

  public void authorization(String client_id, String assertion, String state, String scope,
                            RequestDelegate<AuthorizationResponse> delegate) {
    final BaseResource resource;
    try {
      resource = new BaseResource(new URI(serverURI + "authorization"));
    } catch (URISyntaxException e) {
      invokeHandleError(delegate, e);
      return;
    }

    resource.delegate = new ResourceDelegate<AuthorizationResponse>(resource, delegate) {
      @Override
      public void handleSuccess(int status, HttpResponse response, ExtendedJSONObject body) {
        try {
          body.throwIfFieldsMissingOrMisTyped(AUTHORIZATION_RESPONSE_REQUIRED_STRING_FIELDS, String.class);
          String access_token = body.getString(JSON_KEY_ACCESS_TOKEN);
          String token_type = body.getString(JSON_KEY_TOKEN_TYPE);
          String scope = body.getString(JSON_KEY_SCOPE);
          delegate.handleSuccess(new AuthorizationResponse(access_token, token_type, scope));
          return;
        } catch (Exception e) {
          delegate.handleError(e);
          return;
        }
      }
    };

    final ExtendedJSONObject requestBody = new ExtendedJSONObject();
    requestBody.put(JSON_KEY_RESPONSE_TYPE, AUTHORIZATION_RESPONSE_TYPE);
    requestBody.put(JSON_KEY_CLIENT_ID, client_id);
    requestBody.put(JSON_KEY_ASSERTION, assertion);
    if (scope != null) {
      requestBody.put(JSON_KEY_SCOPE, scope);
    }
    if (state != null) {
      requestBody.put(JSON_KEY_STATE, state);
    }

    post(resource, requestBody, delegate);
  }
}
