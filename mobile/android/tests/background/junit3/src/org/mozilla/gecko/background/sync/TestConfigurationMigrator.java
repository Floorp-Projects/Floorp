/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync;

import java.util.Map;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.background.testhelpers.MockSharedPreferences;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.PrefsBackoffHandler;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.SyncConstants;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.config.ConfigurationMigrator;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.sync.setup.SyncAccounts.SyncAccountParameters;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

public class TestConfigurationMigrator extends AndroidSyncTestCase {
  /**
   * A migrator that makes public certain protected static functions for testing.
   */
  protected static class PublicMigrator extends ConfigurationMigrator {
    public static int upgradeGlobals0to1(final SharedPreferences from, final SharedPreferences to) throws Exception {
      return ConfigurationMigrator.upgradeGlobals0to1(from, to);
    }

    public static int downgradeGlobals1to0(final SharedPreferences from, final SharedPreferences to) throws Exception {
      return ConfigurationMigrator.downgradeGlobals1to0(from, to);
    }

    public static int upgradeShared0to1(final SharedPreferences from, final SharedPreferences to) {
      return ConfigurationMigrator.upgradeShared0to1(from, to);
    }

    public static int downgradeShared1to0(final SharedPreferences from, final SharedPreferences to) {
      return ConfigurationMigrator.downgradeShared1to0(from, to);
    }

    public static int upgradeAndroidAccount0to1(final AccountManager accountManager, final Account account, final SharedPreferences to) throws Exception {
      return ConfigurationMigrator.upgradeAndroidAccount0to1(accountManager, account, to);
    }

    public static int downgradeAndroidAccount1to0(final SharedPreferences from, final AccountManager accountManager, final Account account) throws Exception {
      return ConfigurationMigrator.downgradeAndroidAccount1to0(from, accountManager, account);
    }
  };

  public static final String TEST_USERNAME   = "test@mozilla.com";
  public static final String TEST_SYNCKEY    = "testSyncKey";
  public static final String TEST_PASSWORD   = "testPassword";
  public static final String TEST_SERVERURL = null;
  public static final String TEST_PROFILE   = "default";
  public static final String TEST_PRODUCT   = GlobalConstants.BROWSER_INTENT_PACKAGE;

  public static final String TEST_GUID = "testGuid";
  public static final String TEST_CLIENT_NAME = "test's Nightly on test";
  public static final long TEST_NUM_CLIENTS = 2;

  protected static void putJSON(final Editor editor, final String name, final String JSON) throws Exception {
    editor.putString(name, ExtendedJSONObject.parseJSONObject(JSON).toJSONString());
  }

  /**
   * Write a complete set of unversioned account prefs suitable for testing forward migration.
   * @throws Exception
   */
  public void populateAccountSharedPrefs(final SharedPreferences to) throws Exception {
    final Editor editor = to.edit();

    putJSON(editor, "forms.remote", "{\"timestamp\":1340402010180}");
    putJSON(editor, "forms.local",  "{\"timestamp\":1340402018565}");
    editor.putString("forms.syncID", "JKAkk-wUEUpX");

    putJSON(editor, "tabs.remote", "{\"timestamp\":1340401964300}");
    putJSON(editor, "tabs.local",  "{\"timestamp\":1340401970533}");
    editor.putString("tabs.syncID", "604bXkw7dnUq");

    putJSON(editor, "passwords.remote", "{\"timestamp\":1340401965150}");
    putJSON(editor, "passwords.local",  "{\"timestamp\":1340402005243}");
    editor.putString("passwords.syncID", "VkTH0QiVj6dD");

    putJSON(editor, "history.remote", "{\"timestamp\":1340402003640}");
    putJSON(editor, "history.local",  "{\"timestamp\":1340402015381}");
    editor.putString("history.syncID", "fs1241n-JyWh");

    putJSON(editor, "bookmarks.remote", "{\"timestamp\":1340402003370}");
    putJSON(editor, "bookmarks.local",  "{\"timestamp\":1340402008397}");
    editor.putString("bookmarks.syncID", "P8gG8ERuJ4H1");

    editor.putLong("metaGlobalLastModified", 1340401961960L);
    putJSON(editor, "metaGlobalServerResponseBody", "{\"ttl\":31536000,\"id\":\"global\",\"payload\":\"{\\\"storageVersion\\\":5,\\\"syncID\\\":\\\"Z2RopSDg-0bE\\\",\\\"engines\\\":{\\\"history\\\":{\\\"syncID\\\":\\\"fs1241n-JyWh\\\",\\\"version\\\":1},\\\"bookmarks\\\":{\\\"syncID\\\":\\\"P8gG8ERuJ4H1\\\",\\\"version\\\":2},\\\"passwords\\\":{\\\"syncID\\\":\\\"VkTH0QiVj6dD\\\",\\\"version\\\":1},\\\"prefs\\\":{\\\"syncID\\\":\\\"4lESgyoYPXYI\\\",\\\"version\\\":2},\\\"addons\\\":{\\\"syncID\\\":\\\"yCkJKkH-okoS\\\",\\\"version\\\":1},\\\"forms\\\":{\\\"syncID\\\":\\\"JKAkk-wUEUpX\\\",\\\"version\\\":1},\\\"clients\\\":{\\\"syncID\\\":\\\"KfANCdkZNOFJ\\\",\\\"version\\\":1},\\\"tabs\\\":{\\\"syncID\\\":\\\"604bXkw7dnUq\\\",\\\"version\\\":1}}}\"}");

    editor.putLong("crypto5KeysLastModified", 1340401962760L);
    putJSON(editor, "crypto5KeysServerResponseBody", "{\"ttl\":31536000,\"id\":\"keys\",\"payload\":\"{\\\"ciphertext\\\":\\\"+ZH6AaMhnKOWS7OzpdMfT5X2C7AYgax5JRd2HY4BHAFNPDv8\\\\\\/TwQIJgFDuNjASo0WEujjdkFot39qeQ24RLAz4D11rG\\\\\\/FZwo8FEUB9aSfec1N6sao6KzWkSamdqiJSRjpsUKexp2it1HvwqRDEBH\\\\\\/lgue11axv51u1MAV3ZfX2fdzVIiGTqF1YJAvENZtol3pyEh2HI4FZlv+oLW250nV4w1vAfDNGLVbbjXbdR+kec=\\\",\\\"IV\\\":\\\"bHqF\\\\\\/4PshKt2GQ\\\\\\/njGj2Jw==\\\",\\\"hmac\\\":\\\"f97c20d5c0a141f62a1571a108de1bad4b854b29c8d4b2b0d36da73421e4becc\\\"}\"}");

    editor.putString("syncID", "Z2RopSDg-0bE");
    editor.putString("clusterURL", "https://scl2-sync3.services.mozilla.com/");
    putJSON(editor, "enabledEngineNames", "{\"history\":0,\"bookmarks\":0,\"passwords\":0,\"prefs\":0,\"addons\":0,\"forms\":0,\"clients\":0,\"tabs\":0}");

    editor.putLong("serverClientsTimestamp", 1340401963950L);
    editor.putLong("serverClientRecordTimestamp", 1340401963950L);

    editor.commit();
  }

  /**
   * Write a complete set of unversioned global prefs suitable for testing forward migration.
   * @throws Exception
   */
  public void populateGlobalSharedPrefs(final SharedPreferences to) throws Exception {
    final Editor editor = to.edit();

    editor.putLong("earliestnextsync", 1340402318649L);
    editor.putBoolean("clusterurlisstale", false);

    editor.commit();
  }

  /**
   * Write a complete set of unversioned Account data suitable for testing forward migration.
   * @throws Exception
   */
  public void populateAccountData(final AccountManager accountManager, final Account account) throws Exception {
    accountManager.setUserData(account, "account.guid", TEST_GUID);
    accountManager.setUserData(account, "account.clientName", TEST_CLIENT_NAME);
    accountManager.setUserData(account, "account.numClients", Long.valueOf(TEST_NUM_CLIENTS).toString());
  }

  public void testMigrateGlobals0and1() throws Exception {
    final SharedPreferences v0a = new MockSharedPreferences();
    final SharedPreferences v1a = new MockSharedPreferences();
    final SharedPreferences v0b = new MockSharedPreferences();
    final SharedPreferences v1b = new MockSharedPreferences();

    populateGlobalSharedPrefs(v0a);

    final int NUM_GLOBALS = 2;
    assertEquals(NUM_GLOBALS, v0a.getAll().size());
    assertEquals(NUM_GLOBALS, PublicMigrator.upgradeGlobals0to1(v0a, v1a));
    assertEquals(NUM_GLOBALS, PublicMigrator.downgradeGlobals1to0(v1a, v0b));
    assertEquals(NUM_GLOBALS, PublicMigrator.upgradeGlobals0to1(v0b, v1b));
    assertEquals(v0a.getAll(), v0b.getAll());
    assertEquals(v1a.getAll(), v1b.getAll());
  }

  public void testMigrateShared0and1() throws Exception {
    final SharedPreferences v0a = new MockSharedPreferences();
    final SharedPreferences v1a = new MockSharedPreferences();
    final SharedPreferences v0b = new MockSharedPreferences();
    final SharedPreferences v1b = new MockSharedPreferences();

    populateAccountSharedPrefs(v0a);

    final int NUM_GLOBALS = 24;
    assertEquals(NUM_GLOBALS, v0a.getAll().size());
    assertEquals(NUM_GLOBALS, PublicMigrator.upgradeShared0to1(v0a, v1a));
    assertEquals(NUM_GLOBALS, PublicMigrator.downgradeShared1to0(v1a, v0b));
    assertEquals(NUM_GLOBALS, PublicMigrator.upgradeShared0to1(v0b, v1b));
    assertEquals(v0a.getAll(), v0b.getAll());
    assertEquals(v1a.getAll(), v1b.getAll());
  }

  public void testMigrateAccount0and1() throws Exception {
    final Context context = getApplicationContext();
    final AccountManager accountManager = AccountManager.get(context);
    final SyncAccountParameters syncAccount = new SyncAccountParameters(context, null,
        TEST_USERNAME, TEST_SYNCKEY, TEST_PASSWORD, null);

    Account account = null;
    try {
      account = SyncAccounts.createSyncAccount(syncAccount, false);
      populateAccountData(accountManager, account);

      final int NUM_ACCOUNTS = 3;
      final SharedPreferences a = new MockSharedPreferences();
      final SharedPreferences b = new MockSharedPreferences();

      assertEquals(NUM_ACCOUNTS, PublicMigrator.upgradeAndroidAccount0to1(accountManager, account, a));
      assertEquals(NUM_ACCOUNTS, a.getAll().size());
      assertEquals(NUM_ACCOUNTS, PublicMigrator.downgradeAndroidAccount1to0(a, accountManager, account));

      TestSyncAccounts.deleteAccount(this, accountManager, account);
      account = SyncAccounts.createSyncAccount(syncAccount, false);

      assertEquals(NUM_ACCOUNTS, PublicMigrator.downgradeAndroidAccount1to0(a, accountManager, account));
      assertEquals(NUM_ACCOUNTS, PublicMigrator.upgradeAndroidAccount0to1(accountManager, account, b));
      assertEquals(a.getAll(), b.getAll());
    } finally {
      if (account != null) {
        TestSyncAccounts.deleteAccount(this, accountManager, account);
      }
    }
  }

  public void testMigrate0to1() throws Exception {
    final Context context = getApplicationContext();

    final String ACCOUNT_SHARED_PREFS_NAME = "sync.prefs.3qyu5zoqpuu4zhdiv5l2qthsiter3vop";
    final String GLOBAL_SHARED_PREFS_NAME = "sync.prefs.global";

    final String path = Utils.getPrefsPath(TEST_PRODUCT, TEST_USERNAME, TEST_SERVERURL, TEST_PROFILE, 0);
    assertEquals(ACCOUNT_SHARED_PREFS_NAME, path);
    final SharedPreferences accountPrefs = context.getSharedPreferences(ACCOUNT_SHARED_PREFS_NAME, Utils.SHARED_PREFERENCES_MODE);
    final SharedPreferences globalPrefs = context.getSharedPreferences(GLOBAL_SHARED_PREFS_NAME, Utils.SHARED_PREFERENCES_MODE);

    accountPrefs.edit().clear().commit();
    globalPrefs.edit().clear().commit();
    // Clear prefs we're about to write into.
    final SharedPreferences existingPrefs = Utils.getSharedPreferences(context, TEST_PRODUCT, TEST_USERNAME, TEST_SERVERURL, TEST_PROFILE, 1);
    existingPrefs.edit().clear().commit();

    final AccountManager accountManager = AccountManager.get(context);
    final SyncAccountParameters syncAccount = new SyncAccountParameters(context, null,
        TEST_USERNAME, TEST_SYNCKEY, TEST_PASSWORD, null);

    Account account = null;
    try {
      account = SyncAccounts.createSyncAccount(syncAccount, false); // Wipes prefs.

      populateAccountSharedPrefs(accountPrefs);
      populateGlobalSharedPrefs(globalPrefs);
      populateAccountData(accountManager, account);

      ConfigurationMigrator.upgrade0to1(context, accountManager, account, TEST_PRODUCT, TEST_USERNAME, TEST_SERVERURL, TEST_PROFILE);
    } finally {
      if (account != null) {
        TestSyncAccounts.deleteAccount(this, accountManager, account);
      }
    }

    Map<String, ?> origAccountPrefs = accountPrefs.getAll();
    Map<String, ?> origGlobalPrefs = globalPrefs.getAll();
    assertFalse(origAccountPrefs.isEmpty());
    assertFalse(origGlobalPrefs.isEmpty());

    final SharedPreferences newPrefs = Utils.getSharedPreferences(context, TEST_PRODUCT, TEST_USERNAME, TEST_SERVERURL, TEST_PROFILE, 1);

    // Some global stuff.
    assertEquals(false, newPrefs.getBoolean(SyncConfiguration.PREF_CLUSTER_URL_IS_STALE, true));
    assertEquals(1340402318649L, newPrefs.getLong(PrefsBackoffHandler.PREF_EARLIEST_NEXT + SyncConstants.BACKOFF_PREF_SUFFIX_11, 111));
    // Some per-Sync account stuff.
    assertEquals("{\"timestamp\":1340402003370}", newPrefs.getString("bookmarks.remote", null));
    assertEquals("{\"timestamp\":1340402008397}", newPrefs.getString("bookmarks.local", null));
    assertEquals("P8gG8ERuJ4H1", newPrefs.getString("bookmarks.syncID", null));
    assertEquals(1340401961960L, newPrefs.getLong("metaGlobalLastModified", 0));
    // Some per-Android account stuff.
    assertEquals(TEST_GUID, newPrefs.getString(SyncConfiguration.PREF_ACCOUNT_GUID, null));
    assertEquals(TEST_CLIENT_NAME, newPrefs.getString(SyncConfiguration.PREF_CLIENT_NAME, null));
    assertEquals(TEST_NUM_CLIENTS, newPrefs.getLong(SyncConfiguration.PREF_NUM_CLIENTS, -1L));

    // Now try to downgrade.
    accountPrefs.edit().clear().commit();
    globalPrefs.edit().clear().commit();

    account = null;
    try {
      account = SyncAccounts.createSyncAccount(syncAccount, false);

      ConfigurationMigrator.downgrade1to0(context, accountManager, account, TEST_PRODUCT, TEST_USERNAME, TEST_SERVERURL, TEST_PROFILE);

      final String V0_PREF_ACCOUNT_GUID = "account.guid";
      final String V0_PREF_CLIENT_NAME = "account.clientName";
      final String V0_PREF_NUM_CLIENTS = "account.numClients";

      assertEquals(TEST_GUID, accountManager.getUserData(account, V0_PREF_ACCOUNT_GUID));
      assertEquals(TEST_CLIENT_NAME, accountManager.getUserData(account, V0_PREF_CLIENT_NAME));
      assertEquals(Long.valueOf(TEST_NUM_CLIENTS).toString(), accountManager.getUserData(account, V0_PREF_NUM_CLIENTS));
    } finally {
      if (account != null) {
        TestSyncAccounts.deleteAccount(this, accountManager, account);
      }
    }

    // Check re-constituted prefs against old prefs.
    assertEquals(origAccountPrefs, accountPrefs.getAll());
    assertEquals(origGlobalPrefs, globalPrefs.getAll());
  }
}
