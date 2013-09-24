/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URI;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.background.testhelpers.MockGlobalSession;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SyncConfigurationException;
import org.mozilla.gecko.sync.SyncConstants;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.GlobalSessionCallback;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.sync.setup.SyncAccounts.SyncAccountParameters;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;
import org.mozilla.gecko.sync.syncadapter.SyncAdapter;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.ContentResolver;
import android.content.Context;
import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.entity.StringEntity;
import ch.boye.httpclientandroidlib.message.BasicHttpResponse;

/**
 * When syncing and a server responds with a 400 "Upgrade Required," Sync
 * accounts should be disabled.
 *
 * (We are not testing for package updating, because MY_PACKAGE_REPLACED
 * broadcasts can only be sent by the system. Testing for package replacement
 * needs to be done manually on a device.)
 *
 * @author liuche
 *
 */
public class TestUpgradeRequired extends AndroidSyncTestCase {
  private final String  TEST_SERVER      = "http://test.ser.ver/";

  private static final String TEST_USERNAME     = "user1";
  private static final String TEST_PASSWORD     = "pass1";
  private static final String TEST_SYNC_KEY     = "abcdeabcdeabcdeabcdeabcdea";

  private Context context;

  public static boolean syncsAutomatically(Account a) {
    return ContentResolver.getSyncAutomatically(a, BrowserContract.AUTHORITY);
  }

  public static boolean isSyncable(Account a) {
    return 1 == ContentResolver.getIsSyncable(a, BrowserContract.AUTHORITY);
  }

  public static boolean willEnableOnUpgrade(Account a, AccountManager accountManager) {
    return "1".equals(accountManager.getUserData(a, Constants.DATA_ENABLE_ON_UPGRADE));
  }

  private static Account getTestAccount(AccountManager accountManager) {
    final String type = SyncConstants.ACCOUNTTYPE_SYNC;
    Account[] existing = accountManager.getAccountsByType(type);
    for (Account account : existing) {
      if (account.name.equals(TEST_USERNAME)) {
        return account;
      }
    }
    return null;
  }

  private void deleteTestAccount() {
    final AccountManager accountManager = AccountManager.get(context);
    final Account found = getTestAccount(accountManager);
    if (found == null) {
      return;
    }
    TestSyncAccounts.deleteAccount(this, accountManager, found);
  }

  @Override
  public void setUp() {
    context = getApplicationContext();
    final AccountManager accountManager = AccountManager.get(context);

    deleteTestAccount();

    // Set up and enable Sync accounts.
    SyncAccountParameters syncAccountParams = new SyncAccountParameters(context, accountManager, TEST_USERNAME, TEST_PASSWORD, TEST_SYNC_KEY, TEST_SERVER, null, null, null);
    final Account account = SyncAccounts.createSyncAccount(syncAccountParams, true);
    assertNotNull(account);
    assertTrue(syncsAutomatically(account));
    assertTrue(isSyncable(account));
  }

  private static class LeakySyncAdapter extends SyncAdapter {
    public LeakySyncAdapter(Context context, boolean autoInitialize, Account account) {
      super(context, autoInitialize);
      this.localAccount = account;
    }
  }

  /**
   * Verify that when SyncAdapter is informed of an Upgrade Required
   * response, that it disables the account it's syncing.
   */
  public void testInformUpgradeRequired() {
    final AccountManager accountManager = AccountManager.get(context);
    final Account account = getTestAccount(accountManager);

    assertNotNull(account);
    assertTrue(syncsAutomatically(account));
    assertTrue(isSyncable(account));
    assertFalse(willEnableOnUpgrade(account, accountManager));

    LeakySyncAdapter adapter = new LeakySyncAdapter(context, true, account);
    adapter.informUpgradeRequiredResponse(null);

    // Oh god.
    try {
      Thread.sleep(1000);
    } catch (InterruptedException e) {
      e.printStackTrace();
    }

    // We have disabled the account, but it's still syncable.
    assertFalse(syncsAutomatically(account));
    assertTrue(isSyncable(account));
    assertTrue(willEnableOnUpgrade(account, accountManager));
  }

  private class Result {
    public boolean called = false;
  }

  public static HttpResponse simulate400() {
    HttpResponse response = new BasicHttpResponse(new ProtocolVersion("HTTP", 1, 1), 400, "Bad Request") {
      @Override
      public HttpEntity getEntity() {
        try {
          return new StringEntity("16");
        } catch (UnsupportedEncodingException e) {
          // Never happens.
          return null;
        }
      }
    };
    return response;
  }

  /**
   * Verify that when a 400 response is received with an
   * "Upgrade Required" response code body, we call
   * informUpgradeRequiredResponse on the delegate.
   */
  public void testUpgradeResponse() throws SyncConfigurationException, IllegalArgumentException, NonObjectJSONException, IOException, ParseException, CryptoException {
    final Result calledUpgradeRequired = new Result();
    final GlobalSessionCallback callback = new BlankGlobalSessionCallback() {
      @Override
      public void informUpgradeRequiredResponse(final GlobalSession session) {
        calledUpgradeRequired.called = true;
      }
    };

    final GlobalSession session = new MockGlobalSession(
        TEST_SERVER, TEST_USERNAME, TEST_PASSWORD,
        new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY), callback);

    session.interpretHTTPFailure(simulate400());
    assertTrue(calledUpgradeRequired.called);
  }

  @Override
  public void tearDown() {
    deleteTestAccount();
  }

  public abstract class BlankGlobalSessionCallback implements GlobalSessionCallback {
    public BlankGlobalSessionCallback() {
    }

    @Override
    public void requestBackoff(long backoff) {
    }

    @Override
    public boolean wantNodeAssignment() {
      return false;
    }

    @Override
    public void informUnauthorizedResponse(GlobalSession globalSession,
                                           URI oldClusterURL) {
    }

    @Override
    public void informNodeAssigned(GlobalSession globalSession,
                                   URI oldClusterURL, URI newClusterURL) {
    }

    @Override
    public void informNodeAuthenticationFailed(GlobalSession globalSession,
                                               URI failedClusterURL) {
    }

    @Override
    public void handleAborted(GlobalSession globalSession, String reason) {
    }

    @Override
    public void handleError(GlobalSession globalSession, Exception ex) {
    }

    @Override
    public void handleSuccess(GlobalSession globalSession) {
    }

    @Override
    public void handleStageCompleted(Stage currentState,
                                     GlobalSession globalSession) {
    }

    @Override
    public boolean shouldBackOff() {
      return false;
    }
  }
}
