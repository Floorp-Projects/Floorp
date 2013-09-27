/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

public class Restarter extends Activity {
    private static final String LOGTAG = "GeckoRestarter";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.i(LOGTAG, "Trying to restart " + AppConstants.MOZ_APP_NAME);
        try {
            int countdown = 40;
            while (GeckoAppShell.checkForGeckoProcs() &&  --countdown > 0) {
                // Wait for the old process to die before we continue
                try {
                    Thread.sleep(100);
                } catch (InterruptedException ie) {}
            }

            if (countdown <= 0) {
                // if the countdown expired, something is hung
                GeckoAppShell.killAnyZombies();
                countdown = 10;
                // wait for the kill to take effect
                while (GeckoAppShell.checkForGeckoProcs() &&  --countdown > 0) {
                    try {
                        Thread.sleep(100);
                    } catch (InterruptedException ie) {}
                }
            }
        } catch (Exception e) {
            Log.i(LOGTAG, e.toString());
        }
        try {
            Intent intent = new Intent(Intent.ACTION_MAIN);
            intent.setClassName(AppConstants.ANDROID_PACKAGE_NAME,
                                AppConstants.BROWSER_INTENT_CLASS);
            Bundle b = getIntent().getExtras();
            if (b != null)
                intent.putExtras(b);
            Log.i(LOGTAG, intent.toString());
            startActivity(intent);
        } catch (Exception e) {
            Log.i(LOGTAG, e.toString());
        }
    }
}
