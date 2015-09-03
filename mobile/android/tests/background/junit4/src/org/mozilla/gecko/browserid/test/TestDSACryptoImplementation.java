/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.browserid.test;

import java.math.BigInteger;

import junit.framework.Assert;

import org.junit.Test;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.DSACryptoImplementation;
import org.mozilla.gecko.sync.ExtendedJSONObject;

public class TestDSACryptoImplementation {
  @Test
  public void testToJSONObject() throws Exception {
    BigInteger p = new BigInteger("fca682ce8e12caba26efccf7110e526db078b05edecbcd1eb4a208f3ae1617ae01f35b91a47e6df63413c5e12ed0899bcd132acd50d99151bdc43ee737592e17", 16);
    BigInteger q = new BigInteger("962eddcc369cba8ebb260ee6b6a126d9346e38c5", 16);
    BigInteger g = new BigInteger("678471b27a9cf44ee91a49c5147db1a9aaf244f05a434d6486931d2d14271b9e35030b71fd73da179069b32e2935630e1c2062354d0da20a6c416e50be794ca4", 16);
    BigInteger x = new BigInteger("9516d860392003db5a4f168444903265467614db", 16);
    BigInteger y = new BigInteger("455152a0e499f5c9d11f9f1868c8b868b1443ca853843226a5a9552dd909b4bdba879acc504acb690df0348d60e63ea37e8c7f075302e0df5bcdc76a383888a0", 16);

    BrowserIDKeyPair keyPair = new BrowserIDKeyPair(
        DSACryptoImplementation.createPrivateKey(x, p, q, g),
        DSACryptoImplementation.createPublicKey(y, p, q, g));

    ExtendedJSONObject o = new ExtendedJSONObject("{\"publicKey\":{\"g\":\"678471b27a9cf44ee91a49c5147db1a9aaf244f05a434d6486931d2d14271b9e35030b71fd73da179069b32e2935630e1c2062354d0da20a6c416e50be794ca4\",\"q\":\"962eddcc369cba8ebb260ee6b6a126d9346e38c5\",\"p\":\"fca682ce8e12caba26efccf7110e526db078b05edecbcd1eb4a208f3ae1617ae01f35b91a47e6df63413c5e12ed0899bcd132acd50d99151bdc43ee737592e17\",\"y\":\"455152a0e499f5c9d11f9f1868c8b868b1443ca853843226a5a9552dd909b4bdba879acc504acb690df0348d60e63ea37e8c7f075302e0df5bcdc76a383888a0\",\"algorithm\":\"DS\"},\"privateKey\":{\"g\":\"678471b27a9cf44ee91a49c5147db1a9aaf244f05a434d6486931d2d14271b9e35030b71fd73da179069b32e2935630e1c2062354d0da20a6c416e50be794ca4\",\"q\":\"962eddcc369cba8ebb260ee6b6a126d9346e38c5\",\"p\":\"fca682ce8e12caba26efccf7110e526db078b05edecbcd1eb4a208f3ae1617ae01f35b91a47e6df63413c5e12ed0899bcd132acd50d99151bdc43ee737592e17\",\"x\":\"9516d860392003db5a4f168444903265467614db\",\"algorithm\":\"DS\"}}");
    Assert.assertEquals(o.getObject("privateKey"), keyPair.toJSONObject().getObject("privateKey"));
    Assert.assertEquals(o.getObject("publicKey"), keyPair.toJSONObject().getObject("publicKey"));
  }

  @Test
  public void testFromJSONObject() throws Exception {
    BigInteger p = new BigInteger("fca682ce8e12caba26efccf7110e526db078b05edecbcd1eb4a208f3ae1617ae01f35b91a47e6df63413c5e12ed0899bcd132acd50d99151bdc43ee737592e17", 16);
    BigInteger q = new BigInteger("962eddcc369cba8ebb260ee6b6a126d9346e38c5", 16);
    BigInteger g = new BigInteger("678471b27a9cf44ee91a49c5147db1a9aaf244f05a434d6486931d2d14271b9e35030b71fd73da179069b32e2935630e1c2062354d0da20a6c416e50be794ca4", 16);
    BigInteger x = new BigInteger("9516d860392003db5a4f168444903265467614db", 16);
    BigInteger y = new BigInteger("455152a0e499f5c9d11f9f1868c8b868b1443ca853843226a5a9552dd909b4bdba879acc504acb690df0348d60e63ea37e8c7f075302e0df5bcdc76a383888a0", 16);

    BrowserIDKeyPair keyPair = new BrowserIDKeyPair(
        DSACryptoImplementation.createPrivateKey(x, p, q, g),
        DSACryptoImplementation.createPublicKey(y, p, q, g));

    ExtendedJSONObject o = new ExtendedJSONObject("{\"publicKey\":{\"g\":\"678471b27a9cf44ee91a49c5147db1a9aaf244f05a434d6486931d2d14271b9e35030b71fd73da179069b32e2935630e1c2062354d0da20a6c416e50be794ca4\",\"q\":\"962eddcc369cba8ebb260ee6b6a126d9346e38c5\",\"p\":\"fca682ce8e12caba26efccf7110e526db078b05edecbcd1eb4a208f3ae1617ae01f35b91a47e6df63413c5e12ed0899bcd132acd50d99151bdc43ee737592e17\",\"y\":\"455152a0e499f5c9d11f9f1868c8b868b1443ca853843226a5a9552dd909b4bdba879acc504acb690df0348d60e63ea37e8c7f075302e0df5bcdc76a383888a0\",\"algorithm\":\"DS\"},\"privateKey\":{\"g\":\"678471b27a9cf44ee91a49c5147db1a9aaf244f05a434d6486931d2d14271b9e35030b71fd73da179069b32e2935630e1c2062354d0da20a6c416e50be794ca4\",\"q\":\"962eddcc369cba8ebb260ee6b6a126d9346e38c5\",\"p\":\"fca682ce8e12caba26efccf7110e526db078b05edecbcd1eb4a208f3ae1617ae01f35b91a47e6df63413c5e12ed0899bcd132acd50d99151bdc43ee737592e17\",\"x\":\"9516d860392003db5a4f168444903265467614db\",\"algorithm\":\"DS\"}}");

    Assert.assertEquals(keyPair.getPublic().toJSONObject(), DSACryptoImplementation.createPublicKey(o.getObject("publicKey")).toJSONObject());
    Assert.assertEquals(keyPair.getPrivate().toJSONObject(), DSACryptoImplementation.createPrivateKey(o.getObject("privateKey")).toJSONObject());
  }

  @Test
  public void testRoundTrip() throws Exception {
    BrowserIDKeyPair keyPair = DSACryptoImplementation.generateKeyPair(512);
    ExtendedJSONObject o = keyPair.toJSONObject();
    BrowserIDKeyPair keyPair2 = DSACryptoImplementation.fromJSONObject(o);
    Assert.assertEquals(o, keyPair2.toJSONObject());
  }
}
