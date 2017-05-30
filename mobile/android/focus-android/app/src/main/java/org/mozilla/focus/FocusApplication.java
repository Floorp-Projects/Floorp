/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus;

import android.os.StrictMode;
import android.preference.PreferenceManager;

import org.mozilla.focus.locale.LocaleAwareApplication;
import org.mozilla.focus.search.SearchEngineManager;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.AdjustHelper;
import org.mozilla.focus.utils.AppConstants;

public class FocusApplication extends LocaleAwareApplication {

    @Override
    public void onCreate() {
        super.onCreate();

        PreferenceManager.setDefaultValues(this, R.xml.settings, false);

        enableStrictMode();

        SearchEngineManager.getInstance().init(this);

        TelemetryWrapper.init(this);
        AdjustHelper.setupAdjustIfNeeded(this);
    }

    private void enableStrictMode() {
        // Android/WebView sometimes commit strict mode violations, see e.g.
        // https://github.com/mozilla-mobile/focus-android/issues/660
        if (AppConstants.isReleaseBuild()) {
            return;
        }

        final StrictMode.ThreadPolicy.Builder threadPolicyBuilder = new StrictMode.ThreadPolicy.Builder().detectAll();
        final StrictMode.VmPolicy.Builder vmPolicyBuilder = new StrictMode.VmPolicy.Builder().detectAll();

        if (AppConstants.isBetaBuild()) {
            threadPolicyBuilder.penaltyDialog();
            vmPolicyBuilder.penaltyLog();
        } else { // Dev/debug build
            threadPolicyBuilder.penaltyDialog();
            // We want only penaltyDeath(), but penaltLog() is needed print a stacktrace when a violation happens
            vmPolicyBuilder.penaltyLog().penaltyDeath();
        }

        StrictMode.setThreadPolicy(threadPolicyBuilder.build());
        StrictMode.setVmPolicy(vmPolicyBuilder.build());
    }
}
