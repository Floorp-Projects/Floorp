/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.login;

import java.io.UnsupportedEncodingException;

import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
import org.mozilla.gecko.fxa.login.FxAccountLoginStateMachine.ExecuteDelegate;
import org.mozilla.gecko.fxa.login.FxAccountLoginTransition.LocalError;
import org.mozilla.gecko.fxa.login.FxAccountLoginTransition.LogMessage;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;

public class Promised extends State {
  protected final byte[] quickStretchedPW;
  protected final byte[] unwrapkB;

  public Promised(String email, String uid, boolean verified, byte[] unwrapkB, byte[] quickStretchedPW) {
    super(StateLabel.Promised, email, uid, verified);
    Utils.throwIfNull(email, uid, unwrapkB, quickStretchedPW);
    this.unwrapkB = unwrapkB;
    this.quickStretchedPW = quickStretchedPW;
  }

  @Override
  public ExtendedJSONObject toJSONObject() {
    ExtendedJSONObject o = super.toJSONObject();
    // Fields are non-null by constructor.
    o.put("unwrapkB", Utils.byte2Hex(unwrapkB));
    o.put("quickStretchedPW", Utils.byte2Hex(quickStretchedPW));
    return o;
  }

  @Override
  public void execute(final ExecuteDelegate delegate) {
    byte[] emailUTF8;
    try {
      emailUTF8 = email.getBytes("UTF-8");
    } catch (UnsupportedEncodingException e) {
      delegate.handleTransition(new LocalError(e), new Doghouse(email, uid, verified));
      return;
    }

    delegate.getClient().loginAndGetKeys(emailUTF8, quickStretchedPW, new BaseRequestDelegate<FxAccountClient20.LoginResponse>(this, delegate) {
      @Override
      public void handleSuccess(LoginResponse result) {
        delegate.handleTransition(new LogMessage("loginAndGetKeys succeeded"), new Engaged(email, uid, verified, unwrapkB, result.sessionToken, result.keyFetchToken));
      }
    });
  }

  @Override
  public Action getNeededAction() {
    return Action.NeedsVerification;
  }
}
