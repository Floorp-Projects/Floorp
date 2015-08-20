/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.Intent;
import android.util.Log;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Engaged;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;

import java.io.IOError;
import java.io.UnsupportedEncodingException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;

/**
 * Helper class to manage Android Accounts corresponding to Firefox Accounts.
 */
public class AccountsHelper implements NativeEventListener {
    public static final String LOGTAG = "GeckoAccounts";

    protected final Context mContext;
    protected final GeckoProfile mProfile;

    public AccountsHelper(Context context, GeckoProfile profile) {
        mContext = context;
        mProfile = profile;

        EventDispatcher dispatcher = EventDispatcher.getInstance();
        if (dispatcher == null) {
            Log.e(LOGTAG, "Gecko event dispatcher must not be null", new RuntimeException());
            return;
        }
        dispatcher.registerGeckoThreadListener(this,
                "Accounts:CreateFirefoxAccountFromJSON",
                "Accounts:Create",
                "Accounts:Exist");
    }

    public synchronized void uninit() {
        EventDispatcher dispatcher = EventDispatcher.getInstance();
        if (dispatcher == null) {
            Log.e(LOGTAG, "Gecko event dispatcher must not be null", new RuntimeException());
            return;
        }
        dispatcher.unregisterGeckoThreadListener(this,
                "Accounts:CreateFirefoxAccountFromJSON",
                "Accounts:Create",
                "Accounts:Exist");
    }

    @Override
    public void handleMessage(String event, NativeJSObject message, EventCallback callback) {
        if ("Accounts:CreateFirefoxAccountFromJSON".equals(event)) {
            AndroidFxAccount fxAccount = null;
            try {
                final NativeJSObject json = message.getObject("json");
                final String email = json.getString("email");
                final String uid = json.getString("uid");
                final boolean verified = json.optBoolean("verified", false);
                final byte[] unwrapkB = Utils.hex2Byte(json.getString("unwrapBKey"));
                final byte[] sessionToken = Utils.hex2Byte(json.getString("sessionToken"));
                final byte[] keyFetchToken = Utils.hex2Byte(json.getString("keyFetchToken"));
                final String authServerEndpoint =
                        json.optString("authServerEndpoint", FxAccountConstants.DEFAULT_AUTH_SERVER_ENDPOINT);
                final String tokenServerEndpoint =
                        json.optString("tokenServerEndpoint", FxAccountConstants.DEFAULT_TOKEN_SERVER_ENDPOINT);
                final String profileServerEndpoint =
                        json.optString("profileServerEndpoint", FxAccountConstants.DEFAULT_PROFILE_SERVER_ENDPOINT);
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
            } catch (URISyntaxException | GeneralSecurityException | UnsupportedEncodingException e) {
                Log.w(LOGTAG, "Got exception creating Firefox Account from JSON; ignoring.", e);
                if (callback != null) {
                    callback.sendError("Could not create Firefox Account from JSON: " + e.toString());
                    return;
                }
            }
            if (callback != null) {
                callback.sendSuccess(fxAccount != null);
            }

        } else if ("Accounts:Create".equals(event)) {
            // Do exactly the same thing as if you tapped 'Sync' in Settings.
            final Intent intent = new Intent(FxAccountConstants.ACTION_FXA_GET_STARTED);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            final NativeJSObject extras = message.optObject("extras", null);
            if (extras != null) {
                intent.putExtra("extras", extras.toString());
            }
            mContext.startActivity(intent);

        } else if ("Accounts:Exist".equals(event)) {
            if (callback == null) {
                Log.w(LOGTAG, "Accounts:Exist requires a callback");
                return;
            }

            final String kind = message.optString("kind", null);
            final JSONObject response = new JSONObject();

            try {
                if ("any".equals(kind)) {
                    response.put("exists", SyncAccounts.syncAccountsExist(mContext) ||
                            FirefoxAccounts.firefoxAccountsExist(mContext));
                    callback.sendSuccess(response);
                } else if ("fxa".equals(kind)) {
                    response.put("exists", FirefoxAccounts.firefoxAccountsExist(mContext));
                    callback.sendSuccess(response);
                } else if ("sync11".equals(kind)) {
                    response.put("exists", SyncAccounts.syncAccountsExist(mContext));
                    callback.sendSuccess(response);
                } else {
                    callback.sendError("Could not query account existence: unknown kind.");
                }
            } catch (JSONException e) {
                Log.w(LOGTAG, "Got exception querying account existence; ignoring.", e);
                callback.sendError("Could not query account existence: " + e.toString());
                return;
            }
        }
    }
}
