/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.browserid;

import java.math.BigInteger;
import java.security.GeneralSecurityException;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;
import java.security.Signature;
import java.security.interfaces.RSAPrivateKey;
import java.security.interfaces.RSAPublicKey;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.KeySpec;
import java.security.spec.RSAPrivateKeySpec;
import java.security.spec.RSAPublicKeySpec;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;

public class RSACryptoImplementation {
  public static final String SIGNATURE_ALGORITHM = "SHA256withRSA";

  /**
   * Parameters are serialized as decimal strings. Hex-versus-decimal was
   * reverse-engineered from what the Persona public verifier accepted. We
   * expect to follow the JOSE/JWT spec as it solidifies, and that will probably
   * mean unifying this base.
   */
  protected static final int SERIALIZATION_BASE = 10;

  protected static class RSAVerifyingPublicKey implements VerifyingPublicKey {
    protected final RSAPublicKey publicKey;

    public RSAVerifyingPublicKey(RSAPublicKey publicKey) {
      this.publicKey = publicKey;
    }

    /**
     * Serialize to a JSON object.
     * <p>
     * Parameters are serialized as decimal strings. Hex-versus-decimal was
     * reverse-engineered from what the Persona public verifier accepted.
     */
    @Override
    public ExtendedJSONObject toJSONObject() {
      ExtendedJSONObject o = new ExtendedJSONObject();
      o.put("algorithm", "RS");
      o.put("n", publicKey.getModulus().toString(SERIALIZATION_BASE));
      o.put("e", publicKey.getPublicExponent().toString(SERIALIZATION_BASE));
      return o;
    }

    @Override
    public boolean verifyMessage(byte[] bytes, byte[] signature)
        throws GeneralSecurityException {
      final Signature signer = Signature.getInstance(SIGNATURE_ALGORITHM);
      signer.initVerify(publicKey);
      signer.update(bytes);
      return signer.verify(signature);
    }
  }

  protected static class RSASigningPrivateKey implements SigningPrivateKey {
    protected final RSAPrivateKey privateKey;

    public RSASigningPrivateKey(RSAPrivateKey privateKey) {
      this.privateKey = privateKey;
    }

    @Override
    public String getAlgorithm() {
      return "RS" + (privateKey.getModulus().bitLength() + 7)/8;
    }

    /**
     * Serialize to a JSON object.
     * <p>
     * Parameters are serialized as decimal strings. Hex-versus-decimal was
     * reverse-engineered from what the Persona public verifier accepted.
     */
    @Override
    public ExtendedJSONObject toJSONObject() {
      ExtendedJSONObject o = new ExtendedJSONObject();
      o.put("algorithm", "RS");
      o.put("n", privateKey.getModulus().toString(SERIALIZATION_BASE));
      o.put("d", privateKey.getPrivateExponent().toString(SERIALIZATION_BASE));
      return o;
    }

    @Override
    public byte[] signMessage(byte[] bytes)
        throws GeneralSecurityException {
      final Signature signer = Signature.getInstance(SIGNATURE_ALGORITHM);
      signer.initSign(privateKey);
      signer.update(bytes);
      return signer.sign();
    }
  }

  public static BrowserIDKeyPair generateKeyPair(final int keysize) throws NoSuchAlgorithmException {
    final KeyPairGenerator keyPairGenerator = KeyPairGenerator.getInstance("RSA");
    keyPairGenerator.initialize(keysize);
    final KeyPair keyPair = keyPairGenerator.generateKeyPair();
    RSAPrivateKey privateKey = (RSAPrivateKey) keyPair.getPrivate();
    RSAPublicKey publicKey = (RSAPublicKey) keyPair.getPublic();
    return new BrowserIDKeyPair(new RSASigningPrivateKey(privateKey), new RSAVerifyingPublicKey(publicKey));
  }

  public static SigningPrivateKey createPrivateKey(BigInteger n, BigInteger d) throws NoSuchAlgorithmException, InvalidKeySpecException {
    if (n == null) {
      throw new IllegalArgumentException("n must not be null");
    }
    if (d == null) {
      throw new IllegalArgumentException("d must not be null");
    }
    KeyFactory keyFactory = KeyFactory.getInstance("RSA");
    KeySpec keySpec = new RSAPrivateKeySpec(n, d);
    RSAPrivateKey privateKey = (RSAPrivateKey) keyFactory.generatePrivate(keySpec);
    return new RSASigningPrivateKey(privateKey);
  }

  public static VerifyingPublicKey createPublicKey(BigInteger n, BigInteger e) throws NoSuchAlgorithmException, InvalidKeySpecException {
    if (n == null) {
      throw new IllegalArgumentException("n must not be null");
    }
    if (e == null) {
      throw new IllegalArgumentException("e must not be null");
    }
    KeyFactory keyFactory = KeyFactory.getInstance("RSA");
    KeySpec keySpec = new RSAPublicKeySpec(n, e);
    RSAPublicKey publicKey = (RSAPublicKey) keyFactory.generatePublic(keySpec);
    return new RSAVerifyingPublicKey(publicKey);
  }

  public static SigningPrivateKey createPrivateKey(ExtendedJSONObject o) throws InvalidKeySpecException, NoSuchAlgorithmException {
    String algorithm = o.getString("algorithm");
    if (!"RS".equals(algorithm)) {
      throw new InvalidKeySpecException("algorithm must equal RS, was " + algorithm);
    }
    try {
      BigInteger n = new BigInteger(o.getString("n"), SERIALIZATION_BASE);
      BigInteger d = new BigInteger(o.getString("d"), SERIALIZATION_BASE);
      return createPrivateKey(n, d);
    } catch (NullPointerException e) {
      throw new InvalidKeySpecException("n and d must be integers encoded as strings, base " + SERIALIZATION_BASE);
    } catch (NumberFormatException e) {
      throw new InvalidKeySpecException("n and d must be integers encoded as strings, base " + SERIALIZATION_BASE);
    }
  }

  public static VerifyingPublicKey createPublicKey(ExtendedJSONObject o) throws InvalidKeySpecException, NoSuchAlgorithmException {
    String algorithm = o.getString("algorithm");
    if (!"RS".equals(algorithm)) {
      throw new InvalidKeySpecException("algorithm must equal RS, was " + algorithm);
    }
    try {
      BigInteger n = new BigInteger(o.getString("n"), SERIALIZATION_BASE);
      BigInteger e = new BigInteger(o.getString("e"), SERIALIZATION_BASE);
      return createPublicKey(n, e);
    } catch (NullPointerException e) {
      throw new InvalidKeySpecException("n and e must be integers encoded as strings, base " + SERIALIZATION_BASE);
    } catch (NumberFormatException e) {
      throw new InvalidKeySpecException("n and e must be integers encoded as strings, base " + SERIALIZATION_BASE);
    }
  }

  public static BrowserIDKeyPair fromJSONObject(ExtendedJSONObject o) throws InvalidKeySpecException, NoSuchAlgorithmException {
    try {
      ExtendedJSONObject privateKey = o.getObject(BrowserIDKeyPair.JSON_KEY_PRIVATEKEY);
      ExtendedJSONObject publicKey = o.getObject(BrowserIDKeyPair.JSON_KEY_PUBLICKEY);
      if (privateKey == null) {
        throw new InvalidKeySpecException("privateKey must not be null");
      }
      if (publicKey == null) {
        throw new InvalidKeySpecException("publicKey must not be null");
      }
      return new BrowserIDKeyPair(createPrivateKey(privateKey), createPublicKey(publicKey));
    } catch (NonObjectJSONException e) {
      throw new InvalidKeySpecException("privateKey and publicKey must be JSON objects");
    }
  }
}
