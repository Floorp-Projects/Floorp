/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.fxa;

import java.net.URI;
import java.security.NoSuchAlgorithmException;
import java.util.concurrent.Executors;

import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.JSONWebTokenUtils;
import org.mozilla.gecko.browserid.MockMyIDTokenFactory;
import org.mozilla.gecko.browserid.RSACryptoImplementation;
import org.mozilla.gecko.tokenserver.TokenServerClient;
import org.mozilla.gecko.tokenserver.TokenServerClientDelegate;
import org.mozilla.gecko.tokenserver.TokenServerException;
import org.mozilla.gecko.tokenserver.TokenServerException.TokenServerInvalidCredentialsException;
import org.mozilla.gecko.tokenserver.TokenServerToken;

public class TestLiveTokenServerClientOnDevice extends AndroidSyncTestCase {
  public static final String TEST_USERNAME = "test";

  public static final String TEST_REMOTE_SERVER_URL = "http://auth.oldsync.dev.lcip.org";
  public static final String TEST_REMOTE_AUDIENCE = "http://auth.oldsync.dev.lcip.org"; // Audience accepted by the token server.
  public static final String TEST_REMOTE_URL = TEST_REMOTE_SERVER_URL + "/1.0/sync/1.1";
  public static final String TEST_ENDPOINT = "http://db1.oldsync.dev.lcip.org/1.1/";

  protected final MockMyIDTokenFactory mockMyIDTokenFactory;
  protected final BrowserIDKeyPair keyPair;

  protected TokenServerClient client;

  public TestLiveTokenServerClientOnDevice() throws NoSuchAlgorithmException {
    this.mockMyIDTokenFactory = new MockMyIDTokenFactory();
    this.keyPair = RSACryptoImplementation.generateKeypair(1024);
  }

  public void setUp() throws Exception {
    client = new TokenServerClient(new URI(TEST_REMOTE_URL), Executors.newSingleThreadExecutor());
  }

  public void testRemoteSuccess() throws Exception {
    final String assertion = mockMyIDTokenFactory.createMockMyIDAssertion(keyPair, TEST_USERNAME, TEST_REMOTE_AUDIENCE);
    JSONWebTokenUtils.dumpAssertion(assertion);

    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        client.getTokenFromBrowserIDAssertion(assertion, true, new TokenServerClientDelegate() {
          @Override
          public void handleSuccess(TokenServerToken token) {
            try {
              assertNotNull(token.id);
              assertNotNull(token.key);
              assertNotNull(Long.valueOf(token.uid));
              assertEquals(TEST_ENDPOINT + token.uid, token.endpoint);
              WaitHelper.getTestWaiter().performNotify();
            } catch (Throwable t) {
              WaitHelper.getTestWaiter().performNotify(t);
            }
          }

          @Override
          public void handleFailure(TokenServerException e) {
            WaitHelper.getTestWaiter().performNotify(e);
          }

          @Override
          public void handleError(Exception e) {
            WaitHelper.getTestWaiter().performNotify(e);
          }
        });
      }
    });
  }

  public void testRemoteFailure() throws Exception {
    final String badAssertion = mockMyIDTokenFactory.createMockMyIDAssertion(keyPair, TEST_USERNAME, TEST_REMOTE_AUDIENCE,
        0, 1,
        System.currentTimeMillis(), JSONWebTokenUtils.DEFAULT_ASSERTION_DURATION_IN_MILLISECONDS);
    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        client.getTokenFromBrowserIDAssertion(badAssertion, false, new TokenServerClientDelegate() {
          @Override
          public void handleSuccess(TokenServerToken token) {
            WaitHelper.getTestWaiter().performNotify(new RuntimeException("Expected failure due to expired assertion."));
          }

          @Override
          public void handleFailure(TokenServerException e) {
            try {
              assertEquals(TokenServerInvalidCredentialsException.class, e.getClass());
              WaitHelper.getTestWaiter().performNotify();
            } catch (Throwable t) {
              WaitHelper.getTestWaiter().performNotify(t);
            }
          }

          @Override
          public void handleError(Exception e) {
            WaitHelper.getTestWaiter().performNotify(e);
          }
        });
      }
    });
  }
}
