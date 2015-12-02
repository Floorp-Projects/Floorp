/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.login;

import java.security.NoSuchAlgorithmException;
import java.util.EnumSet;
import java.util.Set;

import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.fxa.login.FxAccountLoginTransition.Transition;
import org.mozilla.gecko.fxa.login.State.StateLabel;

public class FxAccountLoginStateMachine {
  public static final String LOG_TAG = FxAccountLoginStateMachine.class.getSimpleName();

  public interface LoginStateMachineDelegate {
    public FxAccountClient getClient();
    public long getCertificateDurationInMilliseconds();
    public long getAssertionDurationInMilliseconds();
    public void handleTransition(Transition transition, State state);
    public void handleFinal(State state);
    public BrowserIDKeyPair generateKeyPair() throws NoSuchAlgorithmException;
  }

  public static class ExecuteDelegate {
    protected final LoginStateMachineDelegate delegate;
    protected final StateLabel desiredStateLabel;
    // It's as difficult to detect arbitrary cycles as repeated states.
    protected final Set<StateLabel> stateLabelsSeen = EnumSet.noneOf(StateLabel.class);

    protected ExecuteDelegate(StateLabel initialStateLabel, StateLabel desiredStateLabel, LoginStateMachineDelegate delegate) {
      this.delegate = delegate;
      this.desiredStateLabel = desiredStateLabel;
      this.stateLabelsSeen.add(initialStateLabel);
    }

    public FxAccountClient getClient() {
      return delegate.getClient();
    }

    public long getCertificateDurationInMilliseconds() {
      return delegate.getCertificateDurationInMilliseconds();
    }

    public long getAssertionDurationInMilliseconds() {
      return delegate.getAssertionDurationInMilliseconds();
    }

    public BrowserIDKeyPair generateKeyPair() throws NoSuchAlgorithmException {
      return delegate.generateKeyPair();
    }

    public void handleTransition(Transition transition, State state) {
      // Always trigger the transition callback.
      delegate.handleTransition(transition, state);

      // Possibly trigger the final callback. We trigger if we're at our desired
      // state, or if we've seen this state before.
      StateLabel stateLabel = state.getStateLabel();
      if (stateLabel == desiredStateLabel || stateLabelsSeen.contains(stateLabel)) {
        delegate.handleFinal(state);
        return;
      }

      // If this wasn't the last state, leave a bread crumb and move on to the
      // next state.
      stateLabelsSeen.add(stateLabel);
      state.execute(this);
    }
  }

  public void advance(State initialState, final StateLabel desiredStateLabel, final LoginStateMachineDelegate delegate) {
    if (initialState.getStateLabel() == desiredStateLabel) {
      // We're already where we want to be!
      delegate.handleFinal(initialState);
      return;
    }
    ExecuteDelegate executeDelegate = new ExecuteDelegate(initialState.getStateLabel(), desiredStateLabel, delegate);
    initialState.execute(executeDelegate);
  }
}
