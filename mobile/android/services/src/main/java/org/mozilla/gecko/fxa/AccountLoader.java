/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa;

import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.support.v4.content.AsyncTaskLoader;

/**
 * A Loader that queries and updates based on the existence of Firefox and
 * legacy Sync Android Accounts.
 *
 * The loader returns an Android Account (of either Account type) if an account
 * exists, and null to indicate no Account is present.
 *
 * The loader listens for Accounts added and deleted, and also Accounts being
 * updated by Sync or another Activity, via the use of
 * {@link AndroidFxAccount#setState(org.mozilla.gecko.fxa.login.State)}.
 * Be careful of message loops if you update the account state from an activity
 * that uses this loader.
 *
 * This implementation is based on
 * <a href="http://www.androiddesignpatterns.com/2012/08/implementing-loaders.html">http://www.androiddesignpatterns.com/2012/08/implementing-loaders.html</a>.
 */
public class AccountLoader extends AsyncTaskLoader<Account> {
  protected Account account = null;
  protected BroadcastReceiver broadcastReceiver = null;

  public AccountLoader(Context context) {
    super(context);
  }

  // Task that performs the asynchronous load.
  @Override
  public Account loadInBackground() {
    return FirefoxAccounts.getFirefoxAccount(getContext());
  }

  // Deliver the results to the registered listener.
  @Override
  public void deliverResult(Account data) {
    if (isReset()) {
      // The Loader has been reset; ignore the result and invalidate the data.
      releaseResources(data);
      return;
    }

    // Hold a reference to the old data so it doesn't get garbage collected.
    // We must protect it until the new data has been delivered.
    Account oldData = account;
    account = data;

    if (isStarted()) {
      // If the Loader is in a started state, deliver the results to the
      // client. The superclass method does this for us.
      super.deliverResult(data);
    }

    // Invalidate the old data as we don't need it any more.
    if (oldData != null && oldData != data) {
      releaseResources(oldData);
    }
  }

  // The Loader’s state-dependent behavior.
  @Override
  protected void onStartLoading() {
    if (account != null) {
      // Deliver any previously loaded data immediately.
      deliverResult(account);
    }

    // Begin monitoring the underlying data source.
    if (broadcastReceiver == null) {
      broadcastReceiver = makeNewObserver();
      registerObserver(broadcastReceiver);
    }

    if (takeContentChanged() || account == null) {
      // When the observer detects a change, it should call onContentChanged()
      // on the Loader, which will cause the next call to takeContentChanged()
      // to return true. If this is ever the case (or if the current data is
      // null), we force a new load.
      forceLoad();
    }
  }

  @Override
  protected void onStopLoading() {
    // The Loader is in a stopped state, so we should attempt to cancel the
    // current load (if there is one).
    cancelLoad();

    // Note that we leave the observer as is. Loaders in a stopped state
    // should still monitor the data source for changes so that the Loader
    // will know to force a new load if it is ever started again.
  }

  @Override
  protected void onReset() {
    // Ensure the loader has been stopped.  In CursorLoader and the template
    // this code follows (see the class comment), this is onStopLoading, which
    // appears to not set the started flag (see Loader itself).
    stopLoading();

    // At this point we can release the resources associated with 'mData'.
    if (account != null) {
      releaseResources(account);
      account = null;
    }

    // The Loader is being reset, so we should stop monitoring for changes.
    if (broadcastReceiver != null) {
      final BroadcastReceiver observer = broadcastReceiver;
      broadcastReceiver = null;
      unregisterObserver(observer);
    }
  }

  @Override
  public void onCanceled(Account data) {
    // Attempt to cancel the current asynchronous load.
    super.onCanceled(data);

    // The load has been canceled, so we should release the resources
    // associated with 'data'.
    releaseResources(data);
  }

  private void releaseResources(Account data) {
    // For a simple List, there is nothing to do. For something like a Cursor, we
    // would close it in this method. All resources associated with the Loader
    // should be released here.
  }

  // Observer which receives notifications when the data changes.
  protected BroadcastReceiver makeNewObserver() {
    final BroadcastReceiver broadcastReceiver = new BroadcastReceiver() {
      @Override
      public void onReceive(Context context, Intent intent) {
        // Must be called on the main thread of the process. We register the
        // broadcast receiver with a null Handler (see registerObserver), which
        // ensures we're on the main thread when we receive this intent.
        onContentChanged();
      }
    };
    return broadcastReceiver;
  }

  protected void registerObserver(BroadcastReceiver observer) {
    final IntentFilter intentFilter = new IntentFilter();
    // Android Account added or removed.
    intentFilter.addAction(AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION);
    // Firefox Account internal state changed.
    intentFilter.addAction(FxAccountConstants.ACCOUNT_STATE_CHANGED_ACTION);
    // Firefox Account profile state changed.
    intentFilter.addAction(FxAccountConstants.ACCOUNT_PROFILE_JSON_UPDATED_ACTION);

    // null means: "the main thread of the process will be used." We must call
    // onContentChanged on the main thread of the process; this ensures we do.
    final Handler handler = null;
    getContext().registerReceiver(observer, intentFilter, FxAccountConstants.PER_ACCOUNT_TYPE_PERMISSION, handler);
  }

  protected void unregisterObserver(BroadcastReceiver observer) {
    getContext().unregisterReceiver(observer);
  }
}
