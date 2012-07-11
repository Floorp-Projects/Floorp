/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake;

import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.security.GeneralSecurityException;
import java.security.InvalidKeyException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.crypto.HKDF;
import org.mozilla.gecko.sync.crypto.KeyBundle;

public class JPakeCrypto {
  private static final String LOG_TAG = "JPakeCrypto";

  /*
   * Primes P and Q, and generator G - from original Mozilla J-PAKE
   * implementation.
   */
  public static final BigInteger P = new BigInteger(
      "90066455B5CFC38F9CAA4A48B4281F292C260FEEF01FD61037E56258A7795A1C" +
      "7AD46076982CE6BB956936C6AB4DCFE05E6784586940CA544B9B2140E1EB523F" +
      "009D20A7E7880E4E5BFA690F1B9004A27811CD9904AF70420EEFD6EA11EF7DA1" +
      "29F58835FF56B89FAA637BC9AC2EFAAB903402229F491D8D3485261CD068699B" +
      "6BA58A1DDBBEF6DB51E8FE34E8A78E542D7BA351C21EA8D8F1D29F5D5D159394" +
      "87E27F4416B0CA632C59EFD1B1EB66511A5A0FBF615B766C5862D0BD8A3FE7A0" +
      "E0DA0FB2FE1FCB19E8F9996A8EA0FCCDE538175238FC8B0EE6F29AF7F642773E" +
      "BE8CD5402415A01451A840476B2FCEB0E388D30D4B376C37FE401C2A2C2F941D" +
      "AD179C540C1C8CE030D460C4D983BE9AB0B20F69144C1AE13F9383EA1C08504F" +
      "B0BF321503EFE43488310DD8DC77EC5B8349B8BFE97C2C560EA878DE87C11E3D" +
      "597F1FEA742D73EEC7F37BE43949EF1A0D15C3F3E3FC0A8335617055AC91328E" +
      "C22B50FC15B941D3D1624CD88BC25F3E941FDDC6200689581BFEC416B4B2CB73",
      16);

  public static final BigInteger Q = new BigInteger(
      "CFA0478A54717B08CE64805B76E5B14249A77A4838469DF7F7DC987EFCCFB11D",
      16);

  public static final BigInteger G = new BigInteger(
      "5E5CBA992E0A680D885EB903AEA78E4A45A469103D448EDE3B7ACCC54D521E37" +
      "F84A4BDD5B06B0970CC2D2BBB715F7B82846F9A0C393914C792E6A923E2117AB" +
      "805276A975AADB5261D91673EA9AAFFEECBFA6183DFCB5D3B7332AA19275AFA1" +
      "F8EC0B60FB6F66CC23AE4870791D5982AAD1AA9485FD8F4A60126FEB2CF05DB8" +
      "A7F0F09B3397F3937F2E90B9E5B9C9B6EFEF642BC48351C46FB171B9BFA9EF17" +
      "A961CE96C7E7A7CC3D3D03DFAD1078BA21DA425198F07D2481622BCE45969D9C" +
      "4D6063D72AB7A0F08B2F49A7CC6AF335E08C4720E31476B67299E231F8BD90B3" +
      "9AC3AE3BE0C6B6CACEF8289A2E2873D58E51E029CAFBD55E6841489AB66B5B4B" +
      "9BA6E2F784660896AFF387D92844CCB8B69475496DE19DA2E58259B090489AC8" +
      "E62363CDF82CFD8EF2A427ABCD65750B506F56DDE3B988567A88126B914D7828" +
      "E2B63A6D7ED0747EC59E0E0A23CE7D8A74C1D2C2A7AFB6A29799620F00E11C33" +
      "787F7DED3B30E1A22D09F1FBDA1ABBBFBF25CAE05A13F812E34563F99410E73B",
      16);

  /**
   *
   * Round 1 of J-PAKE protocol.
   * Generate x1, x2, and ZKP for other party.
   */
  public static void round1(JPakeParty jp, JPakeNumGenerator gen) throws NoSuchAlgorithmException, UnsupportedEncodingException {
    // Randomly select x1 from [0,q), x2 from [1,q).
    BigInteger x1 = gen.generateFromRange(Q); // [0, q)
    BigInteger x2 = jp.x2 = BigInteger.ONE.add(gen.generateFromRange(Q
        .subtract(BigInteger.ONE))); // [1, q)

    BigInteger gx1 = G.modPow(x1, P);
    BigInteger gx2 = G.modPow(x2, P);

    jp.gx1 = gx1;
    jp.gx2 = gx2;

    // Generate and store zero knowledge proofs.
    jp.zkp1 = createZkp(G, x1, gx1, jp.signerId, gen);
    jp.zkp2 = createZkp(G, x2, gx2, jp.signerId, gen);
  }

  /**
   * Round 2 of J-PAKE protocol.
   * Generate A and ZKP for A.
   * Verify ZKP from other party. Does not check for replay ZKP.
   */
  public static void round2(BigInteger secretValue, JPakeParty jp, JPakeNumGenerator gen)
      throws IncorrectZkpException, NoSuchAlgorithmException,
      Gx3OrGx4IsZeroOrOneException, UnsupportedEncodingException {

    Logger.debug(LOG_TAG, "round2 started.");

    // checkZkp does some additional checks, but we can throw a more informative exception here.
    if (BigInteger.ZERO.compareTo(jp.gx3) == 0 || BigInteger.ONE.compareTo(jp.gx3) == 0 ||
        BigInteger.ZERO.compareTo(jp.gx4) == 0 || BigInteger.ONE.compareTo(jp.gx4) == 0) {
      throw new Gx3OrGx4IsZeroOrOneException();
    }

    // Check ZKP.
    checkZkp(G, jp.gx3, jp.zkp3);
    checkZkp(G, jp.gx4, jp.zkp4);

    // Compute a = g^[(x1+x3+x4)*(x2*secret)].
    BigInteger y1 = jp.gx3.multiply(jp.gx4).mod(P).multiply(jp.gx1).mod(P);
    BigInteger y2 = jp.x2.multiply(secretValue).mod(P);

    BigInteger a  = y1.modPow(y2, P);
    jp.thisZkpA = createZkp(y1, y2, a, jp.signerId, gen);
    jp.thisA = a;

    Logger.debug(LOG_TAG, "round2 finished.");
  }

  /**
   * Final round of J-PAKE protocol.
   */
  public static KeyBundle finalRound(BigInteger secretValue, JPakeParty jp)
      throws IncorrectZkpException, NoSuchAlgorithmException, InvalidKeyException, UnsupportedEncodingException {
    Logger.debug(LOG_TAG, "Final round started.");
    BigInteger gb = jp.gx1.multiply(jp.gx2).mod(P).multiply(jp.gx3)
        .mod(P);
    checkZkp(gb, jp.otherA, jp.otherZkpA);

    // Calculate shared key g^(x1+x3)*x2*x4*secret, which is equivalent to
    // (B/g^(x2*x4*s))^x2 = (B*(g^x4)^x2^s^-1)^2.
    BigInteger k = jp.gx4.modPow(jp.x2.multiply(secretValue).negate().mod(Q), P).multiply(jp.otherA)
        .modPow(jp.x2, P);

    byte[] enc = new byte[32];
    byte[] hmac = new byte[32];
    generateKeyAndHmac(k, enc, hmac);

    Logger.debug(LOG_TAG, "Final round finished; returning key.");
    return new KeyBundle(enc, hmac);
  }

  // TODO Replace this function with the one in the  crypto library
  private static byte[] HMACSHA256(byte[] data, byte[] key) {
    byte[] result = null;
    try {
      Mac hmacSha256;
      hmacSha256 = Mac.getInstance("HmacSHA256");
      SecretKeySpec secret_key = new SecretKeySpec(key, "HmacSHA256");
      hmacSha256.init(secret_key);
      result = hmacSha256.doFinal(data);
    } catch (GeneralSecurityException e) {
      Logger.error(LOG_TAG, "Got exception calculating HMAC.", e);
    }
    return result;
  }

  /* Helper Methods */

  /*
   * Generate the ZKP b = r - x*h, and g^r, where h = hash(g, g^r, g^x, id). (We
   * pass in gx to save on an exponentiation of g^x)
   */
  private static Zkp createZkp(BigInteger g, BigInteger x, BigInteger gx,
      String id, JPakeNumGenerator gen) throws NoSuchAlgorithmException, UnsupportedEncodingException {
    // Generate random r for exponent.
    BigInteger r = gen.generateFromRange(Q);

    // Calculate g^r for ZKP.
    BigInteger gr = g.modPow(r, P);

    // Calculate the ZKP b value = (r-x*h) % q.
    BigInteger h = computeBHash(g, gr, gx, id);
    Logger.debug(LOG_TAG, "myhash: " + h.toString(16));

    // ZKP value = b = r-x*h
    BigInteger b = r.subtract(x.multiply(h)).mod(Q);

    return new Zkp(gr, b, id);
  }

  /*
   * Verify ZKP.
   */
  private static void checkZkp(BigInteger g, BigInteger gx, Zkp zkp)
      throws IncorrectZkpException, NoSuchAlgorithmException, UnsupportedEncodingException {

    BigInteger h = computeBHash(g, zkp.gr, gx, zkp.id);

    // Check parameters of zkp, and compare to computed hash. These shouldn't
    // fail.
    if (gx.compareTo(BigInteger.ONE) < 1) { // g^x > 1.
      Logger.error(LOG_TAG, "g^x > 1 fails.");
      throw new IncorrectZkpException();
    }
    if (gx.compareTo(P.subtract(BigInteger.ONE)) > -1) { // g^x < p-1
      Logger.error(LOG_TAG, "g^x < p-1 fails.");
      throw new IncorrectZkpException();
    }
    if (gx.modPow(Q, P).compareTo(BigInteger.ONE) != 0) {
      Logger.error(LOG_TAG, "g^x^q % p = 1 fails.");
      throw new IncorrectZkpException();
    }
    if (zkp.gr.compareTo(g.modPow(zkp.b, P).multiply(gx.modPow(h, P)).mod(P)) != 0) {
      // b = r-h*x ==> g^r = g^b*g^x^(h)
      Logger.debug(LOG_TAG, "gb*g(xh) = " + g.modPow(zkp.b, P).multiply(gx.modPow(h, P)).mod(P).toString(16));
      Logger.debug(LOG_TAG, "gr = " + zkp.gr.toString(16));
      Logger.debug(LOG_TAG, "b = " + zkp.b.toString(16));
      Logger.debug(LOG_TAG, "g^b = " + g.modPow(zkp.b, P).toString(16));
      Logger.debug(LOG_TAG, "g^(xh) = " + gx.modPow(h, P).toString(16));
      Logger.debug(LOG_TAG, "gx = " + gx.toString(16));
      Logger.debug(LOG_TAG, "h = " + h.toString(16));
      Logger.error(LOG_TAG, "zkp calculation incorrect.");
      throw new IncorrectZkpException();
    }
    Logger.debug(LOG_TAG, "*** ZKP SUCCESS ***");
  }

  /*
   * Use SHA-256 to compute a BigInteger hash of g, gr, gx values with
   * mySignerId to prevent replay. Does not make a twos-complement BigInteger
   * form hash.
   */
  private static BigInteger computeBHash(BigInteger g, BigInteger gr, BigInteger gx,
      String id) throws NoSuchAlgorithmException, UnsupportedEncodingException {
    MessageDigest sha = MessageDigest.getInstance("SHA-256");
    sha.reset();

    /*
     * Note: you should ensure the items in H(...) have clear boundaries. It
     * is simple if the other party knows sizes of g, gr, gx and signerID and
     * hence the boundary is unambiguous. If not, you'd better prepend each
     * item with its byte length, but I've omitted that here.
     */

    hashByteArrayWithLength(sha, BigIntegerHelper.BigIntegerToByteArrayWithoutSign(g));
    hashByteArrayWithLength(sha, BigIntegerHelper.BigIntegerToByteArrayWithoutSign(gr));
    hashByteArrayWithLength(sha, BigIntegerHelper.BigIntegerToByteArrayWithoutSign(gx));
    hashByteArrayWithLength(sha, id.getBytes("UTF-8"));

    byte[] hash = sha.digest();

    return BigIntegerHelper.ByteArrayToBigIntegerWithoutSign(hash);
  }

  /*
   * Update a hash with a byte array's length and the byte array.
   */
  private static void hashByteArrayWithLength(MessageDigest sha, byte[] data) {
    int length = data.length;
    byte[] b = new byte[] { (byte) (length >>> 8), (byte) (length & 0xff) };
    sha.update(b);
    sha.update(data);
  }

  /*
   * Helper function to generate encryption key and HMAC from a byte array.
   */
  public static void generateKeyAndHmac(BigInteger k, byte[] encOut, byte[] hmacOut) throws NoSuchAlgorithmException, InvalidKeyException {
   // Generate HMAC and Encryption keys from synckey.
    byte[] zerokey = new byte[32];
    byte[] prk = HMACSHA256(BigIntegerHelper.BigIntegerToByteArrayWithoutSign(k), zerokey);

    byte[] okm = HKDF.hkdfExpand(prk, HKDF.HMAC_INPUT, 32 * 2);
    System.arraycopy(okm, 0, encOut, 0, 32);
    System.arraycopy(okm, 32, hmacOut, 0, 32);
  }
}
