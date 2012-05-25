/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.receivers;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.SyncAccounts;

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
        AccountManager accountManager = AccountManager.get(context);
        Account[] accounts = accountManager.getAccounts();
        for (Account a : accounts) {
          if ("1".equals(accountManager.getUserData(a, Constants.DATA_ENABLE_ON_UPGRADE))) {
            SyncAccounts.setSyncAutomatically(a, true);
            accountManager.setUserData(a, Constants.DATA_ENABLE_ON_UPGRADE, "0");
          }
        }
      }
    });
  }
}
