package org.mozilla.android.sync.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;

import org.json.simple.parser.ParseException;
import org.junit.Test;
import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.jpake.Gx3OrGx4IsZeroOrOneException;
import org.mozilla.gecko.sync.jpake.IncorrectZkpException;
import org.mozilla.gecko.sync.jpake.JPakeClient;
import org.mozilla.gecko.sync.jpake.JPakeCrypto;
import org.mozilla.gecko.sync.jpake.JPakeNumGenerator;
import org.mozilla.gecko.sync.jpake.JPakeNumGeneratorRandom;
import org.mozilla.gecko.sync.jpake.JPakeParty;
import org.mozilla.gecko.sync.jpake.stage.ComputeKeyVerificationStage;
import org.mozilla.gecko.sync.jpake.stage.VerifyPairingStage;
import org.mozilla.gecko.sync.setup.Constants;

public class TestJPakeSetup {
  // Note: will throw NullPointerException if aborts. Only use stateless public
  // methods.

  @Test
  public void testGx3OrGx4ZeroOrOneThrowsException()
      throws UnsupportedEncodingException
  {
    JPakeNumGeneratorRandom gen = new JPakeNumGeneratorRandom();
    JPakeParty p = new JPakeParty("foobar");
    BigInteger secret = JPakeClient.secretAsBigInteger("secret");

    p.gx4 = new BigInteger("2");
    p.gx3 = new BigInteger("0");
    try {
      JPakeCrypto.round2(secret, p, gen);
      fail("round2 should fail if gx3 == 0");
    } catch (Gx3OrGx4IsZeroOrOneException e) {
      // Hurrah.
    } catch (Exception e) {
      fail("Unexpected exception " + e);
    }

    p.gx3 = new BigInteger("1");
    try {
      JPakeCrypto.round2(secret, p, gen);
      fail("round2 should fail if gx3 == 1");
    } catch (Gx3OrGx4IsZeroOrOneException e) {
      // Hurrah.
    } catch (Exception e) {
      fail("Unexpected exception " + e);
    }

    p.gx3 = new BigInteger("3");
    try {
      JPakeCrypto.round2(secret, p, gen);
    } catch (Gx3OrGx4IsZeroOrOneException e) {
      fail("Unexpected exception " + e);
    } catch (Exception e) {
      // There are plenty of other reasons this should fail.
    }

    p.gx3 = new BigInteger("2");
    p.gx4 = new BigInteger("0");
    try {
      JPakeCrypto.round2(secret, p, gen);
      fail("round2 should fail if gx4 == 0");
    } catch (Gx3OrGx4IsZeroOrOneException e) {
      // Hurrah.
    } catch (Exception e) {
      fail("Unexpected exception " + e);
    }

    p.gx4 = new BigInteger("1");
    try {
      JPakeCrypto.round2(secret, p, gen);
      fail("round2 should fail if gx4 == 1");
    } catch (Gx3OrGx4IsZeroOrOneException e) {
      // Hurrah.
    } catch (Exception e) {
      fail("Unexpected exception " + e);
    }

    p.gx4 = new BigInteger("3");
    try {
      JPakeCrypto.round2(secret, p, gen);
    } catch (Gx3OrGx4IsZeroOrOneException e) {
      fail("Unexpected exception " + e);
    } catch (Exception e) {
      // There are plenty of other reasons this should fail.
    }
  }

  /*
   * Tests encryption key and hmac generation from a derived key, using values
   * taken from a successful run of J-PAKE.
   */
  @Test
  public void testKeyDerivation() throws UnsupportedEncodingException {
    String keyChars16 = "811565455b22c857a3e303d1f48ff72ae9ef42d9c3fe3740ce7772cb5bfe23491dd5b7ee5af4828ab9b7d5844866f378b4cf0156810aff0504ef2947402e8e40be1179cf7f37b231bc0db9e4e1bb239c849aa5c12ed2b0b4413017599270aae71ee993dd755ee8c045c5fe03d713894692bf72158d9835ad905442edfd8235e1d0c915053debfc49d8248e4dae16608743aef5dab061f49fd6edd0b93ecdf9feafcbe47eb7e6c3678356d96e9bcd87814b13b9eb1a791fd446d69cb040ec7d7194031267e26f266ee3decbc1a85c5203427361997adf9823fbffe16af9946f1347c5354956356732e436ef5f8307e96554cf69a54e4e8a78552e3f506e9310a1c4438d3ddce44a37482270533e47fc40dc84abfe39c1f95328d0d2540074f6301d4f121c2f0eac49c47a2c430614234ca26dede2a429e2fdb6d282a85174886c3a68c3cf5edc87ccb82af4ae4a9a26fffadc7f4d8ded4ff47b3d2d171f374b230e52e6b45963d3a0a6b20cbe6a440fd4a932279d52a6fd7694b4cbc0cb67ff3c";
    String expectedEncKey64 = "3TXwVlWf6YbuIPcg8m/2U4UXYV4a8RNu6pE2GOVkJJo=";
    String expectedHmac64 = "L49fnEPAD31G5uEKy5e4bGZ6IF3G/62qW6Ua/1NvBeQ=";

    byte[] encKeyBytes = new byte[32];
    byte[] hmacBytes = new byte[32];

    try {
      JPakeCrypto.generateKeyAndHmac(new BigInteger(keyChars16, 16), encKeyBytes, hmacBytes);
    } catch (Exception e) {
      fail("Unexpected exception " + e);
    }
    String encKey64 = new String(Base64.encodeBase64(encKeyBytes));
    String hmac64 = new String(Base64.encodeBase64(hmacBytes));

    assertTrue(expectedEncKey64.equals(encKey64));
    assertTrue(expectedHmac64.equals(hmac64));
  }

  /*
   * Test correct key derivation when both parties share a secret.
   */
  @Test
  public void testJPakeCorrectSecret() throws Gx3OrGx4IsZeroOrOneException,
      IncorrectZkpException, IOException, ParseException,
      NonObjectJSONException, CryptoException, NoSuchAlgorithmException, InvalidKeyException {
    BigInteger secret = JPakeClient.secretAsBigInteger("byubd7u75qmq");
    JPakeNumGenerator gen = new JPakeNumGeneratorRandom();
    // Keys derived should be the same.
    assertTrue(jPakeDeriveSameKey(gen, gen, secret, secret));
  }

  /*
   * Test incorrect key derivation when parties do not share the same secret.
   */
  @Test
  public void testJPakeIncorrectSecret() throws Gx3OrGx4IsZeroOrOneException,
      IncorrectZkpException, IOException, ParseException,
      NonObjectJSONException, CryptoException, NoSuchAlgorithmException, InvalidKeyException {
    BigInteger secret1 = JPakeClient.secretAsBigInteger("shareSecret1");
    BigInteger secret2 = JPakeClient.secretAsBigInteger("shareSecret2");
    JPakeNumGenerator gen = new JPakeNumGeneratorRandom();
    // Unsuccessful key derivation.
    assertFalse(jPakeDeriveSameKey(gen, gen, secret1, secret2));
  }

  /*
   * Helper simulation of a J-PAKE key derivation between two parties, with
   * secret1 and secret2. Both parties are assumed to be communicating on the
   * same channel; otherwise, J-PAKE would have failed immediately.
   */
  public boolean jPakeDeriveSameKey(JPakeNumGenerator gen1,
      JPakeNumGenerator gen2, BigInteger secret1, BigInteger secret2)
      throws IncorrectZkpException, Gx3OrGx4IsZeroOrOneException, IOException,
      ParseException, NonObjectJSONException, CryptoException, NoSuchAlgorithmException, InvalidKeyException {

    // Communicating parties.
    JPakeParty party1 = new JPakeParty("party1");
    JPakeParty party2 = new JPakeParty("party2");

    JPakeCrypto.round1(party1, gen1);
    // After party1 round 1, these values should no longer be null.
    assertNotNull(party1.signerId);
    assertNotNull(party1.x2);
    assertNotNull(party1.gx1);
    assertNotNull(party1.gx2);
    assertNotNull(party1.zkp1);
    assertNotNull(party1.zkp2);
    assertNotNull(party1.zkp1.b);
    assertNotNull(party1.zkp1.gr);
    assertNotNull(party1.zkp1.id);
    assertNotNull(party1.zkp2.b);
    assertNotNull(party1.zkp2.gr);
    assertNotNull(party1.zkp2.id);

    // party2 receives the following values from party1.
    party2.gx3 = party1.gx1;
    party2.gx4 = party1.gx2;
    party2.zkp3 = party1.zkp1;
    party2.zkp4 = party1.zkp2;
    // TODO Run JPakeClient checks.

    JPakeCrypto.round1(party2, gen2);
    // After party2 round 1, these values should no longer be null.
    assertNotNull(party2.signerId);
    assertNotNull(party2.x2);
    assertNotNull(party2.gx1);
    assertNotNull(party2.gx2);
    assertNotNull(party2.zkp1);
    assertNotNull(party2.zkp2);
    assertNotNull(party2.zkp1.b);
    assertNotNull(party2.zkp1.gr);
    assertNotNull(party2.zkp1.id);
    assertNotNull(party2.zkp2.b);
    assertNotNull(party2.zkp2.gr);
    assertNotNull(party2.zkp2.id);

    // Pass relevant values to party1.
    party1.gx3 = party2.gx1;
    party1.gx4 = party2.gx2;
    party1.zkp3 = party2.zkp1;
    party1.zkp4 = party2.zkp2;
    // TODO Run JPakeClient checks.

    JPakeCrypto.round2(secret1, party1, gen1);
    // After party1 round 2, these values should no longer be null.
    assertNotNull(party1.thisA);
    assertNotNull(party1.thisZkpA);
    assertNotNull(party1.thisZkpA.b);
    assertNotNull(party1.thisZkpA.gr);
    assertNotNull(party1.thisZkpA.id);

    // Pass relevant values to party2.
    party2.otherA = party1.thisA;
    party2.otherZkpA = party1.thisZkpA;

    JPakeCrypto.round2(secret2, party2, gen2);
    // Check for nulls.
    assertNotNull(party2.thisA);
    assertNotNull(party2.thisZkpA);
    assertNotNull(party2.thisZkpA.b);
    assertNotNull(party2.thisZkpA.gr);
    assertNotNull(party2.thisZkpA.id);

    // Pass values to party1.
    party1.otherA = party2.thisA;
    party1.otherZkpA = party2.thisZkpA;

    KeyBundle keyBundle1 = JPakeCrypto.finalRound(secret1, party1);
    assertNotNull(keyBundle1);

    // party1 computes the shared key, generates an encrypted message to party2.
    ExtendedJSONObject verificationMsg = new ComputeKeyVerificationStage()
        .computeKeyVerification(keyBundle1, party1.signerId);
    ExtendedJSONObject payload = verificationMsg
        .getObject(Constants.JSON_KEY_PAYLOAD);
    String ciphertext1 = (String) payload.get(Constants.JSON_KEY_CIPHERTEXT);
    String iv1 = (String) payload.get(Constants.JSON_KEY_IV);

    // party2 computes the key as well, using its copy of the secret.
    KeyBundle keyBundle2 = JPakeCrypto.finalRound(secret2, party2);
    // party2 fetches the encrypted message and verifies the pairing against its
    // own derived key.

    boolean isSuccess = new VerifyPairingStage().verifyCiphertext(ciphertext1, iv1,
        keyBundle2);
    return isSuccess;
  }
}
