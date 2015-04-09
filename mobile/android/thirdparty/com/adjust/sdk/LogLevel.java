package com.adjust.sdk;

import android.util.Log;

/**
 * Created by pfms on 11/03/15.
 */
public enum LogLevel {
    VERBOSE(Log.VERBOSE), DEBUG(Log.DEBUG), INFO(Log.INFO), WARN(Log.WARN), ERROR(Log.ERROR), ASSERT(Log.ASSERT);
    final int androidLogLevel;

    LogLevel(final int androidLogLevel) {
        this.androidLogLevel = androidLogLevel;
    }

    public int getAndroidLogLevel() {
        return androidLogLevel;
    }
}
