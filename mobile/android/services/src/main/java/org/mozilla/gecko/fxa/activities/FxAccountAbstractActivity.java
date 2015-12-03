/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import android.accounts.Account;
import android.app.Activity;
import android.content.Intent;

import org.mozilla.gecko.Locales.LocaleAwareActivity;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;

public abstract class FxAccountAbstractActivity extends LocaleAwareActivity {
  private static final String LOG_TAG = FxAccountAbstractActivity.class.getSimpleName();

  protected final boolean cannotResumeWhenAccountsExist;
  protected final boolean cannotResumeWhenNoAccountsExist;

  public static final int CAN_ALWAYS_RESUME = 0;
  public static final int CANNOT_RESUME_WHEN_ACCOUNTS_EXIST = 1 << 0;
  public static final int CANNOT_RESUME_WHEN_NO_ACCOUNTS_EXIST = 1 << 1;

  public FxAccountAbstractActivity(int resume) {
    super();
    this.cannotResumeWhenAccountsExist = 0 != (resume & CANNOT_RESUME_WHEN_ACCOUNTS_EXIST);
    this.cannotResumeWhenNoAccountsExist = 0 != (resume & CANNOT_RESUME_WHEN_NO_ACCOUNTS_EXIST);
  }

  /**
   * Many Firefox Accounts activities shouldn't display if an account already
   * exists. This function redirects as appropriate.
   *
   * @return true if redirected.
   */
  protected boolean redirectIfAppropriate() {
    if (cannotResumeWhenAccountsExist || cannotResumeWhenNoAccountsExist) {
      final Account account = FirefoxAccounts.getFirefoxAccount(this);
      if (cannotResumeWhenAccountsExist && account != null) {
        redirectToAction(FxAccountConstants.ACTION_FXA_STATUS);
        return true;
      }
      if (cannotResumeWhenNoAccountsExist && account == null) {
        redirectToAction(FxAccountConstants.ACTION_FXA_GET_STARTED);
        return true;
      }
    }
    return false;
  }

  @Override
  public void onResume() {
    super.onResume();
    redirectIfAppropriate();
  }

  @Override
  public void onBackPressed() {
      super.onBackPressed();
      overridePendingTransition(0, 0);
  }

  protected void launchActivity(Class<? extends Activity> activityClass) {
    Intent intent = new Intent(this, activityClass);
    // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
    // the soft keyboard not being shown for the started activity. Why, Android, why?
    intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
    startActivity(intent);
  }

  protected void redirectToAction(final String action) {
    final Intent intent = new Intent(action);
    // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
    // the soft keyboard not being shown for the started activity. Why, Android, why?
    intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
    startActivity(intent);
    finish();
  }
}
