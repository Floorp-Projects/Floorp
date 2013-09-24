/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync;

import java.io.UnsupportedEncodingException;
import java.security.NoSuchAlgorithmException;
import java.util.concurrent.TimeUnit;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.SyncConstants;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.sync.setup.SyncAccounts.SyncAccountParameters;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.test.InstrumentationTestCase;

/**
 * We can use <code>performWait</code> and <code>performNotify</code> here if we
 * are careful about threading issues with <code>AsyncTask</code>. We need to
 * take some care to both create and run certain tasks on the main thread --
 * moving the object allocation out of the UI thread causes failures!
 * <p>
 * @see "<a href='http://stackoverflow.com/questions/2321829/android-asynctask-testing-problem-with-android-test-framework'>
 * http://stackoverflow.com/questions/2321829/android-asynctask-testing-problem-with-android-test-framework</a>."
 */
public class TestSyncAccounts extends AndroidSyncTestCase {
  private static final String TEST_USERNAME   = "testAccount@mozilla.com";
  private static final String TEST_SYNCKEY    = "testSyncKey";
  private static final String TEST_PASSWORD   = "testPassword";
  private static final String TEST_SERVERURL  = "test.server.url/";
  private static final String TEST_CLUSTERURL = "test.cluster.url/";

  public static final String TEST_ACCOUNTTYPE = SyncConstants.ACCOUNTTYPE_SYNC;

  public static final String TEST_PRODUCT = GlobalConstants.BROWSER_INTENT_PACKAGE;
  public static final String TEST_PROFILE = Constants.DEFAULT_PROFILE;
  public static final long TEST_VERSION = SyncConfiguration.CURRENT_PREFS_VERSION;

  public static final String TEST_PREFERENCE = "testPreference";
  public static final String TEST_SYNC_ID = "testSyncID";

  private Account account;
  private Context context;
  private AccountManager accountManager;
  private SyncAccountParameters syncAccount;

  public void setUp() {
    account = null;
    context = getApplicationContext();
    accountManager = AccountManager.get(context);
    syncAccount = new SyncAccountParameters(context, null,
        TEST_USERNAME, TEST_SYNCKEY, TEST_PASSWORD, null);
  }

  public static void deleteAccount(final InstrumentationTestCase test, final AccountManager accountManager, final Account account) {
    performWait(new Runnable() {
      @Override
      public void run() {
        try {
          test.runTestOnUiThread(new Runnable() {
            final AccountManagerCallback<Boolean> callback = new AccountManagerCallback<Boolean>() {
              @Override
              public void run(AccountManagerFuture<Boolean> future) {
                try {
                  future.getResult(5L, TimeUnit.SECONDS);
                } catch (Exception e) {
                }
                performNotify();
              }
            };

            @Override
            public void run() {
              accountManager.removeAccount(account, callback, null);
            }
          });
        } catch (Throwable e) {
          performNotify(e);
        }
      }
    });
  }

  public void tearDown() {
    if (account == null) {
      return;
    }
    deleteAccount(this, accountManager, account);
    account = null;
  }

  public void testSyncAccountParameters() {
    assertEquals(TEST_USERNAME, syncAccount.username);
    assertNull(syncAccount.accountManager);
    assertNull(syncAccount.serverURL);

    try {
      syncAccount = new SyncAccountParameters(context, null,
          null, TEST_SYNCKEY, TEST_PASSWORD, TEST_SERVERURL);
    } catch (IllegalArgumentException e) {
      return;
    } catch (Exception e) {
      fail("Did not expect exception: " + e);
    }
    fail("Expected IllegalArgumentException.");
  }

  public void testCreateAccount() {
    int before = accountManager.getAccountsByType(TEST_ACCOUNTTYPE).length;
    account = SyncAccounts.createSyncAccount(syncAccount, false);
    int afterCreate = accountManager.getAccountsByType(TEST_ACCOUNTTYPE).length;
    assertTrue(afterCreate > before);
    deleteAccount(this, accountManager, account);
    account = null;
    int afterDelete = accountManager.getAccountsByType(TEST_ACCOUNTTYPE).length;
    assertEquals(before, afterDelete);
  }

  public void testCreateSecondAccount() {
    int before = accountManager.getAccountsByType(TEST_ACCOUNTTYPE).length;
    account = SyncAccounts.createSyncAccount(syncAccount, false);
    int afterFirst = accountManager.getAccountsByType(TEST_ACCOUNTTYPE).length;
    assertTrue(afterFirst > before);

    SyncAccountParameters secondSyncAccount = new SyncAccountParameters(context, null,
        "second@username.com", TEST_SYNCKEY, TEST_PASSWORD, null);

    Account second = SyncAccounts.createSyncAccount(secondSyncAccount, false);
    assertNotNull(second);
    int afterSecond = accountManager.getAccountsByType(TEST_ACCOUNTTYPE).length;
    assertTrue(afterSecond > afterFirst);

    deleteAccount(this, accountManager, second);
    deleteAccount(this, accountManager, account);
    account = null;

    int afterDelete = accountManager.getAccountsByType(TEST_ACCOUNTTYPE).length;
    assertEquals(before, afterDelete);
  }

  public void testCreateDuplicateAccount() {
    int before = accountManager.getAccountsByType(TEST_ACCOUNTTYPE).length;
    account = SyncAccounts.createSyncAccount(syncAccount, false);
    int afterCreate = accountManager.getAccountsByType(TEST_ACCOUNTTYPE).length;
    assertTrue(afterCreate > before);

    Account dupe = SyncAccounts.createSyncAccount(syncAccount, false);
    assertNull(dupe);
  }

  public void testClientRecord() throws NoSuchAlgorithmException, UnsupportedEncodingException {
    final String TEST_NAME = "testName";
    final String TEST_GUID = "testGuid";
    syncAccount = new SyncAccountParameters(context, null,
        TEST_USERNAME, TEST_SYNCKEY, TEST_PASSWORD, null, null, TEST_NAME, TEST_GUID);
    account = SyncAccounts.createSyncAccount(syncAccount, false);
    assertNotNull(account);

    SharedPreferences prefs = Utils.getSharedPreferences(context, TEST_PRODUCT, TEST_USERNAME,
        SyncAccounts.DEFAULT_SERVER, TEST_PROFILE, TEST_VERSION);

    // Verify that client record is set.
    assertEquals(TEST_GUID, prefs.getString(SyncConfiguration.PREF_ACCOUNT_GUID, null));
    assertEquals(TEST_NAME, prefs.getString(SyncConfiguration.PREF_CLIENT_NAME, null));

    // Let's verify that clusterURL is correctly not set.
    String clusterURL = prefs.getString(SyncConfiguration.PREF_CLUSTER_URL, null);
    assertNull(clusterURL);
  }

  public void testClusterURL() throws NoSuchAlgorithmException, UnsupportedEncodingException {
    syncAccount = new SyncAccountParameters(context, null,
        TEST_USERNAME, TEST_SYNCKEY, TEST_PASSWORD, TEST_SERVERURL, TEST_CLUSTERURL, null, null);
    account = SyncAccounts.createSyncAccount(syncAccount, false);
    assertNotNull(account);

    SharedPreferences prefs = Utils.getSharedPreferences(context, TEST_PRODUCT, TEST_USERNAME,
        TEST_SERVERURL, TEST_PROFILE, TEST_VERSION);
    String clusterURL = prefs.getString(SyncConfiguration.PREF_CLUSTER_URL, null);
    assertNotNull(clusterURL);
    assertEquals(TEST_CLUSTERURL, clusterURL);

    // Let's verify that client name and GUID are not set.
    assertNull(prefs.getString(SyncConfiguration.PREF_ACCOUNT_GUID, null));
    assertNull(prefs.getString(SyncConfiguration.PREF_CLIENT_NAME, null));
  }

  /**
   * Verify that creating an account wipes stale settings in Shared Preferences.
   */
  public void testCreatingWipesSharedPrefs() throws Exception {
    final String TEST_PREFERENCE = "testPreference";
    final String TEST_SYNC_ID = "testSyncID";

    SharedPreferences prefs = Utils.getSharedPreferences(context, TEST_PRODUCT, TEST_USERNAME,
        TEST_SERVERURL, TEST_PROFILE, TEST_VERSION);
    prefs.edit().putString(SyncConfiguration.PREF_SYNC_ID, TEST_SYNC_ID).commit();
    prefs.edit().putString(TEST_PREFERENCE, TEST_SYNC_ID).commit();

    syncAccount = new SyncAccountParameters(context, null,
        TEST_USERNAME, TEST_SYNCKEY, TEST_PASSWORD, TEST_SERVERURL);
    account = SyncAccounts.createSyncAccount(syncAccount, false);
    assertNotNull(account);

    // All values deleted (known and unknown).
    assertNull(prefs.getString(SyncConfiguration.PREF_SYNC_ID, null));
    assertNull(prefs.getString(TEST_SYNC_ID, null));
  }

  /**
   * Verify that creating an account preserves settings in Shared Preferences when asked.
   */
  public void testCreateSyncAccountWithExistingPreferences() throws Exception {

    SharedPreferences prefs = Utils.getSharedPreferences(context, TEST_PRODUCT, TEST_USERNAME,
        TEST_SERVERURL, TEST_PROFILE, TEST_VERSION);

    prefs.edit().putString(SyncConfiguration.PREF_SYNC_ID, TEST_SYNC_ID).commit();
    prefs.edit().putString(TEST_PREFERENCE, TEST_SYNC_ID).commit();

    assertNotNull(prefs.getString(TEST_PREFERENCE, null));
    assertNotNull(prefs.getString(SyncConfiguration.PREF_SYNC_ID, null));

    syncAccount = new SyncAccountParameters(context, null,
        TEST_USERNAME, TEST_SYNCKEY, TEST_PASSWORD, TEST_SERVERURL);
    account = SyncAccounts.createSyncAccountPreservingExistingPreferences(syncAccount, false);
    assertNotNull(account);

    // All values remain (known and unknown).
    assertNotNull(prefs.getString(TEST_PREFERENCE, null));
    assertNotNull(prefs.getString(SyncConfiguration.PREF_SYNC_ID, null));
  }

  protected void assertParams(final SyncAccountParameters params) throws Exception {
    assertNotNull(params);
    assertEquals(context, params.context);
    assertEquals(Utils.usernameFromAccount(TEST_USERNAME), params.username);
    assertEquals(TEST_PASSWORD, params.password);
    assertEquals(TEST_SERVERURL, params.serverURL);
    assertEquals(TEST_SYNCKEY, params.syncKey);
  }

  public void testBlockingFromAndroidAccountV0() throws Throwable {
    syncAccount = new SyncAccountParameters(context, null,
        TEST_USERNAME, TEST_SYNCKEY, TEST_PASSWORD, TEST_SERVERURL, TEST_CLUSTERURL, null, null);
    try {
      account = SyncAccounts.createSyncAccount(syncAccount);
      assertNotNull(account);

      // Test fetching parameters multiple times. Historically, we needed to
      // invalidate this token type every fetch; now we don't, but we'd like
      // to ensure multiple fetches work.
      SyncAccountParameters params = SyncAccounts.blockingFromAndroidAccountV0(context, accountManager, account);
      assertParams(params);

      params = SyncAccounts.blockingFromAndroidAccountV0(context, accountManager, account);
      assertParams(params);

      // Test this works on the main thread.
      this.runTestOnUiThread(new Runnable() {
        @Override
        public void run() {
          SyncAccountParameters params;
          try {
            params = SyncAccounts.blockingFromAndroidAccountV0(context, accountManager, account);
            assertParams(params);
          } catch (Exception e) {
            fail("Fetching Sync account parameters failed on UI thread.");
          }
        }
      });
    } finally {
      if (account != null) {
        deleteAccount(this, accountManager, account);
        account = null;
      }
    }
  }

  public void testMakeSyncAccountDeletedIntent() throws Throwable {
    syncAccount = new SyncAccountParameters(context, null,
        TEST_USERNAME, TEST_SYNCKEY, TEST_PASSWORD, TEST_SERVERURL, TEST_CLUSTERURL, null, null);
    try {
      account = SyncAccounts.createSyncAccount(syncAccount);
      assertNotNull(account);

      Intent intent = SyncAccounts.makeSyncAccountDeletedIntent(context, accountManager, account);
      assertEquals(SyncConstants.SYNC_ACCOUNT_DELETED_ACTION, intent.getAction());
      assertEquals(SyncConstants.SYNC_ACCOUNT_DELETED_INTENT_VERSION, intent.getLongExtra(Constants.JSON_KEY_VERSION, 0));
      assertEquals(TEST_USERNAME, intent.getStringExtra(Constants.JSON_KEY_ACCOUNT));
      assertTrue(Math.abs(intent.getLongExtra(Constants.JSON_KEY_TIMESTAMP, 0) - System.currentTimeMillis()) < 1000);

      String payload = intent.getStringExtra(Constants.JSON_KEY_PAYLOAD);
      assertNotNull(payload);

      SyncAccountParameters params = new SyncAccountParameters(context, accountManager, ExtendedJSONObject.parseJSONObject(payload));
      // Can't use assertParams because Sync key is deleted.
      assertNotNull(params);
      assertEquals(context, params.context);
      assertEquals(Utils.usernameFromAccount(TEST_USERNAME), params.username);
      assertEquals(TEST_PASSWORD, params.password);
      assertEquals(TEST_SERVERURL, params.serverURL);
      assertEquals("", params.syncKey);
    } finally {
      if (account != null) {
        deleteAccount(this, accountManager, account);
        account = null;
      }
    }
  }

  public void testBlockingPrefsFromAndroidAccountV0() throws Exception {
    // Create test account with prefs. We use a different username to avoid a
    // timing issue, where the delayed clean-up of the account created by the
    // previous test deletes the preferences for this account.
    SharedPreferences prefs = Utils.getSharedPreferences(context, TEST_PRODUCT,
        TEST_USERNAME + "2", TEST_SERVERURL, TEST_PROFILE, TEST_VERSION);
    prefs.edit().putString(TEST_PREFERENCE, TEST_SYNC_ID).commit();

    syncAccount = new SyncAccountParameters(context, null,
      TEST_USERNAME + "2", TEST_SYNCKEY, TEST_PASSWORD, TEST_SERVERURL);
    account = SyncAccounts.createSyncAccountPreservingExistingPreferences(syncAccount, false);
    assertNotNull(account);

    // Fetch account and check prefs.
    SharedPreferences sharedPreferences = SyncAccounts.blockingPrefsFromAndroidAccountV0(context, accountManager,
        account, TEST_PRODUCT, TEST_PROFILE, TEST_VERSION);
    assertNotNull(sharedPreferences);
    assertEquals(TEST_SYNC_ID, sharedPreferences.getString(TEST_PREFERENCE, null));
  }
}
