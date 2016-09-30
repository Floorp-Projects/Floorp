/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.customtabs;

import android.net.Uri;
import android.os.Bundle;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsSessionToken;
import android.util.Log;

import java.util.List;

/**
 * Custom tabs service external, third-party apps connect to.
 */
public class GeckoCustomTabsService extends CustomTabsService {
    private static final String LOGTAG = "GeckoCustomTabsService";

    @Override
    protected boolean warmup(long flags) {
        Log.v(LOGTAG, "warmup()");

        // Pretend warmup was successful
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
        Log.v(LOGTAG, "mayLaunchUrl()");

        return false;
    }

    @Override
    protected Bundle extraCommand(String commandName, Bundle bundle) {
        Log.v(LOGTAG, "extraCommand()");

        return null;
    }
}
