/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa;

import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;

import org.json.simple.JSONObject;
import org.mozilla.gecko.background.fxa.FxAccountClient10.CreateDelegate;
import org.mozilla.gecko.sync.Utils;

/**
 * An abstraction around providing an email and authorization token to the auth
 * server.
 */
public class FxAccount20LoginDelegate implements CreateDelegate {
  protected final byte[] emailUTF8;
  protected final byte[] authPW;

  public FxAccount20LoginDelegate(byte[] emailUTF8, byte[] quickStretchedPW) throws UnsupportedEncodingException, GeneralSecurityException {
    this.emailUTF8 = emailUTF8;
    this.authPW = FxAccountUtils.generateAuthPW(quickStretchedPW);
  }

  @SuppressWarnings("unchecked")
  @Override
  public JSONObject getCreateBody() throws FxAccountClientException {
    final JSONObject body = new JSONObject();
    try {
      body.put("email", new String(emailUTF8, "UTF-8"));
      body.put("authPW", Utils.byte2Hex(authPW));
      return body;
    } catch (UnsupportedEncodingException e) {
      throw new FxAccountClientException(e);
    }
  }
}
