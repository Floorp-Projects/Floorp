/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.config;

import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.Utils;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

/**
 * Migrate Sync preferences between versions.
 * <p>
 * The original preferences were un-versioned; we refer to that as "version 0".
 * The original preferences were stored in three places:
 * <ul>
 * <li>most prefs were kept in per-Sync account Android shared prefs;</li>
 * <li>some prefs were kept in per-App Android shared prefs;</li>
 * <li>some client prefs were kept in the (assumed unique) Android Account.</li>
 * </ul>
 * <p>
 * Post version 0, all preferences are stored in per-Sync account Android shared prefs.
 */
public class ConfigurationMigrator {
  public static final String LOG_TAG = "ConfigMigrator";

  /**
   * Copy and rename preferences.
   *
   * @param from source.
   * @param to sink.
   * @param map map from old preference names to new preference names.
   * @return the number of preferences migrated.
   */
  protected static int copyPreferences(final SharedPreferences from, final Map<String, String> map, final Editor to) {
    int count = 0;

    // SharedPreferences has no way to get a key/value pair without specifying the value type, so we do this instead.
    for (Entry<String, ?> entry : from.getAll().entrySet()) {
      String fromKey = entry.getKey();
      String toKey = map.get(fromKey);
      if (toKey == null) {
        continue;
      }

      Object value = entry.getValue();
      if (value instanceof Boolean) {
         to.putBoolean(toKey, ((Boolean) value).booleanValue());
      } else if (value instanceof Float) {
         to.putFloat(toKey, ((Float) value).floatValue());
      } else if (value instanceof Integer) {
         to.putInt(toKey, ((Integer) value).intValue());
      } else if (value instanceof Long) {
         to.putLong(toKey, ((Long) value).longValue());
      } else if (value instanceof String) {
         to.putString(toKey, (String) value);
      } else {
        // Do nothing -- perhaps SharedPreferences accepts types we don't know about.
      }

      if (Logger.LOG_PERSONAL_INFORMATION) {
        Logger.debug(LOG_TAG, "Migrated '" + fromKey + "' to '" + toKey + "' (" + value + ").");
      } else {
        Logger.debug(LOG_TAG, "Migrated '" + fromKey + "' to '" + toKey + "'.");
      }
      count += 1;
    }

    return count;
  }

  protected final static String V0_PREF_CLUSTER_URL_IS_STALE = "clusterurlisstale";
  protected final static String V1_PREF_CLUSTER_URL_IS_STALE = V0_PREF_CLUSTER_URL_IS_STALE;
  protected final static String V0_PREF_EARLIEST_NEXT_SYNC = "earliestnextsync";
  protected final static String V1_PREF_EARLIEST_NEXT_SYNC = V0_PREF_EARLIEST_NEXT_SYNC;

  /**
   * Extract version 0 preferences from per-App Android shared prefs and write to version 1 per-Sync account shared prefs.
   *
   * @param from per-App version 0 Android shared prefs.
   * @param to per-Sync account version 1 shared prefs.
   * @return the number of preferences migrated.
   * @throws Exception
   */
  protected static int upgradeGlobals0to1(final SharedPreferences from, final SharedPreferences to) throws Exception {
    Map<String, String> map = new HashMap<String, String>();
    map.put(V0_PREF_CLUSTER_URL_IS_STALE, V1_PREF_CLUSTER_URL_IS_STALE);
    map.put(V0_PREF_EARLIEST_NEXT_SYNC, V1_PREF_EARLIEST_NEXT_SYNC);

    Editor editor = to.edit();
    int count = copyPreferences(from, map, editor);
    if (count > 0) {
      editor.commit();
    }
    return count;
  }

  /**
   * Extract version 1 per-Sync account shared prefs and write to version 0 preferences from per-App Android shared prefs.
   *
   * @param from per-Sync account version 1 shared prefs.
   * @param to per-App version 0 Android shared prefs.
   * @return the number of preferences migrated.
   * @throws Exception
   */
  protected static int downgradeGlobals1to0(final SharedPreferences from, final SharedPreferences to) throws Exception {
    Map<String, String> map = new HashMap<String, String>();
    map.put(V1_PREF_CLUSTER_URL_IS_STALE, V0_PREF_CLUSTER_URL_IS_STALE);
    map.put(V1_PREF_EARLIEST_NEXT_SYNC, V0_PREF_EARLIEST_NEXT_SYNC);

    Editor editor = to.edit();
    int count = copyPreferences(from, map, editor);
    if (count > 0) {
      editor.commit();
    }
    return count;
  }

  protected static final String V0_PREF_ACCOUNT_GUID = "account.guid";
  protected static final String V1_PREF_ACCOUNT_GUID = V0_PREF_ACCOUNT_GUID;
  protected static final String V0_PREF_CLIENT_NAME = "account.clientName";
  protected static final String V1_PREF_CLIENT_NAME = V0_PREF_CLIENT_NAME;
  protected static final String V0_PREF_NUM_CLIENTS = "account.numClients";
  protected static final String V1_PREF_NUM_CLIENTS = V0_PREF_NUM_CLIENTS;

  /**
   * Extract version 0 per-Android account user data and write to version 1 per-Sync account shared prefs.
   *
   * @param accountManager Android account manager.
   * @param account Android account.
   * @param to per-Sync account version 1 shared prefs.
   * @return the number of preferences migrated.
   * @throws Exception
   */
  protected static int upgradeAndroidAccount0to1(final AccountManager accountManager, final Account account, final SharedPreferences to) throws Exception {
    final String V0_PREF_ACCOUNT_GUID = "account.guid";
    final String V1_PREF_ACCOUNT_GUID = V0_PREF_ACCOUNT_GUID;
    final String V0_PREF_CLIENT_NAME = "account.clientName";
    final String V1_PREF_CLIENT_NAME = V0_PREF_CLIENT_NAME;
    final String V0_PREF_NUM_CLIENTS = "account.numClients";
    final String V1_PREF_NUM_CLIENTS = V0_PREF_NUM_CLIENTS;

    String accountGUID = null;
    String clientName = null;
    long numClients = -1;
    try {
      accountGUID = accountManager.getUserData(account, V0_PREF_ACCOUNT_GUID);
    } catch (Exception e) {
      // Do nothing.
    }
    try {
      clientName = accountManager.getUserData(account, V0_PREF_CLIENT_NAME);
    } catch (Exception e) {
      // Do nothing.
    }
    try {
      numClients = Long.parseLong(accountManager.getUserData(account, V0_PREF_NUM_CLIENTS));
    } catch (Exception e) {
      // Do nothing.
    }

    final Editor editor = to.edit();

    int count = 0;
    if (accountGUID != null) {
      final String fromKey = V0_PREF_ACCOUNT_GUID;
      final String toKey = V1_PREF_ACCOUNT_GUID;
      if (Logger.LOG_PERSONAL_INFORMATION) {
        Logger.debug(LOG_TAG, "Migrated '" + fromKey + "' to '" + toKey + "' (" + accountGUID + ").");
      } else {
        Logger.debug(LOG_TAG, "Migrated '" + fromKey + "' to '" + toKey + "'.");
      }
      editor.putString(toKey, accountGUID);
      count += 1;
    }
    if (clientName != null) {
      final String fromKey = V0_PREF_CLIENT_NAME;
      final String toKey = V1_PREF_CLIENT_NAME;
      if (Logger.LOG_PERSONAL_INFORMATION) {
        Logger.debug(LOG_TAG, "Migrated '" + fromKey + "' to '" + toKey + "' (" + clientName + ").");
      } else {
        Logger.debug(LOG_TAG, "Migrated '" + fromKey + "' to '" + toKey + "'.");
      }
      editor.putString(toKey, clientName);
      count += 1;
    }
    if (numClients > -1) {
      final String fromKey = V0_PREF_NUM_CLIENTS;
      final String toKey = V1_PREF_NUM_CLIENTS;
      if (Logger.LOG_PERSONAL_INFORMATION) {
        Logger.debug(LOG_TAG, "Migrated '" + fromKey + "' to '" + toKey + "' (" + numClients + ").");
      } else {
        Logger.debug(LOG_TAG, "Migrated '" + fromKey + "' to '" + toKey + "'.");
      }
      editor.putLong(toKey, numClients);
      count += 1;
    }

    if (count > 0) {
      editor.commit();
    }
    return count;
  }

  /**
   * Extract version 1 per-Sync account shared prefs and write to version 0 per-Android account user data.
   *
   * @param from per-Sync account version 1 shared prefs.
   * @param accountManager Android account manager.
   * @param account Android account.
   * @return the number of preferences migrated.
   * @throws Exception
   */
  protected static int downgradeAndroidAccount1to0(final SharedPreferences from, final AccountManager accountManager, final Account account) throws Exception {
    final String accountGUID = from.getString(V1_PREF_ACCOUNT_GUID, null);
    final String clientName = from.getString(V1_PREF_CLIENT_NAME, null);
    final long numClients = from.getLong(V1_PREF_NUM_CLIENTS, -1L);

    int count = 0;
    if (accountGUID != null) {
      Logger.debug(LOG_TAG, "Migrated account GUID.");
      accountManager.setUserData(account, V0_PREF_ACCOUNT_GUID, accountGUID);
      count += 1;
    }
    if (clientName != null) {
      Logger.debug(LOG_TAG, "Migrated client name.");
      accountManager.setUserData(account, V1_PREF_CLIENT_NAME, clientName);
      count += 1;
    }
    if (numClients > -1) {
      Logger.debug(LOG_TAG, "Migrated clients count.");
      accountManager.setUserData(account, V1_PREF_NUM_CLIENTS, Long.toString(numClients));
      count += 1;
    }
    return count;
  }

  /**
   * Extract version 0 per-Android account user data and write to version 1 per-Sync account shared prefs.
   *
   * @param from per-Sync account version 0 shared prefs.
   * @param to per-Sync account version 1 shared prefs.
   * @return the number of preferences migrated.
   * @throws Exception
   */
  protected static int upgradeShared0to1(final SharedPreferences from, final SharedPreferences to) {
    final Map<String, String> map = new HashMap<String, String>();
    final String[] prefs = new String [] {
        "syncID",
        "clusterURL",
        "enabledEngineNames",

        "metaGlobalLastModified", "metaGlobalServerResponseBody",

        "crypto5KeysLastModified", "crypto5KeysServerResponseBody",

        "serverClientsTimestamp", "serverClientRecordTimestamp",

        "forms.remote", "forms.local", "forms.syncID",
        "tabs.remote", "tabs.local", "tabs.syncID",
        "passwords.remote", "passwords.local", "passwords.syncID",
        "history.remote", "history.local", "history.syncID",
        "bookmarks.remote", "bookmarks.local", "bookmarks.syncID",
    };
    for (String pref : prefs) {
      map.put(pref, pref);
    }

    Editor editor = to.edit();
    int count = copyPreferences(from, map, editor);
    if (count > 0) {
      editor.commit();
    }
    return count;
  }

  /**
   * Extract version 1 per-Sync account shared prefs and write to version 0 per-Android account user data.
   *
   * @param from per-Sync account version 1 shared prefs.
   * @param to per-Sync account version 0 shared prefs.
   * @return the number of preferences migrated.
   * @throws Exception
   */
  protected static int downgradeShared1to0(final SharedPreferences from, final SharedPreferences to) {
    // Strictly a copy, no re-naming, no deletions -- so just invert.
    return upgradeShared0to1(from, to);
  }

  public static void upgrade0to1(final Context context, final AccountManager accountManager, final Account account,
      final String product, final String username, final String serverURL, final String profile) throws Exception {

    final String GLOBAL_SHARED_PREFS = "sync.prefs.global";

    final SharedPreferences globalPrefs = context.getSharedPreferences(GLOBAL_SHARED_PREFS, Utils.SHARED_PREFERENCES_MODE);
    final SharedPreferences accountPrefs = Utils.getSharedPreferences(context, product, username, serverURL, profile, 0);
    final SharedPreferences newPrefs = Utils.getSharedPreferences(context, product, username, serverURL, profile, 1);

    upgradeGlobals0to1(globalPrefs, newPrefs);
    upgradeAndroidAccount0to1(accountManager, account, newPrefs);
    upgradeShared0to1(accountPrefs, newPrefs);
  }

  public static void downgrade1to0(final Context context, final AccountManager accountManager, final Account account,
      final String product, final String username, final String serverURL, final String profile) throws Exception {

    final String GLOBAL_SHARED_PREFS = "sync.prefs.global";

    final SharedPreferences globalPrefs = context.getSharedPreferences(GLOBAL_SHARED_PREFS, Utils.SHARED_PREFERENCES_MODE);
    final SharedPreferences accountPrefs = Utils.getSharedPreferences(context, product, username, serverURL, profile, 0);
    final SharedPreferences oldPrefs = Utils.getSharedPreferences(context, product, username, serverURL, profile, 1);

    downgradeGlobals1to0(oldPrefs, globalPrefs);
    downgradeAndroidAccount1to0(oldPrefs, accountManager, account);
    downgradeShared1to0(oldPrefs, accountPrefs);
  }

  /**
   * Migrate, if necessary, existing prefs to a certain version.
   * <p>
   * Stores current prefs version in Android shared prefs with root
   * "sync.prefs.version", which corresponds to the file
   * "sync.prefs.version.xml".
   *
   * @param desiredVersion
   *          version to finish it.
   * @param context
   * @param accountManager
   * @param account
   * @param product
   * @param username
   * @param serverURL
   * @param profile
   * @throws Exception
   */
  public static void ensurePrefsAreVersion(final long desiredVersion,
      final Context context, final AccountManager accountManager, final Account account,
      final String product, final String username, final String serverURL, final String profile) throws Exception {
    if (desiredVersion < 0 || desiredVersion > SyncConfiguration.CURRENT_PREFS_VERSION) {
      throw new IllegalArgumentException("Cannot migrate to unknown version " + desiredVersion + ".");
    }

    SharedPreferences versionPrefs = context.getSharedPreferences("sync.prefs.version", Utils.SHARED_PREFERENCES_MODE);

    // We default to 0 since clients getting this code for the first time will
    // not have "sync.prefs.version.xml" *at all*, and upgrading when all old
    // data is missing is expected to be safe.
    long currentVersion = versionPrefs.getLong(SyncConfiguration.PREF_PREFS_VERSION, 0);
    if (currentVersion == desiredVersion) {
      Logger.info(LOG_TAG, "Current version (" + currentVersion + ") is desired version; no need to migrate.");
      return;
    }

    if (currentVersion < 0 || currentVersion > SyncConfiguration.CURRENT_PREFS_VERSION) {
      throw new IllegalStateException("Cannot migrate from unknown version " + currentVersion + ".");
    }

    // Now we're down to either version 0 or version 1.
    if (currentVersion == 0 && desiredVersion == 1) {
      Logger.info(LOG_TAG, "Upgrading from version 0 to version 1.");
      upgrade0to1(context, accountManager, account, product, username, serverURL, profile);
    } else if (currentVersion == 1 && desiredVersion == 0) {
      Logger.info(LOG_TAG, "Upgrading from version 0 to version 1.");
      upgrade0to1(context, accountManager, account, product, username, serverURL, profile);
    } else {
      Logger.warn(LOG_TAG, "Don't know how to migrate from version " + currentVersion + " to " + desiredVersion + ".");
    }

    Logger.info(LOG_TAG, "Migrated from version " + currentVersion + " to version " + desiredVersion + ".");
    versionPrefs.edit().putLong(SyncConfiguration.PREF_PREFS_VERSION, desiredVersion).commit();
  }
}
