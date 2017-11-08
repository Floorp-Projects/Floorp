/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.net.test;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.android.sync.test.helpers.HTTPServerTestHelper;
import org.mozilla.android.sync.test.helpers.MockResourceDelegate;
import org.mozilla.android.sync.test.helpers.MockServer;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.HttpResponseObserver;

import java.net.URISyntaxException;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

@RunWith(TestRunner.class)
public class TestResource {
  private static final int    TEST_PORT   = HTTPServerTestHelper.getTestPort();
  private static final String TEST_SERVER = "http://localhost:" + TEST_PORT;

  private HTTPServerTestHelper data     = new HTTPServerTestHelper();

  @SuppressWarnings("static-method")
  @Before
  public void setUp() {
    BaseResource.rewriteLocalhost = false;
  }

  @SuppressWarnings("static-method")
  @Test
  public void testLocalhostRewriting() throws URISyntaxException {
    BaseResource r = new BaseResource("http://localhost:5000/foo/bar", true);
    assertEquals("http://10.0.2.2:5000/foo/bar", r.getURI().toASCIIString());
  }

  @SuppressWarnings("static-method")
  public MockResourceDelegate doGet() throws URISyntaxException {
    final BaseResource r = new BaseResource(TEST_SERVER + "/foo/bar");
    MockResourceDelegate delegate = new MockResourceDelegate();
    r.delegate = delegate;
    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        r.get();
      }
    });
    return delegate;
  }

  @Test
  public void testTrivialFetch() throws URISyntaxException {
    MockServer server = data.startHTTPServer();
    server.expectedBasicAuthHeader = MockResourceDelegate.EXPECT_BASIC;
    MockResourceDelegate delegate = doGet();
    assertTrue(delegate.handledHttpResponse);
    data.stopHTTPServer();
  }

  public static class MockHttpResponseObserver implements HttpResponseObserver {
    public HttpResponse response = null;

    @Override
    public void observeHttpResponse(HttpUriRequest request, HttpResponse response) {
      this.response = response;
    }
  }

  @Test
  public void testObservers() throws URISyntaxException {
    data.startHTTPServer();
    // Check that null observer doesn't fail.
    BaseResource.addHttpResponseObserver(null);
    doGet(); // HTTP server stopped in callback.

    // Check that multiple non-null observers gets called with reasonable HttpResponse.
    MockHttpResponseObserver observers[] = { new MockHttpResponseObserver(), new MockHttpResponseObserver() };
    for (MockHttpResponseObserver observer : observers) {
      BaseResource.addHttpResponseObserver(observer);
      assertTrue(BaseResource.isHttpResponseObserver(observer));
      assertNull(observer.response);
    }

    doGet(); // HTTP server stopped in callback.

    for (MockHttpResponseObserver observer : observers) {
      assertNotNull(observer.response);
      assertEquals(200, observer.response.getStatusLine().getStatusCode());
    }

    data.stopHTTPServer();
  }
}
