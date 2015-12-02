/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.login;

import org.mozilla.gecko.fxa.login.FxAccountLoginStateMachine.ExecuteDelegate;
import org.mozilla.gecko.fxa.login.FxAccountLoginTransition.LogMessage;


public class Doghouse extends State {
  public Doghouse(String email, String uid, boolean verified) {
    super(StateLabel.Doghouse, email, uid, verified);
  }

  @Override
  public void execute(final ExecuteDelegate delegate) {
    delegate.handleTransition(new LogMessage("Upgraded Firefox clients might know what to do here."), this);
  }

  @Override
  public Action getNeededAction() {
    return Action.NeedsUpgrade;
  }
}
