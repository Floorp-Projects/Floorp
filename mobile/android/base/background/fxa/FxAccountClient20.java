/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa;

import java.net.URI;
import java.util.concurrent.Executor;

import org.json.simple.JSONObject;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.net.BaseResource;

import ch.boye.httpclientandroidlib.HttpResponse;

public class FxAccountClient20 extends FxAccountClient10 {
  protected static final String[] LOGIN_RESPONSE_REQUIRED_STRING_FIELDS = new String[] { JSON_KEY_UID, JSON_KEY_SESSIONTOKEN };
  protected static final String[] LOGIN_RESPONSE_REQUIRED_STRING_FIELDS_KEYS = new String[] { JSON_KEY_UID, JSON_KEY_SESSIONTOKEN, JSON_KEY_KEYFETCHTOKEN, };
  protected static final String[] LOGIN_RESPONSE_REQUIRED_BOOLEAN_FIELDS = new String[] { JSON_KEY_VERIFIED };

  public FxAccountClient20(String serverURI, Executor executor) {
    super(serverURI, executor);
  }

  public void createAccount(final byte[] emailUTF8, final byte[] quickStretchedPW, final boolean preVerified,
      final RequestDelegate<String> delegate) {
    try {
      createAccount(new FxAccount20CreateDelegate(emailUTF8, quickStretchedPW, preVerified), delegate);
    } catch (final Exception e) {
      invokeHandleError(delegate, e);
      return;
    }
  }

  /**
   * Thin container for login response.
   */
  public static class LoginResponse {
    public final String serverURI;
    public final String uid;
    public final byte[] sessionToken;
    public final boolean verified;
    public final byte[] keyFetchToken;

    public LoginResponse(String serverURI, String uid, boolean verified, byte[] sessionToken, byte[] keyFetchToken) {
      // This is pretty awful.
      this.serverURI = serverURI.endsWith(VERSION_FRAGMENT) ?
          serverURI.substring(0, serverURI.length() - VERSION_FRAGMENT.length()) :
          serverURI;
      this.uid = uid;
      this.verified = verified;
      this.sessionToken = sessionToken;
      this.keyFetchToken = keyFetchToken;
    }
  }

  public void login(final byte[] emailUTF8, final byte[] quickStretchedPW,
      final RequestDelegate<LoginResponse> delegate) {
    login(emailUTF8, quickStretchedPW, false, delegate);
  }

  public void loginAndGetKeys(final byte[] emailUTF8, final byte[] quickStretchedPW,
      final RequestDelegate<LoginResponse> delegate) {
    login(emailUTF8, quickStretchedPW, true, delegate);
  }

  // Public for testing only; prefer login and loginAndGetKeys (without boolean parameter).
  public void login(final byte[] emailUTF8, final byte[] quickStretchedPW, final boolean getKeys,
      final RequestDelegate<LoginResponse> delegate) {
    BaseResource resource;
    JSONObject body;
    final String path = getKeys ? "account/login?keys=true" : "account/login";
    try {
      resource = new BaseResource(new URI(serverURI + path));
      body = new FxAccount20LoginDelegate(emailUTF8, quickStretchedPW).getCreateBody();
    } catch (Exception e) {
      invokeHandleError(delegate, e);
      return;
    }

    resource.delegate = new ResourceDelegate<LoginResponse>(resource, delegate) {
      @Override
      public void handleSuccess(int status, HttpResponse response, ExtendedJSONObject body) {
        try {
          String[] requiredStringFields;
          if (!getKeys) {
            requiredStringFields = LOGIN_RESPONSE_REQUIRED_STRING_FIELDS;
          } else {
            requiredStringFields = LOGIN_RESPONSE_REQUIRED_STRING_FIELDS_KEYS;
          }
          String[] requiredBooleanFields = LOGIN_RESPONSE_REQUIRED_BOOLEAN_FIELDS;

          body.throwIfFieldsMissingOrMisTyped(requiredStringFields, String.class);
          body.throwIfFieldsMissingOrMisTyped(requiredBooleanFields, Boolean.class);

          LoginResponse loginResponse;
          String uid = body.getString(JSON_KEY_UID);
          boolean verified = body.getBoolean(JSON_KEY_VERIFIED);
          byte[] sessionToken = Utils.hex2Byte(body.getString(JSON_KEY_SESSIONTOKEN));
          byte[] keyFetchToken = null;
          if (getKeys) {
            keyFetchToken = Utils.hex2Byte(body.getString(JSON_KEY_KEYFETCHTOKEN));
          }
          loginResponse = new LoginResponse(serverURI, uid, verified, sessionToken, keyFetchToken);

          delegate.handleSuccess(loginResponse);
          return;
        } catch (Exception e) {
          delegate.handleError(e);
          return;
        }
      }
    };

    post(resource, body, delegate);
  }
}
