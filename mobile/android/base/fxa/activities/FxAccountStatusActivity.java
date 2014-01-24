/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.authenticator.FxAccountAuthenticator;
import org.mozilla.gecko.fxa.login.Married;
import org.mozilla.gecko.fxa.login.State;

import android.accounts.Account;
import android.content.ContentResolver;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;
import android.widget.ViewFlipper;

/**
 * Activity which displays account status.
 */
public class FxAccountStatusActivity extends FxAccountAbstractActivity {
  protected static final String LOG_TAG = FxAccountStatusActivity.class.getSimpleName();

  protected ViewFlipper connectionStatusViewFlipper;
  protected View connectionStatusUnverifiedView;
  protected View connectionStatusSignInView;
  protected View connectionStatusSyncingView;
  protected TextView emailTextView;

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

    connectionStatusViewFlipper = (ViewFlipper) ensureFindViewById(null, R.id.connection_status_view, "connection status frame layout");
    connectionStatusUnverifiedView = ensureFindViewById(null, R.id.unverified_view, "unverified vie w");
    connectionStatusSignInView = ensureFindViewById(null, R.id.sign_in_view, "sign in view");
    connectionStatusSyncingView = ensureFindViewById(null, R.id.syncing_view, "syncing view");

    launchActivityOnClick(connectionStatusSignInView, FxAccountUpdateCredentialsActivity.class);

    emailTextView = (TextView) findViewById(R.id.email);

    if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
      createDebugButtons();
    }
  }

  protected void createDebugButtons() {
    if (!FxAccountConstants.LOG_PERSONAL_INFORMATION) {
      return;
    }

    findViewById(R.id.debug_buttons).setVisibility(View.VISIBLE);

    findViewById(R.id.debug_refresh_button).setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Logger.info(LOG_TAG, "Refreshing.");
        refresh();
      }
    });

    findViewById(R.id.debug_dump_button).setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Logger.info(LOG_TAG, "Dumping account details.");
        Account accounts[] = FxAccountAuthenticator.getFirefoxAccounts(FxAccountStatusActivity.this);
        if (accounts.length < 1) {
          return;
        }
        AndroidFxAccount account = new AndroidFxAccount(FxAccountStatusActivity.this, accounts[0]);
        account.dump();
      }
    });

    findViewById(R.id.debug_sync_button).setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Logger.info(LOG_TAG, "Syncing.");
        Account accounts[] = FxAccountAuthenticator.getFirefoxAccounts(FxAccountStatusActivity.this);
        if (accounts.length < 1) {
          return;
        }
        final Bundle extras = new Bundle();
        extras.putBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, true);
        ContentResolver.requestSync(accounts[0], BrowserContract.AUTHORITY, extras);
        // No sense refreshing, since the sync will complete in the future.
      }
    });

    findViewById(R.id.debug_forget_certificate_button).setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Account accounts[] = FxAccountAuthenticator.getFirefoxAccounts(FxAccountStatusActivity.this);
        if (accounts.length < 1) {
          return;
        }
        AndroidFxAccount account = new AndroidFxAccount(FxAccountStatusActivity.this, accounts[0]);
        State state = account.getState();
        try {
          Married married = (Married) state;
          Logger.info(LOG_TAG, "Moving to Cohabiting state: Forgetting certificate.");
          account.setState(married.makeCohabitingState());
          refresh();
        } catch (ClassCastException e) {
          Logger.info(LOG_TAG, "Not in Married state; can't forget certificate.");
          // Ignore.
        }
      }
    });

    findViewById(R.id.debug_require_password_button).setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Logger.info(LOG_TAG, "Moving to Separated state: Forgetting password.");
        Account accounts[] = FxAccountAuthenticator.getFirefoxAccounts(FxAccountStatusActivity.this);
        if (accounts.length < 1) {
          return;
        }
        AndroidFxAccount account = new AndroidFxAccount(FxAccountStatusActivity.this, accounts[0]);
        State state = account.getState();
        account.setState(state.makeSeparatedState());
        refresh();
      }
    });

    findViewById(R.id.debug_require_upgrade_button).setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Logger.info(LOG_TAG, "Moving to Doghouse state: Requiring upgrade.");
        Account accounts[] = FxAccountAuthenticator.getFirefoxAccounts(FxAccountStatusActivity.this);
        if (accounts.length < 1) {
          return;
        }
        AndroidFxAccount account = new AndroidFxAccount(FxAccountStatusActivity.this, accounts[0]);
        State state = account.getState();
        account.setState(state.makeDoghouseState());
        refresh();
      }
    });
  }

  @Override
  public void onResume() {
    super.onResume();
    refresh();
  }

  protected void showNeedsUpgrade() {
    connectionStatusViewFlipper.setDisplayedChild(0);
  }

  protected void showNeedsPassword() {
    connectionStatusViewFlipper.setDisplayedChild(1);
  }

  protected void showNeedsVerification() {
    connectionStatusViewFlipper.setDisplayedChild(2);
  }

  protected void showConnected() {
    connectionStatusViewFlipper.setDisplayedChild(3);
  }

  protected void refresh(Account account) {
    if (account == null) {
      redirectToActivity(FxAccountGetStartedActivity.class);
      return;
    }
    emailTextView.setText(account.name);

    // Interrogate the Firefox Account's state.
    AndroidFxAccount fxAccount = new AndroidFxAccount(this, account);
    State state = fxAccount.getState();
    switch (state.getNeededAction()) {
    case NeedsUpgrade:
      showNeedsUpgrade();
      break;
    case NeedsPassword:
      showNeedsPassword();
      break;
    case NeedsVerification:
      showNeedsVerification();
      break;
    default:
      showConnected();
    }
  }

  protected void refresh() {
    Account accounts[] = FxAccountAuthenticator.getFirefoxAccounts(this);
    if (accounts.length < 1) {
      refresh(null);
      return;
    }
    refresh(accounts[0]);
  }
}
