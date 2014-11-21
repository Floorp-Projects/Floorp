/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.login;

import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.JSONWebTokenUtils;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.login.FxAccountLoginStateMachine.ExecuteDelegate;
import org.mozilla.gecko.fxa.login.FxAccountLoginTransition.LogMessage;
import org.mozilla.gecko.sync.ExtendedJSONObject;

public class Cohabiting extends TokensAndKeysState {
  private static final String LOG_TAG = Cohabiting.class.getSimpleName();

  public Cohabiting(String email, String uid, byte[] sessionToken, byte[] kA, byte[] kB, BrowserIDKeyPair keyPair) {
    super(StateLabel.Cohabiting, email, uid, sessionToken, kA, kB, keyPair);
  }

  @Override
  public void execute(final ExecuteDelegate delegate) {
    delegate.getClient().sign(sessionToken, keyPair.getPublic().toJSONObject(), delegate.getCertificateDurationInMilliseconds(),
        new BaseRequestDelegate<String>(this, delegate) {
      @Override
      public void handleSuccess(String certificate) {
        if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
          try {
            FxAccountConstants.pii(LOG_TAG, "Fetched certificate: " + certificate);
            ExtendedJSONObject c = JSONWebTokenUtils.parseCertificate(certificate);
            if (c != null) {
              FxAccountConstants.pii(LOG_TAG, "Header   : " + c.getObject("header"));
              FxAccountConstants.pii(LOG_TAG, "Payload  : " + c.getObject("payload"));
              FxAccountConstants.pii(LOG_TAG, "Signature: " + c.getString("signature"));
            } else {
              FxAccountConstants.pii(LOG_TAG, "Could not parse certificate!");
            }
          } catch (Exception e) {
            FxAccountConstants.pii(LOG_TAG, "Could not parse certificate!");
          }
        }
        delegate.handleTransition(new LogMessage("sign succeeded"), new Married(email, uid, sessionToken, kA, kB, keyPair, certificate));
      }
    });
  }
}
