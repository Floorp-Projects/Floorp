/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.receivers;

import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.sync.Utils;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;

/**
 * A receiver that takes action when our Android package is upgraded (replaced).
 */
public class FxAccountUpgradeReceiver extends BroadcastReceiver {
  private static final String LOG_TAG = FxAccountUpgradeReceiver.class.getSimpleName();

  /**
   * Produce a list of Runnable instances to be executed sequentially on
   * upgrade.
   * <p>
   * Each Runnable will be executed sequentially on a background thread. Any
   * unchecked Exception thrown will be caught and ignored.
   *
   * @param context Android context.
   * @return list of Runnable instances.
   */
  protected List<Runnable> onUpgradeRunnables(Context context) {
    List<Runnable> runnables = new LinkedList<Runnable>();
    runnables.add(new MaybeUnpickleRunnable(context));
    // Recovering accounts that are in the Doghouse should happen *after* we
    // unpickle any accounts saved to disk.
    runnables.add(new AdvanceFromDoghouseRunnable(context));
    runnables.add(new MaybeInitializeReadingListAuthority(context));
    return runnables;
  }

  @Override
  public void onReceive(final Context context, Intent intent) {
    Logger.setThreadLogTag(FxAccountConstants.GLOBAL_LOG_TAG);
    Logger.info(LOG_TAG, "Upgrade broadcast received.");

    // Iterate Runnable instances one at a time.
    final Executor executor = Executors.newSingleThreadExecutor();
    for (final Runnable runnable : onUpgradeRunnables(context)) {
      executor.execute(new Runnable() {
        @Override
        public void run() {
          try {
            runnable.run();
          } catch (Exception e) {
            // We really don't want to throw on a background thread, so we
            // catch, log, and move on.
            Logger.error(LOG_TAG, "Got exception executing background upgrade Runnable; ignoring.", e);
          }
        }
      });
    }
  }

  /**
   * A Runnable that tries to unpickle any pickled Firefox Accounts.
   */
  protected static class MaybeUnpickleRunnable implements Runnable {
    protected final Context context;

    public MaybeUnpickleRunnable(Context context) {
      this.context = context;
    }

    @Override
    public void run() {
      // Querying the accounts will unpickle any pickled Firefox Account.
      Logger.info(LOG_TAG, "Trying to unpickle any pickled Firefox Account.");
      FirefoxAccounts.getFirefoxAccounts(context);
    }
  }

  /**
   * A Runnable that tries to advance existing Firefox Accounts that are in the
   * Doghouse state to the Separated state.
   * <p>
   * This is our main deprecation-and-upgrade mechanism: in some way, the
   * Account gets moved to the Doghouse state. If possible, an upgraded version
   * of the package advances to Separated, prompting the user to re-connect the
   * Account.
   */
  protected static class AdvanceFromDoghouseRunnable implements Runnable {
    protected final Context context;

    public AdvanceFromDoghouseRunnable(Context context) {
      this.context = context;
    }

    @Override
    public void run() {
      final Account[] accounts = FirefoxAccounts.getFirefoxAccounts(context);
      Logger.info(LOG_TAG, "Trying to advance " + accounts.length + " existing Firefox Accounts from the Doghouse to Separated (if necessary).");
      for (Account account : accounts) {
        try {
          final AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);
          // For great debugging.
          if (FxAccountUtils.LOG_PERSONAL_INFORMATION) {
            fxAccount.dump();
          }
          State state = fxAccount.getState();
          if (state == null || state.getStateLabel() != StateLabel.Doghouse) {
            Logger.debug(LOG_TAG, "Account named like " + Utils.obfuscateEmail(account.name) + " is not in the Doghouse; skipping.");
            continue;
          }
          Logger.debug(LOG_TAG, "Account named like " + Utils.obfuscateEmail(account.name) + " is in the Doghouse; advancing to Separated.");
          fxAccount.setState(state.makeSeparatedState());
        } catch (Exception e) {
          Logger.warn(LOG_TAG, "Got exception trying to advance account named like " + Utils.obfuscateEmail(account.name) +
              " from Doghouse to Separated state; ignoring.", e);
        }
      }
    }
  }

  /**
   * A Runnable that initializes the Reading List authority (specifically, set
   * the sync automatically flag) for existing Firefox Accounts that have not
   * yet seen the authority. That is, if a new authority (Reading List) is added
   * to the set of defaults, existing Firefox Accounts won't be syncing it
   * automatically. This tries to set the sync automatically flag for such
   * existing accounts.
   *
   * Public for testing only.
   */
  public static class MaybeInitializeReadingListAuthority implements Runnable {
    protected final Context context;

    public MaybeInitializeReadingListAuthority(Context context) {
      this.context = context;
    }

    @Override
    public void run() {
      final String authority = BrowserContract.READING_LIST_AUTHORITY;
      Boolean enabledByDefault = AndroidFxAccount.DEFAULT_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP.get(authority);
      if (enabledByDefault == null || !enabledByDefault.booleanValue()) {
        Logger.info(LOG_TAG, "Reading List authority is not enabled by default; not trying to initialize Reading List authority for any accounts.");
      }

      final AccountManager accountManager = AccountManager.get(context);
      final Account[] accounts = FirefoxAccounts.getFirefoxAccounts(context);
      Logger.info(LOG_TAG, "Trying to initialize Reading List authority for " + accounts.length + " existing Firefox Accounts (if necessary).");

      for (Account account : accounts) {
        try {
          final AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);
          // For great debugging.
          if (FxAccountUtils.LOG_PERSONAL_INFORMATION) {
            fxAccount.dump();
          }

          final boolean readingListAuthorityInitialized =
              "1".equals(accountManager.getUserData(account, AndroidFxAccount.ACCOUNT_KEY_READING_LIST_AUTHORITY_INITIALIZED));
          if (readingListAuthorityInitialized) {
            Logger.debug(LOG_TAG, "Reading List authority has already been initialized.");
            continue;
          }

          // The Reading List authority has not been seen. This happens when an
          // authority is added after the Firefox Account has been added (and
          // the package last upgraded). If Firefox Sync is not syncing
          // automatically, Reading List should not start syncing
          // automatically: the user has elected not to upload data to Mozilla
          // servers; we shouldn't opt them in.
          final boolean syncAutomatically = ContentResolver.getSyncAutomatically(account, BrowserContract.AUTHORITY);
            Logger.debug(LOG_TAG, "Setting Reading List authority " +
                (syncAutomatically ? " to " : " to not ") + "sync automatically.");
            ContentResolver.setSyncAutomatically(account, BrowserContract.READING_LIST_AUTHORITY, syncAutomatically);
          // Update the account record.
          accountManager.setUserData(account, AndroidFxAccount.ACCOUNT_KEY_READING_LIST_AUTHORITY_INITIALIZED, "1");
        } catch (Exception e) {
          Logger.warn(LOG_TAG, "Got exception trying to set authoritities to sync automatically for account named like " +
              Utils.obfuscateEmail(account.name) + "; ignoring.", e);
        }
      }
    }
  }
}
