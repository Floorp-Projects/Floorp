/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.Collections;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.RSACryptoImplementation;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.os.Bundle;

/**
 * A Firefox Account that stores its details and state as user data attached to
 * an Android Account instance.
 * <p>
 * Account user data is accessible only to the Android App(s) that own the
 * Account type. Account user data is not removed when the App's private data is
 * cleared.
 */
public class AndroidFxAccount implements AbstractFxAccount {
  protected static final String LOG_TAG = AndroidFxAccount.class.getSimpleName();

  public static final int CURRENT_PREFS_VERSION = 1;

  public static final int CURRENT_ACCOUNT_VERSION = 3;
  public static final String ACCOUNT_KEY_ACCOUNT_VERSION = "version";
  public static final String ACCOUNT_KEY_PROFILE = "profile";
  public static final String ACCOUNT_KEY_IDP_SERVER = "idpServerURI";

  // The audience should always be a prefix of the token server URI.
  public static final String ACCOUNT_KEY_AUDIENCE = "audience";                 // Sync-specific.
  public static final String ACCOUNT_KEY_TOKEN_SERVER = "tokenServerURI";       // Sync-specific.
  public static final String ACCOUNT_KEY_DESCRIPTOR = "descriptor";

  public static final int CURRENT_BUNDLE_VERSION = 1;
  public static final String BUNDLE_KEY_BUNDLE_VERSION = "version";
  public static final String BUNDLE_KEY_ASSERTION = "assertion";
  public static final String BUNDLE_KEY_CERTIFICATE = "certificate";
  public static final String BUNDLE_KEY_INVALID = "invalid";
  public static final String BUNDLE_KEY_SESSION_TOKEN = "sessionToken";
  public static final String BUNDLE_KEY_KEY_FETCH_TOKEN = "keyFetchToken";
  public static final String BUNDLE_KEY_VERIFIED = "verified";
  public static final String BUNDLE_KEY_KA = "kA";
  public static final String BUNDLE_KEY_KB = "kB";
  public static final String BUNDLE_KEY_UNWRAPKB = "unwrapkB";
  public static final String BUNDLE_KEY_ASSERTION_KEY_PAIR = "assertionKeyPair";

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

  protected void persistBundle(ExtendedJSONObject bundle) {
    accountManager.setUserData(account, ACCOUNT_KEY_DESCRIPTOR, bundle.toJSONString());
  }

  protected ExtendedJSONObject unbundle() {
    final int version = getAccountVersion();
    if (version < CURRENT_ACCOUNT_VERSION) {
      // Needs upgrade. For now, do nothing.
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
    return unbundleAccountV1(bundle);
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

  @Override
  public byte[] getEmailUTF8() {
    try {
      return account.name.getBytes("UTF-8");
    } catch (UnsupportedEncodingException e) {
      // Ignore.
      return null;
    }
  }

  /**
   * Note that if the user clears data, an account will be left pointing to a
   * deleted profile. Such is life.
   */
  @Override
  public String getProfile() {
    return accountManager.getUserData(account, ACCOUNT_KEY_PROFILE);
  }

  @Override
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

  @Override
  public void setQuickStretchedPW(byte[] quickStretchedPW) {
    accountManager.setPassword(account, quickStretchedPW == null ? null : Utils.byte2Hex(quickStretchedPW));
  }


  @Override
  public byte[] getQuickStretchedPW() {
    String quickStretchedPW = accountManager.getPassword(account);
    return quickStretchedPW == null ? null : Utils.hex2Byte(quickStretchedPW);
  }

  @Override
  public byte[] getSessionToken() {
    return getBundleDataBytes(BUNDLE_KEY_SESSION_TOKEN);
  }

  @Override
  public byte[] getKeyFetchToken() {
    return getBundleDataBytes(BUNDLE_KEY_KEY_FETCH_TOKEN);
  }

  @Override
  public void setSessionToken(byte[] sessionToken) {
    updateBundleDataBytes(BUNDLE_KEY_SESSION_TOKEN, sessionToken);
  }

  @Override
  public void setKeyFetchToken(byte[] keyFetchToken) {
    updateBundleDataBytes(BUNDLE_KEY_KEY_FETCH_TOKEN, keyFetchToken);
  }

  @Override
  public boolean isVerified() {
    return getBundleDataBoolean(BUNDLE_KEY_VERIFIED, false);
  }

  @Override
  public void setVerified() {
    updateBundleValue(BUNDLE_KEY_VERIFIED, true);
  }

  @Override
  public byte[] getKa() {
    return getBundleDataBytes(BUNDLE_KEY_KA);
  }

  @Override
  public void setKa(byte[] kA) {
    updateBundleValue(BUNDLE_KEY_KA, Utils.byte2Hex(kA));
  }

  @Override
  public void setWrappedKb(byte[] wrappedKb) {
    if (wrappedKb == null) {
      final String message = "wrappedKb is null: cannot set kB.";
      Logger.error(LOG_TAG, message);
      throw new IllegalArgumentException(message);
    }
    byte[] unwrapKb = getBundleDataBytes(BUNDLE_KEY_UNWRAPKB);
    if (unwrapKb == null) {
      Logger.error(LOG_TAG, "unwrapKb is null: cannot set kB.");
      return;
    }
    byte[] kB = new byte[wrappedKb.length]; // We could hard-code this to be 32.
    for (int i = 0; i < wrappedKb.length; i++) {
      kB[i] = (byte) (wrappedKb[i] ^ unwrapKb[i]);
    }
    updateBundleValue(BUNDLE_KEY_KB, Utils.byte2Hex(kB));
  }

  @Override
  public byte[] getKb() {
    return getBundleDataBytes(BUNDLE_KEY_KB);
  }

  protected BrowserIDKeyPair generateNewAssertionKeyPair() throws GeneralSecurityException {
    Logger.info(LOG_TAG, "Generating new assertion key pair.");
    // TODO Have the key size be a non-constant in FxAccountUtils, or read from SharedPreferences, or...
    return RSACryptoImplementation.generateKeyPair(1024);
  }

  @Override
  public BrowserIDKeyPair getAssertionKeyPair() throws GeneralSecurityException {
    try {
      String data = getBundleData(BUNDLE_KEY_ASSERTION_KEY_PAIR);
      return RSACryptoImplementation.fromJSONObject(new ExtendedJSONObject(data));
    } catch (Exception e) {
      // Fall through to generating a new key pair.
    }

    BrowserIDKeyPair keyPair = generateNewAssertionKeyPair();

    ExtendedJSONObject descriptor = unbundle();
    if (descriptor == null) {
      descriptor = new ExtendedJSONObject();
    }
    descriptor.put(BUNDLE_KEY_ASSERTION_KEY_PAIR, keyPair.toJSONObject().toJSONString());
    persistBundle(descriptor);
    return keyPair;
  }

  @Override
  public String getCertificate() {
    return getBundleData(BUNDLE_KEY_CERTIFICATE);
  }

  @Override
  public void setCertificate(String certificate) {
    updateBundleValue(BUNDLE_KEY_CERTIFICATE, certificate);
  }

  @Override
  public String getAssertion() {
    return getBundleData(BUNDLE_KEY_ASSERTION);
  }

  @Override
  public void setAssertion(String assertion) {
    updateBundleValue(BUNDLE_KEY_ASSERTION, assertion);
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
    o.put("quickStretchedPW", accountManager.getPassword(account));
    return o;
  }

  public static Account addAndroidAccount(
      Context context,
      String email,
      String password,
      String profile,
      String idpServerURI,
      String tokenServerURI,
      byte[] sessionToken,
      byte[] keyFetchToken,
      boolean verified)
          throws UnsupportedEncodingException, GeneralSecurityException, URISyntaxException {
    if (email == null) {
      throw new IllegalArgumentException("email must not be null");
    }
    if (password == null) {
      throw new IllegalArgumentException("password must not be null");
    }
    if (idpServerURI == null) {
      throw new IllegalArgumentException("idpServerURI must not be null");
    }
    if (tokenServerURI == null) {
      throw new IllegalArgumentException("tokenServerURI must not be null");
    }
    // sessionToken and keyFetchToken are allowed to be null; they can be
    // fetched via /account/login from the password. These tokens are generated
    // by the server and we have no length or formatting guarantees. However, if
    // one is given, both should be given: they come from the server together.
    if ((sessionToken == null && keyFetchToken != null) ||
        (sessionToken != null && keyFetchToken == null)) {
      throw new IllegalArgumentException("none or both of sessionToken and keyFetchToken may be null");
    }

    byte[] emailUTF8 = email.getBytes("UTF-8");
    byte[] passwordUTF8 = password.getBytes("UTF-8");
    byte[] quickStretchedPW = FxAccountUtils.generateQuickStretchedPW(emailUTF8, passwordUTF8);
    byte[] unwrapBkey = FxAccountUtils.generateUnwrapBKey(quickStretchedPW);

    // Android has internal restrictions that require all values in this
    // bundle to be strings. *sigh*
    Bundle userdata = new Bundle();
    userdata.putString(ACCOUNT_KEY_ACCOUNT_VERSION, "" + CURRENT_ACCOUNT_VERSION);
    userdata.putString(ACCOUNT_KEY_IDP_SERVER, idpServerURI);
    userdata.putString(ACCOUNT_KEY_TOKEN_SERVER, tokenServerURI);
    userdata.putString(ACCOUNT_KEY_AUDIENCE, computeAudience(tokenServerURI));
    userdata.putString(ACCOUNT_KEY_PROFILE, profile);

    ExtendedJSONObject descriptor = new ExtendedJSONObject();
    descriptor.put(BUNDLE_KEY_BUNDLE_VERSION, CURRENT_BUNDLE_VERSION);
    descriptor.put(BUNDLE_KEY_SESSION_TOKEN, sessionToken == null ? null : Utils.byte2Hex(sessionToken));
    descriptor.put(BUNDLE_KEY_KEY_FETCH_TOKEN, keyFetchToken == null ? null : Utils.byte2Hex(keyFetchToken));
    descriptor.put(BUNDLE_KEY_VERIFIED, verified);
    descriptor.put(BUNDLE_KEY_UNWRAPKB, Utils.byte2Hex(unwrapBkey));

    userdata.putString(ACCOUNT_KEY_DESCRIPTOR, descriptor.toJSONString());

    Account account = new Account(email, FxAccountConstants.ACCOUNT_TYPE);
    AccountManager accountManager = AccountManager.get(context);
    boolean added = accountManager.addAccountExplicitly(account, Utils.byte2Hex(quickStretchedPW), userdata);
    if (!added) {
      return null;
    }
    FxAccountAuthenticator.enableSyncing(context, account);
    return account;
  }

  // TODO: this is shit.
  private static String computeAudience(String tokenServerURI) throws URISyntaxException {
     URI uri = new URI(tokenServerURI);
     return new URI(uri.getScheme(), uri.getHost(), null, null).toString();
  }

  @Override
  public boolean isValid() {
    return !getBundleDataBoolean(BUNDLE_KEY_INVALID, false);
  }

  @Override
  public void setInvalid() {
    updateBundleValue(BUNDLE_KEY_INVALID, true);
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
   * <b>For debugging only!</b>
   */
  public void forgetAccountTokens() {
    ExtendedJSONObject descriptor = unbundle();
    if (descriptor == null) {
      return;
    }
    descriptor.remove(BUNDLE_KEY_SESSION_TOKEN);
    descriptor.remove(BUNDLE_KEY_KEY_FETCH_TOKEN);
    persistBundle(descriptor);
  }

  /**
   * <b>For debugging only!</b>
   */
  public void forgetQuickstretchedPW() {
    accountManager.setPassword(account, null);
  }
}
