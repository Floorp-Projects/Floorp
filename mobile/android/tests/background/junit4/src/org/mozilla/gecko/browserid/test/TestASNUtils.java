/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.browserid.test;

import java.math.BigInteger;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.browserid.ASNUtils;
import org.mozilla.gecko.sync.Utils;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestASNUtils {
  public void doTestEncodeDecodeArrays(int length1, int length2) {
    if (4 + length1 + length2 > 127) {
      throw new IllegalArgumentException("Total length must be < 128 - 4.");
    }
    byte[] first = Utils.generateRandomBytes(length1);
    byte[] second = Utils.generateRandomBytes(length2);
    byte[] encoded = ASNUtils.encodeTwoArraysToASN1(first, second);
    byte[][] arrays = ASNUtils.decodeTwoArraysFromASN1(encoded);
    Assert.assertArrayEquals(first, arrays[0]);
    Assert.assertArrayEquals(second, arrays[1]);
  }

  @Test
  public void testEncodeDecodeArrays() {
    doTestEncodeDecodeArrays(0, 0);
    doTestEncodeDecodeArrays(0, 10);
    doTestEncodeDecodeArrays(10, 0);
    doTestEncodeDecodeArrays(10, 10);
  }

  @Test
  public void testEncodeDecodeRandomSizeArrays() {
    for (int i = 0; i < 10; i++) {
      int length1 = Utils.generateBigIntegerLessThan(BigInteger.valueOf(50)).intValue() + 10;
      int length2 = Utils.generateBigIntegerLessThan(BigInteger.valueOf(50)).intValue() + 10;
      doTestEncodeDecodeArrays(length1, length2);
    }
  }
}
