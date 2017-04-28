/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus;

import android.app.Application;
import android.os.StrictMode;
import android.preference.PreferenceManager;

import org.mozilla.focus.search.SearchEngineManager;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.AdjustHelper;
import org.mozilla.focus.utils.AppConstants;

public class FocusApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();

        enableStrictMode();

        PreferenceManager.setDefaultValues(this, R.xml.settings, false);

        SearchEngineManager.getInstance().init(this);

        TelemetryWrapper.init(this);
        AdjustHelper.setupAdjustIfNeeded(this);
    }

    private void enableStrictMode() {
        if (AppConstants.isReleaseBuild()) {
            return;
        }

        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                .detectAll()
                .penaltyDeath()
                .build());

        StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder()
                .detectAll()
                .penaltyDeath()
                .build());
    }
}
