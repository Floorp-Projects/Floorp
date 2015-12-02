/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa;

import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.security.NoSuchAlgorithmException;

import org.json.simple.JSONObject;
import org.mozilla.gecko.background.fxa.FxAccountClient10.CreateDelegate;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.net.SRPConstants;

public class FxAccount10CreateDelegate implements CreateDelegate {
  protected final String     email;
  protected final String     mainSalt;
  protected final String     srpSalt;
  protected final BigInteger v;

  public FxAccount10CreateDelegate(String email, byte[] stretchedPWBytes, String mainSalt, String srpSalt) throws NoSuchAlgorithmException, UnsupportedEncodingException {
    this.email = email;
    this.mainSalt = mainSalt;
    this.srpSalt = srpSalt;
    byte[] srpSaltBytes = Utils.hex2Byte(srpSalt, FxAccountUtils.SALT_LENGTH_BYTES);
    this.v = FxAccountUtils.srpVerifierLowercaseV(email.getBytes("UTF-8"), stretchedPWBytes, srpSaltBytes, SRPConstants._2048.g, SRPConstants._2048.N);
  }

  @SuppressWarnings("unchecked")
  @Override
  public JSONObject getCreateBody() throws FxAccountClientException {
    final JSONObject body = new JSONObject();
    try {
      body.put("email", FxAccountUtils.bytes(email));
    } catch (UnsupportedEncodingException e) {
      throw new FxAccountClientException(e);
    }

    final JSONObject stretching = new JSONObject();
    stretching.put("type", "PBKDF2/scrypt/PBKDF2/v1");
    stretching.put("PBKDF2_rounds_1", 20000);
    stretching.put("scrypt_N", 65536);
    stretching.put("scrypt_r", 8);
    stretching.put("scrypt_p", 1);
    stretching.put("PBKDF2_rounds_2", 20000);
    stretching.put("salt", mainSalt);
    body.put("passwordStretching", stretching);

    final JSONObject srp = new JSONObject();
    srp.put("type", "SRP-6a/SHA256/2048/v1");
    srp.put("verifier", FxAccountUtils.hexModN(v, SRPConstants._2048.N));
    srp.put("salt", srpSalt);
    body.put("srp", srp);
    return body;
  }
}
