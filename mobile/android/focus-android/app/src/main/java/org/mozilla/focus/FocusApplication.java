/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus;

import android.app.Application;
import android.preference.PreferenceManager;

import org.mozilla.focus.search.SearchEngineManager;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.AdjustHelper;

public class FocusApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();

        // Loading preferences depends on the Search Engine manager, because the default search
        // engine pref uses the search engine manager - we therefore need to ensure
        // that prefs aren't loaded until search engines are loaded.
        SearchEngineManager.getInstance().init(this);

        PreferenceManager.setDefaultValues(this, R.xml.settings, false);

        TelemetryWrapper.init(this);
        AdjustHelper.setupAdjustIfNeeded(this);
    }
}
