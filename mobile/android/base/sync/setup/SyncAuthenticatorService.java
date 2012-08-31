/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup;

import java.io.UnsupportedEncodingException;
import java.security.NoSuchAlgorithmException;

import org.mozilla.gecko.sync.GlobalConstants;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.config.AccountPickler;
import org.mozilla.gecko.sync.config.ClientRecordTerminator;
import org.mozilla.gecko.sync.setup.SyncAccounts.SyncAccountParameters;
import org.mozilla.gecko.sync.setup.activities.SetupSyncActivity;

import android.accounts.AbstractAccountAuthenticator;
import android.accounts.Account;
import android.accounts.AccountAuthenticatorResponse;
import android.accounts.AccountManager;
import android.accounts.NetworkErrorException;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
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
     * This is <b>not</b> called when an Android Account is blown away due to the
     * SD card being unmounted.
     * <p>
     * This is a terrible hack, but it's better than the catching the generic
     * "accounts changed" broadcast intent and trying to figure out whether our
     * Account disappeared.
     */
    @Override
    public Bundle getAccountRemovalAllowed(final AccountAuthenticatorResponse response, final Account account)
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

      // Delete the Account pickle in the background.
      ThreadPool.run(new Runnable() {
        @Override
        public void run() {
          Logger.info(LOG_TAG, "Account named " + account.name + " being removed; " +
              "deleting saved pickle file '" + Constants.ACCOUNT_PICKLE_FILENAME + "'.");
          try {
            AccountPickler.deletePickle(mContext, Constants.ACCOUNT_PICKLE_FILENAME);
          } catch (Exception e) {
            // This should never happen, but we really don't want to die in a background thread.
            Logger.warn(LOG_TAG, "Got exception deleting saved pickle file; ignoring.", e);
          }
        }
      });

      // Bug 770785: delete the Account's client record in the background. We want
      // to get the Account's data synchronously, though, since it is possible the
      // Account object will be invalid by the time the Runnable executes. We
      // don't need to worry about accessing prefs too early since deleting the
      // Account doesn't remove them -- at least, not yet.
      SyncAccountParameters tempParams = null;
      try {
        tempParams = SyncAccounts.blockingFromAndroidAccountV0(mContext, AccountManager.get(mContext), account);
      } catch (Exception e) {
        // Do nothing.  Null parameters are handled in the Runnable.
      }
      final SyncAccountParameters params = tempParams;

      ThreadPool.run(new Runnable() {
        @Override
        public void run() {
          Logger.info(LOG_TAG, "Account named " + account.name + " being removed; " +
              "deleting client record from server.");

          if (params == null || params.username == null || params.password == null) {
            Logger.warn(LOG_TAG, "Account parameters were null; not deleting client record from server.");
            return;
          }

          // This is not exactly modular. We need to get some information about
          // the account, namely the current clusterURL and client GUID, and we
          // extract it by hand. We're not worried about the Account being
          // deleted out from under us since the prefs remain even after Account
          // deletion.
          final String product = GlobalConstants.BROWSER_INTENT_PACKAGE;
          final String profile = Constants.DEFAULT_PROFILE;
          final long version = SyncConfiguration.CURRENT_PREFS_VERSION;

          SharedPreferences prefs;
          try {
            prefs = Utils.getSharedPreferences(mContext, product, params.username, params.serverURL, profile, version);
          } catch (Exception e) {
            Logger.warn(LOG_TAG, "Caught exception fetching preferences; not deleting client record from server.", e);
            return;
          }

          final String clientGuid = prefs.getString(SyncConfiguration.PREF_ACCOUNT_GUID, null);
          final String clusterURL = prefs.getString(SyncConfiguration.PREF_CLUSTER_URL, null);

          if (clientGuid == null) {
            Logger.warn(LOG_TAG, "Client GUID was null; not deleting client record from server.");
            return;
          }

          if (clusterURL == null) {
            Logger.warn(LOG_TAG, "Cluster URL was null; not deleting client record from server.");
            return;
          }

          try {
            ClientRecordTerminator.deleteClientRecord(params.username, params.password, clusterURL, clientGuid);
          } catch (Exception e) {
            Logger.warn(LOG_TAG, "Got exception deleting client record from server; ignoring.", e);
          }
        }
      });

      return result;
    }
  }
}
