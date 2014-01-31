/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Engaged;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;

import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;
import android.widget.Toast;

/**
 * Activity which displays account created successfully screen to the user, and
 * starts them on the email verification path.
 */
public class FxAccountConfirmAccountActivity extends FxAccountAbstractActivity implements OnClickListener {
  private static final String LOG_TAG = FxAccountConfirmAccountActivity.class.getSimpleName();

  // Set in onCreate.
  protected TextView verificationLinkTextView;
  protected View resendLink;

  // Set in onResume.
  protected AndroidFxAccount fxAccount;

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
    if (state.getStateLabel() != StateLabel.Engaged) {
      Logger.warn(LOG_TAG, "Cannot confirm Firefox Account in state: " + state.getStateLabel());
      setResult(RESULT_CANCELED);
      finish();
      return;
    }

    final String email = fxAccount.getEmail();
    final String text = getResources().getString(R.string.fxaccount_confirm_account_verification_link, email);
    verificationLinkTextView.setText(text);

    boolean resendLinkShouldBeEnabled = ((Engaged) state).getSessionToken() != null;
    resendLink.setEnabled(resendLinkShouldBeEnabled);
    resendLink.setClickable(resendLinkShouldBeEnabled);
  }

  public static class FxAccountResendCodeTask extends FxAccountSetupTask<Void> {
    protected static final String LOG_TAG = FxAccountResendCodeTask.class.getSimpleName();

    protected final byte[] sessionToken;

    public FxAccountResendCodeTask(Context context, byte[] sessionToken, FxAccountClient client, RequestDelegate<Void> delegate) {
      super(context, null, client, delegate);
      this.sessionToken = sessionToken;
    }

    @Override
    protected InnerRequestDelegate<Void> doInBackground(Void... arg0) {
      try {
        client.resendCode(sessionToken, innerDelegate);
        latch.await();
        return innerDelegate;
      } catch (Exception e) {
        Logger.error(LOG_TAG, "Got exception signing in.", e);
        delegate.handleError(e);
      }
      return null;
    }
  }

  protected static class ResendCodeDelegate implements RequestDelegate<Void> {
    public final Context context;

    public ResendCodeDelegate(Context context) {
      this.context = context;
    }

    @Override
    public void handleError(Exception e) {
      Logger.warn(LOG_TAG, "Got exception requesting fresh confirmation link; ignoring.", e);
      Toast.makeText(context, R.string.fxaccount_confirm_account_verification_link_not_sent, Toast.LENGTH_LONG).show();
    }

    @Override
    public void handleFailure(FxAccountClientRemoteException e) {
      handleError(e);
    }

    @Override
    public void handleSuccess(Void result) {
      Toast.makeText(context, R.string.fxaccount_confirm_account_verification_link_sent, Toast.LENGTH_SHORT).show();
    }
  }

  public static void resendCode(Context context, AndroidFxAccount fxAccount) {
    RequestDelegate<Void> delegate = new ResendCodeDelegate(context);

    byte[] sessionToken;
    try {
      sessionToken = ((Engaged) fxAccount.getState()).getSessionToken();
    } catch (Exception e) {
      delegate.handleError(e);
      return;
    }
    if (sessionToken == null) {
      delegate.handleError(new IllegalStateException("sessionToken should not be null"));
      return;
    }

    Executor executor = Executors.newSingleThreadExecutor();
    FxAccountClient client = new FxAccountClient20(fxAccount.getAccountServerURI(), executor);
    new FxAccountResendCodeTask(context, sessionToken, client, delegate).execute();
  }

  @Override
  public void onClick(View v) {
    resendCode(this, fxAccount);
  }
}
