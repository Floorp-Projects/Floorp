/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.SyncStatusListener;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Engaged;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.Action;
import org.mozilla.gecko.fxa.sync.FxAccountSyncStatusHelper;
import org.mozilla.gecko.fxa.tasks.FxAccountCodeResender;
import org.mozilla.gecko.sync.setup.activities.ActivityUtils;

/**
 * Activity which displays account created successfully screen to the user, and
 * starts them on the email verification path.
 */
public class FxAccountConfirmAccountActivity extends FxAccountAbstractActivity implements OnClickListener {
  private static final String LOG_TAG = FxAccountConfirmAccountActivity.class.getSimpleName();

  // Set in onCreate.
  protected TextView verificationLinkTextView;
  protected View resendLink;
  protected View changeEmail;

  // Set in onResume.
  protected AndroidFxAccount fxAccount;

  protected final InnerSyncStatusDelegate syncStatusDelegate = new InnerSyncStatusDelegate();

  public FxAccountConfirmAccountActivity() {
    super(CANNOT_RESUME_WHEN_NO_ACCOUNTS_EXIST);
  }

  /**
   * {@inheritDoc}
   */
  @Override
  public void onCreate(Bundle icicle) {
    Logger.debug(LOG_TAG, "onCreate(" + icicle + ")");

    super.onCreate(icicle);
    setContentView(R.layout.fxaccount_confirm_account);

    verificationLinkTextView = (TextView) ensureFindViewById(null, R.id.verification_link_text, "verification link text");
    resendLink = ensureFindViewById(null, R.id.resend_confirmation_email_link, "resend confirmation email link");
    resendLink.setOnClickListener(this);
    changeEmail = ensureFindViewById(null, R.id.change_confirmation_email_link, "change confirmation email address");
    changeEmail.setOnClickListener(this);

    View backToBrowsingButton = ensureFindViewById(null, R.id.button, "back to browsing button");
    backToBrowsingButton.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        ActivityUtils.openURLInFennec(v.getContext(), null);
        setResult(Activity.RESULT_OK);
        finish();
      }
    });
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

    FxAccountSyncStatusHelper.getInstance().startObserving(syncStatusDelegate);

    refresh();

    fxAccount.requestSync(FirefoxAccounts.NOW);
  }

  @Override
  public void onPause() {
    super.onPause();
    FxAccountSyncStatusHelper.getInstance().stopObserving(syncStatusDelegate);

    if (fxAccount != null) {
      fxAccount.requestSync(FirefoxAccounts.SOON);
    }
  }

  protected class InnerSyncStatusDelegate implements SyncStatusListener {
    protected final Runnable refreshRunnable = new Runnable() {
      @Override
      public void run() {
        refresh();
      }
    };

    @Override
    public Context getContext() {
      return FxAccountConfirmAccountActivity.this;
    }

    @Override
    public Account getAccount() {
      return fxAccount.getAndroidAccount();
    }

    @Override
    public void onSyncStarted() {
      Logger.info(LOG_TAG, "Got sync started message; ignoring.");
    }

    @Override
    public void onSyncFinished() {
      if (fxAccount == null) {
        return;
      }
      Logger.info(LOG_TAG, "Got sync finished message; refreshing.");
      runOnUiThread(refreshRunnable);
    }
  }

  protected void refresh() {
    final State state = fxAccount.getState();
    final Action neededAction = state.getNeededAction();
    switch (neededAction) {
    case NeedsVerification:
      // This is what we're here to handle.
      break;
    default:
      // We're not in the right place!  Redirect to status.
      Logger.warn(LOG_TAG, "No need to verify Firefox Account that needs action " + neededAction.toString() +
          " (in state " + state.getStateLabel() + ").");
      setResult(RESULT_CANCELED);
      redirectToAction(FxAccountConstants.ACTION_FXA_STATUS);
      return;
    }

    final String email = fxAccount.getEmail();
    final String text = getResources().getString(R.string.fxaccount_confirm_account_verification_link, email);
    verificationLinkTextView.setText(text);

    boolean resendLinkShouldBeEnabled = ((Engaged) state).getSessionToken() != null;
    resendLink.setEnabled(resendLinkShouldBeEnabled);
    resendLink.setClickable(resendLinkShouldBeEnabled);
  }

  @Override
  public void onClick(View v) {
    if (v.equals(resendLink)) {
        FxAccountCodeResender.resendCode(this, fxAccount);
    } else if (v.equals(changeEmail)) {
      final Account account = fxAccount.getAndroidAccount();
      Intent intent = new Intent(this, FxAccountGetStartedActivity.class);
      FxAccountStatusActivity.maybeDeleteAndroidAccount(this, account, intent);
    }
  }
}
