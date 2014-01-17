/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.activities.FxAccountSetupActivity;

import android.accounts.AbstractAccountAuthenticator;
import android.accounts.Account;
import android.accounts.AccountAuthenticatorResponse;
import android.accounts.AccountManager;
import android.accounts.NetworkErrorException;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

public class FxAccountAuthenticator extends AbstractAccountAuthenticator {
  public static final String LOG_TAG = FxAccountAuthenticator.class.getSimpleName();

  public static final String JSON_KEY_UID = "uid";
  public static final String JSON_KEY_SESSION_TOKEN = "session_token";
  public static final String JSON_KEY_KA = "kA";
  public static final String JSON_KEY_KB = "kB";
  public static final String JSON_KEY_IDP_ENDPOINT = "idp_endpoint";
  public static final String JSON_KEY_AUTH_ENDPOINT = "auth_endpoint";

  protected final Context context;
  protected final AccountManager accountManager;

  public FxAccountAuthenticator(Context context) {
    super(context);
    this.context = context;
    this.accountManager = AccountManager.get(context);
  }

  protected static void enableSyncing(Context context, Account account) {
    for (String authority : new String[] {
        AppConstants.ANDROID_PACKAGE_NAME + ".db.browser",
    }) {
      ContentResolver.setSyncAutomatically(account, authority, true);
      ContentResolver.setIsSyncable(account, authority, 1);
    }
  }

  public static Account addAccount(Context context, String email, String uid, String sessionToken, String kA, String kB) {
    final AccountManager accountManager = AccountManager.get(context);
    final Account account = new Account(email, FxAccountConstants.ACCOUNT_TYPE);
    final Bundle userData = new Bundle();
    userData.putString(JSON_KEY_UID, uid);
    userData.putString(JSON_KEY_SESSION_TOKEN, sessionToken);
    userData.putString(JSON_KEY_KA, kA);
    userData.putString(JSON_KEY_KB, kB);
    userData.putString(JSON_KEY_IDP_ENDPOINT, FxAccountConstants.DEFAULT_IDP_ENDPOINT);
    userData.putString(JSON_KEY_AUTH_ENDPOINT, FxAccountConstants.DEFAULT_AUTH_ENDPOINT);
    if (!accountManager.addAccountExplicitly(account, sessionToken, userData)) {
      Logger.warn(LOG_TAG, "Error adding account named " + account.name + " of type " + account.type);
      return null;
    }
    // Enable syncing by default.
    enableSyncing(context, account);
    return account;
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

    Intent intent = new Intent(context, FxAccountSetupActivity.class);
    res.putParcelable(AccountManager.KEY_INTENT, intent);
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

  /**
   * Return Firefox Accounts.
   *
   * @param context Android context.
   * @return Firefox Account objects.
   */
  public static Account[] getFirefoxAccounts(final Context context) {
    return AccountManager.get(context).getAccountsByType(FxAccountConstants.ACCOUNT_TYPE);
  }
}
