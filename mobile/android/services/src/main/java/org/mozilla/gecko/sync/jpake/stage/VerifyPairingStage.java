/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake.stage;

import java.io.UnsupportedEncodingException;

import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.CryptoInfo;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.jpake.JPakeClient;
import org.mozilla.gecko.sync.setup.Constants;

public class VerifyPairingStage extends JPakeStage {

  @Override
  public void execute(JPakeClient jClient) {
    Logger.debug(LOG_TAG, "Verifying their key.");

    ExtendedJSONObject verificationObj = jClient.jIncoming;
    String signerId = (String) verificationObj.get(Constants.JSON_KEY_TYPE);
    if (!signerId.equals(jClient.theirSignerId + "3")) {
      Logger.error(LOG_TAG, "Invalid round 3 message: " + verificationObj.toJSONString());
      jClient.abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
      return;
    }
    ExtendedJSONObject payload;
    try {
      payload = verificationObj.getObject(Constants.JSON_KEY_PAYLOAD);
    } catch (NonObjectJSONException e) {
      Logger.error(LOG_TAG, "JSON exception.", e);
      jClient.abort(Constants.JPAKE_ERROR_INVALID);
      return;
    }
    String theirCiphertext = (String) payload.get(Constants.JSON_KEY_CIPHERTEXT);
    String iv = (String) payload.get(Constants.JSON_KEY_IV);
    boolean correctPairing;
    try {
      correctPairing = verifyCiphertext(theirCiphertext, iv, jClient.myKeyBundle);
    } catch (UnsupportedEncodingException e) {
      Logger.error(LOG_TAG, "Unsupported encoding.", e);
      jClient.abort(Constants.JPAKE_ERROR_INTERNAL);
      return;
    } catch (CryptoException e) {
      Logger.error(LOG_TAG, "Crypto exception.", e);
      jClient.abort(Constants.JPAKE_ERROR_INTERNAL);
      return;
    }
    if (correctPairing) {
      Logger.debug(LOG_TAG, "Keys verified successfully.");
      jClient.paired = true;
      jClient.onPaired();
    } else {
      Logger.error(LOG_TAG, "Keys don't match.");
      jClient.abort(Constants.JPAKE_ERROR_KEYMISMATCH);
      return;
    }
  }

  /*
   * Helper function to verify an incoming ciphertext and IV against derived
   * keyBundle.
   *
   * (Made 'public' for testing and is a stateless function.)
   */

  public boolean verifyCiphertext(String theirCiphertext, String iv,
      KeyBundle keyBundle) throws UnsupportedEncodingException, CryptoException {
    byte[] cleartextBytes = JPakeClient.JPAKE_VERIFY_VALUE.getBytes("UTF-8");
    CryptoInfo encrypted = CryptoInfo.encrypt(cleartextBytes, Base64.decodeBase64(iv), keyBundle);
    String myCiphertext = new String(Base64.encodeBase64(encrypted.getMessage()), "UTF-8");
    return myCiphertext.equals(theirCiphertext);
  }
}
