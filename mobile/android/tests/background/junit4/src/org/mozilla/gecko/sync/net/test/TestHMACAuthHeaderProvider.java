/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.net.test;

import static org.junit.Assert.assertEquals;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;

import org.junit.Test;
import org.mozilla.gecko.sync.net.HMACAuthHeaderProvider;

import ch.boye.httpclientandroidlib.client.methods.HttpGet;
import ch.boye.httpclientandroidlib.client.methods.HttpPost;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;

public class TestHMACAuthHeaderProvider {
  // Expose a few protected static member functions as public for testing.
  protected static class LeakyHMACAuthHeaderProvider extends HMACAuthHeaderProvider {
    public LeakyHMACAuthHeaderProvider(String identifier, String key) {
      super(identifier, key);
    }

    public static String getRequestString(HttpUriRequest request, long timestampInSeconds, String nonce, String extra) {
      return HMACAuthHeaderProvider.getRequestString(request, timestampInSeconds, nonce, extra);
    }

    public static String getSignature(String requestString, String key)
        throws InvalidKeyException, NoSuchAlgorithmException, UnsupportedEncodingException {
      return HMACAuthHeaderProvider.getSignature(requestString, key);
    }
  }

  @Test
  public void testGetRequestStringSpecExample1() throws Exception {
    long timestamp = 1336363200;
    String nonceString = "dj83hs9s";
    String extra = "";
    URI uri = new URI("http://example.com/resource/1?b=1&a=2");

    HttpUriRequest req = new HttpGet(uri);

    String expected = "1336363200\n" +
        "dj83hs9s\n" +
        "GET\n" +
        "/resource/1?b=1&a=2\n" +
        "example.com\n" +
        "80\n" +
        "\n";

    assertEquals(expected, LeakyHMACAuthHeaderProvider.getRequestString(req, timestamp, nonceString, extra));
  }

  @Test
  public void testGetRequestStringSpecExample2() throws Exception {
    long timestamp = 264095;
    String nonceString = "7d8f3e4a";
    String extra = "a,b,c";
    URI uri = new URI("http://example.com/request?b5=%3D%253D&a3=a&c%40=&a2=r%20b&c2&a3=2+q");

    HttpUriRequest req = new HttpPost(uri);

    String expected = "264095\n" +
        "7d8f3e4a\n" +
        "POST\n" +
        "/request?b5=%3D%253D&a3=a&c%40=&a2=r%20b&c2&a3=2+q\n" +
        "example.com\n" +
        "80\n" +
        "a,b,c\n";

    assertEquals(expected, LeakyHMACAuthHeaderProvider.getRequestString(req, timestamp, nonceString, extra));
  }

  @Test
  public void testPort() throws Exception {
    long timestamp = 264095;
    String nonceString = "7d8f3e4a";
    String extra = "a,b,c";
    URI uri = new URI("http://example.com:88/request?b5=%3D%253D&a3=a&c%40=&a2=r%20b&c2&a3=2+q");

    HttpUriRequest req = new HttpPost(uri);

    String expected = "264095\n" +
        "7d8f3e4a\n" +
        "POST\n" +
        "/request?b5=%3D%253D&a3=a&c%40=&a2=r%20b&c2&a3=2+q\n" +
        "example.com\n" +
        "88\n" +
        "a,b,c\n";

    assertEquals(expected, LeakyHMACAuthHeaderProvider.getRequestString(req, timestamp, nonceString, extra));
  }

  @Test
  public void testHTTPS() throws Exception {
    long timestamp = 264095;
    String nonceString = "7d8f3e4a";
    String extra = "a,b,c";
    URI uri = new URI("https://example.com/request?b5=%3D%253D&a3=a&c%40=&a2=r%20b&c2&a3=2+q");

    HttpUriRequest req = new HttpPost(uri);

    String expected = "264095\n" +
        "7d8f3e4a\n" +
        "POST\n" +
        "/request?b5=%3D%253D&a3=a&c%40=&a2=r%20b&c2&a3=2+q\n" +
        "example.com\n" +
        "443\n" +
        "a,b,c\n";

    assertEquals(expected, LeakyHMACAuthHeaderProvider.getRequestString(req, timestamp, nonceString, extra));
  }

  @Test
  public void testSpecSignatureExample() throws Exception {
    String extra = "";
    long timestampInSeconds = 1336363200;
    String nonceString = "dj83hs9s";

    URI uri = new URI("http://example.com/resource/1?b=1&a=2");
    HttpRequestBase req = new HttpGet(uri);

    String requestString = LeakyHMACAuthHeaderProvider.getRequestString(req, timestampInSeconds, nonceString, extra);

    String expected = "1336363200\n" +
        "dj83hs9s\n" +
        "GET\n" +
        "/resource/1?b=1&a=2\n" +
        "example.com\n" +
        "80\n" +
        "\n";

    assertEquals(expected, requestString);

    // There appears to be an error in the current spec.
    // Spec is at https://tools.ietf.org/html/draft-ietf-oauth-v2-http-mac-01#section-1.1
    // Error is reported at http://www.ietf.org/mail-archive/web/oauth/current/msg09741.html
    // assertEquals("bhCQXTVyfj5cmA9uKkPFx1zeOXM=", HMACAuthHeaderProvider.getSignature(requestString, keyString));
  }

  @Test
  public void testCompatibleWithDesktopFirefox() throws Exception {
    // These are test values used in the FF Sync Client testsuite.

    // String identifier = "vmo1txkttblmn51u2p3zk2xiy16hgvm5ok8qiv1yyi86ffjzy9zj0ez9x6wnvbx7";
    String keyString = "b8u1cc5iiio5o319og7hh8faf2gi5ym4aq0zwf112cv1287an65fudu5zj7zo7dz";

    String extra = "";
    long timestampInSeconds = 1329181221;
    String nonceString = "wGX71";

    URI uri = new URI("http://10.250.2.176/alias/");
    HttpRequestBase req = new HttpGet(uri);

    String requestString = LeakyHMACAuthHeaderProvider.getRequestString(req, timestampInSeconds, nonceString, extra);

    assertEquals("jzh5chjQc2zFEvLbyHnPdX11Yck=", LeakyHMACAuthHeaderProvider.getSignature(requestString, keyString));
  }
}
