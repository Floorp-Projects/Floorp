/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;

public class AndroidSyncTestCaseWithAccounts extends AndroidSyncTestCase {
  public final String testAccountType;
  public final String testAccountPrefix;

  protected Context context;
  protected AccountManager accountManager;
  protected int numAccounts;

  public AndroidSyncTestCaseWithAccounts(String accountType, String accountPrefix) {
    super();
    this.testAccountType = accountType;
    this.testAccountPrefix = accountPrefix;
  }

  @Override
  public void setUp() {
    context = getApplicationContext();
    accountManager = AccountManager.get(context);
    deleteTestAccounts(); // Always start with no test accounts.
    numAccounts = accountManager.getAccountsByType(testAccountType).length;
  }

  public List<Account> getTestAccounts() {
    final List<Account> testAccounts = new ArrayList<Account>();

    final Account[] accounts = accountManager.getAccountsByType(testAccountType);
    for (Account account : accounts) {
      if (account.name.startsWith(testAccountPrefix)) {
        testAccounts.add(account);
      }
    }

    return testAccounts;
  }

  public void deleteTestAccounts() {
    for (Account account : getTestAccounts()) {
      TestSyncAccounts.deleteAccount(this, accountManager, account);
    }
  }

  public boolean testAccountsExist() {
    // Note that we don't use FirefoxAccounts.firefoxAccountsExist because it unpickles.
    return !getTestAccounts().isEmpty();
  }

  @Override
  public void tearDown() {
    deleteTestAccounts();
    assertEquals(numAccounts, accountManager.getAccountsByType(testAccountType).length);
  }

  public static void assertFileNotPresent(final Context context, final String filename) throws Exception {
    // Verify file is not present.
    FileInputStream fis = null;
    try {
      fis = context.openFileInput(filename);
      fail("Should get FileNotFoundException.");
    } catch (FileNotFoundException e) {
      // Do nothing; file should not exist.
    } finally {
      if (fis != null) {
        fis.close();
      }
    }
  }
}
