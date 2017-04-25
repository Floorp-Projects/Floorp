/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.os.ResultReceiver;
import android.support.annotation.Nullable;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.fxa.login.StateFactory;
import org.mozilla.gecko.fxa.sync.FxAccountProfileService;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.UnsupportedEncodingException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.text.NumberFormat;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
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

  public static final int CURRENT_SYNC_PREFS_VERSION = 1;
  public static final int CURRENT_RL_PREFS_VERSION = 1;

  // When updating the account, do not forget to update AccountPickler.
  public static final int CURRENT_ACCOUNT_VERSION = 3;
  public static final String ACCOUNT_KEY_ACCOUNT_VERSION = "version";
  public static final String ACCOUNT_KEY_PROFILE = "profile";
  public static final String ACCOUNT_KEY_IDP_SERVER = "idpServerURI";
  private static final String ACCOUNT_KEY_PROFILE_SERVER = "profileServerURI";

  public static final String ACCOUNT_KEY_TOKEN_SERVER = "tokenServerURI";       // Sync-specific.
  public static final String ACCOUNT_KEY_DESCRIPTOR = "descriptor";

  public static final int CURRENT_BUNDLE_VERSION = 2;
  public static final String BUNDLE_KEY_BUNDLE_VERSION = "version";
  public static final String BUNDLE_KEY_STATE_LABEL = "stateLabel";
  public static final String BUNDLE_KEY_STATE = "state";
  public static final String BUNDLE_KEY_PROFILE_JSON = "profile";

  public static final String ACCOUNT_KEY_DEVICE_ID = "deviceId";
  public static final String ACCOUNT_KEY_DEVICE_REGISTRATION_VERSION = "deviceRegistrationVersion";
  private static final String ACCOUNT_KEY_DEVICE_REGISTRATION_TIMESTAMP = "deviceRegistrationTimestamp";
  private static final String ACCOUNT_KEY_DEVICE_PUSH_REGISTRATION_ERROR = "devicePushRegistrationError";
  private static final String ACCOUNT_KEY_DEVICE_PUSH_REGISTRATION_ERROR_TIME = "devicePushRegistrationErrorTime";

  // Account authentication token type for fetching account profile.
  public static final String PROFILE_OAUTH_TOKEN_TYPE = "oauth::profile";

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

  private static final String PREF_KEY_LAST_SYNCED_TIMESTAMP = "lastSyncedTimestamp";

  protected final Context context;
  protected final AccountManager accountManager;
  protected final Account account;

  /**
   * A cache associating Account name (email address) to a representation of the
   * account's internal bundle.
   * <p>
   * The cache is invalidated entirely when <it>any</it> new Account is added,
   * because there is no reliable way to know that an Account has been removed
   * and then re-added.
   */
  protected static final ConcurrentHashMap<String, ExtendedJSONObject> perAccountBundleCache =
      new ConcurrentHashMap<>();

  public static void invalidateCaches() {
    perAccountBundleCache.clear();
  }

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

  /**
   * Persist the Firefox account to disk as a JSON object. Note that this is a wrapper around
   * {@link AccountPickler#pickle}, and is identical to calling it directly.
   * <p>
   * Note that pickling is different from bundling, which involves operations on a
   * {@link android.os.Bundle Bundle} object of miscellaneous data associated with the account.
   * See {@link #persistBundle} and {@link #unbundle} for more.
   */
  public void pickle(final String filename) {
    AccountPickler.pickle(this, filename);
  }

  public Account getAndroidAccount() {
    return this.account;
  }

  protected int getAccountVersion() {
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

  /**
   * Saves the given data as the internal bundle associated with this account.
   * @param bundle to write to account.
   */
  protected synchronized void persistBundle(ExtendedJSONObject bundle) {
    perAccountBundleCache.put(account.name, bundle);
    accountManager.setUserData(account, ACCOUNT_KEY_DESCRIPTOR, bundle.toJSONString());
  }

  protected ExtendedJSONObject unbundle() {
    return unbundle(true);
  }

  /**
   * Retrieve the internal bundle associated with this account.
   * @return bundle associated with account.
   */
  protected synchronized ExtendedJSONObject unbundle(boolean allowCachedBundle) {
    if (allowCachedBundle) {
      final ExtendedJSONObject cachedBundle = perAccountBundleCache.get(account.name);
      if (cachedBundle != null) {
        Logger.debug(LOG_TAG, "Returning cached account bundle.");
        return cachedBundle;
      }
    }

    final int version = getAccountVersion();
    if (version < CURRENT_ACCOUNT_VERSION) {
      // Needs upgrade. For now, do nothing. We'd like to just put your account
      // into the Separated state here and have you update your credentials.
      return null;
    }

    if (version > CURRENT_ACCOUNT_VERSION) {
      // Oh dear.
      return null;
    }

    String bundleString = accountManager.getUserData(account, ACCOUNT_KEY_DESCRIPTOR);
    if (bundleString == null) {
      return null;
    }
    final ExtendedJSONObject bundle = unbundleAccountV2(bundleString);
    perAccountBundleCache.put(account.name, bundle);
    Logger.info(LOG_TAG, "Account bundle persisted to cache.");
    return bundle;
  }

  protected String getBundleData(String key) {
    ExtendedJSONObject o = unbundle();
    if (o == null) {
      return null;
    }
    return o.getString(key);
  }

  protected boolean getBundleDataBoolean(String key, boolean def) {
    ExtendedJSONObject o = unbundle();
    if (o == null) {
      return def;
    }
    Boolean b = o.getBoolean(key);
    if (b == null) {
      return def;
    }
    return b;
  }

  protected byte[] getBundleDataBytes(String key) {
    ExtendedJSONObject o = unbundle();
    if (o == null) {
      return null;
    }
    return o.getByteArrayHex(key);
  }

  protected void updateBundleValues(String key, String value, String... more) {
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

  public String getOAuthServerURI() {
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

  private String constructPrefsPath(String product, long version, String extra) throws GeneralSecurityException, UnsupportedEncodingException {
    String profile = getProfile();
    String username = account.name;

    if (profile == null) {
      throw new IllegalStateException("Missing profile. Cannot fetch prefs.");
    }

    if (username == null) {
      throw new IllegalStateException("Missing username. Cannot fetch prefs.");
    }

    final String fxaServerURI = getAccountServerURI();
    if (fxaServerURI == null) {
      throw new IllegalStateException("No account server URI. Cannot fetch prefs.");
    }

    // This is unique for each syncing 'view' of the account.
    final String serverURLThing = fxaServerURI + "!" + extra;
    return Utils.getPrefsPath(product, username, serverURLThing, profile, version);
  }

  /**
   * This needs to return a string because of the tortured prefs access in GlobalSession.
   */
  public String getSyncPrefsPath() throws GeneralSecurityException, UnsupportedEncodingException {
    final String tokenServerURI = getTokenServerURI();
    if (tokenServerURI == null) {
      throw new IllegalStateException("No token server URI. Cannot fetch prefs.");
    }

    final String product = GlobalConstants.BROWSER_INTENT_PACKAGE + ".fxa";
    final long version = CURRENT_SYNC_PREFS_VERSION;
    return constructPrefsPath(product, version, tokenServerURI);
  }

  public String getReadingListPrefsPath() throws GeneralSecurityException, UnsupportedEncodingException {
    final String product = GlobalConstants.BROWSER_INTENT_PACKAGE + ".reading";
    final long version = CURRENT_RL_PREFS_VERSION;
    return constructPrefsPath(product, version, "");
  }

  public SharedPreferences getSyncPrefs() throws UnsupportedEncodingException, GeneralSecurityException {
    return context.getSharedPreferences(getSyncPrefsPath(), Utils.SHARED_PREFERENCES_MODE);
  }

  public SharedPreferences getReadingListPrefs() throws UnsupportedEncodingException, GeneralSecurityException {
    return context.getSharedPreferences(getReadingListPrefsPath(), Utils.SHARED_PREFERENCES_MODE);
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
      Context context,
      String email,
      String profile,
      String idpServerURI,
      String tokenServerURI,
      String profileServerURI,
      State state,
      final Map<String, Boolean> authoritiesToSyncAutomaticallyMap)
          throws UnsupportedEncodingException, GeneralSecurityException, URISyntaxException {
    return addAndroidAccount(context, email, profile, idpServerURI, tokenServerURI, profileServerURI, state,
        authoritiesToSyncAutomaticallyMap,
        CURRENT_ACCOUNT_VERSION, false, null);
  }

  public static AndroidFxAccount addAndroidAccount(
      Context context,
      String email,
      String profile,
      String idpServerURI,
      String tokenServerURI,
      String profileServerURI,
      State state,
      final Map<String, Boolean> authoritiesToSyncAutomaticallyMap,
      final int accountVersion,
      final boolean fromPickle,
      ExtendedJSONObject bundle)
          throws UnsupportedEncodingException, GeneralSecurityException, URISyntaxException {
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

    if (bundle == null) {
      bundle = new ExtendedJSONObject();
      // TODO: How to upgrade?
      bundle.put(BUNDLE_KEY_BUNDLE_VERSION, CURRENT_BUNDLE_VERSION);
    }
    bundle.put(BUNDLE_KEY_STATE_LABEL, state.getStateLabel().name());
    bundle.put(BUNDLE_KEY_STATE, state.toJSONObject().toJSONString());

    userdata.putString(ACCOUNT_KEY_DESCRIPTOR, bundle.toJSONString());

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

    AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);

    if (!fromPickle) {
      fxAccount.clearSyncPrefs();
    }

    fxAccount.setAuthoritiesToSyncAutomaticallyMap(authoritiesToSyncAutomaticallyMap);

    return fxAccount;
  }

  public void clearSyncPrefs() throws UnsupportedEncodingException, GeneralSecurityException {
    getSyncPrefs().edit().clear().commit();
  }

  public void setAuthoritiesToSyncAutomaticallyMap(Map<String, Boolean> authoritiesToSyncAutomaticallyMap) {
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

  public Map<String, Boolean> getAuthoritiesToSyncAutomaticallyMap() {
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
   */
  public void requestImmediateSync(String[] stagesToSync, String[] stagesToSkip) {
    FirefoxAccounts.requestImmediateSync(getAndroidAccount(), stagesToSync, stagesToSkip);
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

  protected void broadcastAccountStateChangedIntent() {
    final Intent intent = new Intent(FxAccountConstants.ACCOUNT_STATE_CHANGED_ACTION);
    intent.putExtra(Constants.JSON_KEY_ACCOUNT, account.name);
    LocalBroadcastManager.getInstance(context).sendBroadcast(intent);
  }

  public synchronized State getState() {
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

  /**
   * <b>For debugging only!</b>
   */
  public void dump() {
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
  public Intent populateDeletedAccountIntent(final Intent intent) {
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
    final long neverSynced = -1L;
    try {
      return getSyncPrefs().getLong(PREF_KEY_LAST_SYNCED_TIMESTAMP, neverSynced);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception getting last synced time; ignoring.", e);
      return neverSynced;
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

  protected void unsafeTransitionToStageEndpoints(String authServerEndpoint, String tokenServerEndpoint, String profileServerEndpoint) {
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

  @SuppressWarnings("unchecked")
  private <T extends Number> T getUserDataNumber(String key, T defaultValue) {
    final String numStr = accountManager.getUserData(account, key);
    if (TextUtils.isEmpty(numStr)) {
      return defaultValue;
    }
    try {
      return (T) NumberFormat.getInstance().parse(numStr);
    } catch (ParseException e) {
      Logger.warn(LOG_TAG, "Couldn't parse " + key + "; defaulting to 0L.", e);
      return defaultValue;
    }
  }

  @Nullable
  public synchronized String getDeviceId() {
    return accountManager.getUserData(account, ACCOUNT_KEY_DEVICE_ID);
  }

  public synchronized int getDeviceRegistrationVersion() {
    return getUserDataNumber(ACCOUNT_KEY_DEVICE_REGISTRATION_VERSION, 0);
  }

  public synchronized long getDeviceRegistrationTimestamp() {
    return getUserDataNumber(ACCOUNT_KEY_DEVICE_REGISTRATION_TIMESTAMP, 0L);
  }

  public synchronized long getDevicePushRegistrationError() {
    return getUserDataNumber(ACCOUNT_KEY_DEVICE_PUSH_REGISTRATION_ERROR, 0L);
  }

  public synchronized long getDevicePushRegistrationErrorTime() {
    return getUserDataNumber(ACCOUNT_KEY_DEVICE_PUSH_REGISTRATION_ERROR_TIME, 0L);
  }

  public synchronized void setDeviceId(String id) {
    accountManager.setUserData(account, ACCOUNT_KEY_DEVICE_ID, id);
  }

  public synchronized void setDeviceRegistrationVersion(int deviceRegistrationVersion) {
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

  @SuppressLint("ParcelCreator") // The CREATOR field is defined in the super class.
  private class ProfileResultReceiver extends ResultReceiver {
    public ProfileResultReceiver(Handler handler) {
      super(handler);
    }

    @Override
    protected void onReceiveResult(int resultCode, Bundle bundle) {
      super.onReceiveResult(resultCode, bundle);
      switch (resultCode) {
        case Activity.RESULT_OK:
          final String resultData = bundle.getString(FxAccountProfileService.KEY_RESULT_STRING);
          updateBundleValues(BUNDLE_KEY_PROFILE_JSON, resultData);
          Logger.info(LOG_TAG, "Profile JSON fetch succeeeded!");
          FxAccountUtils.pii(LOG_TAG, "Profile JSON fetch returned: " + resultData);
          LocalBroadcastManager.getInstance(context).sendBroadcast(makeProfileJSONUpdatedIntent());
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

  /**
   * Take the lock to own updating any Firefox Account's internal state.
   *
   * We use a <code>Semaphore</code> rather than a <code>ReentrantLock</code>
   * because the callback that needs to release the lock may not be invoked on
   * the thread that initially acquired the lock. Be aware!
   */
  protected static final Semaphore sLock = new Semaphore(1, true /* fair */);

  // Which consumer took the lock?
  // Synchronized by this.
  protected String lockTag = null;

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
  protected synchronized void finalize() {
    if (locked) {
      // Should never happen, but...
      sLock.release();
      locked = false;
      final long id = Thread.currentThread().getId();
      Log.e(Logger.DEFAULT_LOG_TAG, "Thread with tag and thread id releasing lock: " + lockTag + ", " + id + " ... RELEASED DURING FINALIZE");
    }
  }
}
