/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.UnsupportedEncodingException;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.sync.SyncConstants;
import org.mozilla.gecko.sync.Utils;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestUtils extends Utils {

  @Test
  public void testGenerateGUID() {
    for (int i = 0; i < 1000; ++i) {
      assertEquals(12, Utils.generateGuid().length());
    }
  }

  public static final byte[][] BYTE_ARRS = {
    new byte[] {'	'}, // Tab.
    new byte[] {'0'},
    new byte[] {'A'},
    new byte[] {'a'},
    new byte[] {'I', 'U'},
    new byte[] {'`', 'h', 'g', ' ', 's', '`'},
    new byte[] {}
  };
  // Indices correspond with the above array.
  public static final String[] STRING_ARR = {
    "09",
    "30",
    "41",
    "61",
    "4955",
    "606867207360",
    ""
  };

  @Test
  public void testByte2Hex() throws Exception {
    for (int i = 0; i < BYTE_ARRS.length; ++i) {
      final byte[] b = BYTE_ARRS[i];
      final String expected = STRING_ARR[i];
      assertEquals(expected, Utils.byte2Hex(b));
    }
  }

  @Test
  public void testHex2Byte() throws Exception {
    for (int i = 0; i < STRING_ARR.length; ++i) {
      final String s = STRING_ARR[i];
      final byte[] expected = BYTE_ARRS[i];
      assertTrue(Arrays.equals(expected, Utils.hex2Byte(s)));
    }
  }

  @Test
  public void testByte2Hex2ByteAndViceVersa() throws Exception { // There and back again!
    for (int i = 0; i < BYTE_ARRS.length; ++i) {
      // byte2Hex2Byte
      final byte[] b = BYTE_ARRS[i];
      final String s = Utils.byte2Hex(b);
      assertTrue(Arrays.equals(b, Utils.hex2Byte(s)));
    }

    // hex2Byte2Hex
    for (int i = 0; i < STRING_ARR.length; ++i) {
      final String s = STRING_ARR[i];
      final byte[] b = Utils.hex2Byte(s);
      assertEquals(s, Utils.byte2Hex(b));
    }
  }

  @Test
  public void testByte2HexLength() throws Exception {
    for (int i = 0; i < BYTE_ARRS.length; ++i) {
      final byte[] b = BYTE_ARRS[i];
      final String expected = STRING_ARR[i];
      assertEquals(expected, Utils.byte2Hex(b, b.length));
      assertEquals("0" + expected, Utils.byte2Hex(b, 2 * b.length + 1));
      assertEquals("00" + expected, Utils.byte2Hex(b, 2 * b.length + 2));
    }
  }

  @Test
  public void testHex2ByteLength() throws Exception {
    for (int i = 0; i < STRING_ARR.length; ++i) {
      final String s = STRING_ARR[i];
      final byte[] expected = BYTE_ARRS[i];
      assertTrue(Arrays.equals(expected, Utils.hex2Byte(s)));
      final byte[] expected1 = new byte[expected.length + 1];
      System.arraycopy(expected, 0, expected1, 1, expected.length);
      assertTrue(Arrays.equals(expected1, Utils.hex2Byte("00" + s)));
      final byte[] expected2 = new byte[expected.length + 2];
      System.arraycopy(expected, 0, expected2, 2, expected.length);
      assertTrue(Arrays.equals(expected2, Utils.hex2Byte("0000" + s)));
    }
  }

  @Test
  public void testToCommaSeparatedString() {
    ArrayList<String> xs = new ArrayList<String>();
    assertEquals("", Utils.toCommaSeparatedString(null));
    assertEquals("", Utils.toCommaSeparatedString(xs));
    xs.add("test1");
    assertEquals("test1", Utils.toCommaSeparatedString(xs));
    xs.add("test2");
    assertEquals("test1, test2", Utils.toCommaSeparatedString(xs));
    xs.add("test3");
    assertEquals("test1, test2, test3", Utils.toCommaSeparatedString(xs));
  }

  @Test
  public void testUsernameFromAccount() throws NoSuchAlgorithmException, UnsupportedEncodingException {
    assertEquals("xee7ffonluzpdp66l6xgpyh2v2w6ojkc", Utils.sha1Base32("foobar@baz.com"));
    assertEquals("xee7ffonluzpdp66l6xgpyh2v2w6ojkc", Utils.usernameFromAccount("foobar@baz.com"));
    assertEquals("xee7ffonluzpdp66l6xgpyh2v2w6ojkc", Utils.usernameFromAccount("FooBar@Baz.com"));
    assertEquals("xee7ffonluzpdp66l6xgpyh2v2w6ojkc", Utils.usernameFromAccount("xee7ffonluzpdp66l6xgpyh2v2w6ojkc"));
    assertEquals("foobar",                           Utils.usernameFromAccount("foobar"));
    assertEquals("foobar",                           Utils.usernameFromAccount("FOOBAr"));
  }

  @Test
  public void testGetPrefsPath() throws NoSuchAlgorithmException, UnsupportedEncodingException {
    assertEquals("ore7dlrwqi6xr7honxdtpvmh6tly4r7k", Utils.sha1Base32("test.url.com:xee7ffonluzpdp66l6xgpyh2v2w6ojkc"));

    assertEquals("sync.prefs.ore7dlrwqi6xr7honxdtpvmh6tly4r7k", Utils.getPrefsPath("product", "foobar@baz.com", "test.url.com", "default", 0));
    assertEquals("sync.prefs.ore7dlrwqi6xr7honxdtpvmh6tly4r7k", Utils.getPrefsPath("org.mozilla.firefox_beta", "FooBar@Baz.com", "test.url.com", "default", 0));
    assertEquals("sync.prefs.ore7dlrwqi6xr7honxdtpvmh6tly4r7k", Utils.getPrefsPath("org.mozilla.firefox", "xee7ffonluzpdp66l6xgpyh2v2w6ojkc", "test.url.com", "profile", 0));

    assertEquals("sync.prefs.product.ore7dlrwqi6xr7honxdtpvmh6tly4r7k.default.1", Utils.getPrefsPath("product", "foobar@baz.com", "test.url.com", "default", 1));
    assertEquals("sync.prefs.with!spaces_underbars!periods.ore7dlrwqi6xr7honxdtpvmh6tly4r7k.default.1", Utils.getPrefsPath("with spaces_underbars.periods", "foobar@baz.com", "test.url.com", "default", 1));
    assertEquals("sync.prefs.org!mozilla!firefox_beta.ore7dlrwqi6xr7honxdtpvmh6tly4r7k.default.2", Utils.getPrefsPath("org.mozilla.firefox_beta", "FooBar@Baz.com", "test.url.com", "default", 2));
    assertEquals("sync.prefs.org!mozilla!firefox.ore7dlrwqi6xr7honxdtpvmh6tly4r7k.profile.3", Utils.getPrefsPath("org.mozilla.firefox", "xee7ffonluzpdp66l6xgpyh2v2w6ojkc", "test.url.com", "profile", 3));
  }

  @Test
  public void testObfuscateEmail() {
    assertEquals("XXX@XXX.XXX", Utils.obfuscateEmail("foo@bar.com"));
    assertEquals("XXXX@XXX.XXXX.XX", Utils.obfuscateEmail("foot@bar.test.ca"));
  }

  @Test
  public void testNodeWeaveURL() throws Exception {
    Assert.assertEquals("http://userapi.com/endpoint/user/1.0/username/node/weave", Utils.nodeWeaveURL("http://userapi.com/endpoint", "username"));
    Assert.assertEquals("http://userapi.com/endpoint/user/1.0/username/node/weave", Utils.nodeWeaveURL("http://userapi.com/endpoint/", "username"));
    Assert.assertEquals(SyncConstants.DEFAULT_AUTH_SERVER + "user/1.0/username/node/weave", Utils.nodeWeaveURL(null, "username"));
    Assert.assertEquals(SyncConstants.DEFAULT_AUTH_SERVER + "user/1.0/username2/node/weave", Utils.nodeWeaveURL(null, "username2"));
  }
}
