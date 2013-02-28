/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.config;

import java.io.FileOutputStream;
import java.io.PrintStream;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.sync.setup.SyncAccounts.SyncAccountParameters;

import android.accounts.Account;
import android.content.Context;

/**
 * Bug 768102: Android deletes Account objects when the Authenticator that owns
 * the Account disappears. This happens when an App is installed to the SD card
 * and the SD card is un-mounted or the device is rebooted.
 * <p>
 * Bug 769745: Work around this by pickling the current Sync account data every
 * sync.
 * <p>
 * Bug 735842: Work around this by un-pickling when we check if Sync accounts
 * exist (called from Fennec).
 * <p>
 * Android just doesn't support installing Apps that define long-lived Services
 * and/or own Account types onto the SD card. The documentation says not to do
 * it. There are hordes of developers who want to do it, and have tried to
 * register for almost every "package installation changed" broadcast intent
 * that Android supports. They all explicitly state that the package that has
 * changed does *not* receive the broadcast intent, thereby preventing an App
 * from re-establishing its state.
 * <p>
 * <a href="http://developer.android.com/guide/topics/data/install-location.html">Reference.</a>
 * <p>
 * <b>Quote</b>: Your AbstractThreadedSyncAdapter and all its sync functionality
 * will not work until external storage is remounted.
 * <p>
 * <b>Quote</b>: Your running Service will be killed and will not be restarted
 * when external storage is remounted. You can, however, register for the
 * ACTION_EXTERNAL_APPLICATIONS_AVAILABLE broadcast Intent, which will notify
 * your application when applications installed on external storage have become
 * available to the system again. At which time, you can restart your Service.
 * <p>
 * Problem: <a href="http://code.google.com/p/android/issues/detail?id=8485">that intent doesn't work</a>!
 */
public class AccountPickler {
  public static final String LOG_TAG = "AccountPickler";

  public static final long VERSION = 1;

  /**
   * Remove Sync account persisted to disk.
   *
   * @param context Android context.
   * @param filename name of persisted pickle file; must not contain path separators.
   * @return <code>true</code> if given pickle existed and was successfully deleted.
   */
  public static boolean deletePickle(final Context context, final String filename) {
    return context.deleteFile(filename);
  }

  /**
   * Persist Sync account to disk as a JSON object.
   * <p>
   * JSON object has keys:
   * <ul>
   * <li><code>Constants.JSON_KEY_ACCOUNT</code>: the Sync account's un-encoded username,
   * like "test@mozilla.com".</li>
   *
   * <li><code>Constants.JSON_KEY_PASSWORD</code>: the Sync account's password;</li>
   *
   * <li><code>Constants.JSON_KEY_SERVER</code>: the Sync account's server;</li>
   *
   * <li><code>Constants.JSON_KEY_SYNCKEY</code>: the Sync account's sync key;</li>
   *
   * <li><code>Constants.JSON_KEY_CLUSTER</code>: the Sync account's cluster (may be null);</li>
   *
   * <li><code>Constants.JSON_KEY_CLIENT_NAME</code>: the Sync account's client name (may be null);</li>
   *
   * <li><code>Constants.JSON_KEY_CLIENT_GUID</code>: the Sync account's client GUID (may be null);</li>
   *
   * <li><code>Constants.JSON_KEY_SYNC_AUTOMATICALLY</code>: true if the Android Account is syncing automically;</li>
   *
   * <li><code>Constants.JSON_KEY_VERSION</code>: version of this file;</li>
   *
   * <li><code>Constants.JSON_KEY_TIMESTAMP</code>: when this file was written.</li>
   * </ul>
   *
   *
   * @param context Android context.
   * @param filename name of file to persist to; must not contain path separators.
   * @param params the Sync account's parameters.
   * @param syncAutomatically whether the Android Account object is syncing automatically.
   */
  public static void pickle(final Context context, final String filename,
      final SyncAccountParameters params, final boolean syncAutomatically) {
    final ExtendedJSONObject o = params.asJSON();
    o.put(Constants.JSON_KEY_SYNC_AUTOMATICALLY, Boolean.valueOf(syncAutomatically));
    o.put(Constants.JSON_KEY_VERSION, new Long(VERSION));
    o.put(Constants.JSON_KEY_TIMESTAMP, new Long(System.currentTimeMillis()));

    PrintStream ps = null;
    try {
      final FileOutputStream fos = context.openFileOutput(filename, Context.MODE_PRIVATE);
      ps = new PrintStream(fos);
      ps.print(o.toJSONString());
      Logger.debug(LOG_TAG, "Persisted " + o.keySet().size() + " account settings to " + filename + ".");
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Caught exception persisting account settings to " + filename + "; ignoring.", e);
    } finally {
      if (ps != null) {
        ps.close();
      }
    }
  }

  /**
   * Create Android account from saved JSON object.
   *
   * @param context
   *          Android context.
   * @param filename
   *          name of file to read from; must not contain path separators.
   * @return created Android account, or null on error.
   */
  public static Account unpickle(final Context context, final String filename) {
    final String jsonString = Utils.readFile(context, filename);
    if (jsonString == null) {
      Logger.info(LOG_TAG, "Pickle file '" + filename + "' not found; aborting.");
      return null;
    }

    ExtendedJSONObject json = null;
    try {
      json = ExtendedJSONObject.parseJSONObject(jsonString);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception reading pickle file '" + filename + "'; aborting.", e);
      return null;
    }

    SyncAccountParameters params = null;
    try {
      // Null checking of inputs is done in constructor.
      params = new SyncAccountParameters(context, null, json);
    } catch (IllegalArgumentException e) {
      Logger.warn(LOG_TAG, "Un-pickled data included null username, password, or serverURL; aborting.", e);
      return null;
    }

    // Default to syncing automatically.
    boolean syncAutomatically = true;
    if (json.containsKey(Constants.JSON_KEY_SYNC_AUTOMATICALLY)) {
      if (Boolean.FALSE.equals(json.get(Constants.JSON_KEY_SYNC_AUTOMATICALLY))) {
        syncAutomatically = false;
      }
    }

    final Account account = SyncAccounts.createSyncAccountPreservingExistingPreferences(params, syncAutomatically);
    if (account == null) {
      Logger.warn(LOG_TAG, "Failed to add Android Account; aborting.");
      return null;
    }

    Integer version   = json.getIntegerSafely(Constants.JSON_KEY_VERSION);
    Integer timestamp = json.getIntegerSafely(Constants.JSON_KEY_TIMESTAMP);
    if (version == null || timestamp == null) {
      Logger.warn(LOG_TAG, "Did not find version or timestamp in pickle file; ignoring.");
      version = new Integer(-1);
      timestamp = new Integer(-1);
    }

    Logger.info(LOG_TAG, "Un-pickled Android account named " + params.username + " (version " + version + ", pickled at " + timestamp + ").");

    return account;
  }
}
