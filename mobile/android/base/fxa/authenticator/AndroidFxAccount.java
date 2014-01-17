/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.Collections;

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

  public static final String ACCOUNT_KEY_ASSERTION = "assertion";
  public static final String ACCOUNT_KEY_CERTIFICATE = "certificate";
  public static final String ACCOUNT_KEY_INVALID = "invalid";
  public static final String ACCOUNT_KEY_SERVERURI = "serverURI";
  public static final String ACCOUNT_KEY_SESSION_TOKEN = "sessionToken";
  public static final String ACCOUNT_KEY_KEY_FETCH_TOKEN = "keyFetchToken";
  public static final String ACCOUNT_KEY_VERIFIED = "verified";
  public static final String ACCOUNT_KEY_KA = "kA";
  public static final String ACCOUNT_KEY_KB = "kB";
  public static final String ACCOUNT_KEY_UNWRAPKB = "unwrapkB";
  public static final String ACCOUNT_KEY_ASSERTION_KEY_PAIR = "assertionKeyPair";

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

  @Override
  public byte[] getEmailUTF8() {
    try {
      return account.name.getBytes("UTF-8");
    } catch (UnsupportedEncodingException e) {
      // Ignore.
      return null;
    }
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
  public String getServerURI() {
    return accountManager.getUserData(account, ACCOUNT_KEY_SERVERURI);
  }

  protected byte[] getUserDataBytes(String key) {
    String data = accountManager.getUserData(account, key);
    if (data == null) {
      return null;
    }
    return Utils.hex2Byte(data);
  }

  @Override
  public byte[] getSessionToken() {
    return getUserDataBytes(ACCOUNT_KEY_SESSION_TOKEN);
  }

  @Override
  public byte[] getKeyFetchToken() {
    return getUserDataBytes(ACCOUNT_KEY_KEY_FETCH_TOKEN);
  }

  @Override
  public void setSessionToken(byte[] sessionToken) {
    accountManager.setUserData(account, ACCOUNT_KEY_SESSION_TOKEN, sessionToken == null ? null : Utils.byte2Hex(sessionToken));
  }

  @Override
  public void setKeyFetchToken(byte[] keyFetchToken) {
    accountManager.setUserData(account, ACCOUNT_KEY_KEY_FETCH_TOKEN, keyFetchToken == null ? null : Utils.byte2Hex(keyFetchToken));
  }

  @Override
  public boolean isVerified() {
    String data = accountManager.getUserData(account, ACCOUNT_KEY_VERIFIED);
    return Boolean.valueOf(data);
  }

  @Override
  public void setVerified() {
    accountManager.setUserData(account, ACCOUNT_KEY_VERIFIED, Boolean.valueOf(true).toString());
  }

  @Override
  public byte[] getKa() {
    return getUserDataBytes(ACCOUNT_KEY_KA);
  }

  @Override
  public void setKa(byte[] kA) {
    accountManager.setUserData(account, ACCOUNT_KEY_KA, Utils.byte2Hex(kA));
  }

  @Override
  public void setWrappedKb(byte[] wrappedKb) {
    byte[] unwrapKb = getUserDataBytes(ACCOUNT_KEY_UNWRAPKB);
    byte[] kB = new byte[wrappedKb.length]; // We could hard-code this to be 32.
    for (int i = 0; i < wrappedKb.length; i++) {
      kB[i] = (byte) (wrappedKb[i] ^ unwrapKb[i]);
    }
    accountManager.setUserData(account, ACCOUNT_KEY_KB, Utils.byte2Hex(kB));
  }

  @Override
  public byte[] getKb() {
    return getUserDataBytes(ACCOUNT_KEY_KB);
  }

  protected BrowserIDKeyPair generateNewAssertionKeyPair() throws GeneralSecurityException {
    Logger.info(LOG_TAG, "Generating new assertion key pair.");
    // TODO Have the key size be a non-constant in FxAccountUtils, or read from SharedPreferences, or...
    return RSACryptoImplementation.generateKeyPair(1024);
  }

  @Override
  public BrowserIDKeyPair getAssertionKeyPair() throws GeneralSecurityException {
    try {
      String data = accountManager.getUserData(account, ACCOUNT_KEY_ASSERTION_KEY_PAIR);
      return RSACryptoImplementation.fromJSONObject(new ExtendedJSONObject(data));
    } catch (Exception e) {
      // Fall through to generating a new key pair.
    }

    BrowserIDKeyPair keyPair = generateNewAssertionKeyPair();
    accountManager.setUserData(account, ACCOUNT_KEY_ASSERTION_KEY_PAIR, keyPair.toJSONObject().toJSONString());
    return keyPair;
  }

  @Override
  public String getCertificate() {
    return accountManager.getUserData(account, ACCOUNT_KEY_CERTIFICATE);
  }

  @Override
  public void setCertificate(String certificate) {
    accountManager.setUserData(account, ACCOUNT_KEY_CERTIFICATE, certificate);
  }

  @Override
  public String getAssertion() {
    return accountManager.getUserData(account, ACCOUNT_KEY_ASSERTION);
  }

  @Override
  public void setAssertion(String assertion) {
    accountManager.setUserData(account, ACCOUNT_KEY_ASSERTION, assertion);
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
    ExtendedJSONObject o = new ExtendedJSONObject();
    for (String key : new String[] {
        ACCOUNT_KEY_ASSERTION,
        ACCOUNT_KEY_CERTIFICATE,
        ACCOUNT_KEY_SERVERURI,
        ACCOUNT_KEY_SESSION_TOKEN,
        ACCOUNT_KEY_INVALID,
        ACCOUNT_KEY_KEY_FETCH_TOKEN,
        ACCOUNT_KEY_VERIFIED,
        ACCOUNT_KEY_KA,
        ACCOUNT_KEY_KB,
        ACCOUNT_KEY_UNWRAPKB,
        ACCOUNT_KEY_ASSERTION_KEY_PAIR,
    }) {
      o.put(key, accountManager.getUserData(account, key));
    }
    o.put("email", account.name);
    try {
      o.put("emailUTF8", Utils.byte2Hex(account.name.getBytes("UTF-8")));
    } catch (UnsupportedEncodingException e) {
      // Ignore.
    }
    o.put("quickStretchedPW", accountManager.getPassword(account));
    return o;
  }

  public static Account addAndroidAccount(Context context, String email, String password,
      String serverURI, byte[] sessionToken, byte[] keyFetchToken, boolean verified)
          throws UnsupportedEncodingException, GeneralSecurityException {
    if (email == null) {
      throw new IllegalArgumentException("email must not be null");
    }
    if (password == null) {
      throw new IllegalArgumentException("password must not be null");
    }
    if (serverURI == null) {
      throw new IllegalArgumentException("serverURI must not be null");
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

    Bundle userdata = new Bundle();
    userdata.putString(AndroidFxAccount.ACCOUNT_KEY_SERVERURI, serverURI);
    userdata.putString(AndroidFxAccount.ACCOUNT_KEY_SESSION_TOKEN, sessionToken == null ? null : Utils.byte2Hex(sessionToken));
    userdata.putString(AndroidFxAccount.ACCOUNT_KEY_KEY_FETCH_TOKEN, keyFetchToken == null ? null : Utils.byte2Hex(keyFetchToken));
    userdata.putString(AndroidFxAccount.ACCOUNT_KEY_VERIFIED, Boolean.valueOf(verified).toString());
    userdata.putString(AndroidFxAccount.ACCOUNT_KEY_UNWRAPKB, Utils.byte2Hex(unwrapBkey));

    Account account = new Account(email, FxAccountConstants.ACCOUNT_TYPE);
    AccountManager accountManager = AccountManager.get(context);
    boolean added = accountManager.addAccountExplicitly(account, Utils.byte2Hex(quickStretchedPW), userdata);
    if (!added) {
      return null;
    }
    FxAccountAuthenticator.enableSyncing(context, account);
    return account;
  }

  @Override
  public boolean isValid() {
    // Boolean.valueOf only returns true for the string "true"; this errors in
    // the direction of marking accounts valid.
    boolean invalid = Boolean.valueOf(accountManager.getUserData(account, ACCOUNT_KEY_INVALID)).booleanValue();
    return !invalid;
  }

  @Override
  public void setInvalid() {
    accountManager.setUserData(account, ACCOUNT_KEY_INVALID, Boolean.valueOf(true).toString());
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
      FxAccountConstants.pii(LOG_TAG, key + ": " + o.getString(key));
    }
  }

  /**
   * <b>For debugging only!</b>
   */
  public void forgetAccountTokens() {
    accountManager.setUserData(account, ACCOUNT_KEY_SESSION_TOKEN, null);
    accountManager.setUserData(account, ACCOUNT_KEY_KEY_FETCH_TOKEN, null);
  }

  /**
   * <b>For debugging only!</b>
   */
  public void forgetQuickstretchedPW() {
    accountManager.setPassword(account, null);
  }
}
