/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.sync;

import java.net.URI;
import java.security.NoSuchAlgorithmException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.SkewHandler;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.JSONWebTokenUtils;
import org.mozilla.gecko.browserid.RSACryptoImplementation;
import org.mozilla.gecko.browserid.verifier.BrowserIDRemoteVerifierClient;
import org.mozilla.gecko.browserid.verifier.BrowserIDVerifierDelegate;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.authenticator.FxAccountAuthenticator;
import org.mozilla.gecko.fxa.login.FxAccountLoginStateMachine;
import org.mozilla.gecko.fxa.login.FxAccountLoginStateMachine.LoginStateMachineDelegate;
import org.mozilla.gecko.fxa.login.FxAccountLoginTransition.Transition;
import org.mozilla.gecko.fxa.login.Married;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.SharedPreferencesClientsDataDelegate;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.BaseGlobalSessionCallback;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.HawkAuthHeaderProvider;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;
import org.mozilla.gecko.tokenserver.TokenServerClient;
import org.mozilla.gecko.tokenserver.TokenServerClientDelegate;
import org.mozilla.gecko.tokenserver.TokenServerException;
import org.mozilla.gecko.tokenserver.TokenServerToken;

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
    protected final SyncResult syncResult;
    protected final AndroidFxAccount fxAccount;

    public SessionCallback(CountDownLatch latch, SyncResult syncResult, AndroidFxAccount fxAccount) {
      if (latch == null) {
        throw new IllegalArgumentException("latch must not be null");
      }
      if (syncResult == null) {
        throw new IllegalArgumentException("syncResult must not be null");
      }
      if (fxAccount == null) {
        throw new IllegalArgumentException("fxAccount must not be null");
      }
      this.latch = latch;
      this.syncResult = syncResult;
      this.fxAccount = fxAccount;
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

    /**
     * No error!  Say that we made progress.
     */
    protected void setSyncResultSuccess() {
      syncResult.stats.numUpdates += 1;
    }

    /**
     * Soft error. Say that we made progress, so that Android will sync us again
     * after exponential backoff.
     */
    protected void setSyncResultSoftError() {
      syncResult.stats.numUpdates += 1;
      syncResult.stats.numIoExceptions += 1;
    }

    /**
     * Hard error. We don't want Android to sync us again, even if we make
     * progress, until the user intervenes.
     */
    protected void setSyncResultHardError() {
      syncResult.stats.numAuthExceptions += 1;
    }

    @Override
    public void handleSuccess(GlobalSession globalSession) {
      setSyncResultSuccess();
      Logger.info(LOG_TAG, "Sync succeeded.");
      latch.countDown();
    }

    @Override
    public void handleError(GlobalSession globalSession, Exception e) {
      // This is awful, but we need to propagate bad assertions back up the
      // chain somehow, and this will do for now.
      if (e instanceof TokenServerException) {
        // We should only get here *after* we're locked into the married state.
        State state = fxAccount.getState();
        if (state.getStateLabel() == StateLabel.Married) {
          Married married = (Married) state;
          fxAccount.setState(married.makeCohabitingState());
        }
      }
      setSyncResultSoftError();
      Logger.warn(LOG_TAG, "Sync failed.", e);
      latch.countDown();
    }

    @Override
    public void handleAborted(GlobalSession globalSession, String reason) {
      setSyncResultSoftError();
      Logger.warn(LOG_TAG, "Sync aborted: " + reason);
      latch.countDown();
    }
  };

  protected void syncWithAssertion(final String audience, final String assertion, URI tokenServerEndpointURI, final String prefsPath, final SharedPreferences sharedPrefs, final KeyBundle syncKeyBundle, final BaseGlobalSessionCallback callback) {
    TokenServerClient tokenServerclient = new TokenServerClient(tokenServerEndpointURI, executor);
    tokenServerclient.getTokenFromBrowserIDAssertion(assertion, true, new TokenServerClientDelegate() {
      @Override
      public void handleSuccess(final TokenServerToken token) {
        FxAccountConstants.pii(LOG_TAG, "Got token! uid is " + token.uid + " and endpoint is " + token.endpoint + ".");

        FxAccountGlobalSession globalSession = null;
        try {
          ClientsDataDelegate clientsDataDelegate = new SharedPreferencesClientsDataDelegate(sharedPrefs);

          // We compute skew over time using SkewHandler. This yields an unchanging
          // skew adjustment that the HawkAuthHeaderProvider uses to adjust its
          // timestamps. Eventually we might want this to adapt within the scope of a
          // global session.
          final SkewHandler tokenServerSkewHandler = SkewHandler.getSkewHandlerFromEndpointString(token.endpoint);
          final long tokenServerSkew = tokenServerSkewHandler.getSkewInSeconds();
          AuthHeaderProvider authHeaderProvider = new HawkAuthHeaderProvider(token.id, token.key.getBytes("UTF-8"), false, tokenServerSkew);

          final Context context = getContext();
          SyncConfiguration syncConfig = new SyncConfiguration(token.uid, authHeaderProvider, sharedPrefs, syncKeyBundle);

          // EXTRAS
          final Bundle extras = Bundle.EMPTY;
          globalSession = new FxAccountGlobalSession(token.endpoint, syncConfig, callback, context, extras, clientsDataDelegate);
          globalSession.start();
        } catch (Exception e) {
          callback.handleError(globalSession, e);
          return;
        }
      }

      @Override
      public void handleFailure(TokenServerException e) {
        debugAssertion(audience, assertion);
        handleError(e);
      }

      @Override
      public void handleError(Exception e) {
        Logger.error(LOG_TAG, "Failed to get token.", e);
        callback.handleError(null, e);
      }
    });
  }

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
    Logger.resetLogging();

    Logger.info(LOG_TAG, "Syncing FxAccount" +
        " account named " + account.name +
        " for authority " + authority +
        " with instance " + this + ".");

    final Context context = getContext();
    final AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);
    if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
      fxAccount.dump();
    }
    final CountDownLatch latch = new CountDownLatch(1);
    final BaseGlobalSessionCallback callback = new SessionCallback(latch, syncResult, fxAccount);

    try {
      State state;
      try {
        state = fxAccount.getState();
      } catch (Exception e) {
        callback.handleError(null, e);
        return;
      }

      final String prefsPath = fxAccount.getSyncPrefsPath();

      // This will be the same chunk of SharedPreferences that GlobalSession/SyncConfiguration will later create.
      final SharedPreferences sharedPrefs = context.getSharedPreferences(prefsPath, Utils.SHARED_PREFERENCES_MODE);

      final String audience = fxAccount.getAudience();
      final String authServerEndpoint = fxAccount.getAccountServerURI();
      final String tokenServerEndpoint = fxAccount.getTokenServerURI();
      final URI tokenServerEndpointURI = new URI(tokenServerEndpoint);

      // TODO: why doesn't the loginPolicy extract the audience from the account?
      final FxAccountClient client = new FxAccountClient20(authServerEndpoint, executor);
      final FxAccountLoginStateMachine stateMachine = new FxAccountLoginStateMachine();
      stateMachine.advance(state, StateLabel.Married, new LoginStateMachineDelegate() {
        @Override
        public FxAccountClient getClient() {
          return client;
        }

        @Override
        public long getCertificateDurationInMilliseconds() {
          return 60 * 60 * 1000;
        }

        @Override
        public long getAssertionDurationInMilliseconds() {
          return 15 * 60 * 1000;
        }

        @Override
        public BrowserIDKeyPair generateKeyPair() throws NoSuchAlgorithmException {
          return RSACryptoImplementation.generateKeyPair(1024);
        }

        @Override
        public void handleTransition(Transition transition, State state) {
          Logger.warn(LOG_TAG, "handleTransition: " + transition + " to " + state);
        }

        @Override
        public void handleFinal(State state) {
          Logger.warn(LOG_TAG, "handleFinal: in " + state);
          fxAccount.setState(state);

          try {
            if (state.getStateLabel() != StateLabel.Married) {
              callback.handleError(null, new RuntimeException("Cannot sync from state: " + state));
              return;
            }

            Married married = (Married) state;
            final long now = System.currentTimeMillis();
            SkewHandler skewHandler = SkewHandler.getSkewHandlerFromEndpointString(tokenServerEndpoint);
            String assertion = married.generateAssertion(audience, JSONWebTokenUtils.DEFAULT_ASSERTION_ISSUER,
                now + skewHandler.getSkewInMillis(),
                this.getAssertionDurationInMilliseconds());
            syncWithAssertion(audience, assertion, tokenServerEndpointURI, prefsPath, sharedPrefs, married.getSyncKeyBundle(), callback);
          } catch (Exception e) {
            callback.handleError(null, e);
            return;
          }
        }
      });

      latch.await();
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Got error syncing.", e);
      callback.handleError(null, e);
    }

    Logger.error(LOG_TAG, "Syncing done.");
  }

  protected void debugAssertion(String audience, String assertion) {
    final CountDownLatch verifierLatch = new CountDownLatch(1);
    BrowserIDRemoteVerifierClient client = new BrowserIDRemoteVerifierClient(URI.create(BrowserIDRemoteVerifierClient.DEFAULT_VERIFIER_URL));
    client.verify(audience, assertion, new BrowserIDVerifierDelegate() {
      @Override
      public void handleSuccess(ExtendedJSONObject response) {
        Logger.info(LOG_TAG, "Remote verifier returned success: " + response.toJSONString());
        verifierLatch.countDown();
      }

      @Override
      public void handleFailure(ExtendedJSONObject response) {
        Logger.warn(LOG_TAG, "Remote verifier returned failure: " + response.toJSONString());
        verifierLatch.countDown();
      }

      @Override
      public void handleError(Exception e) {
        Logger.error(LOG_TAG, "Remote verifier returned error.", e);
        verifierLatch.countDown();
      }
    });

    try {
      verifierLatch.await();
    } catch (InterruptedException e) {
      Logger.error(LOG_TAG, "Got error.", e);
    }
  }
}
