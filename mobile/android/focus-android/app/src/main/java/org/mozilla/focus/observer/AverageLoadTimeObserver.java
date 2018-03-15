/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.observer;

import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.util.Log;

import org.mozilla.focus.BuildConfig;
import org.mozilla.focus.architecture.NonNullObserver;
import org.mozilla.focus.session.Session;
import org.mozilla.focus.telemetry.TelemetryWrapper;

public class AverageLoadTimeObserver extends NonNullObserver<Boolean> {

    private static final String LOG_TAG = "AverageLoadTimeObserver";
    private long startLoadTime = 0;
    private boolean loadStarted = false;

    private Session session;

    public AverageLoadTimeObserver(@NonNull Session session) {
        this.session = session;
    }

    @Override
    protected void onValueChanged(Boolean loading) {
        if (loading) {
            if (!loadStarted) {
                startLoadTime = SystemClock.elapsedRealtime();
                Log.i(LOG_TAG, "zerdatime " + startLoadTime +
                        " - page load start");
                loadStarted = true;

                // Add Nimbledroid start bookend for loading website
                Log.i("NimbleDroidV1", "Scenario.begin " +
                        BuildConfig.FLAVOR + "_" +
                        session.getUrl().getValue() + "_load");
            }
        } else {
            if (loadStarted) {
                Log.i(LOG_TAG, "Loaded page at " + session.getUrl().getValue());
                long endTime = SystemClock.elapsedRealtime();
                Log.i(LOG_TAG, "zerdatime " + endTime +
                        " - page load stop");
                Log.i(LOG_TAG, (endTime - startLoadTime) + " - elapsed load");
                TelemetryWrapper.addLoadToAverage(endTime - startLoadTime);
                loadStarted = false;

                // Add Nimbledroid start bookend for loading website
                Log.i("NimbleDroidV1", "Scenario.end " +
                        BuildConfig.FLAVOR + "_" +
                        session.getUrl().getValue() + "_load");
            }
        }
    }
}
