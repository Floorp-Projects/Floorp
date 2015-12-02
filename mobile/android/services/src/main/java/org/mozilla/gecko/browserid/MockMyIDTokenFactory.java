/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.browserid;

import java.math.BigInteger;
import java.security.NoSuchAlgorithmException;
import java.security.spec.InvalidKeySpecException;

/**
 * Generate certificates and assertions backed by mockmyid.com's private key.
 * <p>
 * These artifacts are for testing only.
 */
public class MockMyIDTokenFactory {
  public static final BigInteger MOCKMYID_x = new BigInteger("385cb3509f086e110c5e24bdd395a84b335a09ae", 16);
  public static final BigInteger MOCKMYID_y = new BigInteger("738ec929b559b604a232a9b55a5295afc368063bb9c20fac4e53a74970a4db7956d48e4c7ed523405f629b4cc83062f13029c4d615bbacb8b97f5e56f0c7ac9bc1d4e23809889fa061425c984061fca1826040c399715ce7ed385c4dd0d402256912451e03452d3c961614eb458f188e3e8d2782916c43dbe2e571251ce38262", 16);
  public static final BigInteger MOCKMYID_p = new BigInteger("ff600483db6abfc5b45eab78594b3533d550d9f1bf2a992a7a8daa6dc34f8045ad4e6e0c429d334eeeaaefd7e23d4810be00e4cc1492cba325ba81ff2d5a5b305a8d17eb3bf4a06a349d392e00d329744a5179380344e82a18c47933438f891e22aeef812d69c8f75e326cb70ea000c3f776dfdbd604638c2ef717fc26d02e17", 16);
  public static final BigInteger MOCKMYID_q = new BigInteger("e21e04f911d1ed7991008ecaab3bf775984309c3", 16);
  public static final BigInteger MOCKMYID_g = new BigInteger("c52a4a0ff3b7e61fdf1867ce84138369a6154f4afa92966e3c827e25cfa6cf508b90e5de419e1337e07a2e9e2a3cd5dea704d175f8ebf6af397d69e110b96afb17c7a03259329e4829b0d03bbc7896b15b4ade53e130858cc34d96269aa89041f409136c7242a38895c9d5bccad4f389af1d7a4bd1398bd072dffa896233397a", 16);

  // Computed lazily by static <code>getMockMyIDPrivateKey</code>.
  protected static SigningPrivateKey cachedMockMyIDPrivateKey;

  public static SigningPrivateKey getMockMyIDPrivateKey() throws NoSuchAlgorithmException, InvalidKeySpecException {
    if (cachedMockMyIDPrivateKey == null) {
      cachedMockMyIDPrivateKey = DSACryptoImplementation.createPrivateKey(MOCKMYID_x, MOCKMYID_p, MOCKMYID_q, MOCKMYID_g);
    }
    return cachedMockMyIDPrivateKey;
  }

  /**
   * Sign a public key asserting ownership of username@mockmyid.com with
   * mockmyid.com's private key.
   *
   * @param publicKeyToSign
   *          public key to sign.
   * @param username
   *          sign username@mockmyid.com
   * @param issuedAt
   *          timestamp for certificate, in milliseconds since the epoch.
   * @param expiresAt
   *          expiration timestamp for certificate, in milliseconds since the epoch.
   * @return encoded certificate string.
   * @throws Exception
   */
  public String createMockMyIDCertificate(final VerifyingPublicKey publicKeyToSign, String username,
      final long issuedAt, final long expiresAt)
          throws Exception {
    if (!username.endsWith("@mockmyid.com")) {
      username = username + "@mockmyid.com";
    }
    SigningPrivateKey mockMyIdPrivateKey = getMockMyIDPrivateKey();
    return JSONWebTokenUtils.createCertificate(publicKeyToSign, username, "mockmyid.com", issuedAt, expiresAt, mockMyIdPrivateKey);
  }

  /**
   * Sign a public key asserting ownership of username@mockmyid.com with
   * mockmyid.com's private key.
   *
   * @param publicKeyToSign
   *          public key to sign.
   * @param username
   *          sign username@mockmyid.com
   * @return encoded certificate string.
   * @throws Exception
   */
  public String createMockMyIDCertificate(final VerifyingPublicKey publicKeyToSign, final String username)
      throws Exception {
    long ciat = System.currentTimeMillis();
    long cexp = ciat + JSONWebTokenUtils.DEFAULT_CERTIFICATE_DURATION_IN_MILLISECONDS;
    return createMockMyIDCertificate(publicKeyToSign, username, ciat, cexp);
  }

  /**
   * Generate an assertion asserting ownership of username@mockmyid.com to a
   * relying party. The underlying certificate is signed by mockymid.com's
   * private key.
   *
   * @param keyPair
   *          to sign with.
   * @param username
   *          sign username@mockmyid.com.
   * @param certificateIssuedAt
   *          timestamp for certificate, in milliseconds since the epoch.
   * @param certificateExpiresAt
   *          expiration timestamp for certificate, in milliseconds since the epoch.
   * @param assertionIssuedAt
   *          timestamp for assertion, in milliseconds since the epoch; if null,
   *          no timestamp is included.
   * @param assertionExpiresAt
   *          expiration timestamp for assertion, in milliseconds since the epoch.
   * @return encoded assertion string.
   * @throws Exception
   */
  public String createMockMyIDAssertion(BrowserIDKeyPair keyPair, String username, String audience,
      long certificateIssuedAt, long certificateExpiresAt,
      Long assertionIssuedAt, long assertionExpiresAt)
          throws Exception {
    String certificate = createMockMyIDCertificate(keyPair.getPublic(), username,
        certificateIssuedAt, certificateExpiresAt);
    return JSONWebTokenUtils.createAssertion(keyPair.getPrivate(), certificate, audience,
        JSONWebTokenUtils.DEFAULT_ASSERTION_ISSUER, assertionIssuedAt, assertionExpiresAt);
  }

  /**
   * Generate an assertion asserting ownership of username@mockmyid.com to a
   * relying party. The underlying certificate is signed by mockymid.com's
   * private key.
   *
   * @param keyPair
   *          to sign with.
   * @param username
   *          sign username@mockmyid.com.
   * @return encoded assertion string.
   * @throws Exception
   */
  public String createMockMyIDAssertion(BrowserIDKeyPair keyPair, String username, String audience)
      throws Exception {
    long ciat = System.currentTimeMillis();
    long cexp = ciat + JSONWebTokenUtils.DEFAULT_CERTIFICATE_DURATION_IN_MILLISECONDS;
    long aiat = ciat + 1;
    long aexp = aiat + JSONWebTokenUtils.DEFAULT_ASSERTION_DURATION_IN_MILLISECONDS;
    return createMockMyIDAssertion(keyPair, username, audience,
        ciat, cexp, aiat, aexp);
  }
}
