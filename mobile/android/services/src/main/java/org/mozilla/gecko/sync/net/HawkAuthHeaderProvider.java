/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import java.io.IOException;
import java.io.InputStream;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.security.GeneralSecurityException;
import java.security.InvalidKeyException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Locale;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.StringUtils;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpEntityEnclosingRequest;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.message.BasicHeader;
import ch.boye.httpclientandroidlib.protocol.BasicHttpContext;

/**
 * An <code>AuthHeaderProvider</code> that returns an Authorization header for
 * Hawk: <a href="https://github.com/hueniverse/hawk">https://github.com/hueniverse/hawk</a>.
 *
 * Hawk is an HTTP authentication scheme using a message authentication code
 * (MAC) algorithm to provide partial HTTP request cryptographic verification.
 * Hawk is the successor to the HMAC authentication scheme.
 */
public class HawkAuthHeaderProvider implements AuthHeaderProvider {
  public static final String LOG_TAG = HawkAuthHeaderProvider.class.getSimpleName();

  public static final int HAWK_HEADER_VERSION = 1;

  protected static final int NONCE_LENGTH_IN_BYTES = 8;
  protected static final String HMAC_SHA256_ALGORITHM = "hmacSHA256";

  protected final String id;
  protected final byte[] key;
  protected final boolean includePayloadHash;
  protected final long skewSeconds;

  /**
   * Create a Hawk Authorization header provider.
   * <p>
   * Hawk specifies no mechanism by which a client receives an
   * identifier-and-key pair from the server.
   * <p>
   * Hawk requests can include a payload verification hash with requests that
   * enclose an entity (PATCH, POST, and PUT requests).  <b>You should default
   * to including the payload verification hash<b> unless you have a good reason
   * not to -- the server can always ignore payload verification hashes provided
   * by the client.
   *
   * @param id
   *          to name requests with.
   * @param key
   *          to sign request with.
   *
   * @param includePayloadHash
   *          true if payload verification hash should be included in signed
   *          request header. See <a href="https://github.com/hueniverse/hawk#payload-validation">https://github.com/hueniverse/hawk#payload-validation</a>.
   *
   * @param skewSeconds
   *          a number of seconds by which to skew the current time when
   *          computing a header.
   */
  public HawkAuthHeaderProvider(String id, byte[] key, boolean includePayloadHash, long skewSeconds) {
    if (id == null) {
      throw new IllegalArgumentException("id must not be null");
    }
    if (key == null) {
      throw new IllegalArgumentException("key must not be null");
    }
    this.id = id;
    this.key = key;
    this.includePayloadHash = includePayloadHash;
    this.skewSeconds = skewSeconds;
  }

  /**
   * @return the current time in milliseconds.
   */
  @SuppressWarnings("static-method")
  protected long now() {
    return System.currentTimeMillis();
  }

  /**
   * @return the current time in seconds, adjusted for skew. This should
   *         approximate the server's timestamp.
   */
  protected long getTimestampSeconds() {
    return (now() / 1000) + skewSeconds;
  }

  @Override
  public Header getAuthHeader(HttpRequestBase request, BasicHttpContext context, DefaultHttpClient client) throws GeneralSecurityException {
    long timestamp = getTimestampSeconds();
    String nonce = Base64.encodeBase64String(Utils.generateRandomBytes(NONCE_LENGTH_IN_BYTES));
    String extra = "";

    try {
      return getAuthHeader(request, context, client, timestamp, nonce, extra, this.includePayloadHash);
    } catch (Exception e) {
      // We lie a little and make every exception a GeneralSecurityException.
      throw new GeneralSecurityException(e);
    }
  }

  /**
   * Helper function that generates an HTTP Authorization: Hawk header given
   * additional Hawk specific data.
   *
   * @throws NoSuchAlgorithmException
   * @throws InvalidKeyException
   * @throws IOException
   */
  protected Header getAuthHeader(HttpRequestBase request, BasicHttpContext context, DefaultHttpClient client,
      long timestamp, String nonce, String extra, boolean includePayloadHash)
          throws InvalidKeyException, NoSuchAlgorithmException, IOException {
    if (timestamp < 0) {
      throw new IllegalArgumentException("timestamp must contain only [0-9].");
    }

    if (nonce == null) {
      throw new IllegalArgumentException("nonce must not be null.");
    }
    if (nonce.length() == 0) {
      throw new IllegalArgumentException("nonce must not be empty.");
    }

    String payloadHash = null;
    if (includePayloadHash) {
      payloadHash = getPayloadHashString(request);
    } else {
      Logger.debug(LOG_TAG, "Configured to not include payload hash for this request.");
    }

    String app = null;
    String dlg = null;
    String requestString = getRequestString(request, "header", timestamp, nonce, payloadHash, extra, app, dlg);
    String macString = getSignature(requestString.getBytes(StringUtils.UTF_8), this.key);

    StringBuilder sb = new StringBuilder();
    sb.append("Hawk id=\"");
    sb.append(this.id);
    sb.append("\", ");
    sb.append("ts=\"");
    sb.append(timestamp);
    sb.append("\", ");
    sb.append("nonce=\"");
    sb.append(nonce);
    sb.append("\", ");
    if (payloadHash != null) {
      sb.append("hash=\"");
      sb.append(payloadHash);
      sb.append("\", ");
    }
    if (extra != null && extra.length() > 0) {
      sb.append("ext=\"");
      sb.append(escapeExtraHeaderAttribute(extra));
      sb.append("\", ");
    }
    sb.append("mac=\"");
    sb.append(macString);
    sb.append("\"");

    return new BasicHeader("Authorization", sb.toString());
  }

  /**
   * Get the payload verification hash for the given request, if possible.
   * <p>
   * Returns null if the request does not enclose an entity (is not an HTTP
   * PATCH, POST, or PUT). Throws if the payload verification hash cannot be
   * computed.
   *
   * @param request
   *          to compute hash for.
   * @return verification hash, or null if the request does not enclose an entity.
   * @throws IllegalArgumentException if the request does not enclose a valid non-null entity.
   * @throws NoSuchAlgorithmException
   * @throws IOException
   */
  protected static String getPayloadHashString(HttpRequestBase request)
      throws NoSuchAlgorithmException, IOException, IllegalArgumentException {
    final boolean shouldComputePayloadHash = request instanceof HttpEntityEnclosingRequest;
    if (!shouldComputePayloadHash) {
      Logger.debug(LOG_TAG, "Not computing payload verification hash for non-enclosing request.");
      return null;
    }
    if (!(request instanceof HttpEntityEnclosingRequest)) {
      throw new IllegalArgumentException("Cannot compute payload verification hash for enclosing request without an entity");
    }
    final HttpEntity entity = ((HttpEntityEnclosingRequest) request).getEntity();
    if (entity == null) {
      throw new IllegalArgumentException("Cannot compute payload verification hash for enclosing request with a null entity");
    }
    return Base64.encodeBase64String(getPayloadHash(entity));
  }

  /**
   * Escape the user-provided extra string for the ext="" header attribute.
   * <p>
   * Hawk escapes the header ext="" attribute differently than it does the extra
   * line in the normalized request string.
   * <p>
   * See <a href="https://github.com/hueniverse/hawk/blob/871cc597973110900467bd3dfb84a3c892f678fb/lib/browser.js#L385">https://github.com/hueniverse/hawk/blob/871cc597973110900467bd3dfb84a3c892f678fb/lib/browser.js#L385</a>.
   *
   * @param extra to escape.
   * @return extra escaped for the ext="" header attribute.
   */
  protected static String escapeExtraHeaderAttribute(String extra) {
    return extra.replaceAll("\\\\", "\\\\").replaceAll("\"", "\\\"");
  }

  /**
   * Escape the user-provided extra string for inserting into the normalized
   * request string.
   * <p>
   * Hawk escapes the header ext="" attribute differently than it does the extra
   * line in the normalized request string.
   * <p>
   * See <a href="https://github.com/hueniverse/hawk/blob/871cc597973110900467bd3dfb84a3c892f678fb/lib/crypto.js#L67">https://github.com/hueniverse/hawk/blob/871cc597973110900467bd3dfb84a3c892f678fb/lib/crypto.js#L67</a>.
   *
   * @param extra to escape.
   * @return extra escaped for the normalized request string.
   */
  protected static String escapeExtraString(String extra) {
    return extra.replaceAll("\\\\", "\\\\").replaceAll("\n", "\\n");
  }

  /**
   * Return the content type with no parameters (pieces following ;).
   *
   * @param contentTypeHeader to interrogate.
   * @return base content type.
   */
  protected static String getBaseContentType(Header contentTypeHeader) {
    if (contentTypeHeader == null) {
      throw new IllegalArgumentException("contentTypeHeader must not be null.");
    }
    String contentType = contentTypeHeader.getValue();
    if (contentType == null) {
      throw new IllegalArgumentException("contentTypeHeader value must not be null.");
    }
    int index = contentType.indexOf(";");
    if (index < 0) {
      return contentType.trim();
    }
    return contentType.substring(0, index).trim();
  }

  /**
   * Generate the SHA-256 hash of a normalized Hawk payload generated from an
   * HTTP entity.
   * <p>
   * <b>Warning:</b> the entity <b>must</b> be repeatable.  If it is not, this
   * code throws an <code>IllegalArgumentException</code>.
   * <p>
   * This is under-specified; the code here was reverse engineered from the code
   * at
   * <a href="https://github.com/hueniverse/hawk/blob/871cc597973110900467bd3dfb84a3c892f678fb/lib/crypto.js#L81">https://github.com/hueniverse/hawk/blob/871cc597973110900467bd3dfb84a3c892f678fb/lib/crypto.js#L81</a>.
   * @param entity to normalize and hash.
   * @return hash.
   * @throws IllegalArgumentException if entity is not repeatable.
   */
  protected static byte[] getPayloadHash(HttpEntity entity) throws IOException, NoSuchAlgorithmException {
    if (!entity.isRepeatable()) {
      throw new IllegalArgumentException("entity must be repeatable");
    }
    final MessageDigest digest = MessageDigest.getInstance("SHA-256");
    digest.update(("hawk." + HAWK_HEADER_VERSION + ".payload\n").getBytes(StringUtils.UTF_8));
    digest.update(getBaseContentType(entity.getContentType()).getBytes(StringUtils.UTF_8));
    digest.update("\n".getBytes(StringUtils.UTF_8));
    InputStream stream = entity.getContent();
    try {
      int numRead;
      byte[] buffer = new byte[4096];
      while (-1 != (numRead = stream.read(buffer))) {
        if (numRead > 0) {
          digest.update(buffer, 0, numRead);
        }
      }
      digest.update("\n".getBytes(StringUtils.UTF_8)); // Trailing newline is specified by Hawk.
      return digest.digest();
    } finally {
      stream.close();
    }
  }

  /**
   * Generate a normalized Hawk request string. This is under-specified; the
   * code here was reverse engineered from the code at
   * <a href="https://github.com/hueniverse/hawk/blob/871cc597973110900467bd3dfb84a3c892f678fb/lib/crypto.js#L55">https://github.com/hueniverse/hawk/blob/871cc597973110900467bd3dfb84a3c892f678fb/lib/crypto.js#L55</a>.
   * <p>
   * This method trusts its inputs to be valid.
   */
  protected static String getRequestString(HttpUriRequest request, String type, long timestamp, String nonce, String hash, String extra, String app, String dlg) {
    String method = request.getMethod().toUpperCase(Locale.US);

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

    StringBuilder sb = new StringBuilder();
    sb.append("hawk.");
    sb.append(HAWK_HEADER_VERSION);
    sb.append('.');
    sb.append(type);
    sb.append('\n');
    sb.append(timestamp);
    sb.append('\n');
    sb.append(nonce);
    sb.append('\n');
    sb.append(method);
    sb.append('\n');
    sb.append(path);
    sb.append('\n');
    sb.append(host);
    sb.append('\n');
    sb.append(port);
    sb.append('\n');
    if (hash != null) {
      sb.append(hash);
    }
    sb.append("\n");
    if (extra != null && extra.length() > 0) {
      sb.append(escapeExtraString(extra));
    }
    sb.append("\n");
    if (app != null) {
      sb.append(app);
      sb.append("\n");
      if (dlg != null) {
        sb.append(dlg);
      }
      sb.append("\n");
    }

    return sb.toString();
  }

  protected static byte[] hmacSha256(byte[] message, byte[] key)
      throws NoSuchAlgorithmException, InvalidKeyException {

    SecretKeySpec keySpec = new SecretKeySpec(key, HMAC_SHA256_ALGORITHM);

    Mac hasher = Mac.getInstance(HMAC_SHA256_ALGORITHM);
    hasher.init(keySpec);
    hasher.update(message);

    return hasher.doFinal();
  }

  /**
   * Sign a Hawk request string.
   *
   * @param requestString to sign.
   * @param key as <code>String</code>.
   * @return signature as base-64 encoded string.
   * @throws InvalidKeyException
   * @throws NoSuchAlgorithmException
   * @throws UnsupportedEncodingException
   */
  protected static String getSignature(byte[] requestString, byte[] key)
      throws InvalidKeyException, NoSuchAlgorithmException, UnsupportedEncodingException {
    return Base64.encodeBase64String(hmacSha256(requestString, key));
  }
}
