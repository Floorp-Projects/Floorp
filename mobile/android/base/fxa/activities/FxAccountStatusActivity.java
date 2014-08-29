/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.sync.setup.activities.LocaleAware.LocaleAwareFragmentActivity;

import android.accounts.Account;
import android.annotation.TargetApi;
import android.app.ActionBar;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.MenuItem;

/**
 * Activity which displays account status.
 */
public class FxAccountStatusActivity extends LocaleAwareFragmentActivity {
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

    maybeSetHomeButtonEnabled();
  }

  /**
   * Sufficiently recent Android versions need additional code to receive taps
   * on the status bar to go "up". See <a
   * href="http://stackoverflow.com/a/8953148">this stackoverflow answer</a> for
   * more information.
   */
  @TargetApi(Build.VERSION_CODES.ICE_CREAM_SANDWICH)
  protected void maybeSetHomeButtonEnabled() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
      Logger.debug(LOG_TAG, "Not enabling home button; version too low.");
      return;
    }
    final ActionBar actionBar = getActionBar();
    if (actionBar != null) {
      Logger.debug(LOG_TAG, "Enabling home button.");
      actionBar.setHomeButtonEnabled(true);
      return;
    }
    Logger.debug(LOG_TAG, "Not enabling home button.");
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

  @Override
  public boolean onOptionsItemSelected(MenuItem item) {
    int itemId = item.getItemId();
    switch (itemId) {
    case android.R.id.home:
      finish();
      return true;
    }
    return super.onOptionsItemSelected(item);
  }
}
