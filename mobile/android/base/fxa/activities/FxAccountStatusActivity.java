/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.authenticator.FxAccountAuthenticator;

import android.accounts.Account;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

/**
 * Activity which displays account status.
 */
public class FxAccountStatusActivity extends FxAccountAbstractActivity {
  protected static final String LOG_TAG = FxAccountStatusActivity.class.getSimpleName();

  protected View connectionStatusUnverifiedView;
  protected View connectionStatusSignInView;
  protected View connectionStatusSyncingView;

  public FxAccountStatusActivity() {
    super(CANNOT_RESUME_WHEN_NO_ACCOUNTS_EXIST);
  }

  /**
   * {@inheritDoc}
   */
  @Override
  public void onCreate(Bundle icicle) {
    Logger.setThreadLogTag(FxAccountConstants.GLOBAL_LOG_TAG);
    Logger.debug(LOG_TAG, "onCreate(" + icicle + ")");

    super.onCreate(icicle);
    setContentView(R.layout.fxaccount_status);

    connectionStatusUnverifiedView = ensureFindViewById(null, R.id.unverified_view, "unverified view");
    connectionStatusSignInView = ensureFindViewById(null, R.id.sign_in_view, "sign in view");
    connectionStatusSyncingView = ensureFindViewById(null, R.id.syncing_view, "syncing view");

    launchActivityOnClick(connectionStatusSignInView, FxAccountUpdateCredentialsActivity.class);
  }

  @Override
  public void onResume() {
    super.onResume();
    refresh();
  }

  protected void refresh(Account account) {
    TextView email = (TextView) findViewById(R.id.email);

    if (account == null) {
      redirectToActivity(FxAccountGetStartedActivity.class);
      return;
    }

    AndroidFxAccount fxAccount = new AndroidFxAccount(this, account);

    email.setText(account.name);

    // Not as good as interrogating state machine, but will do for now.
    if (!fxAccount.isVerified()) {
      connectionStatusUnverifiedView.setVisibility(View.VISIBLE);
      connectionStatusSignInView.setVisibility(View.GONE);
      connectionStatusSyncingView.setVisibility(View.GONE);
      return;
    }

    if (fxAccount.getQuickStretchedPW() == null) {
      connectionStatusUnverifiedView.setVisibility(View.GONE);
      connectionStatusSignInView.setVisibility(View.VISIBLE);
      connectionStatusSyncingView.setVisibility(View.GONE);
      return;
    }

    connectionStatusUnverifiedView.setVisibility(View.GONE);
    connectionStatusSignInView.setVisibility(View.GONE);
    connectionStatusSyncingView.setVisibility(View.VISIBLE);
  }

  protected void refresh() {
    Account accounts[] = FxAccountAuthenticator.getFirefoxAccounts(this);
    if (accounts.length < 1) {
      refresh(null);
      return;
    }
    refresh(accounts[0]);
  }

  protected void dumpAccountDetails() {
    Account accounts[] = FxAccountAuthenticator.getFirefoxAccounts(this);
    if (accounts.length < 1) {
      return;
    }
    AndroidFxAccount fxAccount = new AndroidFxAccount(this, accounts[0]);
    fxAccount.dump();
  }

  protected void forgetAccountTokens() {
    Account accounts[] = FxAccountAuthenticator.getFirefoxAccounts(this);
    if (accounts.length < 1) {
      return;
    }
    AndroidFxAccount fxAccount = new AndroidFxAccount(this, accounts[0]);
    fxAccount.forgetAccountTokens();
    fxAccount.dump();
  }

  protected void forgetQuickStretchedPW() {
    Account accounts[] = FxAccountAuthenticator.getFirefoxAccounts(this);
    if (accounts.length < 1) {
      return;
    }
    AndroidFxAccount fxAccount = new AndroidFxAccount(this, accounts[0]);
    fxAccount.forgetQuickstretchedPW();
    fxAccount.dump();
  }

  public void onClickRefresh(View view) {
    Logger.debug(LOG_TAG, "Refreshing.");
    refresh();
  }

  public void onClickForgetAccountTokens(View view) {
    Logger.debug(LOG_TAG, "Forgetting account tokens.");
    forgetAccountTokens();
  }

  public void onClickForgetPassword(View view) {
    Logger.debug(LOG_TAG, "Forgetting quickStretchedPW.");
    forgetQuickStretchedPW();
  }

  public void onClickDumpAccountDetails(View view) {
    Logger.debug(LOG_TAG, "Dumping account details.");
    dumpAccountDetails();
  }

  public void onClickGetStarted(View view) {
    Logger.debug(LOG_TAG, "Launching get started activity.");
    redirectToActivity(FxAccountGetStartedActivity.class);
  }

  public void onClickVerify(View view) {
    Logger.debug(LOG_TAG, "Launching verification activity.");
  }

  public void onClickSignIn(View view) {
    Logger.debug(LOG_TAG, "Launching sign in again activity.");
  }
}
