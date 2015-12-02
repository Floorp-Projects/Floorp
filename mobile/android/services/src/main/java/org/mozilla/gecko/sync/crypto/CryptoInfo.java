/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.crypto;

import java.security.GeneralSecurityException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.Mac;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

import org.mozilla.apache.commons.codec.binary.Base64;

/*
 * All info in these objects should be decoded (i.e. not BaseXX encoded).
 */
public class CryptoInfo {
  private static final String TRANSFORMATION     = "AES/CBC/PKCS5Padding";
  private static final String KEY_ALGORITHM_SPEC = "AES";

  private byte[] message;
  private byte[] iv;
  private byte[] hmac;
  private KeyBundle keys;

  /**
   * Return a CryptoInfo with given plaintext encrypted using given keys.
   */
  public static CryptoInfo encrypt(byte[] plaintextBytes, KeyBundle keys) throws CryptoException {
    CryptoInfo info = new CryptoInfo(plaintextBytes, keys);
    info.encrypt();
    return info;
  }

  /**
   * Return a CryptoInfo with given plaintext encrypted using given keys and initial vector.
   */
  public static CryptoInfo encrypt(byte[] plaintextBytes, byte[] iv, KeyBundle keys) throws CryptoException {
    CryptoInfo info = new CryptoInfo(plaintextBytes, iv, null, keys);
    info.encrypt();
    return info;
  }

  /**
   * Return a CryptoInfo with given ciphertext decrypted using given keys and initial vector, verifying that given HMAC validates.
   */
  public static CryptoInfo decrypt(byte[] ciphertext, byte[] iv, byte[] hmac, KeyBundle keys) throws CryptoException {
    CryptoInfo info = new CryptoInfo(ciphertext, iv, hmac, keys);
    info.decrypt();
    return info;
  }

  /*
   * Constructor typically used when encrypting.
   */
  public CryptoInfo(byte[] message, KeyBundle keys) {
    this.setMessage(message);
    this.setKeys(keys);
  }

  /*
   * Constructor typically used when decrypting.
   */
  public CryptoInfo(byte[] message, byte[] iv, byte[] hmac, KeyBundle keys) {
    this.setMessage(message);
    this.setIV(iv);
    this.setHMAC(hmac);
    this.setKeys(keys);
  }

  public byte[] getMessage() {
    return message;
  }

  public void setMessage(byte[] message) {
    this.message = message;
  }

  public byte[] getIV() {
    return iv;
  }

  public void setIV(byte[] iv) {
    this.iv = iv;
  }

  public byte[] getHMAC() {
    return hmac;
  }

  public void setHMAC(byte[] hmac) {
    this.hmac = hmac;
  }

  public KeyBundle getKeys() {
    return keys;
  }

  public void setKeys(KeyBundle keys) {
    this.keys = keys;
  }

  /*
   * Generate HMAC for given cipher text.
   */
  public static byte[] generatedHMACFor(byte[] message, KeyBundle keys) throws NoSuchAlgorithmException, InvalidKeyException {
    Mac hmacHasher = HKDF.makeHMACHasher(keys.getHMACKey());
    return hmacHasher.doFinal(Base64.encodeBase64(message));
  }

  /*
   * Return true if generated HMAC is the same as the specified HMAC.
   */
  public boolean generatedHMACIsHMAC() throws NoSuchAlgorithmException, InvalidKeyException {
    byte[] generatedHMAC = generatedHMACFor(getMessage(), getKeys());
    byte[] expectedHMAC  = getHMAC();
    return Arrays.equals(generatedHMAC, expectedHMAC);
  }

  /**
   * Performs functionality common to both encryption and decryption.
   *
   * @param cipher
   * @param inputMessage non-BaseXX-encoded message
   * @return encrypted/decrypted message
   * @throws CryptoException
   */
  private static byte[] commonCrypto(Cipher cipher, byte[] inputMessage)
                        throws CryptoException {
    byte[] outputMessage = null;
    try {
      outputMessage = cipher.doFinal(inputMessage);
    } catch (IllegalBlockSizeException | BadPaddingException e) {
      throw new CryptoException(e);
    }
      return outputMessage;
  }

  /**
   * Encrypt a CryptoInfo in-place.
   *
   * @throws CryptoException
   */
  public void encrypt() throws CryptoException {

    Cipher cipher = CryptoInfo.getCipher(TRANSFORMATION);
    try {
      byte[] encryptionKey = getKeys().getEncryptionKey();
      SecretKeySpec spec = new SecretKeySpec(encryptionKey, KEY_ALGORITHM_SPEC);

      // If no IV is provided, we allow the cipher to provide one.
      if (getIV() == null || getIV().length == 0) {
        cipher.init(Cipher.ENCRYPT_MODE, spec);
      } else {
        cipher.init(Cipher.ENCRYPT_MODE, spec, new IvParameterSpec(getIV()));
      }
    } catch (GeneralSecurityException ex) {
      throw new CryptoException(ex);
    }

    // Encrypt.
    byte[] encryptedBytes = commonCrypto(cipher, getMessage());
    byte[] iv = cipher.getIV();

    byte[] hmac;
    // Generate HMAC.
    try {
      hmac = generatedHMACFor(encryptedBytes, keys);
    } catch (NoSuchAlgorithmException | InvalidKeyException e) {
      throw new CryptoException(e);
    }

    // Update in place.  keys is already set.
    this.setHMAC(hmac);
    this.setIV(iv);
    this.setMessage(encryptedBytes);
  }

  /**
   * Decrypt a CryptoInfo in-place.
   *
   * @throws CryptoException
   */
  public void decrypt() throws CryptoException {

    // Check HMAC.
    try {
      if (!generatedHMACIsHMAC()) {
        throw new HMACVerificationException();
      }
    } catch (NoSuchAlgorithmException | InvalidKeyException e) {
      throw new CryptoException(e);
    }

    Cipher cipher = CryptoInfo.getCipher(TRANSFORMATION);
    try {
      byte[] encryptionKey = getKeys().getEncryptionKey();
      SecretKeySpec spec = new SecretKeySpec(encryptionKey, KEY_ALGORITHM_SPEC);
      cipher.init(Cipher.DECRYPT_MODE, spec, new IvParameterSpec(getIV()));
    } catch (GeneralSecurityException ex) {
      throw new CryptoException(ex);
    }
    byte[] decryptedBytes = commonCrypto(cipher, getMessage());
    byte[] iv = cipher.getIV();

    // Update in place.  keys is already set.
    this.setHMAC(null);
    this.setIV(iv);
    this.setMessage(decryptedBytes);
  }

  /**
   * Helper to get a Cipher object.
   *
   * @param transformation The type of Cipher to get.
   */
  private static Cipher getCipher(String transformation) throws CryptoException {
    try {
      return Cipher.getInstance(transformation);
    } catch (NoSuchAlgorithmException | NoSuchPaddingException e) {
      throw new CryptoException(e);
    }
  }
}
