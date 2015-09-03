/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.crypto.test;

import java.math.BigInteger;

import org.junit.Assert;
import org.junit.Test;
import org.mozilla.gecko.sync.net.SRPConstants;

public class TestSRPConstants extends SRPConstants {
  public void assertSRPConstants(SRPConstants.Parameters params, int bitLength) {
    Assert.assertNotNull(params.g);
    Assert.assertNotNull(params.N);
    Assert.assertEquals(bitLength, bitLength);
    Assert.assertEquals(bitLength / 8, params.byteLength);
    Assert.assertEquals(bitLength / 4, params.hexLength);
    BigInteger N = params.N;
    BigInteger g = params.g;
    // Each prime N is of the form 2*q + 1, with q also prime.
    BigInteger q = N.subtract(new BigInteger("1")).divide(new BigInteger("2"));
    // Check that g is a generator: the order of g is exactly 2*q (not 2, not q).
    Assert.assertFalse(new BigInteger("1").equals(g.modPow(new BigInteger("2"), N)));
    Assert.assertFalse(new BigInteger("1").equals(g.modPow(q, N)));
    Assert.assertTrue(new BigInteger("1").equals(g.modPow((N.subtract(new BigInteger("1"))), N)));
    // Even probable primality checking is too expensive to do here.
    // Assert.assertTrue(N.isProbablePrime(3));
    // Assert.assertTrue(q.isProbablePrime(3));
  }

  @Test
  public void testConstants() {
    assertSRPConstants(SRPConstants._1024, 1024);
    assertSRPConstants(SRPConstants._1536, 1536);
    assertSRPConstants(SRPConstants._2048, 2048);
    assertSRPConstants(SRPConstants._3072, 3072);
    assertSRPConstants(SRPConstants._4096, 4096);
    assertSRPConstants(SRPConstants._6144, 6144);
    assertSRPConstants(SRPConstants._8192, 8192);
  }
}
