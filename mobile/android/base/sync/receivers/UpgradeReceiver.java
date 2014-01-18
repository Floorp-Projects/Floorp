/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.receivers;

import org.mozilla.gecko.sync.CredentialException;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.config.ConfigurationMigrator;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.sync.setup.SyncAccounts.SyncAccountParameters;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public class UpgradeReceiver extends BroadcastReceiver {
  private static final String LOG_TAG = "UpgradeReceiver";

  @Override
  public void onReceive(final Context context, Intent intent) {
    Logger.debug(LOG_TAG, "Broadcast received.");
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
          Logger.info(LOG_TAG, "Migrating preferences on upgrade for Android account named " + account.name + ".");

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
          final String profile = Constants.DEFAULT_PROFILE;
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
}
