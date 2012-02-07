/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jason Voll
 * Richard Newman <rnewman@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.sync.crypto;

import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.KeyGenerator;
import javax.crypto.Mac;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

import org.mozilla.apache.commons.codec.binary.Base32;
import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.sync.Utils;
import java.security.InvalidKeyException;

/*
 * Implements the basic required cryptography options.
 */
public class Cryptographer {

  private static final String TRANSFORMATION     = "AES/CBC/PKCS5Padding";
  private static final String KEY_ALGORITHM_SPEC = "AES";
  private static final int    KEY_SIZE           = 256;

  public static CryptoInfo encrypt(CryptoInfo info) throws CryptoException {

    Cipher cipher = getCipher();
    try {
      byte[] encryptionKey = info.getKeys().getEncryptionKey();
      SecretKeySpec spec = new SecretKeySpec(encryptionKey, KEY_ALGORITHM_SPEC);

      // If no IV is provided, we allow the cipher to provide one.
      if (info.getIV() == null ||
          info.getIV().length == 0) {
        cipher.init(Cipher.ENCRYPT_MODE, spec);
      } else {
        System.out.println("IV is " + info.getIV().length);
        cipher.init(Cipher.ENCRYPT_MODE, spec, new IvParameterSpec(info.getIV()));
      }
    } catch (GeneralSecurityException ex) {
      throw new CryptoException(ex);
    }

    // Encrypt.
    byte[] encryptedBytes = commonCrypto(cipher, info.getMessage());
    info.setMessage(encryptedBytes);

    // Save IV.
    info.setIV(cipher.getIV());

    // Generate HMAC.
    try {
      info.setHMAC(generateHMAC(info));
    } catch (NoSuchAlgorithmException e) {
      throw new CryptoException(e);
    } catch (InvalidKeyException e) {
      throw new CryptoException(e);
    }

    return info;

  }

  /*
   * Perform a decryption.
   *
   * @argument info info bundle for decryption
   *
   * @return decrypted byte[]
   *
   * @throws CryptoException
   */
  public static byte[] decrypt(CryptoInfo info) throws CryptoException {

    // Check HMAC.
    try {
      if (!verifyHMAC(info)) {
        throw new HMACVerificationException();
      }
    } catch (NoSuchAlgorithmException e) {
      throw new CryptoException(e);
    } catch (InvalidKeyException e) {
      throw new CryptoException(e);
    }

    Cipher cipher = getCipher();
    try {
      byte[] encryptionKey = info.getKeys().getEncryptionKey();
      SecretKeySpec spec = new SecretKeySpec(encryptionKey, KEY_ALGORITHM_SPEC);
      cipher.init(Cipher.DECRYPT_MODE, spec, new IvParameterSpec(info.getIV()));
    } catch (GeneralSecurityException ex) {
      ex.printStackTrace();
      throw new CryptoException(ex);
    }
    return commonCrypto(cipher, info.getMessage());
  }

  /*
   * Make 2 random 256 bit keys (encryption and HMAC).
   */
  public static KeyBundle generateKeys() throws CryptoException {
    KeyGenerator keygen;
    try {
      keygen = KeyGenerator.getInstance(KEY_ALGORITHM_SPEC);
    } catch (NoSuchAlgorithmException e) {
      e.printStackTrace();
      throw new CryptoException(e);
    }

    keygen.init(KEY_SIZE);
    byte[] encryptionKey = keygen.generateKey().getEncoded();
    byte[] hmacKey = keygen.generateKey().getEncoded();
    return new KeyBundle(encryptionKey, hmacKey);
  }

  /*
   * Performs functionality common to both the encryption and decryption
   * operations.
   *
   * Input: Cipher object, non-BaseXX-encoded byte[] input Output:
   * encrypted/decrypted byte[]
   */
  private static byte[] commonCrypto(Cipher cipher, byte[] inputMessage)
                        throws CryptoException {
    byte[] outputMessage = null;
    try {
      outputMessage = cipher.doFinal(inputMessage);
    } catch (IllegalBlockSizeException e) {
      e.printStackTrace();
      throw new CryptoException(e);
    } catch (BadPaddingException e) {
      e.printStackTrace();
      throw new CryptoException(e);
    }
    return outputMessage;
  }

  /*
   * Helper to get a Cipher object.
   * Input: None.
   * Output: Cipher object.
   */
  private static Cipher getCipher() throws CryptoException {
    Cipher cipher = null;
    try {
      cipher = Cipher.getInstance(TRANSFORMATION);
    } catch (NoSuchAlgorithmException e) {
      e.printStackTrace();
      throw new CryptoException(e);
    } catch (NoSuchPaddingException e) {
      e.printStackTrace();
      throw new CryptoException(e);
    }
    return cipher;
  }

  /*
   * Helper to verify HMAC Input: CryptoInfo Output: true if HMAC is correct
   */
  private static boolean verifyHMAC(CryptoInfo bundle) throws NoSuchAlgorithmException, InvalidKeyException {
    byte[] generatedHMAC = generateHMAC(bundle);
    byte[] expectedHMAC  = bundle.getHMAC();
    boolean eq = Arrays.equals(generatedHMAC, expectedHMAC);
    if (!eq) {
      System.err.println("Failed HMAC verification.");
      System.err.println("Expecting: " + Utils.byte2hex(generatedHMAC));
      System.err.println("Got:       " + Utils.byte2hex(expectedHMAC));
    }
    return eq;
  }

  /*
   * Helper to generate HMAC Input: CryptoInfo Output: a generated HMAC for
   * given cipher text
   */
  private static byte[] generateHMAC(CryptoInfo bundle) throws NoSuchAlgorithmException, InvalidKeyException {
    Mac hmacHasher = HKDF.makeHMACHasher(bundle.getKeys().getHMACKey());
    return hmacHasher.doFinal(Base64.encodeBase64(bundle.getMessage()));
  }

  public static byte[] sha1(String utf8) throws NoSuchAlgorithmException, UnsupportedEncodingException {
    MessageDigest sha1 = MessageDigest.getInstance("SHA-1");
    return sha1.digest(utf8.getBytes("UTF-8"));
  }

  public static String sha1Base32(String utf8) throws NoSuchAlgorithmException, UnsupportedEncodingException {
    return new Base32().encodeAsString(sha1(utf8)).toLowerCase();
  }
}
