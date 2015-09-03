/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test.helpers.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.PrintWriter;
import java.io.StringWriter;

import org.junit.Test;
import org.mozilla.android.sync.test.helpers.HTTPServerTestHelper;
import org.mozilla.android.sync.test.helpers.MockServer;
import org.mozilla.android.sync.test.helpers.HTTPServerTestHelper.HTTPServerAlreadyRunningError;

public class TestHTTPServerTestHelper {
  public static final int TEST_PORT = HTTPServerTestHelper.getTestPort();

  protected MockServer mockServer = new MockServer();

  @Test
  public void testStartStop() {
    // Need to be able to start and stop multiple times.
    for (int i = 0; i < 2; i++) {
      HTTPServerTestHelper httpServer = new HTTPServerTestHelper();

      assertNull(httpServer.connection);
      httpServer.startHTTPServer(mockServer);

      assertNotNull(httpServer.connection);
      httpServer.stopHTTPServer();
    }
  }

  public void startAgain() {
    HTTPServerTestHelper httpServer = new HTTPServerTestHelper();
    httpServer.startHTTPServer(mockServer);
  }

  @Test
  public void testStartTwice() {
    HTTPServerTestHelper httpServer = new HTTPServerTestHelper();

    httpServer.startHTTPServer(mockServer);
    assertNotNull(httpServer.connection);

    // Should not be able to start multiple times.
    try {
      try {
        startAgain();

        fail("Expected exception.");
      } catch (Throwable e) {
        assertEquals(HTTPServerAlreadyRunningError.class, e.getClass());

        StringWriter sw = new StringWriter();
        e.printStackTrace(new PrintWriter(sw));
        String s = sw.toString();

        // Ensure we get a useful stack trace.
        // We should have the method trying to start the server the second time...
        assertTrue(s.contains("startAgain"));
        // ... as well as the the method that started the server the first time.
        assertTrue(s.contains("testStartTwice"));
      }
    } finally {
      httpServer.stopHTTPServer();
    }
  }

  protected static class LeakyHTTPServerTestHelper extends HTTPServerTestHelper {
    // Make this constructor public, just for this test.
    public LeakyHTTPServerTestHelper(int port) {
      super(port);
    }
  }

  @Test
  public void testForceStartTwice() {
    HTTPServerTestHelper httpServer1 = new HTTPServerTestHelper();
    HTTPServerTestHelper httpServer2 = new LeakyHTTPServerTestHelper(httpServer1.port + 1);

    // Should be able to start multiple times if we specify it.
    try {
      httpServer1.startHTTPServer(mockServer);
      assertNotNull(httpServer1.connection);

      httpServer2.startHTTPServer(mockServer, true);
      assertNotNull(httpServer2.connection);
    } finally {
      httpServer1.stopHTTPServer();
      httpServer2.stopHTTPServer();
    }
  }
}
