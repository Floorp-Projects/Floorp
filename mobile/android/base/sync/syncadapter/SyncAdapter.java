/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.syncadapter;

import java.io.IOException;
import java.net.URI;
import java.security.NoSuchAlgorithmException;
import java.util.Collection;
import java.util.concurrent.atomic.AtomicBoolean;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.AlreadySyncingException;
import org.mozilla.gecko.sync.BackoffHandler;
import org.mozilla.gecko.sync.CredentialException;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.PrefsBackoffHandler;
import org.mozilla.gecko.sync.SharedPreferencesClientsDataDelegate;
import org.mozilla.gecko.sync.SharedPreferencesNodeAssignmentCallback;
import org.mozilla.gecko.sync.Sync11Configuration;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.SyncConfigurationException;
import org.mozilla.gecko.sync.SyncConstants;
import org.mozilla.gecko.sync.SyncException;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.config.AccountPickler;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.BaseGlobalSessionCallback;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;
import org.mozilla.gecko.sync.net.ConnectionMonitorThread;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.sync.setup.SyncAccounts.SyncAccountParameters;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.content.AbstractThreadedSyncAdapter;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SyncResult;
import android.database.sqlite.SQLiteConstraintException;
import android.database.sqlite.SQLiteException;
import android.os.Bundle;

public class SyncAdapter extends AbstractThreadedSyncAdapter implements BaseGlobalSessionCallback {
  private static final String  LOG_TAG = "SyncAdapter";

  private static final int     BACKOFF_PAD_SECONDS = 5;
  public  static final int     MULTI_DEVICE_INTERVAL_MILLISECONDS = 5 * 60 * 1000;         // 5 minutes.
  public  static final int     SINGLE_DEVICE_INTERVAL_MILLISECONDS = 24 * 60 * 60 * 1000;  // 24 hours.

  private final Context        mContext;

  protected long syncStartTimestamp;

  protected volatile BackoffHandler backoffHandler;

  public SyncAdapter(Context context, boolean autoInitialize) {
    super(context, autoInitialize);
    mContext = context;
  }

  /**
   * Handle an exception: update stats, log errors, etc.
   * Wakes up sleeping threads by calling notifyMonitor().
   *
   * @param globalSession
   *          current global session, or null.
   * @param e
   *          Exception to handle.
   */
  protected void processException(final GlobalSession globalSession, final Exception e) {
    try {
      if (e instanceof SQLiteConstraintException) {
        Logger.error(LOG_TAG, "Constraint exception. Aborting sync.", e);
        syncResult.stats.numParseExceptions++;       // This is as good as we can do.
        return;
      }
      if (e instanceof SQLiteException) {
        Logger.error(LOG_TAG, "Couldn't open database (locked?). Aborting sync.", e);
        syncResult.stats.numIoExceptions++;
        return;
      }
      if (e instanceof OperationCanceledException) {
        Logger.error(LOG_TAG, "Operation canceled. Aborting sync.", e);
        return;
      }
      if (e instanceof AuthenticatorException) {
        syncResult.stats.numParseExceptions++;
        Logger.error(LOG_TAG, "AuthenticatorException. Aborting sync.", e);
        return;
      }
      if (e instanceof IOException) {
        syncResult.stats.numIoExceptions++;
        Logger.error(LOG_TAG, "IOException. Aborting sync.", e);
        e.printStackTrace();
        return;
      }

      // Blanket stats updating for SyncException subclasses.
      if (e instanceof SyncException) {
        ((SyncException) e).updateStats(globalSession, syncResult);
      } else {
        // Generic exception.
        syncResult.stats.numIoExceptions++;
      }

      if (e instanceof CredentialException.MissingAllCredentialsException) {
        // This is bad: either we couldn't fetch credentials, or the credentials
        // were totally blank. Most likely the user has two copies of Firefox
        // installed, and something is misbehaving.
        // Either way, disable this account.
        if (localAccount == null) {
          // Should not happen, but be safe.
          Logger.error(LOG_TAG, "No credentials attached to account. Aborting sync.");
          return;
        }

        Logger.error(LOG_TAG, "No credentials attached to account " + localAccount.name + ". Aborting sync.");
        try {
          SyncAccounts.setSyncAutomatically(localAccount, false);
        } catch (Exception ex) {
          Logger.error(LOG_TAG, "Unable to disable account " + localAccount.name + ".", ex);
        }
        return;
      }

      if (e instanceof CredentialException.MissingCredentialException) {
        Logger.error(LOG_TAG, "Credentials attached to account, but missing " +
            ((CredentialException.MissingCredentialException) e).missingCredential + ". Aborting sync.");
        return;
      }

      if (e instanceof CredentialException) {
        Logger.error(LOG_TAG, "Credentials attached to account were bad.");
        return;
      }

      // Bug 755638 - Uncaught SecurityException when attempting to sync multiple Fennecs
      // to the same Sync account.
      // Uncheck Sync checkbox because we cannot sync this instance.
      if (e instanceof SecurityException) {
        Logger.error(LOG_TAG, "SecurityException, multiple Fennecs. Disabling this instance.", e);
        SyncAccounts.backgroundSetSyncAutomatically(localAccount, false);
        return;
      }
      // Generic exception.
      Logger.error(LOG_TAG, "Unknown exception. Aborting sync.", e);
    } finally {
      notifyMonitor();
    }
  }

  @Override
  public void onSyncCanceled() {
    super.onSyncCanceled();
    // TODO: cancel the sync!
    // From the docs: "This will be invoked on a separate thread than the sync
    // thread and so you must consider the multi-threaded implications of the
    // work that you do in this method."
  }

  public Object syncMonitor = new Object();
  private SyncResult syncResult;

  protected Account localAccount;
  protected boolean thisSyncIsForced = false;
  protected SharedPreferences accountSharedPreferences;
  protected SharedPreferencesClientsDataDelegate clientsDataDelegate;
  protected SharedPreferencesNodeAssignmentCallback nodeAssignmentDelegate;

  /**
   * Request that no sync start right away.  A new sync won't start until
   * at least <code>backoff</code> milliseconds from now.
   *
   * Don't call this unless you are inside `run`.
   *
   * @param backoff time to wait in milliseconds.
   */
  @Override
  public void requestBackoff(final long backoff) {
    if (this.backoffHandler == null) {
      throw new IllegalStateException("No backoff handler: requesting backoff outside run()?");
    }
    if (backoff > 0) {
      // Fuzz the backoff time (up to 25% more) to prevent client lock-stepping; agrees with desktop.
      final long fuzzedBackoff = backoff + Math.round((double) backoff * 0.25d * Math.random());
      this.backoffHandler.extendEarliestNextRequest(System.currentTimeMillis() + fuzzedBackoff);
    }
  }

  @Override
  public boolean shouldBackOffStorage() {
    if (thisSyncIsForced) {
      /*
       * If the user asks us to sync, we should sync regardless. This path is
       * hit if the user force syncs and we restart a session after a
       * freshStart.
       */
      return false;
    }

    if (nodeAssignmentDelegate.wantNodeAssignment()) {
      /*
       * We recently had a 401 and we aborted the last sync. We should kick off
       * another sync to fetch a new node/weave cluster URL, since ours is
       * stale. If we have a user authentication error, the next sync will
       * determine that and will stop requesting node assignment, so this will
       * only force one abnormally scheduled sync.
       */
      return false;
    }

    if (this.backoffHandler == null) {
      throw new IllegalStateException("No backoff handler: checking backoff outside run()?");
    }
    return this.backoffHandler.delayMilliseconds() > 0;
  }

  /**
   * Asynchronously request an immediate sync, optionally syncing only the given
   * named stages.
   * <p>
   * Returns immediately.
   *
   * @param account
   *          the Android <code>Account</code> instance to sync.
   * @param stageNames
   *          stage names to sync, or <code>null</code> to sync all known stages.
   */
  public static void requestImmediateSync(final Account account, final String[] stageNames) {
    requestImmediateSync(account, stageNames, null);
  }

  /**
   * Asynchronously request an immediate sync, optionally syncing only the given
   * named stages.
   * <p>
   * Returns immediately.
   *
   * @param account
   *          the Android <code>Account</code> instance to sync.
   * @param stageNames
   *          stage names to sync, or <code>null</code> to sync all known stages.
   * @param moreExtras
   *          bundle of extras to give to the sync, or <code>null</code>
   */
  public static void requestImmediateSync(final Account account, final String[] stageNames, Bundle moreExtras) {
    if (account == null) {
      Logger.warn(LOG_TAG, "Not requesting immediate sync because Android Account is null.");
      return;
    }

    final Bundle extras = new Bundle();
    Utils.putStageNamesToSync(extras, stageNames, null);
    extras.putBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, true);

    if (moreExtras != null) {
      extras.putAll(moreExtras);
    }

    ContentResolver.requestSync(account, BrowserContract.AUTHORITY, extras);
  }

  @Override
  public void onPerformSync(final Account account,
                            final Bundle extras,
                            final String authority,
                            final ContentProviderClient provider,
                            final SyncResult syncResult) {
    syncStartTimestamp = System.currentTimeMillis();

    Logger.setThreadLogTag(SyncConstants.GLOBAL_LOG_TAG);
    Logger.resetLogging();
    Utils.reseedSharedRandom(); // Make sure we don't work with the same random seed for too long.

    // Set these so that we don't need to thread them through assorted calls and callbacks.
    this.syncResult   = syncResult;
    this.localAccount = account;

    SyncAccountParameters params;
    try {
      params = SyncAccounts.blockingFromAndroidAccountV0(mContext, AccountManager.get(mContext), this.localAccount);
    } catch (Exception e) {
      // Updates syncResult and (harmlessly) calls notifyMonitor().
      processException(null, e);
      return;
    }

    // params and the following fields are non-null at this point.
    final String username  = params.username; // Encoded with Utils.usernameFromAccount.
    final String password  = params.password;
    final String serverURL = params.serverURL;
    final String syncKey   = params.syncKey;

    final AtomicBoolean setNextSync = new AtomicBoolean(true);
    final SyncAdapter self = this;
    final Runnable runnable = new Runnable() {
      @Override
      public void run() {
        Logger.trace(LOG_TAG, "AccountManagerCallback invoked.");
        // TODO: N.B.: Future must not be used on the main thread.
        try {
          if (Logger.LOG_PERSONAL_INFORMATION) {
            Logger.pii(LOG_TAG, "Syncing account named " + account.name +
                " for authority " + authority + ".");
          } else {
            // Replace "foo@bar.com" with "XXX@XXX.XXX".
            Logger.info(LOG_TAG, "Syncing account named like " + Utils.obfuscateEmail(account.name) +
                " for authority " + authority + ".");
          }

          // We dump this information right away to help with debugging.
          Logger.debug(LOG_TAG, "Username: " + username);
          Logger.debug(LOG_TAG, "Server:   " + serverURL);
          if (Logger.LOG_PERSONAL_INFORMATION) {
            Logger.debug(LOG_TAG, "Password: " + password);
            Logger.debug(LOG_TAG, "Sync key: " + syncKey);
          } else {
            Logger.debug(LOG_TAG, "Password? " + (password != null));
            Logger.debug(LOG_TAG, "Sync key? " + (syncKey != null));
          }

          // Support multiple accounts by mapping each server/account pair to a branch of the
          // shared preferences space.
          final String product = GlobalConstants.BROWSER_INTENT_PACKAGE;
          final String profile = Constants.DEFAULT_PROFILE;
          final long version = SyncConfiguration.CURRENT_PREFS_VERSION;
          self.accountSharedPreferences = Utils.getSharedPreferences(mContext, product, username, serverURL, profile, version);
          self.clientsDataDelegate = new SharedPreferencesClientsDataDelegate(accountSharedPreferences);
          self.backoffHandler = new PrefsBackoffHandler(accountSharedPreferences, SyncConstants.BACKOFF_PREF_SUFFIX_11);
          final String nodeWeaveURL = Utils.nodeWeaveURL(serverURL, username);
          self.nodeAssignmentDelegate = new SharedPreferencesNodeAssignmentCallback(accountSharedPreferences, nodeWeaveURL);

          Logger.info(LOG_TAG,
              "Client is named '" + clientsDataDelegate.getClientName() + "'" +
              ", has client guid " + clientsDataDelegate.getAccountGUID() +
              ", and has " + clientsDataDelegate.getClientsCount() + " clients.");

          final boolean thisSyncIsForced = (extras != null) && (extras.getBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, false));
          final long delayMillis = backoffHandler.delayMilliseconds();
          boolean shouldSync = thisSyncIsForced || (delayMillis <= 0L);
          if (!shouldSync) {
            long remainingSeconds = delayMillis / 1000;
            syncResult.delayUntil = remainingSeconds + BACKOFF_PAD_SECONDS;
            setNextSync.set(false);
            self.notifyMonitor();
            return;
          }

          final String prefsPath = Utils.getPrefsPath(product, username, serverURL, profile, version);
          self.performSync(account, extras, authority, provider, syncResult,
              username, password, prefsPath, serverURL, syncKey);
        } catch (Exception e) {
          self.processException(null, e);
          return;
        }
      }
    };

    synchronized (syncMonitor) {
      // Perform the work in a new thread from within this synchronized block,
      // which allows us to be waiting on the monitor before the callback can
      // notify us in a failure case. Oh, concurrent programming.
      new Thread(runnable).start();

      // Start our stale connection monitor thread.
      ConnectionMonitorThread stale = new ConnectionMonitorThread();
      stale.start();

      Logger.trace(LOG_TAG, "Waiting on sync monitor.");
      try {
        syncMonitor.wait();

        if (setNextSync.get()) {
          long interval = getSyncInterval(clientsDataDelegate);
          long next = System.currentTimeMillis() + interval;

          if (thisSyncIsForced) {
            Logger.info(LOG_TAG, "Setting minimum next sync time to " + next + " (" + interval + "ms from now).");
            self.backoffHandler.setEarliestNextRequest(next);
          } else {
            Logger.info(LOG_TAG, "Extending minimum next sync time to " + next + " (" + interval + "ms from now).");
            self.backoffHandler.extendEarliestNextRequest(next);
          }
        }
        Logger.info(LOG_TAG, "Sync took " + Utils.formatDuration(syncStartTimestamp, System.currentTimeMillis()) + ".");
      } catch (InterruptedException e) {
        Logger.warn(LOG_TAG, "Waiting on sync monitor interrupted.", e);
      } finally {
        // And we're done with HTTP stuff.
        stale.shutdown();
      }
    }
  }

  public int getSyncInterval(ClientsDataDelegate clientsDataDelegate) {
    // Must have been a problem that means we can't access the Account.
    if (this.localAccount == null) {
      return SINGLE_DEVICE_INTERVAL_MILLISECONDS;
    }

    int clientsCount = clientsDataDelegate.getClientsCount();
    if (clientsCount <= 1) {
      return SINGLE_DEVICE_INTERVAL_MILLISECONDS;
    }

    return MULTI_DEVICE_INTERVAL_MILLISECONDS;
  }

  /**
   * Now that we have a sync key and password, go ahead and do the work.
   * @throws NoSuchAlgorithmException
   * @throws IllegalArgumentException
   * @throws SyncConfigurationException
   * @throws AlreadySyncingException
   * @throws NonObjectJSONException
   * @throws ParseException
   * @throws IOException
   * @throws CryptoException
   */
  protected void performSync(final Account account,
                             final Bundle extras,
                             final String authority,
                             final ContentProviderClient provider,
                             final SyncResult syncResult,
                             final String username,
                             final String password,
                             final String prefsPath,
                             final String serverURL,
                             final String syncKey)
                                 throws NoSuchAlgorithmException,
                                        SyncConfigurationException,
                                        IllegalArgumentException,
                                        AlreadySyncingException,
                                        IOException, ParseException,
                                        NonObjectJSONException, CryptoException {
    Logger.trace(LOG_TAG, "Performing sync.");

    /**
     * Bug 769745: pickle Sync account parameters to JSON file. Un-pickle in
     * <code>SyncAccounts.syncAccountsExist</code>.
     */
    try {
      // Constructor can throw on nulls, which should not happen -- but let's be safe.
      final SyncAccountParameters params = new SyncAccountParameters(mContext, null,
        account.name, // Un-encoded, like "test@mozilla.com".
        syncKey,
        password,
        serverURL,
        null, // We'll re-fetch cluster URL; not great, but not harmful.
        clientsDataDelegate.getClientName(),
        clientsDataDelegate.getAccountGUID());

      // Bug 772971: pickle Sync account parameters on background thread to
      // avoid strict mode warnings.
      ThreadPool.run(new Runnable() {
        @Override
        public void run() {
          final boolean syncAutomatically = ContentResolver.getSyncAutomatically(account, authority);
          try {
            AccountPickler.pickle(mContext, Constants.ACCOUNT_PICKLE_FILENAME, params, syncAutomatically);
          } catch (Exception e) {
            // Should never happen, but we really don't want to die in a background thread.
            Logger.warn(LOG_TAG, "Got exception pickling current account details; ignoring.", e);
          }
        }
      });
    } catch (IllegalArgumentException e) {
      // Do nothing.
    }

    if (username == null) {
      throw new IllegalArgumentException("username must not be null.");
    }

    if (syncKey == null) {
      throw new SyncConfigurationException();
    }

    final KeyBundle keyBundle = new KeyBundle(username, syncKey);

    if (keyBundle == null ||
        keyBundle.getEncryptionKey() == null ||
        keyBundle.getHMACKey() == null) {
      throw new SyncConfigurationException();
    }

    final AuthHeaderProvider authHeaderProvider = new BasicAuthHeaderProvider(username, password);
    final SharedPreferences prefs = getContext().getSharedPreferences(prefsPath, Utils.SHARED_PREFERENCES_MODE);
    final SyncConfiguration config = new Sync11Configuration(username, authHeaderProvider, prefs, keyBundle);

    Collection<String> knownStageNames = SyncConfiguration.validEngineNames();
    config.stagesToSync = Utils.getStagesToSyncFromBundle(knownStageNames, extras);

    GlobalSession globalSession = new GlobalSession(config, this, this.mContext, clientsDataDelegate, nodeAssignmentDelegate);
    globalSession.start();
  }

  private void notifyMonitor() {
    synchronized (syncMonitor) {
      Logger.trace(LOG_TAG, "Notifying sync monitor.");
      syncMonitor.notifyAll();
    }
  }

  // Implementing GlobalSession callbacks.
  @Override
  public void handleError(GlobalSession globalSession, Exception ex) {
    Logger.info(LOG_TAG, "GlobalSession indicated error.");
    this.processException(globalSession, ex);
  }

  @Override
  public void handleAborted(GlobalSession globalSession, String reason) {
    Logger.warn(LOG_TAG, "Sync aborted: " + reason);
    notifyMonitor();
  }

  @Override
  public void handleSuccess(GlobalSession globalSession) {
    Logger.info(LOG_TAG, "GlobalSession indicated success.");
    globalSession.config.persistToPrefs();
    notifyMonitor();
  }

  @Override
  public void handleStageCompleted(Stage currentState,
                                   GlobalSession globalSession) {
    Logger.trace(LOG_TAG, "Stage completed: " + currentState);
  }

  @Override
  public void informUnauthorizedResponse(GlobalSession session, URI oldClusterURL) {
    nodeAssignmentDelegate.setClusterURLIsStale(true);
  }

  @Override
  public void informUpgradeRequiredResponse(final GlobalSession session) {
    final AccountManager manager = AccountManager.get(mContext);
    final Account toDisable      = localAccount;
    if (toDisable == null || manager == null) {
      Logger.warn(LOG_TAG, "Attempting to disable account, but null found.");
      return;
    }
    // Sync needs to be upgraded. Don't automatically sync anymore.
    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        manager.setUserData(toDisable, Constants.DATA_ENABLE_ON_UPGRADE, "1");
        SyncAccounts.setSyncAutomatically(toDisable, false);
      }
    });
  }
}
