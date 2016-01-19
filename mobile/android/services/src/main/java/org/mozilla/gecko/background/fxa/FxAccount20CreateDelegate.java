/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;

import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;

public class FxAccount20CreateDelegate {
  protected final byte[] emailUTF8;
  protected final byte[] authPW;
  protected final boolean preVerified;

  /**
   * Make a new "create account" delegate.
   *
   * @param emailUTF8
   *          email as UTF-8 bytes.
   * @param quickStretchedPW
   *          quick stretched password as bytes.
   * @param preVerified
   *          true if account should be marked already verified; only effective
   *          for non-production auth servers.
   * @throws UnsupportedEncodingException
   * @throws GeneralSecurityException
   */
  public FxAccount20CreateDelegate(byte[] emailUTF8, byte[] quickStretchedPW, boolean preVerified) throws UnsupportedEncodingException, GeneralSecurityException {
    this.emailUTF8 = emailUTF8;
    this.authPW = FxAccountUtils.generateAuthPW(quickStretchedPW);
    this.preVerified = preVerified;
  }

  public ExtendedJSONObject getCreateBody() throws FxAccountClientException {
    final ExtendedJSONObject body = new ExtendedJSONObject();
    try {
      body.put("email", new String(emailUTF8, "UTF-8"));
      body.put("authPW", Utils.byte2Hex(authPW));
      if (preVerified) {
        // Production endpoints do not allow preVerified; this assumes we only
        // set it when it's okay to send it.
        body.put("preVerified", preVerified);
      }
      return body;
    } catch (UnsupportedEncodingException e) {
      throw new FxAccountClientException(e);
    }
  }
}
