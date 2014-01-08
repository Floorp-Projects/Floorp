/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.sync;

import java.net.URI;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.RSACryptoImplementation;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.FxAccountAuthenticator;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.SharedPreferencesClientsDataDelegate;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.BaseGlobalSessionCallback;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;

import android.accounts.Account;
import android.content.AbstractThreadedSyncAdapter;
import android.content.ContentProviderClient;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SyncResult;
import android.os.Bundle;

public class FxAccountSyncAdapter extends AbstractThreadedSyncAdapter {
  private static final String LOG_TAG = FxAccountSyncAdapter.class.getSimpleName();

  protected final ExecutorService executor;

  public FxAccountSyncAdapter(Context context, boolean autoInitialize) {
    super(context, autoInitialize);
    this.executor = Executors.newSingleThreadExecutor();
  }

  /**
   * A trivial global session callback that ignores backoff requests, upgrades,
   * and authorization errors. It simply waits until the sync completes.
   */
  protected static class SessionCallback implements BaseGlobalSessionCallback {
    protected final CountDownLatch latch;

    public SessionCallback(CountDownLatch latch) {
      if (latch == null) {
        throw new IllegalArgumentException("latch must not be null");
      }
      this.latch = latch;
    }

    @Override
    public boolean shouldBackOff() {
      return false;
    }

    @Override
    public void requestBackoff(long backoff) {
    }

    @Override
    public void informUpgradeRequiredResponse(GlobalSession session) {
    }

    @Override
    public void informUnauthorizedResponse(GlobalSession globalSession, URI oldClusterURL) {
    }

    @Override
    public void handleStageCompleted(Stage currentState, GlobalSession globalSession) {
    }

    @Override
    public void handleSuccess(GlobalSession globalSession) {
      Logger.info(LOG_TAG, "Successfully synced!");
      latch.countDown();
    }

    @Override
    public void handleError(GlobalSession globalSession, Exception ex) {
      Logger.warn(LOG_TAG, "Sync failed.", ex);
      latch.countDown();
    }

    @Override
    public void handleAborted(GlobalSession globalSession, String reason) {
      Logger.warn(LOG_TAG, "Sync aborted: " + reason);
      latch.countDown();
    }
  };

  /**
   * A trivial Sync implementation that does not cache client keys,
   * certificates, or tokens.
   *
   * This should be replaced with a full {@link FxAccountAuthenticator}-based
   * token implementation.
   */
  @Override
  public void onPerformSync(final Account account, final Bundle extras, final String authority, ContentProviderClient provider, SyncResult syncResult) {
    Logger.setThreadLogTag(FxAccountConstants.GLOBAL_LOG_TAG);

    Logger.info(LOG_TAG, "Syncing FxAccount" +
        " account named " + account.name +
        " for authority " + authority +
        " with instance " + this + ".");

    final CountDownLatch latch = new CountDownLatch(1);
    try {
      final BrowserIDKeyPair keyPair = RSACryptoImplementation.generateKeyPair(1024);
      Logger.info(LOG_TAG, "Generated keypair. ");

      final FxAccount fxAccount = FxAccountAuthenticator.fromAndroidAccount(getContext(), account);
      final String tokenServerEndpoint = fxAccount.authEndpoint + (fxAccount.authEndpoint.endsWith("/") ? "" : "/") + "1.0/sync/1.1";

      fxAccount.login(getContext(), tokenServerEndpoint, keyPair, new FxAccount.Delegate() {
        @Override
        public void handleSuccess(final String uid, final String endpoint, final AuthHeaderProvider authHeaderProvider) {
          Logger.pii(LOG_TAG, "Got token! uid is " + uid + " and endpoint is " + endpoint + ".");

          final BaseGlobalSessionCallback callback = new SessionCallback(latch);

          Executors.newSingleThreadExecutor().execute(new Runnable() {
            @Override
            public void run() {
              FxAccountGlobalSession globalSession = null;
              try {
                SharedPreferences sharedPrefs = getContext().getSharedPreferences(FxAccountConstants.PREFS_PATH, Context.MODE_PRIVATE);
                ClientsDataDelegate clientsDataDelegate = new SharedPreferencesClientsDataDelegate(sharedPrefs);
                final KeyBundle syncKeyBundle = FxAccountUtils.generateSyncKeyBundle(fxAccount.kB);
                globalSession = new FxAccountGlobalSession(endpoint, uid, authHeaderProvider, FxAccountConstants.PREFS_PATH, syncKeyBundle, callback, getContext(), extras, clientsDataDelegate);
                globalSession.start();
              } catch (Exception e) {
                callback.handleError(globalSession, e);
                return;
              }
            }
          });
        }

        @Override
        public void handleError(Exception e) {
          Logger.info(LOG_TAG, "Failed to get token.", e);
          latch.countDown();
        }
      });

      latch.await();
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Got error logging in.", e);
    }
  }
}
