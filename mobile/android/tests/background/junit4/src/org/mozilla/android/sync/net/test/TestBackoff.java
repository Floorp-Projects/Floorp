/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.net.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import org.junit.Test;
import org.mozilla.gecko.background.testhelpers.MockGlobalSession;
import org.mozilla.gecko.background.testhelpers.MockSharedPreferences;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.message.BasicHttpResponse;
import ch.boye.httpclientandroidlib.message.BasicStatusLine;

import org.mozilla.android.sync.test.helpers.MockGlobalSessionCallback;

public class TestBackoff {
  private final String TEST_USERNAME            = "johndoe";
  private final String TEST_PASSWORD            = "password";
  private final String TEST_SYNC_KEY            = "abcdeabcdeabcdeabcdeabcdea";
  private final long   TEST_BACKOFF_IN_SECONDS  = 1201;

  /**
   * Test that interpretHTTPFailure calls requestBackoff if
   * X-Weave-Backoff is present.
   */
  @Test
  public void testBackoffCalledIfBackoffHeaderPresent() {
    try {
      final MockGlobalSessionCallback callback = new MockGlobalSessionCallback();
      SyncConfiguration config = new SyncConfiguration(TEST_USERNAME, new BasicAuthHeaderProvider(TEST_USERNAME, TEST_PASSWORD), new MockSharedPreferences(), new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY));
      final GlobalSession session = new MockGlobalSession(config, callback);

      final HttpResponse response = new BasicHttpResponse(
        new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), 200, "OK"));
      response.addHeader("X-Weave-Backoff", Long.toString(TEST_BACKOFF_IN_SECONDS)); // Backoff given in seconds.

      session.interpretHTTPFailure(response); // This is synchronous...

      assertEquals(false, callback.calledSuccess); // ... so we can test immediately.
      assertEquals(false, callback.calledError);
      assertEquals(false, callback.calledAborted);
      assertEquals(true,  callback.calledRequestBackoff);
      assertEquals(TEST_BACKOFF_IN_SECONDS * 1000, callback.weaveBackoff); // Backoff returned in milliseconds.
    } catch (Exception e) {
      e.printStackTrace();
      fail("Got exception.");
    }
  }

  /**
   * Test that interpretHTTPFailure does not call requestBackoff if
   * X-Weave-Backoff is not present.
   */
  @Test
  public void testBackoffNotCalledIfBackoffHeaderNotPresent() {
    try {
      final MockGlobalSessionCallback callback = new MockGlobalSessionCallback();
      SyncConfiguration config = new SyncConfiguration(TEST_USERNAME, new BasicAuthHeaderProvider(TEST_USERNAME, TEST_PASSWORD), new MockSharedPreferences(), new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY));
      final GlobalSession session = new MockGlobalSession(config, callback);

      final HttpResponse response = new BasicHttpResponse(
	new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), 200, "OK"));

      session.interpretHTTPFailure(response); // This is synchronous...

      assertEquals(false, callback.calledSuccess); // ... so we can test immediately.
      assertEquals(false, callback.calledError);
      assertEquals(false, callback.calledAborted);
      assertEquals(false, callback.calledRequestBackoff);
    } catch (Exception e) {
      e.printStackTrace();
      fail("Got exception.");
    }
  }

  /**
   * Test that interpretHTTPFailure calls requestBackoff with the
   * largest specified value if X-Weave-Backoff and Retry-After are
   * present.
   */
  @Test
  public void testBackoffCalledIfMultipleBackoffHeadersPresent() {
    try {
      final MockGlobalSessionCallback callback = new MockGlobalSessionCallback();
      SyncConfiguration config = new SyncConfiguration(TEST_USERNAME, new BasicAuthHeaderProvider(TEST_USERNAME, TEST_PASSWORD), new MockSharedPreferences(), new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY));
      final GlobalSession session = new MockGlobalSession(config, callback);

      final HttpResponse response = new BasicHttpResponse(
        new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), 200, "OK"));
      response.addHeader("Retry-After", Long.toString(TEST_BACKOFF_IN_SECONDS)); // Backoff given in seconds.
      response.addHeader("X-Weave-Backoff", Long.toString(TEST_BACKOFF_IN_SECONDS + 1)); // If we now add a second header, the larger should be returned.

      session.interpretHTTPFailure(response); // This is synchronous...

      assertEquals(false, callback.calledSuccess); // ... so we can test immediately.
      assertEquals(false, callback.calledError);
      assertEquals(false, callback.calledAborted);
      assertEquals(true,  callback.calledRequestBackoff);
      assertEquals((TEST_BACKOFF_IN_SECONDS + 1) * 1000, callback.weaveBackoff); // Backoff returned in milliseconds.
    } catch (Exception e) {
      e.printStackTrace();
      fail("Got exception.");
    }
  }
}
