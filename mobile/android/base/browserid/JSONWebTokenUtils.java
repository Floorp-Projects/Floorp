/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.browserid;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.Map;

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

  protected static String getPayloadString(String payloadString, String issuer,
      long issuedAt, String audience, long expiresAt) throws NonObjectJSONException,
      IOException, ParseException {
    ExtendedJSONObject payload;
    if (payloadString != null) {
      payload = new ExtendedJSONObject(payloadString);
    } else {
      payload = new ExtendedJSONObject();
    }
    payload.put("iss", issuer);
    payload.put("iat", issuedAt);
    if (audience != null) {
      payload.put("aud", audience);
    }
    payload.put("exp", expiresAt);
    return payload.toJSONString();
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
    String payloadString = getPayloadString(certificatePayloadString, issuer, issuedAt, null, expiresAt);
    return JSONWebTokenUtils.encode(payloadString, privateKey);
  }

  public static String createCertificate(VerifyingPublicKey publicKeyToSign, String email, SigningPrivateKey privateKey) throws NonObjectJSONException, IOException, ParseException, GeneralSecurityException  {
    String issuer = DEFAULT_CERTIFICATE_ISSUER;
    long issuedAt = System.currentTimeMillis();
    long durationInMilliseconds = DEFAULT_CERTIFICATE_DURATION_IN_MILLISECONDS;
    return createCertificate(publicKeyToSign, email, issuer, issuedAt, issuedAt + durationInMilliseconds, privateKey);
  }

  public static String createAssertion(SigningPrivateKey privateKeyToSignWith, String certificate, String audience,
      String issuer, long issuedAt, long durationInMilliseconds) throws NonObjectJSONException, IOException, ParseException, GeneralSecurityException  {
    long expiresAt = issuedAt + durationInMilliseconds;
    String emptyAssertionPayloadString = "{}";
    String payloadString = getPayloadString(emptyAssertionPayloadString, issuer, issuedAt, audience, expiresAt);
    String signature = JSONWebTokenUtils.encode(payloadString, privateKeyToSignWith);
    return certificate + "~" + signature;
  }

  public static String createAssertion(SigningPrivateKey privateKeyToSignWith, String certificate, String audience) throws NonObjectJSONException, IOException, ParseException, GeneralSecurityException  {
    String issuer = DEFAULT_ASSERTION_ISSUER;
    long issuedAt = System.currentTimeMillis();
    long durationInMilliseconds = DEFAULT_ASSERTION_DURATION_IN_MILLISECONDS;
    return createAssertion(privateKeyToSignWith, certificate, audience, issuer, issuedAt, durationInMilliseconds);
  }

  /**
   * For debugging only!
   *
   * @param input certificate to dump.
   * @return true if the certificate is well-formed.
   */
  public static boolean dumpCertificate(String input) {
    try {
      String[] parts = input.split("\\.");
      if (parts.length != 3) {
        throw new IllegalArgumentException("certificate must have three parts");
      }
      String cHeader = new String(Base64.decodeBase64(parts[0]));
      String cPayload = new String(Base64.decodeBase64(parts[1]));
      String cSignature = Utils.byte2Hex(Base64.decodeBase64(parts[2]));
      System.out.println("certificate header:    " + cHeader);
      System.out.println("certificate payload:   " + cPayload);
      System.out.println("certificate signature: " + cSignature);
      return true;
    } catch (Exception e) {
      System.out.println("Malformed certificate -- got exception trying to dump contents.");
      e.printStackTrace();
      return false;
    }
  }

  /**
   * For debugging only!
   *
   * @param input assertion to dump.
   * @return true if the assertion is well-formed.
   */
  public static boolean dumpAssertion(String input) {
    try {
      String[] parts = input.split("~");
      if (parts.length != 2) {
        throw new IllegalArgumentException("input must have two parts");
      }
      String certificate = parts[0];
      String assertion = parts[1];
      parts = assertion.split("\\.");
      if (parts.length != 3) {
        throw new IllegalArgumentException("assertion must have three parts");
      }
      String aHeader = new String(Base64.decodeBase64(parts[0]));
      String aPayload = new String(Base64.decodeBase64(parts[1]));
      String aSignature = Utils.byte2Hex(Base64.decodeBase64(parts[2]));
      // We do all the assertion parsing *before* dumping the certificate in
      // case there's a malformed assertion.
      dumpCertificate(certificate);
      System.out.println("assertion   header:    " + aHeader);
      System.out.println("assertion   payload:   " + aPayload);
      System.out.println("assertion   signature: " + aSignature);
      return true;
    } catch (Exception e) {
      System.out.println("Malformed assertion -- got exception trying to dump contents.");
      e.printStackTrace();
      return false;
    }
  }
}
