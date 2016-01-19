/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.fxa.login;

import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient10.StatusResponse;
import org.mozilla.gecko.background.fxa.FxAccountClient10.TwoKeys;
import org.mozilla.gecko.background.fxa.FxAccountClient10.LoginResponse;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.background.fxa.FxAccountRemoteError;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.browserid.MockMyIDTokenFactory;
import org.mozilla.gecko.browserid.RSACryptoImplementation;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;

import java.io.UnsupportedEncodingException;
import java.util.HashMap;
import java.util.Map;

import ch.boye.httpclientandroidlib.HttpStatus;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.entity.StringEntity;
import ch.boye.httpclientandroidlib.message.BasicHttpResponse;

public class MockFxAccountClient implements FxAccountClient {
  protected static MockMyIDTokenFactory mockMyIdTokenFactory = new MockMyIDTokenFactory();

  public final String serverURI = "http://testServer.com";

  public final Map<String, User> users = new HashMap<String, User>();
  public final Map<String, String> sessionTokens = new HashMap<String, String>();
  public final Map<String, String> keyFetchTokens = new HashMap<String, String>();

  public static class User {
    public final String email;
    public final byte[] quickStretchedPW;
    public final String uid;
    public boolean verified;
    public final byte[] kA;
    public final byte[] wrapkB;

    public User(String email, byte[] quickStretchedPW) {
      this.email = email;
      this.quickStretchedPW = quickStretchedPW;
      this.uid = "uid/" + this.email;
      this.verified = false;
      this.kA = Utils.generateRandomBytes(FxAccountUtils.CRYPTO_KEY_LENGTH_BYTES);
      this.wrapkB = Utils.generateRandomBytes(FxAccountUtils.CRYPTO_KEY_LENGTH_BYTES);
    }
  }

  protected LoginResponse addLogin(User user, byte[] sessionToken, byte[] keyFetchToken) {
    // byte[] sessionToken = Utils.generateRandomBytes(8);
    if (sessionToken != null) {
      sessionTokens.put(Utils.byte2Hex(sessionToken), user.email);
    }
    // byte[] keyFetchToken = Utils.generateRandomBytes(8);
    if (keyFetchToken != null) {
      keyFetchTokens.put(Utils.byte2Hex(keyFetchToken), user.email);
    }
    return new LoginResponse(user.email, user.uid, user.verified, sessionToken, keyFetchToken);
  }

  public void addUser(String email, byte[] quickStretchedPW, boolean verified, byte[] sessionToken, byte[] keyFetchToken) {
    User user = new User(email, quickStretchedPW);
    users.put(email, user);
    if (verified) {
      verifyUser(email);
    }
    addLogin(user, sessionToken, keyFetchToken);
  }

  public void verifyUser(String email) {
    users.get(email).verified = true;
  }

  public void clearAllUserTokens() throws UnsupportedEncodingException {
    sessionTokens.clear();
    keyFetchTokens.clear();
  }

  protected BasicHttpResponse makeHttpResponse(int statusCode, String body) {
    BasicHttpResponse httpResponse = new BasicHttpResponse(new ProtocolVersion("HTTP", 1, 1), statusCode, body);
    httpResponse.setEntity(new StringEntity(body, "UTF-8"));
    return httpResponse;
  }

  protected <T> void handleFailure(RequestDelegate<T> requestDelegate, int code, int errno, String message) {
    requestDelegate.handleFailure(new FxAccountClientRemoteException(makeHttpResponse(code, message),
        code, errno, "Bad authorization", message, null, new ExtendedJSONObject()));
  }

  @Override
  public void status(byte[] sessionToken, RequestDelegate<StatusResponse> requestDelegate) {
    String email = sessionTokens.get(Utils.byte2Hex(sessionToken));
    User user = users.get(email);
    if (email == null || user == null) {
      handleFailure(requestDelegate, HttpStatus.SC_UNAUTHORIZED, FxAccountRemoteError.INVALID_AUTHENTICATION_TOKEN, "invalid sessionToken");
      return;
    }
    requestDelegate.handleSuccess(new StatusResponse(email, user.verified));
  }

  @Override
  public void keys(byte[] keyFetchToken, RequestDelegate<TwoKeys> requestDelegate) {
    String email = keyFetchTokens.get(Utils.byte2Hex(keyFetchToken));
    User user = users.get(email);
    if (email == null || user == null) {
      handleFailure(requestDelegate, HttpStatus.SC_UNAUTHORIZED, FxAccountRemoteError.INVALID_AUTHENTICATION_TOKEN, "invalid keyFetchToken");
      return;
    }
    if (!user.verified) {
      handleFailure(requestDelegate, HttpStatus.SC_BAD_REQUEST, FxAccountRemoteError.ATTEMPT_TO_OPERATE_ON_AN_UNVERIFIED_ACCOUNT, "user is unverified");
      return;
    }
    requestDelegate.handleSuccess(new TwoKeys(user.kA, user.wrapkB));
  }

  @Override
  public void sign(byte[] sessionToken, ExtendedJSONObject publicKey, long certificateDurationInMilliseconds, RequestDelegate<String> requestDelegate) {
    String email = sessionTokens.get(Utils.byte2Hex(sessionToken));
    User user = users.get(email);
    if (email == null || user == null) {
      handleFailure(requestDelegate, HttpStatus.SC_UNAUTHORIZED, FxAccountRemoteError.INVALID_AUTHENTICATION_TOKEN, "invalid sessionToken");
      return;
    }
    if (!user.verified) {
      handleFailure(requestDelegate, HttpStatus.SC_BAD_REQUEST, FxAccountRemoteError.ATTEMPT_TO_OPERATE_ON_AN_UNVERIFIED_ACCOUNT, "user is unverified");
      return;
    }
    try {
      final long iat = System.currentTimeMillis();
      final long dur = certificateDurationInMilliseconds;
      final long exp = iat + dur;
      String certificate = mockMyIdTokenFactory.createMockMyIDCertificate(RSACryptoImplementation.createPublicKey(publicKey), "test", iat, exp);
      requestDelegate.handleSuccess(certificate);
    } catch (Exception e) {
      requestDelegate.handleError(e);
    }
  }
}
