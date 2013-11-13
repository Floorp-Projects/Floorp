/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa;

import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.security.NoSuchAlgorithmException;

import org.mozilla.gecko.sync.Utils;

public class FxAccountUtils {
  public static final int SALT_LENGTH_BYTES = 32;
  public static final int SALT_LENGTH_HEX = 2 * SALT_LENGTH_BYTES;

  public static final int HASH_LENGTH_BYTES = 16;
  public static final int HASH_LENGTH_HEX = 2 * HASH_LENGTH_BYTES;

  public static final String KW_VERSION_STRING = "identity.mozilla.com/picl/v1/";

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
}
