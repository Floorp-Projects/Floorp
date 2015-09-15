/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.fxa.test;

import java.math.BigInteger;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.net.SRPConstants;
import org.robolectric.RobolectricGradleTestRunner;

/**
 * Test vectors from
 * <a href="https://wiki.mozilla.org/Identity/AttachedServices/KeyServerProtocol#stretch-KDF">https://wiki.mozilla.org/Identity/AttachedServices/KeyServerProtocol#stretch-KDF</a>
 * and
 * <a href="https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol/5a9bc81e499306d769ca19b40b50fa60123df15d">https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol/5a9bc81e499306d769ca19b40b50fa60123df15d</a>.
 */
@RunWith(RobolectricGradleTestRunner.class)
public class TestFxAccountUtils {
  protected static void assertEncoding(String base16String, String utf8String) throws Exception {
    Assert.assertEquals(base16String, FxAccountUtils.bytes(utf8String));
  }

  @Test
  public void testUTF8Encoding() throws Exception {
    assertEncoding("616e6472c3a9406578616d706c652e6f7267", "andré@example.org");
    assertEncoding("70c3a4737377c3b67264", "pässwörd");
  }

  @Test
  public void testHexModN() {
    BigInteger N = BigInteger.valueOf(14);
    Assert.assertEquals(4, N.bitLength());
    Assert.assertEquals(1, (N.bitLength() + 7)/8);
    Assert.assertEquals("00", FxAccountUtils.hexModN(BigInteger.valueOf(0), N));
    Assert.assertEquals("05", FxAccountUtils.hexModN(BigInteger.valueOf(5), N));
    Assert.assertEquals("0b", FxAccountUtils.hexModN(BigInteger.valueOf(11), N));
    Assert.assertEquals("00", FxAccountUtils.hexModN(BigInteger.valueOf(14), N));
    Assert.assertEquals("01", FxAccountUtils.hexModN(BigInteger.valueOf(15), N));
    Assert.assertEquals("02", FxAccountUtils.hexModN(BigInteger.valueOf(16), N));
    Assert.assertEquals("02", FxAccountUtils.hexModN(BigInteger.valueOf(30), N));

    N = BigInteger.valueOf(260);
    Assert.assertEquals("00ff", FxAccountUtils.hexModN(BigInteger.valueOf(255), N));
    Assert.assertEquals("0100", FxAccountUtils.hexModN(BigInteger.valueOf(256), N));
    Assert.assertEquals("0101", FxAccountUtils.hexModN(BigInteger.valueOf(257), N));
    Assert.assertEquals("0001", FxAccountUtils.hexModN(BigInteger.valueOf(261), N));
  }

  @Test
  public void testSRPVerifierFunctions() throws Exception {
    byte[] emailUTF8Bytes = Utils.hex2Byte("616e6472c3a9406578616d706c652e6f7267");
    byte[] srpPWBytes = Utils.hex2Byte("00f9b71800ab5337d51177d8fbc682a3653fa6dae5b87628eeec43a18af59a9d", 32);
    byte[] srpSaltBytes = Utils.hex2Byte("00f1000000000000000000000000000000000000000000000000000000000179", 32);

    String expectedX = "81925186909189958012481408070938147619474993903899664126296984459627523279550";
    BigInteger x = FxAccountUtils.srpVerifierLowercaseX(emailUTF8Bytes, srpPWBytes, srpSaltBytes);
    Assert.assertEquals(expectedX, x.toString(10));

    String expectedV = "11464957230405843056840989945621595830717843959177257412217395741657995431613430369165714029818141919887853709633756255809680435884948698492811770122091692817955078535761033207000504846365974552196983218225819721112680718485091921646083608065626264424771606096544316730881455897489989950697705196721477608178869100211706638584538751009854562396937282582855620488967259498367841284829152987988548996842770025110751388952323221706639434861071834212055174768483159061566055471366772641252573641352721966728239512914666806496255304380341487975080159076396759492553066357163103546373216130193328802116982288883318596822";
    BigInteger v = FxAccountUtils.srpVerifierLowercaseV(emailUTF8Bytes, srpPWBytes, srpSaltBytes, SRPConstants._2048.g, SRPConstants._2048.N);
    Assert.assertEquals(expectedV, v.toString(10));

    String expectedVHex = "00173ffa0263e63ccfd6791b8ee2a40f048ec94cd95aa8a3125726f9805e0c8283c658dc0b607fbb25db68e68e93f2658483049c68af7e8214c49fde2712a775b63e545160d64b00189a86708c69657da7a1678eda0cd79f86b8560ebdb1ffc221db360eab901d643a75bf1205070a5791230ae56466b8c3c1eb656e19b794f1ea0d2a077b3a755350208ea0118fec8c4b2ec344a05c66ae1449b32609ca7189451c259d65bd15b34d8729afdb5faff8af1f3437bbdc0c3d0b069a8ab2a959c90c5a43d42082c77490f3afcc10ef5648625c0605cdaace6c6fdc9e9a7e6635d619f50af7734522470502cab26a52a198f5b00a279858916507b0b4e9ef9524d6";
    Assert.assertEquals(expectedVHex, FxAccountUtils.hexModN(v, SRPConstants._2048.N));
  }

  @Test
  public void testGenerateSyncKeyBundle() throws Exception {
    byte[] kB = Utils.hex2Byte("d02d8fe39f28b601159c543f2deeb8f72bdf2043e8279aa08496fbd9ebaea361");
    KeyBundle bundle = FxAccountUtils.generateSyncKeyBundle(kB);
    Assert.assertEquals("rsLwECkgPYeGbYl92e23FskfIbgld9TgeifEaB9ZwTI=", Base64.encodeBase64String(bundle.getEncryptionKey()));
    Assert.assertEquals("fs75EseCD/VOLodlIGmwNabBjhTYBHFCe7CGIf0t8Tw=", Base64.encodeBase64String(bundle.getHMACKey()));
  }

  @Test
  public void testGeneration() throws Exception {
    byte[] quickStretchedPW = FxAccountUtils.generateQuickStretchedPW(
        Utils.hex2Byte("616e6472c3a9406578616d706c652e6f7267"),
        Utils.hex2Byte("70c3a4737377c3b67264"));
    Assert.assertEquals("e4e8889bd8bd61ad6de6b95c059d56e7b50dacdaf62bd84644af7e2add84345d",
        Utils.byte2Hex(quickStretchedPW));
    Assert.assertEquals("247b675ffb4c46310bc87e26d712153abe5e1c90ef00a4784594f97ef54f2375",
        Utils.byte2Hex(FxAccountUtils.generateAuthPW(quickStretchedPW)));
    byte[] unwrapkB = FxAccountUtils.generateUnwrapBKey(quickStretchedPW);
    Assert.assertEquals("de6a2648b78284fcb9ffa81ba95803309cfba7af583c01a8a1a63e567234dd28",
        Utils.byte2Hex(unwrapkB));
    byte[] wrapkB = Utils.hex2Byte("7effe354abecbcb234a8dfc2d7644b4ad339b525589738f2d27341bb8622ecd8");
    Assert.assertEquals("a095c51c1c6e384e8d5777d97e3c487a4fc2128a00ab395a73d57fedf41631f0",
        Utils.byte2Hex(FxAccountUtils.unwrapkB(unwrapkB, wrapkB)));
  }

  @Test
  public void testClientState() throws Exception {
    final String hexKB = "fd5c747806c07ce0b9d69dcfea144663e630b65ec4963596a22f24910d7dd15d";
    final byte[] byteKB = Utils.hex2Byte(hexKB);
    final String clientState = FxAccountUtils.computeClientState(byteKB);
    final String expected = "6ae94683571c7a7c54dab4700aa3995f";
    Assert.assertEquals(expected, clientState);
  }

  @Test
  public void testGetAudienceForURL() throws Exception {
    // Sub-domains and path components.
    Assert.assertEquals("http://sub.test.com", FxAccountUtils.getAudienceForURL("http://sub.test.com"));
    Assert.assertEquals("http://test.com", FxAccountUtils.getAudienceForURL("http://test.com/"));
    Assert.assertEquals("http://test.com", FxAccountUtils.getAudienceForURL("http://test.com/path/component"));
    Assert.assertEquals("http://test.com", FxAccountUtils.getAudienceForURL("http://test.com/path/component/"));

    // No port and default port.
    Assert.assertEquals("http://test.com", FxAccountUtils.getAudienceForURL("http://test.com"));
    Assert.assertEquals("http://test.com:80", FxAccountUtils.getAudienceForURL("http://test.com:80"));

    Assert.assertEquals("https://test.com", FxAccountUtils.getAudienceForURL("https://test.com"));
    Assert.assertEquals("https://test.com:443", FxAccountUtils.getAudienceForURL("https://test.com:443"));

    // Ports that are the default ports for a different scheme.
    Assert.assertEquals("https://test.com:80", FxAccountUtils.getAudienceForURL("https://test.com:80"));
    Assert.assertEquals("http://test.com:443", FxAccountUtils.getAudienceForURL("http://test.com:443"));

    // Arbitrary ports.
    Assert.assertEquals("http://test.com:8080", FxAccountUtils.getAudienceForURL("http://test.com:8080"));
    Assert.assertEquals("https://test.com:4430", FxAccountUtils.getAudienceForURL("https://test.com:4430"));
  }
}
