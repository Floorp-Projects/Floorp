/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import org.json.JSONException;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Engaged;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.restrictions.Restrictable;
import org.mozilla.gecko.restrictions.Restrictions;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.HashMap;
import java.util.Map;

/**
 * Helper class to manage Android Accounts corresponding to Firefox Accounts.
 */
public class AccountsHelper implements BundleEventListener {
    private static final String LOGTAG = "GeckoAccounts";

    protected final Context mContext;
    protected final GeckoProfile mProfile;

    public AccountsHelper(Context context, GeckoProfile profile) {
        mContext = context;
        mProfile = profile;

        EventDispatcher.getInstance().registerGeckoThreadListener(this,
                "Accounts:CreateFirefoxAccountFromJSON",
                "Accounts:UpdateFirefoxAccountFromJSON",
                "Accounts:DeleteFirefoxAccount",
                "Accounts:Exist",
                "Accounts:ProfileUpdated");
        EventDispatcher.getInstance().registerUiThreadListener(this,
                "Accounts:Create",
                "Accounts:ShowSyncPreferences");
    }

    public synchronized void uninit() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
                "Accounts:CreateFirefoxAccountFromJSON",
                "Accounts:UpdateFirefoxAccountFromJSON",
                "Accounts:DeleteFirefoxAccount",
                "Accounts:Exist",
                "Accounts:ProfileUpdated");
        EventDispatcher.getInstance().unregisterUiThreadListener(this,
                "Accounts:Create",
                "Accounts:ShowSyncPreferences");
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if (!Restrictions.isAllowed(mContext, Restrictable.MODIFY_ACCOUNTS)) {
            // We register for messages in all contexts; we drop, with a log and an error to JavaScript,
            // when the profile is restricted.  It's better to return errors than silently ignore messages.
            Log.e(LOGTAG, "Profile is not allowed to modify accounts!  Ignoring event: " + event);
            if (callback != null) {
                callback.sendError("Profile is not allowed to modify accounts!");
            }
            return;
        }

        if ("Accounts:CreateFirefoxAccountFromJSON".equals(event)) {
            // As we are about to create a new account, let's ensure our in-memory accounts cache
            // is empty so that there are no undesired side-effects.
            AndroidFxAccount.invalidateCaches();

            AndroidFxAccount fxAccount = null;
            try {
                final GeckoBundle json = message.getBundle("json");
                final String email = json.getString("email");
                final String uid = json.getString("uid");
                final boolean verified = json.getBoolean("verified", false);
                final byte[] unwrapkB = Utils.hex2Byte(json.getString("unwrapBKey"));
                final byte[] sessionToken = Utils.hex2Byte(json.getString("sessionToken"));
                final byte[] keyFetchToken = Utils.hex2Byte(json.getString("keyFetchToken"));
                final String authServerEndpoint = json.getString("authServerEndpoint",
                        FxAccountConstants.DEFAULT_AUTH_SERVER_ENDPOINT);
                final String tokenServerEndpoint = json.getString("tokenServerEndpoint",
                        FxAccountConstants.DEFAULT_TOKEN_SERVER_ENDPOINT);
                final String profileServerEndpoint = json.getString("profileServerEndpoint",
                        FxAccountConstants.DEFAULT_PROFILE_SERVER_ENDPOINT);
                // TODO: handle choose what to Sync.
                State state = new Engaged(email, uid, verified, unwrapkB, sessionToken, keyFetchToken);
                fxAccount = AndroidFxAccount.addAndroidAccount(mContext,
                        email,
                        mProfile.getName(),
                        authServerEndpoint,
                        tokenServerEndpoint,
                        profileServerEndpoint,
                        state,
                        AndroidFxAccount.DEFAULT_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP);

                final String[] declinedSyncEngines = json.getStringArray("declinedSyncEngines");
                if (declinedSyncEngines != null) {
                    Log.i(LOGTAG, "User has selected engines; storing to prefs.");
                    final Map<String, Boolean> selectedEngines = new HashMap<String, Boolean>();
                    for (String enabledSyncEngine : SyncConfiguration.validEngineNames()) {
                        selectedEngines.put(enabledSyncEngine, true);
                    }
                    for (String declinedSyncEngine : declinedSyncEngines) {
                        selectedEngines.put(declinedSyncEngine, false);
                    }
                    // The "forms" engine has the same state as the "history" engine.
                    selectedEngines.put("forms", selectedEngines.get("history"));
                    FxAccountUtils.pii(LOGTAG, "User selected engines: " +
                                               selectedEngines.toString());
                    try {
                        SyncConfiguration.storeSelectedEnginesToPrefs(
                                fxAccount.getSyncPrefs(), selectedEngines);
                    } catch (UnsupportedEncodingException | GeneralSecurityException e) {
                        Log.e(LOGTAG, "Got exception storing selected engines; ignoring.", e);
                    }
                }
            } catch (URISyntaxException | GeneralSecurityException |
                     UnsupportedEncodingException e) {
                Log.w(LOGTAG, "Got exception creating Firefox Account from JSON; ignoring.", e);
                if (callback != null) {
                    callback.sendError("Could not create Firefox Account from JSON: " +
                                       e.toString());
                    return;
                }
            }
            if (callback != null) {
                callback.sendSuccess(fxAccount != null);
            }

        } else if ("Accounts:UpdateFirefoxAccountFromJSON".equals(event)) {
            // We might be significantly changing state of the account; let's ensure our in-memory
            // accounts cache is empty so that there are no undesired side-effects.
            AndroidFxAccount.invalidateCaches();

            final Account account = FirefoxAccounts.getFirefoxAccount(mContext);
            if (account == null) {
                if (callback != null) {
                    callback.sendError("Could not update Firefox Account since none exists");
                }
                return;
            }

            final GeckoBundle json = message.getBundle("json");
            final String email = json.getString("email");
            final String uid = json.getString("uid");

            // Protect against cross-connecting accounts.
            if (account.name == null || !account.name.equals(email)) {
                final String errorMessage = "Cannot update Firefox Account from JSON: " +
                        "datum has different email address!";
                Log.e(LOGTAG, errorMessage);
                if (callback != null) {
                    callback.sendError(errorMessage);
                }
                return;
            }

            final boolean verified = json.getBoolean("verified", false);
            final byte[] unwrapkB = Utils.hex2Byte(json.getString("unwrapBKey", ""));
            final byte[] sessionToken = Utils.hex2Byte(json.getString("sessionToken", ""));
            final byte[] keyFetchToken = Utils.hex2Byte(json.getString("keyFetchToken", ""));

            if (unwrapkB.length == 0 || sessionToken.length == 0 || keyFetchToken.length == 0) {
                final String error = "Cannot update Firefox Account from JSON: invalid key/tokens";
                Log.e(LOGTAG, error);
                if (callback != null) {
                    callback.sendError(error);
                }
                return;
            }

            final State state = new Engaged(email, uid, verified, unwrapkB,
                                            sessionToken, keyFetchToken);

            final AndroidFxAccount fxAccount = new AndroidFxAccount(mContext, account);
            fxAccount.setState(state);

            if (callback != null) {
                callback.sendSuccess(true);
            }

        } else if ("Accounts:Create".equals(event)) {
            // Do exactly the same thing as if you tapped 'Sync' in Settings.
            final Intent intent = new Intent(FxAccountConstants.ACTION_FXA_GET_STARTED);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            final GeckoBundle extras = message.getBundle("extras");
            if (extras != null) {
                try {
                    intent.putExtra("extras", extras.toJSONObject().toString());
                } catch (final JSONException e) {
                    Log.e(LOGTAG, "Cannot convert extras", e);
                }
            }
            mContext.startActivity(intent);

        } else if ("Accounts:DeleteFirefoxAccount".equals(event)) {
            try {
                final Account account = FirefoxAccounts.getFirefoxAccount(mContext);
                if (account == null) {
                    Log.w(LOGTAG, "Could not delete Firefox Account since none exists!");
                    if (callback != null) {
                        callback.sendError("Could not delete Firefox Account since none exists");
                    }
                    return;
                }

                final AccountManagerCallback<Boolean> accountManagerCallback =
                        new AccountManagerCallback<Boolean>() {
                    @Override
                    public void run(AccountManagerFuture<Boolean> future) {
                        try {
                            final boolean result = future.getResult();
                            Log.i(LOGTAG, "Account named like " +
                                          Utils.obfuscateEmail(account.name) + " removed: " +
                                          result);
                            if (callback != null) {
                                callback.sendSuccess(result);
                            }
                        } catch (OperationCanceledException | IOException |
                                 AuthenticatorException e) {
                            if (callback != null) {
                                callback.sendError("Could not delete Firefox Account: " +
                                                   e.toString());
                            }
                        }
                    }
                };

                AccountManager.get(mContext).removeAccount(
                        account, accountManagerCallback, ThreadUtils.getBackgroundHandler());
            } catch (Exception e) {
                Log.w(LOGTAG, "Got exception updating Firefox Account from JSON; ignoring.", e);
                if (callback != null) {
                    callback.sendError("Could not update Firefox Account from JSON: " +
                                       e.toString());
                    return;
                }
            }

        } else if ("Accounts:Exist".equals(event)) {
            if (callback == null) {
                Log.w(LOGTAG, "Accounts:Exist requires a callback");
                return;
            }

            final String kind = message.getString("kind", null);
            final GeckoBundle response = new GeckoBundle();

            if ("any".equals(kind)) {
                response.putBoolean("exists", FirefoxAccounts.firefoxAccountsExist(mContext));
                callback.sendSuccess(response);
            } else if ("fxa".equals(kind)) {
                final Account account = FirefoxAccounts.getFirefoxAccount(mContext);
                response.putBoolean("exists", account != null);
                if (account != null) {
                    response.putString("email", account.name);
                    // We should always be able to extract the server endpoints.
                    final AndroidFxAccount fxAccount = new AndroidFxAccount(mContext, account);
                    response.putString("authServerEndpoint", fxAccount.getAccountServerURI());
                    response.putString("profileServerEndpoint", fxAccount.getProfileServerURI());
                    response.putString("tokenServerEndpoint", fxAccount.getTokenServerURI());
                    try {
                        // It is possible for the state fetch to fail and us to not be
                        // able to provide a UID.  Long term, the UID (and verification
                        // flag) will be attached to the Android account user data and not
                        // the internal state representation.
                        final State state = fxAccount.getState();
                        response.putString("uid", state.uid);
                    } catch (Exception e) {
                        Log.w(LOGTAG, "Got exception extracting account UID; ignoring.", e);
                    }
                }
                callback.sendSuccess(response);
            } else {
                callback.sendError("Could not query account existence: unknown kind.");
            }

        } else if ("Accounts:ProfileUpdated".equals(event)) {
            final Account account = FirefoxAccounts.getFirefoxAccount(mContext);
            if (account == null) {
                Log.w(LOGTAG, "Can't change profile of non-existent Firefox Account!; ignored");
                return;
            }
            final AndroidFxAccount androidFxAccount = new AndroidFxAccount(mContext, account);
            androidFxAccount.fetchProfileJSON();

        } else if ("Accounts:ShowSyncPreferences".equals(event)) {
            final Account account = FirefoxAccounts.getFirefoxAccount(mContext);
            if (account == null) {
                Log.w(LOGTAG, "Can't change show Sync preferences of " +
                              "non-existent Firefox Account!; ignored");
                return;
            }
            // We don't necessarily have an Activity context here, so we always start in a new task.
            final Intent intent = new Intent(FxAccountConstants.ACTION_FXA_STATUS);
            intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mContext.startActivity(intent);
        }
    }
}
