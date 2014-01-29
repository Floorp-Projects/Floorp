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
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;
import android.widget.ViewFlipper;

/**
 * Activity which displays account status.
 */
public class FxAccountStatusActivity extends FxAccountAbstractActivity {
  protected static final String LOG_TAG = FxAccountStatusActivity.class.getSimpleName();

  protected TextView syncStatusTextView;
  protected ViewFlipper connectionStatusViewFlipper;
  protected View connectionStatusUnverifiedView;
  protected View connectionStatusSignInView;
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

    syncStatusTextView = (TextView) ensureFindViewById(null, R.id.sync_status_text, "sync status text");
    connectionStatusViewFlipper = (ViewFlipper) ensureFindViewById(null, R.id.connection_status_view, "connection status frame layout");
    connectionStatusUnverifiedView = ensureFindViewById(null, R.id.unverified_view, "unverified view");
    connectionStatusSignInView = ensureFindViewById(null, R.id.sign_in_view, "sign in view");

    launchActivityOnClick(connectionStatusSignInView, FxAccountUpdateCredentialsActivity.class);

    emailTextView = (TextView) findViewById(R.id.email);

    if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
      createDebugButtons();
    }
  }

  @Override
  public void onResume() {
    super.onResume();
    refresh();
  }

  protected void showNeedsUpgrade() {
    syncStatusTextView.setText(R.string.fxaccount_status_sync);
    connectionStatusViewFlipper.setVisibility(View.VISIBLE);
    connectionStatusViewFlipper.setDisplayedChild(0);
  }

  protected void showNeedsPassword() {
    syncStatusTextView.setText(R.string.fxaccount_status_sync);
    connectionStatusViewFlipper.setVisibility(View.VISIBLE);
    connectionStatusViewFlipper.setDisplayedChild(1);
  }

  protected void showNeedsVerification() {
    syncStatusTextView.setText(R.string.fxaccount_status_sync);
    connectionStatusViewFlipper.setVisibility(View.VISIBLE);
    connectionStatusViewFlipper.setDisplayedChild(2);
  }

  protected void showConnected() {
    syncStatusTextView.setText(R.string.fxaccount_status_sync_enabled);
    connectionStatusViewFlipper.setVisibility(View.GONE);
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


  protected void createDebugButtons() {
    if (!FxAccountConstants.LOG_PERSONAL_INFORMATION) {
      return;
    }

    final LinearLayout existingUserView = (LinearLayout) findViewById(R.id.existing_user);
    if (existingUserView == null) {
      return;
    }

    final LinearLayout debugButtonsView = new LinearLayout(this);
    debugButtonsView.setOrientation(LinearLayout.VERTICAL);
    debugButtonsView.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
    existingUserView.addView(debugButtonsView, existingUserView.getChildCount());

    Button button;

    button = new Button(this);
    debugButtonsView.addView(button, debugButtonsView.getChildCount());
    button.setText("Refresh status view");
    button.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Logger.info(LOG_TAG, "Refreshing.");
        refresh();
      }
    });

    button = new Button(this);
    debugButtonsView.addView(button, debugButtonsView.getChildCount());
    button.setText("Dump account details");
    button.setOnClickListener(new OnClickListener() {
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

    button = new Button(this);
    debugButtonsView.addView(button, debugButtonsView.getChildCount());
    button.setText("Force sync");
    button.setOnClickListener(new OnClickListener() {
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

    button = new Button(this);
    debugButtonsView.addView(button, debugButtonsView.getChildCount());
    button.setText("Forget certificate (if applicable)");
    button.setOnClickListener(new OnClickListener() {
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

    button = new Button(this);
    debugButtonsView.addView(button, debugButtonsView.getChildCount());
    button.setText("Require password");
    button.setOnClickListener(new OnClickListener() {
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

    button = new Button(this);
    debugButtonsView.addView(button, debugButtonsView.getChildCount());
    button.setText("Require upgrade");
    button.setOnClickListener(new OnClickListener() {
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
}
