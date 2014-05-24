/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.tasks;

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

import android.content.Context;
import android.widget.Toast;

/**
 * A helper class that provides a simple interface for requesting
 * a Firefox Account verification email to be resent.
 */
public class FxAccountCodeResender {
  private static final String LOG_TAG = FxAccountCodeResender.class.getSimpleName();

  private static class FxAccountResendCodeTask extends FxAccountSetupTask<Void> {
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

  private static class ResendCodeDelegate implements RequestDelegate<Void> {
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

  /**
   * Resends the account verification email, and displays an appropriate
   * toast on both send success and failure. Note that because the underlying implementation
   * uses {@link AsyncTask}, the provided context must be UI-capable and
   * this method called from the UI thread.
   *
   * Note that it may actually be possible to run this (and the {@link AsyncTask}) method
   * from a background thread - but this hasn't been tested.
   *
   * @param context A UI-capable Android context.
   * @param fxAccount The Firefox Account to resend the code to.
   */
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
}
