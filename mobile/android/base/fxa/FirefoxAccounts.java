/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa;

import java.io.File;
import java.util.EnumSet;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.fxa.authenticator.AccountPickler;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.sync.FxAccountSyncAdapter;
import org.mozilla.gecko.fxa.sync.FxAccountSyncStatusHelper;
import org.mozilla.gecko.fxa.tasks.FxAccountCodeResender;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.Utils;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.os.AsyncTask;
import android.os.Bundle;

/**
 * Simple public accessors for Firefox account objects.
 */
public class FirefoxAccounts {
  private static final String LOG_TAG = FirefoxAccounts.class.getSimpleName();

  public enum SyncHint {
    /**
     * Hint that a requested sync is preferred immediately.
     * <p>
     * On many devices, not including <code>SCHEDULE_NOW</code> means a delay of
     * at least 30 seconds.
     */
    SCHEDULE_NOW,

    /**
     * Hint that a requested sync may ignore local rate limiting.
     * <p>
     * This is just a hint; the actual requested sync may not obey the hint.
     */
    IGNORE_LOCAL_RATE_LIMIT,

    /**
     * Hint that a requested sync may ignore remote server backoffs.
     * <p>
     * This is just a hint; the actual requested sync may not obey the hint.
     */
    IGNORE_REMOTE_SERVER_BACKOFF,
  }

  public static final EnumSet<SyncHint> SOON = EnumSet.noneOf(SyncHint.class);

  public static final EnumSet<SyncHint> NOW = EnumSet.of(
      SyncHint.SCHEDULE_NOW);

  public static final EnumSet<SyncHint> FORCE = EnumSet.of(
      SyncHint.SCHEDULE_NOW,
      SyncHint.IGNORE_LOCAL_RATE_LIMIT,
      SyncHint.IGNORE_REMOTE_SERVER_BACKOFF);

  public interface SyncStatusListener {
    public Context getContext();
    public Account getAccount();
    public void onSyncStarted();
    public void onSyncFinished();
  }

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
          accounts[0] = fxAccount.getAndroidAccount();
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

  protected static void putHintsToSync(final Bundle extras, EnumSet<SyncHint> syncHints) {
    // stagesToSync and stagesToSkip are allowed to be null.
    if (syncHints == null) {
      throw new IllegalArgumentException("syncHints must not be null");
    }

    final boolean scheduleNow = syncHints.contains(SyncHint.SCHEDULE_NOW);
    final boolean ignoreLocalRateLimit = syncHints.contains(SyncHint.IGNORE_LOCAL_RATE_LIMIT);
    final boolean ignoreRemoteServerBackoff = syncHints.contains(SyncHint.IGNORE_REMOTE_SERVER_BACKOFF);

    extras.putBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, scheduleNow);
    // The default when manually syncing is to ignore the local rate limit and
    // any remote server backoff requests. Since we can't add flags to a manual
    // sync instigated by the user, we have to reverse the natural conditionals.
    // See also the FORCE EnumSet.
    extras.putBoolean(FxAccountSyncAdapter.SYNC_EXTRAS_RESPECT_LOCAL_RATE_LIMIT, !ignoreLocalRateLimit);
    extras.putBoolean(FxAccountSyncAdapter.SYNC_EXTRAS_RESPECT_REMOTE_SERVER_BACKOFF, !ignoreRemoteServerBackoff);
  }

  public static EnumSet<SyncHint> getHintsToSyncFromBundle(final Bundle extras) {
    final EnumSet<SyncHint> syncHints = EnumSet.noneOf(SyncHint.class);

    final boolean scheduleNow = extras.getBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, false);
    final boolean ignoreLocalRateLimit = !extras.getBoolean(FxAccountSyncAdapter.SYNC_EXTRAS_RESPECT_LOCAL_RATE_LIMIT, false);
    final boolean ignoreRemoteServerBackoff = !extras.getBoolean(FxAccountSyncAdapter.SYNC_EXTRAS_RESPECT_REMOTE_SERVER_BACKOFF, false);

    if (scheduleNow) {
      syncHints.add(SyncHint.SCHEDULE_NOW);
    }
    if (ignoreLocalRateLimit) {
      syncHints.add(SyncHint.IGNORE_LOCAL_RATE_LIMIT);
    }
    if (ignoreRemoteServerBackoff) {
      syncHints.add(SyncHint.IGNORE_REMOTE_SERVER_BACKOFF);
    }

    return syncHints;
  }

  public static void logSyncHints(EnumSet<SyncHint> syncHints) {
    final boolean scheduleNow = syncHints.contains(SyncHint.SCHEDULE_NOW);
    final boolean ignoreLocalRateLimit = syncHints.contains(SyncHint.IGNORE_LOCAL_RATE_LIMIT);
    final boolean ignoreRemoteServerBackoff = syncHints.contains(SyncHint.IGNORE_REMOTE_SERVER_BACKOFF);

    Logger.info(LOG_TAG, "Sync hints" +
        "; scheduling now: " + scheduleNow +
        "; ignoring local rate limit: " + ignoreLocalRateLimit +
        "; ignoring remote server backoff: " + ignoreRemoteServerBackoff + ".");
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
   * @param syncHints to pass to sync.
   * @param stagesToSync stage names to sync.
   * @param stagesToSkip stage names to skip.
   */
  public static void requestSync(final Account account, EnumSet<SyncHint> syncHints, String[] stagesToSync, String[] stagesToSkip) {
    if (account == null) {
      throw new IllegalArgumentException("account must not be null");
    }
    if (syncHints == null) {
      throw new IllegalArgumentException("syncHints must not be null");
    }

    final Bundle extras = new Bundle();
    putHintsToSync(extras, syncHints);
    Utils.putStageNamesToSync(extras, stagesToSync, stagesToSkip);

    Logger.info(LOG_TAG, "Requesting sync.");
    logSyncHints(syncHints);

    // We get strict mode warnings on some devices, so make the request on a
    // background thread.
    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        for (String authority : AndroidFxAccount.DEFAULT_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP.keySet()) {
          ContentResolver.requestSync(account, authority, extras);
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

  public static String getOldSyncUpgradeURL(final Resources res, final Locale locale) {
    final String VERSION = AppConstants.MOZ_APP_VERSION;
    final String OS = AppConstants.OS_TARGET;
    final String LOCALE = Utils.getLanguageTag(locale);
    return res.getString(R.string.fxaccount_link_old_firefox, VERSION, OS, LOCALE);
  }

  /**
   * Resends the account verification email, and displays an appropriate
   * toast on both send success and failure. Note that because the underlying implementation
   * uses {@link AsyncTask}, the provided context must be UI-capable, and this
   * method called from the UI thread (see
   * {@link org.mozilla.gecko.fxa.tasks.FxAccountCodeResender#resendCode(Context, AndroidFxAccount)}
   * for more).
   *
   * @param context a UI-capable Android context.
   * @return true if an account exists, false otherwise.
   */
  public static boolean resendVerificationEmail(final Context context) {
    final Account account = getFirefoxAccount(context);
    if (account == null) {
      return false;
    }

    final AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);
    FxAccountCodeResender.resendCode(context, fxAccount);
    return true;
  }
}
