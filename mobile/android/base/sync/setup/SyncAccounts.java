/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;
import org.mozilla.gecko.sync.syncadapter.SyncAdapter;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Bundle;
import android.util.Log;

/**
 * This class contains utilities that are of use to Fennec
 * and Sync setup activities.
 * <p>
 * Do not break these APIs without correcting upstream code!
 */
public class SyncAccounts {

  public final static String DEFAULT_SERVER = "https://auth.services.mozilla.com/";
  private static final String LOG_TAG = "SyncAccounts";
  private static final String GLOBAL_LOG_TAG = "FxSync";

  /**
   * Returns true if a Sync account is set up.
   * <p>
   * Do not call this method from the main thread.
   */
  public static boolean syncAccountsExist(Context c) {
    return AccountManager.get(c).getAccountsByType(Constants.ACCOUNTTYPE_SYNC).length > 0;
  }

  /**
   * This class provides background-thread abstracted access to whether a
   * Firefox Sync account has been set up on this device.
   * <p>
   * Subclass this task and override `onPostExecute` to act on the result.
   */
  public static class AccountsExistTask extends AsyncTask<Context, Void, Boolean> {
    @Override
    protected Boolean doInBackground(Context... params) {
      Context c = params[0];
      return syncAccountsExist(c);
    }
  }

  /**
   * This class encapsulates the parameters needed to create a new Firefox Sync
   * account.
   */
  public static class SyncAccountParameters {
    public final Context context;
    public final AccountManager accountManager;


    public final String username;   // services.sync.account
    public final String syncKey;    // in password manager: "chrome://weave (Mozilla Services Encryption Passphrase)"
    public final String password;   // in password manager: "chrome://weave (Mozilla Services Password)"
    public final String serverURL;  // services.sync.serverURL
    public final String clusterURL; // services.sync.clusterURL
    public final String clientName; // services.sync.client.name
    public final String clientGuid; // services.sync.client.GUID

    /**
     * Encapsulate the parameters needed to create a new Firefox Sync account.
     *
     * @param context
     *          the current <code>Context</code>; cannot be null.
     * @param accountManager
     *          an <code>AccountManager</code> instance to use; if null, get it
     *          from <code>context</code>.
     * @param username
     *          the desired username; cannot be null.
     * @param syncKey
     *          the desired sync key; cannot be null.
     * @param password
     *          the desired password; cannot be null.
     * @param serverURL
     *          the server URL to use; if null, use the default.
     * @param clusterURL
     *          the cluster URL to use; if null, a fresh cluster URL will be
     *          retrieved from the server during the next sync.
     * @param clientName
     *          the client name; if null, a fresh client record will be uploaded
     *          to the server during the next sync.
     * @param clientGuid
     *          the client GUID; if null, a fresh client record will be uploaded
     *          to the server during the next sync.
     */
    public SyncAccountParameters(Context context, AccountManager accountManager,
        String username, String syncKey, String password,
        String serverURL, String clusterURL,
        String clientName, String clientGuid) {
      if (context == null) {
        throw new IllegalArgumentException("Null context passed to SyncAccountParameters constructor.");
      }
      if (username == null) {
        throw new IllegalArgumentException("Null username passed to SyncAccountParameters constructor.");
      }
      if (syncKey == null) {
        throw new IllegalArgumentException("Null syncKey passed to SyncAccountParameters constructor.");
      }
      if (password == null) {
        throw new IllegalArgumentException("Null password passed to SyncAccountParameters constructor.");
      }
      this.context = context;
      this.accountManager = accountManager;
      this.username = username;
      this.syncKey = syncKey;
      this.password = password;
      this.serverURL = serverURL;
      this.clusterURL = clusterURL;
      this.clientName = clientName;
      this.clientGuid = clientGuid;
    }

    public SyncAccountParameters(Context context, AccountManager accountManager,
        String username, String syncKey, String password, String serverURL) {
      this(context, accountManager, username, syncKey, password, serverURL, null, null, null);
    }
  }

  /**
   * This class provides background-thread abstracted access to creating a
   * Firefox Sync account.
   * <p>
   * Subclass this task and override `onPostExecute` to act on the result. The
   * <code>Result</code> (of type <code>Account</code>) is null if an error
   * occurred and the account could not be added.
   */
  public static class CreateSyncAccountTask extends AsyncTask<SyncAccountParameters, Void, Account> {
    @Override
    protected Account doInBackground(SyncAccountParameters... params) {
      SyncAccountParameters syncAccount = params[0];
      try {
        return createSyncAccount(syncAccount);
      } catch (Exception e) {
        Log.e(GLOBAL_LOG_TAG, "Unable to create account.", e);
        return null;
      }
    }
  }

  /**
   * Create a sync account.
   * <p>
   * Do not call this method from the main thread.
   *
   * @param syncAccount
   *        The parameters of the account to be created.
   * @return The created <code>Account</code>, or null if an error occurred and
   *         the account could not be added.
   */
  public static Account createSyncAccount(SyncAccountParameters syncAccount) {
    final Context context = syncAccount.context;
    final AccountManager accountManager = (syncAccount.accountManager == null) ?
          AccountManager.get(syncAccount.context) : syncAccount.accountManager;
    final String username  = syncAccount.username;
    final String syncKey   = syncAccount.syncKey;
    final String password  = syncAccount.password;
    final String serverURL = (syncAccount.serverURL == null) ?
        DEFAULT_SERVER : syncAccount.serverURL;

    Logger.debug(LOG_TAG, "Using account manager " + accountManager);
    if (!RepoUtils.stringsEqual(syncAccount.serverURL, DEFAULT_SERVER)) {
      Logger.info(LOG_TAG, "Setting explicit server URL: " + serverURL);
    }

    final Account account = new Account(username, Constants.ACCOUNTTYPE_SYNC);
    final Bundle userbundle = new Bundle();

    // Add sync key and server URL.
    userbundle.putString(Constants.OPTION_SYNCKEY, syncKey);
    userbundle.putString(Constants.OPTION_SERVER, serverURL);
    Logger.debug(LOG_TAG, "Adding account for " + Constants.ACCOUNTTYPE_SYNC);
    boolean result = false;
    try {
      result = accountManager.addAccountExplicitly(account, password, userbundle);
    } catch (SecurityException e) {
      // We use Log rather than Logger here to avoid possibly hiding these errors.
      final String message = e.getMessage();
      if (message != null && (message.indexOf("is different than the authenticator's uid") > 0)) {
        Log.wtf(GLOBAL_LOG_TAG,
                "Unable to create account. " +
                "If you have more than one version of " +
                "Firefox/Beta/Aurora/Nightly/Fennec installed, that's why.",
                e);
      } else {
        Log.e(GLOBAL_LOG_TAG, "Unable to create account.", e);
      }
    }

    if (!result) {
      Logger.error(LOG_TAG, "Failed to add account " + account + "!");
      return null;
    }
    Logger.debug(LOG_TAG, "Account " + account + " added successfully.");

    setSyncAutomatically(account);
    setClientRecord(context, accountManager, account, syncAccount.clientName, syncAccount.clientGuid);

    // TODO: add other ContentProviders as needed (e.g. passwords)
    // TODO: for each, also add to res/xml to make visible in account settings
    Logger.debug(LOG_TAG, "Finished setting syncables.");

    // TODO: correctly implement Sync Options.
    Logger.info(LOG_TAG, "Clearing preferences for this account.");
    try {
      SharedPreferences.Editor editor = Utils.getSharedPreferences(context, username, serverURL).edit().clear();
      if (syncAccount.clusterURL != null) {
        editor.putString(SyncConfiguration.PREF_CLUSTER_URL, syncAccount.clusterURL);
      }
      editor.commit();
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Could not clear prefs path!", e);
    }
    return account;
  }

  public static void setIsSyncable(Account account, boolean isSyncable) {
    String authority = BrowserContract.AUTHORITY;
    ContentResolver.setIsSyncable(account, authority, isSyncable ? 1 : 0);
  }

  public static void setSyncAutomatically(Account account, boolean syncAutomatically) {
    if (syncAutomatically) {
      ContentResolver.setMasterSyncAutomatically(true);
    }

    String authority = BrowserContract.AUTHORITY;
    Logger.debug(LOG_TAG, "Setting authority " + authority + " to " +
                          (syncAutomatically ? "" : "not ") + "sync automatically.");
    ContentResolver.setSyncAutomatically(account, authority, syncAutomatically);
  }

  public static void setSyncAutomatically(Account account) {
    setSyncAutomatically(account, true);
    setIsSyncable(account, true);
  }

  protected static void setClientRecord(Context context, AccountManager accountManager, Account account,
      String clientName, String clientGuid) {
    if (clientName != null && clientGuid != null) {
      Logger.debug(LOG_TAG, "Setting client name to " + clientName + " and client GUID to " + clientGuid + ".");
      SyncAdapter.setAccountGUID(accountManager, account, clientGuid);
      SyncAdapter.setClientName(accountManager, account, clientName);
      return;
    }
    Logger.debug(LOG_TAG, "Client name and guid not both non-null, so not setting client data.");
  }
}
