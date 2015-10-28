/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.fxa.authenticator;

import org.mozilla.gecko.background.sync.AndroidSyncTestCaseWithAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AccountPickler;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Separated;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.test.RenamingDelegatingContext;

public class TestAccountPickler extends AndroidSyncTestCaseWithAccounts {
  private static final String TEST_TOKEN_SERVER_URI = "tokenServerURI";
  private static final String TEST_PROFILE_SERVER_URI = "profileServerURI";
  private static final String TEST_AUTH_SERVER_URI = "serverURI";
  private static final String TEST_PROFILE = "profile";
  private final static String FILENAME_PREFIX = "TestAccountPickler-";
  private final static String PICKLE_FILENAME = "pickle";

  private final static String TEST_ACCOUNTTYPE = FxAccountConstants.ACCOUNT_TYPE;

  // Test account names must start with TEST_USERNAME in order to be recognized
  // as test accounts and deleted in tearDown.
  public static final String TEST_USERNAME   = "testFirefoxAccount@mozilla.com";

  public Account account;
  public RenamingDelegatingContext context;

  public TestAccountPickler() {
    super(TEST_ACCOUNTTYPE, TEST_USERNAME);
  }

  @Override
  public void setUp() {
    super.setUp();
    this.account = null;
    // Randomize the filename prefix in case we don't clean up correctly.
    this.context = new RenamingDelegatingContext(getApplicationContext(), FILENAME_PREFIX +
        Math.random() * 1000001 + "-");
    this.accountManager = AccountManager.get(context);
  }

  @Override
  public void tearDown() {
    super.tearDown();
    this.context.deleteFile(PICKLE_FILENAME);
  }

  public AndroidFxAccount addTestAccount() throws Exception {
    final State state = new Separated(TEST_USERNAME, "uid", false); // State choice is arbitrary.
    final AndroidFxAccount account = AndroidFxAccount.addAndroidAccount(context, TEST_USERNAME,
        TEST_PROFILE, TEST_AUTH_SERVER_URI, TEST_TOKEN_SERVER_URI, TEST_PROFILE_SERVER_URI, state,
        AndroidSyncTestCaseWithAccounts.TEST_SYNC_AUTOMATICALLY_MAP_WITH_ALL_AUTHORITIES_DISABLED);
    assertNotNull(account);
    assertNotNull(account.getProfile());
    assertTrue(testAccountsExist()); // Sanity check.
    this.account = account.getAndroidAccount(); // To remove in tearDown() if we throw.
    return account;
  }

  public void testPickle() throws Exception {
    final AndroidFxAccount account = addTestAccount();

    final long now = System.currentTimeMillis();
    final ExtendedJSONObject o = AccountPickler.toJSON(account, now);
    assertNotNull(o.toJSONString());

    assertEquals(3, o.getLong(AccountPickler.KEY_PICKLE_VERSION).longValue());
    assertTrue(o.getLong(AccountPickler.KEY_PICKLE_TIMESTAMP).longValue() < System.currentTimeMillis());

    assertEquals(AndroidFxAccount.CURRENT_ACCOUNT_VERSION, o.getIntegerSafely(AccountPickler.KEY_ACCOUNT_VERSION).intValue());
    assertEquals(FxAccountConstants.ACCOUNT_TYPE, o.getString(AccountPickler.KEY_ACCOUNT_TYPE));

    assertEquals(TEST_USERNAME, o.getString(AccountPickler.KEY_EMAIL));
    assertEquals(TEST_PROFILE, o.getString(AccountPickler.KEY_PROFILE));
    assertEquals(TEST_AUTH_SERVER_URI, o.getString(AccountPickler.KEY_IDP_SERVER_URI));
    assertEquals(TEST_TOKEN_SERVER_URI, o.getString(AccountPickler.KEY_TOKEN_SERVER_URI));
    assertEquals(TEST_PROFILE_SERVER_URI, o.getString(AccountPickler.KEY_PROFILE_SERVER_URI));

    assertNotNull(o.getObject(AccountPickler.KEY_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP));
    assertNotNull(o.get(AccountPickler.KEY_BUNDLE));
  }

  public void testPickleAndUnpickle() throws Exception {
    final AndroidFxAccount inputAccount = addTestAccount();

    AccountPickler.pickle(inputAccount, PICKLE_FILENAME);
    final ExtendedJSONObject inputJSON = AccountPickler.toJSON(inputAccount, 0);
    final State inputState = inputAccount.getState();
    assertNotNull(inputJSON);
    assertNotNull(inputState);

    // unpickle adds an account to the AccountManager so delete it first.
    deleteTestAccounts();
    assertFalse(testAccountsExist());

    final AndroidFxAccount unpickledAccount = AccountPickler.unpickle(context, PICKLE_FILENAME);
    assertNotNull(unpickledAccount);
    final ExtendedJSONObject unpickledJSON = AccountPickler.toJSON(unpickledAccount, 0);
    final State unpickledState = unpickledAccount.getState();
    assertNotNull(unpickledJSON);
    assertNotNull(unpickledState);

    assertEquals(inputJSON, unpickledJSON);
    assertStateEquals(inputState, unpickledState);
  }

  public void testDeletePickle() throws Exception {
    final AndroidFxAccount account = addTestAccount();
    AccountPickler.pickle(account, PICKLE_FILENAME);

    final String s = Utils.readFile(context, PICKLE_FILENAME);
    assertNotNull(s);
    assertTrue(s.length() > 0);

    AccountPickler.deletePickle(context, PICKLE_FILENAME);
    assertFileNotPresent(context, PICKLE_FILENAME);
  }

  private void assertStateEquals(final State expected, final State actual) throws Exception {
    // TODO: Write and use State.equals. Thus, this is only thorough for the State base class.
    assertEquals(expected.getStateLabel(), actual.getStateLabel());
    assertEquals(expected.email, actual.email);
    assertEquals(expected.uid, actual.uid);
    assertEquals(expected.verified, actual.verified);
  }
}
