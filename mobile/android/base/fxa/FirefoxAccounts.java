/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;

/**
 * Simple public accessors for Firefox account objects.
 */
public class FirefoxAccounts {
  /**
   * Return true if at least one Firefox account exists.
   *
   * @param context Android context.
   * @return true if at least one Firefox account exists.
   */
  public static boolean firefoxAccountsExist(final Context context) {
    return getFirefoxAccounts(context).length > 0;
  }

  /**
   * Return Firefox accounts.
   *
   * @param context Android context.
   * @return Firefox account objects.
   */
  public static Account[] getFirefoxAccounts(final Context context) {
    return AccountManager.get(context).getAccountsByType(FxAccountConstants.ACCOUNT_TYPE);
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
}