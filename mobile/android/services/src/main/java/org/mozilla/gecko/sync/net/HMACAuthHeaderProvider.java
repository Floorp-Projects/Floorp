/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.security.GeneralSecurityException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.StringUtils;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.message.BasicHeader;
import ch.boye.httpclientandroidlib.protocol.BasicHttpContext;

/**
 * An <code>AuthHeaderProvider</code> that returns an Authorization header for
 * HMAC-SHA1-signed requests in the format expected by Mozilla Services
 * identity-attached services and specified by the MAC Authentication spec, available at
 * <a href="https://tools.ietf.org/html/draft-ietf-oauth-v2-http-mac">https://tools.ietf.org/html/draft-ietf-oauth-v2-http-mac</a>.
 * <p>
 * See <a href="https://wiki.mozilla.org/Services/Sagrada/ServiceClientFlow#Access">https://wiki.mozilla.org/Services/Sagrada/ServiceClientFlow#Access</a>.
 */
public class HMACAuthHeaderProvider implements AuthHeaderProvider {
  public static final String LOG_TAG = "HMACAuthHeaderProvider";

  public static final int NONCE_LENGTH_IN_BYTES = 8;

  public static final String HMAC_SHA1_ALGORITHM = "hmacSHA1";

  public final String identifier;
  public final String key;

  public HMACAuthHeaderProvider(String identifier, String key) {
    // Validate identifier string.  From the MAC Authentication spec:
    // id             = "id" "=" string-value
    // string-value   = ( <"> plain-string <"> ) / plain-string
    // plain-string   = 1*( %x20-21 / %x23-5B / %x5D-7E )
    // We add quotes around the id string, so input identifier must be a plain-string.
    if (identifier == null) {
      throw new IllegalArgumentException("identifier must not be null.");
    }
    if (!isPlainString(identifier)) {
      throw new IllegalArgumentException("identifier must be a plain-string.");
    }

    if (key == null) {
      throw new IllegalArgumentException("key must not be null.");
    }

    this.identifier = identifier;
    this.key = key;
  }

  @Override
  public Header getAuthHeader(HttpRequestBase request, BasicHttpContext context, DefaultHttpClient client) throws GeneralSecurityException {
    long timestamp = System.currentTimeMillis() / 1000;
    String nonce = Base64.encodeBase64String(Utils.generateRandomBytes(NONCE_LENGTH_IN_BYTES));
    String extra = "";

    try {
      return getAuthHeader(request, context, client, timestamp, nonce, extra);
    } catch (InvalidKeyException | NoSuchAlgorithmException | UnsupportedEncodingException e) {
      // We lie a little and make every exception a GeneralSecurityException.
      throw new GeneralSecurityException(e);
    }
  }

  /**
   * Test if input is a <code>plain-string</code>.
   * <p>
   * A plain-string is defined by the MAC Authentication spec as
   * <code>plain-string   = 1*( %x20-21 / %x23-5B / %x5D-7E )</code>.
   *
   * @param input
   *          as a String of "US-ASCII" bytes.
   * @return true if input is a <code>plain-string</code>; false otherwise.
   * @throws UnsupportedEncodingException
   */
  protected static boolean isPlainString(String input) {
    if (input == null || input.length() == 0) {
      return false;
    }

    byte[] bytes;
    try {
      bytes = input.getBytes("US-ASCII");
    } catch (UnsupportedEncodingException e) {
      // Should never happen.
      Logger.warn(LOG_TAG, "Got exception in isPlainString; returning false.", e);
      return false;
    }

    for (byte b : bytes) {
      if ((0x20 <= b && b <= 0x21) || (0x23 <= b && b <= 0x5B) || (0x5D <= b && b <= 0x7E)) {
        continue;
      }
      return false;
    }

    return true;
  }

  /**
   * Helper function that generates an HTTP Authorization header given
   * additional MAC Authentication specific data.
   *
   * @throws UnsupportedEncodingException
   * @throws NoSuchAlgorithmException 
   * @throws InvalidKeyException 
   */
  protected Header getAuthHeader(HttpRequestBase request, BasicHttpContext context, DefaultHttpClient client,
      long timestamp, String nonce, String extra)
      throws UnsupportedEncodingException, InvalidKeyException, NoSuchAlgorithmException {
    // Validate timestamp.  From the MAC Authentication spec:
    // timestamp      = 1*DIGIT
    // This is equivalent to timestamp >= 0.
    if (timestamp < 0) {
      throw new IllegalArgumentException("timestamp must contain only [0-9].");
    }

    // Validate nonce string.  From the MAC Authentication spec:
    // nonce          = "nonce" "=" string-value
    // string-value   = ( <"> plain-string <"> ) / plain-string
    // plain-string   = 1*( %x20-21 / %x23-5B / %x5D-7E )
    // We add quotes around the nonce string, so input nonce must be a plain-string.
    if (nonce == null) {
      throw new IllegalArgumentException("nonce must not be null.");
    }
    if (nonce.length() == 0) {
      throw new IllegalArgumentException("nonce must not be empty.");
    }
    if (!isPlainString(nonce)) {
      throw new IllegalArgumentException("nonce must be a plain-string.");
    }

    // Validate extra string.  From the MAC Authentication spec:
    // ext            = "ext" "=" string-value
    // string-value   = ( <"> plain-string <"> ) / plain-string
    // plain-string   = 1*( %x20-21 / %x23-5B / %x5D-7E )
    // We add quotes around the extra string, so input extra must be a plain-string.
    // We break the spec by allowing ext to be an empty string, i.e. to match 0*(...).
    if (extra == null) {
      throw new IllegalArgumentException("extra must not be null.");
    }
    if (extra.length() > 0 && !isPlainString(extra)) {
      throw new IllegalArgumentException("extra must be a plain-string.");
    }

    String requestString = getRequestString(request, timestamp, nonce, extra);
    String macString = getSignature(requestString, this.key);

    String h = "MAC id=\"" + this.identifier + "\", " +
               "ts=\""     + timestamp       + "\", " +
               "nonce=\""  + nonce           + "\", " +
               "mac=\""    + macString       + "\"";

    if (extra != null) {
      h += ", ext=\"" + extra + "\"";
    }

    Header header = new BasicHeader("Authorization", h);

    return header;
  }

  protected static byte[] sha1(byte[] message, byte[] key)
      throws NoSuchAlgorithmException, InvalidKeyException {

    SecretKeySpec keySpec = new SecretKeySpec(key, HMAC_SHA1_ALGORITHM);

    Mac hasher = Mac.getInstance(HMAC_SHA1_ALGORITHM);
    hasher.init(keySpec);
    hasher.update(message);

    byte[] hmac = hasher.doFinal();

    return hmac;
  }

  /**
   * Sign an HMAC request string.
   *
   * @param requestString to sign.
   * @param key as <code>String</code>.
   * @return signature as base-64 encoded string.
   * @throws InvalidKeyException
   * @throws NoSuchAlgorithmException
   */
  protected static String getSignature(String requestString, String key)
      throws InvalidKeyException, NoSuchAlgorithmException {
    String macString = Base64.encodeBase64String(sha1(requestString.getBytes(StringUtils.UTF_8), key.getBytes(StringUtils.UTF_8)));

    return macString;
  }

  /**
   * Generate an HMAC request string.
   * <p>
   * This method trusts its inputs to be valid as per the MAC Authentication spec.
   *
   * @param request HTTP request.
   * @param timestamp to use.
   * @param nonce to use.
   * @param extra to use.
   * @return request string.
   */
  protected static String getRequestString(HttpUriRequest request, long timestamp, String nonce, String extra) {
    String method = request.getMethod().toUpperCase();

    URI uri = request.getURI();
    String host = uri.getHost();

    String path = uri.getRawPath();
    if (uri.getRawQuery() != null) {
      path += "?";
      path += uri.getRawQuery();
    }
    if (uri.getRawFragment() != null) {
      path += "#";
      path += uri.getRawFragment();
    }

    int port = uri.getPort();
    String scheme = uri.getScheme();
    if (port != -1) {
    } else if ("http".equalsIgnoreCase(scheme)) {
      port = 80;
    } else if ("https".equalsIgnoreCase(scheme)) {
      port = 443;
    } else {
      throw new IllegalArgumentException("Unsupported URI scheme: " + scheme + ".");
    }

    String requestString = timestamp + "\n" +
        nonce       + "\n" +
        method      + "\n" +
        path        + "\n" +
        host        + "\n" +
        port        + "\n" +
        extra       + "\n";

    return requestString;
  }
}
