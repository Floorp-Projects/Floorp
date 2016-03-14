/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import org.mozilla.bouncer.BouncerService;

/**
 * Bouncer activity version of BrowserApp.
 *
 * This class has the same name as org.mozilla.gecko.BrowserApp to preserve
 * shortcuts created by the bouncer app.
 */
public class BrowserApp extends Activity {
    private static final String LOGTAG = "GeckoBouncerActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // This races distribution installation against the Play Store killing our process to
        // install the update.  We'll live with it.  To do better, consider using an Intent to
        // notify when the service has completed.
        startService(new Intent(this, BouncerService.class));

        final String appPackageName = Uri.encode(getPackageName());
        final Uri uri = Uri.parse("market://details?id=" + appPackageName);
        Log.i(LOGTAG, "Lanching activity with URL: " + uri.toString());

        // It might be more correct to catch failure in case the Play Store isn't installed.  The
        // fallback action is to open the Play Store website... but doing so may offer Firefox as
        // browser (since even the bouncer offers to view URLs), which will be very confusing.
        // Therefore, we don't try to be fancy here, and we just fail (silently).
        startActivity(new Intent(Intent.ACTION_VIEW, uri));

        finish();
    }
}
