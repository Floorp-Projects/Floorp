/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.Utils;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.util.Log;

/**
 * This class contains utilities that are of use to Fennec
 * and Sync setup activities.
 *
 * Do not break these APIs without correcting upstream code!
 */
public class SyncAccounts {

  private final static String DEFAULT_SERVER = "https://auth.services.mozilla.com/";
  private static final String LOG_TAG = "SyncAccounts";

  /**
   * Returns true if a Sync account is set up.
   *
   * Do not call this method from the main thread.
   */
  public static boolean syncAccountsExist(Context c) {
    return AccountManager.get(c).getAccountsByType("org.mozilla.firefox_sync").length > 0;
  }

  /**
   * This class provides background-thread abstracted access to whether a
   * Firefox Sync account has been set up on this device.
   *
   * Subclass this task and override `onPostExecute` to act on the result.
   */
  public static class AccountsExistTask extends AsyncTask<Context, Void, Boolean> {
    @Override
    protected Boolean doInBackground(Context... params) {
      Context c = params[0];
      return syncAccountsExist(c);
    }
  }

  public static Intent createAccount(Context context,
                                     AccountManager accountManager,
                                     String username,
                                     String syncKey,
                                     String password,
                                     String serverURL) {

    final Account account = new Account(username, Constants.ACCOUNTTYPE_SYNC);
    final Bundle userbundle = new Bundle();

    // Add sync key and server URL.
    userbundle.putString(Constants.OPTION_SYNCKEY, syncKey);
    if (serverURL != null) {
      Logger.info(LOG_TAG, "Setting explicit server URL: " + serverURL);
      userbundle.putString(Constants.OPTION_SERVER, serverURL);
    } else {
      userbundle.putString(Constants.OPTION_SERVER, DEFAULT_SERVER);
    }
    Logger.debug(LOG_TAG, "Adding account for " + Constants.ACCOUNTTYPE_SYNC);
    boolean result = false;
    try {
      result = accountManager.addAccountExplicitly(account, password, userbundle);
    } catch (SecurityException e) {
      final String message = e.getMessage();
      if (message != null && (message.indexOf("is different than the authenticator's uid") > 0)) {
        Log.wtf("FirefoxSync",
                "Unable to create account. " +
                "If you have more than one version of " +
                "Firefox/Beta/Aurora/Nightly/Fennec installed, that's why.",
                e);
      } else {
        Log.e("FirefoxSync", "Unable to create account.", e);
      }
    }

    Logger.debug(LOG_TAG, "Account: " + account + " added successfully? " + result);
    if (!result) {
      Logger.error(LOG_TAG, "Failed to add account!");
    }

    // Set components to sync (default: all).
    ContentResolver.setMasterSyncAutomatically(true);

    String authority = BrowserContract.AUTHORITY;
    Logger.debug(LOG_TAG, "Setting authority " + authority + " to sync automatically.");
    ContentResolver.setSyncAutomatically(account, authority, true);
    ContentResolver.setIsSyncable(account, authority, 1);

    // TODO: add other ContentProviders as needed (e.g. passwords)
    // TODO: for each, also add to res/xml to make visible in account settings
    Logger.debug(LOG_TAG, "Finished setting syncables.");

    // TODO: correctly implement Sync Options.
    Logger.info(LOG_TAG, "Clearing preferences for this account.");
    try {
      Utils.getSharedPreferences(context, username, serverURL).edit().clear().commit();
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Could not clear prefs path!", e);
    }

    final Intent intent = new Intent();
    intent.putExtra(AccountManager.KEY_ACCOUNT_NAME, username);
    intent.putExtra(AccountManager.KEY_ACCOUNT_TYPE, Constants.ACCOUNTTYPE_SYNC);
    intent.putExtra(AccountManager.KEY_AUTHTOKEN, Constants.ACCOUNTTYPE_SYNC);
    return intent;
  }
}
