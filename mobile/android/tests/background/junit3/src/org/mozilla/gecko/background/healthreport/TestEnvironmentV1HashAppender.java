/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport;

import java.io.UnsupportedEncodingException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.LinkedList;

import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.background.healthreport.EnvironmentV1.EnvironmentAppender;
import org.mozilla.gecko.background.healthreport.EnvironmentV1.HashAppender;
import org.mozilla.gecko.background.helpers.FakeProfileTestCase;
import org.mozilla.gecko.sync.Utils;

/**
 * Tests the HashAppender functionality. Note that these tests must be run on an Android
 * device because the SHA-1 native library needs to be loaded.
 */
public class TestEnvironmentV1HashAppender extends FakeProfileTestCase {
  // input and expected values via: http://oauth.googlecode.com/svn/code/c/liboauth/src/sha1.c
  private final static String[] INPUTS = new String[] {
    "abc",
    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
    "" // To be filled in below.
  };
  static {
    final String baseStr = "01234567";
    final int repetitions = 80;
    final StringBuilder builder = new StringBuilder(baseStr.length() * repetitions);
    for (int i = 0; i < 80; ++i) {
      builder.append(baseStr);
    }
    INPUTS[2] = builder.toString();
  }

  private final static String[] EXPECTEDS = new String[] {
    "a9993e364706816aba3e25717850c26c9cd0d89d",
    "84983e441c3bd26ebaae4aa1f95129e5e54670f1",
    "dea356a2cddd90c7a7ecedc5ebb563934f460452"
  };
  static {
    for (int i = 0; i < EXPECTEDS.length; ++i) {
      EXPECTEDS[i] = new Base64(-1, null, false).encodeAsString(Utils.hex2Byte(EXPECTEDS[i]));
    }
  }

  public void testSHA1Hashing() throws Exception {
    for (int i = 0; i < INPUTS.length; ++i) {
      final String input = INPUTS[i];
      final String expected = EXPECTEDS[i];

      final HashAppender appender = new HashAppender();
      addStringToAppenderInParts(appender, input);
      final String result = appender.toString();

      assertEquals(expected, result);
    }
  }

  /**
   * Tests to ensure output is the same as the former MessageDigest implementation (bug 959652).
   */
  public void testAgainstMessageDigestImpl() throws Exception {
    // List.add doesn't allow add(null) so we make a LinkedList here.
    final LinkedList<String> inputs = new LinkedList<String>(Arrays.asList(INPUTS));
    inputs.add(null);

    for (final String input : inputs) {
      final HashAppender hAppender = new HashAppender();
      final MessageDigestHashAppender mdAppender = new MessageDigestHashAppender();

      hAppender.append(input);
      mdAppender.append(input);

      final String hResult = hAppender.toString();
      final String mdResult = mdAppender.toString();
      assertEquals(mdResult, hResult);
    }
  }

  public void testIntegersAgainstMessageDigestImpl() throws Exception {
    final int[] INPUTS = {Integer.MIN_VALUE, -1337, -42, 0, 42, 1337, Integer.MAX_VALUE};
    for (final int input : INPUTS) {
      final HashAppender hAppender = new HashAppender();
      final MessageDigestHashAppender mdAppender = new MessageDigestHashAppender();

      hAppender.append(input);
      mdAppender.append(input);

      final String hResult = hAppender.toString();
      final String mdResult = mdAppender.toString();
      assertEquals(mdResult, hResult);
    }
  }

  private void addStringToAppenderInParts(final EnvironmentAppender appender, final String input) {
    int substrInd = 0;
    int substrLength = 1;
    while (substrInd < input.length()) {
      final int endInd = Math.min(substrInd + substrLength, input.length());

      appender.append(input.substring(substrInd, endInd));

      substrInd = endInd;
      ++substrLength;
    }
  }

  // --- COPY-PASTA'D CODE, FOR TESTING PURPOSES. ---
  public static class MessageDigestHashAppender extends EnvironmentAppender {
    final MessageDigest hasher;

    public MessageDigestHashAppender() throws NoSuchAlgorithmException {
      // Note to the security-minded reader: we deliberately use SHA-1 here, not
      // a stronger hash. These identifiers don't strictly need a cryptographic
      // hash function, because there is negligible value in attacking the hash.
      // We use SHA-1 because it's *shorter* -- the exact same reason that Git
      // chose SHA-1.
      hasher = MessageDigest.getInstance("SHA-1");
    }

    @Override
    public void append(String s) {
      try {
        hasher.update(((s == null) ? "null" : s).getBytes("UTF-8"));
      } catch (UnsupportedEncodingException e) {
        // This can never occur. Thanks, Java.
      }
    }

    @Override
    public void append(int profileCreation) {
      append(Integer.toString(profileCreation, 10));
    }

    @Override
    public String toString() {
      // We *could* use ASCII85… but the savings would be negated by the
      // inclusion of JSON-unsafe characters like double-quote.
      return new Base64(-1, null, false).encodeAsString(hasher.digest());
    }
  }
}
