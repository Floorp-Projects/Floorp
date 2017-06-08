/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.sync;

import android.accounts.Account;
import android.content.AbstractThreadedSyncAdapter;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SyncResult;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.v4.content.LocalBroadcastManager;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.background.fxa.SkewHandler;
import org.mozilla.gecko.browserid.JSONWebTokenUtils;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.devices.FxAccountDeviceListUpdater;
import org.mozilla.gecko.fxa.devices.FxAccountDeviceRegistrator;
import org.mozilla.gecko.fxa.authenticator.AccountPickler;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.authenticator.FxADefaultLoginStateMachineDelegate;
import org.mozilla.gecko.fxa.authenticator.FxAccountAuthenticator;
import org.mozilla.gecko.fxa.login.FxAccountLoginStateMachine;
import org.mozilla.gecko.fxa.login.Married;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.fxa.sync.FxAccountSyncDelegate.Result;
import org.mozilla.gecko.sync.BackoffHandler;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.PrefsBackoffHandler;
import org.mozilla.gecko.sync.SharedPreferencesClientsDataDelegate;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.GlobalSessionCallback;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.HawkAuthHeaderProvider;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;
import org.mozilla.gecko.sync.telemetry.TelemetryCollector;
import org.mozilla.gecko.sync.telemetry.TelemetryContract;
import org.mozilla.gecko.tokenserver.TokenServerClient;
import org.mozilla.gecko.tokenserver.TokenServerClientDelegate;
import org.mozilla.gecko.tokenserver.TokenServerException;
import org.mozilla.gecko.tokenserver.TokenServerToken;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class FxAccountSyncAdapter extends AbstractThreadedSyncAdapter {
  private static final String LOG_TAG = FxAccountSyncAdapter.class.getSimpleName();

  public static final int NOTIFICATION_ID = LOG_TAG.hashCode();

  // Tracks the last seen storage hostname for backoff purposes.
  private static final String PREF_BACKOFF_STORAGE_HOST = "backoffStorageHost";

  // Used to do cheap in-memory rate limiting. Don't sync again if we
  // successfully synced within this duration.
  private static final int MINIMUM_SYNC_DELAY_MILLIS = 15 * 1000;        // 15 seconds.
  private volatile long lastSyncRealtimeMillis;

  // Non-user initiated sync can't take longer than 30 minutes.
  // To ensure we're not churning through device's battery/resources, we limit sync to 10 minutes,
  // and request a re-sync if we hit that deadline.
  private static final long SYNC_DEADLINE_DELTA_MILLIS = TimeUnit.MINUTES.toMillis(10);

  protected final ExecutorService executor;
  protected final FxAccountNotificationManager notificationManager;

  public FxAccountSyncAdapter(Context context, boolean autoInitialize) {
    super(context, autoInitialize);
    this.executor = Executors.newSingleThreadExecutor();
    this.notificationManager = new FxAccountNotificationManager(NOTIFICATION_ID);
  }

  protected static class SyncDelegate extends FxAccountSyncDelegate {
    @Override
    public void handleSuccess() {
      Logger.info(LOG_TAG, "Sync succeeded.");
      super.handleSuccess();
    }

    @Override
    public void handleError(Exception e) {
      Logger.error(LOG_TAG, "Got exception syncing.", e);
      super.handleError(e);
    }

    @Override
    public void handleCannotSync(State finalState) {
      Logger.warn(LOG_TAG, "Cannot sync from state: " + finalState.getStateLabel());
      super.handleCannotSync(finalState);
    }

    @Override
    public void postponeSync(long millis) {
      if (millis <= 0) {
        Logger.debug(LOG_TAG, "Asked to postpone sync, but zero delay.");
      }
      super.postponeSync(millis);
    }

    @Override
    public void rejectSync() {
      super.rejectSync();
    }

    /* package-local */ void requestFollowUpSync(String stage) {
      this.stageNamesForFollowUpSync.add(stage);
    }

    protected final Collection<String> stageNamesToSync;

    // Keeps track of incomplete stages during this sync that need to be re-synced once we're done.
    private final List<String> stageNamesForFollowUpSync = Collections.synchronizedList(new ArrayList<String>());
    private boolean fullSyncNecessary = false;

    public SyncDelegate(BlockingQueue<Result> latch, SyncResult syncResult, AndroidFxAccount fxAccount, Collection<String> stageNamesToSync) {
      super(latch, syncResult);
      this.stageNamesToSync = Collections.unmodifiableCollection(stageNamesToSync);
    }

    public Collection<String> getStageNamesToSync() {
      return this.stageNamesToSync;
    }
  }

  /**
   * Locally broadcasts telemetry gathered by GlobalSession, so that it may be processed by
   * Java Telemetry - specifically, {@link org.mozilla.gecko.telemetry.TelemetryBackgroundReceiver}.
   * Due to how packages are built, we can not call into it directly from *.sync.
   */
  private static class InstrumentedSessionCallback extends SessionCallback {
    private static final String ACTION_BACKGROUND_TELEMETRY = "org.mozilla.gecko.telemetry.BACKGROUND";

    private final LocalBroadcastManager localBroadcastManager;
    private final TelemetryCollector telemetryCollector;

    InstrumentedSessionCallback(LocalBroadcastManager localBroadcastManager, SyncDelegate syncDelegate, SchedulePolicy schedulePolicy) {
      super(syncDelegate, schedulePolicy);
      this.localBroadcastManager = localBroadcastManager;
      this.telemetryCollector = new TelemetryCollector();
    }

    TelemetryCollector getCollector() {
      return telemetryCollector;
    }

    @Override
    public void handleSuccess(GlobalSession globalSession) {
      super.handleSuccess(globalSession);
      recordTelemetry();
    }

    @Override
    public void handleError(GlobalSession globalSession, Exception ex, String reason) {
      super.handleError(globalSession, ex, reason);
      // If an error hasn't been set downstream, record what we know at this point.
      if (!telemetryCollector.hasError()) {
        telemetryCollector.setError(TelemetryCollector.KEY_ERROR_INTERNAL, reason, ex);
      }
      recordTelemetry();
    }

    @Override
    public void handleError(GlobalSession globalSession, Exception ex) {
      super.handleError(globalSession, ex);
      // If an error hasn't been set downstream, record what we know at this point.
      if (!telemetryCollector.hasError()) {
        telemetryCollector.setError(TelemetryCollector.KEY_ERROR_INTERNAL, ex);
      }
      recordTelemetry();
    }

    @Override
    public void handleAborted(GlobalSession globalSession, String reason) {
      super.handleAborted(globalSession, reason);
      // Note to future maintainers: while there are reasons, other than 'backoff', this method
      // might be called, in practice that _is_ the only reason it gets called at the moment of
      // writing this. If this changes, please do expand this telemetry handling.
      this.telemetryCollector.setError(TelemetryCollector.KEY_ERROR_INTERNAL, "backoff");
      recordTelemetry();
    }

    private void recordTelemetry() {
      telemetryCollector.setFinished(SystemClock.elapsedRealtime());
      final Intent telemetryIntent = new Intent();
      telemetryIntent.setAction(ACTION_BACKGROUND_TELEMETRY);
      telemetryIntent.putExtra(TelemetryContract.KEY_TYPE, TelemetryContract.KEY_TYPE_SYNC);
      telemetryIntent.putExtra(TelemetryContract.KEY_TELEMETRY, this.telemetryCollector.build());
      localBroadcastManager.sendBroadcast(telemetryIntent);
    }
  }

  protected static class SessionCallback implements GlobalSessionCallback {
    protected final SyncDelegate syncDelegate;
    protected final SchedulePolicy schedulePolicy;
    protected volatile BackoffHandler storageBackoffHandler;

    public SessionCallback(SyncDelegate syncDelegate, SchedulePolicy schedulePolicy) {
      this.syncDelegate = syncDelegate;
      this.schedulePolicy = schedulePolicy;
    }

    public void setBackoffHandler(BackoffHandler backoffHandler) {
      this.storageBackoffHandler = backoffHandler;
    }

    @Override
    public boolean shouldBackOffStorage() {
      return storageBackoffHandler.delayMilliseconds() > 0;
    }

    @Override
    public void requestBackoff(long backoffMillis) {
      final boolean onlyExtend = true;      // Because we trust what the storage server says.
      schedulePolicy.configureBackoffMillisOnBackoff(storageBackoffHandler, backoffMillis, onlyExtend);
    }

    @Override
    public void informUpgradeRequiredResponse(GlobalSession session) {
      schedulePolicy.onUpgradeRequired();
    }

    @Override
    public void informUnauthorizedResponse(GlobalSession globalSession, URI oldClusterURL) {
      schedulePolicy.onUnauthorized();
    }

    @Override
    public void informMigrated(GlobalSession globalSession) {
      // It's not possible to migrate a Firefox Account to another Account type
      // yet. Yell loudly but otherwise ignore.
      Logger.error(LOG_TAG,
          "Firefox Account informMigrated called, but it's not yet possible to migrate.  " +
          "Ignoring even though something is terribly wrong.");
    }

    @Override
    public void handleStageCompleted(Stage currentState, GlobalSession globalSession) {
    }

    /**
     * Schedule an incomplete stage for a follow-up sync.
     */
    @Override
    public void handleIncompleteStage(Stage currentState,
                                      GlobalSession globalSession) {
      syncDelegate.requestFollowUpSync(currentState.getRepositoryName());
    }

    /**
     * Use with caution, as this will request an immediate follow-up sync of all stages.
     */
    @Override
    public void handleFullSyncNecessary() {
      syncDelegate.fullSyncNecessary = true;
    }

    @Override
    public void handleSuccess(GlobalSession globalSession) {
      Logger.info(LOG_TAG, "Global session succeeded.");

      // Get the number of clients, so we can schedule the sync interval accordingly.
      try {
        int otherClientsCount = globalSession.getClientsDelegate().getClientsCount();
        Logger.debug(LOG_TAG, "" + otherClientsCount + " other client(s).");
        this.schedulePolicy.onSuccessfulSync(otherClientsCount);
      } finally {
        // Continue with the usual success flow.
        syncDelegate.handleSuccess();
      }
    }

    @Override
    public void handleError(GlobalSession globalSession, Exception ex, String reason) {
      this.handleError(globalSession, ex);
    }

    @Override
    public void handleError(GlobalSession globalSession, Exception e) {
      Logger.warn(LOG_TAG, "Global session failed."); // Exception will be dumped by delegate below.
      syncDelegate.handleError(e);
      // TODO: should we reduce the periodic sync interval?
    }

    @Override
    public void handleAborted(GlobalSession globalSession, String reason) {
      Logger.warn(LOG_TAG, "Global session aborted: " + reason);
      syncDelegate.handleError(null);
      // TODO: should we reduce the periodic sync interval?
    }
  };

  /**
   * Return true if the provided {@link BackoffHandler} isn't reporting that we're in
   * a backoff state, or the provided {@link Bundle} contains flags that indicate
   * we should force a sync.
   */
  private boolean shouldPerformSync(final BackoffHandler backoffHandler, final String kind, final Bundle extras) {
    final long delay = backoffHandler.delayMilliseconds();
    if (delay <= 0) {
      return true;
    }

    if (extras == null) {
      return false;
    }

    final boolean forced = extras.getBoolean(ContentResolver.SYNC_EXTRAS_IGNORE_BACKOFF, false);
    if (forced) {
      Logger.info(LOG_TAG, "Forced sync (" + kind + "): overruling remaining backoff of " + delay + "ms.");
    } else {
      Logger.info(LOG_TAG, "Not syncing (" + kind + "): must wait another " + delay + "ms.");
    }
    return forced;
  }

  protected void syncWithAssertion(final String assertion,
                                   final URI tokenServerEndpointURI,
                                   final BackoffHandler tokenBackoffHandler,
                                   final SharedPreferences sharedPrefs,
                                   final KeyBundle syncKeyBundle,
                                   final String clientState,
                                   final InstrumentedSessionCallback callback,
                                   final Bundle extras,
                                   final AndroidFxAccount fxAccount,
                                   final long syncDeadline) {
    final TokenServerClientDelegate delegate = new TokenServerClientDelegate() {
      private boolean didReceiveBackoff = false;

      @Override
      public String getUserAgent() {
        return FxAccountConstants.USER_AGENT;
      }

      @Override
      public void handleSuccess(final TokenServerToken token) {
        FxAccountUtils.pii(LOG_TAG, "Got token! uid is " + token.uid + " and endpoint is " + token.endpoint + ".");
        fxAccount.releaseSharedAccountStateLock();

        if (!didReceiveBackoff) {
          // We must be OK to touch this token server.
          tokenBackoffHandler.setEarliestNextRequest(0L);
        }

        final URI storageServerURI;
        try {
          storageServerURI = new URI(token.endpoint);
        } catch (URISyntaxException e) {
          handleError(e);
          return;
        }
        final String storageHostname = storageServerURI.getHost();

        // We back off on a per-host basis. When we have an endpoint URI from a token, we
        // can check on the backoff status for that host.
        // If we're supposed to be backing off, we abort the not-yet-started session.
        final BackoffHandler storageBackoffHandler = new PrefsBackoffHandler(sharedPrefs, "sync.storage");
        callback.setBackoffHandler(storageBackoffHandler);

        String lastStorageHost = sharedPrefs.getString(PREF_BACKOFF_STORAGE_HOST, null);
        final boolean storageHostIsUnchanged = lastStorageHost != null &&
                                               lastStorageHost.equalsIgnoreCase(storageHostname);
        if (storageHostIsUnchanged) {
          Logger.debug(LOG_TAG, "Storage host is unchanged.");
          if (!shouldPerformSync(storageBackoffHandler, "storage", extras)) {
            Logger.info(LOG_TAG, "Not syncing: storage server requested backoff.");
            callback.handleAborted(null, "Storage backoff");
            return;
          }
        } else {
          Logger.debug(LOG_TAG, "Received new storage host.");
        }

        // Invalidate the previous backoff, because our storage host has changed,
        // or we never had one at all, or we're OK to sync.
        storageBackoffHandler.setEarliestNextRequest(0L);

        GlobalSession globalSession = null;
        try {
          final ClientsDataDelegate clientsDataDelegate = new SharedPreferencesClientsDataDelegate(sharedPrefs, getContext());
          if (FxAccountUtils.LOG_PERSONAL_INFORMATION) {
            FxAccountUtils.pii(LOG_TAG, "Client device name is: '" + clientsDataDelegate.getClientName() + "'.");
            FxAccountUtils.pii(LOG_TAG, "Client device data last modified: " + clientsDataDelegate.getLastModifiedTimestamp());
          }

          // We compute skew over time using SkewHandler. This yields an unchanging
          // skew adjustment that the HawkAuthHeaderProvider uses to adjust its
          // timestamps. Eventually we might want this to adapt within the scope of a
          // global session.
          final SkewHandler storageServerSkewHandler = SkewHandler.getSkewHandlerForHostname(storageHostname);
          final long storageServerSkew = storageServerSkewHandler.getSkewInSeconds();
          // We expect Sync to upload large sets of records. Calculating the
          // payload verification hash for these record sets could be expensive,
          // so we explicitly do not send payload verification hashes to the
          // Sync storage endpoint.
          final boolean includePayloadVerificationHash = false;
          final AuthHeaderProvider authHeaderProvider = new HawkAuthHeaderProvider(token.id, token.key.getBytes("UTF-8"), includePayloadVerificationHash, storageServerSkew);

          final Context context = getContext();
          final SyncConfiguration syncConfig = new SyncConfiguration(token.uid, authHeaderProvider, sharedPrefs, syncKeyBundle);

          Collection<String> knownStageNames = SyncConfiguration.validEngineNames();
          syncConfig.stagesToSync = Utils.getStagesToSyncFromBundle(knownStageNames, extras);
          syncConfig.setClusterURL(storageServerURI);

          globalSession = new GlobalSession(syncConfig, callback, context, clientsDataDelegate, callback.getCollector());
          callback.getCollector().setIDs(token.hashedFxaUid, clientsDataDelegate.getAccountGUID());
          globalSession.start(syncDeadline);
        } catch (Exception e) {
          callback.handleError(globalSession, e);
          return;
        }
      }

      @Override
      public void handleFailure(TokenServerException e) {
        Logger.error(LOG_TAG, "Failed to get token.", e);
        try {
          // We should only get here *after* we're locked into the married state.
          State state = fxAccount.getState();
          if (state.getStateLabel() == StateLabel.Married) {
            Married married = (Married) state;
            fxAccount.setState(married.makeCohabitingState());
          }
        } finally {
          fxAccount.releaseSharedAccountStateLock();
        }
        callback.getCollector().setError(
                TelemetryCollector.KEY_ERROR_TOKEN,
                e.getClass().getSimpleName()
        );
        callback.handleError(null, e);
      }

      @Override
      public void handleError(Exception e) {
        Logger.error(LOG_TAG, "Failed to get token.", e);
        fxAccount.releaseSharedAccountStateLock();
        callback.getCollector().setError(
                TelemetryCollector.KEY_ERROR_TOKEN,
                e.getClass().getSimpleName()
        );
        callback.handleError(null, e);
      }

      @Override
      public void handleBackoff(int backoffSeconds) {
        // This is the token server telling us to back off.
        Logger.info(LOG_TAG, "Token server requesting backoff of " + backoffSeconds + "s. Backoff handler: " + tokenBackoffHandler);
        didReceiveBackoff = true;

        // If we've already stored a backoff, overrule it: we only use the server
        // value for token server scheduling.
        tokenBackoffHandler.setEarliestNextRequest(delay(backoffSeconds * 1000));
      }

      private long delay(long delay) {
        return System.currentTimeMillis() + delay;
      }
    };
    callback.getCollector().setStarted(SystemClock.elapsedRealtime());
    TokenServerClient tokenServerclient = new TokenServerClient(tokenServerEndpointURI, executor);
    tokenServerclient.getTokenFromBrowserIDAssertion(assertion, true, clientState, delegate);
  }

  public void maybeRegisterDevice(Context context, AndroidFxAccount fxAccount) {
    // Register the device if necessary (asynchronous, in another thread).
    // As part of device registration, we obtain a PushSubscription, register our push endpoint
    // with FxA, and update account data with fxaDeviceId, which is part of our synced
    // clients record.
    if (FxAccountDeviceRegistrator.shouldRegister(fxAccount)) {
      FxAccountDeviceRegistrator.register(context);
      // We might need to re-register periodically to ensure our FxA push subscription is valid.
      // This involves unsubscribing, subscribing and updating remote FxA device record with
      // new push subscription information.
    } else if (FxAccountDeviceRegistrator.shouldRenewRegistration(fxAccount)) {
      FxAccountDeviceRegistrator.renewRegistration(context);
    }
  }

  private void onSessionTokenStateReached(Context context, AndroidFxAccount fxAccount) {
    // This does not block the main thread, if work has to be done it is executed in a new thread.
    maybeRegisterDevice(context, fxAccount);

    FxAccountDeviceListUpdater deviceListUpdater = new FxAccountDeviceListUpdater(fxAccount, context.getContentResolver());
    // Since the clients stage requires a fresh list of remote devices, we update the device list synchronously.
    deviceListUpdater.update();
  }

  /**
   * A trivial Sync implementation that does not cache client keys,
   * certificates, or tokens.
   *
   * This should be replaced with a full {@link FxAccountAuthenticator}-based
   * token implementation.
   */
  @Override
  public void onPerformSync(final Account account, final Bundle extras, final String authority, ContentProviderClient provider, final SyncResult syncResult) {
    Logger.setThreadLogTag(FxAccountConstants.GLOBAL_LOG_TAG);
    Logger.resetLogging();

    final Context context = getContext();
    final AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);

    // NB: we use elapsedRealtime which is time since boot, to ensure our clock is monotonic and isn't
    // paused while CPU is in the power-saving mode.
    final long syncDeadline = SystemClock.elapsedRealtime() + SYNC_DEADLINE_DELTA_MILLIS;

    Logger.info(LOG_TAG, "Syncing FxAccount" +
        " account named like " + Utils.obfuscateEmail(account.name) +
        " for authority " + authority +
        " with instance " + this + ".");

    Logger.info(LOG_TAG, "Account last synced at: " + fxAccount.getLastSyncedTimestamp());

    if (FxAccountUtils.LOG_PERSONAL_INFORMATION) {
      fxAccount.dump();
    }

    FirefoxAccounts.logSyncOptions(extras);

    if (this.lastSyncRealtimeMillis > 0L &&
        (this.lastSyncRealtimeMillis + MINIMUM_SYNC_DELAY_MILLIS) > SystemClock.elapsedRealtime() &&
            !extras.getBoolean(ContentResolver.SYNC_EXTRAS_IGNORE_BACKOFF, false)) {
      Logger.info(LOG_TAG, "Not syncing FxAccount " + Utils.obfuscateEmail(account.name) +
                           ": minimum interval not met.");
      return;
    }

    // Pickle in a background thread to avoid strict mode warnings.
    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        try {
          AccountPickler.pickle(fxAccount, FxAccountConstants.ACCOUNT_PICKLE_FILENAME);
        } catch (Exception e) {
          // Should never happen, but we really don't want to die in a background thread.
          Logger.warn(LOG_TAG, "Got exception pickling current account details; ignoring.", e);
        }
      }
    });

    final BlockingQueue<Result> latch = new LinkedBlockingQueue<>(1);

    Collection<String> knownStageNames = SyncConfiguration.validEngineNames();
    Collection<String> stageNamesToSync = Utils.getStagesToSyncFromBundle(knownStageNames, extras);

    final SyncDelegate syncDelegate = new SyncDelegate(latch, syncResult, fxAccount, stageNamesToSync);
    Result offeredResult = null;

    try {
      // This will be the same chunk of SharedPreferences that we pass through to GlobalSession/SyncConfiguration.
      final SharedPreferences sharedPrefs = fxAccount.getSyncPrefs();

      final BackoffHandler backgroundBackoffHandler = new PrefsBackoffHandler(sharedPrefs, "background");
      final BackoffHandler rateLimitBackoffHandler = new PrefsBackoffHandler(sharedPrefs, "rate");

      // If this sync was triggered by user action, this will be true.
      final boolean isImmediate = (extras != null) &&
                                  (extras.getBoolean(ContentResolver.SYNC_EXTRAS_UPLOAD, false) ||
                                   extras.getBoolean(ContentResolver.SYNC_EXTRAS_IGNORE_BACKOFF, false));

      // If it's not an immediate sync, it must be either periodic or tickled.
      // Check our background rate limiter.
      if (!isImmediate) {
        if (!shouldPerformSync(backgroundBackoffHandler, "background", extras)) {
          syncDelegate.rejectSync();
          return;
        }
      }

      // Regardless, let's make sure we're not syncing too often.
      if (!shouldPerformSync(rateLimitBackoffHandler, "rate", extras)) {
        syncDelegate.postponeSync(rateLimitBackoffHandler.delayMilliseconds());
        return;
      }

      final SchedulePolicy schedulePolicy = new FxAccountSchedulePolicy(context, fxAccount);

      // Set a small scheduled 'backoff' to rate-limit the next sync,
      // and extend the background delay even further into the future.
      schedulePolicy.configureBackoffMillisBeforeSyncing(rateLimitBackoffHandler, backgroundBackoffHandler);

      final String tokenServerEndpoint = fxAccount.getTokenServerURI();
      final URI tokenServerEndpointURI = new URI(tokenServerEndpoint);
      final String audience = FxAccountUtils.getAudienceForURL(tokenServerEndpoint);

      try {
        // The clock starts... now!
        fxAccount.acquireSharedAccountStateLock(FxAccountSyncAdapter.LOG_TAG);
      } catch (InterruptedException e) {
        // OK, skip this sync.
        syncDelegate.handleError(e);
        return;
      }

      final State state;
      try {
        state = fxAccount.getState();
      } catch (Exception e) {
        fxAccount.releaseSharedAccountStateLock();
        syncDelegate.handleError(e);
        return;
      }

      final FxAccountLoginStateMachine stateMachine = new FxAccountLoginStateMachine();
      stateMachine.advance(state, StateLabel.Married, new FxADefaultLoginStateMachineDelegate(context, fxAccount) {
        @Override
        public void handleNotMarried(State notMarried) {
          Logger.info(LOG_TAG, "handleNotMarried: in " + notMarried.getStateLabel());
          schedulePolicy.onHandleFinal(notMarried.getNeededAction());
          syncDelegate.handleCannotSync(notMarried);
          if (notMarried.getStateLabel() == StateLabel.Engaged) {
            onSessionTokenStateReached(context, fxAccount);
          }
        }

        private boolean shouldRequestToken(final BackoffHandler tokenBackoffHandler, final Bundle extras) {
          return shouldPerformSync(tokenBackoffHandler, "token", extras);
        }

        @Override
        public void handleMarried(Married married) {
          schedulePolicy.onHandleFinal(married.getNeededAction());
          Logger.info(LOG_TAG, "handleMarried: in " + married.getStateLabel());

          try {
            final String assertion = married.generateAssertion(audience, JSONWebTokenUtils.DEFAULT_ASSERTION_ISSUER);

            /*
             * At this point we're in the correct state to sync, and we're ready to fetch
             * a token and do some work.
             *
             * But first we need to do two things:
             * 1. Check to see whether we're in a backoff situation for the token server.
             *    If we are, but we're not forcing a sync, then we go no further.
             * 2. Clear an existing backoff (if we're syncing it doesn't matter, and if
             *    we're forcing we'll get a new backoff if things are still bad).
             *
             * Note that we don't check the storage backoff before the token dance: the token
             * server tells us which server we're syncing to!
             *
             * That logic lives in the TokenServerClientDelegate elsewhere in this file.
             */

            // Strictly speaking this backoff check could be done prior to walking through
            // the login state machine, allowing us to short-circuit sooner.
            // We don't expect many token server backoffs, and most users will be sitting
            // in the Married state, so instead we simply do this here, once.
            final BackoffHandler tokenBackoffHandler = new PrefsBackoffHandler(sharedPrefs, "token");
            if (!shouldRequestToken(tokenBackoffHandler, extras)) {
              Logger.info(LOG_TAG, "Not syncing (token server).");
              syncDelegate.postponeSync(tokenBackoffHandler.delayMilliseconds());
              return;
            }

            onSessionTokenStateReached(context, fxAccount);

            final InstrumentedSessionCallback sessionCallback = new InstrumentedSessionCallback(
                    LocalBroadcastManager.getInstance(context),
                    syncDelegate,
                    schedulePolicy
            );

            final KeyBundle syncKeyBundle = married.getSyncKeyBundle();
            final String clientState = married.getClientState();
            syncWithAssertion(
                    assertion, tokenServerEndpointURI, tokenBackoffHandler, sharedPrefs,
                    syncKeyBundle, clientState, sessionCallback, extras, fxAccount, syncDeadline);

            // Force fetch the profile avatar information. (asynchronous, in another thread)
            Logger.info(LOG_TAG, "Fetching profile avatar information.");
            fxAccount.fetchProfileJSON();
          } catch (Exception e) {
            syncDelegate.handleError(e);
            return;
          }
        }
      });

      offeredResult = latch.take();
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Got error syncing.", e);
      syncDelegate.handleError(e);
    } finally {
      fxAccount.releaseSharedAccountStateLock();
    }

    lastSyncRealtimeMillis = SystemClock.elapsedRealtime();

    // We got to this point without being offered a result, and so it's unwise to proceed with
    // trying to sync stages again. Nothing else we can do but log an error.
    if (offeredResult == null) {
      Logger.error(LOG_TAG, "Did not receive a sync result from the delegate.");
      return;
    }

    // Full sync (of all of stages) is necessary if we hit "concurrent modification" errors while
    // uploading meta/global stage. This is considered both a rare and important event, so it's
    // deemed safe and necessary to request an immediate sync, which will ignore any back-offs and
    // will happen right away.
    if (syncDelegate.fullSyncNecessary) {
      Logger.info(LOG_TAG, "Syncing done. Full follow-up sync necessary, requesting immediate sync.");
      fxAccount.requestImmediateSync(null, null);
      return;
    }

    // If there are any incomplete stages, request a follow-up sync. Otherwise, we're done.
    // Incomplete stage is:
    // - one that hit a 412 error during either upload or download of data, indicating that
    //   its collection has been modified remotely, or
    // - one that hit a sync deadline
    final String[] stagesToSyncAgain;
    synchronized (syncDelegate.stageNamesForFollowUpSync) {
      stagesToSyncAgain = syncDelegate.stageNamesForFollowUpSync.toArray(
              new String[syncDelegate.stageNamesForFollowUpSync.size()]
      );
    }

    if (stagesToSyncAgain.length == 0) {
      Logger.info(LOG_TAG, "Syncing done.");
      return;
    }

    // If there are any other stages marked as incomplete, request that they're synced again.
    Logger.info(LOG_TAG, "Syncing done. Requesting an immediate follow-up sync.");
    fxAccount.requestImmediateSync(stagesToSyncAgain, null);
  }
}
