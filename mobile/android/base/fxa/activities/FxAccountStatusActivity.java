/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;

import android.accounts.Account;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;

/**
 * Activity which displays account status.
 */
public class FxAccountStatusActivity extends FragmentActivity {
  private static final String LOG_TAG = FxAccountStatusActivity.class.getSimpleName();

  protected FxAccountStatusFragment statusFragment;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // Display the fragment as the content.
    statusFragment = new FxAccountStatusFragment();
    getSupportFragmentManager()
      .beginTransaction()
      .replace(android.R.id.content, statusFragment)
      .commit();
  }

  @Override
  public void onResume() {
    super.onResume();

    final AndroidFxAccount fxAccount = getAndroidFxAccount();
    if (fxAccount == null) {
      Logger.warn(LOG_TAG, "Could not get Firefox Account.");

      // Gracefully redirect to get started.
      Intent intent = new Intent(this, FxAccountGetStartedActivity.class);
      // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
      // the soft keyboard not being shown for the started activity. Why, Android, why?
      intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
      startActivity(intent);

      setResult(RESULT_CANCELED);
      finish();
      return;
    }
    statusFragment.refresh(fxAccount);
  }

  /**
   * Helper to fetch (unique) Android Firefox Account if one exists, or return null.
   */
  protected AndroidFxAccount getAndroidFxAccount() {
    Account account = FirefoxAccounts.getFirefoxAccount(this);
    if (account == null) {
      return null;
    }
    return new AndroidFxAccount(this, account);
  }
}
