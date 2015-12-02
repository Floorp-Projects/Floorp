/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake.stage;

import java.io.UnsupportedEncodingException;
import java.security.NoSuchAlgorithmException;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.jpake.BigIntegerHelper;
import org.mozilla.gecko.sync.jpake.JPakeClient;
import org.mozilla.gecko.sync.jpake.JPakeCrypto;
import org.mozilla.gecko.sync.jpake.JPakeJson;
import org.mozilla.gecko.sync.jpake.JPakeParty;
import org.mozilla.gecko.sync.jpake.Zkp;
import org.mozilla.gecko.sync.setup.Constants;

public class ComputeStepOneStage extends JPakeStage {

  @Override
  public void execute(JPakeClient jClient) {
    Logger.debug(LOG_TAG, "Computing round 1.");

    JPakeParty jClientParty = jClient.jParty;
    try {
      JPakeCrypto.round1(jClientParty, jClient.numGen);
    } catch (NoSuchAlgorithmException e) {
      Logger.error(LOG_TAG, "No such algorithm.", e);
      jClient.abort(Constants.JPAKE_ERROR_INTERNAL);
      return;
    } catch (UnsupportedEncodingException e) {
      Logger.error(LOG_TAG, "Unsupported encoding.", e);
      jClient.abort(Constants.JPAKE_ERROR_INVALID);
      return;
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Unexpected exception.", e);
      jClient.abort(Constants.JPAKE_ERROR_INTERNAL);
      return;
    }

    // Set outgoing message.
    ExtendedJSONObject jOne = new ExtendedJSONObject();
    jOne.put(Constants.ZKP_KEY_GX1,
        BigIntegerHelper.toEvenLengthHex(jClientParty.gx1));
    jOne.put(Constants.ZKP_KEY_GX2,
        BigIntegerHelper.toEvenLengthHex(jClientParty.gx2));

    Zkp zkp1 = jClientParty.zkp1;
    Zkp zkp2 = jClientParty.zkp2;
    ExtendedJSONObject jZkp1 = JPakeJson.makeJZkp(zkp1.gr, zkp1.b, jClient.mySignerId);
    ExtendedJSONObject jZkp2 = JPakeJson.makeJZkp(zkp2.gr, zkp2.b, jClient.mySignerId);

    jOne.put(Constants.ZKP_KEY_ZKP_X1, jZkp1);
    jOne.put(Constants.ZKP_KEY_ZKP_X2, jZkp2);

    jClient.jOutgoing = new ExtendedJSONObject();
    jClient.jOutgoing.put(Constants.JSON_KEY_TYPE, jClient.mySignerId + "1");
    jClient.jOutgoing.put(Constants.JSON_KEY_PAYLOAD, jOne);
    jClient.jOutgoing.put(Constants.JSON_KEY_VERSION, JPakeClient.KEYEXCHANGE_VERSION);
    Logger.debug(LOG_TAG, "Sending: " + jClient.jOutgoing.toJSONString());

    jClient.runNextStage();
  }
}
