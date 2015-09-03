/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.net.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.IOException;
import java.io.PrintStream;
import java.net.URISyntaxException;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mozilla.android.sync.test.helpers.HTTPServerTestHelper;
import org.mozilla.android.sync.test.helpers.MockServer;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.setup.auth.AuthenticateAccountStage;
import org.mozilla.gecko.sync.setup.auth.AuthenticateAccountStage.AuthenticateAccountStageDelegate;
import org.simpleframework.http.Request;
import org.simpleframework.http.Response;

import ch.boye.httpclientandroidlib.HttpResponse;

/**
 * Tests the authentication request stage of manual Account setup.
 * @author liuche
 *
 */
public class TestAccountAuthenticatorStage {
  private static final int TEST_PORT      = HTTPServerTestHelper.getTestPort();
  private static final String TEST_SERVER = "http://localhost:" + TEST_PORT;

  private static final String USERNAME  = "john-hashed";
  private static final String PASSWORD  = "password";

  private MockServer authServer;
  private HTTPServerTestHelper serverHelper = new HTTPServerTestHelper();
  private AuthenticateAccountStage authStage = new AuthenticateAccountStage();
  private AuthenticateAccountStageDelegate testCallback;

  @Before
  public void setup() {
    // Make mock server to check authentication header.
    authServer = new MockServer() {
      @Override
      protected void handle(Request request, Response response, int code, String body) {
        try {
          final int c;
          String responseAuth = request.getValue("Authorization");
          // Trim whitespace, HttpResponse has an extra space?
          if (expectedBasicAuthHeader.equals(responseAuth.trim())) {
            c = 200;
          } else {
            c = 401;
          }

          Logger.debug(LOG_TAG, "Handling request...");
          PrintStream bodyStream = this.handleBasicHeaders(request, response, c, "application/json");
          bodyStream.println(body);
          bodyStream.close();
        } catch (IOException e) {
          Logger.error(LOG_TAG, "Oops.", e);
        }
      }
    };
    authServer.expectedBasicAuthHeader = authStage.makeAuthHeader(USERNAME, PASSWORD);

    // Authentication delegate to handle HTTP responses.
    testCallback = new AuthenticateAccountStageDelegate() {
      protected int numFailedTries = 0;

      @Override
      public void handleSuccess(boolean isSuccess) {
        if (isSuccess) {
          // Succeed on retry (after failing first attempt).
          assertEquals(1, numFailedTries);
          testWaiter().performNotify();
        } else {
          numFailedTries++;
          // Fail only once.
          if (numFailedTries != 1) {
            testWaiter().performNotify(new Exception("Failed on retry."));
            return;
          }
          String authHeader = authStage.makeAuthHeader(USERNAME, PASSWORD);
          try {
            authStage.authenticateAccount(testCallback, TEST_SERVER, authHeader);
          } catch (URISyntaxException e) {
            fail("Malformed URI.");
          }
        }
      }

      @Override
      public void handleFailure(HttpResponse response) {
        fail("Unexpected response " + response.getStatusLine().getStatusCode());
      }

      @Override
      public void handleError(Exception e) {
        fail("Unexpected error during authentication.");
      }
    };
    assertTrue(testWaiter().isIdle());

  }

  @After
  public void cleanup() {
    serverHelper.stopHTTPServer();
    assertTrue(testWaiter().isIdle());
  }

  @Test
  public void testAuthenticationRetry() {
    serverHelper.startHTTPServer(authServer);
    testWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        // Try auth request with incorrect password. We want to fail the first time.
        String authHeader = authStage.makeAuthHeader(USERNAME, "wrong-password");
        try {
          authStage.authenticateAccount(testCallback, TEST_SERVER, authHeader);
        } catch (URISyntaxException e) {
          fail("Malformed URI.");
        }
      }
    });
  }

  protected static WaitHelper testWaiter() {
    return WaitHelper.getTestWaiter();
  }
}
