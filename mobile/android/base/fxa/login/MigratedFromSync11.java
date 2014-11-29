/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.login;

import org.mozilla.gecko.fxa.login.FxAccountLoginStateMachine.ExecuteDelegate;
import org.mozilla.gecko.fxa.login.FxAccountLoginTransition.PasswordRequired;

public class MigratedFromSync11 extends State {
  public final String password;

  public MigratedFromSync11(String email, String uid, boolean verified, String password) {
    super(StateLabel.MigratedFromSync11, email, uid, verified);
    // Null password is allowed.
    this.password = password;
  }

  @Override
  public void execute(final ExecuteDelegate delegate) {
    delegate.handleTransition(new PasswordRequired(), this);
  }

  @Override
  public Action getNeededAction() {
    return Action.NeedsFinishMigrating;
  }
}
