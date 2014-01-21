/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;
import java.util.concurrent.CountDownLatch;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.fxa.activities.FxAccountSetupTask.InnerRequestDelegate;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import android.app.ProgressDialog;
import android.content.Context;
import android.os.AsyncTask;
import ch.boye.httpclientandroidlib.HttpResponse;

/**
 * An <code>AsyncTask</code> wrapper around signing up for, and signing in to, a
 * Firefox Account.
 * <p>
 * It's strange to add explicit blocking to callback-threading code, but we do
 * it here to take advantage of Android's built in support for background work.
 * We really want to avoid making a threading mistake that brings down the whole
 * process.
 */
abstract class FxAccountSetupTask<T> extends AsyncTask<Void, Void, InnerRequestDelegate<T>> {
  protected static final String LOG_TAG = FxAccountSetupTask.class.getSimpleName();

  protected final Context context;
  protected final String email;
  protected final byte[] emailUTF8;
  protected final String password;
  protected final byte[] passwordUTF8;
  protected final FxAccountClient20 client;

  protected ProgressDialog progressDialog = null;

  // Initialized lazily.
  protected byte[] quickStretchedPW;

  // AsyncTask's are one-time-use, so final members are fine.
  protected final CountDownLatch latch = new CountDownLatch(1);
  protected final InnerRequestDelegate<T> innerDelegate = new InnerRequestDelegate<T>(latch);

  protected final RequestDelegate<T> delegate;

  public FxAccountSetupTask(Context context, String email, String password, FxAccountClient20 client, RequestDelegate<T> delegate) throws UnsupportedEncodingException, GeneralSecurityException {
    this.context = context;
    this.email = email;
    this.emailUTF8 = email.getBytes("UTF-8");
    this.password = password;
    this.passwordUTF8 = password.getBytes("UTF-8");
    this.client = client;
    this.delegate = delegate;
  }

  /**
   * Stretching the password is expensive, so we compute the stretched value lazily.
   *
   * @return stretched password.
   * @throws GeneralSecurityException
   * @throws UnsupportedEncodingException
   */
  public byte[] generateQuickStretchedPW() throws UnsupportedEncodingException, GeneralSecurityException {
    if (this.quickStretchedPW == null) {
      this.quickStretchedPW = FxAccountUtils.generateQuickStretchedPW(emailUTF8, passwordUTF8);
    }
    return this.quickStretchedPW;
  }

  @Override
  protected void onPreExecute() {
    progressDialog = new ProgressDialog(context);
    progressDialog.setTitle("Firefox Account..."); // XXX.
    progressDialog.setMessage("Please wait.");
    progressDialog.setCancelable(false);
    progressDialog.setIndeterminate(true);
    progressDialog.show();
  }

  @Override
  protected void onPostExecute(InnerRequestDelegate<T> result) {
    if (progressDialog != null) {
      progressDialog.dismiss();
    }

    // We are on the UI thread, and need to invoke these callbacks here to allow UI updating.
    if (result.response != null) {
      delegate.handleSuccess(result.response);
    } else if (result.exception instanceof HTTPFailureException) {
      HTTPFailureException e = (HTTPFailureException) result.exception;
      delegate.handleFailure(e.response.getStatusCode(), e.response.httpResponse());
    } else if (innerDelegate.exception != null) {
      delegate.handleError(innerDelegate.exception);
    } else {
      delegate.handleError(new IllegalStateException("Got bad state."));
    }
  }

  @Override
  protected void onCancelled(InnerRequestDelegate<T> result) {
    if (progressDialog != null) {
      progressDialog.dismiss();
    }
    delegate.handleError(new IllegalStateException("Task was cancelled."));
  }

  protected static class InnerRequestDelegate<T> implements RequestDelegate<T> {
    protected final CountDownLatch latch;
    public T response = null;
    public Exception exception = null;

    protected InnerRequestDelegate(CountDownLatch latch) {
      this.latch = latch;
    }

    @Override
    public void handleError(Exception e) {
      Logger.error(LOG_TAG, "Got exception.");
      this.exception = e;
      latch.countDown();
    }

    @Override
    public void handleFailure(int status, HttpResponse response) {
      Logger.warn(LOG_TAG, "Got failure.");
      this.exception = new HTTPFailureException(new SyncStorageResponse(response));
      latch.countDown();
    }

    @Override
    public void handleSuccess(T result) {
      Logger.info(LOG_TAG, "Got success.");
      this.response = result;
      latch.countDown();
    }
  }

  public static class FxAccountSignUpTask extends FxAccountSetupTask<String> {
    protected static final String LOG_TAG = FxAccountSignUpTask.class.getSimpleName();

    public FxAccountSignUpTask(Context context, String email, String password, FxAccountClient20 client, RequestDelegate<String> delegate) throws UnsupportedEncodingException, GeneralSecurityException {
      super(context, email, password, client, delegate);
    }

    @Override
    protected InnerRequestDelegate<String> doInBackground(Void... arg0) {
      try {
        client.createAccount(emailUTF8, generateQuickStretchedPW(), false, innerDelegate);
        latch.await();
        return innerDelegate;
      } catch (Exception e) {
        Logger.error(LOG_TAG, "Got exception logging in.", e);
        delegate.handleError(e);
      }
      return null;
    }
  }

  public static class FxAccountSignInTask extends FxAccountSetupTask<LoginResponse> {
    protected static final String LOG_TAG = FxAccountSignUpTask.class.getSimpleName();

    public FxAccountSignInTask(Context context, String email, String password, FxAccountClient20 client, RequestDelegate<LoginResponse> delegate) throws UnsupportedEncodingException, GeneralSecurityException {
      super(context, email, password, client, delegate);
    }

    @Override
    protected InnerRequestDelegate<LoginResponse> doInBackground(Void... arg0) {
      try {
        client.loginAndGetKeys(emailUTF8, generateQuickStretchedPW(), innerDelegate);
        latch.await();
        return innerDelegate;
      } catch (Exception e) {
        Logger.error(LOG_TAG, "Got exception signing in.", e);
        delegate.handleError(e);
      }
      return null;
    }
  }
}
