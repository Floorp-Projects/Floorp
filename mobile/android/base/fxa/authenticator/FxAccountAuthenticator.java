/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.fxa.FxAccountConstants;

import android.accounts.AbstractAccountAuthenticator;
import android.accounts.Account;
import android.accounts.AccountAuthenticatorResponse;
import android.accounts.AccountManager;
import android.accounts.NetworkErrorException;
import android.content.ContentResolver;
import android.content.Context;
import android.os.Bundle;

public class FxAccountAuthenticator extends AbstractAccountAuthenticator {
  public static final String LOG_TAG = FxAccountAuthenticator.class.getSimpleName();

  protected final Context context;
  protected final AccountManager accountManager;

  public FxAccountAuthenticator(Context context) {
    super(context);
    this.context = context;
    this.accountManager = AccountManager.get(context);
  }

  @Override
  public Bundle addAccount(AccountAuthenticatorResponse response,
      String accountType, String authTokenType, String[] requiredFeatures,
      Bundle options)
          throws NetworkErrorException {
    Logger.debug(LOG_TAG, "addAccount");

    final Bundle res = new Bundle();

    if (!FxAccountConstants.ACCOUNT_TYPE.equals(accountType)) {
      res.putInt(AccountManager.KEY_ERROR_CODE, -1);
      res.putString(AccountManager.KEY_ERROR_MESSAGE, "Not adding unknown account type.");
      return res;
    }

    final Account account = new Account("test@test.com", FxAccountConstants.ACCOUNT_TYPE);
    final String password = "password";
    final Bundle userData = Bundle.EMPTY;

    if (!accountManager.addAccountExplicitly(account, password, userData)) {
      res.putInt(AccountManager.KEY_ERROR_CODE, -1);
      res.putString(AccountManager.KEY_ERROR_MESSAGE, "Failed to add account explicitly.");
      return res;
    }

    Logger.info(LOG_TAG, "Added account named " + account.name + " of type " + account.type);

    // Enable syncing by default.
    for (String authority : new String[] {
        AppConstants.ANDROID_PACKAGE_NAME + ".db.browser",
        AppConstants.ANDROID_PACKAGE_NAME + ".db.formhistory",
        AppConstants.ANDROID_PACKAGE_NAME + ".db.tabs",
        AppConstants.ANDROID_PACKAGE_NAME + ".db.passwords",
        }) {
      ContentResolver.setSyncAutomatically(account, authority, true);
      ContentResolver.setIsSyncable(account, authority, 1);
    }

    res.putString(AccountManager.KEY_ACCOUNT_NAME, account.name);
    res.putString(AccountManager.KEY_ACCOUNT_TYPE, account.type);
    return res;
  }

  @Override
  public Bundle confirmCredentials(AccountAuthenticatorResponse response, Account account, Bundle options)
      throws NetworkErrorException {
    Logger.debug(LOG_TAG, "confirmCredentials");

    return null;
  }

  @Override
  public Bundle editProperties(AccountAuthenticatorResponse response, String accountType) {
    Logger.debug(LOG_TAG, "editProperties");

    return null;
  }

  @Override
  public Bundle getAuthToken(final AccountAuthenticatorResponse response,
      final Account account, final String authTokenType, final Bundle options)
          throws NetworkErrorException {
    Logger.debug(LOG_TAG, "getAuthToken");

    Logger.warn(LOG_TAG, "Returning null bundle for getAuthToken.");

    return null;
  }

  @Override
  public String getAuthTokenLabel(String authTokenType) {
    Logger.debug(LOG_TAG, "getAuthTokenLabel");

    return null;
  }

  @Override
  public Bundle hasFeatures(AccountAuthenticatorResponse response,
      Account account, String[] features) throws NetworkErrorException {
    Logger.debug(LOG_TAG, "hasFeatures");

    return null;
  }

  @Override
  public Bundle updateCredentials(AccountAuthenticatorResponse response,
      Account account, String authTokenType, Bundle options)
          throws NetworkErrorException {
    Logger.debug(LOG_TAG, "updateCredentials");

    return null;
  }
}
