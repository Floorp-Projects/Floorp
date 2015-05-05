/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabqueue;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.mozglue.ContextUtils;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.sync.setup.activities.WebURLFinder;

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

        ContextUtils.SafeIntent intent = new ContextUtils.SafeIntent(getIntent());

        // For the moment lets exit early and start fennec as normal if we're not in nightly with
        // the tab queue build flag.
        if (!AppConstants.MOZ_ANDROID_TAB_QUEUE || !AppConstants.NIGHTLY_BUILD) {
            loadNormally(intent.getUnsafe());
            return;
        }

        // The URL is usually hiding somewhere in the extra text. Extract it.
        final String dataString = intent.getDataString();
        if (TextUtils.isEmpty(dataString)) {
            abortDueToNoURL(dataString);
            return;
        }

        boolean shouldShowOpenInBackgroundToast = GeckoSharedPrefs.forApp(this).getBoolean(GeckoPreferences.PREFS_TAB_QUEUE, false);

        if (shouldShowOpenInBackgroundToast) {
            showToast(intent.getUnsafe());
        } else {
            loadNormally(intent.getUnsafe());
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
        intent.setClassName(getApplicationContext(), AppConstants.BROWSER_INTENT_CLASS_NAME);
        startActivity(intent);
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
