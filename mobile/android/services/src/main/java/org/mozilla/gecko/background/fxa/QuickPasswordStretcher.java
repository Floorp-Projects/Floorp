/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa;

import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;
import java.util.HashMap;
import java.util.Map;

import org.mozilla.gecko.sync.Utils;

public class QuickPasswordStretcher implements PasswordStretcher {
  protected final String password;
  protected final Map<String, String> cache = new HashMap<String, String>();

  public QuickPasswordStretcher(String password) {
    this.password = password;
  }

  @Override
  public synchronized byte[] getQuickStretchedPW(byte[] emailUTF8) throws UnsupportedEncodingException, GeneralSecurityException {
    if (emailUTF8 == null) {
      throw new IllegalArgumentException("emailUTF8 must not be null");
    }
    String key = Utils.byte2Hex(emailUTF8);
    if (!cache.containsKey(key)) {
      byte[] value = FxAccountUtils.generateQuickStretchedPW(emailUTF8, password.getBytes("UTF-8"));
      cache.put(key, Utils.byte2Hex(value));
      return value;
    }
    return Utils.hex2Byte(cache.get(key));
  }
}
