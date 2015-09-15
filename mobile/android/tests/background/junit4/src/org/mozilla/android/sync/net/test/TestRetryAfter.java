package org.mozilla.android.sync.net.test;

import java.util.Date;

import org.junit.Test;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.runner.RunWith;
import org.mozilla.gecko.sync.net.SyncResponse;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.impl.cookie.DateUtils;
import ch.boye.httpclientandroidlib.message.BasicHttpResponse;
import ch.boye.httpclientandroidlib.message.BasicStatusLine;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestRetryAfter {
  private int TEST_SECONDS = 120;

  @Test
  public void testRetryAfterParsesSeconds() {
    final HttpResponse response = new BasicHttpResponse(
        new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), 503, "Illegal method/protocol"));
    response.addHeader("Retry-After", Long.toString(TEST_SECONDS)); // Retry-After given in seconds.

    final SyncResponse syncResponse = new SyncResponse(response);
    assertEquals(TEST_SECONDS, syncResponse.retryAfterInSeconds());
  }

  @Test
  public void testRetryAfterParsesHTTPDate() {
    Date future = new Date(System.currentTimeMillis() + TEST_SECONDS * 1000);

    final HttpResponse response = new BasicHttpResponse(
        new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), 503, "Illegal method/protocol"));
    response.addHeader("Retry-After", DateUtils.formatDate(future));

    final SyncResponse syncResponse = new SyncResponse(response);
    assertTrue(syncResponse.retryAfterInSeconds() > TEST_SECONDS - 15);
    assertTrue(syncResponse.retryAfterInSeconds() < TEST_SECONDS + 15);
  }

  @SuppressWarnings("static-method")
  @Test
  public void testRetryAfterParsesMalformed() {
    final HttpResponse response = new BasicHttpResponse(
        new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), 503, "Illegal method/protocol"));
    response.addHeader("Retry-After", "10X");

    final SyncResponse syncResponse = new SyncResponse(response);
    assertEquals(-1, syncResponse.retryAfterInSeconds());
  }

  @SuppressWarnings("static-method")
  @Test
  public void testRetryAfterParsesNeither() {
    final HttpResponse response = new BasicHttpResponse(
        new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), 503, "Illegal method/protocol"));

    final SyncResponse syncResponse = new SyncResponse(response);
    assertEquals(-1, syncResponse.retryAfterInSeconds());
  }

  @Test
  public void testRetryAfterParsesLargerRetryAfter() {
    final HttpResponse response = new BasicHttpResponse(
        new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), 503, "Illegal method/protocol"));
    response.addHeader("Retry-After", Long.toString(TEST_SECONDS + 1));
    response.addHeader("X-Weave-Backoff", Long.toString(TEST_SECONDS));

    final SyncResponse syncResponse = new SyncResponse(response);
    assertEquals(1000 * (TEST_SECONDS + 1), syncResponse.totalBackoffInMilliseconds());
  }

  @Test
  public void testRetryAfterParsesLargerXWeaveBackoff() {
    final HttpResponse response = new BasicHttpResponse(
        new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), 503, "Illegal method/protocol"));
    response.addHeader("Retry-After", Long.toString(TEST_SECONDS));
    response.addHeader("X-Weave-Backoff", Long.toString(TEST_SECONDS + 1));

    final SyncResponse syncResponse = new SyncResponse(response);
    assertEquals(1000 * (TEST_SECONDS + 1), syncResponse.totalBackoffInMilliseconds());
  }
}
