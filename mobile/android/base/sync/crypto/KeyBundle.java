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
 *  Jason Voll <jvoll@mozilla.com>
 *  Richard Newman <rnewman@mozilla.com>
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
import java.security.NoSuchAlgorithmException;

import javax.crypto.KeyGenerator;
import javax.crypto.Mac;

import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.sync.Utils;
import java.security.InvalidKeyException;
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
}
