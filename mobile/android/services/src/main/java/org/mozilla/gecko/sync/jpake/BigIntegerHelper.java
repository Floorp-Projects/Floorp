/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake;

import java.math.BigInteger;

public class BigIntegerHelper {

  public static byte[] BigIntegerToByteArrayWithoutSign(BigInteger value) {
    byte[] bytes = value.toByteArray();
    if (bytes[0] == (byte) 0) {
      bytes = copyArray(bytes, 1, bytes.length - 1);
    }
    return bytes;
  }

  private static byte[] copyArray(byte[] original, int start, int length) {
    byte[] copy = new byte[length];
    System.arraycopy(original, start, copy, 0,
        Math.min(original.length - start, length));
    return copy;
  }

  /**
   * Convert an array of bytes to a non-negative big integer.
   */
  public static BigInteger ByteArrayToBigIntegerWithoutSign(byte[] array) {
    return new BigInteger(1, array);
  }

  /**
   * Convert a big integer into hex string. If the length is not even, add an
   * '0' character in the beginning to make it even.
   */
  public static String toEvenLengthHex(BigInteger value) {
    String result = value.toString(16);
    if (result.length() % 2 != 0) {
      result = "0" + result;
    }
    return result;
  }
}
