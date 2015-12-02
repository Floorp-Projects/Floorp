/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake.stage;

import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.jpake.IncorrectZkpException;
import org.mozilla.gecko.sync.jpake.JPakeClient;
import org.mozilla.gecko.sync.jpake.JPakeCrypto;
import org.mozilla.gecko.sync.jpake.Zkp;
import org.mozilla.gecko.sync.setup.Constants;

public class ComputeFinalStage extends JPakeStage {

  @Override
  public void execute(JPakeClient jClient) {
    Logger.debug(LOG_TAG, "Computing final round.");

    // Check incoming message type.
    if (!jClient.jIncoming.get(Constants.JSON_KEY_TYPE).equals(jClient.theirSignerId + "2")) {
      Logger.error(LOG_TAG, "Invalid round 2 message: " + jClient.jIncoming.toJSONString());
      jClient.abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
      return;
    }

    // Check incoming message fields.
    ExtendedJSONObject iPayload;
    ExtendedJSONObject zkpPayload;
    try {
      iPayload = jClient.jIncoming.getObject(Constants.JSON_KEY_PAYLOAD);
      if (iPayload == null
          || iPayload.getObject(Constants.ZKP_KEY_ZKP_A) == null) {
        Logger.error(LOG_TAG,
            "Invalid round 2 message: " + jClient.jIncoming.toJSONString());
        jClient.abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
        return;
      }
      zkpPayload = iPayload.getObject(Constants.ZKP_KEY_ZKP_A);
    } catch (NonObjectJSONException e) {
      Logger.error(LOG_TAG, "JSON object Exception.", e);
      jClient.abort(Constants.JPAKE_ERROR_INVALID);
      return;
    }

    if (!jClient.theirSignerId.equals(zkpPayload.get(Constants.ZKP_KEY_ID))) {
      Logger.error(LOG_TAG, "Invalid round 2 message: " + jClient.jIncoming.toJSONString());
      jClient.abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
      return;
    }

    // Extract fields.
    jClient.jParty.otherA = new BigInteger((String) iPayload.get(Constants.ZKP_KEY_A), 16);

    // Extract ZKP.
    String gr = (String) zkpPayload.get(Constants.ZKP_KEY_GR);
    String b  = (String) zkpPayload.get(Constants.ZKP_KEY_B);
    String id = (String) zkpPayload.get(Constants.ZKP_KEY_ID);

    jClient.jParty.otherZkpA = new Zkp(new BigInteger(gr, 16), new BigInteger(b, 16), id);

    jClient.myKeyBundle = null;
    try {
      jClient.myKeyBundle = JPakeCrypto.finalRound(JPakeClient.secretAsBigInteger(jClient.secret), jClient.jParty);
    } catch (IncorrectZkpException e) {
      Logger.error(LOG_TAG, "ZKP mismatch");
      jClient.abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
      return;
    } catch (NoSuchAlgorithmException e) {
      Logger.error(LOG_TAG, "NoSuchAlgorithmException", e);
      jClient.abort(Constants.JPAKE_ERROR_INTERNAL);
      return;
    } catch (InvalidKeyException e) {
      Logger.error(LOG_TAG, "InvalidKeyException", e);
      jClient.abort(Constants.JPAKE_ERROR_INTERNAL);
      return;
    } catch (UnsupportedEncodingException e) {
      Logger.error(LOG_TAG, "UnsupportedEncodingException", e);
      jClient.abort(Constants.JPAKE_ERROR_INTERNAL);
      return;
    }

    // Run next stage.
    jClient.runNextStage();
  }

}
