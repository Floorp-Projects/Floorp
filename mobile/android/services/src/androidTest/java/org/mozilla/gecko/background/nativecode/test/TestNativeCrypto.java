/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.nativecode.test;

import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

import junit.framework.TestCase;

import org.mozilla.gecko.background.nativecode.NativeCrypto;
import org.mozilla.gecko.sync.Utils;

/*
 * Tests the Java wrapper over native implementations of crypto code. Test vectors from:
 *   * PBKDF2SHA256:
 *     - <https://github.com/ircmaxell/PHP-PasswordLib/blob/master/test/Data/Vectors/pbkdf2-draft-josefsson-sha256.test-vectors>
 *     - <https://gitorious.org/scrypt/nettle-scrypt/blobs/37c0d5288e991604fe33dba2f1724986a8dddf56/testsuite/pbkdf2-test.c>
 *   * SHA-1:
 *     - <http://oauth.googlecode.com/svn/code/c/liboauth/src/sha1.c>
 */
public class TestNativeCrypto extends TestCase {

  public final void testPBKDF2SHA256A() throws UnsupportedEncodingException, GeneralSecurityException {
    String  p = "password";
    String  s = "salt";
    int dkLen = 32;

    checkPBKDF2SHA256(p, s, 1, dkLen, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    checkPBKDF2SHA256(p, s, 4096, dkLen, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
  }

  public final void testPBKDF2SHA256B() throws UnsupportedEncodingException, GeneralSecurityException {
    String  p = "passwordPASSWORDpassword";
    String  s = "saltSALTsaltSALTsaltSALTsaltSALTsalt";
    int dkLen = 40;

    checkPBKDF2SHA256(p, s, 4096, dkLen, "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9");
  }

  public final void testPBKDF2SHA256scryptA() throws UnsupportedEncodingException, GeneralSecurityException {
    String  p = "passwd";
    String  s = "salt";
    int dkLen = 64;

    checkPBKDF2SHA256(p, s, 1, dkLen, "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
  }

  public final void testPBKDF2SHA256scryptB() throws UnsupportedEncodingException, GeneralSecurityException {
    String  p = "Password";
    String  s = "NaCl";
    int dkLen = 64;

    checkPBKDF2SHA256(p, s, 80000, dkLen, "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d");
  }

  public final void testPBKDF2SHA256C() throws UnsupportedEncodingException, GeneralSecurityException {
    String  p = "pass\0word";
    String  s = "sa\0lt";
    int dkLen = 16;

    checkPBKDF2SHA256(p, s, 4096, dkLen, "89b69d0516f829893c696226650a8687");
  }

  /*
  // This test takes two or three minutes to run, so we don't.
  public final void testPBKDF2SHA256D() throws UnsupportedEncodingException, GeneralSecurityException {
    String  p = "password";
    String  s = "salt";
    int dkLen = 32;

    checkPBKDF2SHA256(p, s, 16777216, dkLen, "cf81c66fe8cfc04d1f31ecb65dab4089f7f179e89b3b0bcb17ad10e3ac6eba46");
  }
  */

  public final void testTimePBKDF2SHA256() throws UnsupportedEncodingException, GeneralSecurityException {
    checkPBKDF2SHA256("password", "salt", 80000, 32, null);
  }

  public final void testPBKDF2SHA256InvalidLenArg() throws UnsupportedEncodingException, GeneralSecurityException {
    final String p = "password";
    final String s = "salt";
    final int c = 1;
    final int dkLen = -1; // Should always be positive.

    try {
      NativeCrypto.pbkdf2SHA256(p.getBytes("US-ASCII"), s.getBytes("US-ASCII"), c, dkLen);
      fail("Expected sha256 to throw with negative dkLen argument.");
    } catch (IllegalArgumentException e) { } // Expected.
  }

  public final void testSHA1() throws UnsupportedEncodingException {
    final String[] inputs = new String[] {
      "abc",
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
      "" // To be filled in below.
    };
    final String baseStr = "01234567";
    final int repetitions = 80;
    final StringBuilder builder = new StringBuilder(baseStr.length() * repetitions);
    for (int i = 0; i < 80; ++i) {
      builder.append(baseStr);
    }
    inputs[2] = builder.toString();

    final String[] expecteds = new String[] {
      "a9993e364706816aba3e25717850c26c9cd0d89d",
      "84983e441c3bd26ebaae4aa1f95129e5e54670f1",
      "dea356a2cddd90c7a7ecedc5ebb563934f460452"
    };

    for (int i = 0; i < inputs.length; ++i) {
      final byte[] input = inputs[i].getBytes("US-ASCII");
      final String expected = expecteds[i];

      final byte[] actual = NativeCrypto.sha1(input);
      assertNotNull("Hashed value is non-null", actual);
      assertExpectedBytes(expected, actual);
    }
  }

  /**
   * Test to ensure the output of our SHA1 algo is the same as MessageDigest's. This is important
   * because we intend to replace MessageDigest in FHR with this SHA-1 algo (bug 959652).
   */
  public final void testSHA1AgainstMessageDigest() throws UnsupportedEncodingException,
      NoSuchAlgorithmException {
    final String[] inputs = {
      "password",
      "saranghae",
      "aoeusnthaoeusnthaoeusnth \0 12345098765432109876_!"
    };

    final MessageDigest digest = MessageDigest.getInstance("SHA-1");
    for (final String input : inputs) {
      final byte[] inputBytes = input.getBytes("US-ASCII");

      final byte[] mdBytes = digest.digest(inputBytes);
      final byte[] ourBytes = NativeCrypto.sha1(inputBytes);
      assertTrue("MessageDigest hash is the same as NativeCrypto SHA-1 hash",
          Arrays.equals(ourBytes, mdBytes));
    }
  }

  private void checkPBKDF2SHA256(String p, String s, int c, int dkLen,
                                final String expectedStr)
                                                    throws GeneralSecurityException, UnsupportedEncodingException {
    long start = System.currentTimeMillis();
    byte[] key = NativeCrypto.pbkdf2SHA256(p.getBytes("US-ASCII"), s.getBytes("US-ASCII"), c, dkLen);
    assertNotNull(key);

    long end = System.currentTimeMillis();

    System.err.println("SHA-256 " + c + " took " + (end - start) + "ms");
    if (expectedStr == null) {
      return;
    }

    assertEquals(dkLen, Utils.hex2Byte(expectedStr).length);
    assertExpectedBytes(expectedStr, key);
  }

  private void assertExpectedBytes(final String expectedStr, byte[] key) {
    assertEquals(expectedStr, Utils.byte2Hex(key));
    byte[] expected = Utils.hex2Byte(expectedStr);

    assertEquals(expected.length, key.length);
    for (int i = 0; i < key.length; i++) {
      assertEquals(expected[i], key[i]);
    }
  }
}
