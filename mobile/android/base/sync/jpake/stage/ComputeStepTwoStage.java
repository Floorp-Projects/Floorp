/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake.stage;

import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.security.NoSuchAlgorithmException;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.jpake.BigIntegerHelper;
import org.mozilla.gecko.sync.jpake.Gx3OrGx4IsZeroOrOneException;
import org.mozilla.gecko.sync.jpake.IncorrectZkpException;
import org.mozilla.gecko.sync.jpake.JPakeClient;
import org.mozilla.gecko.sync.jpake.JPakeCrypto;
import org.mozilla.gecko.sync.jpake.JPakeJson;
import org.mozilla.gecko.sync.jpake.Zkp;
import org.mozilla.gecko.sync.setup.Constants;

public class ComputeStepTwoStage extends JPakeStage {

  @Override
  public void execute(JPakeClient jClient) {
    Logger.debug(LOG_TAG, "Computing round 2.");

    // Check incoming message sender.
    if (!jClient.jIncoming.get(Constants.JSON_KEY_TYPE).equals(jClient.theirSignerId + "1")) {
      Logger.error(LOG_TAG, "Invalid round 1 message: " + jClient.jIncoming.toJSONString());
      jClient.abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
      return;
    }

    // Check incoming message fields.
    ExtendedJSONObject iPayload;
    try {
      iPayload = jClient.jIncoming.getObject(Constants.JSON_KEY_PAYLOAD);
    } catch (NonObjectJSONException e) {
      Logger.error(LOG_TAG, "JSON object exception.", e);
      jClient.abort(Constants.JPAKE_ERROR_INVALID);
      return;
    }
    if (iPayload == null) {
      Logger.error(LOG_TAG, "Invalid round 1 message: " + jClient.jIncoming.toJSONString());
      jClient.abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
      return;
    }
    ExtendedJSONObject zkpPayload3;
    ExtendedJSONObject zkpPayload4;
    try {
      zkpPayload3 = iPayload.getObject(Constants.ZKP_KEY_ZKP_X1);
      zkpPayload4 = iPayload.getObject(Constants.ZKP_KEY_ZKP_X2);
    } catch (NonObjectJSONException e1) {
      jClient.abort(Constants.JPAKE_ERROR_INVALID);
      return;
    }

    if (zkpPayload3 == null || zkpPayload4 == null) {
      Logger.error(LOG_TAG, "Invalid round 1 zkpPayload message");
      jClient.abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
      return;
    }

    if (!jClient.theirSignerId.equals(zkpPayload3.get(Constants.ZKP_KEY_ID)) ||
        !jClient.theirSignerId.equals(zkpPayload4.get(Constants.ZKP_KEY_ID))) {
      Logger.error(LOG_TAG, "Invalid round 1 zkpPayload message");
      jClient.abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
      return;
    }

    // Extract message fields.
    jClient.jParty.gx3 = new BigInteger((String) iPayload.get(Constants.ZKP_KEY_GX1), 16);
    jClient.jParty.gx4 = new BigInteger((String) iPayload.get(Constants.ZKP_KEY_GX2), 16);

    // Extract ZKPs.
    String zkp3_gr = (String) zkpPayload3.get(Constants.ZKP_KEY_GR);
    String zkp3_b  = (String) zkpPayload3.get(Constants.ZKP_KEY_B);
    String zkp3_id = (String) zkpPayload3.get(Constants.ZKP_KEY_ID);

    String zkp4_gr = (String) zkpPayload4.get(Constants.ZKP_KEY_GR);
    String zkp4_b  = (String) zkpPayload4.get(Constants.ZKP_KEY_B);
    String zkp4_id = (String) zkpPayload4.get(Constants.ZKP_KEY_ID);

    jClient.jParty.zkp3 = new Zkp(new BigInteger(zkp3_gr, 16), new BigInteger(zkp3_b, 16), zkp3_id);
    jClient.jParty.zkp4 = new Zkp(new BigInteger(zkp4_gr, 16), new BigInteger(zkp4_b, 16), zkp4_id);

    // J-PAKE round 2.
    try {
      JPakeCrypto.round2(JPakeClient.secretAsBigInteger(jClient.secret), jClient.jParty, jClient.numGen);
    } catch (Gx3OrGx4IsZeroOrOneException e) {
      Logger.error(LOG_TAG, "gx3 and gx4 cannot equal 0 or 1.");
      jClient.abort(Constants.JPAKE_ERROR_INTERNAL);
      return;
    } catch (IncorrectZkpException e) {
      Logger.error(LOG_TAG, "ZKP mismatch");
      jClient.abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
      return;
    } catch (NoSuchAlgorithmException e) {
      Logger.error(LOG_TAG, "NoSuchAlgorithmException", e);
      jClient.abort(Constants.JPAKE_ERROR_INTERNAL);
      return;
    } catch (UnsupportedEncodingException e) {
      Logger.error(LOG_TAG, "UnsupportedEncodingException", e);
      jClient.abort(Constants.JPAKE_ERROR_INTERNAL);
      return;
    }

    // Make outgoing payload.
    Zkp zkpA = jClient.jParty.thisZkpA;
    ExtendedJSONObject oPayload = new ExtendedJSONObject();
    ExtendedJSONObject jZkpA = JPakeJson.makeJZkp(zkpA.gr, zkpA.b, zkpA.id);
    oPayload.put(Constants.ZKP_KEY_A, BigIntegerHelper.toEvenLengthHex(jClient.jParty.thisA));
    oPayload.put(Constants.ZKP_KEY_ZKP_A, jZkpA);

    // Make outgoing message.
    jClient.jOutgoing = new ExtendedJSONObject();
    jClient.jOutgoing.put(Constants.JSON_KEY_TYPE, jClient.mySignerId + "2");
    jClient.jOutgoing.put(Constants.JSON_KEY_VERSION, JPakeClient.KEYEXCHANGE_VERSION);
    jClient.jOutgoing.put(Constants.JSON_KEY_PAYLOAD, oPayload);

    jClient.runNextStage();
  }
}
