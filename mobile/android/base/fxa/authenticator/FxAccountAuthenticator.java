/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.activities.FxAccountGetStartedActivity;

import android.accounts.AbstractAccountAuthenticator;
import android.accounts.Account;
import android.accounts.AccountAuthenticatorResponse;
import android.accounts.AccountManager;
import android.accounts.NetworkErrorException;
import android.content.Context;
import android.content.Intent;
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

    Intent intent = new Intent(context, FxAccountGetStartedActivity.class);
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
   * If the account is going to be removed, broadcast an "account deleted"
   * intent. This allows us to clean up the account.
   * <p>
   * It is preferable to receive Android's LOGIN_ACCOUNTS_CHANGED_ACTION broadcast
   * than to create our own hacky broadcast here, but that doesn't include enough
   * information about which Accounts changed to correctly identify whether a Sync
   * account has been removed (when some Firefox channels are installed on the SD
   * card). We can work around this by storing additional state but it's both messy
   * and expensive because the broadcast is noisy.
   * <p>
   * Note that this is <b>not</b> called when an Android Account is blown away
   * due to the SD card being unmounted.
   */
  @Override
  public Bundle getAccountRemovalAllowed(final AccountAuthenticatorResponse response, Account account)
      throws NetworkErrorException {
    Bundle result = super.getAccountRemovalAllowed(response, account);

    if (result == null ||
        !result.containsKey(AccountManager.KEY_BOOLEAN_RESULT) ||
        result.containsKey(AccountManager.KEY_INTENT)) {
      return result;
    }

    final boolean removalAllowed = result.getBoolean(AccountManager.KEY_BOOLEAN_RESULT);
    if (!removalAllowed) {
      return result;
    }

    // Broadcast a message to all Firefox channels sharing this Android
    // Account type telling that this Firefox account has been deleted.
    //
    // Broadcast intents protected with permissions are secure, so it's okay
    // to include private information such as a password.
    final Intent intent = AndroidFxAccount.makeDeletedAccountIntent(context, account);
    Logger.info(LOG_TAG, "Account named " + account.name + " being removed; " +
        "broadcasting secure intent " + intent.getAction() + ".");
    context.sendBroadcast(intent, FxAccountConstants.PER_ACCOUNT_TYPE_PERMISSION);

    return result;
  }
}
