/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.IntentService;
import android.content.Intent;
import android.util.Log;

public class GeckoService extends IntentService {

    private static final String LOGTAG = "GeckoService";
    private static final boolean DEBUG = false;


    public GeckoService() {
        super("GeckoService");
    }

    @Override // Service
    public void onCreate() {
        GeckoAppShell.ensureCrashHandling();
        GeckoAppShell.setApplicationContext(getApplicationContext());

        super.onCreate();

        if (DEBUG) {
            Log.d(LOGTAG, "Created");
        }
        GeckoThread.ensureInit(/* args */ null, /* action */ null);
        GeckoThread.launch();
    }

    @Override // Service
    public void onDestroy() {
        // TODO: Make sure Gecko is in a somewhat idle state
        // because our process could be killed at anytime.

        if (DEBUG) {
            Log.d(LOGTAG, "Destroyed");
        }
        super.onDestroy();
    }

    @Override // IntentService
    protected void onHandleIntent(final Intent intent) {

        if (intent == null) {
            return;
        }

        // We are on the intent service worker thread.
        if (DEBUG) {
            Log.d(LOGTAG, "Handling " + intent.getAction());
        }
    }
}
