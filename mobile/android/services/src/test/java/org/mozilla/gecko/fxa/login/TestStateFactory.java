/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.fxa.login;

import junit.framework.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.DSACryptoImplementation;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;

@RunWith(TestRunner.class)
public class TestStateFactory {
  @Test
  public void testGetStateV3() throws Exception {
    MigratedFromSync11 migrated = new MigratedFromSync11("email", "uid", true, "password");

    // For the current version, we expect to read back what we wrote.
    ExtendedJSONObject o;
    State state;

    o = migrated.toJSONObject();
    Assert.assertEquals(3, o.getLong("version").intValue());
    state = StateFactory.fromJSONObject(migrated.stateLabel, o);
    Assert.assertEquals(StateLabel.MigratedFromSync11, state.stateLabel);
    Assert.assertEquals(o, state.toJSONObject());

    // Null passwords are OK.
    MigratedFromSync11 migratedNullPassword = new MigratedFromSync11("email", "uid", true, null);

    o = migratedNullPassword.toJSONObject();
    Assert.assertEquals(3, o.getLong("version").intValue());
    state = StateFactory.fromJSONObject(migratedNullPassword.stateLabel, o);
    Assert.assertEquals(StateLabel.MigratedFromSync11, state.stateLabel);
    Assert.assertEquals(o, state.toJSONObject());
  }

  @Test
  public void testGetStateV2() throws Exception {
    byte[] sessionToken = Utils.generateRandomBytes(32);
    byte[] kA = Utils.generateRandomBytes(32);
    byte[] kB = Utils.generateRandomBytes(32);
    BrowserIDKeyPair keyPair = DSACryptoImplementation.generateKeyPair(512);
    Cohabiting cohabiting = new Cohabiting("email", "uid", sessionToken, kA, kB, keyPair);
    String certificate = "certificate";
    Married married = new Married("email", "uid", sessionToken, kA, kB, keyPair, certificate);

    // For the current version, we expect to read back what we wrote.
    ExtendedJSONObject o;
    State state;

    o = married.toJSONObject();
    Assert.assertEquals(3, o.getLong("version").intValue());
    state = StateFactory.fromJSONObject(married.stateLabel, o);
    Assert.assertEquals(StateLabel.Married, state.stateLabel);
    Assert.assertEquals(o, state.toJSONObject());

    o = cohabiting.toJSONObject();
    Assert.assertEquals(3, o.getLong("version").intValue());
    state = StateFactory.fromJSONObject(cohabiting.stateLabel, o);
    Assert.assertEquals(StateLabel.Cohabiting, state.stateLabel);
    Assert.assertEquals(o, state.toJSONObject());
  }

  @Test
  public void testGetStateV1() throws Exception {
    // We can't rely on generating correct V1 objects (since the generation code
    // may change); so we hard code a few test examples here. These examples
    // have RSA key pairs; when they're parsed, we return DSA key pairs.
    ExtendedJSONObject o = new ExtendedJSONObject("{\"uid\":\"uid\",\"sessionToken\":\"4e2830da6ce466ddb401fbca25b96a621209eea83851254800f84cc4069ef011\",\"certificate\":\"certificate\",\"keyPair\":{\"publicKey\":{\"e\":\"65537\",\"n\":\"7598360104379019497828904063491254083855849024432238665262988260947462372141971045236693389494635158997975098558915846889960089362159921622822266839560631\",\"algorithm\":\"RS\"},\"privateKey\":{\"d\":\"6807533330618101360064115400338014782301295929300445938471117364691566605775022173055292460962170873583673516346599808612503093914221141089102289381448225\",\"n\":\"7598360104379019497828904063491254083855849024432238665262988260947462372141971045236693389494635158997975098558915846889960089362159921622822266839560631\",\"algorithm\":\"RS\"}},\"email\":\"email\",\"verified\":true,\"kB\":\"0b048f285c19067f200da7bfbe734ed213cefcd8f543f0fdd4a8ccab48cbbc89\",\"kA\":\"59a9edf2d41de8b24e69df9133bc88e96913baa75421882f4c55d842d18fc8a1\",\"version\":1}");
    // A Married state is regressed to a Cohabited state.
    Cohabiting state = (Cohabiting) StateFactory.fromJSONObject(StateLabel.Married, o);

    Assert.assertEquals(StateLabel.Cohabiting, state.stateLabel);
    Assert.assertEquals("uid", state.uid);
    Assert.assertEquals("4e2830da6ce466ddb401fbca25b96a621209eea83851254800f84cc4069ef011", Utils.byte2Hex(state.sessionToken));
    Assert.assertEquals("DS128", state.keyPair.getPrivate().getAlgorithm());

    o = new ExtendedJSONObject("{\"uid\":\"uid\",\"sessionToken\":\"4e2830da6ce466ddb401fbca25b96a621209eea83851254800f84cc4069ef011\",\"keyPair\":{\"publicKey\":{\"e\":\"65537\",\"n\":\"7598360104379019497828904063491254083855849024432238665262988260947462372141971045236693389494635158997975098558915846889960089362159921622822266839560631\",\"algorithm\":\"RS\"},\"privateKey\":{\"d\":\"6807533330618101360064115400338014782301295929300445938471117364691566605775022173055292460962170873583673516346599808612503093914221141089102289381448225\",\"n\":\"7598360104379019497828904063491254083855849024432238665262988260947462372141971045236693389494635158997975098558915846889960089362159921622822266839560631\",\"algorithm\":\"RS\"}},\"email\":\"email\",\"verified\":true,\"kB\":\"0b048f285c19067f200da7bfbe734ed213cefcd8f543f0fdd4a8ccab48cbbc89\",\"kA\":\"59a9edf2d41de8b24e69df9133bc88e96913baa75421882f4c55d842d18fc8a1\",\"version\":1}");
    state = (Cohabiting) StateFactory.fromJSONObject(StateLabel.Cohabiting, o);

    Assert.assertEquals(StateLabel.Cohabiting, state.stateLabel);
    Assert.assertEquals("uid", state.uid);
    Assert.assertEquals("4e2830da6ce466ddb401fbca25b96a621209eea83851254800f84cc4069ef011", Utils.byte2Hex(state.sessionToken));
    Assert.assertEquals("DS128", state.keyPair.getPrivate().getAlgorithm());
  }
}
