/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertArrayEquals;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertEquals;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertNotNull;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fFail;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import org.mozilla.gecko.background.nativecode.NativeCrypto;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.tests.helpers.GeckoHelper;

import android.os.SystemClock;

/**
 * Tests the Java wrapper over native implementations of crypto code. Test vectors from:
 *   * PBKDF2SHA256:
 *     - <https://github.com/ircmaxell/PHP-PasswordLib/blob/master/test/Data/Vectors/pbkdf2-draft-josefsson-sha256.test-vectors>
       - <https://gitorious.org/scrypt/nettle-scrypt/blobs/37c0d5288e991604fe33dba2f1724986a8dddf56/testsuite/pbkdf2-test.c>
     * SHA-1:
       - <http://oauth.googlecode.com/svn/code/c/liboauth/src/sha1.c>
 */
public class testNativeCrypto extends UITest {
  private final static String LOGTAG = "testNativeCrypto";

  /**
   * Robocop supports only a single test function per test class. Therefore, we
   * have a single top-level test function that dispatches to sub-tests,
   * accepting that we might fail part way through the cycle. Proper JUnit 3
   * testing can't land soon enough!
   *
   * @throws Exception
   */
  public void test() throws Exception {
    // This test could complete very quickly. If it completes too soon, the
    // minidumps directory may not be created before the process is
    // taken down, causing bug 722166. But we can't run the test and then block
    // for Gecko:Ready, since it may have arrived before we block. So we wait.
    // Again, JUnit 3 can't land soon enough!
    GeckoHelper.blockForReady();

    _testPBKDF2SHA256A();
    _testPBKDF2SHA256B();
    _testPBKDF2SHA256C();
    _testPBKDF2SHA256scryptA();
    _testPBKDF2SHA256scryptB();
    _testPBKDF2SHA256InvalidLenArg();

    _testSHA1();
    _testSHA1AgainstMessageDigest();

    _testSHA256();
    _testSHA256MultiPart();
    _testSHA256AgainstMessageDigest();
    _testSHA256WithMultipleUpdatesFromStream();
  }

  public void _testPBKDF2SHA256A() throws UnsupportedEncodingException, GeneralSecurityException {
    final String  p = "password";
    final String  s = "salt";
    final int dkLen = 32;

    checkPBKDF2SHA256(p, s, 1, dkLen, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    checkPBKDF2SHA256(p, s, 4096, dkLen, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
  }

  public void _testPBKDF2SHA256B() throws UnsupportedEncodingException, GeneralSecurityException {
    final String  p = "passwordPASSWORDpassword";
    final String  s = "saltSALTsaltSALTsaltSALTsaltSALTsalt";
    final int dkLen = 40;

    checkPBKDF2SHA256(p, s, 4096, dkLen, "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9");
  }

  public void _testPBKDF2SHA256scryptA() throws UnsupportedEncodingException, GeneralSecurityException {
    final String  p = "passwd";
    final String  s = "salt";
    final int dkLen = 64;

    checkPBKDF2SHA256(p, s, 1, dkLen, "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
  }

  public void _testPBKDF2SHA256scryptB() throws UnsupportedEncodingException, GeneralSecurityException {
    final String  p = "Password";
    final String  s = "NaCl";
    final int dkLen = 64;

    checkPBKDF2SHA256(p, s, 80000, dkLen, "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d");
  }

  public void _testPBKDF2SHA256C() throws UnsupportedEncodingException, GeneralSecurityException {
    final String  p = "pass\0word";
    final String  s = "sa\0lt";
    final int dkLen = 16;

    checkPBKDF2SHA256(p, s, 4096, dkLen, "89b69d0516f829893c696226650a8687");
  }

  public void _testPBKDF2SHA256InvalidLenArg() throws UnsupportedEncodingException, GeneralSecurityException {
    final String p = "password";
    final String s = "salt";
    final int c = 1;
    final int dkLen = -1; // Should always be positive.

    try {
      final byte[] key = NativeCrypto.pbkdf2SHA256(p.getBytes("US-ASCII"), s.getBytes("US-ASCII"), c, dkLen);
      fFail("Expected sha256 to throw with negative dkLen argument.");
    } catch (IllegalArgumentException e) { } // Expected.
  }

  private void _testSHA1() throws UnsupportedEncodingException {
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
      fAssertNotNull("Hashed value is non-null", actual);
      assertExpectedBytes(expected, actual);
    }
  }

  /**
   * Test to ensure the output of our SHA1 algo is the same as MessageDigest's. This is important
   * because we intend to replace MessageDigest in FHR with this SHA-1 algo (bug 959652).
   */
  private void _testSHA1AgainstMessageDigest() throws UnsupportedEncodingException,
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
      fAssertArrayEquals("MessageDigest hash is the same as NativeCrypto SHA-1 hash", mdBytes, ourBytes);
    }
  }

  private void _testSHA256() throws UnsupportedEncodingException {
    final String[] inputs = new String[] {
      "abc",
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
      "" // To be filled in below.
    };
    final String baseStr = "01234567";
    final int repetitions = 80;
    final StringBuilder builder = new StringBuilder(baseStr.length() * repetitions);
    for (int i = 0; i < repetitions; ++i) {
      builder.append(baseStr);
    }
    inputs[2] = builder.toString();

    final String[] expecteds = new String[] {
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
      "594847328451bdfa85056225462cc1d867d877fb388df0ce35f25ab5562bfbb5"
    };

    for (int i = 0; i < inputs.length; ++i) {
      final byte[] input = inputs[i].getBytes("US-ASCII");
      final String expected = expecteds[i];

      final byte[] ctx = NativeCrypto.sha256init();
      NativeCrypto.sha256update(ctx, input, input.length);
      final byte[] actual = NativeCrypto.sha256finalize(ctx);
      fAssertNotNull("Hashed value is non-null", actual);
      assertExpectedBytes(expected, actual);
    }
  }

  private void _testSHA256MultiPart() throws UnsupportedEncodingException {
    final String input = "01234567";
    final int repetitions = 80;
    final String expected = "594847328451bdfa85056225462cc1d867d877fb388df0ce35f25ab5562bfbb5";

    final byte[] inputBytes = input.getBytes("US-ASCII");
    final byte[] ctx = NativeCrypto.sha256init();
    for (int i = 0; i < repetitions; ++i) {
      NativeCrypto.sha256update(ctx, inputBytes, inputBytes.length);
    }
    final byte[] actual = NativeCrypto.sha256finalize(ctx);
    fAssertNotNull("Hashed value is non-null", actual);
    assertExpectedBytes(expected, actual);
  }

  private void _testSHA256AgainstMessageDigest() throws UnsupportedEncodingException,
      NoSuchAlgorithmException {
    final String[] inputs = {
      "password",
      "saranghae",
      "aoeusnthaoeusnthaoeusnth \0 12345098765432109876_!"
    };

    final MessageDigest digest = MessageDigest.getInstance("SHA-256");
    for (final String input : inputs) {
      final byte[] inputBytes = input.getBytes("US-ASCII");

      final byte[] mdBytes = digest.digest(inputBytes);

      final byte[] ctx = NativeCrypto.sha256init();
      NativeCrypto.sha256update(ctx, inputBytes, inputBytes.length);
      final byte[] ourBytes = NativeCrypto.sha256finalize(ctx);
      fAssertArrayEquals("MessageDigest hash is the same as NativeCrypto SHA-256 hash", mdBytes, ourBytes);
    }
  }

  private void _testSHA256WithMultipleUpdatesFromStream() throws UnsupportedEncodingException {
    final String input = "HelloWorldThisIsASuperLongStringThatIsReadAsAStreamOfBytes";
    final ByteArrayInputStream stream = new ByteArrayInputStream(input.getBytes("UTF-8"));
    final String expected = "8b5cb76b80f7eb6fb83ee138bfd31e2922e71dd245daa21a8d9876e8dee9eef5";

    byte[] buffer = new byte[10];
    final byte[] ctx = NativeCrypto.sha256init();
    int c;

    try {
      while ((c = stream.read(buffer)) != -1) {
        NativeCrypto.sha256update(ctx, buffer, c);
      }
      final byte[] actual = NativeCrypto.sha256finalize(ctx);
      fAssertNotNull("Hashed value is non-null", actual);
      assertExpectedBytes(expected, actual);
    } catch (IOException e) {
      fFail("IOException while reading stream");
    }
  }

  private void checkPBKDF2SHA256(String p, String s, int c, int dkLen, final String expectedStr)
      throws GeneralSecurityException, UnsupportedEncodingException {
    final long start = SystemClock.elapsedRealtime();

    final byte[] key = NativeCrypto.pbkdf2SHA256(p.getBytes("US-ASCII"), s.getBytes("US-ASCII"), c, dkLen);
    fAssertNotNull("Hash result is non-null", key);

    final long end = SystemClock.elapsedRealtime();
    dumpLog(LOGTAG, "SHA-256 " + c + " took " + (end - start) + "ms");

    if (expectedStr == null) {
      return;
    }

    fAssertEquals("Hash result is the appropriate length", dkLen,
        Utils.hex2Byte(expectedStr).length);
    assertExpectedBytes(expectedStr, key);
  }

  private void assertExpectedBytes(final String expectedStr, byte[] key) {
    fAssertEquals("Expected string matches hash result", expectedStr, Utils.byte2Hex(key));
    final byte[] expected = Utils.hex2Byte(expectedStr);

    fAssertEquals("Expected byte array length matches key length", expected.length, key.length);
    fAssertArrayEquals("Expected byte array matches key byte array", expected, key);
  }
}
