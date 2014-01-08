/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa;

import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;

import org.json.simple.JSONObject;

public class FxAccount20CreateDelegate extends FxAccount20LoginDelegate {
  protected final boolean preVerified;

  /**
   * Make a new "create account" delegate.
   *
   * @param emailUTF8
   *          email as UTF-8 bytes.
   * @param passwordUTF8
   *          password as UTF-8 bytes.
   * @param preVerified
   *          true if account should be marked already verified; only effective
   *          for non-production auth servers.
   * @throws UnsupportedEncodingException
   * @throws GeneralSecurityException
   */
  public FxAccount20CreateDelegate(byte[] emailUTF8, byte[] passwordUTF8, boolean preVerified) throws UnsupportedEncodingException, GeneralSecurityException {
    super(emailUTF8, passwordUTF8);
    this.preVerified = preVerified;
  }

  @SuppressWarnings("unchecked")
  @Override
  public JSONObject getCreateBody() throws FxAccountClientException {
    final JSONObject body = super.getCreateBody();
    body.put("preVerified", preVerified);
    return body;
  }
}
