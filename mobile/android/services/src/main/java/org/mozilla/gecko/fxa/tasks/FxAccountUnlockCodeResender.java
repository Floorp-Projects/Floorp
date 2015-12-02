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

import android.content.Context;
import android.os.AsyncTask;
import android.widget.Toast;

/**
 * A helper class that provides a simple interface for requesting a Firefox
 * Account unlock account email be (re)-sent.
 */
public class FxAccountUnlockCodeResender {
  private static final String LOG_TAG = FxAccountUnlockCodeResender.class.getSimpleName();

  private static class FxAccountUnlockCodeTask extends FxAccountSetupTask<Void> {
    protected static final String LOG_TAG = FxAccountUnlockCodeTask.class.getSimpleName();

    protected final byte[] emailUTF8;

    public FxAccountUnlockCodeTask(Context context, byte[] emailUTF8, FxAccountClient client, RequestDelegate<Void> delegate) {
      super(context, null, client, null, delegate);
      this.emailUTF8 = emailUTF8;
    }

    @Override
    protected InnerRequestDelegate<Void> doInBackground(Void... arg0) {
      try {
        client.resendUnlockCode(emailUTF8, innerDelegate);
        latch.await();
        return innerDelegate;
      } catch (Exception e) {
        Logger.error(LOG_TAG, "Got exception signing in.", e);
        delegate.handleError(e);
      }
      return null;
    }
  }

  private static class ResendUnlockCodeDelegate implements RequestDelegate<Void> {
    public final Context context;

    public ResendUnlockCodeDelegate(Context context) {
      this.context = context;
    }

    @Override
    public void handleError(Exception e) {
      Logger.warn(LOG_TAG, "Got exception requesting fresh unlock code; ignoring.", e);
      Toast.makeText(context, R.string.fxaccount_unlock_code_not_sent, Toast.LENGTH_LONG).show();
    }

    @Override
    public void handleFailure(FxAccountClientRemoteException e) {
      handleError(e);
    }

    @Override
    public void handleSuccess(Void result) {
      Toast.makeText(context, R.string.fxaccount_unlock_code_sent, Toast.LENGTH_SHORT).show();
    }
  }

  /**
   * Resends the account unlock email, and displays an appropriate toast on both
   * send success and failure. Note that because the underlying implementation
   * uses {@link AsyncTask}, the provided context must be UI-capable and this
   * method called from the UI thread.
   *
   * Note that it may actually be possible to run this (and the
   * {@link AsyncTask}) method from a background thread - but this hasn't been
   * tested.
   *
   * @param context
   *          A UI-capable Android context.
   * @param authServerURI
   *          to send request to.
   * @param emailUTF8
   *          bytes of email address identifying account; null indicates a local failure.
   */
  public static void resendUnlockCode(Context context, String authServerURI, byte[] emailUTF8) {
    RequestDelegate<Void> delegate = new ResendUnlockCodeDelegate(context);

    if (emailUTF8 == null) {
      delegate.handleError(new IllegalArgumentException("emailUTF8 must not be null"));
      return;
    }

    final Executor executor = Executors.newSingleThreadExecutor();
    final FxAccountClient client = new FxAccountClient20(authServerURI, executor);
    new FxAccountUnlockCodeTask(context, emailUTF8, client, delegate).execute();
  }
}
