/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.StringUtils;

import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;

/**
 * An abstraction around providing an email and authorization token to the auth
 * server.
 */
public class FxAccount20LoginDelegate {
  protected final byte[] emailUTF8;
  protected final byte[] authPW;

  public FxAccount20LoginDelegate(byte[] emailUTF8, byte[] quickStretchedPW) throws UnsupportedEncodingException, GeneralSecurityException {
    this.emailUTF8 = emailUTF8;
    this.authPW = FxAccountUtils.generateAuthPW(quickStretchedPW);
  }

  public ExtendedJSONObject getCreateBody() {
    final ExtendedJSONObject body = new ExtendedJSONObject();
    body.put("email", new String(emailUTF8, StringUtils.UTF_8));
    body.put("authPW", Utils.byte2Hex(authPW));
    return body;
  }
}
