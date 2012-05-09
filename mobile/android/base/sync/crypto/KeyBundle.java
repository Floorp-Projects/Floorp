/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.crypto;

import java.io.UnsupportedEncodingException;
import java.security.NoSuchAlgorithmException;

import javax.crypto.KeyGenerator;
import javax.crypto.Mac;

import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.sync.Utils;
import java.security.InvalidKeyException;
import java.util.Arrays;
import java.util.Locale;

public class KeyBundle {
    private static final String KEY_ALGORITHM_SPEC = "AES";
    private static final int    KEY_SIZE           = 256;

    private byte[] encryptionKey;
    private byte[] hmacKey;

    // These are the same for every sync key bundle.
    private static final byte[] EMPTY_BYTES      = {};
    private static final byte[] ENCR_INPUT_BYTES = {1};
    private static final byte[] HMAC_INPUT_BYTES = {2};

    /**
     * If we encounter characters not allowed by the API (as found for
     * instance in an email address), hash the value.
     * @param account
     *        An account string.
     * @return
     *        An acceptable string.
     * @throws UnsupportedEncodingException
     * @throws NoSuchAlgorithmException
     */
    public static String usernameFromAccount(String account) throws NoSuchAlgorithmException, UnsupportedEncodingException {
      if (account == null || account.equals("")) {
        throw new IllegalArgumentException("No account name provided.");
      }
      if (account.matches("^[A-Za-z0-9._-]+$")) {
        return account.toLowerCase(Locale.US);
      }
      return Utils.sha1Base32(account.toLowerCase(Locale.US));
    }

    // If we encounter characters not allowed by the API (as found for
    // instance in an email address), hash the value.

    /*
     * Mozilla's use of HKDF for getting keys from the Sync Key string.
     *
     * We do exactly 2 HKDF iterations and make the first iteration the
     * encryption key and the second iteration the HMAC key.
     *
     */
    public KeyBundle(String username, String base32SyncKey) throws CryptoException {
      if (base32SyncKey == null) {
        throw new IllegalArgumentException("No sync key provided.");
      }
      if (username == null || username.equals("")) {
        throw new IllegalArgumentException("No username provided.");
      }
      // Hash appropriately.
      try {
        username = usernameFromAccount(username);
      } catch (NoSuchAlgorithmException e) {
        throw new IllegalArgumentException("Invalid username.");
      } catch (UnsupportedEncodingException e) {
        throw new IllegalArgumentException("Invalid username.");
      }

      byte[] syncKey = Utils.decodeFriendlyBase32(base32SyncKey);
      byte[] user    = username.getBytes();

      Mac hmacHasher;
      try {
        hmacHasher = HKDF.makeHMACHasher(syncKey);
      } catch (NoSuchAlgorithmException e) {
        throw new CryptoException(e);
      } catch (InvalidKeyException e) {
        throw new CryptoException(e);
      }
      assert(hmacHasher != null); // If makeHMACHasher doesn't throw, then hmacHasher is non-null.

      byte[] encrBytes = Utils.concatAll(EMPTY_BYTES, HKDF.HMAC_INPUT, user, ENCR_INPUT_BYTES);
      byte[] encrKey   = HKDF.digestBytes(encrBytes, hmacHasher);
      byte[] hmacBytes = Utils.concatAll(encrKey, HKDF.HMAC_INPUT, user, HMAC_INPUT_BYTES);

      this.hmacKey       = HKDF.digestBytes(hmacBytes, hmacHasher);
      this.encryptionKey = encrKey;
    }

    public KeyBundle(byte[] encryptionKey, byte[] hmacKey) {
       this.setEncryptionKey(encryptionKey);
       this.setHMACKey(hmacKey);
    }

    /**
     * Make a KeyBundle with the specified base64-encoded keys.
     *
     * @return A KeyBundle with the specified keys.
     */
    public static KeyBundle fromBase64EncodedKeys(String base64EncryptionKey, String base64HmacKey) throws UnsupportedEncodingException {
      return new KeyBundle(Base64.decodeBase64(base64EncryptionKey.getBytes("UTF-8")),
                           Base64.decodeBase64(base64HmacKey.getBytes("UTF-8")));
    }

    /**
     * Make a KeyBundle with two random 256 bit keys (encryption and HMAC).
     *
     * @return A KeyBundle with random keys.
     */
    public static KeyBundle withRandomKeys() throws CryptoException {
      KeyGenerator keygen;
      try {
        keygen = KeyGenerator.getInstance(KEY_ALGORITHM_SPEC);
      } catch (NoSuchAlgorithmException e) {
        throw new CryptoException(e);
      }

      keygen.init(KEY_SIZE);
      byte[] encryptionKey = keygen.generateKey().getEncoded();
      byte[] hmacKey = keygen.generateKey().getEncoded();

      return new KeyBundle(encryptionKey, hmacKey);
    }

    public byte[] getEncryptionKey() {
        return encryptionKey;
    }

    public void setEncryptionKey(byte[] encryptionKey) {
        this.encryptionKey = encryptionKey;
    }

    public byte[] getHMACKey() {
        return hmacKey;
    }

    public void setHMACKey(byte[] hmacKey) {
        this.hmacKey = hmacKey;
    }

    @Override
    public boolean equals(Object o) {
      if (!(o instanceof KeyBundle)) {
        return false;
      }
      KeyBundle other = (KeyBundle) o;
      return Arrays.equals(other.encryptionKey, this.encryptionKey) &&
             Arrays.equals(other.hmacKey, this.hmacKey);
    }
}
