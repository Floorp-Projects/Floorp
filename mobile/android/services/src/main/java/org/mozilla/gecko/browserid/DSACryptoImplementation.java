/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.browserid;

import android.annotation.SuppressLint;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.PRNGFixes;

import java.math.BigInteger;
import java.security.GeneralSecurityException;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;
import java.security.Signature;
import java.security.interfaces.DSAParams;
import java.security.interfaces.DSAPrivateKey;
import java.security.interfaces.DSAPublicKey;
import java.security.spec.DSAPrivateKeySpec;
import java.security.spec.DSAPublicKeySpec;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.KeySpec;

public class DSACryptoImplementation {
  private static final String LOG_TAG = DSACryptoImplementation.class.getSimpleName();

  public static final String SIGNATURE_ALGORITHM = "SHA1withDSA";
  public static final int SIGNATURE_LENGTH_BYTES = 40; // DSA signatures are always 40 bytes long.

  /**
   * Parameters are serialized as hex strings. Hex-versus-decimal was
   * reverse-engineered from what the Persona public verifier accepted. We
   * expect to follow the JOSE/JWT spec as it solidifies, and that will probably
   * mean unifying this base.
   */
  protected static final int SERIALIZATION_BASE = 16;

  protected static class DSAVerifyingPublicKey implements VerifyingPublicKey {
    protected final DSAPublicKey publicKey;

    public DSAVerifyingPublicKey(DSAPublicKey publicKey) {
      this.publicKey = publicKey;
    }

    /**
     * Serialize to a JSON object.
     * <p>
     * Parameters are serialized as hex strings. Hex-versus-decimal was
     * reverse-engineered from what the Persona public verifier accepted.
     */
    @Override
    public ExtendedJSONObject toJSONObject() {
      DSAParams params = publicKey.getParams();
      ExtendedJSONObject o = new ExtendedJSONObject();
      o.put("algorithm", "DS");
      o.put("y", publicKey.getY().toString(SERIALIZATION_BASE));
      o.put("g", params.getG().toString(SERIALIZATION_BASE));
      o.put("p", params.getP().toString(SERIALIZATION_BASE));
      o.put("q", params.getQ().toString(SERIALIZATION_BASE));
      return o;
    }

    @Override
    public boolean verifyMessage(byte[] bytes, byte[] signature)
        throws GeneralSecurityException {
      if (bytes == null) {
        throw new IllegalArgumentException("bytes must not be null");
      }
      if (signature == null) {
        throw new IllegalArgumentException("signature must not be null");
      }
      if (signature.length != SIGNATURE_LENGTH_BYTES) {
        return false;
      }
      byte[] first = new byte[signature.length / 2];
      byte[] second = new byte[signature.length / 2];
      System.arraycopy(signature, 0, first, 0, first.length);
      System.arraycopy(signature, first.length, second, 0, second.length);
      BigInteger r = new BigInteger(Utils.byte2Hex(first), 16);
      BigInteger s = new BigInteger(Utils.byte2Hex(second), 16);
      // This is awful, but encoding an extra 0 byte works better on devices.
      byte[] encoded = ASNUtils.encodeTwoArraysToASN1(
          Utils.hex2Byte(r.toString(16), 1 + SIGNATURE_LENGTH_BYTES / 2),
          Utils.hex2Byte(s.toString(16), 1 + SIGNATURE_LENGTH_BYTES / 2));

      final Signature signer = Signature.getInstance(SIGNATURE_ALGORITHM);
      signer.initVerify(publicKey);
      signer.update(bytes);
      return signer.verify(encoded);
    }
  }

  protected static class DSASigningPrivateKey implements SigningPrivateKey {
    protected final DSAPrivateKey privateKey;

    public DSASigningPrivateKey(DSAPrivateKey privateKey) {
      this.privateKey = privateKey;
    }

    @Override
    public String getAlgorithm() {
      return "DS" + (privateKey.getParams().getP().bitLength() + 7)/8;
    }

    /**
     * Serialize to a JSON object.
     * <p>
     * Parameters are serialized as decimal strings. Hex-versus-decimal was
     * reverse-engineered from what the Persona public verifier accepted.
     */
    @Override
    public ExtendedJSONObject toJSONObject() {
      DSAParams params = privateKey.getParams();
      ExtendedJSONObject o = new ExtendedJSONObject();
      o.put("algorithm", "DS");
      o.put("x", privateKey.getX().toString(SERIALIZATION_BASE));
      o.put("g", params.getG().toString(SERIALIZATION_BASE));
      o.put("p", params.getP().toString(SERIALIZATION_BASE));
      o.put("q", params.getQ().toString(SERIALIZATION_BASE));
      return o;
    }

    @SuppressLint("TrulyRandom")
    @Override
    public byte[] signMessage(byte[] bytes)
        throws GeneralSecurityException {
      if (bytes == null) {
        throw new IllegalArgumentException("bytes must not be null");
      }

      try {
        PRNGFixes.apply();
      } catch (Exception e) {
        // Not much to be done here: it was weak before, and we couldn't patch it, so it's weak now.  Not worth aborting.
        Logger.error(LOG_TAG, "Got exception applying PRNGFixes!  Cryptographic data produced on this device may be weak.  Ignoring.", e);
      }

      final Signature signer = Signature.getInstance(SIGNATURE_ALGORITHM);
      signer.initSign(privateKey);
      signer.update(bytes);
      final byte[] signature = signer.sign();

      final byte[][] arrays = ASNUtils.decodeTwoArraysFromASN1(signature);
      BigInteger r = new BigInteger(arrays[0]);
      BigInteger s = new BigInteger(arrays[1]);
      // This is awful, but signatures are always 40 bytes long.
      byte[] decoded = Utils.concatAll(
          Utils.hex2Byte(r.toString(16), SIGNATURE_LENGTH_BYTES / 2),
          Utils.hex2Byte(s.toString(16), SIGNATURE_LENGTH_BYTES / 2));
      return decoded;
    }
  }

  public static BrowserIDKeyPair generateKeyPair(int keysize)
      throws NoSuchAlgorithmException {
    final KeyPairGenerator keyPairGenerator = KeyPairGenerator.getInstance("DSA");
    keyPairGenerator.initialize(keysize);
    final KeyPair keyPair = keyPairGenerator.generateKeyPair();
    DSAPrivateKey privateKey = (DSAPrivateKey) keyPair.getPrivate();
    DSAPublicKey publicKey = (DSAPublicKey) keyPair.getPublic();
    return new BrowserIDKeyPair(new DSASigningPrivateKey(privateKey), new DSAVerifyingPublicKey(publicKey));
  }

  public static SigningPrivateKey createPrivateKey(BigInteger x, BigInteger p, BigInteger q, BigInteger g) throws NoSuchAlgorithmException, InvalidKeySpecException {
    if (x == null) {
      throw new IllegalArgumentException("x must not be null");
    }
    if (p == null) {
      throw new IllegalArgumentException("p must not be null");
    }
    if (q == null) {
      throw new IllegalArgumentException("q must not be null");
    }
    if (g == null) {
      throw new IllegalArgumentException("g must not be null");
    }
    KeySpec keySpec = new DSAPrivateKeySpec(x, p, q, g);
    KeyFactory keyFactory = KeyFactory.getInstance("DSA");
    DSAPrivateKey privateKey = (DSAPrivateKey) keyFactory.generatePrivate(keySpec);
    return new DSASigningPrivateKey(privateKey);
  }

  public static VerifyingPublicKey createPublicKey(BigInteger y, BigInteger p, BigInteger q, BigInteger g) throws NoSuchAlgorithmException, InvalidKeySpecException {
    if (y == null) {
      throw new IllegalArgumentException("n must not be null");
    }
    if (p == null) {
      throw new IllegalArgumentException("p must not be null");
    }
    if (q == null) {
      throw new IllegalArgumentException("q must not be null");
    }
    if (g == null) {
      throw new IllegalArgumentException("g must not be null");
    }
    KeySpec keySpec = new DSAPublicKeySpec(y, p, q, g);
    KeyFactory keyFactory = KeyFactory.getInstance("DSA");
    DSAPublicKey publicKey = (DSAPublicKey) keyFactory.generatePublic(keySpec);
    return new DSAVerifyingPublicKey(publicKey);
  }

  public static SigningPrivateKey createPrivateKey(ExtendedJSONObject o) throws InvalidKeySpecException, NoSuchAlgorithmException {
    String algorithm = o.getString("algorithm");
    if (!"DS".equals(algorithm)) {
      throw new InvalidKeySpecException("algorithm must equal DS, was " + algorithm);
    }
    try {
      BigInteger x = new BigInteger(o.getString("x"), SERIALIZATION_BASE);
      BigInteger p = new BigInteger(o.getString("p"), SERIALIZATION_BASE);
      BigInteger q = new BigInteger(o.getString("q"), SERIALIZATION_BASE);
      BigInteger g = new BigInteger(o.getString("g"), SERIALIZATION_BASE);
      return createPrivateKey(x, p, q, g);
    } catch (NullPointerException | NumberFormatException e) {
      throw new InvalidKeySpecException("x, p, q, and g must be integers encoded as strings, base " + SERIALIZATION_BASE);
    }
  }

  public static VerifyingPublicKey createPublicKey(ExtendedJSONObject o) throws InvalidKeySpecException, NoSuchAlgorithmException {
    String algorithm = o.getString("algorithm");
    if (!"DS".equals(algorithm)) {
      throw new InvalidKeySpecException("algorithm must equal DS, was " + algorithm);
    }
    try {
      BigInteger y = new BigInteger(o.getString("y"), SERIALIZATION_BASE);
      BigInteger p = new BigInteger(o.getString("p"), SERIALIZATION_BASE);
      BigInteger q = new BigInteger(o.getString("q"), SERIALIZATION_BASE);
      BigInteger g = new BigInteger(o.getString("g"), SERIALIZATION_BASE);
      return createPublicKey(y, p, q, g);
    } catch (NullPointerException | NumberFormatException e) {
      throw new InvalidKeySpecException("y, p, q, and g must be integers encoded as strings, base " + SERIALIZATION_BASE);
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
