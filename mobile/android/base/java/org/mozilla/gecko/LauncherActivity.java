/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

import org.mozilla.gecko.db.BrowserContract;
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

        if (isViewIntent()) {
            dispatchViewIntent();
        } else {
            dispatchNormalIntent();
        }

        finish();
    }

    /**
     * Dispatch a VIEW action intent; either to the browser or to the tab queue service.
     */
    private void dispatchViewIntent() {
        if (TabQueueHelper.TAB_QUEUE_ENABLED
                && TabQueueHelper.isTabQueueEnabled(this)
                && !getIntent().getBooleanExtra(BrowserContract.SKIP_TAB_QUEUE_FLAG, false)) {
            dispatchTabQueueIntent();
        } else {
            dispatchNormalIntent();
        }
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

    private boolean isViewIntent() {
        final String action = getIntent().getAction();
        return Intent.ACTION_VIEW.equals(action);
    }
}
