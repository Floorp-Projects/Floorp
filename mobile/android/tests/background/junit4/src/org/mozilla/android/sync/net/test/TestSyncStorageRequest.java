/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.net.test;

import org.json.simple.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.android.sync.test.helpers.BaseTestStorageRequestDelegate;
import org.mozilla.android.sync.test.helpers.HTTPServerTestHelper;
import org.mozilla.android.sync.test.helpers.MockServer;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.simpleframework.http.Request;
import org.simpleframework.http.Response;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

@RunWith(TestRunner.class)
public class TestSyncStorageRequest {
  private static final int    TEST_PORT   = HTTPServerTestHelper.getTestPort();
  private static final String TEST_SERVER = "http://localhost:" + TEST_PORT;

  private static final String LOCAL_META_URL  = TEST_SERVER + "/1.1/c6o7dvmr2c4ud2fyv6woz2u4zi22bcyd/storage/meta/global";
  private static final String LOCAL_BAD_REQUEST_URL  = TEST_SERVER + "/1.1/c6o7dvmr2c4ud2fyv6woz2u4zi22bcyd/storage/bad";

  private static final String EXPECTED_ERROR_CODE = "12";
  private static final String EXPECTED_RETRY_AFTER_ERROR_MESSAGE = "{error:'informative error message'}";

  // Corresponds to rnewman+testandroid@mozilla.com.
  private static final String TEST_USERNAME    = "c6o7dvmr2c4ud2fyv6woz2u4zi22bcyd";
  private static final String TEST_PASSWORD    = "password";
  private final AuthHeaderProvider authHeaderProvider = new BasicAuthHeaderProvider(TEST_USERNAME, TEST_PASSWORD);

  private HTTPServerTestHelper data = new HTTPServerTestHelper();

  public class TestSyncStorageRequestDelegate extends
      BaseTestStorageRequestDelegate {
    public TestSyncStorageRequestDelegate(AuthHeaderProvider authHeaderProvider) {
      super(authHeaderProvider);
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse res) {
      assertTrue(res.wasSuccessful());
      assertTrue(res.httpResponse().containsHeader("X-Weave-Timestamp"));

      // Make sure we consume the rest of the body, so we can reuse the
      // connection. Even test code has to be correct in this regard!
      try {
        System.out.println("Success body: " + res.body());
      } catch (Exception e) {
        e.printStackTrace();
      }
      BaseResource.consumeEntity(res);
      data.stopHTTPServer();
    }
  }

  public class TestBadSyncStorageRequestDelegate extends
      BaseTestStorageRequestDelegate {

    public TestBadSyncStorageRequestDelegate(AuthHeaderProvider authHeaderProvider) {
      super(authHeaderProvider);
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse res) {
      assertTrue(!res.wasSuccessful());
      assertTrue(res.httpResponse().containsHeader("X-Weave-Timestamp"));
      try {
        String responseMessage = res.getErrorMessage();
        String expectedMessage = SyncStorageResponse.SERVER_ERROR_MESSAGES.get(EXPECTED_ERROR_CODE);
        assertEquals(expectedMessage, responseMessage);
      } catch (Exception e) {
        fail("Got exception fetching error message.");
      }
      BaseResource.consumeEntity(res);
      data.stopHTTPServer();
    }
  }


  @Test
  public void testSyncStorageRequest() throws URISyntaxException, IOException {
    BaseResource.rewriteLocalhost = false;
    data.startHTTPServer();
    SyncStorageRecordRequest r = new SyncStorageRecordRequest(new URI(LOCAL_META_URL));
    TestSyncStorageRequestDelegate delegate = new TestSyncStorageRequestDelegate(authHeaderProvider);
    r.delegate = delegate;
    r.get();
    // Server is stopped in the callback.
  }

  public class ErrorMockServer extends MockServer {
    @Override
    public void handle(Request request, Response response) {
      super.handle(request, response, 400, EXPECTED_ERROR_CODE);
    }
  }

  @Test
  public void testErrorResponse() throws URISyntaxException {
    BaseResource.rewriteLocalhost = false;
    data.startHTTPServer(new ErrorMockServer());
    SyncStorageRecordRequest r = new SyncStorageRecordRequest(new URI(LOCAL_BAD_REQUEST_URL));
    TestBadSyncStorageRequestDelegate delegate = new TestBadSyncStorageRequestDelegate(authHeaderProvider);
    r.delegate = delegate;
    r.post(new JSONObject());
    // Server is stopped in the callback.
  }

  // Test that the Retry-After header is correctly parsed and that handleRequestFailure
  // is being called.
  public class TestRetryAfterSyncStorageRequestDelegate extends BaseTestStorageRequestDelegate {

    public TestRetryAfterSyncStorageRequestDelegate(AuthHeaderProvider authHeaderProvider) {
      super(authHeaderProvider);
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse res) {
      assertTrue(!res.wasSuccessful());
      assertTrue(res.httpResponse().containsHeader("Retry-After"));
      assertEquals(res.retryAfterInSeconds(), 3001);
      try {
        String responseMessage = res.getErrorMessage();
        String expectedMessage = EXPECTED_RETRY_AFTER_ERROR_MESSAGE;
        assertEquals(expectedMessage, responseMessage);
      } catch (Exception e) {
        fail("Got exception fetching error message.");
      }
      BaseResource.consumeEntity(res);
      data.stopHTTPServer();
    }
  }

  public class RetryAfterMockServer extends MockServer {
    @Override
    public void handle(Request request, Response response) {
      String errorBody = EXPECTED_RETRY_AFTER_ERROR_MESSAGE;
      response.setValue("Retry-After", "3001");
      super.handle(request, response, 503, errorBody);
    }
  }

  @Test
  public void testRetryAfterResponse() throws URISyntaxException {
    BaseResource.rewriteLocalhost = false;
    data.startHTTPServer(new RetryAfterMockServer());
    SyncStorageRecordRequest r = new SyncStorageRecordRequest(new URI(LOCAL_BAD_REQUEST_URL)); // URL not used -- we 503 every response
    TestRetryAfterSyncStorageRequestDelegate delegate = new TestRetryAfterSyncStorageRequestDelegate(authHeaderProvider);
    r.delegate = delegate;
    r.post(new JSONObject());
    // Server is stopped in the callback.
  }
  
  // Test that the X-Weave-Backoff header is correctly parsed and that handleRequestSuccess
  // is still being called.
  public class TestWeaveBackoffSyncStorageRequestDelegate extends
  TestSyncStorageRequestDelegate {

    public TestWeaveBackoffSyncStorageRequestDelegate(AuthHeaderProvider authHeaderProvider) {
      super(authHeaderProvider);
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse res) {
      assertTrue(res.httpResponse().containsHeader("X-Weave-Backoff"));
      assertEquals(res.weaveBackoffInSeconds(), 1801);
      super.handleRequestSuccess(res);
    }
  }
  
  public class WeaveBackoffMockServer extends MockServer {
    @Override
    public void handle(Request request, Response response) {
      response.setValue("X-Weave-Backoff", "1801");
      super.handle(request, response);
    }
  }

  @Test
  public void testWeaveBackoffResponse() throws URISyntaxException {
    BaseResource.rewriteLocalhost = false;
    data.startHTTPServer(new WeaveBackoffMockServer());
    SyncStorageRecordRequest r = new SyncStorageRecordRequest(new URI(LOCAL_META_URL)); // URL re-used -- we need any successful response
    TestWeaveBackoffSyncStorageRequestDelegate delegate = new TestWeaveBackoffSyncStorageRequestDelegate(new BasicAuthHeaderProvider(TEST_USERNAME, TEST_PASSWORD));
    r.delegate = delegate;
    r.post(new JSONObject());
    // Server is stopped in the callback.
  }

  // Test that the X-Weave-{Quota-Remaining, Alert, Records} headers are correctly parsed and
  // that handleRequestSuccess is still being called.
  public class TestHeadersSyncStorageRequestDelegate extends
  TestSyncStorageRequestDelegate {

    public TestHeadersSyncStorageRequestDelegate(AuthHeaderProvider authHeaderProvider) {
      super(authHeaderProvider);
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse res) {
      assertTrue(res.httpResponse().containsHeader("X-Weave-Quota-Remaining"));
      assertTrue(res.httpResponse().containsHeader("X-Weave-Alert"));
      assertTrue(res.httpResponse().containsHeader("X-Weave-Records"));
      assertEquals(65536, res.weaveQuotaRemaining());
      assertEquals("First weave alert string", res.weaveAlert());
      assertEquals(50, res.weaveRecords());

      super.handleRequestSuccess(res);
    }
  }

  public class HeadersMockServer extends MockServer {
    @Override
    public void handle(Request request, Response response) {
      response.setValue("X-Weave-Quota-Remaining", "65536");
      response.setValue("X-Weave-Alert", "First weave alert string");
      response.addValue("X-Weave-Alert", "Second weave alert string");
      response.setValue("X-Weave-Records", "50");

      super.handle(request, response);
    }
  }

  @Test
  public void testHeadersResponse() throws URISyntaxException {
    BaseResource.rewriteLocalhost = false;
    data.startHTTPServer(new HeadersMockServer());
    SyncStorageRecordRequest r = new SyncStorageRecordRequest(new URI(LOCAL_META_URL)); // URL re-used -- we need any successful response
    TestHeadersSyncStorageRequestDelegate delegate = new TestHeadersSyncStorageRequestDelegate(authHeaderProvider);
    r.delegate = delegate;
    r.post(new JSONObject());
    // Server is stopped in the callback.
  }

  public class DeleteMockServer extends MockServer {
    @Override
    public void handle(Request request, Response response) {
      assertNotNull(request.getValue("x-confirm-delete"));
      assertEquals("1", request.getValue("x-confirm-delete"));
      super.handle(request, response);
    }
  }

  @Test
  public void testDelete() throws URISyntaxException {
    BaseResource.rewriteLocalhost = false;
    data.startHTTPServer(new DeleteMockServer());
    SyncStorageRecordRequest r = new SyncStorageRecordRequest(new URI(LOCAL_META_URL)); // URL re-used -- we need any successful response
    TestSyncStorageRequestDelegate delegate = new TestSyncStorageRequestDelegate(authHeaderProvider);
    r.delegate = delegate;
    r.delete();
    // Server is stopped in the callback.
  }
}
