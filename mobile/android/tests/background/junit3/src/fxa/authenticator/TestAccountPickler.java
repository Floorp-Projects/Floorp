/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.fxa.authenticator;

import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AccountPickler;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Separated;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.background.sync.TestSyncAccounts;
import org.mozilla.gecko.sync.Utils;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.test.InstrumentationTestCase;
import android.test.RenamingDelegatingContext;

public class TestAccountPickler extends AndroidSyncTestCase {
  private final static String FILENAME_PREFIX = "TestAccountPickler-";
  private final static String PICKLE_FILENAME = "pickle";

  public Account account;
  public RenamingDelegatingContext context;
  public AccountManager accountManager;

  @Override
  public void setUp() {
    this.account = null;
    // Randomize the filename prefix in case we don't clean up correctly.
    this.context = new RenamingDelegatingContext(getApplicationContext(), FILENAME_PREFIX +
        Math.random() * 1000001 + "-");
    this.accountManager = AccountManager.get(context);
  }

  public void tearDown() {
    if (this.account != null) {
      deleteAccount(this, this.accountManager, this.account);
      this.account = null;
    }
    this.context.deleteFile(PICKLE_FILENAME);
  }

  public static void deleteAccount(final InstrumentationTestCase test,
      final AccountManager accountManager, final Account account) {
    TestSyncAccounts.deleteAccount(test, accountManager, account);
  }

  private boolean accountsExist() {
    // Note that we don't use FirefoxAccounts.firefoxAccountsExist because it unpickles.
    return AccountManager.get(context).getAccountsByType(FxAccountConstants.ACCOUNT_TYPE).length > 0;
  }

  public AndroidFxAccount addDummyAccount() throws Exception {
    final String email = "iu@fakedomain.io";
    final State state = new Separated(email, "uid", false); // State choice is arbitrary.
    final AndroidFxAccount account = AndroidFxAccount.addAndroidAccount(context, email,
        "profile", "serverURI", "tokenServerURI", state);
    assertNotNull(account);
    assertTrue(accountsExist()); // Sanity check.
    this.account = account.getAndroidAccount(); // To remove in tearDown() if we throw.
    return account;
  }

  public void testPickleAndUnpickle() throws Exception {
    final AndroidFxAccount inputAccount = addDummyAccount();
    // Sync is enabled by default so we do a more thorough test by disabling it.
    inputAccount.disableSyncing();

    AccountPickler.pickle(inputAccount, PICKLE_FILENAME);

    // unpickle adds an account to the AccountManager so delete it first.
    deleteAccount(this, this.accountManager, inputAccount.getAndroidAccount());
    assertFalse(accountsExist());

    final AndroidFxAccount unpickledAccount =
        AccountPickler.unpickle(context, PICKLE_FILENAME);
    assertNotNull(unpickledAccount);
    this.account = unpickledAccount.getAndroidAccount(); // To remove in tearDown().
    assertAccountsEquals(inputAccount, unpickledAccount);
  }

  public void testDeletePickle() throws Exception {
    final AndroidFxAccount account = addDummyAccount();
    AccountPickler.pickle(account, PICKLE_FILENAME);

    final String s = Utils.readFile(context, PICKLE_FILENAME);
    assertNotNull(s);
    assertTrue(s.length() > 0);

    AccountPickler.deletePickle(context, PICKLE_FILENAME);
    org.mozilla.gecko.background.sync.TestAccountPickler.assertFileNotPresent(
        context, PICKLE_FILENAME);
  }

  private void assertAccountsEquals(final AndroidFxAccount expected,
      final AndroidFxAccount actual) throws Exception {
    // TODO: Write and use AndroidFxAccount.equals
    // TODO: protected.
    //assertEquals(expected.getAccountVersion(), actual.getAccountVersion());
    assertEquals(expected.getProfile(), actual.getProfile());
    assertEquals(expected.getAccountServerURI(), actual.getAccountServerURI());
    assertEquals(expected.getAudience(), actual.getAudience());
    assertEquals(expected.getTokenServerURI(), actual.getTokenServerURI());
    assertEquals(expected.getSyncPrefsPath(), actual.getSyncPrefsPath());
    assertEquals(expected.isSyncingEnabled(), actual.isSyncingEnabled());
    assertEquals(expected.getEmail(), actual.getEmail());
    assertStateEquals(expected.getState(), actual.getState());
  }

  private void assertStateEquals(final State expected, final State actual) throws Exception {
    // TODO: Write and use State.equals. Thus, this is only thorough for the State base class.
    assertEquals(expected.getStateLabel(), actual.getStateLabel());
    assertEquals(expected.email, actual.email);
    assertEquals(expected.uid, actual.uid);
    assertEquals(expected.verified, actual.verified);
  }
}
