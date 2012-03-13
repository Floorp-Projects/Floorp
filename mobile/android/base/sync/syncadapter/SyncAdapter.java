/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.syncadapter;

import java.io.IOException;
import java.net.URI;
import java.security.NoSuchAlgorithmException;
import java.util.concurrent.TimeUnit;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.AlreadySyncingException;
import org.mozilla.gecko.sync.GlobalConstants;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.SyncConfigurationException;
import org.mozilla.gecko.sync.SyncException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.delegates.GlobalSessionCallback;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.content.AbstractThreadedSyncAdapter;
import android.content.ContentProviderClient;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.SyncResult;
import android.database.sqlite.SQLiteConstraintException;
import android.database.sqlite.SQLiteException;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;

public class SyncAdapter extends AbstractThreadedSyncAdapter implements GlobalSessionCallback, ClientsDataDelegate {
  private static final String  LOG_TAG = "SyncAdapter";

  private static final String  PREFS_EARLIEST_NEXT_SYNC = "earliestnextsync";
  private static final String  PREFS_INVALIDATE_AUTH_TOKEN = "invalidateauthtoken";
  private static final String  PREFS_CLUSTER_URL_IS_STALE = "clusterurlisstale";

  private static final int     SHARED_PREFERENCES_MODE = 0;
  private static final int     BACKOFF_PAD_SECONDS = 5;
  private static final int     MINIMUM_SYNC_INTERVAL_MILLISECONDS = 5 * 60 * 1000;   // 5 minutes.

  private final AccountManager mAccountManager;
  private final Context        mContext;

  public SyncAdapter(Context context, boolean autoInitialize) {
    super(context, autoInitialize);
    mContext = context;
    Log.d(LOG_TAG, "AccountManager.get(" + mContext + ")");
    mAccountManager = AccountManager.get(context);
  }

  private SharedPreferences getGlobalPrefs() {
    return mContext.getSharedPreferences("sync.prefs.global", SHARED_PREFERENCES_MODE);
  }

  /**
   * Backoff.
   */
  public synchronized long getEarliestNextSync() {
    SharedPreferences sharedPreferences = getGlobalPrefs();
    return sharedPreferences.getLong(PREFS_EARLIEST_NEXT_SYNC, 0);
  }
  public synchronized void setEarliestNextSync(long next) {
    SharedPreferences sharedPreferences = getGlobalPrefs();
    Editor edit = sharedPreferences.edit();
    edit.putLong(PREFS_EARLIEST_NEXT_SYNC, next);
    edit.commit();
  }
  public synchronized void extendEarliestNextSync(long next) {
    SharedPreferences sharedPreferences = getGlobalPrefs();
    if (sharedPreferences.getLong(PREFS_EARLIEST_NEXT_SYNC, 0) >= next) {
      return;
    }
    Editor edit = sharedPreferences.edit();
    edit.putLong(PREFS_EARLIEST_NEXT_SYNC, next);
    edit.commit();
  }

  public synchronized boolean getShouldInvalidateAuthToken() {
    SharedPreferences sharedPreferences = getGlobalPrefs();
    return sharedPreferences.getBoolean(PREFS_INVALIDATE_AUTH_TOKEN, false);
  }
  public synchronized void clearShouldInvalidateAuthToken() {
    SharedPreferences sharedPreferences = getGlobalPrefs();
    Editor edit = sharedPreferences.edit();
    edit.remove(PREFS_INVALIDATE_AUTH_TOKEN);
    edit.commit();
  }
  public synchronized void setShouldInvalidateAuthToken() {
    SharedPreferences sharedPreferences = getGlobalPrefs();
    Editor edit = sharedPreferences.edit();
    edit.putBoolean(PREFS_INVALIDATE_AUTH_TOKEN, true);
    edit.commit();
  }

  private void handleException(Exception e, SyncResult syncResult) {
    setShouldInvalidateAuthToken();
    try {
      if (e instanceof SQLiteConstraintException) {
        Log.e(LOG_TAG, "Constraint exception. Aborting sync.", e);
        syncResult.stats.numParseExceptions++;       // This is as good as we can do.
        return;
      }
      if (e instanceof SQLiteException) {
        Log.e(LOG_TAG, "Couldn't open database (locked?). Aborting sync.", e);
        syncResult.stats.numIoExceptions++;
        return;
      }
      if (e instanceof OperationCanceledException) {
        Log.e(LOG_TAG, "Operation canceled. Aborting sync.", e);
        return;
      }
      if (e instanceof AuthenticatorException) {
        syncResult.stats.numParseExceptions++;
        Log.e(LOG_TAG, "AuthenticatorException. Aborting sync.", e);
        return;
      }
      if (e instanceof IOException) {
        syncResult.stats.numIoExceptions++;
        Log.e(LOG_TAG, "IOException. Aborting sync.", e);
        e.printStackTrace();
        return;
      }
      syncResult.stats.numIoExceptions++;
      Log.e(LOG_TAG, "Unknown exception. Aborting sync.", e);
    } finally {
      notifyMonitor();
    }
  }

  private AccountManagerFuture<Bundle> getAuthToken(final Account account,
                            AccountManagerCallback<Bundle> callback,
                            Handler handler) {
    return mAccountManager.getAuthToken(account, Constants.AUTHTOKEN_TYPE_PLAIN, true, callback, handler);
  }

  private void invalidateAuthToken(Account account) {
    AccountManagerFuture<Bundle> future = getAuthToken(account, null, null);
    String token;
    try {
      token = future.getResult().getString(AccountManager.KEY_AUTHTOKEN);
      mAccountManager.invalidateAuthToken(Constants.ACCOUNTTYPE_SYNC, token);
    } catch (Exception e) {
      Log.e(LOG_TAG, "Couldn't invalidate auth token: " + e);
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

  private Account localAccount;

  /**
   * Return the number of milliseconds until we're allowed to sync again,
   * or 0 if now is fine.
   */
  public long delayMilliseconds() {
    long earliestNextSync = getEarliestNextSync();
    if (earliestNextSync <= 0) {
      return 0;
    }
    long now = System.currentTimeMillis();
    return Math.max(0, earliestNextSync - now);
  }

  @Override
  public boolean shouldBackOff() {
    if (wantNodeAssignment()) {
      /*
       * We recently had a 401 and we aborted the last sync. We should kick off
       * another sync to fetch a new node/weave cluster URL, since ours is
       * stale. If we have a user authentication error, the next sync will
       * determine that and will stop requesting node assignment, so this will
       * only force one abnormally scheduled sync.
       */
      return false;
    }

    return delayMilliseconds() > 0;
  }

  @Override
  public void onPerformSync(final Account account,
                            final Bundle extras,
                            final String authority,
                            final ContentProviderClient provider,
                            final SyncResult syncResult) {
    Utils.reseedSharedRandom(); // Make sure we don't work with the same random seed for too long.

    boolean force = (extras != null) && (extras.getBoolean("force", false));
    long delay = delayMilliseconds();
    if (delay > 0) {
      if (force) {
        Log.i(LOG_TAG, "Forced sync: overruling remaining backoff of " + delay + "ms.");
      } else {
        Log.i(LOG_TAG, "Not syncing: must wait another " + delay + "ms.");
        long remainingSeconds = delay / 1000;
        syncResult.delayUntil = remainingSeconds + BACKOFF_PAD_SECONDS;
        return;
      }
    }

    // TODO: don't clear the auth token unless we have a sync error.
    Log.i(LOG_TAG, "Got onPerformSync. Extras bundle is " + extras);
    Log.i(LOG_TAG, "Account name: " + account.name);

    // TODO: don't always invalidate; use getShouldInvalidateAuthToken.
    // However, this fixes Bug 716815, so it'll do for now.
    Log.d(LOG_TAG, "Invalidating auth token.");
    invalidateAuthToken(account);

    final SyncAdapter self = this;
    final AccountManagerCallback<Bundle> callback = new AccountManagerCallback<Bundle>() {
      @Override
      public void run(AccountManagerFuture<Bundle> future) {
        Log.i(LOG_TAG, "AccountManagerCallback invoked.");
        // TODO: N.B.: Future must not be used on the main thread.
        try {
          Bundle bundle = future.getResult(60L, TimeUnit.SECONDS);
          if (bundle.containsKey("KEY_INTENT")) {
            Log.w(LOG_TAG, "KEY_INTENT included in AccountManagerFuture bundle. Problem?");
          }
          String username  = bundle.getString(Constants.OPTION_USERNAME);
          String syncKey   = bundle.getString(Constants.OPTION_SYNCKEY);
          String serverURL = bundle.getString(Constants.OPTION_SERVER);
          String password  = bundle.getString(AccountManager.KEY_AUTHTOKEN);
          Log.d(LOG_TAG, "Username: " + username);
          Log.d(LOG_TAG, "Server:   " + serverURL);
          Log.d(LOG_TAG, "Password? " + (password != null));
          Log.d(LOG_TAG, "Key?      " + (syncKey != null));
          if (password == null) {
            Log.e(LOG_TAG, "No password: aborting sync.");
            syncResult.stats.numAuthExceptions++;
            notifyMonitor();
            return;
          }
          if (syncKey == null) {
            Log.e(LOG_TAG, "No Sync Key: aborting sync.");
            syncResult.stats.numAuthExceptions++;
            notifyMonitor();
            return;
          }
          KeyBundle keyBundle = new KeyBundle(username, syncKey);

          // Support multiple accounts by mapping each server/account pair to a branch of the
          // shared preferences space.
          String prefsPath = Utils.getPrefsPath(username, serverURL);
          self.performSync(account, extras, authority, provider, syncResult,
              username, password, prefsPath, serverURL, keyBundle);
        } catch (Exception e) {
          self.handleException(e, syncResult);
          return;
        }
      }
    };

    final Handler handler = null;
    final Runnable fetchAuthToken = new Runnable() {
      @Override
      public void run() {
        getAuthToken(account, callback, handler);
      }
    };
    synchronized (syncMonitor) {
      // Perform the work in a new thread from within this synchronized block,
      // which allows us to be waiting on the monitor before the callback can
      // notify us in a failure case. Oh, concurrent programming.
      new Thread(fetchAuthToken).start();

      Log.i(LOG_TAG, "Waiting on sync monitor.");
      try {
        syncMonitor.wait();
        long next = System.currentTimeMillis() + MINIMUM_SYNC_INTERVAL_MILLISECONDS;
        Log.i(LOG_TAG, "Setting minimum next sync time to " + next);
        extendEarliestNextSync(next);
      } catch (InterruptedException e) {
        Log.i(LOG_TAG, "Waiting on sync monitor interrupted.", e);
      }
    }
 }


  /**
   * Now that we have a sync key and password, go ahead and do the work.
   * @param prefsPath TODO
   * @throws NoSuchAlgorithmException
   * @throws IllegalArgumentException
   * @throws SyncConfigurationException
   * @throws AlreadySyncingException
   * @throws NonObjectJSONException
   * @throws ParseException
   * @throws IOException
   */
  protected void performSync(Account account, Bundle extras, String authority,
                             ContentProviderClient provider,
                             SyncResult syncResult,
                             String username, String password,
                             String prefsPath,
                             String serverURL, KeyBundle keyBundle)
                                 throws NoSuchAlgorithmException,
                                        SyncConfigurationException,
                                        IllegalArgumentException,
                                        AlreadySyncingException,
                                        IOException, ParseException,
                                        NonObjectJSONException {
    Log.i(LOG_TAG, "Performing sync.");
    this.syncResult   = syncResult;
    this.localAccount = account;
    // TODO: default serverURL.
    GlobalSession globalSession = new GlobalSession(SyncConfiguration.DEFAULT_USER_API,
                                                    serverURL, username, password, prefsPath,
                                                    keyBundle, this, this.mContext, extras, this);

    globalSession.start();
  }

  private void notifyMonitor() {
    synchronized (syncMonitor) {
      Log.i(LOG_TAG, "Notifying sync monitor.");
      syncMonitor.notifyAll();
    }
  }

  // Implementing GlobalSession callbacks.
  @Override
  public void handleError(GlobalSession globalSession, Exception ex) {
    Log.i(LOG_TAG, "GlobalSession indicated error. Flagging auth token as invalid, just in case.");
    setShouldInvalidateAuthToken();
    this.updateStats(globalSession, ex);
    notifyMonitor();
  }

  @Override
  public void handleAborted(GlobalSession globalSession, String reason) {
    Log.w(LOG_TAG, "Sync aborted: " + reason);
    notifyMonitor();
  }

  /**
   * Introspect the exception, incrementing the appropriate stat counters.
   * TODO: increment number of inserts, deletes, conflicts.
   *
   * @param globalSession
   * @param ex
   */
  private void updateStats(GlobalSession globalSession,
                           Exception ex) {
    if (ex instanceof SyncException) {
      ((SyncException) ex).updateStats(globalSession, syncResult);
    }
    // TODO: non-SyncExceptions.
    // TODO: wouldn't it be nice to update stats for *every* exception we get?
  }

  @Override
  public void handleSuccess(GlobalSession globalSession) {
    Log.i(LOG_TAG, "GlobalSession indicated success.");
    Log.i(LOG_TAG, "Prefs target: " + globalSession.config.prefsPath);
    globalSession.config.persistToPrefs();
    notifyMonitor();
  }

  @Override
  public void handleStageCompleted(Stage currentState,
                                   GlobalSession globalSession) {
    Log.i(LOG_TAG, "Stage completed: " + currentState);
  }

  @Override
  public void requestBackoff(long backoff) {
    if (backoff > 0) {
      this.extendEarliestNextSync(System.currentTimeMillis() + backoff);
    }
  }

  @Override
  public synchronized String getAccountGUID() {
    String accountGUID = mAccountManager.getUserData(localAccount, Constants.ACCOUNT_GUID);
    if (accountGUID == null) {
      accountGUID = Utils.generateGuid();
      mAccountManager.setUserData(localAccount, Constants.ACCOUNT_GUID, accountGUID);
    }
    return accountGUID;
  }

  @Override
  public synchronized String getClientName() {
    String clientName = mAccountManager.getUserData(localAccount, Constants.CLIENT_NAME);
    if (clientName == null) {
      clientName = GlobalConstants.PRODUCT_NAME + " on " + android.os.Build.MODEL;
      mAccountManager.setUserData(localAccount, Constants.CLIENT_NAME, clientName);
    }
    return clientName;
  }

  @Override
  public synchronized void setClientsCount(int clientsCount) {
    mAccountManager.setUserData(localAccount, Constants.NUM_CLIENTS,
        Integer.toString(clientsCount));
  }

  @Override
  public synchronized int getClientsCount() {
    String clientsCount = mAccountManager.getUserData(localAccount, Constants.NUM_CLIENTS);
    if (clientsCount == null) {
      clientsCount = "0";
      mAccountManager.setUserData(localAccount, Constants.NUM_CLIENTS, clientsCount);
    }
    return Integer.parseInt(clientsCount);
  }

  public synchronized boolean getClusterURLIsStale() {
    SharedPreferences sharedPreferences = getGlobalPrefs();
    return sharedPreferences.getBoolean(PREFS_CLUSTER_URL_IS_STALE, false);
  }

  public synchronized void setClusterURLIsStale(boolean clusterURLIsStale) {
    SharedPreferences sharedPreferences = getGlobalPrefs();
    Editor edit = sharedPreferences.edit();
    edit.putBoolean(PREFS_CLUSTER_URL_IS_STALE, clusterURLIsStale);
    edit.commit();
  }

  @Override
  public boolean wantNodeAssignment() {
    return getClusterURLIsStale();
  }

  @Override
  public void informNodeAuthenticationFailed(GlobalSession session, URI failedClusterURL) {
    // TODO: communicate to the user interface that we need a new user password!
    // TODO: only freshen the cluster URL (better yet, forget the cluster URL) after the user has provided new credentials.
    setClusterURLIsStale(false);
  }

  @Override
  public void informNodeAssigned(GlobalSession session, URI oldClusterURL, URI newClusterURL) {
    setClusterURLIsStale(false);
  }

  @Override
  public void informUnauthorizedResponse(GlobalSession session, URI oldClusterURL) {
    setClusterURLIsStale(true);
  }
}
