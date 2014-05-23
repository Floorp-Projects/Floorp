/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.tasks;

import java.util.concurrent.CountDownLatch;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.fxa.tasks.FxAccountSetupTask.InnerRequestDelegate;

import android.content.Context;
import android.os.AsyncTask;

/**
 * An <code>AsyncTask</code> wrapper around signing up for, and signing in to, a
 * Firefox Account.
 * <p>
 * It's strange to add explicit blocking to callback-threading code, but we do
 * it here to take advantage of Android's built in support for background work.
 * We really want to avoid making a threading mistake that brings down the whole
 * process.
 */
public abstract class FxAccountSetupTask<T> extends AsyncTask<Void, Void, InnerRequestDelegate<T>> {
  private static final String LOG_TAG = FxAccountSetupTask.class.getSimpleName();

  public interface ProgressDisplay {
    public void showProgress();
    public void dismissProgress();
  }

  protected final Context context;
  protected final FxAccountClient client;
  protected final ProgressDisplay progressDisplay;

  // Initialized lazily.
  protected byte[] quickStretchedPW;

  // AsyncTask's are one-time-use, so final members are fine.
  protected final CountDownLatch latch = new CountDownLatch(1);
  protected final InnerRequestDelegate<T> innerDelegate = new InnerRequestDelegate<T>(latch);

  protected final RequestDelegate<T> delegate;

  public FxAccountSetupTask(Context context, ProgressDisplay progressDisplay, FxAccountClient client, RequestDelegate<T> delegate) {
    this.context = context;
    this.client = client;
    this.delegate = delegate;
    this.progressDisplay = progressDisplay;
  }

  @Override
  protected void onPreExecute() {
    if (progressDisplay != null) {
      progressDisplay.showProgress();
    }
  }

  @Override
  protected void onPostExecute(InnerRequestDelegate<T> result) {
    if (progressDisplay != null) {
      progressDisplay.dismissProgress();
    }

    // We are on the UI thread, and need to invoke these callbacks here to allow UI updating.
    if (innerDelegate.failure != null) {
      delegate.handleFailure(innerDelegate.failure);
    } else if (innerDelegate.exception != null) {
      delegate.handleError(innerDelegate.exception);
    } else {
      delegate.handleSuccess(result.response);
    }
  }

  @Override
  protected void onCancelled(InnerRequestDelegate<T> result) {
    if (progressDisplay != null) {
      progressDisplay.dismissProgress();
    }
    delegate.handleError(new IllegalStateException("Task was cancelled."));
  }

  protected static class InnerRequestDelegate<T> implements RequestDelegate<T> {
    protected final CountDownLatch latch;
    public T response = null;
    public Exception exception = null;
    public FxAccountClientRemoteException failure = null;

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
    public void handleFailure(FxAccountClientRemoteException e) {
      Logger.warn(LOG_TAG, "Got failure.");
      this.failure = e;
      latch.countDown();
    }

    @Override
    public void handleSuccess(T result) {
      Logger.info(LOG_TAG, "Got success.");
      this.response = result;
      latch.countDown();
    }
  }
}
