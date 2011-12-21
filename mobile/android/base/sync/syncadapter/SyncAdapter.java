/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chenxia Liu <liuche@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.sync.syncadapter;

import java.io.IOException;
import java.security.NoSuchAlgorithmException;
import java.util.concurrent.TimeUnit;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.AlreadySyncingException;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.SyncConfigurationException;
import org.mozilla.gecko.sync.SyncException;
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
import android.content.SyncResult;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;

public class SyncAdapter extends AbstractThreadedSyncAdapter implements GlobalSessionCallback {

  private static final String  LOG_TAG = "SyncAdapter";
  private final AccountManager mAccountManager;
  private final Context        mContext;

  public SyncAdapter(Context context, boolean autoInitialize) {
    super(context, autoInitialize);
    mContext = context;
    mAccountManager = AccountManager.get(context);
  }

  private void handleException(Exception e, SyncResult syncResult) {
    if (e instanceof OperationCanceledException) {
      Log.e(LOG_TAG, "Operation canceled. Aborting sync.");
      e.printStackTrace();
      return;
    }
    if (e instanceof AuthenticatorException) {
      syncResult.stats.numParseExceptions++;
      Log.e(LOG_TAG, "AuthenticatorException. Aborting sync.");
      e.printStackTrace();
      return;
    }
    if (e instanceof IOException) {
      syncResult.stats.numIoExceptions++;
      Log.e(LOG_TAG, "IOException. Aborting sync.");
      e.printStackTrace();
      return;
    }
    syncResult.stats.numIoExceptions++;
    Log.e(LOG_TAG, "Unknown exception. Aborting sync.");
    e.printStackTrace();
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

  public boolean shouldPerformSync = false;

  @Override
  public void onPerformSync(final Account account,
                            final Bundle extras,
                            final String authority,
                            final ContentProviderClient provider,
                            final SyncResult syncResult) {

    // TODO: don't clear the auth token unless we have a sync error.
    Log.i(LOG_TAG, "Got onPerformSync. Extras bundle is " + extras);
    Log.d(LOG_TAG, "Extras clusterURL: " + extras.getString("clusterURL"));
    Log.i(LOG_TAG, "Account name: " + account.name);
    if (!shouldPerformSync) {
      Log.i(LOG_TAG, "Not performing sync.");
      return;
    }
    Log.i(LOG_TAG, "XXX CLEARING AUTH TOKEN XXX");
    invalidateAuthToken(account);

    final SyncAdapter self = this;
    AccountManagerCallback<Bundle> callback = new AccountManagerCallback<Bundle>() {
      @Override
      public void run(AccountManagerFuture<Bundle> future) {
        Log.i(LOG_TAG, "AccountManagerCallback invoked.");
        // TODO: N.B.: Future must not be used on the main thread.
        try {
          Bundle bundle      = future.getResult(60L, TimeUnit.SECONDS);
          String username    = bundle.getString(Constants.OPTION_USERNAME);
          String syncKey     = bundle.getString(Constants.OPTION_SYNCKEY);
          String serverURL   = bundle.getString(Constants.OPTION_SERVER);
          String password    = bundle.getString(AccountManager.KEY_AUTHTOKEN);
          Log.d(LOG_TAG, "Username: " + username);
          Log.d(LOG_TAG, "Server:   " + serverURL);
          Log.d(LOG_TAG, "Password: " + password);  // TODO: remove
          Log.d(LOG_TAG, "Key:      " + syncKey);   // TODO: remove
          if (password == null) {
            Log.e(LOG_TAG, "No password: aborting sync.");
            return;
          }
          if (syncKey == null) {
            Log.e(LOG_TAG, "No Sync Key: aborting sync.");
            return;
          }
          KeyBundle keyBundle = new KeyBundle(username, syncKey);
          self.performSync(account, extras, authority, provider, syncResult,
              username, password, serverURL, keyBundle);
        } catch (Exception e) {
          self.handleException(e, syncResult);
          return;
        }
      }
    };
    Handler handler = null;
    getAuthToken(account, callback, handler);
    synchronized (syncMonitor) {
      try {
        Log.i(LOG_TAG, "Waiting on sync monitor.");
        syncMonitor.wait();
      } catch (InterruptedException e) {
        Log.i(LOG_TAG, "Waiting on sync monitor interrupted.", e);
      }
    }
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
   */
  protected void performSync(Account account, Bundle extras, String authority,
                             ContentProviderClient provider,
                             SyncResult syncResult,
                             String username, String password,
                             String serverURL,
                             KeyBundle keyBundle)
                                 throws NoSuchAlgorithmException,
                                        SyncConfigurationException,
                                        IllegalArgumentException,
                                        AlreadySyncingException,
                                        IOException, ParseException,
                                        NonObjectJSONException {
    Log.i(LOG_TAG, "Performing sync.");
    this.syncResult = syncResult;
    // TODO: default serverURL.
    GlobalSession globalSession = new GlobalSession(SyncConfiguration.DEFAULT_USER_API,
                                                    serverURL, username, password, keyBundle,
                                                    this, this.mContext, null);

    globalSession.start();

  }

  private void notifyMonitor() {
    synchronized (syncMonitor) {
      Log.i(LOG_TAG, "Notifying sync monitor.");
      syncMonitor.notify();
    }
  }

  // Implementing GlobalSession callbacks.
  @Override
  public void handleError(GlobalSession globalSession, Exception ex) {
    Log.i(LOG_TAG, "GlobalSession indicated error.");
    this.updateStats(globalSession, ex);
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
    notifyMonitor();
  }

  @Override
  public void handleStageCompleted(Stage currentState,
                                   GlobalSession globalSession) {
    Log.i(LOG_TAG, "Stage completed: " + currentState);
  }
}

//        try {
//            // see if we already have a sync-state attached to this account. By handing
//            // This value to the server, we can just get the contacts that have
//            // been updated on the server-side since our last sync-up
//            long lastSyncMarker = getServerSyncMarker(account);
//
//            // By default, contacts from a 3rd party provider are hidden in the contacts
//            // list. So let's set the flag that causes them to be visible, so that users
//            // can actually see these contacts.
//            if (lastSyncMarker == 0) {
//                ContactManager.setAccountContactsVisibility(getContext(), account, true);
//            }
//
//            List<RawContact> dirtyContacts;
//            List<RawContact> updatedContacts;
//
//            // Use the account manager to request the AuthToken we'll need
//            // to talk to our sample server.  If we don't have an AuthToken
//            // yet, this could involve a round-trip to the server to request
//            // and AuthToken.
//            final String authtoken = mAccountManager.blockingGetAuthToken(account,
//                    Constants.AUTHTOKEN_TYPE, NOTIFY_AUTH_FAILURE);
//
//            // Make sure that the sample group exists
//            final long groupId = ContactManager.ensureSampleGroupExists(mContext, account);
//
//            // Find the local 'dirty' contacts that we need to tell the server about...
//            // Find the local users that need to be sync'd to the server...
//            dirtyContacts = ContactManager.getDirtyContacts(mContext, account);
//
//            // Send the dirty contacts to the server, and retrieve the server-side changes
//            updatedContacts = NetworkUtilities.syncContacts(account, authtoken,
//                    lastSyncMarker, dirtyContacts);
//
//            // Update the local contacts database with the changes. updateContacts()
//            // returns a syncState value that indicates the high-water-mark for
//            // the changes we received.
//            Log.d(TAG, "Calling contactManager's sync contacts");
//            long newSyncState = ContactManager.updateContacts(mContext,
//                    account.name,
//                    updatedContacts,
//                    groupId,
//                    lastSyncMarker);
//
//            // This is a demo of how you can update IM-style status messages
//            // for contacts on the client. This probably won't apply to
//            // 2-way contact sync providers - it's more likely that one-way
//            // sync providers (IM clients, social networking apps, etc) would
//            // use this feature.
//            ContactManager.updateStatusMessages(mContext, updatedContacts);
//
//            // Save off the new sync marker. On our next sync, we only want to receive
//            // contacts that have changed since this sync...
//            setServerSyncMarker(account, newSyncState);
//
//            if (dirtyContacts.size() > 0) {
//                ContactManager.clearSyncFlags(mContext, dirtyContacts);
//            }
//
//        } catch (final AuthenticatorException e) {
//            Log.e(TAG, "AuthenticatorException", e);
//            syncResult.stats.numParseExceptions++;
//        } catch (final OperationCanceledException e) {
//            Log.e(TAG, "OperationCanceledExcetpion", e);
//        } catch (final IOException e) {
//            Log.e(TAG, "IOException", e);
//            syncResult.stats.numIoExceptions++;
//        } catch (final AuthenticationException e) {
//            Log.e(TAG, "AuthenticationException", e);
//            syncResult.stats.numAuthExceptions++;
//        } catch (final ParseException e) {
//            Log.e(TAG, "ParseException", e);
//            syncResult.stats.numParseExceptions++;
//        } catch (final JSONException e) {
//            Log.e(TAG, "JSONException", e);
//            syncResult.stats.numParseExceptions++;
//        }
