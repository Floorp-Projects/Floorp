/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import org.mozilla.gecko.mozglue.GeckoLoader;

/**
 * This broadcast receiver receives ACTION_MY_PACKAGE_REPLACED broadcasts and
 * starts procedures that should run after the APK has been updated.
 */
public class PackageReplacedReceiver extends BroadcastReceiver {
    public static final String ACTION_MY_PACKAGE_REPLACED = "android.intent.action.MY_PACKAGE_REPLACED";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null || !ACTION_MY_PACKAGE_REPLACED.equals(intent.getAction())) {
            // This is not the broadcast we are looking for.
            return;
        }

        // Extract Gecko libs to allow them to be loaded from cache on startup.
        extractGeckoLibs(context);
    }

    private static void extractGeckoLibs(final Context context) {
        final String resourcePath = context.getPackageResourcePath();
        GeckoLoader.loadMozGlue(context);
        GeckoLoader.extractGeckoLibs(context, resourcePath);
    }
}
