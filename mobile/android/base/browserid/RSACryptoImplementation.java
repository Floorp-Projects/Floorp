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

public class RSACryptoImplementation {
  public static final String SIGNATURE_ALGORITHM = "SHA256withRSA";

  protected static class RSAVerifyingPublicKey implements VerifyingPublicKey {
    protected final RSAPublicKey publicKey;

    public RSAVerifyingPublicKey(RSAPublicKey publicKey) {
      this.publicKey = publicKey;
    }

    @Override
    public String serialize() {
      ExtendedJSONObject o = new ExtendedJSONObject();
      o.put("algorithm", "RS");
      o.put("n", publicKey.getModulus().toString(10));
      o.put("e", publicKey.getPublicExponent().toString(10));
      return o.toJSONString();
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

    @Override
    public String serialize() {
      ExtendedJSONObject o = new ExtendedJSONObject();
      o.put("algorithm", "RS");
      o.put("n", privateKey.getModulus().toString(10));
      o.put("e", privateKey.getPrivateExponent().toString(10));
      return o.toJSONString();
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

  public static BrowserIDKeyPair generateKeypair(final int keysize) throws NoSuchAlgorithmException {
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
}
