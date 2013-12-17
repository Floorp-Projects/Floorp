/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.EventDispatcher;
import org.mozilla.gecko.util.GeckoEventListener;

import org.mozilla.gecko.fxa.authenticator.FxAccountAuthenticator;

import org.json.JSONException;
import org.json.JSONObject;

import android.accounts.Account;
import android.content.Context;

import android.util.Log;

/**
 * Helper class to manage Firefox Accounts.
 *
 * Listens for messages starting "FxAccount:" from Javascript, and creates a new
 * Android account object in response to "FxAccount:{Verified,Login}".
 *
 * "FxAccount:Create" is ignored: it corresponds to a user creating a Firefox
 * Account on the server; but they have not yet completed an email verification
 * loop.  The provided data will not include the users keys.
 *
 * "FxAccount:Verified" corresponds to a user signing up: creating a Firefox
 * Account on the server, and then completing an email verification loop while
 * still in the about:accounts window.
 *
 * "FxAccount:Login" corresponds to a user signing in to an existing verified
 * Firefox Account.
 */
public final class FirefoxAccountsHelper
             implements GeckoEventListener
{
    public static final String LOGTAG = "FxAcctsHelper";

    // For extra debugging.  Not final so it can be changed by an add-on.
    public static boolean LOG_PERSONAL_INFORMATION = false;

    public static final String EVENT_CREATE   = "FxAccount:Create";
    public static final String EVENT_LOGIN    = "FxAccount:Login";
    public static final String EVENT_VERIFIED = "FxAccount:Verified";

    protected final Context mContext;

    public FirefoxAccountsHelper(Context context) {
        mContext = context;

        EventDispatcher dispatcher = GeckoAppShell.getEventDispatcher();
        if (dispatcher == null) {
            Log.e(LOGTAG, "Gecko event dispatcher must not be null", new RuntimeException());
            return;
        }
        dispatcher.registerEventListener(EVENT_CREATE, this);
        dispatcher.registerEventListener(EVENT_LOGIN, this);
        dispatcher.registerEventListener(EVENT_VERIFIED, this);
    }

    public synchronized void uninit() {
        EventDispatcher dispatcher = GeckoAppShell.getEventDispatcher();
        if (dispatcher == null) {
            Log.e(LOGTAG, "Gecko event dispatcher must not be null", new RuntimeException());
            return;
        }
        dispatcher.unregisterEventListener(EVENT_CREATE, this);
        dispatcher.unregisterEventListener(EVENT_LOGIN, this);
        dispatcher.unregisterEventListener(EVENT_VERIFIED, this);
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        Log.i(LOGTAG, "FirefoxAccountsHelper got event " + event);
        if (!(EVENT_CREATE.equals(event) ||
              EVENT_LOGIN.equals(event) ||
              EVENT_VERIFIED.equals(event))) {
            Log.e(LOGTAG, "FirefoxAccountsHelper got unexpected event " + event);
            return;
        }

        if (EVENT_CREATE.equals(event)) {
            Log.i(LOGTAG, "FirefoxAccountsHelper ignoring event " + event);
            return;
        }

        try {
            final JSONObject data = message.getJSONObject("data");
            if (data == null) {
                Log.e(LOGTAG, "data must not be null");
                return;
            }

            if (LOG_PERSONAL_INFORMATION) {
                Log.w(LOGTAG, "data: " + data.toString());
            }

            String email = data.optString("email");
            String uid = data.optString("uid");
            String sessionToken = data.optString("sessionToken");
            String kA = data.optString("kA");
            String kB = data.optString("kB");

            if (LOG_PERSONAL_INFORMATION) {
                Log.w(LOGTAG, "email: " + email);
                Log.w(LOGTAG, "uid: " + uid);
                Log.w(LOGTAG, "sessionToken: " + sessionToken);
                Log.w(LOGTAG, "kA: " + kA);
                Log.w(LOGTAG, "kB: " + kB);
            }

            Account account = FxAccountAuthenticator.addAccount(mContext, email, uid, sessionToken, kA, kB);
            if (account == null) {
                Log.e(LOGTAG, "Got null adding FxAccount.");
                return;
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Got exception in handleMessage handling event " + event, e);
            return;
        }
    }
}
