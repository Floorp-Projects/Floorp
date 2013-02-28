/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup;

import java.io.UnsupportedEncodingException;
import java.security.NoSuchAlgorithmException;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.SyncConstants;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.setup.activities.SetupSyncActivity;

import android.accounts.AbstractAccountAuthenticator;
import android.accounts.Account;
import android.accounts.AccountAuthenticatorResponse;
import android.accounts.AccountManager;
import android.accounts.NetworkErrorException;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;

public class SyncAuthenticatorService extends Service {
  private static final String LOG_TAG = "SyncAuthService";

  private SyncAccountAuthenticator sAccountAuthenticator = null;

  @Override
  public void onCreate() {
    Logger.debug(LOG_TAG, "onCreate");
    sAccountAuthenticator = getAuthenticator();
  }

  @Override
  public IBinder onBind(Intent intent) {
    if (intent.getAction().equals(android.accounts.AccountManager.ACTION_AUTHENTICATOR_INTENT)) {
      return getAuthenticator().getIBinder();
    }
    return null;
  }

  private SyncAccountAuthenticator getAuthenticator() {
    if (sAccountAuthenticator == null) {
      sAccountAuthenticator = new SyncAccountAuthenticator(this);
    }
    return sAccountAuthenticator;
  }

  /**
   * Generate a "plain" auth token.
   * <p>
   * Android caches only the value of the key
   * <code>AccountManager.KEY_AUTHTOKEN</code>, so if a caller needs the other
   * keys in this bundle, it needs to invalidate the token (so that the bundle
   * is re-generated).
   *
   * @param context
   *          Android context.
   * @param account
   *          Android account.
   * @return a <code>Bundle</code> instance containing a subset of the following
   *         keys: (caller's must check for missing keys)
   *         <ul>
   *         <li><code>AccountManager.KEY_ACCOUNT_TYPE</code>: the Android
   *         Account's type</li>
   *
   *         <li><code>AccountManager.KEY_ACCOUNT_NAME</code>: the Android
   *         Account's name</li>
   *
   *         <li><code>AccountManager.KEY_AUTHTOKEN</code>: the Sync account's
   *         password </li>
   *
   *         <li><code> Constants.OPTION_USERNAME</code>: the Sync account's
   *         hashed username</li>
   *
   *         <li><code>Constants.OPTION_SERVER</code>: the Sync account's
   *         server</li>
   *
   *         <li><code> Constants.OPTION_SYNCKEY</code>: the Sync account's
   *         sync key</li>
   *
   *         </ul>
   * @throws NetworkErrorException
   */
  public static Bundle getPlainAuthToken(final Context context, final Account account)
      throws NetworkErrorException {
    // Extract the username and password from the Account Manager, and ask
    // the server for an appropriate AuthToken.
    final AccountManager am = AccountManager.get(context);
    final String password = am.getPassword(account);
    if (password == null) {
      Logger.warn(LOG_TAG, "Returning null bundle for getPlainAuthToken since Account password is null.");
      return null;
    }

    final Bundle result = new Bundle();

    // This is a Sync account.
    result.putString(AccountManager.KEY_ACCOUNT_TYPE, SyncConstants.ACCOUNTTYPE_SYNC);

    // Server.
    String serverURL = am.getUserData(account, Constants.OPTION_SERVER);
    result.putString(Constants.OPTION_SERVER, serverURL);

    // Full username, before hashing.
    result.putString(AccountManager.KEY_ACCOUNT_NAME, account.name);

    // Username after hashing.
    try {
      String username = Utils.usernameFromAccount(account.name);
      Logger.pii(LOG_TAG, "Account " + account.name + " hashes to " + username + ".");
      Logger.debug(LOG_TAG, "Setting username. Null? " + (username == null));
      result.putString(Constants.OPTION_USERNAME, username);
    } catch (NoSuchAlgorithmException e) {
      // Do nothing. Calling code must check for missing value.
      Logger.debug(LOG_TAG, "Exception in account lookup: " + e);
    } catch (UnsupportedEncodingException e) {
      // Do nothing. Calling code must check for missing value.
      Logger.debug(LOG_TAG, "Exception in account lookup: " + e);
    }

    // Sync key.
    final String syncKey = am.getUserData(account, Constants.OPTION_SYNCKEY);
    Logger.debug(LOG_TAG, "Setting sync key. Null? " + (syncKey == null));
    result.putString(Constants.OPTION_SYNCKEY, syncKey);

    // Password.
    result.putString(AccountManager.KEY_AUTHTOKEN, password);
    return result;
  }

  private static class SyncAccountAuthenticator extends AbstractAccountAuthenticator {
    private Context mContext;
    public SyncAccountAuthenticator(Context context) {
      super(context);
      mContext = context;
    }

    @Override
    public Bundle addAccount(AccountAuthenticatorResponse response,
        String accountType, String authTokenType, String[] requiredFeatures,
        Bundle options) throws NetworkErrorException {
      Logger.debug(LOG_TAG, "addAccount()");
      final Intent intent = new Intent(mContext, SetupSyncActivity.class);
      intent.putExtra(AccountManager.KEY_ACCOUNT_AUTHENTICATOR_RESPONSE,
                      response);
      intent.putExtra("accountType", SyncConstants.ACCOUNTTYPE_SYNC);
      intent.putExtra(Constants.INTENT_EXTRA_IS_SETUP, true);

      final Bundle result = new Bundle();
      result.putParcelable(AccountManager.KEY_INTENT, intent);

      return result;
    }

    @Override
    public Bundle confirmCredentials(AccountAuthenticatorResponse response,
                                     Account account,
                                     Bundle options) throws NetworkErrorException {
      Logger.debug(LOG_TAG, "confirmCredentials()");
      return null;
    }

    @Override
    public Bundle editProperties(AccountAuthenticatorResponse response,
                                 String accountType) {
      Logger.debug(LOG_TAG, "editProperties");
      return null;
    }

    @Override
    public Bundle getAuthToken(AccountAuthenticatorResponse response,
        Account account, String authTokenType, Bundle options)
        throws NetworkErrorException {
      Logger.debug(LOG_TAG, "getAuthToken()");

      if (Constants.AUTHTOKEN_TYPE_PLAIN.equals(authTokenType)) {
        return getPlainAuthToken(mContext, account);
      }

      final Bundle result = new Bundle();
      result.putString(AccountManager.KEY_ERROR_MESSAGE, "invalid authTokenType");
      return result;
    }

    @Override
    public String getAuthTokenLabel(String authTokenType) {
      Logger.debug(LOG_TAG, "getAuthTokenLabel()");
      return null;
    }

    @Override
    public Bundle hasFeatures(AccountAuthenticatorResponse response,
        Account account, String[] features) throws NetworkErrorException {
      Logger.debug(LOG_TAG, "hasFeatures()");
      return null;
    }

    @Override
    public Bundle updateCredentials(AccountAuthenticatorResponse response,
        Account account, String authTokenType, Bundle options)
        throws NetworkErrorException {
      Logger.debug(LOG_TAG, "updateCredentials()");
      return null;
    }

    /**
     * Bug 769745: persist pickled Sync account settings so that we can unpickle
     * after Fennec is moved to the SD card.
     * <p>
     * This is <b>not</b> called when an Android Account is blown away due to
     * the SD card being unmounted.
     * <p>
     * Broadcasting a Firefox intent to version sharing this Android Account is
     * a terrible hack, but it's better than the catching the generic
     * "accounts changed" broadcast intent and trying to figure out whether our
     * Account disappeared.
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

      // Bug 790931: Broadcast a message to all Firefox versions sharing this
      // Android Account type telling that this Sync Account has been deleted.
      //
      // We would really prefer to receive Android's
      // LOGIN_ACCOUNTS_CHANGED_ACTION broadcast, but that
      // doesn't include enough information about which Accounts changed to
      // correctly identify whether a Sync account has been removed (when some
      // Firefox versions are installed on the SD card).
      //
      // Broadcast intents protected with permissions are secure, so it's okay
      // to include password and sync key, etc.
      final Intent intent = SyncAccounts.makeSyncAccountDeletedIntent(mContext, AccountManager.get(mContext), account);
      Logger.info(LOG_TAG, "Account named " + account.name + " being removed; " +
          "broadcasting secure intent " + intent.getAction() + ".");
      mContext.sendBroadcast(intent, SyncConstants.PER_ACCOUNT_TYPE_PERMISSION);

      return result;
    }
  }
}
