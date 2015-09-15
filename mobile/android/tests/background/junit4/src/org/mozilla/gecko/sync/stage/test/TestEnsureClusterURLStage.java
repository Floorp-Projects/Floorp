/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.stage.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;

import org.json.simple.parser.ParseException;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.android.sync.test.helpers.HTTPServerTestHelper;
import org.mozilla.android.sync.test.helpers.MockGlobalSessionCallback;
import org.mozilla.android.sync.test.helpers.MockServer;
import org.mozilla.gecko.background.testhelpers.MockGlobalSession;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.AlreadySyncingException;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.NodeAuthenticationException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SyncConfigurationException;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.stage.EnsureClusterURLStage;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.message.BasicHttpResponse;
import ch.boye.httpclientandroidlib.message.BasicStatusLine;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestEnsureClusterURLStage {
  private static final int     TEST_PORT        = HTTPServerTestHelper.getTestPort();
  private static final String  TEST_SERVER      = "http://localhost:" + TEST_PORT + "/";
  private static final String  TEST_OLD_CLUSTER_URL = TEST_SERVER + "cluster/old/";
  private static final String  TEST_NEW_CLUSTER_URL = TEST_SERVER + "cluster/new/";
  static String                TEST_NW_URL      = TEST_SERVER + "/1.0/c6o7dvmr2c4ud2fyv6woz2u4zi22bcyd/node/weave"; // GET https://server/pathname/version/username/node/weave
  private HTTPServerTestHelper data             = new HTTPServerTestHelper();

  private final String TEST_USERNAME            = "johndoe";
  private final String TEST_PASSWORD            = "password";
  private final String TEST_SYNC_KEY            = "abcdeabcdeabcdeabcdeabcdea";

  @SuppressWarnings("static-method")
  @Before
  public void setUp() {
    // BaseResource rewrites localhost to 10.0.2.2, which works on Android but not on desktop.
    BaseResource.rewriteLocalhost = false;
  }

  public class MockClusterURLFetchDelegate implements EnsureClusterURLStage.ClusterURLFetchDelegate {
    public boolean      successCalled   = false;
    public URI          successUrl      = null;
    public boolean      throttleCalled  = false;
    public boolean      failureCalled   = false;
    public HttpResponse failureResponse = null;
    public boolean      errorCalled     = false;
    public Exception    errorException  = null;

    @Override
    public void handleSuccess(URI url) {
      successCalled = true;
      successUrl = url;
      WaitHelper.getTestWaiter().performNotify();
    }

    @Override
    public void handleThrottled() {
      throttleCalled = true;
      WaitHelper.getTestWaiter().performNotify();
    }

    @Override
    public void handleFailure(HttpResponse response) {
      failureCalled = true;
      failureResponse = response;
      WaitHelper.getTestWaiter().performNotify();
    }

    @Override
    public void handleError(Exception e) {
      errorCalled = true;
      errorException = e;
      WaitHelper.getTestWaiter().performNotify(e);
    }
  }

  public MockClusterURLFetchDelegate doFetchClusterURL() {
    final MockClusterURLFetchDelegate delegate = new MockClusterURLFetchDelegate();
    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        try {
          EnsureClusterURLStage.fetchClusterURL(TEST_NW_URL, delegate);
        } catch (URISyntaxException e) {
          WaitHelper.getTestWaiter().performNotify(e);
        }
      }
    });
    return delegate;
  }

  @Test
  public void testFetchClusterURLGood() {
    data.startHTTPServer(new MockServer(200, TEST_NEW_CLUSTER_URL));
    MockClusterURLFetchDelegate delegate = doFetchClusterURL();
    data.stopHTTPServer();

    assertTrue(delegate.successCalled);
    assertEquals(TEST_NEW_CLUSTER_URL, delegate.successUrl.toASCIIString());
  }

  @Test
  public void testFetchClusterURLThrottled() {
    data.startHTTPServer(new MockServer(200, "null"));
    MockClusterURLFetchDelegate delegate = doFetchClusterURL();
    data.stopHTTPServer();

    assertTrue(delegate.throttleCalled);
  }

  @Test
  public void testFetchClusterURLFailure() {
    data.startHTTPServer(new MockServer(404, ""));
    MockClusterURLFetchDelegate delegate = doFetchClusterURL();
    data.stopHTTPServer();

    assertTrue(delegate.failureCalled);
    assertEquals(404, delegate.failureResponse.getStatusLine().getStatusCode());
  }

  @Test
  /**
   * Test that we fetch a node/weave cluster URL if there isn't a cluster URL set.
   */
  public void testNodeAssignedIfNotSet() throws SyncConfigurationException, IllegalArgumentException, NonObjectJSONException, IOException, ParseException, CryptoException {
    final MockGlobalSessionCallback callback = new MockGlobalSessionCallback(TEST_SERVER);
    final GlobalSession session = new MockGlobalSession(TEST_USERNAME, TEST_PASSWORD,
        new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY), callback)
    .withStage(Stage.ensureClusterURL, new EnsureClusterURLStage(callback));

    assertEquals(null, session.config.getClusterURL());

    data.startHTTPServer(new MockServer(200, TEST_NEW_CLUSTER_URL));
    WaitHelper.getTestWaiter().performWait(WaitHelper.onThreadRunnable(new Runnable() {
      public void run() {
        try {
          session.start();
        } catch (AlreadySyncingException e) {
          WaitHelper.getTestWaiter().performNotify(e);
        }
      }
    }));
    data.stopHTTPServer();

    assertEquals(true,  callback.calledSuccess);
    assertEquals(false, callback.calledError);
    assertEquals(false, callback.calledAborted);
    assertEquals(false, callback.calledRequestBackoff);

    assertEquals(false, callback.calledInformUnauthorizedResponse);
    assertEquals(true,  callback.calledInformNodeAssigned);
    assertEquals(false, callback.calledInformNodeAuthenticationFailed);

    assertSame(null, callback.calledInformNodeAssignedOldClusterURL);
    assertEquals(TEST_NEW_CLUSTER_URL, callback.calledInformNodeAssignedNewClusterURL.toASCIIString());
  }

  /**
   * Test that, when there is no override, we don't fetch a node/weave cluster URL if there is a cluster URL set.
   */
  @Test
  public void testNodeNotAssignedIfSet() throws SyncConfigurationException, IllegalArgumentException, NonObjectJSONException, IOException, ParseException, CryptoException, URISyntaxException {
    final MockGlobalSessionCallback callback = new MockGlobalSessionCallback(TEST_SERVER);
    final GlobalSession session = new MockGlobalSession(TEST_USERNAME, TEST_PASSWORD,
        new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY), callback)
    .withStage(Stage.ensureClusterURL, new EnsureClusterURLStage(callback));

    session.config.setClusterURL(new URI(TEST_OLD_CLUSTER_URL));
    assertEquals(TEST_OLD_CLUSTER_URL, session.config.getClusterURLString());

    data.startHTTPServer(new MockServer(200, TEST_NEW_CLUSTER_URL));
    WaitHelper.getTestWaiter().performWait(WaitHelper.onThreadRunnable(new Runnable() {
      public void run() {
        try {
          session.start();
        } catch (AlreadySyncingException e) {
          WaitHelper.getTestWaiter().performNotify(e);
        }
      }
    }));
    data.stopHTTPServer();

    assertEquals(true,  callback.calledSuccess);
    assertEquals(false, callback.calledError);
    assertEquals(false, callback.calledAborted);
    assertEquals(false, callback.calledRequestBackoff);

    assertEquals(false, callback.calledInformUnauthorizedResponse);
    assertEquals(false, callback.calledInformNodeAssigned);
    assertEquals(false, callback.calledInformNodeAuthenticationFailed);
  }

  /**
   * Test that, when there is an override, we fetch a node/weave cluster URL and callback if it is new.
   */
  @Test
  public void testNodeAssignedCalledWhenOverridden() throws SyncConfigurationException, IllegalArgumentException, NonObjectJSONException, IOException, ParseException, CryptoException, URISyntaxException {
    final MockGlobalSessionCallback callback = new MockGlobalSessionCallback(TEST_SERVER) {
      @Override
      public boolean wantNodeAssignment() {
        return true;
      }
    };

    final GlobalSession session = new MockGlobalSession(TEST_USERNAME, TEST_PASSWORD,
        new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY), callback)
    .withStage(Stage.ensureClusterURL, new EnsureClusterURLStage(callback));

    session.config.setClusterURL(new URI(TEST_OLD_CLUSTER_URL));
    assertEquals(TEST_OLD_CLUSTER_URL, session.config.getClusterURLString());

    data.startHTTPServer(new MockServer(200, TEST_NEW_CLUSTER_URL));
    WaitHelper.getTestWaiter().performWait(WaitHelper.onThreadRunnable(new Runnable() {
      public void run() {
        try {
          session.start();
        } catch (AlreadySyncingException e) {
          WaitHelper.getTestWaiter().performNotify(e);
        }
      }
    }));
    data.stopHTTPServer();

    assertEquals(true,  callback.calledSuccess);
    assertEquals(false, callback.calledError);
    assertEquals(false, callback.calledAborted);
    assertEquals(false, callback.calledRequestBackoff);

    assertEquals(false, callback.calledInformUnauthorizedResponse);
    assertEquals(true,  callback.calledInformNodeAssigned);
    assertEquals(false, callback.calledInformNodeAuthenticationFailed);

    assertEquals(TEST_OLD_CLUSTER_URL, callback.calledInformNodeAssignedOldClusterURL.toASCIIString());
    assertEquals(TEST_NEW_CLUSTER_URL, callback.calledInformNodeAssignedNewClusterURL.toASCIIString());
  }

  /**
   * Test that, when there is an override, we fetch a node/weave cluster URL and callback correctly if it is not new.
   */
  @Test
  public void testNodeFailedCalledWhenOverridden() throws SyncConfigurationException, IllegalArgumentException, NonObjectJSONException, IOException, ParseException, CryptoException, URISyntaxException {
    final MockGlobalSessionCallback callback = new MockGlobalSessionCallback(TEST_SERVER) {
      @Override
      public boolean wantNodeAssignment() {
        return true;
      }
    };

    final GlobalSession session = new MockGlobalSession(TEST_USERNAME, TEST_PASSWORD,
        new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY), callback)
    .withStage(Stage.ensureClusterURL, new EnsureClusterURLStage(callback));

    session.config.setClusterURL(new URI(TEST_OLD_CLUSTER_URL));
    assertEquals(TEST_OLD_CLUSTER_URL, session.config.getClusterURLString());

    data.startHTTPServer(new MockServer(200, TEST_OLD_CLUSTER_URL));
    WaitHelper.getTestWaiter().performWait(WaitHelper.onThreadRunnable(new Runnable() {
      public void run() {
        try {
          session.start();
        } catch (AlreadySyncingException e) {
          WaitHelper.getTestWaiter().performNotify(e);
        }
      }
    }));
    data.stopHTTPServer();

    assertEquals(false, callback.calledSuccess);
    assertEquals(true,  callback.calledError);
    assertEquals(false, callback.calledAborted);
    assertEquals(false, callback.calledRequestBackoff);
    assertTrue(callback.calledErrorException instanceof NodeAuthenticationException);

    assertEquals(false, callback.calledInformUnauthorizedResponse);
    assertEquals(false, callback.calledInformNodeAssigned);
    assertEquals(true,  callback.calledInformNodeAuthenticationFailed);
    assertEquals(TEST_OLD_CLUSTER_URL, callback.calledInformNodeAuthenticationFailedClusterURL.toASCIIString());
  }

  @Test
  public void testInterpretHTTPFailureCallsMaybeNodeReassigned() throws SyncConfigurationException, IllegalArgumentException, NonObjectJSONException, IOException, ParseException, CryptoException {
    final MockGlobalSessionCallback callback = new MockGlobalSessionCallback(TEST_SERVER);
    final GlobalSession session = new MockGlobalSession(TEST_USERNAME, TEST_PASSWORD,
        new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY), callback);

    final HttpResponse response = new BasicHttpResponse(
      new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), 401, "User authentication failed"));

    session.interpretHTTPFailure(response);                        // This is synchronous...
    assertEquals(true, callback.calledInformUnauthorizedResponse); // ... so we can test immediately.
  }
}
