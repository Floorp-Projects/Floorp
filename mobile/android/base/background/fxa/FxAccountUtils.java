/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa;

import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.security.GeneralSecurityException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;

import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.crypto.HKDF;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.crypto.PBKDF2;

public class FxAccountUtils {
  public static final int SALT_LENGTH_BYTES = 32;
  public static final int SALT_LENGTH_HEX = 2 * SALT_LENGTH_BYTES;

  public static final int HASH_LENGTH_BYTES = 16;
  public static final int HASH_LENGTH_HEX = 2 * HASH_LENGTH_BYTES;

  public static final String KW_VERSION_STRING = "identity.mozilla.com/picl/v1/";

  public static final int NUMBER_OF_QUICK_STRETCH_ROUNDS = 1000;

  public static String bytes(String string) throws UnsupportedEncodingException {
    return Utils.byte2Hex(string.getBytes("UTF-8"));
  }

  public static byte[] KW(String name) throws UnsupportedEncodingException {
    return Utils.concatAll(
        KW_VERSION_STRING.getBytes("UTF-8"),
        name.getBytes("UTF-8"));
  }

  public static byte[] KWE(String name, byte[] emailUTF8) throws UnsupportedEncodingException {
    return Utils.concatAll(
        KW_VERSION_STRING.getBytes("UTF-8"),
        name.getBytes("UTF-8"),
        ":".getBytes("UTF-8"),
        emailUTF8);
  }

  /**
   * Calculate the SRP verifier <tt>x</tt> value.
   */
  public static BigInteger srpVerifierLowercaseX(byte[] emailUTF8, byte[] srpPWBytes, byte[] srpSaltBytes)
      throws NoSuchAlgorithmException, UnsupportedEncodingException {
    byte[] inner = Utils.sha256(Utils.concatAll(emailUTF8, ":".getBytes("UTF-8"), srpPWBytes));
    byte[] outer = Utils.sha256(Utils.concatAll(srpSaltBytes, inner));
    return new BigInteger(1, outer);
  }

  /**
   * Calculate the SRP verifier <tt>v</tt> value.
   */
  public static BigInteger srpVerifierLowercaseV(byte[] emailUTF8, byte[] srpPWBytes, byte[] srpSaltBytes, BigInteger g, BigInteger N)
      throws NoSuchAlgorithmException, UnsupportedEncodingException {
    BigInteger x = srpVerifierLowercaseX(emailUTF8, srpPWBytes, srpSaltBytes);
    BigInteger v = g.modPow(x, N);
    return v;
  }

  /**
   * Format x modulo N in hexadecimal, using as many characters as N takes (in hexadecimal).
   * @param x to format.
   * @param N modulus.
   * @return x modulo N in hexadecimal.
   */
  public static String hexModN(BigInteger x, BigInteger N) {
    int byteLength = (N.bitLength() + 7) / 8;
    int hexLength = 2 * byteLength;
    return Utils.byte2Hex(Utils.hex2Byte((x.mod(N)).toString(16), byteLength), hexLength);
  }

  /**
   * The first engineering milestone of PICL (Profile-in-the-Cloud) was
   * comprised of Sync 1.1 fronted by a Firefox Account. The sync key was
   * generated from the Firefox Account password-derived kB value using this
   * method.
   */
  public static KeyBundle generateSyncKeyBundle(final byte[] kB) throws InvalidKeyException, NoSuchAlgorithmException, UnsupportedEncodingException {
    byte[] encryptionKey = new byte[32];
    byte[] hmacKey = new byte[32];
    byte[] derived = HKDF.derive(kB, new byte[0], FxAccountUtils.KW("oldsync"), 2*32);
    System.arraycopy(derived, 0*32, encryptionKey, 0, 1*32);
    System.arraycopy(derived, 1*32, hmacKey, 0, 1*32);
    return new KeyBundle(encryptionKey, hmacKey);
  }

  /**
   * Firefox Accounts are password authenticated, but clients should not store
   * the plain-text password for any amount of time. Equivalent, but slightly
   * more secure, is the quickly client-side stretched password.
   * <p>
   * We separate this since multiple login-time operations want it, and the
   * PBKDF2 operation is computationally expensive.
   */
  public static byte[] generateQuickStretchedPW(byte[] emailUTF8, byte[] passwordUTF8) throws GeneralSecurityException, UnsupportedEncodingException {
    byte[] S = FxAccountUtils.KWE("quickStretch", emailUTF8);
    return PBKDF2.pbkdf2SHA256(passwordUTF8, S, NUMBER_OF_QUICK_STRETCH_ROUNDS, 32);
  }

  /**
   * The password-derived credential used to authenticate to the Firefox Account
   * auth server.
   */
  public static byte[] generateAuthPW(byte[] quickStretchedPW) throws GeneralSecurityException, UnsupportedEncodingException {
    return HKDF.derive(quickStretchedPW, new byte[0], FxAccountUtils.KW("authPW"), 32);
  }

  /**
   * The password-derived credential used to unwrap keys managed by the Firefox
   * Account auth server.
   */
  public static byte[] generateUnwrapBKey(byte[] quickStretchedPW) throws GeneralSecurityException, UnsupportedEncodingException {
    return HKDF.derive(quickStretchedPW, new byte[0], FxAccountUtils.KW("unwrapBkey"), 32);
  }
}
