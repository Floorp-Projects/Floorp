/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.browserid.test;

import java.math.BigInteger;

import junit.framework.Assert;

import org.junit.Test;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.RSACryptoImplementation;
import org.mozilla.gecko.sync.ExtendedJSONObject;

public class TestRSACryptoImplementation {
  @Test
  public void testToJSONObject() throws Exception {
    BigInteger n = new BigInteger("7042170764319402120473546823641395184140303948430445023576085129538272863656735924617881022040465877164076593767104512065359975488480629290310209335113577");
    BigInteger e = new BigInteger("65537");
    BigInteger d = new BigInteger("2050102629239206449128199335463237235732683202345308155771672920433658970744825199440426256856862541525088288448769859770132714705204296375901885294992205");

    BrowserIDKeyPair keyPair = new BrowserIDKeyPair(
        RSACryptoImplementation.createPrivateKey(n, d),
        RSACryptoImplementation.createPublicKey(n, e));

    ExtendedJSONObject o = new ExtendedJSONObject("{\"publicKey\":{\"e\":\"65537\",\"n\":\"7042170764319402120473546823641395184140303948430445023576085129538272863656735924617881022040465877164076593767104512065359975488480629290310209335113577\",\"algorithm\":\"RS\"},\"privateKey\":{\"d\":\"2050102629239206449128199335463237235732683202345308155771672920433658970744825199440426256856862541525088288448769859770132714705204296375901885294992205\",\"n\":\"7042170764319402120473546823641395184140303948430445023576085129538272863656735924617881022040465877164076593767104512065359975488480629290310209335113577\",\"algorithm\":\"RS\"}}");
    Assert.assertEquals(o.getObject("privateKey"), keyPair.toJSONObject().getObject("privateKey"));
    Assert.assertEquals(o.getObject("publicKey"), keyPair.toJSONObject().getObject("publicKey"));
  }

  @Test
  public void testFromJSONObject() throws Exception {
    BigInteger n = new BigInteger("7042170764319402120473546823641395184140303948430445023576085129538272863656735924617881022040465877164076593767104512065359975488480629290310209335113577");
    BigInteger e = new BigInteger("65537");
    BigInteger d = new BigInteger("2050102629239206449128199335463237235732683202345308155771672920433658970744825199440426256856862541525088288448769859770132714705204296375901885294992205");

    BrowserIDKeyPair keyPair = new BrowserIDKeyPair(
        RSACryptoImplementation.createPrivateKey(n, d),
        RSACryptoImplementation.createPublicKey(n, e));

    ExtendedJSONObject o = new ExtendedJSONObject("{\"publicKey\":{\"e\":\"65537\",\"n\":\"7042170764319402120473546823641395184140303948430445023576085129538272863656735924617881022040465877164076593767104512065359975488480629290310209335113577\",\"algorithm\":\"RS\"},\"privateKey\":{\"d\":\"2050102629239206449128199335463237235732683202345308155771672920433658970744825199440426256856862541525088288448769859770132714705204296375901885294992205\",\"n\":\"7042170764319402120473546823641395184140303948430445023576085129538272863656735924617881022040465877164076593767104512065359975488480629290310209335113577\",\"algorithm\":\"RS\"}}");

    Assert.assertEquals(keyPair.getPublic().toJSONObject(), RSACryptoImplementation.createPublicKey(o.getObject("publicKey")).toJSONObject());
    Assert.assertEquals(keyPair.getPrivate().toJSONObject(), RSACryptoImplementation.createPrivateKey(o.getObject("privateKey")).toJSONObject());
  }

  @Test
  public void testRoundTrip() throws Exception {
    BrowserIDKeyPair keyPair = RSACryptoImplementation.generateKeyPair(512);
    ExtendedJSONObject o = keyPair.toJSONObject();
    BrowserIDKeyPair keyPair2 = RSACryptoImplementation.fromJSONObject(o);
    Assert.assertEquals(o, keyPair2.toJSONObject());
  }
}
