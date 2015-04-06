/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Collection;
import java.util.EnumSet;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

import org.mozilla.gecko.background.ReadingListConstants;
import org.mozilla.gecko.background.common.PrefsBranch;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FirefoxAccounts.SyncHint;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.sync.FxAccountSyncDelegate;
import org.mozilla.gecko.fxa.sync.FxAccountSyncDelegate.Result;
import org.mozilla.gecko.sync.BackoffHandler;
import org.mozilla.gecko.sync.PrefsBackoffHandler;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BearerAuthHeaderProvider;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.AbstractThreadedSyncAdapter;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SyncResult;
import android.os.Bundle;

public class ReadingListSyncAdapter extends AbstractThreadedSyncAdapter {
  public static final String PREF_LOCAL_NAME = "device.localname";

  private static final String LOG_TAG = ReadingListSyncAdapter.class.getSimpleName();
  private static final long TIMEOUT_SECONDS = 60;
  protected final ExecutorService executor;

  // Don't sync again if we successfully synced within this duration.
  private static final int AFTER_SUCCESS_SYNC_DELAY_SECONDS = 5 * 60; // 5 minutes.
  // Don't sync again if we unsuccessfully synced within this duration.
  private static final int AFTER_ERROR_SYNC_DELAY_SECONDS = 15 * 60; // 15 minutes.

  public ReadingListSyncAdapter(Context context, boolean autoInitialize) {
    super(context, autoInitialize);
    this.executor = Executors.newSingleThreadExecutor();
  }

  protected static abstract class SyncAdapterSynchronizerDelegate implements ReadingListSynchronizerDelegate {
    private final FxAccountSyncDelegate syncDelegate;
    private final ContentProviderClient cpc;
    private final SyncResult result;

    SyncAdapterSynchronizerDelegate(FxAccountSyncDelegate syncDelegate,
                                    ContentProviderClient cpc,
                                    SyncResult result) {
      this.syncDelegate = syncDelegate;
      this.cpc = cpc;
      this.result = result;
    }

    abstract public void onInvalidAuthentication();

    @Override
    public void onUnableToSync(Exception e) {
      Logger.warn(LOG_TAG, "Unable to sync.", e);
      if (e instanceof ReadingListInvalidAuthenticationException) {
        onInvalidAuthentication();
      }
      cpc.release();
      syncDelegate.handleError(e);
    }

    @Override
    public void onDeletionsUploadComplete() {
      Logger.debug(LOG_TAG, "Step: onDeletionsUploadComplete");
      this.result.stats.numEntries += 1;   // TODO: Bug 1140809.
    }

    @Override
    public void onStatusUploadComplete(Collection<String> uploaded,
                                       Collection<String> failed) {
      Logger.debug(LOG_TAG, "Step: onStatusUploadComplete");
      this.result.stats.numEntries += 1;   // TODO: Bug 1140809.
    }

    @Override
    public void onNewItemUploadComplete(Collection<String> uploaded,
                                        Collection<String> failed) {
      Logger.debug(LOG_TAG, "Step: onNewItemUploadComplete");
      this.result.stats.numEntries += 1;   // TODO: Bug 1140809.
    }

    @Override
    public void onModifiedUploadComplete() {
      Logger.debug(LOG_TAG, "Step: onModifiedUploadComplete");
      this.result.stats.numEntries += 1;   // TODO: Bug 1140809.
    }

    @Override
    public void onDownloadComplete() {
      Logger.debug(LOG_TAG, "Step: onDownloadComplete");
      this.result.stats.numInserts += 1;   // TODO: Bug 1140809.
    }

    @Override
    public void onComplete() {
      Logger.info(LOG_TAG, "Reading list synchronization complete.");
      cpc.release();
      syncDelegate.handleSuccess();
    }
  }

  private void syncWithAuthorization(final Context context,
                                     final URI endpoint,
                                     final SyncResult syncResult,
                                     final FxAccountSyncDelegate syncDelegate,
                                     final String authToken,
                                     final SharedPreferences sharedPrefs,
                                     final Bundle extras) {
    final AuthHeaderProvider auth = new BearerAuthHeaderProvider(authToken);

    final PrefsBranch branch = new PrefsBranch(sharedPrefs, "readinglist.");
    final ReadingListClient remote = new ReadingListClient(endpoint, auth);
    final ContentProviderClient cpc = getContentProviderClient(context); // Released by the inner SyncAdapterSynchronizerDelegate.

    final LocalReadingListStorage local = new LocalReadingListStorage(cpc);
    String localName = branch.getString(PREF_LOCAL_NAME, null);
    if (localName == null) {
      localName = FxAccountUtils.defaultClientName(context);
    }

    // Make sure DB rows don't refer to placeholder values.
    local.updateLocalNames(localName);

    final ReadingListSynchronizer synchronizer = new ReadingListSynchronizer(branch, remote, local);

    synchronizer.syncAll(new SyncAdapterSynchronizerDelegate(syncDelegate, cpc, syncResult) {
      @Override
      public void onInvalidAuthentication() {
        // The reading list server rejected our oauth token! Invalidate it. Next
        // time through, we'll request a new one, which will drive the login
        // state machine, produce a new assertion, and eventually a fresh token.
        Logger.info(LOG_TAG, "Invalidating oauth token after 401!");
        AccountManager.get(context).invalidateAuthToken(FxAccountConstants.ACCOUNT_TYPE, authToken);
      }
    });
    // TODO: backoffs, and everything else handled by a SessionCallback.
  }

  @Override
  public void onPerformSync(final Account account, final Bundle extras, final String authority, final ContentProviderClient provider, final SyncResult syncResult) {
    Logger.setThreadLogTag(ReadingListConstants.GLOBAL_LOG_TAG);
    Logger.resetLogging();

    final EnumSet<SyncHint> syncHints = FirefoxAccounts.getHintsToSyncFromBundle(extras);
    FirefoxAccounts.logSyncHints(syncHints);

    final Context context = getContext();
    final AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);

    // Don't sync Reading List if we're in a non-default configuration, but allow testing against stage.
    final String accountServerURI = fxAccount.getAccountServerURI();
    final boolean usingDefaultAuthServer = FxAccountConstants.DEFAULT_AUTH_SERVER_ENDPOINT.equals(accountServerURI);
    final boolean usingStageAuthServer = FxAccountConstants.STAGE_AUTH_SERVER_ENDPOINT.equals(accountServerURI);
    if (!usingDefaultAuthServer && !usingStageAuthServer) {
      Logger.error(LOG_TAG, "Skipping Reading List sync because Firefox Account is not using prod or stage auth server.");
      // Stop syncing the Reading List entirely.
      ContentResolver.setIsSyncable(account, BrowserContract.READING_LIST_AUTHORITY, 0);
      return;
    }
    final String tokenServerURI = fxAccount.getTokenServerURI();
    final boolean usingDefaultSyncServer = FxAccountConstants.DEFAULT_TOKEN_SERVER_ENDPOINT.equals(tokenServerURI);
    final boolean usingStageSyncServer = FxAccountConstants.STAGE_TOKEN_SERVER_ENDPOINT.equals(tokenServerURI);
    if (!usingDefaultSyncServer && !usingStageSyncServer) {
      Logger.error(LOG_TAG, "Skipping Reading List sync because Sync is not using the prod or stage Sync (token) server.");
      Logger.debug(LOG_TAG, "If the user has chosen to not store Sync data with Mozilla, we shouldn't store Reading List data with Mozilla .");
      // Stop syncing the Reading List entirely.
      ContentResolver.setIsSyncable(account, BrowserContract.READING_LIST_AUTHORITY, 0);
      return;
    }

    Result result = Result.Error;
    final BlockingQueue<Result> latch = new LinkedBlockingQueue<Result>(1);
    final FxAccountSyncDelegate syncDelegate = new FxAccountSyncDelegate(latch, syncResult);

    // Allow testing against stage.
    final String endpointString;
    if (usingStageAuthServer) {
      endpointString = ReadingListConstants.DEFAULT_DEV_ENDPOINT;
    } else {
      endpointString = ReadingListConstants.DEFAULT_PROD_ENDPOINT;
    }

    Logger.info(LOG_TAG, "Syncing reading list against endpoint: " + endpointString);
    final URI endpointURI;
    try {
      endpointURI = new URI(endpointString);
    } catch (URISyntaxException e) {
      // Should never happen.
      Logger.error(LOG_TAG, "Unexpected malformed URI for reading list service: " + endpointString);
      syncDelegate.handleError(e);
      return;
    }

    final AccountManager accountManager = AccountManager.get(context);
    // If we have an auth failure that requires user intervention, FxA will show system
    // notifications prompting the user to re-connect as it advances the internal account state.
    // true causes the auth token fetch to return null on failure immediately, rather than doing
    // Mysterious Internal Work to try to get the token.
    final boolean notifyAuthFailure = true;
    try {
      final SharedPreferences sharedPrefs = fxAccount.getReadingListPrefs();

      final BackoffHandler storageBackoffHandler = new PrefsBackoffHandler(sharedPrefs, "storage");
      final long storageBackoffDelayMilliseconds = storageBackoffHandler.delayMilliseconds();
      if (!syncHints.contains(SyncHint.SCHEDULE_NOW) && !syncHints.contains(SyncHint.IGNORE_REMOTE_SERVER_BACKOFF) && storageBackoffDelayMilliseconds > 0) {
        Logger.warn(LOG_TAG, "Not syncing: storage requested additional backoff: " + storageBackoffDelayMilliseconds + " milliseconds.");
        syncDelegate.rejectSync();
        return;
      }

      final BackoffHandler rateLimitBackoffHandler = new PrefsBackoffHandler(sharedPrefs, "rate");
      final long rateLimitBackoffDelayMilliseconds = rateLimitBackoffHandler.delayMilliseconds();
      if (!syncHints.contains(SyncHint.SCHEDULE_NOW) && !syncHints.contains(SyncHint.IGNORE_LOCAL_RATE_LIMIT) && rateLimitBackoffDelayMilliseconds > 0) {
        Logger.warn(LOG_TAG, "Not syncing: local rate limiting for another: " + rateLimitBackoffDelayMilliseconds + " milliseconds.");
        syncDelegate.rejectSync();
        return;
      }

      final String authToken = accountManager.blockingGetAuthToken(account, ReadingListConstants.AUTH_TOKEN_TYPE, notifyAuthFailure);
      if (authToken == null) {
        throw new RuntimeException("Couldn't get oauth token!  Aborting sync.");
      }

      final ReadingListBackoffObserver observer = new ReadingListBackoffObserver(endpointURI.getHost());
      BaseResource.addHttpResponseObserver(observer);
      try {
        syncWithAuthorization(context, endpointURI, syncResult, syncDelegate, authToken, sharedPrefs, extras);
        result = latch.poll(TIMEOUT_SECONDS, TimeUnit.SECONDS);
      } finally {
        BaseResource.removeHttpResponseObserver(observer);
        long backoffInSeconds = observer.largestBackoffObservedInSeconds.get();
        if (backoffInSeconds > 0) {
          Logger.warn(LOG_TAG, "Observed " + backoffInSeconds + "-second backoff request.");
          storageBackoffHandler.extendEarliestNextRequest(System.currentTimeMillis() + 1000 * backoffInSeconds);
        }
      }

      if (result == null) {
        // The poll timed out. Let's call this an error.
        result = Result.Error;
      }

      switch (result) {
      case Success:
        requestPeriodicSync(account, ReadingListSyncAdapter.AFTER_SUCCESS_SYNC_DELAY_SECONDS);
        break;
      case Error:
        requestPeriodicSync(account, ReadingListSyncAdapter.AFTER_ERROR_SYNC_DELAY_SECONDS);
        break;
      case Postponed:
        break;
      case Rejected:
        break;
      }

      Logger.info(LOG_TAG, "Reading list sync done.");
    } catch (Exception e) {
      // We can get lots of exceptions here; handle them uniformly.
      Logger.error(LOG_TAG, "Got error syncing.", e);
      syncDelegate.handleError(e);
    }

    /*
     * TODO:
     * * Account error notifications. How do we avoid these overlapping with Sync?
     * * Pickling. How do we avoid pickling twice if you use both Sync and RL?
     */

    /*
     * TODO:
     * * Auth.
     * * Server URI lookup.
     * * Syncing.
     * * Error handling.
     * * Forcing syncs/interactive use.
     */
  }

  private ContentProviderClient getContentProviderClient(Context context) {
    final ContentResolver contentResolver = context.getContentResolver();
    final ContentProviderClient client = contentResolver.acquireContentProviderClient(ReadingListItems.CONTENT_URI);
    return client;
  }

  /**
   * Updates the existing system periodic sync interval to the specified duration.
   *
   * @param intervalSeconds the requested period, which Android will vary by up to 4%.
   */
  protected void requestPeriodicSync(final Account account, final long intervalSeconds) {
    final String authority = BrowserContract.AUTHORITY;
    Logger.info(LOG_TAG, "Scheduling periodic sync for " + intervalSeconds + ".");
    ContentResolver.addPeriodicSync(account, authority, Bundle.EMPTY, intervalSeconds);
  }
}
