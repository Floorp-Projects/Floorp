/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import java.io.FileOutputStream;
import java.io.PrintStream;
import java.security.NoSuchAlgorithmException;
import java.security.spec.InvalidKeySpecException;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.fxa.login.StateFactory;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.Utils;

import android.content.Context;

/**
 * Android deletes Account objects when the Authenticator that owns the Account
 * disappears. This happens when an App is installed to the SD card and the SD
 * card is un-mounted or the device is rebooted.
 * <p>
 * We work around this by pickling the current Firefox account data every sync
 * and unpickling when we check if Firefox accounts exist (called from Fennec).
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
 * <p>
 * See bug 768102 for more information in the context of Sync.
 */
public class AccountPickler {
  public static final String LOG_TAG = AccountPickler.class.getSimpleName();

  public static final long PICKLE_VERSION = 3;

  public static final String KEY_PICKLE_VERSION = "pickle_version";
  public static final String KEY_PICKLE_TIMESTAMP = "pickle_timestamp";

  public static final String KEY_ACCOUNT_VERSION = "account_version";
  public static final String KEY_ACCOUNT_TYPE = "account_type";
  public static final String KEY_EMAIL = "email";
  public static final String KEY_PROFILE = "profile";
  public static final String KEY_IDP_SERVER_URI = "idpServerURI";
  public static final String KEY_TOKEN_SERVER_URI = "tokenServerURI";
  public static final String KEY_PROFILE_SERVER_URI = "profileServerURI";

  public static final String KEY_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP = "authoritiesToSyncAutomaticallyMap";

  // Deprecated, but maintained for migration purposes.
  public static final String KEY_IS_SYNCING_ENABLED = "isSyncingEnabled";

  public static final String KEY_BUNDLE = "bundle";

  /**
   * Remove Firefox account persisted to disk.
   * This operation is synchronized to avoid race condition while deleting the account.
   *
   * @param context Android context.
   * @param filename name of persisted pickle file; must not contain path separators.
   * @return <code>true</code> if given pickle existed and was successfully deleted.
   */
  public synchronized static boolean deletePickle(final Context context, final String filename) {
    return context.deleteFile(filename);
  }

  public static ExtendedJSONObject toJSON(final AndroidFxAccount account, final long now) {
    final ExtendedJSONObject o = new ExtendedJSONObject();
    o.put(KEY_PICKLE_VERSION, PICKLE_VERSION);
    o.put(KEY_PICKLE_TIMESTAMP, now);

    o.put(KEY_ACCOUNT_VERSION, AndroidFxAccount.CURRENT_ACCOUNT_VERSION);
    o.put(KEY_ACCOUNT_TYPE, FxAccountConstants.ACCOUNT_TYPE);
    o.put(KEY_EMAIL, account.getEmail());
    o.put(KEY_PROFILE, account.getProfile());
    o.put(KEY_IDP_SERVER_URI, account.getAccountServerURI());
    o.put(KEY_TOKEN_SERVER_URI, account.getTokenServerURI());
    o.put(KEY_PROFILE_SERVER_URI, account.getProfileServerURI());

    final ExtendedJSONObject p = new ExtendedJSONObject();
    for (Entry<String, Boolean> pair : account.getAuthoritiesToSyncAutomaticallyMap().entrySet()) {
      p.put(pair.getKey(), pair.getValue());
    }
    o.put(KEY_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP, p);

    // TODO: If prefs version changes under us, SyncPrefsPath will change, "clearing" prefs.

    final ExtendedJSONObject bundle = account.unbundle();
    if (bundle == null) {
      Logger.warn(LOG_TAG, "Unable to obtain account bundle; aborting.");
      return null;
    }
    o.put(KEY_BUNDLE, bundle);

    return o;
  }

  /**
   * Persist Firefox account to disk as a JSON object.
   * This operation is synchronized to avoid race condition while deleting the account.
   *
   * @param account the AndroidFxAccount to persist to disk
   * @param filename name of file to persist to; must not contain path separators.
   */
  public synchronized static void pickle(final AndroidFxAccount account, final String filename) {
    final ExtendedJSONObject o = toJSON(account, System.currentTimeMillis());
    writeToDisk(account.context, filename, o);
  }

  private static void writeToDisk(final Context context, final String filename,
      final ExtendedJSONObject pickle) {
    try {
      final FileOutputStream fos = context.openFileOutput(filename, Context.MODE_PRIVATE);
      try {
        final PrintStream ps = new PrintStream(fos);
        try {
          ps.print(pickle.toJSONString());
          Logger.debug(LOG_TAG, "Persisted " + pickle.keySet().size() +
              " account settings to " + filename + ".");
        } finally {
          ps.close();
        }
      } finally {
        fos.close();
      }
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Caught exception persisting account settings to " + filename +
          "; ignoring.", e);
    }
  }

  /**
   * Create Android account from saved JSON object. Assumes that an account does not exist.
   * This operation is synchronized to avoid race condition while deleting the account.
   *
   * @param context
   *          Android context.
   * @param filename
   *          name of file to read from; must not contain path separators.
   * @return created Android account, or null on error.
   */
  public synchronized static AndroidFxAccount unpickle(final Context context, final String filename) {
    final String jsonString = Utils.readFile(context, filename);
    if (jsonString == null) {
      Logger.info(LOG_TAG, "Pickle file '" + filename + "' not found; aborting.");
      return null;
    }

    ExtendedJSONObject json = null;
    try {
      json = new ExtendedJSONObject(jsonString);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception reading pickle file '" + filename + "'; aborting.", e);
      return null;
    }

    final UnpickleParams params;
    try {
      params = UnpickleParams.fromJSON(json);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception extracting unpickle json; aborting.", e);
      return null;
    }

    final AndroidFxAccount account;
    try {
      account = AndroidFxAccount.addAndroidAccount(context, params.email, params.profile,
          params.authServerURI, params.tokenServerURI, params.profileServerURI, params.state,
          params.authoritiesToSyncAutomaticallyMap,
          params.accountVersion,
          true, params.bundle);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Exception when adding Android Account; aborting.", e);
      return null;
    }

    if (account == null) {
      Logger.warn(LOG_TAG, "Failed to add Android Account; aborting.");
      return null;
    }

    Long timestamp = json.getLong(KEY_PICKLE_TIMESTAMP);
    if (timestamp == null) {
      Logger.warn(LOG_TAG, "Did not find timestamp in pickle file; ignoring.");
      timestamp = -1L;
    }

    Logger.info(LOG_TAG, "Un-pickled Android account named " + params.email + " (version " +
        params.pickleVersion + ", pickled at " + timestamp + ").");

    return account;
  }

  private static class UnpickleParams {
    private Long pickleVersion;

    private int accountVersion;
    private String email;
    private String profile;
    private String authServerURI;
    private String tokenServerURI;
    private String profileServerURI;
    private final Map<String, Boolean> authoritiesToSyncAutomaticallyMap = new HashMap<>();

    private ExtendedJSONObject bundle;
    private State state;

    private UnpickleParams() {
    }

    private static UnpickleParams fromJSON(final ExtendedJSONObject json)
        throws InvalidKeySpecException, NoSuchAlgorithmException, NonObjectJSONException {
      final UnpickleParams params = new UnpickleParams();
      params.pickleVersion = json.getLong(KEY_PICKLE_VERSION);
      if (params.pickleVersion == null) {
        throw new IllegalStateException("Pickle version not found.");
      }

      /*
       * Version 1 and version 2 are identical, except version 2 throws if the
       * internal Android Account type has changed. Version 1 used to throw in
       * this case, but we intentionally used the pickle file to migrate across
       * Account types, bumping the version simultaneously.
       *
       * Version 3 replaces "isSyncEnabled" with a map (String -> Boolean)
       * associating Android authorities to whether or not they are configured
       * to sync automatically.
       */
      switch (params.pickleVersion.intValue()) {
      case 3: {
        // Sanity check.
        final String accountType = json.getString(KEY_ACCOUNT_TYPE);
        if (!FxAccountConstants.ACCOUNT_TYPE.equals(accountType)) {
          throw new IllegalStateException("Account type has changed from " + accountType + " to " + FxAccountConstants.ACCOUNT_TYPE + ".");
        }

        params.unpickleV3(json);
      }
      break;

      case 2: {
        // Sanity check.
        final String accountType = json.getString(KEY_ACCOUNT_TYPE);
        if (!FxAccountConstants.ACCOUNT_TYPE.equals(accountType)) {
          throw new IllegalStateException("Account type has changed from " + accountType + " to " + FxAccountConstants.ACCOUNT_TYPE + ".");
        }

        params.unpickleV1(json);
      }
      break;

      case 1: {
        // Warn about account type changing, but don't throw over it.
        final String accountType = json.getString(KEY_ACCOUNT_TYPE);
        if (!FxAccountConstants.ACCOUNT_TYPE.equals(accountType)) {
          Logger.warn(LOG_TAG, "Account type has changed from " + accountType + " to " + FxAccountConstants.ACCOUNT_TYPE + "; ignoring.");
        }

        params.unpickleV1(json);
      }
      break;

      default:
        throw new IllegalStateException("Unknown pickle version, " + params.pickleVersion + ".");
      }

      return params;
    }

    private void unpickleV1(final ExtendedJSONObject json)
        throws NonObjectJSONException, NoSuchAlgorithmException, InvalidKeySpecException {

      this.accountVersion = json.getIntegerSafely(KEY_ACCOUNT_VERSION);
      this.email = json.getString(KEY_EMAIL);
      this.profile = json.getString(KEY_PROFILE);
      this.authServerURI = json.getString(KEY_IDP_SERVER_URI);
      this.tokenServerURI = json.getString(KEY_TOKEN_SERVER_URI);
      this.profileServerURI = json.getString(KEY_PROFILE_SERVER_URI);

      // Fallback to default value when profile server URI was not pickled.
      if (this.profileServerURI == null) {
        this.profileServerURI = FxAccountConstants.DEFAULT_AUTH_SERVER_ENDPOINT.equals(this.authServerURI)
            ? FxAccountConstants.DEFAULT_PROFILE_SERVER_ENDPOINT
            : FxAccountConstants.STAGE_PROFILE_SERVER_ENDPOINT;
      }

      // We get the default value for everything except syncing browser data.
      this.authoritiesToSyncAutomaticallyMap.put(BrowserContract.AUTHORITY, json.getBoolean(KEY_IS_SYNCING_ENABLED));

      this.bundle = json.getObject(KEY_BUNDLE);
      if (bundle == null) {
        throw new IllegalStateException("Pickle bundle is null.");
      }
      this.state = getState(bundle);
    }

    private void unpickleV3(final ExtendedJSONObject json)
        throws NonObjectJSONException, NoSuchAlgorithmException, InvalidKeySpecException {
      // We'll overwrite the extracted sync automatically map.
      unpickleV1(json);

      // Extract the map of authorities to sync automatically.
      authoritiesToSyncAutomaticallyMap.clear();
      final ExtendedJSONObject o = json.getObject(KEY_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP);
      if (o == null) {
        return;
      }
      for (String key : o.keySet()) {
        final Boolean enabled = o.getBoolean(key);
        if (enabled != null) {
          authoritiesToSyncAutomaticallyMap.put(key, enabled);
        }
      }
    }

    private State getState(final ExtendedJSONObject bundle) throws InvalidKeySpecException,
            NonObjectJSONException, NoSuchAlgorithmException {
      // TODO: Should copy-pasta BUNDLE_KEY_STATE & LABEL to this file to ensure we maintain
      // old versions?
      final StateLabel stateLabelString = StateLabel.valueOf(
          bundle.getString(AndroidFxAccount.BUNDLE_KEY_STATE_LABEL));
      final String stateString = bundle.getString(AndroidFxAccount.BUNDLE_KEY_STATE);
      if (stateLabelString == null || stateString == null) {
        throw new IllegalStateException("stateLabel and stateString must not be null, but: " +
            "(stateLabel == null) = " + (stateLabelString == null) +
            " and (stateString == null) = " + (stateString == null));
      }

      try {
        return StateFactory.fromJSONObject(stateLabelString, new ExtendedJSONObject(stateString));
      } catch (Exception e) {
        throw new IllegalStateException("could not get state", e);
      }
    }
  }
}
