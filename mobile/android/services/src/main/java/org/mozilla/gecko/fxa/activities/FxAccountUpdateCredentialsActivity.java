/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;

import android.content.Intent;

/**
 * Activity which displays a screen for updating the local password.
 */
public class FxAccountUpdateCredentialsActivity extends FxAccountAbstractUpdateCredentialsActivity {
  protected static final String LOG_TAG = FxAccountUpdateCredentialsActivity.class.getSimpleName();

  public FxAccountUpdateCredentialsActivity() {
    super(R.layout.fxaccount_update_credentials);
  }

  @Override
  public void onResume() {
    super.onResume();
    this.fxAccount = getAndroidFxAccount();
    if (fxAccount == null) {
      Logger.warn(LOG_TAG, "Could not get Firefox Account.");
      setResult(RESULT_CANCELED);
      finish();
      return;
    }
    final State state = fxAccount.getState();
    if (state.getStateLabel() != StateLabel.Separated) {
      Logger.warn(LOG_TAG, "Cannot update credentials from Firefox Account in state: " + state.getStateLabel());
      setResult(RESULT_CANCELED);
      finish();
      return;
    }
    emailEdit.setText(fxAccount.getEmail());
  }

  @Override
  public Intent makeSuccessIntent(String email, LoginResponse result) {
    // We don't show anything after updating credentials. The updating Activity
    // sets its result to OK and the user is returned to the previous task,
    // which is often the Status Activity.
    return null;
  }
}
