/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.customtabs.CustomTabsIntent;

import org.mozilla.gecko.customtabs.CustomTabsActivity;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.tabqueue.TabQueueHelper;
import org.mozilla.gecko.tabqueue.TabQueueService;

/**
 * Activity that receives incoming Intents and dispatches them to the appropriate activities (e.g. browser, custom tabs, web app).
 */
public class LauncherActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        GeckoAppShell.ensureCrashHandling();

        final SafeIntent safeIntent = new SafeIntent(getIntent());

        // If it's not a view intent, it won't be a custom tabs intent either. Just launch!
        if (!isViewIntentWithURL(safeIntent)) {
            dispatchNormalIntent();

        // Is this a custom tabs intent, and are custom tabs enabled?
        } else if (AppConstants.MOZ_ANDROID_CUSTOM_TABS && isCustomTabsIntent(safeIntent)
                && isCustomTabsEnabled()) {
            dispatchCustomTabsIntent();

        // Can we dispatch this VIEW action intent to the tab queue service?
        } else if (!safeIntent.getBooleanExtra(BrowserContract.SKIP_TAB_QUEUE_FLAG, false)
                && TabQueueHelper.TAB_QUEUE_ENABLED
                && TabQueueHelper.isTabQueueEnabled(this)) {
            dispatchTabQueueIntent();

        // Dispatch this VIEW action intent to the browser.
        } else {
            dispatchNormalIntent();
        }

        finish();
    }

    /**
     * Launch tab queue service to display overlay.
     */
    private void dispatchTabQueueIntent() {
        Intent intent = new Intent(getIntent());
        intent.setClass(getApplicationContext(), TabQueueService.class);
        startService(intent);
    }

    /**
     * Launch the browser activity.
     */
    private void dispatchNormalIntent() {
        Intent intent = new Intent(getIntent());
        intent.setClassName(getApplicationContext(), AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
        startActivity(intent);
    }

    private void dispatchCustomTabsIntent() {
        Intent intent = new Intent(getIntent());
        intent.setClassName(getApplicationContext(), CustomTabsActivity.class.getName());
        startActivity(intent);
    }

    private static boolean isViewIntentWithURL(@NonNull final SafeIntent safeIntent) {
        return Intent.ACTION_VIEW.equals(safeIntent.getAction())
                && safeIntent.getDataString() != null;
    }

    private static boolean isCustomTabsIntent(@NonNull final SafeIntent safeIntent) {
        return isViewIntentWithURL(safeIntent)
                && safeIntent.hasExtra(CustomTabsIntent.EXTRA_SESSION);
    }

    private boolean isCustomTabsEnabled() {
        return GeckoSharedPrefs.forApp(this).getBoolean(GeckoPreferences.PREFS_CUSTOM_TABS, false);
    }
}
