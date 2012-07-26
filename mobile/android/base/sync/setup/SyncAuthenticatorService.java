/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup;

import java.io.UnsupportedEncodingException;
import java.security.NoSuchAlgorithmException;

import org.mozilla.gecko.sync.GlobalConstants;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.config.AccountPickler;
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
      intent.putExtra("accountType", GlobalConstants.ACCOUNTTYPE_SYNC);
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
      if (!authTokenType.equals(Constants.AUTHTOKEN_TYPE_PLAIN)) {
        final Bundle result = new Bundle();
        result.putString(AccountManager.KEY_ERROR_MESSAGE,
            "invalid authTokenType");
        return result;
      }

      // Extract the username and password from the Account Manager, and ask
      // the server for an appropriate AuthToken.
      final AccountManager am = AccountManager.get(mContext);
      final String password = am.getPassword(account);
      if (password != null) {
        final Bundle result = new Bundle();

        // This is a Sync account.
        result.putString(AccountManager.KEY_ACCOUNT_TYPE, GlobalConstants.ACCOUNTTYPE_SYNC);

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
        } catch (UnsupportedEncodingException e) {
          // Do nothing. Calling code must check for missing value.
        }

        // Sync key.
        final String syncKey = am.getUserData(account, Constants.OPTION_SYNCKEY);
        Logger.debug(LOG_TAG, "Setting sync key. Null? " + (syncKey == null));
        result.putString(Constants.OPTION_SYNCKEY, syncKey);

        // Password.
        result.putString(AccountManager.KEY_AUTHTOKEN, password);
        return result;
      }
      Logger.warn(LOG_TAG, "Returning null bundle for getAuthToken.");
      return null;
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
     * This is <b>not</b> called when an Android Account is blown away due to the
     * SD card being unmounted.
     * <p>
     * This is a terrible hack, but it's better than the catching the generic
     * "accounts changed" broadcast intent and trying to figure out whether our
     * Account disappeared.
     */
    @Override
    public Bundle getAccountRemovalAllowed(AccountAuthenticatorResponse response, Account account) throws NetworkErrorException {
      Bundle result = super.getAccountRemovalAllowed(response, account);

      if (result != null &&
          result.containsKey(AccountManager.KEY_BOOLEAN_RESULT) &&
          !result.containsKey(AccountManager.KEY_INTENT)) {
        final boolean removalAllowed = result.getBoolean(AccountManager.KEY_BOOLEAN_RESULT);

        if (removalAllowed) {
          Logger.info(LOG_TAG, "Account named " + account.name + " being removed; " +
              "deleting saved pickle file '" + Constants.ACCOUNT_PICKLE_FILENAME + "'.");
          AccountPickler.deletePickle(mContext, Constants.ACCOUNT_PICKLE_FILENAME);
        }
      }

      return result;
    }
  }
}
