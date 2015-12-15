/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.receivers;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.CredentialException;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.SyncConstants;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.config.ConfigurationMigrator;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.sync.setup.SyncAccounts.SyncAccountParameters;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.support.v4.app.NotificationCompat.Builder;

public class UpgradeReceiver extends BroadcastReceiver {
  private static final String LOG_TAG = "UpgradeReceiver";

  @Override
  public void onReceive(final Context context, Intent intent) {
    Logger.debug(LOG_TAG, "Broadcast received.");

    // This unpickles any pickled accounts.
    if (!SyncAccounts.syncAccountsExist(context)) {
      Logger.info(LOG_TAG, "No Sync Accounts found; not upgrading anything.");
      return;
    }

    // Show account not supported notification.
    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        notifyAccountDeprecation(context);
      }
    });

    // Should filter for specific MY_PACKAGE_REPLACED intent, but Android does
    // not expose it.
    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        final AccountManager accountManager = AccountManager.get(context);
        final Account[] accounts = SyncAccounts.syncAccounts(context);

        for (Account a : accounts) {
          if ("1".equals(accountManager.getUserData(a, Constants.DATA_ENABLE_ON_UPGRADE))) {
            SyncAccounts.setSyncAutomatically(a, true);
            accountManager.setUserData(a, Constants.DATA_ENABLE_ON_UPGRADE, "0");
          }

          // If we are both set to enable after upgrade, and to be removed: we
          // enable after upgrade first and then we try to remove. If removal
          // fails, since we enabled sync, we'll try again the next time we
          // sync, until we (eventually) remove the account.
          if ("1".equals(accountManager.getUserData(a, Constants.DATA_SHOULD_BE_REMOVED))) {
            accountManager.removeAccount(a, null, null);
          }
        }
      }
    });

    /**
     * Bug 761682: migrate preferences forward.
     */
    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        AccountManager accountManager = AccountManager.get(context);
        final Account[] accounts = SyncAccounts.syncAccounts(context);

        for (Account account : accounts) {
          Logger.info(LOG_TAG, "Migrating preferences on upgrade for Android account named " + Utils.obfuscateEmail(account.name) + ".");

          SyncAccountParameters params;
          try {
            params = SyncAccounts.blockingFromAndroidAccountV0(context, accountManager, account);
          } catch (CredentialException e) {
            Logger.warn(LOG_TAG, "Caught exception fetching account parameters while trying to migrate preferences; ignoring.", e);
            continue;
          }

          final String product = GlobalConstants.BROWSER_INTENT_PACKAGE;
          final String username = params.username;
          final String serverURL = params.serverURL;
          final String profile = "default";
          try {
            ConfigurationMigrator.ensurePrefsAreVersion(SyncConfiguration.CURRENT_PREFS_VERSION, context, accountManager, account,
                product, username, serverURL, profile);
          } catch (Exception e) {
            Logger.warn(LOG_TAG, "Caught exception trying to migrate preferences; ignoring.", e);
            continue;
          }
        }
      }
    });
  }

  /**
   * Show a persistent notification telling the user that their Old Sync account is deprecated.
   */
  private void notifyAccountDeprecation(final Context context) {
    final Intent notificationIntent = new Intent(SyncConstants.SYNC_ACCOUNT_DEPRECATED_ACTION);
    notificationIntent.setClass(context, SyncAccountDeletedService.class);
    final PendingIntent pendingIntent = PendingIntent.getService(context, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);
    final Builder builder = new Builder(context)
        .setSmallIcon(R.drawable.ic_status_logo)
        .setContentTitle(context.getString(R.string.old_sync_deprecated_notification_title))
        .setContentText(context.getString(R.string.old_sync_deprecated_notification_content))
        .setAutoCancel(true)
        .setOngoing(true)
        .setContentIntent(pendingIntent);

    final NotificationManager notificationManager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
    final int notificationID = SyncConstants.SYNC_ACCOUNT_DEPRECATED_ACTION.hashCode();
    notificationManager.notify(notificationID, builder.build());
  }
}
