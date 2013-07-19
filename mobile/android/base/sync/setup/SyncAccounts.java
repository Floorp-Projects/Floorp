/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup;

import java.io.File;
import java.io.UnsupportedEncodingException;
import java.security.NoSuchAlgorithmException;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.CredentialException;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.SyncConstants;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.config.AccountPickler;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;

/**
 * This class contains utilities that are of use to Fennec
 * and Sync setup activities.
 * <p>
 * Do not break these APIs without correcting upstream code!
 */
public class SyncAccounts {
  private static final String LOG_TAG = "SyncAccounts";

  private static final String MOTO_BLUR_SETTINGS_ACTIVITY = "com.motorola.blur.settings.AccountsAndServicesPreferenceActivity";
  private static final String MOTO_BLUR_PACKAGE           = "com.motorola.blur.setup";

  public final static String DEFAULT_SERVER = "https://auth.services.mozilla.com/";

  /**
   * Return Sync accounts.
   *
   * @param c
   *          Android context.
   * @return Sync accounts.
   */
  public static Account[] syncAccounts(final Context c) {
    return AccountManager.get(c).getAccountsByType(SyncConstants.ACCOUNTTYPE_SYNC);
  }

  /**
   * Returns true if a Sync account is set up, or we have a pickled Sync account
   * on disk that should be un-pickled (Bug 769745). If we have a pickled Sync
   * account, try to un-pickle it and create the corresponding Sync account.
   * <p>
   * Do not call this method from the main thread.
   */
  public static boolean syncAccountsExist(Context c) {
    final boolean accountsExist = AccountManager.get(c).getAccountsByType(SyncConstants.ACCOUNTTYPE_SYNC).length > 0;
    if (accountsExist) {
      return true;
    }

    final File file = c.getFileStreamPath(Constants.ACCOUNT_PICKLE_FILENAME);
    if (!file.exists()) {
      return false;
    }

    // There is a small race window here: if the user creates a new Sync account
    // between our checks, this could erroneously report that no Sync accounts
    // exist.
    final Account account = AccountPickler.unpickle(c, Constants.ACCOUNT_PICKLE_FILENAME);
    return (account != null);
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

    public SyncAccountParameters(final Context context, final AccountManager accountManager, final ExtendedJSONObject o) {
      this(context, accountManager,
          o.getString(Constants.JSON_KEY_ACCOUNT),
          o.getString(Constants.JSON_KEY_SYNCKEY),
          o.getString(Constants.JSON_KEY_PASSWORD),
          o.getString(Constants.JSON_KEY_SERVER),
          o.getString(Constants.JSON_KEY_CLUSTER),
          o.getString(Constants.JSON_KEY_CLIENT_NAME),
          o.getString(Constants.JSON_KEY_CLIENT_GUID));
    }

    public ExtendedJSONObject asJSON() {
      final ExtendedJSONObject o = new ExtendedJSONObject();
      o.put(Constants.JSON_KEY_ACCOUNT, username);
      o.put(Constants.JSON_KEY_PASSWORD, password);
      o.put(Constants.JSON_KEY_SERVER, serverURL);
      o.put(Constants.JSON_KEY_SYNCKEY, syncKey);
      o.put(Constants.JSON_KEY_CLUSTER, clusterURL);
      o.put(Constants.JSON_KEY_CLIENT_NAME, clientName);
      o.put(Constants.JSON_KEY_CLIENT_GUID, clientGuid);
      return o;
    }
  }

  /**
   * Create a sync account, clearing any existing preferences, and set it to
   * sync automatically.
   * <p>
   * Do not call this method from the main thread.
   *
   * @param syncAccount
   *          parameters of the account to be created.
   * @return created <code>Account</code>, or null if an error occurred and the
   *         account could not be added.
   */
  public static Account createSyncAccount(SyncAccountParameters syncAccount) {
    return createSyncAccount(syncAccount, true, true);
  }

  /**
   * Create a sync account, clearing any existing preferences.
   * <p>
   * Do not call this method from the main thread.
   * <p>
   * Intended for testing; use
   * <code>createSyncAccount(SyncAccountParameters)</code> instead.
   *
   * @param syncAccount
   *          parameters of the account to be created.
   * @param syncAutomatically
   *          whether to start syncing this Account automatically (
   *          <code>false</code> for test accounts).
   * @return created Android <code>Account</code>, or null if an error occurred
   *         and the account could not be added.
   */
  public static Account createSyncAccount(SyncAccountParameters syncAccount,
      boolean syncAutomatically) {
    return createSyncAccount(syncAccount, syncAutomatically, true);
  }

  public static Account createSyncAccountPreservingExistingPreferences(SyncAccountParameters syncAccount,
      boolean syncAutomatically) {
    return createSyncAccount(syncAccount, syncAutomatically, false);
  }

  /**
   * Create a sync account.
   * <p>
   * Do not call this method from the main thread.
   * <p>
   * Intended for testing; use
   * <code>createSyncAccount(SyncAccountParameters)</code> instead.
   *
   * @param syncAccount
   *          parameters of the account to be created.
   * @param syncAutomatically
   *          whether to start syncing this Account automatically (
   *          <code>false</code> for test accounts).
   * @param clearPreferences
   *          <code>true</code> to clear existing preferences before creating.
   * @return created Android <code>Account</code>, or null if an error occurred
   *         and the account could not be added.
   */
  protected static Account createSyncAccount(SyncAccountParameters syncAccount,
      boolean syncAutomatically, boolean clearPreferences) {
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

    final Account account = new Account(username, SyncConstants.ACCOUNTTYPE_SYNC);
    final Bundle userbundle = new Bundle();

    // Add sync key and server URL.
    userbundle.putString(Constants.OPTION_SYNCKEY, syncKey);
    userbundle.putString(Constants.OPTION_SERVER, serverURL);
    Logger.debug(LOG_TAG, "Adding account for " + SyncConstants.ACCOUNTTYPE_SYNC);
    boolean result = false;
    try {
      result = accountManager.addAccountExplicitly(account, password, userbundle);
    } catch (SecurityException e) {
      // We use Log rather than Logger here to avoid possibly hiding these errors.
      final String message = e.getMessage();
      if (message != null && (message.indexOf("is different than the authenticator's uid") > 0)) {
        Log.wtf(SyncConstants.GLOBAL_LOG_TAG,
                "Unable to create account. " +
                "If you have more than one version of " +
                "Firefox/Beta/Aurora/Nightly/Fennec installed, that's why.",
                e);
      } else {
        Log.e(SyncConstants.GLOBAL_LOG_TAG, "Unable to create account.", e);
      }
    }

    if (!result) {
      Logger.error(LOG_TAG, "Failed to add account " + account + "!");
      return null;
    }
    Logger.debug(LOG_TAG, "Account " + account + " added successfully.");

    setSyncAutomatically(account, syncAutomatically);
    setIsSyncable(account, syncAutomatically);
    Logger.debug(LOG_TAG, "Set account to sync automatically? " + syncAutomatically + ".");

    try {
      final String product = GlobalConstants.BROWSER_INTENT_PACKAGE;
      final String profile = Constants.DEFAULT_PROFILE;
      final long version = SyncConfiguration.CURRENT_PREFS_VERSION;

      final SharedPreferences.Editor editor = Utils.getSharedPreferences(context, product, username, serverURL, profile, version).edit();
      if (clearPreferences) {
        final String prefsPath = Utils.getPrefsPath(product, username, serverURL, profile, version);
        Logger.info(LOG_TAG, "Clearing preferences path " + prefsPath + " for this account.");
        editor.clear();
      }

      if (syncAccount.clusterURL != null) {
        editor.putString(SyncConfiguration.PREF_CLUSTER_URL, syncAccount.clusterURL);
      }

      if (syncAccount.clientName != null && syncAccount.clientGuid != null) {
        Logger.debug(LOG_TAG, "Setting client name to " + syncAccount.clientName + " and client GUID to " + syncAccount.clientGuid + ".");
        editor.putString(SyncConfiguration.PREF_CLIENT_NAME, syncAccount.clientName);
        editor.putString(SyncConfiguration.PREF_ACCOUNT_GUID, syncAccount.clientGuid);
      } else {
        Logger.debug(LOG_TAG, "Client name and guid not both non-null, so not setting client data.");
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

  public static void backgroundSetSyncAutomatically(final Account account, final boolean syncAutomatically) {
    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        setSyncAutomatically(account, syncAutomatically);
      }
    });
  }
  /**
   * Bug 721760: try to start a vendor-specific Accounts & Sync activity on Moto
   * Blur devices.
   * <p>
   * Bug 773562: actually start and catch <code>ActivityNotFoundException</code>,
   * rather than just returning the <code>Intent</code> only, because some
   * Moto devices fail to start the activity.
   *
   * @param context
   *          current Android context.
   * @param vendorPackage
   *          vendor specific package name.
   * @param vendorClass
   *          vendor specific class name.
   * @return null on failure, otherwise the <code>Intent</code> started.
   */
  protected static Intent openVendorSyncSettings(Context context, final String vendorPackage, final String vendorClass) {
    try {
      final int contextFlags = Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY;
      Context foreignContext = context.createPackageContext(vendorPackage, contextFlags);
      Class<?> klass = foreignContext.getClassLoader().loadClass(vendorClass);

      final Intent intent = new Intent(foreignContext, klass);
      context.startActivity(intent);
      Logger.info(LOG_TAG, "Vendor package " + vendorPackage + " and class " +
          vendorClass + " found, and activity launched.");
      return intent;
    } catch (NameNotFoundException e) {
      Logger.debug(LOG_TAG, "Vendor package " + vendorPackage + " not found. Skipping.");
    } catch (ClassNotFoundException e) {
      Logger.debug(LOG_TAG, "Vendor package " + vendorPackage + " found but class " +
          vendorClass + " not found. Skipping.", e);
    } catch (ActivityNotFoundException e) {
      // Bug 773562 - android.content.ActivityNotFoundException on Motorola devices.
      Logger.warn(LOG_TAG, "Vendor package " + vendorPackage + " and class " +
          vendorClass + " found, but activity not launched. Skipping.", e);
    } catch (Exception e) {
      // Just in case.
      Logger.warn(LOG_TAG, "Caught exception launching activity from vendor package " + vendorPackage +
          " and class " + vendorClass + ". Ignoring.", e);
    }
    return null;
  }

  /**
   * Start Sync settings activity.
   *
   * @param context
   *          current Android context.
   * @return the <code>Intent</code> started.
   */
  public static Intent openSyncSettings(Context context) {
    // Bug 721760 - opening Sync settings takes user to Battery & Data Manager
    // on a variety of Motorola devices. This work around tries to load the
    // correct Intent by hand. Oh, Android.
    Intent intent = openVendorSyncSettings(context, MOTO_BLUR_PACKAGE, MOTO_BLUR_SETTINGS_ACTIVITY);
    if (intent != null) {
      return intent;
    }

    // Open default Sync settings activity.
    intent = new Intent(Settings.ACTION_SYNC_SETTINGS);
    // Bug 774233: do not start activity as a new task (second run fails on some HTC devices).
    context.startActivity(intent); // We should always find this Activity.
    return intent;
  }

  /**
   * Synchronously extract Sync account parameters from Android account version
   * 0, using plain auth token type.
   * <p>
   * Safe to call from main thread.
   *
   * @param context
   *          Android context.
   * @param accountManager
   *          Android account manager.
   * @param account
   *          Android Account.
   * @return Sync account parameters, always non-null; fields username,
   *         password, serverURL, and syncKey always non-null.
   */
  public static SyncAccountParameters blockingFromAndroidAccountV0(final Context context, final AccountManager accountManager, final Account account)
      throws CredentialException {
    String username;
    try {
      username = Utils.usernameFromAccount(account.name);
    } catch (NoSuchAlgorithmException e) {
      throw new CredentialException.MissingCredentialException("username");
    } catch (UnsupportedEncodingException e) {
      throw new CredentialException.MissingCredentialException("username");
    }

    /*
     * If we are accessing an Account that we don't own, Android will throw an
     * unchecked <code>SecurityException</code> saying
     * "W FxSync(XXXX) java.lang.SecurityException: caller uid XXXXX is different than the authenticator's uid".
     * We catch that error and throw accordingly.
     */
    String password;
    String syncKey;
    String serverURL;
    try {
      password = accountManager.getPassword(account);
      syncKey = accountManager.getUserData(account, Constants.OPTION_SYNCKEY);
      serverURL = accountManager.getUserData(account, Constants.OPTION_SERVER);
    } catch (SecurityException e) {
      Logger.warn(LOG_TAG, "Got security exception fetching Sync account parameters; throwing.");
      throw new CredentialException.MissingAllCredentialsException(e);
    }

    if (password  == null &&
        username  == null &&
        syncKey   == null &&
        serverURL == null) {
      throw new CredentialException.MissingAllCredentialsException();
    }

    if (password == null) {
      throw new CredentialException.MissingCredentialException("password");
    }

    if (syncKey == null) {
      throw new CredentialException.MissingCredentialException("syncKey");
    }

    if (serverURL == null) {
      throw new CredentialException.MissingCredentialException("serverURL");
    }

    try {
      // SyncAccountParameters constructor throws on null inputs. This shouldn't
      // happen, but let's be safe.
      return new SyncAccountParameters(context, accountManager, username, syncKey, password, serverURL);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception fetching Sync account parameters; throwing.");
      throw new CredentialException.MissingAllCredentialsException(e);
    }
  }

  /**
   * Bug 790931: create an intent announcing that a Sync account will be
   * deleted.
   * <p>
   * This intent <b>must</b> be broadcast with secure permissions, because it
   * contains sensitive user information including the Sync account password and
   * Sync key.
   * <p>
   * Version 1 of the created intent includes extras with keys
   * <code>Constants.JSON_KEY_VERSION</code>,
   * <code>Constants.JSON_KEY_TIMESTAMP</code>, and
   * <code>Constants.JSON_KEY_ACCOUNT</code> (which is the Android Account name,
   * not the encoded Sync Account name).
   * <p>
   * If possible, it contains the key <code>Constants.JSON_KEY_PAYLOAD</code>
   * with value the Sync account parameters as JSON, <b>except the Sync key has
   * been replaced with the empty string</b>. (We replace, rather than remove,
   * the Sync key because SyncAccountParameters expects a non-null Sync key.)
   *
   * @see SyncAccountParameters#asJSON
   *
   * @param context
   *          Android context.
   * @param accountManager
   *          Android account manager.
   * @param account
   *          Android account being removed.
   * @return <code>Intent</code> to broadcast.
   */
  public static Intent makeSyncAccountDeletedIntent(final Context context, final AccountManager accountManager, final Account account) {
    final Intent intent = new Intent(SyncConstants.SYNC_ACCOUNT_DELETED_ACTION);

    intent.putExtra(Constants.JSON_KEY_VERSION, Long.valueOf(SyncConstants.SYNC_ACCOUNT_DELETED_INTENT_VERSION));
    intent.putExtra(Constants.JSON_KEY_TIMESTAMP, Long.valueOf(System.currentTimeMillis()));
    intent.putExtra(Constants.JSON_KEY_ACCOUNT, account.name);

    SyncAccountParameters accountParameters = null;
    try {
      accountParameters = SyncAccounts.blockingFromAndroidAccountV0(context, accountManager, account);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Caught exception fetching account parameters.", e);
    }

    if (accountParameters != null) {
      ExtendedJSONObject json = accountParameters.asJSON();
      json.put(Constants.JSON_KEY_SYNCKEY, ""); // Reduce attack surface area by removing Sync key.
      intent.putExtra(Constants.JSON_KEY_PAYLOAD, json.toJSONString());
    }

    return intent;
  }

  /**
   * Synchronously fetch SharedPreferences of a profile associated with a Sync
   * account.
   * <p>
   * Safe to call from main thread.
   *
   * @param context
   *          Android context.
   * @param accountManager
   *          Android account manager.
   * @param account
   *          Android Account.
   * @param product
   *          package.
   * @param profile
   *          of account.
   * @param version
   *          number.
   * @return SharedPreferences associated with Sync account.
   * @throws CredentialException
   * @throws NoSuchAlgorithmException
   * @throws UnsupportedEncodingException
   */
  public static SharedPreferences blockingPrefsFromAndroidAccountV0(final Context context, final AccountManager accountManager, final Account account,
      final String product, final String profile, final long version)
          throws CredentialException, NoSuchAlgorithmException, UnsupportedEncodingException {
    SyncAccountParameters params = SyncAccounts.blockingFromAndroidAccountV0(context, accountManager, account);
    String prefsPath = Utils.getPrefsPath(product, params.username, params.serverURL, profile, version);

    return context.getSharedPreferences(prefsPath, Utils.SHARED_PREFERENCES_MODE);
  }

  /**
   * Synchronously fetch SharedPreferences of a profile associated with the
   * default Firefox profile of a Sync Account.
   * <p>
   * Uses the default package, default profile, and current version.
   * <p>
   * Safe to call from main thread.
   *
   * @param context
   *          Android context.
   * @param accountManager
   *          Android account manager.
   * @param account
   *          Android Account.
   * @return SharedPreferences associated with Sync account.
   * @throws CredentialException
   * @throws NoSuchAlgorithmException
   * @throws UnsupportedEncodingException
   */
  public static SharedPreferences blockingPrefsFromDefaultProfileV0(final Context context, final AccountManager accountManager, final Account account)
      throws CredentialException, NoSuchAlgorithmException, UnsupportedEncodingException {
    final String product = GlobalConstants.BROWSER_INTENT_PACKAGE;
    final String profile = Constants.DEFAULT_PROFILE;
    final long version = SyncConfiguration.CURRENT_PREFS_VERSION;

    return blockingPrefsFromAndroidAccountV0(context, accountManager, account, product, profile, version);
  }
}
