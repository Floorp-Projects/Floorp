/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import java.security.NoSuchAlgorithmException;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.fxa.login.FxAccountLoginStateMachine.LoginStateMachineDelegate;
import org.mozilla.gecko.fxa.login.FxAccountLoginTransition.Transition;
import org.mozilla.gecko.fxa.login.Married;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.fxa.login.StateFactory;
import org.mozilla.gecko.fxa.sync.FxAccountNotificationManager;
import org.mozilla.gecko.fxa.sync.FxAccountSyncAdapter;

import android.content.Context;

public abstract class FxADefaultLoginStateMachineDelegate implements LoginStateMachineDelegate {
  protected final static String LOG_TAG = LoginStateMachineDelegate.class.getSimpleName();

  protected final Context context;
  protected final AndroidFxAccount fxAccount;
  protected final Executor executor;
  protected final FxAccountClient client;

  public FxADefaultLoginStateMachineDelegate(Context context, AndroidFxAccount fxAccount) {
    this.context = context;
    this.fxAccount = fxAccount;
    this.executor = Executors.newSingleThreadExecutor();
    this.client = new FxAccountClient20(fxAccount.getAccountServerURI(), executor);
  }

  abstract public void handleNotMarried(State notMarried);
  abstract public void handleMarried(Married married);

  @Override
  public FxAccountClient getClient() {
    return client;
  }

  @Override
  public long getCertificateDurationInMilliseconds() {
    return 12 * 60 * 60 * 1000;
  }

  @Override
  public long getAssertionDurationInMilliseconds() {
    return 15 * 60 * 1000;
  }

  @Override
  public BrowserIDKeyPair generateKeyPair() throws NoSuchAlgorithmException {
    return StateFactory.generateKeyPair();
  }

  @Override
  public void handleTransition(Transition transition, State state) {
    Logger.info(LOG_TAG, "handleTransition: " + transition + " to " + state.getStateLabel());
  }

  @Override
  public void handleFinal(State state) {
    Logger.info(LOG_TAG, "handleFinal: in " + state.getStateLabel());
    fxAccount.setState(state);
    // Update any notifications displayed.
    final FxAccountNotificationManager notificationManager = new FxAccountNotificationManager(FxAccountSyncAdapter.NOTIFICATION_ID);
    notificationManager.update(context, fxAccount);

    if (state.getStateLabel() != StateLabel.Married) {
      handleNotMarried(state);
      return;
    } else {
      handleMarried((Married) state);
    }
  }
}
