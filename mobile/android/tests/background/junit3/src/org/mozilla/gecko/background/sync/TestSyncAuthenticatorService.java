/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync;

import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.sync.SyncConstants;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.sync.setup.SyncAccounts.SyncAccountParameters;
import org.mozilla.gecko.sync.setup.SyncAuthenticatorService;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.os.Bundle;

public class TestSyncAuthenticatorService extends AndroidSyncTestCase {
  private static final String TEST_USERNAME   = "testAccount@mozilla.com";
  private static final String TEST_SYNCKEY    = "testSyncKey";
  private static final String TEST_PASSWORD   = "testPassword";
  private static final String TEST_SERVERURL  = "test.server.url/";

  private Account account;
  private Context context;
  private AccountManager accountManager;
  private SyncAccountParameters syncAccount;

  public void setUp() {
    account = null;
    context = getApplicationContext();
    accountManager = AccountManager.get(context);
    syncAccount = new SyncAccountParameters(context, accountManager,
        TEST_USERNAME, TEST_SYNCKEY, TEST_PASSWORD, TEST_SERVERURL);
  }

  public void tearDown() {
    if (account == null) {
      return;
    }
    TestSyncAccounts.deleteAccount(this, accountManager, account);
    account = null;
  }

  public void testGetPlainAuthToken() throws Exception {
    account = SyncAccounts.createSyncAccount(syncAccount, false);
    assertNotNull(account);

    final Bundle bundle = SyncAuthenticatorService.getPlainAuthToken(context, account);

    assertEquals(TEST_USERNAME, bundle.getString(AccountManager.KEY_ACCOUNT_NAME));
    assertEquals(SyncConstants.ACCOUNTTYPE_SYNC, bundle.getString(AccountManager.KEY_ACCOUNT_TYPE));
    assertEquals(Utils.usernameFromAccount(TEST_USERNAME), bundle.getString(Constants.OPTION_USERNAME));
    assertEquals(TEST_PASSWORD, bundle.getString(AccountManager.KEY_AUTHTOKEN));
    assertEquals(TEST_SYNCKEY, bundle.getString(Constants.OPTION_SYNCKEY));
    assertEquals(TEST_SERVERURL, bundle.getString(Constants.OPTION_SERVER));
  }

  public void testGetBadTokenType() throws Exception {
    account = SyncAccounts.createSyncAccount(syncAccount, false);
    assertNotNull(account);

    assertNull(accountManager.blockingGetAuthToken(account, "BAD_TOKEN_TYPE", false));
  }
}
