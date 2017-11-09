/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.fxa;

import java.security.GeneralSecurityException;

import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.DSACryptoImplementation;
import org.mozilla.gecko.browserid.JSONWebTokenUtils;
import org.mozilla.gecko.browserid.RSACryptoImplementation;
import org.mozilla.gecko.browserid.SigningPrivateKey;
import org.mozilla.gecko.browserid.VerifyingPublicKey;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;

public class TestBrowserIDKeyPairGeneration extends AndroidSyncTestCase {
  public void doTestEncodeDecode(BrowserIDKeyPair keyPair) throws Exception {
    SigningPrivateKey privateKey = keyPair.getPrivate();
    VerifyingPublicKey publicKey = keyPair.getPublic();

    ExtendedJSONObject o = new ExtendedJSONObject();
    o.put("key", Utils.generateGuid());

    String token = JSONWebTokenUtils.encode(o.toJSONString(), privateKey);
    assertNotNull(token);

    String payload = JSONWebTokenUtils.decode(token, publicKey);
    assertEquals(o.toJSONString(), payload);

    try {
      JSONWebTokenUtils.decode(token + "x", publicKey);
      fail("Expected exception.");
    } catch (GeneralSecurityException e) {
      // Do nothing.
    }
  }

  public void testEncodeDecodeSuccessRSA() throws Exception {
    doTestEncodeDecode(RSACryptoImplementation.generateKeyPair(1024));
    doTestEncodeDecode(RSACryptoImplementation.generateKeyPair(2048));
  }

  public void testEncodeDecodeSuccessDSA() throws Exception {
    doTestEncodeDecode(DSACryptoImplementation.generateKeyPair(512));
    doTestEncodeDecode(DSACryptoImplementation.generateKeyPair(1024));
  }
}
