//
//  Logger.java
//  Adjust
//
//  Created by Christian Wellenbrock on 2013-04-18.
//  Copyright (c) 2013 adjust GmbH. All rights reserved.
//  See the file MIT-LICENSE for copying permission.
//

package com.adjust.sdk;

import android.util.Log;

import java.util.Arrays;
import java.util.Locale;

import static com.adjust.sdk.Constants.LOGTAG;

public class Logger implements ILogger {

    private LogLevel logLevel;
    private static String formatErrorMessage = "Error formating log message: %s, with params: %s";

    public Logger() {
        setLogLevel(LogLevel.INFO);
    }

    @Override
    public void setLogLevel(LogLevel logLevel) {
        this.logLevel = logLevel;
    }

    @Override
    public void setLogLevelString(String logLevelString) {
        if (null != logLevelString) {
            try {
                setLogLevel(LogLevel.valueOf(logLevelString.toUpperCase(Locale.US)));
            } catch (IllegalArgumentException iae) {
                error("Malformed logLevel '%s', falling back to 'info'", logLevelString);
            }
        }
    }

    @Override
    public void verbose(String message, Object... parameters) {
        if (logLevel.androidLogLevel <= Log.VERBOSE) {
            try {
                Log.v(LOGTAG, String.format(message, parameters));
            } catch (Exception e) {
                Log.e(LOGTAG, String.format(formatErrorMessage, message, Arrays.toString(parameters)));
            }
        }
    }

    @Override
    public void debug(String message, Object... parameters) {
        if (logLevel.androidLogLevel <= Log.DEBUG) {
            try {
                Log.d(LOGTAG, String.format(message, parameters));
            } catch (Exception e) {
                Log.e(LOGTAG, String.format(formatErrorMessage, message, Arrays.toString(parameters)));
            }
        }
    }

    @Override
    public void info(String message, Object... parameters) {
        if (logLevel.androidLogLevel <= Log.INFO) {
            try {
                Log.i(LOGTAG, String.format(message, parameters));
            } catch (Exception e) {
                Log.e(LOGTAG, String.format(formatErrorMessage, message, Arrays.toString(parameters)));
            }
        }
    }

    @Override
    public void warn(String message, Object... parameters) {
        if (logLevel.androidLogLevel <= Log.WARN) {
            try {
                Log.w(LOGTAG, String.format(message, parameters));
            } catch (Exception e) {
                Log.e(LOGTAG, String.format(formatErrorMessage, message, Arrays.toString(parameters)));
            }
        }
    }

    @Override
    public void error(String message, Object... parameters) {
        if (logLevel.androidLogLevel <= Log.ERROR) {
            try {
                Log.e(LOGTAG, String.format(message, parameters));
            } catch (Exception e) {
                Log.e(LOGTAG, String.format(formatErrorMessage, message, Arrays.toString(parameters)));
            }
        }
    }

    @Override
    public void Assert(String message, Object... parameters) {
        try {
            Log.println(Log.ASSERT, LOGTAG, String.format(message, parameters));
        } catch (Exception e) {
            Log.e(LOGTAG, String.format(formatErrorMessage, message, Arrays.toString(parameters)));
        }
    }
}
