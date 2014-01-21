/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;

import android.os.Bundle;

/**
 * Activity which displays sign up/sign in screen to the user.
 */
public class FxAccountCreateAccountNotAllowedActivity extends FxAccountAbstractActivity {
  protected static final String LOG_TAG = FxAccountCreateAccountNotAllowedActivity.class.getSimpleName();

  public FxAccountCreateAccountNotAllowedActivity() {
    super(CANNOT_RESUME_WHEN_ACCOUNTS_EXIST);
  }

  /**
   * {@inheritDoc}
   */
  @Override
  public void onCreate(Bundle icicle) {
    Logger.debug(LOG_TAG, "onCreate(" + icicle + ")");

    super.onCreate(icicle);
    setContentView(R.layout.fxaccount_create_account_not_allowed);

    linkifyTextViews(null, new int[] { R.id.learn_more_link });
  }
}
