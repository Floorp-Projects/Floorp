/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.net.test;

import static org.junit.Assert.assertEquals;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.sync.net.HawkAuthHeaderProvider;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.client.methods.HttpGet;
import ch.boye.httpclientandroidlib.client.methods.HttpPost;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;
import ch.boye.httpclientandroidlib.entity.StringEntity;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.message.BasicHeader;
import ch.boye.httpclientandroidlib.protocol.BasicHttpContext;
import org.robolectric.RobolectricGradleTestRunner;

/**
 * These test vectors were taken from
 * <a href="https://github.com/hueniverse/hawk/blob/871cc597973110900467bd3dfb84a3c892f678fb/README.md">https://github.com/hueniverse/hawk/blob/871cc597973110900467bd3dfb84a3c892f678fb/README.md</a>.
 */
@RunWith(RobolectricGradleTestRunner.class)
public class TestHawkAuthHeaderProvider {
  // Expose a few protected static member functions as public for testing.
  protected static class LeakyHawkAuthHeaderProvider extends HawkAuthHeaderProvider {
    public LeakyHawkAuthHeaderProvider(String tokenId, byte[] reqHMACKey) {
      // getAuthHeader takes includePayloadHash as a parameter.
      super(tokenId, reqHMACKey, false, 0L);
    }

    // Public for testing.
    public static String getRequestString(HttpUriRequest request, String type, long timestamp, String nonce, String hash, String extra, String app, String dlg) {
      return HawkAuthHeaderProvider.getRequestString(request, type, timestamp, nonce, hash, extra, app, dlg);
    }

    // Public for testing.
    public static String getSignature(String requestString, String key)
        throws InvalidKeyException, NoSuchAlgorithmException, UnsupportedEncodingException {
      return HawkAuthHeaderProvider.getSignature(requestString.getBytes("UTF-8"), key.getBytes("UTF-8"));
    }

    // Public for testing.
    @Override
    public Header getAuthHeader(HttpRequestBase request, BasicHttpContext context, DefaultHttpClient client,
        long timestamp, String nonce, String extra, boolean includePayloadHash)
            throws InvalidKeyException, NoSuchAlgorithmException, IOException {
      return super.getAuthHeader(request, context, client, timestamp, nonce, extra, includePayloadHash);
    }

    // Public for testing.
    public static String getBaseContentType(Header contentTypeHeader) {
      return HawkAuthHeaderProvider.getBaseContentType(contentTypeHeader);
    }
  }

  @Test
  public void testSpecRequestString() throws Exception {
    long timestamp = 1353832234;
    String nonce = "j4h3g2";
    String extra = "some-app-ext-data";
    String hash = null;
    String app = null;
    String dlg = null;

    URI uri = new URI("http://example.com:8000/resource/1?b=1&a=2");
    HttpUriRequest req = new HttpGet(uri);

    String expected = "hawk.1.header\n" +
        "1353832234\n" +
        "j4h3g2\n" +
        "GET\n" +
        "/resource/1?b=1&a=2\n" +
        "example.com\n" +
        "8000\n" +
        "\n" +
        "some-app-ext-data\n";

    // LeakyHawkAuthHeaderProvider.
    assertEquals(expected, LeakyHawkAuthHeaderProvider.getRequestString(req, "header", timestamp, nonce, hash, extra, app, dlg));
  }

  @Test
  public void testSpecSignatureExample() throws Exception {
    String input = "hawk.1.header\n" +
        "1353832234\n" +
        "j4h3g2\n" +
        "GET\n" +
        "/resource/1?b=1&a=2\n" +
        "example.com\n" +
        "8000\n" +
        "\n" +
        "some-app-ext-data\n";

    assertEquals("6R4rV5iE+NPoym+WwjeHzjAGXUtLNIxmo1vpMofpLAE=", LeakyHawkAuthHeaderProvider.getSignature(input, "werxhqb98rpaxn39848xrunpaw3489ruxnpa98w4rxn"));
  }

  @Test
  public void testSpecPayloadExample() throws Exception {
    LeakyHawkAuthHeaderProvider provider = new LeakyHawkAuthHeaderProvider("dh37fgj492je", "werxhqb98rpaxn39848xrunpaw3489ruxnpa98w4rxn".getBytes("UTF-8"));
    URI uri = new URI("http://example.com:8000/resource/1?b=1&a=2");
    HttpPost req = new HttpPost(uri);
    String body = "Thank you for flying Hawk";
    req.setEntity(new StringEntity(body));
    Header header = provider.getAuthHeader(req, null, null, 1353832234L, "j4h3g2", "some-app-ext-data", true);
    String expected = "Hawk id=\"dh37fgj492je\", ts=\"1353832234\", nonce=\"j4h3g2\", hash=\"Yi9LfIIFRtBEPt74PVmbTF/xVAwPn7ub15ePICfgnuY=\", ext=\"some-app-ext-data\", mac=\"aSe1DERmZuRl3pI36/9BdZmnErTw3sNzOOAUlfeKjVw=\"";
    assertEquals("Authorization", header.getName());
    assertEquals(expected, header.getValue());
  }

  @Test
  public void testSpecAuthorizationHeader() throws Exception {
    LeakyHawkAuthHeaderProvider provider = new LeakyHawkAuthHeaderProvider("dh37fgj492je", "werxhqb98rpaxn39848xrunpaw3489ruxnpa98w4rxn".getBytes("UTF-8"));
    URI uri = new URI("http://example.com:8000/resource/1?b=1&a=2");
    HttpGet req = new HttpGet(uri);
    Header header = provider.getAuthHeader(req, null, null, 1353832234L, "j4h3g2", "some-app-ext-data", false);
    String expected = "Hawk id=\"dh37fgj492je\", ts=\"1353832234\", nonce=\"j4h3g2\", ext=\"some-app-ext-data\", mac=\"6R4rV5iE+NPoym+WwjeHzjAGXUtLNIxmo1vpMofpLAE=\"";
    assertEquals("Authorization", header.getName());
    assertEquals(expected, header.getValue());

    // For a non-POST, non-PUT request, a request to include the payload verification hash is silently ignored.
    header = provider.getAuthHeader(req, null, null, 1353832234L, "j4h3g2", "some-app-ext-data", true);
    assertEquals("Authorization", header.getName());
    assertEquals(expected, header.getValue());
  }

  @Test
  public void testGetBaseContentType() throws Exception {
    assertEquals("text/plain", LeakyHawkAuthHeaderProvider.getBaseContentType(new BasicHeader("Content-Type", "text/plain")));
    assertEquals("text/plain", LeakyHawkAuthHeaderProvider.getBaseContentType(new BasicHeader("Content-Type", "text/plain;one")));
    assertEquals("text/plain", LeakyHawkAuthHeaderProvider.getBaseContentType(new BasicHeader("Content-Type", "text/plain;one;two")));
    assertEquals("text/html", LeakyHawkAuthHeaderProvider.getBaseContentType(new BasicHeader("Content-Type", "text/html;charset=UTF-8")));
    assertEquals("text/html", LeakyHawkAuthHeaderProvider.getBaseContentType(new BasicHeader("Content-Type", "text/html; charset=UTF-8")));
    assertEquals("text/html", LeakyHawkAuthHeaderProvider.getBaseContentType(new BasicHeader("Content-Type", "text/html ;charset=UTF-8")));
  }
}
