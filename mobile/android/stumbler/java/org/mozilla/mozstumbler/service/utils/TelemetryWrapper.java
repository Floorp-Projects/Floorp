package org.mozilla.mozstumbler.service.utils;

import android.util.Log;
import org.mozilla.mozstumbler.service.AppGlobals;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class TelemetryWrapper {
    private static final String LOG_TAG = AppGlobals.makeLogTag(TelemetryWrapper.class.getSimpleName());
    private static Method mAddToHistogram;

    public static void addToHistogram(String key, int value) {
        if (mAddToHistogram == null) {
            try {
                Class<?> telemetry = Class.forName("org.mozilla.gecko.Telemetry");
                mAddToHistogram = telemetry.getMethod("addToHistogram", String.class, int.class);
            } catch (ClassNotFoundException e) {
                Log.d(LOG_TAG, "Class not found!");
                return;
            } catch (NoSuchMethodException e) {
                Log.d(LOG_TAG, "Method not found!");
                return;
            }
        }

        if (mAddToHistogram != null) {
            try {
                mAddToHistogram.invoke(null, key, value);
            }
            catch (IllegalArgumentException | InvocationTargetException | IllegalAccessException e) {
                Log.d(LOG_TAG, "Got exception invoking.");
            }
        }
    }
}
