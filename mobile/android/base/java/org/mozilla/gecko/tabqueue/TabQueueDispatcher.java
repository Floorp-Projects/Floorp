/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabqueue;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.mozglue.SafeIntentUtils;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;

/**
 * This class takes over external url loads (Intent.VIEW) from the BrowserApp class.  It determines if
 * the tab queue functionality is enabled and forwards the intent to the TabQueueService to process if it is.
 *
 * If the tab queue functionality is not enabled then it forwards the intent to BrowserApp to handle as normal.
 */
public class TabQueueDispatcher extends Locales.LocaleAwareActivity {
    private static final String LOGTAG = "Gecko" + TabQueueDispatcher.class.getSimpleName();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        GeckoAppShell.ensureCrashHandling();

        // The EXCLUDE_FROM_RECENTS flag is sticky
        // (see http://grepcode.com/file/repository.grepcode.com/java/ext/com.google.android/android/5.1.0_r1/com/android/server/am/ActivityRecord.java/#468)
        // So let's remove this whilst keeping all other flags the same, otherwise BrowserApp will vanish from Recents!
        Intent intent = getIntent();
        int flags = intent.getFlags() & ~Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS;
        intent.setFlags(flags);

        SafeIntentUtils.SafeIntent safeIntent = new SafeIntentUtils.SafeIntent(intent);

        // For the moment lets exit early and start fennec as normal if we're not in nightly with
        // the tab queue build flag.
        if (!TabQueueHelper.TAB_QUEUE_ENABLED) {
            loadNormally(safeIntent.getUnsafe());
            return;
        }

        // Skip the Tab Queue if instructed.
        boolean shouldSkipTabQueue = safeIntent.getBooleanExtra(BrowserContract.SKIP_TAB_QUEUE_FLAG, false);
        if (shouldSkipTabQueue) {
            loadNormally(safeIntent.getUnsafe());
            return;
        }

        // The URL is usually hiding somewhere in the extra text. Extract it.
        final String dataString = safeIntent.getDataString();
        if (TextUtils.isEmpty(dataString)) {
            abortDueToNoURL(dataString);
            return;
        }

        boolean shouldShowOpenInBackgroundToast = TabQueueHelper.isTabQueueEnabled(this);

        if (shouldShowOpenInBackgroundToast) {
            showToast(safeIntent.getUnsafe());
        } else {
            loadNormally(safeIntent.getUnsafe());
        }
    }

    private void showToast(Intent intent) {
        intent.setClass(getApplicationContext(), TabQueueService.class);
        startService(intent);
        finish();
    }

    /**
     * Start fennec with the supplied intent.
     */
    private void loadNormally(Intent intent) {
        intent.setClassName(getApplicationContext(), AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
        startActivity(intent);
        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.INTENT, "");
        finish();
    }

    /**
     * Abort as we were started with no URL.
     * @param dataString
     */
    private void abortDueToNoURL(String dataString) {
        // TODO: Lets decide what to do here in bug 1134148
        Log.w(LOGTAG, "Unable to process tab queue insertion. No URL found! - passed data string: " + dataString);
        finish();
    }
}
