/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.browserid;


/**
 * Java produces signature in ASN.1 format. Here's some hard-coded encoding and decoding
 * code, courtesy of a comment in
 * <a href="http://stackoverflow.com/questions/10921733/how-sign-method-of-the-digital-signature-combines-the-r-s-values-in-to-array">http://stackoverflow.com/questions/10921733/how-sign-method-of-the-digital-signature-combines-the-r-s-values-in-to-array</a>.
 */
public class ASNUtils {
  /**
   * Decode two short arrays from ASN.1 bytes.
   * @param input to extract.
   * @return length 2 array of byte arrays.
   */
  public static byte[][] decodeTwoArraysFromASN1(byte[] input) throws IllegalArgumentException {
    if (input == null) {
      throw new IllegalArgumentException("input must not be null");
    }
    if (input.length <= 3)
      throw new IllegalArgumentException("bad length");
    if (input[0] != 0x30)
      throw new IllegalArgumentException("bad encoding");
    if ((input[1] & ((byte) 0x80)) != 0)
      throw new IllegalArgumentException("bad length encoding");
    if (input[2] != 0x02)
      throw new IllegalArgumentException("bad encoding");
    if ((input[3] & ((byte) 0x80)) != 0)
      throw new IllegalArgumentException("bad length encoding");
    byte rLength = input[3];
    if (input.length <= 5 + rLength)
      throw new IllegalArgumentException("bad length");
    if (input[4 + rLength] != 0x02)
      throw new IllegalArgumentException("bad encoding");
    if ((input[5 + rLength] & (byte) 0x80) !=0)
      throw new IllegalArgumentException("bad length encoding");
    byte sLength = input[5 + rLength];
    if (input.length != 6 + sLength + rLength)
      throw new IllegalArgumentException("bad length");
    byte[] rArr = new byte[rLength];
    byte[] sArr = new byte[sLength];
    System.arraycopy(input, 4, rArr, 0, rLength);
    System.arraycopy(input, 6 + rLength, sArr, 0, sLength);
    return new byte[][] { rArr, sArr };
  }

  /**
   * Encode two short arrays into ASN.1 bytes.
   * @param first array to encode.
   * @param second array to encode.
   * @return array.
   */
  public static byte[] encodeTwoArraysToASN1(byte[] first, byte[] second) throws IllegalArgumentException {
    if (first == null) {
      throw new IllegalArgumentException("first must not be null");
    }
    if (second == null) {
      throw new IllegalArgumentException("second must not be null");
    }
    byte[] output = new byte[6 + first.length + second.length];
    output[0] = 0x30;
    if (4 + first.length + second.length > 255)
      throw new IllegalArgumentException("bad length");
    output[1] = (byte) (4 + first.length + second.length);
    if ((output[1] & ((byte) 0x80)) != 0)
      throw new IllegalArgumentException("bad length encoding");
    output[2] = 0x02;
    output[3] = (byte) first.length;
    if ((output[3] & ((byte) 0x80)) != 0)
      throw new IllegalArgumentException("bad length encoding");
    System.arraycopy(first, 0, output, 4, first.length);
    output[4 + first.length] = 0x02;
    output[5 + first.length] = (byte) second.length;
    if ((output[5 + first.length] & ((byte) 0x80)) != 0)
      throw new IllegalArgumentException("bad length encoding");
    System.arraycopy(second, 0, output, 6 + first.length, second.length);
    return output;
  }
}
