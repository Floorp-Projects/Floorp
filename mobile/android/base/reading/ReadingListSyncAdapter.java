/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Collection;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

import org.mozilla.gecko.background.common.PrefsBranch;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.sync.FxAccountSyncDelegate;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
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
                                     final Account account,
                                     final SyncResult syncResult,
                                     final FxAccountSyncDelegate syncDelegate,
                                     final String authToken,
                                     final SharedPreferences sharedPrefs,
                                     final Bundle extras) {
    final AuthHeaderProvider auth = new BearerAuthHeaderProvider(authToken);

    final String endpointString = ReadingListConstants.DEFAULT_PROD_ENDPOINT;
    final URI endpoint;
    Logger.info(LOG_TAG, "Syncing reading list against " + endpointString);
    try {
      endpoint = new URI(endpointString);
    } catch (URISyntaxException e) {
      // Should never happen.
      Logger.error(LOG_TAG, "Unexpected malformed URI for reading list service: " + endpointString);
      syncDelegate.handleError(e);
      return;
    }

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
        AccountManager.get(context).invalidateAuthToken(account.type, authToken);
      }
    });
    // TODO: backoffs, and everything else handled by a SessionCallback.
  }

  @Override
  public void onPerformSync(final Account account, final Bundle extras, final String authority, final ContentProviderClient provider, final SyncResult syncResult) {
    Logger.setThreadLogTag(ReadingListConstants.GLOBAL_LOG_TAG);
    Logger.resetLogging();

    final Context context = getContext();
    final AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);

    final CountDownLatch latch = new CountDownLatch(1);
    final FxAccountSyncDelegate syncDelegate = new FxAccountSyncDelegate(latch, syncResult, fxAccount);

    final AccountManager accountManager = AccountManager.get(context);
    // If we have an auth failure that requires user intervention, FxA will show system
    // notifications prompting the user to re-connect as it advances the internal account state.
    // true causes the auth token fetch to return null on failure immediately, rather than doing
    // Mysterious Internal Work to try to get the token.
    final boolean notifyAuthFailure = true;
    try {
      final String authToken = accountManager.blockingGetAuthToken(account, ReadingListConstants.AUTH_TOKEN_TYPE, notifyAuthFailure);
      if (authToken == null) {
        throw new RuntimeException("Couldn't get oauth token!  Aborting sync.");
      }
      final SharedPreferences sharedPrefs = fxAccount.getReadingListPrefs();
      syncWithAuthorization(context, account, syncResult, syncDelegate, authToken, sharedPrefs, extras);

      latch.await(TIMEOUT_SECONDS, TimeUnit.SECONDS);
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
     * * Backoff and retry-after.
     * * Sync scheduling.
     * * Forcing syncs/interactive use.
     */
  }

  private ContentProviderClient getContentProviderClient(Context context) {
    final ContentResolver contentResolver = context.getContentResolver();
    final ContentProviderClient client = contentResolver.acquireContentProviderClient(ReadingListItems.CONTENT_URI);
    return client;
  }
}
