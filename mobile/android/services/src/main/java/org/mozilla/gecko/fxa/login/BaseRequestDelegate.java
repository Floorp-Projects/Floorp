/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.login;

import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.fxa.login.FxAccountLoginStateMachine.ExecuteDelegate;
import org.mozilla.gecko.fxa.login.FxAccountLoginTransition.AccountNeedsVerification;
import org.mozilla.gecko.fxa.login.FxAccountLoginTransition.LocalError;
import org.mozilla.gecko.fxa.login.FxAccountLoginTransition.RemoteError;

public abstract class BaseRequestDelegate<T> implements FxAccountClient20.RequestDelegate<T> {
  protected final ExecuteDelegate delegate;
  protected final State state;

  public BaseRequestDelegate(State state, ExecuteDelegate delegate) {
    this.delegate = delegate;
    this.state = state;
  }

  @Override
  public void handleFailure(FxAccountClientRemoteException e) {
    // Order matters here: we don't want to ignore upgrade required responses
    // even if the server tells us something else as well. We don't go directly
    // to the Doghouse on upgrade required; we want the user to try to update
    // their credentials, and then display UI telling them they need to upgrade.
    // Then they go to the Doghouse.
    if (e.isUpgradeRequired()) {
      delegate.handleTransition(new RemoteError(e), new Separated(state.email, state.uid, state.verified));
      return;
    }
    if (e.isInvalidAuthentication()) {
      delegate.handleTransition(new RemoteError(e), new Separated(state.email, state.uid, state.verified));
      return;
    }
    if (e.isUnverified()) {
      delegate.handleTransition(new AccountNeedsVerification(), state);
      return;
    }
    delegate.handleTransition(new RemoteError(e), state);
  }

  @Override
  public void handleError(Exception e) {
    delegate.handleTransition(new LocalError(e), state);
  }
}
