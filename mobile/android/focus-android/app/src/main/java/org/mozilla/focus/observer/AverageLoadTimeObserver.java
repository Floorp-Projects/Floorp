package org.mozilla.focus.observer;

import android.os.SystemClock;
import android.util.Log;

import org.mozilla.focus.architecture.NonNullObserver;
import org.mozilla.focus.session.SessionManager;
import org.mozilla.focus.telemetry.TelemetryWrapper;

public class AverageLoadTimeObserver extends NonNullObserver<Boolean> {

    private static final String LOG_TAG = "AverageLoadTimeObserver";
    private long startLoadTime = 0;
    private boolean loadStarted = false;

    @Override
    protected void onValueChanged(Boolean loading) {
        if (loading) {
            if (!loadStarted) {
                startLoadTime = SystemClock.elapsedRealtime();
                Log.i(LOG_TAG, "zerdatime " + startLoadTime +
                        " - page load start");
                loadStarted = true;
            }
        } else {
            if (loadStarted) {
                Log.i(LOG_TAG, "Loaded page at " +
                        SessionManager.getInstance().getCurrentSession().getUrl().getValue());
                long endTime = SystemClock.elapsedRealtime();
                Log.i(LOG_TAG, "zerdatime " + endTime +
                        " - page load stop");
                Log.i(LOG_TAG, (endTime - startLoadTime) + " - elapsed load");
                TelemetryWrapper.addLoadToAverage(endTime - startLoadTime);
                loadStarted = false;
            }
        }
    }
}
