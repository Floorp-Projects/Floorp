/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test.helpers;

import static org.junit.Assert.fail;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.SocketAddress;
import java.util.IdentityHashMap;
import java.util.Map;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;
import org.simpleframework.transport.connect.Connection;
import org.simpleframework.transport.connect.SocketConnection;

/**
 * Test helper code to bind <code>MockServer</code> instances to ports.
 * <p>
 * Maintains a collection of running servers and (by default) throws helpful
 * errors if two servers are started "on top" of each other. The
 * <b>unchecked</b> exception thrown contains a stack trace pointing to where
 * the new server is being created and where the pre-existing server was
 * created.
 * <p>
 * Parses a system property to determine current test port, which is fixed for
 * the duration of a test execution.
 */
public class HTTPServerTestHelper {
  private static final String LOG_TAG = "HTTPServerTestHelper";

  /**
   * Port to run HTTP servers on during this test execution.
   * <p>
   * Lazily initialized on first call to {@link #getTestPort}.
   */
  public static Integer testPort = null;

  public static final String LOCAL_HTTP_PORT_PROPERTY = "android.sync.local.http.port";
  public static final int    LOCAL_HTTP_PORT_DEFAULT = 15125;

  public final int port;

  public Connection connection;
  public MockServer server;

  /**
   * Create a helper to bind <code>MockServer</code> instances.
   * <p>
   * Use {@link #getTestPort} to determine the port this helper will bind to.
   */
  public HTTPServerTestHelper() {
    this.port = getTestPort();
  }

  // For testing only.
  protected HTTPServerTestHelper(int port) {
    this.port = port;
  }

  /**
   * Lazily initialize test port for this test execution.
   * <p>
   * Only called from {@link #getTestPort}.
   * <p>
   * If the test port has not been determined, we try to parse it from a system
   * property; if that fails, we return the default test port.
   */
  protected synchronized static void ensureTestPort() {
    if (testPort != null) {
      return;
    }

    String value = System.getProperty(LOCAL_HTTP_PORT_PROPERTY);
    if (value != null) {
      try {
        testPort = Integer.valueOf(value);
      } catch (NumberFormatException e) {
        Logger.warn(LOG_TAG, "Got exception parsing local test port; ignoring. ", e);
      }
    }

    if (testPort == null) {
      testPort = Integer.valueOf(LOCAL_HTTP_PORT_DEFAULT);
    }
  }

  /**
   * The port to which all HTTP servers will be found for the duration of this
   * test execution.
   * <p>
   * We try to parse the port from a system property; if that fails, we return
   * the default test port.
   *
   * @return port number.
   */
  public synchronized static int getTestPort() {
    if (testPort == null) {
      ensureTestPort();
    }

    return testPort.intValue();
  }

  /**
   * Used to maintain a stack trace pointing to where a server was started.
   */
  public static class HTTPServerStartedError extends Error {
    private static final long serialVersionUID = -6778447718799087274L;

    public final HTTPServerTestHelper httpServer;

    public HTTPServerStartedError(HTTPServerTestHelper httpServer) {
      this.httpServer = httpServer;
    }
  }

  /**
   * Thrown when a server is started "on top" of another server. The cause error
   * will be an <code>HTTPServerStartedError</code> with a stack trace pointing
   * to where the pre-existing server was started.
   */
  public static class HTTPServerAlreadyRunningError extends Error {
    private static final long serialVersionUID = -6778447718799087275L;

    public HTTPServerAlreadyRunningError(Throwable e) {
      super(e);
    }
  }

  /**
   * Maintain a hash of running servers. Each value is an error with a stack
   * traces pointing to where that server was started.
   * <p>
   * We don't key on the server itself because each server is a <it>helper</it>
   * that may be started many times with different <code>MockServer</code>
   * instances.
   * <p>
   * Synchronize access on the class.
   */
  protected static Map<Connection, HTTPServerStartedError> runningServers =
      new IdentityHashMap<Connection, HTTPServerStartedError>();

  protected synchronized static void throwIfServerAlreadyRunning() {
    for (HTTPServerStartedError value : runningServers.values()) {
      throw new HTTPServerAlreadyRunningError(value);
    }
  }

  protected synchronized static void registerServerAsRunning(HTTPServerTestHelper httpServer) {
    if (httpServer == null || httpServer.connection == null) {
      throw new IllegalArgumentException("HTTPServerTestHelper or connection was null; perhaps server has not been started?");
    }

    HTTPServerStartedError old = runningServers.put(httpServer.connection, new HTTPServerStartedError(httpServer));
    if (old != null) {
      // Should never happen.
      throw old;
    }
  }

  protected synchronized static void unregisterServerAsRunning(HTTPServerTestHelper httpServer) {
    if (httpServer == null || httpServer.connection == null) {
      throw new IllegalArgumentException("HTTPServerTestHelper or connection was null; perhaps server has not been started?");
    }

    runningServers.remove(httpServer.connection);
  }

  public MockServer startHTTPServer(MockServer server, boolean allowMultipleServers) {
    BaseResource.rewriteLocalhost = false; // No sense rewriting when we're running the unit tests.
    BaseResourceDelegate.connectionTimeoutInMillis = 1000; // No sense waiting a long time for a local connection.

    if (!allowMultipleServers) {
      throwIfServerAlreadyRunning();
    }

    try {
      this.server = server;
      connection = new SocketConnection(server);
      SocketAddress address = new InetSocketAddress(port);
      connection.connect(address);

      registerServerAsRunning(this);

      Logger.info(LOG_TAG, "Started HTTP server on port " + port + ".");
    } catch (IOException ex) {
      Logger.error(LOG_TAG, "Error starting HTTP server on port " + port + ".", ex);
      fail(ex.toString());
    }

    return server;
  }

  public MockServer startHTTPServer(MockServer server) {
    return startHTTPServer(server, false);
  }

  public MockServer startHTTPServer() {
    return startHTTPServer(new MockServer());
  }

  public void stopHTTPServer() {
    try {
      if (connection != null) {
        unregisterServerAsRunning(this);

        connection.close();
      }
      server = null;
      connection = null;

      Logger.info(LOG_TAG, "Stopped HTTP server on port " + port + ".");

      Logger.debug(LOG_TAG, "Closing connection pool...");
      BaseResource.shutdownConnectionManager();
    } catch (IOException ex) {
      Logger.error(LOG_TAG, "Error stopping HTTP server on port " + port + ".", ex);
      fail(ex.toString());
    }
  }
}
