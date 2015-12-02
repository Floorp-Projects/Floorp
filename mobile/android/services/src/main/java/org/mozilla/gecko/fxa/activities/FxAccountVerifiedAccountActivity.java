/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.sync.setup.activities.ActivityUtils;

import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;

/**
 * Activity which displays "Account verified" success screen.
 */
public class FxAccountVerifiedAccountActivity extends FxAccountAbstractActivity {
  private static final String LOG_TAG = FxAccountVerifiedAccountActivity.class.getSimpleName();

  protected AndroidFxAccount fxAccount;

  public FxAccountVerifiedAccountActivity() {
    super(CANNOT_RESUME_WHEN_NO_ACCOUNTS_EXIST);
  }

  /**
   * {@inheritDoc}
   */
  @Override
  public void onCreate(Bundle icicle) {
    Logger.debug(LOG_TAG, "onCreate(" + icicle + ")");

    super.onCreate(icicle);
    setContentView(R.layout.fxaccount_account_verified);
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
    State state = fxAccount.getState();
    if (!state.verified) {
      Logger.warn(LOG_TAG, "Firefox Account is not verified; not displaying verified account activity.");
      setResult(RESULT_CANCELED);
      finish();
      return;
    }

    View backToBrowsingButton = ensureFindViewById(null, R.id.button, "back to browsing button");
    backToBrowsingButton.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        ActivityUtils.openURLInFennec(v.getContext(), null);
      }
    });
  }
}
