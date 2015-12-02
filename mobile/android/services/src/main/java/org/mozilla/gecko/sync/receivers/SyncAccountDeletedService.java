/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.receivers;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.Sync11Configuration;
import org.mozilla.gecko.sync.SyncConstants;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.config.AccountPickler;
import org.mozilla.gecko.sync.config.ClientRecordTerminator;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;
import org.mozilla.gecko.sync.repositories.android.FennecTabsRepository;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.SyncAccounts.SyncAccountParameters;

import android.accounts.AccountManager;
import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;

public class SyncAccountDeletedService extends IntentService {
  public static final String LOG_TAG = "SyncAccountDeletedService";

  public SyncAccountDeletedService() {
    super(LOG_TAG);
  }

  @Override
  protected void onHandleIntent(Intent intent) {
    // Intent can, in theory, be null. Bug 1025937.
    if (intent == null) {
      Logger.debug(LOG_TAG, "Short-circuiting on null intent.");
      return;
    }

    final Context context = this;

    long intentVersion = intent.getLongExtra(Constants.JSON_KEY_VERSION, 0);
    long expectedVersion = SyncConstants.SYNC_ACCOUNT_DELETED_INTENT_VERSION;
    if (intentVersion != expectedVersion) {
      Logger.warn(LOG_TAG, "Intent malformed: version " + intentVersion + " given but version " + expectedVersion + "expected. " +
          "Not cleaning up after deleted Account.");
      return;
    }

    String accountName = intent.getStringExtra(Constants.JSON_KEY_ACCOUNT); // Android Account name, not Sync encoded account name.
    if (accountName == null) {
      Logger.warn(LOG_TAG, "Intent malformed: no account name given. Not cleaning up after deleted Account.");
      return;
    }

    // Delete the Account pickle.
    Logger.info(LOG_TAG, "Sync account named " + accountName + " being removed; " +
        "deleting saved pickle file '" + Constants.ACCOUNT_PICKLE_FILENAME + "'.");
    deletePickle(context);

    SyncAccountParameters params;
    try {
      String payload = intent.getStringExtra(Constants.JSON_KEY_PAYLOAD);
      if (payload == null) {
        Logger.warn(LOG_TAG, "Intent malformed: no payload given. Not deleting client record.");
        return;
      }
      ExtendedJSONObject o = ExtendedJSONObject.parseJSONObject(payload);
      params = new SyncAccountParameters(context, AccountManager.get(context), o);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception fetching account parameters from intent data; not deleting client record.");
      return;
    }

    // Bug 770785: delete the Account's client record.
    Logger.info(LOG_TAG, "Account named " + accountName + " being removed; " +
        "deleting client record from server.");
    deleteClientRecord(context, accountName, params.password, params.serverURL);

    // Delete client database and non-local tabs.
    Logger.info(LOG_TAG, "Deleting the entire clients database and non-local tabs");
    FennecTabsRepository.deleteNonLocalClientsAndTabs(context);
  }

  public static void deletePickle(final Context context) {
    try {
      AccountPickler.deletePickle(context, Constants.ACCOUNT_PICKLE_FILENAME);
    } catch (Exception e) {
      // This should never happen, but we really don't want to die in a background thread.
      Logger.warn(LOG_TAG, "Got exception deleting saved pickle file; ignoring.", e);
    }
  }

  public static void deleteClientRecord(final Context context, final String accountName,
      final String password, final String serverURL) {
    String encodedUsername;
    try {
      encodedUsername = Utils.usernameFromAccount(accountName);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception deleting client record from server; ignoring.", e);
      return;
    }

    if (accountName == null || encodedUsername == null || password == null || serverURL == null) {
      Logger.warn(LOG_TAG, "Account parameters were null; not deleting client record from server.");
      return;
    }

    // This is not exactly modular. We need to get some information about
    // the account, namely the current clusterURL and client GUID, and we
    // extract it by hand. We're not worried about the Account being
    // deleted out from under us since the prefs remain even after Account
    // deletion.
    final String product = GlobalConstants.BROWSER_INTENT_PACKAGE;
    final String profile = Constants.DEFAULT_PROFILE;
    final long version = SyncConfiguration.CURRENT_PREFS_VERSION;

    SharedPreferences prefs;
    try {
      prefs = Utils.getSharedPreferences(context, product, encodedUsername, serverURL, profile, version);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Caught exception fetching preferences; not deleting client record from server.", e);
      return;
    }

    try {
      final String clientGUID = prefs.getString(SyncConfiguration.PREF_ACCOUNT_GUID, null);
      if (clientGUID == null) {
        Logger.warn(LOG_TAG, "Client GUID was null; not deleting client record from server.");
        return;
      }

      BasicAuthHeaderProvider authHeaderProvider = new BasicAuthHeaderProvider(encodedUsername, password);
      SyncConfiguration configuration = new Sync11Configuration(encodedUsername, authHeaderProvider, prefs);
      if (configuration.getClusterURL() == null) {
        Logger.warn(LOG_TAG, "Cluster URL was null; not deleting client record from server.");
        return;
      }

      try {
        ClientRecordTerminator.deleteClientRecord(configuration, clientGUID);
      } catch (Exception e) {
        // This should never happen, but we really don't want to die in a background thread.
        Logger.warn(LOG_TAG, "Got exception deleting client record from server; ignoring.", e);
      }
    } finally {
      // Finally, a good place to do this.
      prefs.edit().clear().commit();
    }
  }
}
