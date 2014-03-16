/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.browserid;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.Map;
import java.util.TreeMap;

import org.json.simple.JSONObject;
import org.json.simple.parser.ParseException;
import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.apache.commons.codec.binary.StringUtils;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.Utils;

/**
 * Encode and decode JSON Web Tokens.
 * <p>
 * Reverse-engineered from the Node.js jwcrypto library at
 * <a href="https://github.com/mozilla/jwcrypto">https://github.com/mozilla/jwcrypto</a>
 * and informed by the informal draft standard "JSON Web Token (JWT)" at
 * <a href="http://self-issued.info/docs/draft-ietf-oauth-json-web-token.html">http://self-issued.info/docs/draft-ietf-oauth-json-web-token.html</a>.
 */
public class JSONWebTokenUtils {
  public static final long DEFAULT_CERTIFICATE_DURATION_IN_MILLISECONDS = 60 * 60 * 1000;
  public static final long DEFAULT_ASSERTION_DURATION_IN_MILLISECONDS = 60 * 60 * 1000;
  public static final long DEFAULT_FUTURE_EXPIRES_AT_IN_MILLISECONDS = 9999999999999L;
  public static final String DEFAULT_CERTIFICATE_ISSUER = "127.0.0.1";
  public static final String DEFAULT_ASSERTION_ISSUER = "127.0.0.1";

  public static String encode(String payload, SigningPrivateKey privateKey) throws UnsupportedEncodingException, GeneralSecurityException  {
    return encode(payload, privateKey, null);
  }

  protected static String encode(String payload, SigningPrivateKey privateKey, Map<String, Object> headerFields) throws UnsupportedEncodingException, GeneralSecurityException  {
    ExtendedJSONObject header = new ExtendedJSONObject();
    if (headerFields != null) {
      header.putAll(headerFields);
    }
    header.put("alg", privateKey.getAlgorithm());
    String encodedHeader  = Base64.encodeBase64URLSafeString(header.toJSONString().getBytes("UTF-8"));
    String encodedPayload = Base64.encodeBase64URLSafeString(payload.getBytes("UTF-8"));
    ArrayList<String> segments = new ArrayList<String>();
    segments.add(encodedHeader);
    segments.add(encodedPayload);
    byte[] message = Utils.toDelimitedString(".", segments).getBytes("UTF-8");
    byte[] signature = privateKey.signMessage(message);
    segments.add(Base64.encodeBase64URLSafeString(signature));
    return Utils.toDelimitedString(".", segments);
  }

  public static String decode(String token, VerifyingPublicKey publicKey) throws GeneralSecurityException, UnsupportedEncodingException  {
    if (token == null) {
      throw new IllegalArgumentException("token must not be null");
    }
    String[] segments = token.split("\\.");
    if (segments == null || segments.length != 3) {
      throw new GeneralSecurityException("malformed token");
    }
    byte[] message = (segments[0] + "." + segments[1]).getBytes("UTF-8");
    byte[] signature = Base64.decodeBase64(segments[2]);
    boolean verifies = publicKey.verifyMessage(message, signature);
    if (!verifies) {
      throw new GeneralSecurityException("bad signature");
    }
    String payload = StringUtils.newStringUtf8(Base64.decodeBase64(segments[1]));
    return payload;
  }

  /**
   * Public for testing.
   */
  @SuppressWarnings("unchecked")
  public static String getPayloadString(String payloadString, String audience, String issuer,
      Long issuedAt, long expiresAt) throws NonObjectJSONException,
      IOException, ParseException {
    ExtendedJSONObject payload;
    if (payloadString != null) {
      payload = new ExtendedJSONObject(payloadString);
    } else {
      payload = new ExtendedJSONObject();
    }
    if (audience != null) {
      payload.put("aud", audience);
    }
    payload.put("iss", issuer);
    if (issuedAt != null) {
      payload.put("iat", issuedAt);
    }
    payload.put("exp", expiresAt);
    // TreeMap so that keys are sorted. A small attempt to keep output stable over time.
    return JSONObject.toJSONString(new TreeMap<Object, Object>(payload.object));
  }

  protected static String getCertificatePayloadString(VerifyingPublicKey publicKeyToSign, String email) throws NonObjectJSONException, IOException, ParseException  {
    ExtendedJSONObject payload = new ExtendedJSONObject();
    ExtendedJSONObject principal = new ExtendedJSONObject();
    principal.put("email", email);
    payload.put("principal", principal);
    payload.put("public-key", publicKeyToSign.toJSONObject());
    return payload.toJSONString();
  }

  public static String createCertificate(VerifyingPublicKey publicKeyToSign, String email,
      String issuer, long issuedAt, long expiresAt, SigningPrivateKey privateKey) throws NonObjectJSONException, IOException, ParseException, GeneralSecurityException  {
    String certificatePayloadString = getCertificatePayloadString(publicKeyToSign, email);
    String payloadString = getPayloadString(certificatePayloadString, null, issuer, issuedAt, expiresAt);
    return JSONWebTokenUtils.encode(payloadString, privateKey);
  }

  /**
   * Create a Browser ID assertion.
   *
   * @param privateKeyToSignWith
   *          private key to sign assertion with.
   * @param certificate
   *          to include in assertion; no attempt is made to ensure the
   *          certificate is valid, or corresponds to the private key, or any
   *          other condition.
   * @param audience
   *          to produce assertion for.
   * @param issuer
   *          to produce assertion for.
   * @param issuedAt
   *          timestamp for assertion, in milliseconds since the epoch; if null,
   *          no timestamp is included.
   * @param expiresAt
   *          expiration timestamp for assertion, in milliseconds since the epoch.
   * @return assertion.
   * @throws NonObjectJSONException
   * @throws IOException
   * @throws ParseException
   * @throws GeneralSecurityException
   */
  public static String createAssertion(SigningPrivateKey privateKeyToSignWith, String certificate, String audience,
      String issuer, Long issuedAt, long expiresAt) throws NonObjectJSONException, IOException, ParseException, GeneralSecurityException  {
    String emptyAssertionPayloadString = "{}";
    String payloadString = getPayloadString(emptyAssertionPayloadString, audience, issuer, issuedAt, expiresAt);
    String signature = JSONWebTokenUtils.encode(payloadString, privateKeyToSignWith);
    return certificate + "~" + signature;
  }

  /**
   * For debugging only!
   *
   * @param input
   *          certificate to dump.
   * @return non-null object with keys header, payload, signature if the
   *         certificate is well-formed.
   */
  public static ExtendedJSONObject parseCertificate(String input) {
    try {
      String[] parts = input.split("\\.");
      if (parts.length != 3) {
        return null;
      }
      String cHeader = new String(Base64.decodeBase64(parts[0]));
      String cPayload = new String(Base64.decodeBase64(parts[1]));
      String cSignature = Utils.byte2Hex(Base64.decodeBase64(parts[2]));
      ExtendedJSONObject o = new ExtendedJSONObject();
      o.put("header", new ExtendedJSONObject(cHeader));
      o.put("payload", new ExtendedJSONObject(cPayload));
      o.put("signature", cSignature);
      return o;
    } catch (Exception e) {
      return null;
    }
  }

  /**
   * For debugging only!
   *
   * @param input certificate to dump.
   * @return true if the certificate is well-formed.
   */
  public static boolean dumpCertificate(String input) {
    ExtendedJSONObject c = parseCertificate(input);
    try {
      if (c == null) {
        System.out.println("Malformed certificate -- got exception trying to dump contents.");
        return false;
      }
      System.out.println("certificate header:    " + c.getString("header"));
      System.out.println("certificate payload:   " + c.getString("payload"));
      System.out.println("certificate signature: " + c.getString("signature"));
      return true;
    } catch (Exception e) {
      System.out.println("Malformed certificate -- got exception trying to dump contents.");
      return false;
    }
  }

  /**
   * For debugging only!
   *
   * @param input assertion to dump.
   * @return true if the assertion is well-formed.
   */
  public static ExtendedJSONObject parseAssertion(String input) {
    try {
      String[] parts = input.split("~");
      if (parts.length != 2) {
        return null;
      }
      String certificate = parts[0];
      String assertion = parts[1];
      parts = assertion.split("\\.");
      if (parts.length != 3) {
        return null;
      }
      String aHeader = new String(Base64.decodeBase64(parts[0]));
      String aPayload = new String(Base64.decodeBase64(parts[1]));
      String aSignature = Utils.byte2Hex(Base64.decodeBase64(parts[2]));
      // We do all the assertion parsing *before* dumping the certificate in
      // case there's a malformed assertion.
      ExtendedJSONObject o = new ExtendedJSONObject();
      o.put("header", new ExtendedJSONObject(aHeader));
      o.put("payload", new ExtendedJSONObject(aPayload));
      o.put("signature", aSignature);
      o.put("certificate", certificate);
      return o;
    } catch (Exception e) {
      return null;
    }
  }

  /**
   * For debugging only!
   *
   * @param input assertion to dump.
   * @return true if the assertion is well-formed.
   */
  public static boolean dumpAssertion(String input) {
    ExtendedJSONObject a = parseAssertion(input);
    try {
      if (a == null) {
        System.out.println("Malformed assertion -- got exception trying to dump contents.");
        return false;
      }
      dumpCertificate(a.getString("certificate"));
      System.out.println("assertion   header:    " + a.getString("header"));
      System.out.println("assertion   payload:   " + a.getString("payload"));
      System.out.println("assertion   signature: " + a.getString("signature"));
      return true;
    } catch (Exception e) {
      System.out.println("Malformed assertion -- got exception trying to dump contents.");
      return false;
    }
  }
}
