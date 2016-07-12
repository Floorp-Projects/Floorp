/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

package org.mozilla.gecko.background.fxa;

import android.accounts.Account;
import android.content.Context;
import android.content.Loader;
import android.os.AsyncTask;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;

import org.mozilla.gecko.background.sync.AndroidSyncTestCaseWithAccounts;
import org.mozilla.gecko.fxa.AccountLoader;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Separated;
import org.mozilla.gecko.fxa.login.State;

import java.io.UnsupportedEncodingException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A version of https://android.googlesource.com/platform/frameworks/base/+/c91893511dc1b9e634648406c9ae61b15476e65d/test-runner/src/android/test/LoaderTestCase.java,
 * hacked to work with the v4 support library, and patched to work around
 * https://code.google.com/p/android/issues/detail?id=40987.
 */
public class TestAccountLoader extends AndroidSyncTestCaseWithAccounts {
  // Test account names must start with TEST_USERNAME in order to be recognized
  // as test accounts and deleted in tearDown.
  private static final String TEST_USERNAME = "testAccount@mozilla.com";
  private static final String TEST_ACCOUNTTYPE = FxAccountConstants.ACCOUNT_TYPE;

  private static final String TEST_SYNCKEY = "testSyncKey";
  private static final String TEST_SYNCPASSWORD = "testSyncPassword";

  private static final String TEST_TOKEN_SERVER_URI = "testTokenServerURI";
  private static final String TEST_PROFILE_SERVER_URI = "testProfileServerURI";
  private static final String TEST_AUTH_SERVER_URI = "testAuthServerURI";
  private static final String TEST_PROFILE = "testProfile";

  public TestAccountLoader() {
    super(TEST_ACCOUNTTYPE, TEST_USERNAME);
  }

  static {
    // Force class loading of AsyncTask on the main thread so that it's handlers are tied to
    // the main thread and responses from the worker thread get delivered on the main thread.
    // The tests are run on another thread, allowing them to block waiting on a response from
    // the code running on the main thread. The main thread can't block since the AsyncTask
    // results come in via the event loop.
    new AsyncTask<Void, Void, Void>() {
      @Override
      protected Void doInBackground(Void... args) {
        return null;
      }

      @Override
      protected void onPostExecute(Void result) {
      }
    };
  }

  /**
   * Runs a Loader synchronously and returns the result of the load. The loader will
   * be started, stopped, and destroyed by this method so it cannot be reused.
   *
   * @param loader The loader to run synchronously
   * @return The result from the loader
   */
  public <T> T getLoaderResultSynchronously(final Loader<T> loader) {
    // The test thread blocks on this queue until the loader puts it's result in
    final ArrayBlockingQueue<AtomicReference<T>> queue = new ArrayBlockingQueue<AtomicReference<T>>(1);

    // This callback runs on the "main" thread and unblocks the test thread
    // when it puts the result into the blocking queue
    final Loader.OnLoadCompleteListener<T> listener = new Loader.OnLoadCompleteListener<T>() {
      @Override
      public void onLoadComplete(Loader<T> completedLoader, T data) {
        // Shut the loader down
        completedLoader.unregisterListener(this);
        completedLoader.stopLoading();
        completedLoader.reset();
        // Store the result, unblocking the test thread
        queue.add(new AtomicReference<T>(data));
      }
    };

    // This handler runs on the "main" thread of the process since AsyncTask
    // is documented as needing to run on the main thread and many Loaders use
    // AsyncTask
    final Handler mainThreadHandler = new Handler(Looper.getMainLooper()) {
      @Override
      public void handleMessage(Message msg) {
        loader.registerListener(0, listener);
        loader.startLoading();
      }
    };

    // Ask the main thread to start the loading process
    mainThreadHandler.sendEmptyMessage(0);

    // Block on the queue waiting for the result of the load to be inserted
    T result;
    while (true) {
      try {
        result = queue.take().get();
        break;
      } catch (InterruptedException e) {
        throw new RuntimeException("waiting thread interrupted", e);
      }
    }
    return result;
  }

  public void testInitialLoad() throws UnsupportedEncodingException, GeneralSecurityException, URISyntaxException {
    // This is tricky. We can't mock the AccountManager easily -- see
    // https://groups.google.com/d/msg/android-mock/VXyzvKTMUGs/Y26wVPrl50sJ --
    // and we don't want to delete any existing accounts on device. So our test
    // needs to be adaptive (and therefore a little race-prone).

    final Context context = getApplicationContext();
    final AccountLoader loader = new AccountLoader(context);

    final boolean firefoxAccountsExist = FirefoxAccounts.firefoxAccountsExist(context);

    if (firefoxAccountsExist) {
      assertFirefoxAccount(getLoaderResultSynchronously((Loader<Account>) loader));
      return;
    }

    // This account will get cleaned up in tearDown.
    final State state = new Separated(TEST_USERNAME, "uid", false); // State choice is arbitrary.
    final AndroidFxAccount account = AndroidFxAccount.addAndroidAccount(context,
        TEST_USERNAME, TEST_PROFILE, TEST_AUTH_SERVER_URI, TEST_TOKEN_SERVER_URI, TEST_PROFILE_SERVER_URI,
        state, AndroidSyncTestCaseWithAccounts.TEST_SYNC_AUTOMATICALLY_MAP_WITH_ALL_AUTHORITIES_DISABLED);
    assertNotNull(account);
    assertFirefoxAccount(getLoaderResultSynchronously((Loader<Account>) loader));
  }

  protected void assertFirefoxAccount(Account account) {
    assertNotNull(account);
    assertEquals(FxAccountConstants.ACCOUNT_TYPE, account.type);
  }
}
