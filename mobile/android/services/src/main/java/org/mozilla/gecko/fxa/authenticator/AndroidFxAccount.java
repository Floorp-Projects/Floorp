/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.PeriodicSync;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.ResultReceiver;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.EnvironmentUtils;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.fxa.login.StateFactory;
import org.mozilla.gecko.fxa.sync.FxAccountProfileService;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Semaphore;

/**
 * A Firefox Account that stores its details and state as user data attached to
 * an Android Account instance.
 * <p>
 * Account user data is accessible only to the Android App(s) that own the
 * Account type. Account user data is not removed when the App's private data is
 * cleared.
 */
public class AndroidFxAccount {
  protected static final String LOG_TAG = AndroidFxAccount.class.getSimpleName();

  private static final int CURRENT_SYNC_PREFS_VERSION = 1;
  private static final int CURRENT_RL_PREFS_VERSION = 1;

  // When updating the account, do not forget to update AccountPickler.
  public static final int CURRENT_ACCOUNT_VERSION = 3;
  private static final String ACCOUNT_KEY_ACCOUNT_VERSION = "version";
  private static final String ACCOUNT_KEY_PROFILE = "profile";
  private static final String ACCOUNT_KEY_IDP_SERVER = "idpServerURI";
  private static final String ACCOUNT_KEY_PROFILE_SERVER = "profileServerURI";
  private static final String ACCOUNT_KEY_UID = "uid";

  // Ephemeral field used to support renaming an account on API versions prior to 21.
  // On these API levels we can't simply rename an account: we need to delete it, and then create it
  // again. This will cause our "account was just deleted" side-effects to run in
  // FxAccountAuthenticator@getAccountRemovalAllowed, and we actually need them to not run.
  // If they run, our account pickle is blown away, our FxA device record is removed, etc.
  // And so, while renaming the account, we set this flag in userData, and check for it in our cleanup
  // logic.
  // It's important that when renaming is complete - or fails - that this flag is reset, otherwise
  // normal account deletion will not function correctly.
  /* package-private */ static final String ACCOUNT_KEY_RENAME_IN_PROGRESS = "accountBeingRenamed";
  /* package-private */ static final String ACCOUNT_VALUE_RENAME_IN_PROGRESS = "true";

  private static final String ACCOUNT_KEY_TOKEN_SERVER = "tokenServerURI";       // Sync-specific.
  private static final String ACCOUNT_KEY_DESCRIPTOR = "descriptor";

  public static final String ACCOUNT_KEY_FIRST_RUN_SCOPE = "firstRunScope";

  private static final int CURRENT_BUNDLE_VERSION = 2;
  private static final String BUNDLE_KEY_BUNDLE_VERSION = "version";
  /* package-private */static final String BUNDLE_KEY_STATE_LABEL = "stateLabel";
  /* package-private */ static final String BUNDLE_KEY_STATE = "state";
  private static final String BUNDLE_KEY_PROFILE_JSON = "profile";

  private static final String ACCOUNT_KEY_DEVICE_ID = "deviceId";
  private static final String ACCOUNT_KEY_DEVICE_REGISTRATION_VERSION = "deviceRegistrationVersion";
  private static final String ACCOUNT_KEY_DEVICE_REGISTRATION_TIMESTAMP = "deviceRegistrationTimestamp";
  private static final String ACCOUNT_KEY_DEVICE_PUSH_REGISTRATION_ERROR = "devicePushRegistrationError";
  private static final String ACCOUNT_KEY_DEVICE_PUSH_REGISTRATION_ERROR_TIME = "devicePushRegistrationErrorTime";

  // We only see the hashed FxA UID once every sync.
  // We might need it later for telemetry purposes outside of the context of a sync, which
  // is why it is persisted.
  // It is not expected to change during the lifetime of an account, but we set
  // that value every time we see an FxA token nonetheless.
  private static final String ACCOUNT_KEY_HASHED_FXA_UID = "hashedFxAUID";

  // Account authentication token type for fetching account profile.
  private static final String PROFILE_OAUTH_TOKEN_TYPE = "oauth::profile";

  // Services may request OAuth tokens from the Firefox Account dynamically.
  // Each such token is prefixed with "oauth::" and a service-dependent scope.
  // Such tokens should be destroyed when the account is removed from the device.
  // This list collects all the known "oauth::" token types in order to delete them when necessary.
  private static final List<String> KNOWN_OAUTH_TOKEN_TYPES;

  static {
    final List<String> list = new ArrayList<>();
    list.add(PROFILE_OAUTH_TOKEN_TYPE);
    KNOWN_OAUTH_TOKEN_TYPES = Collections.unmodifiableList(list);
  }

  public static final Map<String, Boolean> DEFAULT_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP;
  static {
    final HashMap<String, Boolean> m = new HashMap<String, Boolean>();
    // By default, Firefox Sync is enabled.
    m.put(BrowserContract.AUTHORITY, true);
    DEFAULT_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP = Collections.unmodifiableMap(m);
  }

  // List of accounts keys which are explicitly carried over when account is renamed on pre-21 devices.
  private static final List<String> ACCOUNT_KEY_TO_CARRY_OVER_ON_RENAME_SET;
  static {
    ArrayList<String> keysToCarryOver = new ArrayList<>(6);
    keysToCarryOver.add(ACCOUNT_KEY_ACCOUNT_VERSION);
    keysToCarryOver.add(ACCOUNT_KEY_IDP_SERVER);
    keysToCarryOver.add(ACCOUNT_KEY_TOKEN_SERVER);
    keysToCarryOver.add(ACCOUNT_KEY_PROFILE_SERVER);
    keysToCarryOver.add(ACCOUNT_KEY_PROFILE);
    keysToCarryOver.add(ACCOUNT_KEY_DESCRIPTOR);
    ACCOUNT_KEY_TO_CARRY_OVER_ON_RENAME_SET = Collections.unmodifiableList(keysToCarryOver);
  }

  private static final String PREF_KEY_LAST_SYNCED_TIMESTAMP = "lastSyncedTimestamp";

  protected final Context context;
  private final AccountManager accountManager;
  private static final long neverSynced = -1;

  // This is really, really meant to be final. Only changed when account name changes.
  // See Bug 1368147.
  protected volatile Account account;

  /**
   * Create an Android Firefox Account instance backed by an Android Account
   * instance.
   * <p>
   * We expect a long-lived application context to avoid life-cycle issues that
   * might arise if the internally cached AccountManager instance surfaces UI.
   * <p>
   * We take care to not install any listeners or observers that might outlive
   * the AccountManager; and Android ensures the AccountManager doesn't outlive
   * the associated context.
   *
   * @param applicationContext
   *          to use as long-lived ambient Android context.
   * @param account
   *          Android account to use for storage.
   */
  public AndroidFxAccount(Context applicationContext, Account account) {
    this.context = applicationContext;
    this.account = account;
    this.accountManager = AccountManager.get(this.context);
  }

  public static AndroidFxAccount fromContext(Context context) {
    context = context.getApplicationContext();
    Account account = FirefoxAccounts.getFirefoxAccount(context);
    if (account == null) {
      return null;
    }
    return new AndroidFxAccount(context, account);
  }

  @Nullable
  public String getUserData(String key) {
    return accountManager.getUserData(account, key);
  }

  public Account getAndroidAccount() {
    return this.account;
  }

  private int getAccountVersion() {
    String v = accountManager.getUserData(account, ACCOUNT_KEY_ACCOUNT_VERSION);
    if (v == null) {
      return 0;         // Implicit.
    }

    try {
      return Integer.parseInt(v, 10);
    } catch (NumberFormatException ex) {
      return 0;
    }
  }

  private String getAccountUID() {
    // UID isn't going to change for the lifetime of our account.
    String accountUID = accountManager.getUserData(account, ACCOUNT_KEY_UID);
    if (accountUID != null) {
      return accountUID;
    }

    // And now, let's hear it for some side-effects!
    // If account was created before Bug 1368147, UID won't be there.
    // We need to ensure it is preset in the user data for ease of future reference, and so we parse
    // it out of the persisted profile data.
    final AccountPickler.UnpickleParams profileParams;

    try {
      profileParams = AccountPickler.unpickleParams(
              context, FxAccountConstants.ACCOUNT_PICKLE_FILENAME);
    } catch (Exception e) {
      throw new IllegalStateException("Couldn't process account profile data", e);
    }

    final String unpickledAccountUID = profileParams.getUID();
    // This should never happen. But it's useful to know if it does, so just crash.
    if (unpickledAccountUID == null) {
      throw new IllegalStateException("Unpickled account UID is null");
    }

    // Persist our UID into userData, so that we never have to do this dance again.
    accountManager.setUserData(account, ACCOUNT_KEY_UID, unpickledAccountUID);

    return unpickledAccountUID;
  }

  /**
   * Saves the given data as the internal bundle associated with this account.
   * @param bundle to write to account.
   */
  private synchronized void persistBundle(ExtendedJSONObject bundle) {
    accountManager.setUserData(account, ACCOUNT_KEY_DESCRIPTOR, bundle.toJSONString());
  }

  /**
   * Retrieve the internal bundle associated with this account.
   * @return bundle associated with account.
   */
  /* package-private */ synchronized ExtendedJSONObject unbundle() {
    final String bundleString = accountManager.getUserData(account, ACCOUNT_KEY_DESCRIPTOR);

    final int version = getAccountVersion();
    if (version < CURRENT_ACCOUNT_VERSION) {
      // Needs upgrade. For now, do nothing. We'd like to just put your account
      // into the Separated state here and have you update your credentials.
      return null;
    }

    if (version > CURRENT_ACCOUNT_VERSION) {
      // Oh dear.
      throw new IllegalStateException("Invalid account bundle version. Current: " + CURRENT_ACCOUNT_VERSION + ", bundle version: " + version);
    }

    if (bundleString == null) {
      return null;
    }

    return unbundleAccountV2(bundleString);
  }

  private String getBundleData(String key) {
    ExtendedJSONObject o = unbundle();
    if (o == null) {
      return null;
    }
    return o.getString(key);
  }

  private void updateBundleValues(String key, String value, String... more) {
    if (more.length % 2 != 0) {
      throw new IllegalArgumentException("more must be a list of key, value pairs");
    }
    ExtendedJSONObject descriptor = unbundle();
    if (descriptor == null) {
      return;
    }
    descriptor.put(key, value);
    for (int i = 0; i + 1 < more.length; i += 2) {
      descriptor.put(more[i], more[i+1]);
    }
    persistBundle(descriptor);
  }

  private ExtendedJSONObject unbundleAccountV1(String bundle) {
    ExtendedJSONObject o;
    try {
      o = new ExtendedJSONObject(bundle);
    } catch (Exception e) {
      return null;
    }
    if (CURRENT_BUNDLE_VERSION == o.getIntegerSafely(BUNDLE_KEY_BUNDLE_VERSION)) {
      return o;
    }
    return null;
  }

  private ExtendedJSONObject unbundleAccountV2(String bundle) {
    return unbundleAccountV1(bundle);
  }

  /**
   * Note that if the user clears data, an account will be left pointing to a
   * deleted profile. Such is life.
   */
  public String getProfile() {
    return accountManager.getUserData(account, ACCOUNT_KEY_PROFILE);
  }

  public String getAccountServerURI() {
    return accountManager.getUserData(account, ACCOUNT_KEY_IDP_SERVER);
  }

  public String getTokenServerURI() {
    return accountManager.getUserData(account, ACCOUNT_KEY_TOKEN_SERVER);
  }

  public String getProfileServerURI() {
    String profileURI = accountManager.getUserData(account, ACCOUNT_KEY_PROFILE_SERVER);
    if (profileURI == null) {
      if (isStaging()) {
        return FxAccountConstants.STAGE_PROFILE_SERVER_ENDPOINT;
      }
      return FxAccountConstants.DEFAULT_PROFILE_SERVER_ENDPOINT;
    }
    return profileURI;
  }

  /* package-private */ String getOAuthServerURI() {
    // Allow testing against stage.
    if (isStaging()) {
      return FxAccountConstants.STAGE_OAUTH_SERVER_ENDPOINT;
    } else {
      return FxAccountConstants.DEFAULT_OAUTH_SERVER_ENDPOINT;
    }
  }

  private boolean isStaging() {
    return FxAccountConstants.STAGE_AUTH_SERVER_ENDPOINT.equals(getAccountServerURI());
  }

  private String constructPrefsPath(@NonNull String accountKey, String product, long version, String extra) throws GeneralSecurityException, UnsupportedEncodingException {
    String profile = getProfile();

    if (profile == null) {
      throw new IllegalStateException("Missing profile. Cannot fetch prefs.");
    }

    if (accountKey == null) {
      throw new IllegalStateException("Missing accountKey. Cannot fetch prefs.");
    }

    final String fxaServerURI = getAccountServerURI();
    if (fxaServerURI == null) {
      throw new IllegalStateException("No account server URI. Cannot fetch prefs.");
    }

    // This is unique for each syncing 'view' of the account.
    final String serverURLThing = fxaServerURI + "!" + extra;
    return Utils.getPrefsPath(product, accountKey, serverURLThing, profile, version);
  }

  /**
   * This needs to return a string because of the tortured prefs access in GlobalSession.
   */
  private String getSyncPrefsPath(final String accountKey) throws GeneralSecurityException, UnsupportedEncodingException {
    final String tokenServerURI = getTokenServerURI();
    if (tokenServerURI == null) {
      throw new IllegalStateException("No token server URI. Cannot fetch prefs.");
    }

    final String product = GlobalConstants.BROWSER_INTENT_PACKAGE + ".fxa";
    final long version = CURRENT_SYNC_PREFS_VERSION;
    return constructPrefsPath(accountKey, product, version, tokenServerURI);
  }

  private String getReadingListPrefsPath(final String accountKey) throws GeneralSecurityException, UnsupportedEncodingException {
    final String product = GlobalConstants.BROWSER_INTENT_PACKAGE + ".reading";
    final long version = CURRENT_RL_PREFS_VERSION;
    return constructPrefsPath(accountKey, product, version, "");
  }

  @SuppressWarnings("unchecked")
  private static void migrateSharedPreferencesValues(SharedPreferences sharedPreferences, Map<String, ?> values) {
    final SharedPreferences.Editor editor = sharedPreferences.edit();
    for (String key : values.keySet()) {
      final Object value = values.get(key);
      if (value instanceof String) {
        editor.putString(key, (String) value);
      } else if (value instanceof Integer) {
        editor.putInt(key, ((Integer) value));
      } else if (value instanceof Float) {
        editor.putFloat(key, ((Float) value));
      } else if (value instanceof Long) {
        editor.putLong(key, ((Long) value));
      } else if (value instanceof Boolean) {
        editor.putBoolean(key, ((Boolean) value));
      } else if (value instanceof Set) {
        // This performs an unchecked cast. Let's hope it won't fail!
        editor.putStringSet(key, ((Set<String>) value));
      }
    }
    editor.apply();
  }

  private SharedPreferences maybeMigrateSharedPreferences(SharedPreferences sharedPreferences) throws UnsupportedEncodingException, GeneralSecurityException {
    // Move everything from the old preference file to the new.
    final SharedPreferences oldPreferences = context.getSharedPreferences(
            getSyncPrefsPath(account.name),
            Utils.SHARED_PREFERENCES_MODE
    );
    return doMaybeMigrateSharedPreferences(sharedPreferences, oldPreferences);
  }

  @VisibleForTesting
  /* package-private */ static SharedPreferences doMaybeMigrateSharedPreferences(SharedPreferences sharedPreferences, SharedPreferences oldSharedPreferences) throws UnsupportedEncodingException, GeneralSecurityException {
    // Bug 1368147: we used to store sync preferences with an account email in their file name; but,
    // that is now an ephemeral entity; the path forward is to store sync preferences with an
    // account UID in the filename. Logic below performs this migration.

    // It appears that sharedPreferences have been written to before, so we don't need to migrate.
    // This is bound to be one of the least efficient way of performing this check.
    if (sharedPreferences.getAll().size() != 0) {
      return sharedPreferences;
    }

    // If the old preference file doesn't exist either, or is empty - return the new one.
    final Map<String, ?> oldPreferenceValues = oldSharedPreferences.getAll();
    if (oldPreferenceValues.size() == 0) {
      return sharedPreferences;
    }

    // Copy over values from existing prefs to the newly created prefs.
    migrateSharedPreferencesValues(sharedPreferences, oldPreferenceValues);
    // Clear out old prefs.
    oldSharedPreferences.edit().clear().apply();

    return sharedPreferences;
  }

  public SharedPreferences getSyncPrefs() throws UnsupportedEncodingException, GeneralSecurityException {
    final SharedPreferences sharedPreferences = context.getSharedPreferences(
            getSyncPrefsPath(getAccountUID()),
            Utils.SHARED_PREFERENCES_MODE
    );

    maybeMigrateSharedPreferences(sharedPreferences);

    return sharedPreferences;
  }

  private SharedPreferences getReadingListPrefs() throws UnsupportedEncodingException, GeneralSecurityException {
    final SharedPreferences sharedPreferences = context.getSharedPreferences(
            getReadingListPrefsPath(getAccountUID()),
            Utils.SHARED_PREFERENCES_MODE
    );

    maybeMigrateSharedPreferences(sharedPreferences);

    return sharedPreferences;
  }

  /**
   * Extract a JSON dictionary of the string values associated to this account.
   * <p>
   * <b>For debugging use only!</b> The contents of this JSON object completely
   * determine the user's Firefox Account status and yield access to whatever
   * user data the device has access to.
   *
   * @return JSON-object of Strings.
   */
  public ExtendedJSONObject toJSONObject() {
    ExtendedJSONObject o = unbundle();
    o.put("email", account.name);
    try {
      o.put("emailUTF8", Utils.byte2Hex(account.name.getBytes("UTF-8")));
    } catch (UnsupportedEncodingException e) {
      // Ignore.
    }
    o.put("fxaDeviceId", getDeviceId());
    o.put("fxaDeviceRegistrationVersion", getDeviceRegistrationVersion());
    o.put("fxaDeviceRegistrationTimestamp", getDeviceRegistrationTimestamp());
    return o;
  }

  public static AndroidFxAccount addAndroidAccount(
      @NonNull Context context,
      @NonNull String uid,
      @NonNull String email,
      @NonNull String profile,
      @NonNull String idpServerURI,
      @NonNull String tokenServerURI,
      @NonNull String profileServerURI,
      @NonNull State state,
      final Map<String, Boolean> authoritiesToSyncAutomaticallyMap)
          throws UnsupportedEncodingException, GeneralSecurityException, URISyntaxException {
    return addAndroidAccount(context, uid, email, profile, idpServerURI, tokenServerURI, profileServerURI, state,
        authoritiesToSyncAutomaticallyMap,
        CURRENT_ACCOUNT_VERSION, false, null);
  }

  /* package-private */ static AndroidFxAccount addAndroidAccount(
      @NonNull Context context,
      @NonNull String uid,
      @NonNull String email,
      @NonNull String profile,
      @NonNull String idpServerURI,
      @NonNull String tokenServerURI,
      @NonNull String profileServerURI,
      @NonNull State state,
      final Map<String, Boolean> authoritiesToSyncAutomaticallyMap,
      final int accountVersion,
      final boolean fromPickle,
      ExtendedJSONObject bundle)
          throws UnsupportedEncodingException, GeneralSecurityException, URISyntaxException {
    if (uid == null) {
      throw new IllegalArgumentException("uid must not be null");
    }
    if (email == null) {
      throw new IllegalArgumentException("email must not be null");
    }
    if (profile == null) {
      throw new IllegalArgumentException("profile must not be null");
    }
    if (idpServerURI == null) {
      throw new IllegalArgumentException("idpServerURI must not be null");
    }
    if (tokenServerURI == null) {
      throw new IllegalArgumentException("tokenServerURI must not be null");
    }
    if (profileServerURI == null) {
      throw new IllegalArgumentException("profileServerURI must not be null");
    }
    if (state == null) {
      throw new IllegalArgumentException("state must not be null");
    }

    // TODO: Add migration code.
    if (accountVersion != CURRENT_ACCOUNT_VERSION) {
      throw new IllegalStateException("Could not create account of version " + accountVersion +
          ". Current version is " + CURRENT_ACCOUNT_VERSION + ".");
    }

    // Android has internal restrictions that require all values in this
    // bundle to be strings. *sigh*
    Bundle userdata = new Bundle();
    userdata.putString(ACCOUNT_KEY_ACCOUNT_VERSION, "" + CURRENT_ACCOUNT_VERSION);
    userdata.putString(ACCOUNT_KEY_IDP_SERVER, idpServerURI);
    userdata.putString(ACCOUNT_KEY_TOKEN_SERVER, tokenServerURI);
    userdata.putString(ACCOUNT_KEY_PROFILE_SERVER, profileServerURI);
    userdata.putString(ACCOUNT_KEY_PROFILE, profile);
    userdata.putString(ACCOUNT_KEY_UID, uid);

    if (bundle == null) {
      bundle = new ExtendedJSONObject();
      // TODO: How to upgrade?
      bundle.put(BUNDLE_KEY_BUNDLE_VERSION, CURRENT_BUNDLE_VERSION);
    }
    bundle.put(BUNDLE_KEY_STATE_LABEL, state.getStateLabel().name());
    bundle.put(BUNDLE_KEY_STATE, state.toJSONObject().toJSONString());

    userdata.putString(ACCOUNT_KEY_DESCRIPTOR, bundle.toJSONString());

    optionallyTagWithFirstRunScope(context, userdata);

    Account account = new Account(email, FxAccountConstants.ACCOUNT_TYPE);
    AccountManager accountManager = AccountManager.get(context);
    // We don't set an Android password, because we don't want to persist the
    // password (or anything else as powerful as the password). Instead, we
    // internally manage a sessionToken with a remotely owned lifecycle.
    boolean added = accountManager.addAccountExplicitly(account, null, userdata);
    if (!added) {
      return null;
    }

    // Try to work around an intermittent issue described at
    // http://stackoverflow.com/a/11698139.  What happens is that tests that
    // delete and re-create the same account frequently will find the account
    // missing all or some of the userdata bundle, possibly due to an Android
    // AccountManager caching bug.
    for (String key : userdata.keySet()) {
      accountManager.setUserData(account, key, userdata.getString(key));
    }

    final AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);

    if (!fromPickle) {
      fxAccount.clearSyncPrefs();

      // We can nearly eliminate various pickle races by pickling the account right after creation.
      // These races are an unfortunate by-product of Bug 1368147. The long-term solution is to stop
      // pickling entirely, but for now we just ensure we'll have a pickle file as soon as possible.
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
    }

    fxAccount.setAuthoritiesToSyncAutomaticallyMap(authoritiesToSyncAutomaticallyMap);

    return fxAccount;
  }

  /**
   * If there's an active First Run session, tag our new account with an appropriate first run scope.
   * We do this in order to reliably determine if an account was created during the current "first run".
   *
   * See {@link FirefoxAccounts#optionallySeparateAccountsDuringFirstRun} for details.
   */
  private static void optionallyTagWithFirstRunScope(final Context context, final Bundle userdata) {
    final String firstRunUUID = EnvironmentUtils.firstRunUUID(context);
    if (firstRunUUID != null) {
      userdata.putString(ACCOUNT_KEY_FIRST_RUN_SCOPE, firstRunUUID);
    }
  }

  /**
   * If there's an active First Run session, tag the given account with it.  If
   * there's no active First Run session, remove any existing tag.  We do this
   * in order to reliably determine if an account was created during the current
   * "first run"; this allows us to re-connect an account that was not created
   * during the current "first run".
   *
   * See {@link FirefoxAccounts#optionallySeparateAccountsDuringFirstRun} for details.
   */
  public void updateFirstRunScope(final Context context) {
    String firstRunUUID = EnvironmentUtils.firstRunUUID(context);
    accountManager.setUserData(account, ACCOUNT_KEY_FIRST_RUN_SCOPE, firstRunUUID);
  }

  private void clearSyncPrefs() throws UnsupportedEncodingException, GeneralSecurityException {
    getSyncPrefs().edit().clear().apply();
  }

  private void setAuthoritiesToSyncAutomaticallyMap(Map<String, Boolean> authoritiesToSyncAutomaticallyMap) {
    if (authoritiesToSyncAutomaticallyMap == null) {
      throw new IllegalArgumentException("authoritiesToSyncAutomaticallyMap must not be null");
    }

    for (String authority : DEFAULT_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP.keySet()) {
      boolean authorityEnabled = DEFAULT_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP.get(authority);
      final Boolean enabled = authoritiesToSyncAutomaticallyMap.get(authority);
      if (enabled != null) {
        authorityEnabled = enabled.booleanValue();
      }
      // Accounts are always capable of being synced ...
      ContentResolver.setIsSyncable(account, authority, 1);
      // ... but not always automatically synced.
      ContentResolver.setSyncAutomatically(account, authority, authorityEnabled);
    }
  }

  /* package-private */ Map<String, Boolean> getAuthoritiesToSyncAutomaticallyMap() {
    final Map<String, Boolean> authoritiesToSync = new HashMap<>();
    for (String authority : DEFAULT_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP.keySet()) {
      final boolean enabled = ContentResolver.getSyncAutomatically(account, authority);
      authoritiesToSync.put(authority, enabled);
    }
    return authoritiesToSync;
  }

  /**
   * Is a sync currently in progress?
   *
   * @return true if Android is currently syncing the underlying Android Account.
   */
  public boolean isCurrentlySyncing() {
    boolean active = false;
    for (String authority : AndroidFxAccount.DEFAULT_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP.keySet()) {
      active |= ContentResolver.isSyncActive(account, authority);
    }
    return active;
  }

  /**
   * Request an immediate sync.  Use this to sync as soon as possible in response to user action.
   *
   * @param stagesToSync stage names to sync; can be null to sync <b>all</b> known stages.
   * @param stagesToSkip stage names to skip; can be null to skip <b>no</b> known stages.
   * @param ignoreSettings whether we should check if syncing is allowed via in-app or system settings.
   */
  public void requestImmediateSync(String[] stagesToSync, String[] stagesToSkip, boolean ignoreSettings) {
    FirefoxAccounts.requestImmediateSync(getAndroidAccount(), stagesToSync, stagesToSkip, ignoreSettings);
  }

  /**
   * Request an eventual sync.  Use this to request the system queue a sync for some time in the
   * future.
   *
   * @param stagesToSync stage names to sync; can be null to sync <b>all</b> known stages.
   * @param stagesToSkip stage names to skip; can be null to skip <b>no</b> known stages.
   */
  public void requestEventualSync(String[] stagesToSync, String[] stagesToSkip) {
    FirefoxAccounts.requestEventualSync(getAndroidAccount(), stagesToSync, stagesToSkip);
  }

  public synchronized void setState(State state) {
    if (state == null) {
      throw new IllegalArgumentException("state must not be null");
    }
    Logger.info(LOG_TAG, "Moving account named like " + getObfuscatedEmail() +
        " to state " + state.getStateLabel().toString());
    updateBundleValues(
        BUNDLE_KEY_STATE_LABEL, state.getStateLabel().name(),
        BUNDLE_KEY_STATE, state.toJSONObject().toJSONString());
    broadcastAccountStateChangedIntent();
  }

  private void broadcastAccountStateChangedIntent() {
    final Intent intent = new Intent(FxAccountConstants.ACCOUNT_STATE_CHANGED_ACTION);
    intent.putExtra(Constants.JSON_KEY_ACCOUNT, account.name);
    LocalBroadcastManager.getInstance(context).sendBroadcast(intent);
  }

  public synchronized State getState() {
    // Ensure we're working with the latest 'account' state. It might have changed underneath us.
    // Note that this "state refresh" is inefficient for some callers, since they might have
    // created this object (and thus fetched an account from the accounts system) just moments ago.
    // Other callers maintain this object for a while, and thus might be out-of-sync with the world.
    // See Bug 1407316 for higher-order improvements that will make this unnecessary.
    refreshAndroidAccount();

    String stateLabelString = getBundleData(BUNDLE_KEY_STATE_LABEL);
    String stateString = getBundleData(BUNDLE_KEY_STATE);
    if (stateLabelString == null || stateString == null) {
      throw new IllegalStateException("stateLabelString and stateString must not be null, but: " +
          "(stateLabelString == null) = " + (stateLabelString == null) +
          " and (stateString == null) = " + (stateString == null));
    }

    try {
      StateLabel stateLabel = StateLabel.valueOf(stateLabelString);
      Logger.debug(LOG_TAG, "Account is in state " + stateLabel);
      return StateFactory.fromJSONObject(stateLabel, new ExtendedJSONObject(stateString));
    } catch (Exception e) {
      throw new IllegalStateException("could not get state", e);
    }
  }

  private void refreshAndroidAccount() {
    final Account[] accounts = accountManager.getAccountsByType(FxAccountConstants.ACCOUNT_TYPE);
    if (accounts.length == 0) {
      throw new IllegalStateException("Unexpectedly missing any accounts of type " + FxAccountConstants.ACCOUNT_TYPE);
    }
    account = accounts[0];
  }

  /**
   * <b>For debugging only!</b>
   */
  public void dump() {
    // Make sure we dump using the latest 'account'. It might have changed since this object was
    // initialized.
    account = FirefoxAccounts.getFirefoxAccount(context);

    if (!FxAccountUtils.LOG_PERSONAL_INFORMATION) {
      return;
    }
    ExtendedJSONObject o = toJSONObject();
    ArrayList<String> list = new ArrayList<String>(o.keySet());
    Collections.sort(list);
    for (String key : list) {
      FxAccountUtils.pii(LOG_TAG, key + ": " + o.get(key));
    }
  }

  /**
   * Return the Firefox Account's local email address.
   * <p>
   * It is important to note that this is the local email address, and not
   * necessarily the normalized remote email address that the server expects.
   *
   * @return local email address.
   */
  public String getEmail() {
    return account.name;
  }

  /**
   * Return the Firefox Account's local email address, obfuscated.
   * <p>
   * Use this when logging.
   *
   * @return local email address, obfuscated.
   */
  public String getObfuscatedEmail() {
    return Utils.obfuscateEmail(account.name);
  }

  /**
   * Populate an intent used for starting FxAccountDeletedService service.
   *
   * @param intent Intent to populate with necessary extras
   * @return <code>Intent</code> with a deleted action and account/OAuth information extras
   */
  /* package-private */ Intent populateDeletedAccountIntent(final Intent intent) {
    final List<String> tokens = new ArrayList<>();

    intent.putExtra(FxAccountConstants.ACCOUNT_DELETED_INTENT_VERSION_KEY,
        Long.valueOf(FxAccountConstants.ACCOUNT_DELETED_INTENT_VERSION));
    intent.putExtra(FxAccountConstants.ACCOUNT_DELETED_INTENT_ACCOUNT_KEY, account.name);
    intent.putExtra(FxAccountConstants.ACCOUNT_DELETED_INTENT_ACCOUNT_PROFILE, getProfile());

    // Get the tokens from AccountManager. Note: currently, only reading list service supports OAuth. The following logic will
    // be extended in future to support OAuth for other services.
    for (String tokenKey : KNOWN_OAUTH_TOKEN_TYPES) {
      final String authToken = accountManager.peekAuthToken(account, tokenKey);
      if (authToken != null) {
        tokens.add(authToken);
      }
    }

    // Update intent with tokens and service URI.
    intent.putExtra(FxAccountConstants.ACCOUNT_OAUTH_SERVICE_ENDPOINT_KEY, getOAuthServerURI());
    // Deleted broadcasts are package-private, so there's no security risk include the tokens in the extras
    intent.putExtra(FxAccountConstants.ACCOUNT_DELETED_INTENT_ACCOUNT_AUTH_TOKENS, tokens.toArray(new String[tokens.size()]));

    try {
      intent.putExtra(FxAccountConstants.ACCOUNT_DELETED_INTENT_ACCOUNT_SESSION_TOKEN, getState().getSessionToken());
    } catch (State.NotASessionTokenState e) {
      // Ignore, if sessionToken is null we won't try to do anything anyway.
    }
    intent.putExtra(FxAccountConstants.ACCOUNT_DELETED_INTENT_ACCOUNT_SERVER_URI, getAccountServerURI());
    intent.putExtra(FxAccountConstants.ACCOUNT_DELETED_INTENT_ACCOUNT_DEVICE_ID, getDeviceId());

    return intent;
  }

  /**
   * Create an intent announcing that the profile JSON attached to this Firefox Account has been updated.
   * <p>
   * It is not guaranteed that the profile JSON has changed.
   *
   * @return <code>Intent</code> to broadcast.
   */
  private Intent makeProfileJSONUpdatedIntent() {
    final Intent intent = new Intent();
    intent.setAction(FxAccountConstants.ACCOUNT_PROFILE_JSON_UPDATED_ACTION);
    return intent;
  }

  public void setLastSyncedTimestamp(long now) {
    try {
      getSyncPrefs().edit().putLong(PREF_KEY_LAST_SYNCED_TIMESTAMP, now).commit();
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception setting last synced time; ignoring.", e);
    }
  }

  public long getLastSyncedTimestamp() {
    try {
      return getSyncPrefs().getLong(PREF_KEY_LAST_SYNCED_TIMESTAMP, neverSynced);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception getting last synced time; ignoring.", e);
      return neverSynced;
    }
  }

  public boolean neverSynced() {
    try {
      return getSyncPrefs().getLong(PREF_KEY_LAST_SYNCED_TIMESTAMP, neverSynced) == -1;
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception getting last synced time; ignoring.", e);
      return false;
    }
  }

  // Debug only!  This is dangerous!
  public void unsafeTransitionToDefaultEndpoints() {
    unsafeTransitionToStageEndpoints(
        FxAccountConstants.DEFAULT_AUTH_SERVER_ENDPOINT,
        FxAccountConstants.DEFAULT_TOKEN_SERVER_ENDPOINT,
        FxAccountConstants.DEFAULT_PROFILE_SERVER_ENDPOINT);
    }

  // Debug only!  This is dangerous!
  public void unsafeTransitionToStageEndpoints() {
    unsafeTransitionToStageEndpoints(
        FxAccountConstants.STAGE_AUTH_SERVER_ENDPOINT,
        FxAccountConstants.STAGE_TOKEN_SERVER_ENDPOINT,
        FxAccountConstants.STAGE_PROFILE_SERVER_ENDPOINT);
  }

  private void unsafeTransitionToStageEndpoints(String authServerEndpoint, String tokenServerEndpoint, String profileServerEndpoint) {
    try {
      getReadingListPrefs().edit().clear().commit();
    } catch (UnsupportedEncodingException | GeneralSecurityException e) {
      // Ignore.
    }
    try {
      getSyncPrefs().edit().clear().commit();
    } catch (UnsupportedEncodingException | GeneralSecurityException e) {
      // Ignore.
    }
    State state = getState();
    setState(state.makeSeparatedState());
    accountManager.setUserData(account, ACCOUNT_KEY_IDP_SERVER, authServerEndpoint);
    accountManager.setUserData(account, ACCOUNT_KEY_TOKEN_SERVER, tokenServerEndpoint);
    accountManager.setUserData(account, ACCOUNT_KEY_PROFILE_SERVER, profileServerEndpoint);
    ContentResolver.setIsSyncable(account, BrowserContract.READING_LIST_AUTHORITY, 1);
  }

  /**
   * Returns the current profile JSON if available, or null.
   *
   * @return profile JSON object.
   */
  public ExtendedJSONObject getProfileJSON() {
    final String profileString = getBundleData(BUNDLE_KEY_PROFILE_JSON);
    if (profileString == null) {
      return null;
    }

    try {
      return new ExtendedJSONObject(profileString);
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Failed to parse profile JSON; ignoring and returning null.", e);
    }
    return null;
  }

  /**
   * Fetch the profile JSON associated to the underlying Firefox Account from the server and update the local store.
   * <p>
   * The LocalBroadcastManager is used to notify the receivers asynchronously after a successful fetch.
   */
  public void fetchProfileJSON() {
    ThreadUtils.postToBackgroundThread(new Runnable() {
      @Override
      public void run() {
        // Fetch profile information from server.
        String authToken;
        try {
          authToken = accountManager.blockingGetAuthToken(account, AndroidFxAccount.PROFILE_OAUTH_TOKEN_TYPE, true);
          if (authToken == null) {
            throw new RuntimeException("Couldn't get oauth token!  Aborting profile fetch.");
          }
        } catch (Exception e) {
          Logger.error(LOG_TAG, "Error fetching profile information; ignoring.", e);
          return;
        }

        Logger.info(LOG_TAG, "Intent service launched to fetch profile.");
        final Intent intent = new Intent(context, FxAccountProfileService.class);
        intent.putExtra(FxAccountProfileService.KEY_AUTH_TOKEN, authToken);
        intent.putExtra(FxAccountProfileService.KEY_PROFILE_SERVER_URI, getProfileServerURI());
        intent.putExtra(FxAccountProfileService.KEY_RESULT_RECEIVER, new ProfileResultReceiver(new Handler()));
        context.startService(intent);
      }
    });
  }

  private long getUserDataLong(String key, long defaultValue) {
    final String numStr = accountManager.getUserData(account, key);
    if (TextUtils.isEmpty(numStr)) {
      return defaultValue;
    }
    try {
      return Long.parseLong(numStr);
    } catch (NumberFormatException e) {
      Logger.warn(LOG_TAG, "Couldn't parse " + key + "; defaulting to " + defaultValue, e);
      return defaultValue;
    }
  }

  @Nullable
  public synchronized String getDeviceId() {
    return accountManager.getUserData(account, ACCOUNT_KEY_DEVICE_ID);
  }

  public synchronized int getDeviceRegistrationVersion() {
    final String numStr = accountManager.getUserData(account, ACCOUNT_KEY_DEVICE_REGISTRATION_VERSION);
    if (TextUtils.isEmpty(numStr)) {
      return 0;
    }
    try {
      return Integer.parseInt(numStr);
    } catch (NumberFormatException e) {
      Logger.warn(LOG_TAG, "Couldn't parse ACCOUNT_KEY_DEVICE_REGISTRATION_VERSION; defaulting to 0", e);
      return 0;
    }
  }

  public synchronized long getDeviceRegistrationTimestamp() {
    return getUserDataLong(ACCOUNT_KEY_DEVICE_REGISTRATION_TIMESTAMP, 0L);
  }

  public synchronized long getDevicePushRegistrationError() {
    return getUserDataLong(ACCOUNT_KEY_DEVICE_PUSH_REGISTRATION_ERROR, 0L);
  }

  public synchronized long getDevicePushRegistrationErrorTime() {
    return getUserDataLong(ACCOUNT_KEY_DEVICE_PUSH_REGISTRATION_ERROR_TIME, 0L);
  }

  public synchronized void setDeviceId(String id) {
    accountManager.setUserData(account, ACCOUNT_KEY_DEVICE_ID, id);
  }

  private synchronized void setDeviceRegistrationVersion(int deviceRegistrationVersion) {
    accountManager.setUserData(account, ACCOUNT_KEY_DEVICE_REGISTRATION_VERSION,
        Integer.toString(deviceRegistrationVersion));
  }

  public synchronized void setDeviceRegistrationTimestamp(long timestamp) {
    accountManager.setUserData(account, ACCOUNT_KEY_DEVICE_REGISTRATION_TIMESTAMP,
            Long.toString(timestamp));
  }

  public synchronized void resetDeviceRegistrationVersion() {
    setDeviceRegistrationVersion(0);
  }

  public synchronized void setFxAUserData(String id, int deviceRegistrationVersion, long timestamp) {
    accountManager.setUserData(account, ACCOUNT_KEY_DEVICE_ID, id);
    accountManager.setUserData(account, ACCOUNT_KEY_DEVICE_REGISTRATION_VERSION,
            Integer.toString(deviceRegistrationVersion));
    accountManager.setUserData(account, ACCOUNT_KEY_DEVICE_REGISTRATION_TIMESTAMP,
            Long.toString(timestamp));
  }

  public synchronized void setDevicePushRegistrationError(long error, long errorTimeMs) {
    accountManager.setUserData(account, ACCOUNT_KEY_DEVICE_PUSH_REGISTRATION_ERROR, Long.toString(error));
    accountManager.setUserData(account, ACCOUNT_KEY_DEVICE_PUSH_REGISTRATION_ERROR_TIME, Long.toString(errorTimeMs));
  }

  public synchronized void resetDevicePushRegistrationError() {
    setDevicePushRegistrationError(0L, 0l);
  }

  public synchronized void setCachedHashedFxAUID(final String newHashedFxAUID) {
    accountManager.setUserData(account, ACCOUNT_KEY_HASHED_FXA_UID, newHashedFxAUID);
  }

  public synchronized String getCachedHashedFxAUID() {
    return accountManager.getUserData(account, ACCOUNT_KEY_HASHED_FXA_UID);
  }

  @SuppressLint("ParcelCreator") // The CREATOR field is defined in the super class.
  private class ProfileResultReceiver extends ResultReceiver {
    /* package-private */ ProfileResultReceiver(Handler handler) {
      super(handler);
    }

    @Override
    protected void onReceiveResult(int resultCode, Bundle bundle) {
      super.onReceiveResult(resultCode, bundle);
      switch (resultCode) {
        case Activity.RESULT_OK:
          Logger.info(LOG_TAG, "Profile JSON fetch succeeded!");
          final String resultData = bundle.getString(FxAccountProfileService.KEY_RESULT_STRING);
          FxAccountUtils.pii(LOG_TAG, "Profile JSON fetch returned: " + resultData);

          renameAccountIfNecessary(resultData, new Runnable() {
            @Override
            public void run() {
              updateBundleValues(BUNDLE_KEY_PROFILE_JSON, resultData);
              LocalBroadcastManager.getInstance(context).sendBroadcast(makeProfileJSONUpdatedIntent());
            }
          });
          break;
        case Activity.RESULT_CANCELED:
          Logger.warn(LOG_TAG, "Failed to fetch profile JSON; ignoring.");
          break;
        default:
          Logger.warn(LOG_TAG, "Invalid result code received; ignoring.");
          break;
      }
    }
  }

  private void renameAccountIfNecessary(final String profileData, final Runnable callback) {
    final ExtendedJSONObject profileJSON;
    try {
      profileJSON = new ExtendedJSONObject(profileData);
    } catch (NonObjectJSONException | IOException e) {
      Logger.error(LOG_TAG, "Error processing fetched account json string", e);
      callback.run();
      return;
    }
    if (!profileJSON.containsKey("email")) {
      Logger.error(LOG_TAG, "Profile JSON missing email key");
      callback.run();
      return;
    }
    final String email = profileJSON.getString("email");

    // To prevent race against a shared state, we need to hold a lock which is released after
    // account is renamed, or it's determined that the rename isn't necessary.
    try {
      acquireSharedAccountStateLock(LOG_TAG);
    } catch (InterruptedException e) {
      Logger.error(LOG_TAG, "Could not acquire a shared account state lock.");
      callback.run();
      return;
    }

    try {
      // Update our internal state. It's possible that the account changed underneath us while
      // we were fetching profile information. See Bug 1401318.
      account = FirefoxAccounts.getFirefoxAccount(context);

      // If primary email didn't change, there's nothing for us to do.
      if (account.name.equals(email)) {
        callback.run();
        return;
      }

      Logger.info(LOG_TAG, "Renaming Android Account.");
      FxAccountUtils.pii(LOG_TAG, "Renaming Android account from " + account.name + " to " + email);

      // Then, get the currently auto-syncing authorities.
      // We'll toggle these on once we re-add the account.
      final Map<String, Boolean> currentAuthoritiesToSync = getAuthoritiesToSyncAutomaticallyMap();

      // We also need to manually carry over current sync intervals.
      final Map<String, List<PeriodicSync>> periodicSyncsForAuthorities = new HashMap<>();
      for (String authority : currentAuthoritiesToSync.keySet()) {
        periodicSyncsForAuthorities.put(authority, ContentResolver.getPeriodicSyncs(account, authority));
      }

      final Runnable migrateSyncSettings = new Runnable() {
        @Override
        public void run() {
          // Set up auto-syncing for the newly added account.
          setAuthoritiesToSyncAutomaticallyMap(currentAuthoritiesToSync);

          // Set up all of the periodic syncs we had prior.
          for (String authority : periodicSyncsForAuthorities.keySet()) {
            final List<PeriodicSync> periodicSyncs = periodicSyncsForAuthorities.get(authority);
            for (PeriodicSync periodicSync : periodicSyncs) {
              ContentResolver.addPeriodicSync(
                      account,
                      periodicSync.authority,
                      periodicSync.extras,
                      periodicSync.period
              );
            }
          }
        }
      };

      // On API21+, we can simply "rename" the account, which will recreate it carrying over user data.
      // Our regular "account was just deleted" side-effects will not run.
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
        doOptionalProfileRename21Plus(email, migrateSyncSettings, callback);

        // Prior to API21, we have to perform this operation manually: make a copy of the user data,
        // delete current account, and then re-create it.
        // We also need to ensure our regular "account was just deleted" side-effects are not invoked.
      } else {
        doOptionalProfileRenamePre21(email, migrateSyncSettings, callback);
      }
    } finally {
      releaseSharedAccountStateLock();
    }
  }

  @TargetApi(Build.VERSION_CODES.LOLLIPOP)
  private void doOptionalProfileRename21Plus(final String newEmail, final Runnable migrateSyncSettingsCallback, final Runnable callback) {
    final Account currentAccount = new Account(account.name, account.type);
    accountManager.renameAccount(currentAccount, newEmail, new AccountManagerCallback<Account>() {
      @Override
      public void run(AccountManagerFuture<Account> future) {
        if (future.isCancelled()) {
          Logger.error(LOG_TAG, "Account rename task cancelled.");
          callback.run();
          return;
        }

        if (!future.isDone()) {
          Logger.error(LOG_TAG, "Account rename callback invoked, by task is not finished.");
          callback.run();
          return;
        }

        try {
          final Account updatedAccount = future.getResult();

          // We tried, we really did.
          if (!updatedAccount.name.equals(newEmail)) {
            Logger.error(LOG_TAG, "Tried to update account name, but it didn't seem to have changed.");
          } else {
            account = updatedAccount;
            migrateSyncSettingsCallback.run();

            callback.run();
          }
        } catch (OperationCanceledException | IOException | AuthenticatorException e) {
          Logger.error(LOG_TAG, "Unexpected exception while trying to rename an account", e);
          callback.run();
        }
      }
      // Request that callbacks are posted to the current thread.
    }, new Handler());
  }

  @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
  private void doOptionalProfileRenamePre21(final String email, final Runnable migrateSyncSettingsCallback, final Runnable callback) {
    // First, manually make a copy of account's current user data.
    final Bundle currentUserData = new Bundle();

    for (String key : ACCOUNT_KEY_TO_CARRY_OVER_ON_RENAME_SET) {
      currentUserData.putString(key, accountManager.getUserData(account, key));
    }

    // Ensure our account deletion side-effects will not run. See comments at this key's definition.
    // Note that this key is not copied over to the new account. We do need to ensure this field is
    // reset if we fail to remove the account. Otherwise our regular account deletion logic won't run
    // in the future.
    accountManager.setUserData(account, ACCOUNT_KEY_RENAME_IN_PROGRESS, ACCOUNT_VALUE_RENAME_IN_PROGRESS);

    // Then, remove current account.
    final Account currentAccount = new Account(account.name, account.type);
    accountManager.removeAccount(currentAccount, new AccountManagerCallback<Boolean>() {
      @Override
      public void run(AccountManagerFuture<Boolean> future) {
        boolean accountRemovalSucceeded = false;
        boolean removeResult = false;
        try {
          removeResult = future.getResult();
          accountRemovalSucceeded = !future.isCancelled() && future.isDone() && removeResult;
        } catch (OperationCanceledException | IOException | AuthenticatorException e) {
          Logger.error(LOG_TAG, "Exception while obtaining account remove task results. Moving on.", e);
        }

        // Did something go wrong? Log specific error, clear ACCOUNT_KEY_RENAME_IN_PROGRESS and move on.
        // Bug 1398978 tracks sending a telemetry event to monitor these failures in the wild.
        if (!accountRemovalSucceeded) {
          if (future.isCancelled()) {
            Logger.error(LOG_TAG, "Account remove task cancelled. Moving on.");
          } else if (!future.isDone()) {
            Logger.error(LOG_TAG, "Account remove callback invoked, but task is not finished. Moving on.");
          } else if (!removeResult) {
            Logger.error(LOG_TAG, "Failed to remove current account while renaming accounts. Moving on.");
          }

          accountManager.setUserData(account, ACCOUNT_KEY_RENAME_IN_PROGRESS, null);
          callback.run();
          return;
        }

        // It appears that we've successfully removed the account. It's now time to re-add it.

        // Finally, add an Android account with new name and old user data.
        final Account newAccount = new Account(email, FxAccountConstants.ACCOUNT_TYPE);
        final boolean didAdd = accountManager.addAccountExplicitly(newAccount, null, currentUserData);

        // Rename succeeded, now let's configure the newly added account.
        if (didAdd) {
          account = newAccount;

          migrateSyncSettingsCallback.run();

          callback.run();
          return;
        }

        // Oh, this isn't good. This could mean that account already exists, or that something else
        // happened. Account should not already exist, since supposedly we just removed it.
        // AccountManager docs are not specific on what "something else" might actually mean.
        // At this point it's as if we never had an account to begin with, and so we skip calling
        // our callback.
        // Bug 1398978 tracks sending a telemetry event to monitor such failures in the wild.
        Logger.error(LOG_TAG, "Failed to add account with a new name after deleting old account.");
      }
    // Request that callbacks are posted to the current thread.
    }, new Handler());
  }

  /**
   * Take the lock to own updating any Firefox Account's internal state.
   *
   * We use a <code>Semaphore</code> rather than a <code>ReentrantLock</code>
   * because the callback that needs to release the lock may not be invoked on
   * the thread that initially acquired the lock. Be aware!
   */
  private static final Semaphore sLock = new Semaphore(1, true /* fair */);

  // Which consumer took the lock?
  // Synchronized by this.
  private String lockTag = null;

  // Are we locked?  (It's not easy to determine who took the lock dynamically,
  // so we maintain this flag internally.)
  // Synchronized by this.
  protected boolean locked = false;

  // Block until we can take the shared state lock.
  public synchronized void acquireSharedAccountStateLock(final String tag) throws InterruptedException {
    final long id = Thread.currentThread().getId();
    this.lockTag = tag;
    Log.d(Logger.DEFAULT_LOG_TAG, "Thread with tag and thread id acquiring lock: " + lockTag + ", " + id + " ...");
    sLock.acquire();
    locked = true;
    Log.d(Logger.DEFAULT_LOG_TAG, "Thread with tag and thread id acquiring lock: " + lockTag + ", " + id + " ... ACQUIRED");
  }

  // If we hold the shared state lock, release it.  Otherwise, ignore the request.
  public synchronized void releaseSharedAccountStateLock() {
    final long id = Thread.currentThread().getId();
    Log.d(Logger.DEFAULT_LOG_TAG, "Thread with tag and thread id releasing lock: " + lockTag + ", " + id + " ...");
    if (locked) {
      sLock.release();
      locked = false;
      Log.d(Logger.DEFAULT_LOG_TAG, "Thread with tag and thread id releasing lock: " + lockTag + ", " + id + " ... RELEASED");
    } else {
      Log.d(Logger.DEFAULT_LOG_TAG, "Thread with tag and thread id releasing lock: " + lockTag + ", " + id + " ... NOT LOCKED");
    }
  }

  @Override
  protected synchronized void finalize() throws Throwable {
    super.finalize();
    if (locked) {
      // Should never happen, but...
      sLock.release();
      locked = false;
      final long id = Thread.currentThread().getId();
      Log.e(Logger.DEFAULT_LOG_TAG, "Thread with tag and thread id releasing lock: " + lockTag + ", " + id + " ... RELEASED DURING FINALIZE");
    }
  }
}
