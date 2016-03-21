/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa;

import java.io.File;
import java.util.concurrent.CountDownLatch;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.fxa.authenticator.AccountPickler;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.sync.FxAccountSyncStatusHelper;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.Utils;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.ContentResolver;
import android.content.Context;
import android.os.Bundle;

/**
 * Simple public accessors for Firefox account objects.
 */
public class FirefoxAccounts {
  private static final String LOG_TAG = FirefoxAccounts.class.getSimpleName();

  /**
   * Returns true if a FirefoxAccount exists, false otherwise.
   *
   * @param context Android context.
   * @return true if at least one Firefox account exists.
   */
  public static boolean firefoxAccountsExist(final Context context) {
    return getFirefoxAccounts(context).length > 0;
  }

  /**
   * Return Firefox accounts.
   * <p>
   * If no accounts exist in the AccountManager, one may be created
   * via a pickled FirefoxAccount, if available, and that account
   * will be added to the AccountManager and returned.
   * <p>
   * Note that this can be called from any thread.
   *
   * @param context Android context.
   * @return Firefox account objects.
   */
  public static Account[] getFirefoxAccounts(final Context context) {
    final Account[] accounts =
        AccountManager.get(context).getAccountsByType(FxAccountConstants.ACCOUNT_TYPE);
    if (accounts.length > 0) {
      return accounts;
    }

    final Account pickledAccount = getPickledAccount(context);
    return (pickledAccount != null) ? new Account[] {pickledAccount} : new Account[0];
  }

  private static Account getPickledAccount(final Context context) {
    // To avoid a StrictMode violation for disk access, we call this from a background thread.
    // We do this every time, so the caller doesn't have to care.
    final CountDownLatch latch = new CountDownLatch(1);
    final Account[] accounts = new Account[1];
    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        try {
          final File file = context.getFileStreamPath(FxAccountConstants.ACCOUNT_PICKLE_FILENAME);
          if (!file.exists()) {
            accounts[0] = null;
            return;
          }

          // There is a small race window here: if the user creates a new Firefox account
          // between our checks, this could erroneously report that no Firefox accounts
          // exist.
          final AndroidFxAccount fxAccount =
              AccountPickler.unpickle(context, FxAccountConstants.ACCOUNT_PICKLE_FILENAME);
          accounts[0] = fxAccount != null ? fxAccount.getAndroidAccount() : null;
        } finally {
          latch.countDown();
        }
      }
    });

    try {
      latch.await(); // Wait for the background thread to return.
    } catch (InterruptedException e) {
      Logger.warn(LOG_TAG,
          "Foreground thread unexpectedly interrupted while getting pickled account", e);
      return null;
    }

    return accounts[0];
  }

  /**
   * @param context Android context.
   * @return the configured Firefox account if one exists, or null otherwise.
   */
  public static Account getFirefoxAccount(final Context context) {
    Account[] accounts = getFirefoxAccounts(context);
    if (accounts.length > 0) {
      return accounts[0];
    }
    return null;
  }

  /**
   * @return
   *     the {@link State} instance associated with the current account, or <code>null</code> if
   *     no accounts exist.
   */
  public static State getFirefoxAccountState(final Context context) {
    final Account account = getFirefoxAccount(context);
    if (account == null) {
      return null;
    }

    final AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);
    try {
      return fxAccount.getState();
    } catch (final Exception ex) {
      Logger.warn(LOG_TAG, "Could not get FX account state.", ex);
      return null;
    }
  }

  /*
   * @param context Android context
   * @return the email address associated with the configured Firefox account if one exists; null otherwise.
   */
  public static String getFirefoxAccountEmail(final Context context) {
    final Account account = getFirefoxAccount(context);
    if (account == null) {
      return null;
    }
    return account.name;
  }

  public static void logSyncOptions(Bundle syncOptions) {
    final boolean scheduleNow = syncOptions.getBoolean(ContentResolver.SYNC_EXTRAS_IGNORE_BACKOFF, false);

    Logger.info(LOG_TAG, "Sync options -- scheduling now: " + scheduleNow);
  }

  public static void requestImmediateSync(final Account account, String[] stagesToSync, String[] stagesToSkip) {
    final Bundle syncOptions = new Bundle();
    syncOptions.putBoolean(ContentResolver.SYNC_EXTRAS_IGNORE_BACKOFF, true);
    syncOptions.putBoolean(ContentResolver.SYNC_EXTRAS_EXPEDITED, true);
    requestSync(account, syncOptions, stagesToSync, stagesToSkip);
  }

  public static void requestEventualSync(final Account account, String[] stagesToSync, String[] stagesToSkip) {
    requestSync(account, Bundle.EMPTY, stagesToSync, stagesToSkip);
  }

  /**
   * Request a sync for the given Android Account.
   * <p>
   * Any hints are strictly optional: the actual requested sync is scheduled by
   * the Android sync scheduler, and the sync mechanism may ignore hints as it
   * sees fit.
   * <p>
   * It is safe to call this method from any thread.
   *
   * @param account to sync.
   * @param syncOptions to pass to sync.
   * @param stagesToSync stage names to sync.
   * @param stagesToSkip stage names to skip.
   */
  protected static void requestSync(final Account account, final Bundle syncOptions, String[] stagesToSync, String[] stagesToSkip) {
    if (account == null) {
      throw new IllegalArgumentException("account must not be null");
    }
    if (syncOptions == null) {
      throw new IllegalArgumentException("syncOptions must not be null");
    }

    Utils.putStageNamesToSync(syncOptions, stagesToSync, stagesToSkip);

    Logger.info(LOG_TAG, "Requesting sync.");
    logSyncOptions(syncOptions);

    // We get strict mode warnings on some devices, so make the request on a
    // background thread.
    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        for (String authority : AndroidFxAccount.DEFAULT_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP.keySet()) {
          ContentResolver.requestSync(account, authority, syncOptions);
        }
      }
    });
  }

  /**
   * Start notifying <code>syncStatusListener</code> of sync status changes.
   * <p>
   * Only a weak reference to <code>syncStatusListener</code> is held.
   *
   * @param syncStatusListener to start notifying.
   */
  public static void addSyncStatusListener(SyncStatusListener syncStatusListener) {
    // startObserving null-checks its argument.
    FxAccountSyncStatusHelper.getInstance().startObserving(syncStatusListener);
  }

  /**
   * Stop notifying <code>syncStatusListener</code> of sync status changes.
   *
   * @param syncStatusListener to stop notifying.
   */
  public static void removeSyncStatusListener(SyncStatusListener syncStatusListener) {
    // stopObserving null-checks its argument.
    FxAccountSyncStatusHelper.getInstance().stopObserving(syncStatusListener);
  }
}
