/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.crypto.test;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.apache.commons.codec.binary.Base32;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.Utils;

import java.io.UnsupportedEncodingException;
import java.util.Arrays;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

@RunWith(TestRunner.class)
public class TestBase32 {

  public static void assertSame(byte[] arrayOne, byte[] arrayTwo) {
    assertTrue(Arrays.equals(arrayOne, arrayTwo));
  }

  @Test
  public void testBase32() throws UnsupportedEncodingException {
    byte[] decoded  = new Base32().decode("MZXW6YTBOI======");
    byte[] expected = "foobar".getBytes();
    assertSame(decoded, expected);

    byte[] encoded = new Base32().encode("fooba".getBytes());
    expected = "MZXW6YTB".getBytes();
    assertSame(encoded, expected);
  }

  @Test
  public void testFriendlyBase32() {
    // These checks are drawn from Firefox, test_utils_encodeBase32.js.
    byte[] decoded  = Utils.decodeFriendlyBase32("mzxw6ytb9jrgcztpn5rgc4tcme");
    byte[] expected = "foobarbafoobarba".getBytes();
    assertEquals(decoded.length, 16);
    assertSame(decoded, expected);

    // These are real values extracted from the Service object in a Firefox profile.
    String base32Key   = "6m8mv8ex2brqnrmsb9fjuvfg7y";
    String expectedHex = "f316caac97d06306c5920b8a9a54a6fe";

    byte[] computedBytes = Utils.decodeFriendlyBase32(base32Key);
    byte[] expectedBytes = Utils.hex2Byte(expectedHex);

    assertSame(computedBytes, expectedBytes);
  }
}
