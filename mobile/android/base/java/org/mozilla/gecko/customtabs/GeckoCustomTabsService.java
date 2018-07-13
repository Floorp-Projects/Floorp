/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.customtabs;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsSessionToken;
import android.util.Log;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoService;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;

import java.util.List;

/**
 * Custom tabs service external, third-party apps connect to.
 */
public class GeckoCustomTabsService extends CustomTabsService {
    private static final String LOGTAG = "GeckoCustomTabsService";
    private static final boolean DEBUG = false;
    private static final int MAX_SPECULATIVE_URLS = 50;

    @Override
    protected boolean updateVisuals(CustomTabsSessionToken sessionToken, Bundle bundle) {
        Log.v(LOGTAG, "updateVisuals()");

        return false;
    }

    @Override
    protected boolean warmup(long flags) {

        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.SERVICE, "customtab-warmup");

        if (DEBUG) {
            Log.v(LOGTAG, "warming up...");
        }

        if (GeckoThread.isRunning()) {
            return true;
        }

        final Intent intent = GeckoService.getIntentToStartGecko(this);
        // Use a default profile for warming up Gecko.
        final GeckoProfile profile = GeckoProfile.get(this);
        GeckoService.setIntentProfile(intent, profile.getName(), profile.getDir().getAbsolutePath());
        startService(intent);
        return true;
    }

    @Override
    protected boolean newSession(CustomTabsSessionToken sessionToken) {
        Log.v(LOGTAG, "newSession()");

        // Pretend session has been started
        return true;
    }

    @Override
    protected boolean mayLaunchUrl(CustomTabsSessionToken sessionToken, Uri uri, Bundle bundle, List<Bundle> list) {
        if (DEBUG) {
            Log.v(LOGTAG, "opening speculative connections...");
        }

        if (uri == null) {
            return false;
        }

        GeckoThread.speculativeConnect(uri.toString());

        if (list == null) {
            return true;
        }

        for (int i = 0; i < list.size() && i < MAX_SPECULATIVE_URLS; i++) {
            Bundle listItem = list.get(i);
            if (listItem == null) {
                continue;
            }

            Uri listUri = listItem.getParcelable(KEY_URL);
            if (listUri == null) {
                continue;
            }

            GeckoThread.speculativeConnect(listUri.toString());
        }

        return true;
    }

    @Override
    protected Bundle extraCommand(String commandName, Bundle bundle) {
        Log.v(LOGTAG, "extraCommand()");

        return null;
    }

    @Override
    protected boolean requestPostMessageChannel(CustomTabsSessionToken sessionToken, Uri postMessageOrigin) {
        return false;
    }

    @Override
    protected int postMessage(CustomTabsSessionToken sessionToken, String message, Bundle extras) {
        return RESULT_FAILURE_DISALLOWED;
    }
}
