/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import java.io.UnsupportedEncodingException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.EnumSet;
import java.util.List;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.fxa.login.StateFactory;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;

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

  public static final int CURRENT_PREFS_VERSION = 1;

  // When updating the account, do not forget to update AccountPickler.
  public static final int CURRENT_ACCOUNT_VERSION = 3;
  public static final String ACCOUNT_KEY_ACCOUNT_VERSION = "version";
  public static final String ACCOUNT_KEY_PROFILE = "profile";
  public static final String ACCOUNT_KEY_IDP_SERVER = "idpServerURI";

  // The audience should always be a prefix of the token server URI.
  public static final String ACCOUNT_KEY_AUDIENCE = "audience";                 // Sync-specific.
  public static final String ACCOUNT_KEY_TOKEN_SERVER = "tokenServerURI";       // Sync-specific.
  public static final String ACCOUNT_KEY_DESCRIPTOR = "descriptor";

  public static final int CURRENT_BUNDLE_VERSION = 2;
  public static final String BUNDLE_KEY_BUNDLE_VERSION = "version";
  public static final String BUNDLE_KEY_STATE_LABEL = "stateLabel";
  public static final String BUNDLE_KEY_STATE = "state";

  protected static final List<String> ANDROID_AUTHORITIES = Collections.unmodifiableList(Arrays.asList(
      new String[] { BrowserContract.AUTHORITY }));

  protected final Context context;
  protected final AccountManager accountManager;
  protected final Account account;

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

  /**
   * Persist the Firefox account to disk as a JSON object. Note that this is a wrapper around
   * {@link AccountPickler#pickle}, and is identical to calling it directly.
   * <p>
   * Note that pickling is different from bundling, which involves operations on a
   * {@link android.os.Bundle Bundle} object of miscellaenous data associated with the account.
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
  protected void persistBundle(ExtendedJSONObject bundle) {
    accountManager.setUserData(account, ACCOUNT_KEY_DESCRIPTOR, bundle.toJSONString());
  }

  /**
   * Retrieve the internal bundle associated with this account.
   * @return bundle associated with account.
   */
  protected ExtendedJSONObject unbundle() {
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

    String bundle = accountManager.getUserData(account, ACCOUNT_KEY_DESCRIPTOR);
    if (bundle == null) {
      return null;
    }
    return unbundleAccountV2(bundle);
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
    return b.booleanValue();
  }

  protected byte[] getBundleDataBytes(String key) {
    ExtendedJSONObject o = unbundle();
    if (o == null) {
      return null;
    }
    return o.getByteArrayHex(key);
  }

  protected void updateBundleDataBytes(String key, byte[] value) {
    updateBundleValue(key, value == null ? null : Utils.byte2Hex(value));
  }

  protected void updateBundleValue(String key, boolean value) {
    ExtendedJSONObject descriptor = unbundle();
    if (descriptor == null) {
      return;
    }
    descriptor.put(key, value);
    persistBundle(descriptor);
  }

  protected void updateBundleValue(String key, String value) {
    ExtendedJSONObject descriptor = unbundle();
    if (descriptor == null) {
      return;
    }
    descriptor.put(key, value);
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

  public String getAudience() {
    return accountManager.getUserData(account, ACCOUNT_KEY_AUDIENCE);
  }

  public String getTokenServerURI() {
    return accountManager.getUserData(account, ACCOUNT_KEY_TOKEN_SERVER);
  }

  /**
   * This needs to return a string because of the tortured prefs access in GlobalSession.
   */
  public String getSyncPrefsPath() throws GeneralSecurityException, UnsupportedEncodingException {
    String profile = getProfile();
    String username = account.name;

    if (profile == null) {
      throw new IllegalStateException("Missing profile. Cannot fetch prefs.");
    }

    if (username == null) {
      throw new IllegalStateException("Missing username. Cannot fetch prefs.");
    }

    final String tokenServerURI = getTokenServerURI();
    if (tokenServerURI == null) {
      throw new IllegalStateException("No token server URI. Cannot fetch prefs.");
    }

    final String fxaServerURI = getAccountServerURI();
    if (fxaServerURI == null) {
      throw new IllegalStateException("No account server URI. Cannot fetch prefs.");
    }

    final String product = GlobalConstants.BROWSER_INTENT_PACKAGE + ".fxa";
    final long version = CURRENT_PREFS_VERSION;

    // This is unique for each syncing 'view' of the account.
    final String serverURLThing = fxaServerURI + "!" + tokenServerURI;
    return Utils.getPrefsPath(product, username, serverURLThing, profile, version);
  }

  public SharedPreferences getSyncPrefs() throws UnsupportedEncodingException, GeneralSecurityException {
    return context.getSharedPreferences(getSyncPrefsPath(), Utils.SHARED_PREFERENCES_MODE);
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
    return o;
  }

  public static AndroidFxAccount addAndroidAccount(
      Context context,
      String email,
      String profile,
      String idpServerURI,
      String tokenServerURI,
      State state)
          throws UnsupportedEncodingException, GeneralSecurityException, URISyntaxException {
    return addAndroidAccount(context, email, profile, idpServerURI, tokenServerURI, state,
        CURRENT_ACCOUNT_VERSION, true, false, null);
  }

  public static AndroidFxAccount addAndroidAccount(
      Context context,
      String email,
      String profile,
      String idpServerURI,
      String tokenServerURI,
      State state,
      final int accountVersion,
      final boolean syncEnabled,
      final boolean fromPickle,
      ExtendedJSONObject bundle)
          throws UnsupportedEncodingException, GeneralSecurityException, URISyntaxException {
    if (email == null) {
      throw new IllegalArgumentException("email must not be null");
    }
    if (idpServerURI == null) {
      throw new IllegalArgumentException("idpServerURI must not be null");
    }
    if (tokenServerURI == null) {
      throw new IllegalArgumentException("tokenServerURI must not be null");
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
    userdata.putString(ACCOUNT_KEY_AUDIENCE, FxAccountUtils.getAudienceForURL(tokenServerURI));
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

    AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);

    if (!fromPickle) {
      fxAccount.clearSyncPrefs();
    }

    if (syncEnabled) {
      fxAccount.enableSyncing();
    } else {
      fxAccount.disableSyncing();
    }

    return fxAccount;
  }

  public void clearSyncPrefs() throws UnsupportedEncodingException, GeneralSecurityException {
    getSyncPrefs().edit().clear().commit();
  }

  public static Iterable<String> getAndroidAuthorities() {
    return ANDROID_AUTHORITIES;
  }

  /**
   * Return true if the underlying Android account is currently set to sync automatically.
   * <p>
   * This is, confusingly, not the same thing as "being syncable": that refers
   * to whether this account can be synced, ever; this refers to whether Android
   * will try to sync the account at appropriate times.
   *
   * @return true if the account is set to sync automatically.
   */
  public boolean isSyncing() {
    boolean isSyncEnabled = true;
    for (String authority : getAndroidAuthorities()) {
      isSyncEnabled &= ContentResolver.getSyncAutomatically(account, authority);
    }
    return isSyncEnabled;
  }

  public void enableSyncing() {
    Logger.info(LOG_TAG, "Enabling sync for account named like " + getObfuscatedEmail());
    for (String authority : getAndroidAuthorities()) {
      ContentResolver.setSyncAutomatically(account, authority, true);
      ContentResolver.setIsSyncable(account, authority, 1);
    }
  }

  public void disableSyncing() {
    Logger.info(LOG_TAG, "Disabling sync for account named like " + getObfuscatedEmail());
    for (String authority : getAndroidAuthorities()) {
      ContentResolver.setSyncAutomatically(account, authority, false);
    }
  }

  /**
   * Is a sync currently in progress?
   *
   * @return true if Android is currently syncing the underlying Android Account.
   */
  public boolean isCurrentlySyncing() {
    boolean active = false;
    for (String authority : AndroidFxAccount.getAndroidAuthorities()) {
      active |= ContentResolver.isSyncActive(account, authority);
    }
    return active;
  }

  /**
   * Request a sync.  See {@link FirefoxAccounts#requestSync(Account, EnumSet, String[], String[])}.
   */
  public void requestSync() {
    requestSync(FirefoxAccounts.SOON, null, null);
  }

  /**
   * Request a sync.  See {@link FirefoxAccounts#requestSync(Account, EnumSet, String[], String[])}.
   *
   * @param syncHints to pass to sync.
   */
  public void requestSync(EnumSet<FirefoxAccounts.SyncHint> syncHints) {
    requestSync(syncHints, null, null);
  }

  /**
   * Request a sync.  See {@link FirefoxAccounts#requestSync(Account, EnumSet, String[], String[])}.
   *
   * @param syncHints to pass to sync.
   * @param stagesToSync stage names to sync.
   * @param stagesToSkip stage names to skip.
   */
  public void requestSync(EnumSet<FirefoxAccounts.SyncHint> syncHints, String[] stagesToSync, String[] stagesToSkip) {
    FirefoxAccounts.requestSync(getAndroidAccount(), syncHints, stagesToSync, stagesToSkip);
  }

  public synchronized void setState(State state) {
    if (state == null) {
      throw new IllegalArgumentException("state must not be null");
    }
    Logger.info(LOG_TAG, "Moving account named like " + getObfuscatedEmail() +
        " to state " + state.getStateLabel().toString());
    updateBundleValue(BUNDLE_KEY_STATE_LABEL, state.getStateLabel().name());
    updateBundleValue(BUNDLE_KEY_STATE, state.toJSONObject().toJSONString());
  }

  public synchronized State getState() {
    String stateLabelString = getBundleData(BUNDLE_KEY_STATE_LABEL);
    String stateString = getBundleData(BUNDLE_KEY_STATE);
    if (stateLabelString == null) {
      throw new IllegalStateException("stateLabelString must not be null");
    }
    if (stateString == null) {
      throw new IllegalStateException("stateString must not be null");
    }

    try {
      StateLabel stateLabel = StateLabel.valueOf(stateLabelString);
      return StateFactory.fromJSONObject(stateLabel, new ExtendedJSONObject(stateString));
    } catch (Exception e) {
      throw new IllegalStateException("could not get state", e);
    }
  }

  /**
   * <b>For debugging only!</b>
   */
  public void dump() {
    if (!FxAccountConstants.LOG_PERSONAL_INFORMATION) {
      return;
    }
    ExtendedJSONObject o = toJSONObject();
    ArrayList<String> list = new ArrayList<String>(o.keySet());
    Collections.sort(list);
    for (String key : list) {
      FxAccountConstants.pii(LOG_TAG, key + ": " + o.get(key));
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
   * Create an intent announcing that a Firefox account will be deleted.
   *
   * @param context
   *          Android context.
   * @param account
   *          Android account being removed.
   * @return <code>Intent</code> to broadcast.
   */
  public static Intent makeDeletedAccountIntent(final Context context, final Account account) {
    final Intent intent = new Intent(FxAccountConstants.ACCOUNT_DELETED_ACTION);

    intent.putExtra(FxAccountConstants.ACCOUNT_DELETED_INTENT_VERSION_KEY,
        Long.valueOf(FxAccountConstants.ACCOUNT_DELETED_INTENT_VERSION));
    intent.putExtra(FxAccountConstants.ACCOUNT_DELETED_INTENT_ACCOUNT_KEY, account.name);
    return intent;
  }
}
