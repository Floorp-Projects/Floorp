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
import android.os.Looper;
import android.content.AsyncTaskLoader;
import android.support.v4.content.LocalBroadcastManager;

import java.lang.ref.WeakReference;

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

  // Hold a weak reference to AccountLoader instance in this Runnable to avoid potentially leaking it
  // after posting to a Handler in the BroadcastReceiver returned from makeNewObserver.
  private final BroadcastReceiverRunnable broadcastReceiverRunnable = new BroadcastReceiverRunnable(this);

  public AccountLoader(final Context context) {
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
      registerLocalObserver(getContext(), broadcastReceiver);
      registerSystemObserver(getContext(), broadcastReceiver);
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
      unregisterObserver(getContext(), observer);
    }
  }

  @Override
  public void onCanceled(final Account data) {
    // Attempt to cancel the current asynchronous load.
    super.onCanceled(data);

    // The load has been canceled, so we should release the resources
    // associated with 'data'.
    releaseResources(data);
  }

  // Observer which receives notifications when the data changes.
  protected BroadcastReceiver makeNewObserver() {
    return new BroadcastReceiver() {
      @Override
      public void onReceive(Context context, Intent intent) {
        // onContentChanged must be called on the main thread.
        // If we're already on the main thread, call it directly.
        if (Looper.myLooper() == Looper.getMainLooper()) {
          onContentChanged();
          return;
        }

        // Otherwise, post a Runnable to a Handler bound to the main thread's message loop.
        final Handler mainHandler = new Handler(Looper.getMainLooper());
        mainHandler.post(broadcastReceiverRunnable);
      }
    };
  }

  private static class BroadcastReceiverRunnable implements Runnable {
    private final WeakReference<AccountLoader> accountLoaderWeakReference;

    public BroadcastReceiverRunnable(final AccountLoader accountLoader) {
      accountLoaderWeakReference = new WeakReference<>(accountLoader);
    }

    @Override
    public void run() {
      final AccountLoader accountLoader = accountLoaderWeakReference.get();
      if (accountLoader != null) {
        accountLoader.onContentChanged();
      }
    }
  }

  private void releaseResources(Account data) {
    // For a simple List, there is nothing to do. For something like a Cursor, we
    // would close it in this method. All resources associated with the Loader
    // should be released here.
  }

  /**
   * Register provided observer with the LocalBroadcastManager to listen for internal events.
   *
   * @param context <code>Context</code> to use for obtaining LocalBroadcastManager instance.
   * @param observer <code>BroadcastReceiver</code> which will handle local events.
   */
  protected static void registerLocalObserver(final Context context, final BroadcastReceiver observer) {
    final IntentFilter intentFilter = new IntentFilter();
    // Firefox Account internal state changed.
    intentFilter.addAction(FxAccountConstants.ACCOUNT_STATE_CHANGED_ACTION);
    // Firefox Account profile state changed.
    intentFilter.addAction(FxAccountConstants.ACCOUNT_PROFILE_JSON_UPDATED_ACTION);

    LocalBroadcastManager.getInstance(context).registerReceiver(observer, intentFilter);
  }

  /**
   * Register provided observer for handling system-wide broadcasts.
   *
   * @param context <code>Context</code> to use for registering a receiver.
   * @param observer <code>BroadcastReceiver</code> which will handle system events.
   */
  protected static void registerSystemObserver(final Context context, final BroadcastReceiver observer) {
    context.registerReceiver(observer,
            // Android Account added or removed.
            new IntentFilter(AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION),
            // No broadcast permissions required.
            null,
            // Null handler ensures that broadcasts will be handled on the main thread.
            null
    );
  }

  protected static void unregisterObserver(final Context context, final BroadcastReceiver observer) {
    LocalBroadcastManager.getInstance(context).unregisterReceiver(observer);
    context.unregisterReceiver(observer);
  }
}

