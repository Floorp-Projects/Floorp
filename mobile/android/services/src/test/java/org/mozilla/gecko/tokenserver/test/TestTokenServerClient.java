/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.tokenserver.test;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.client.methods.HttpGet;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.entity.StringEntity;
import ch.boye.httpclientandroidlib.message.BasicHeader;
import ch.boye.httpclientandroidlib.message.BasicHttpResponse;
import ch.boye.httpclientandroidlib.message.BasicStatusLine;
import junit.framework.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.common.log.writers.StringLogWriter;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.tokenserver.TokenServerClient;
import org.mozilla.gecko.tokenserver.TokenServerClient.TokenFetchResourceDelegate;
import org.mozilla.gecko.tokenserver.TokenServerClientDelegate;
import org.mozilla.gecko.tokenserver.TokenServerException;
import org.mozilla.gecko.tokenserver.TokenServerException.TokenServerConditionsRequiredException;
import org.mozilla.gecko.tokenserver.TokenServerException.TokenServerInvalidCredentialsException;
import org.mozilla.gecko.tokenserver.TokenServerException.TokenServerMalformedRequestException;
import org.mozilla.gecko.tokenserver.TokenServerException.TokenServerMalformedResponseException;
import org.mozilla.gecko.tokenserver.TokenServerException.TokenServerUnknownServiceException;
import org.mozilla.gecko.tokenserver.TokenServerToken;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

@RunWith(TestRunner.class)
public class TestTokenServerClient {
  public static final String JSON = "application/json";
  public static final String TEXT = "text/plain";

  public static final String TEST_TOKEN_RESPONSE = "{\"api_endpoint\": \"https://stage-aitc1.services.mozilla.com/1.0/1659259\"," +
      "\"duration\": 300," +
      "\"id\": \"eySHORTENED\"," +
      "\"key\": \"-plSHORTENED\"," +
      "\"hashed_fxa_uid\": \"rAnD0MU1D\"," +
      "\"uid\": 1659259}";

  public static final String TEST_CONDITIONS_RESPONSE = "{\"errors\":[{" +
      "\"location\":\"header\"," +
      "\"description\":\"Need to accept conditions\"," +
      "\"condition_urls\":{\"tos\":\"http://url-to-tos.com\"}," +
      "\"name\":\"X-Conditions-Accepted\"}]," +
      "\"status\":\"error\"}";

  public static final String TEST_ERROR_RESPONSE = "{\"status\": \"error\"," +
      "\"errors\": [{\"location\": \"body\", \"name\": \"\", \"description\": \"Unauthorized EXTENDED\"}]}";

  public static final String TEST_INVALID_TIMESTAMP_RESPONSE = "{\"status\": \"invalid-timestamp\", " +
      "\"errors\": [{\"location\": \"body\", \"name\": \"\", \"description\": \"Unauthorized\"}]}";

  protected TokenServerClient client;

  @Before
  public void setUp() throws Exception {
    this.client = new TokenServerClient(new URI("http://unused.com"), Executors.newSingleThreadExecutor());
  }

  protected TokenServerToken doProcessResponse(int statusCode, String contentType, Object token)
      throws UnsupportedEncodingException, TokenServerException {
    final HttpResponse response = new BasicHttpResponse(
        new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), statusCode, "OK"));

    StringEntity stringEntity = new StringEntity(token.toString());
    stringEntity.setContentType(contentType);
    response.setEntity(stringEntity);

    return client.processResponse(new SyncResponse(response));
  }

  @SuppressWarnings("rawtypes")
  protected TokenServerException expectProcessResponseFailure(int statusCode, String contentType, Object token, Class klass)
      throws TokenServerException, UnsupportedEncodingException {
    try {
      doProcessResponse(statusCode, contentType, token.toString());
      fail("Expected exception of type " + klass + ".");

      return null;
    } catch (TokenServerException e) {
      assertEquals(klass, e.getClass());

      return e;
    }
  }

  @SuppressWarnings("rawtypes")
  protected TokenServerException expectProcessResponseFailure(Object token, Class klass)
      throws UnsupportedEncodingException, TokenServerException {
    return expectProcessResponseFailure(200, "application/json", token, klass);
  }

  @Test
  public void testProcessResponseSuccess() throws Exception {
    TokenServerToken token = doProcessResponse(200, "application/json", TEST_TOKEN_RESPONSE);

    assertEquals("eySHORTENED", token.id);
    assertEquals("-plSHORTENED", token.key);
    assertEquals("1659259", token.uid);
    assertEquals("https://stage-aitc1.services.mozilla.com/1.0/1659259", token.endpoint);
  }

  @Test
  public void testProcessResponseFailure() throws Exception {
    // Wrong Content-Type.
    expectProcessResponseFailure(200, TEXT, new ExtendedJSONObject(), TokenServerMalformedResponseException.class);

    // Not valid JSON.
    expectProcessResponseFailure("#!", TokenServerMalformedResponseException.class);

    // Status code 400.
    expectProcessResponseFailure(400, JSON, new ExtendedJSONObject(), TokenServerMalformedRequestException.class);

    // Status code 401.
    expectProcessResponseFailure(401, JSON, new ExtendedJSONObject(), TokenServerInvalidCredentialsException.class);
    expectProcessResponseFailure(401, JSON, TEST_INVALID_TIMESTAMP_RESPONSE, TokenServerInvalidCredentialsException.class);

    // Status code 404.
    expectProcessResponseFailure(404, JSON, new ExtendedJSONObject(), TokenServerUnknownServiceException.class);

    // Status code 406, which is not specially handled, but with errors. We take
    // care that errors are actually printed because we're going to want this to
    // work when things go wrong.
    StringLogWriter logWriter = new StringLogWriter();

    Logger.startLoggingTo(logWriter);
    try {
      expectProcessResponseFailure(406, JSON, TEST_ERROR_RESPONSE, TokenServerException.class);

      assertTrue(logWriter.toString().contains("Unauthorized EXTENDED"));
    } finally {
      Logger.stopLoggingTo(logWriter);
    }

    // Status code 503.
    expectProcessResponseFailure(503, JSON, new ExtendedJSONObject(), TokenServerException.class);
  }

  @Test
  public void testProcessResponseConditionsRequired() throws Exception {

    // Status code 403: conditions need to be accepted, but malformed (no urls).
    expectProcessResponseFailure(403, JSON, new ExtendedJSONObject(), TokenServerMalformedResponseException.class);

    // Status code 403, with urls.
    TokenServerConditionsRequiredException e = (TokenServerConditionsRequiredException)
        expectProcessResponseFailure(403, JSON, TEST_CONDITIONS_RESPONSE, TokenServerConditionsRequiredException.class);

    ExtendedJSONObject expectedUrls = new ExtendedJSONObject();
    expectedUrls.put("tos", "http://url-to-tos.com");
    assertEquals(expectedUrls.toString(), e.conditionUrls.toString());
  }

  @Test
  public void testProcessResponseMalformedToken() throws Exception {
    ExtendedJSONObject token;

    // Missing key.
    token = new ExtendedJSONObject(TEST_TOKEN_RESPONSE);
    token.remove("api_endpoint");
    expectProcessResponseFailure(token, TokenServerMalformedResponseException.class);

    // Key has wrong type; expected String.
    token = new ExtendedJSONObject(TEST_TOKEN_RESPONSE);
    token.put("api_endpoint", new ExtendedJSONObject());
    expectProcessResponseFailure(token, TokenServerMalformedResponseException.class);

    // Key has wrong type; expected number.
    token = new ExtendedJSONObject(TEST_TOKEN_RESPONSE);
    token.put("uid", "NON NUMERIC");
    expectProcessResponseFailure(token, TokenServerMalformedResponseException.class);
  }

  private class MockBaseResource extends BaseResource {
    public MockBaseResource(String uri) throws URISyntaxException {
      super(uri);
      this.request = new HttpGet(this.uri);
    }

    public HttpRequestBase prepareHeadersAndReturn() throws Exception {
      super.prepareClient();
      return request;
    }
  }

  @Test
  public void testClientStateHeader() throws Exception {
    String assertion = "assertion";
    String clientState = "abcdef";
    MockBaseResource resource = new MockBaseResource("http://unused.local/");

    TokenServerClientDelegate delegate = new TokenServerClientDelegate() {
      @Override
      public void handleSuccess(TokenServerToken token) {
      }

      @Override
      public void handleFailure(TokenServerException e) {
      }

      @Override
      public void handleError(Exception e) {
      }

      @Override
      public void handleBackoff(int backoffSeconds) {
      }

      @Override
      public String getUserAgent() {
        return null;
      }
    };

    resource.delegate = new TokenServerClient.TokenFetchResourceDelegate(client, resource, delegate, assertion, clientState , true) {
      @Override
      public AuthHeaderProvider getAuthHeaderProvider() {
        return null;
      }
    };

    HttpRequestBase request = resource.prepareHeadersAndReturn();
    Assert.assertEquals("abcdef", request.getFirstHeader("X-Client-State").getValue());
    Assert.assertEquals("1", request.getFirstHeader("X-Conditions-Accepted").getValue());
  }

  public static class MockTokenServerClient extends TokenServerClient {
    public MockTokenServerClient(URI uri, Executor executor) {
      super(uri, executor);
    }
  }

  public static final class MockTokenServerClientDelegate implements
      TokenServerClientDelegate {
    public volatile boolean backoffCalled;
    public volatile boolean succeeded;
    public volatile int backoff;

    @Override
    public String getUserAgent() {
      return null;
    }

    @Override
    public void handleSuccess(TokenServerToken token) {
      succeeded = true;
      WaitHelper.getTestWaiter().performNotify();
    }

    @Override
    public void handleFailure(TokenServerException e) {
      succeeded = false;
      WaitHelper.getTestWaiter().performNotify();
    }

    @Override
    public void handleError(Exception e) {
      succeeded = false;
      WaitHelper.getTestWaiter().performNotify(e);
    }

    @Override
    public void handleBackoff(int backoffSeconds) {
      backoffCalled = true;
      backoff = backoffSeconds;
    }
  }

  private void expectDelegateCalls(URI uri, MockTokenServerClient client, int code, Header header, String body, boolean succeeded, long backoff, boolean expectBackoff) throws UnsupportedEncodingException {
    final BaseResource resource = new BaseResource(uri);
    final String assertion = "someassertion";
    final String clientState = "abcdefabcdefabcdefabcdefabcdefab";
    final boolean conditionsAccepted = true;

    MockTokenServerClientDelegate delegate = new MockTokenServerClientDelegate();

    final TokenFetchResourceDelegate tokenFetchResourceDelegate = new TokenServerClient.TokenFetchResourceDelegate(client, resource, delegate, assertion, clientState, conditionsAccepted);

    final BasicStatusLine statusline = new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), code, "Whatever");
    final HttpResponse response = new BasicHttpResponse(statusline);
    response.setHeader(header);
    if (body != null) {
      final StringEntity entity = new StringEntity(body);
      entity.setContentType("application/json");
      response.setEntity(entity);
    }

    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        tokenFetchResourceDelegate.handleHttpResponse(response);
      }
    });

    assertEquals(expectBackoff, delegate.backoffCalled);
    assertEquals(backoff, delegate.backoff);
    assertEquals(succeeded, delegate.succeeded);
  }

  @Test
  public void testBackoffHandling() throws URISyntaxException, UnsupportedEncodingException {
    final URI uri = new URI("http://unused.com");
    final MockTokenServerClient client = new MockTokenServerClient(uri, Executors.newSingleThreadExecutor());

    // Even the 200 code here is false because the body is malformed.
    expectDelegateCalls(uri, client, 200, new BasicHeader("x-backoff", "13"), "baaaa", false, 13, true);
    expectDelegateCalls(uri, client, 400, new BasicHeader("X-Weave-Backoff", "15"), null, false, 15, true);
    expectDelegateCalls(uri, client, 503, new BasicHeader("retry-after", "3"), null, false, 3, true);

    // Retry-After is only processed on 503.
    expectDelegateCalls(uri, client, 200, new BasicHeader("retry-after", "13"), null, false, 0, false);

    // Now let's try one with a valid body.
    expectDelegateCalls(uri, client, 200, new BasicHeader("X-Backoff", "1234"), TEST_TOKEN_RESPONSE, true, 1234, true);
  }
}
