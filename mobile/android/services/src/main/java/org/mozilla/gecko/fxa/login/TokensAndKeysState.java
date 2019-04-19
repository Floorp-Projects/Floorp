/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.login;

import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;

public abstract class TokensAndKeysState extends State {
  protected final byte[] sessionToken;
  /* package-private */ final byte[] kSync;
  /* package-private */ final String kXCS;
  protected final BrowserIDKeyPair keyPair;

  /* package-private */ TokensAndKeysState(StateLabel stateLabel, String email, String uid, byte[] sessionToken, byte[] kSync, String kXCS, BrowserIDKeyPair keyPair) {
    super(stateLabel, email, uid, true);
    Utils.throwIfNull(sessionToken, kSync, kXCS, keyPair);
    this.sessionToken = sessionToken;
    this.kSync = kSync;
    this.kXCS = kXCS;
    this.keyPair = keyPair;
  }

  @Override
  public ExtendedJSONObject toJSONObject() {
    ExtendedJSONObject o = super.toJSONObject();
    // Fields are non-null by constructor.
    o.put("sessionToken", Utils.byte2Hex(sessionToken));
    o.put("kSync", Utils.byte2Hex(kSync));
    o.put("kXCS", kXCS);
    o.put("keyPair", keyPair.toJSONObject());
    return o;
  }

  @Override
  public byte[] getSessionToken() {
    return sessionToken;
  }

  @Override
  public Action getNeededAction() {
    return Action.None;
  }
}
