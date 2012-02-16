/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import android.util.Log;

public class Logger {

  // For extra debugging.
  public static boolean LOG_PERSONAL_INFORMATION = false;

  // If true, log to System.out as well as using Android's Log.* calls.
  public static boolean LOG_TO_STDOUT = false;

  public static void logToStdout(String... s) {
    if (LOG_TO_STDOUT) {
      for (String string : s) {
        System.out.print(string);
      }
      System.out.println("");
    }
  }

  public static void error(String logTag, String message) {
    Logger.error(logTag, message, null);
  }

  public static void error(String logTag, String message, Throwable error) {
    logToStdout(logTag, " :: ERROR: ", message);
    if (!Log.isLoggable(logTag, Log.ERROR)) {
      return;
    }
    Log.e(logTag, message, error);
  }

  public static void warn(String logTag, String message) {
    Logger.warn(logTag, message, null);
  }

  public static void warn(String logTag, String message, Throwable error) {
    logToStdout(logTag, " :: WARN: ", message);
    if (!Log.isLoggable(logTag, Log.WARN)) {
      return;
    }
    Log.w(logTag, message, error);
  }

  public static void info(String logTag, String message) {
    logToStdout(logTag, " :: INFO: ", message);
    if (!Log.isLoggable(logTag, Log.INFO)) {
      return;
    }
    Log.i(logTag, message);
  }

  public static void debug(String logTag, String message) {
    Logger.debug(logTag, message, null);
  }

  public static void debug(String logTag, String message, Throwable error) {
    logToStdout(logTag, " :: DEBUG: ", message);
    if (!Log.isLoggable(logTag, Log.DEBUG)) {
      return;
    }
    Log.d(logTag, message, error);
  }

  public static void trace(String logTag, String message) {
    logToStdout(logTag, " :: TRACE: ", message);
    if (!Log.isLoggable(logTag, Log.VERBOSE)) {
      return;
    }
    Log.v(logTag, message);
  }

  public static void pii(String logTag, String message) {
    if (LOG_PERSONAL_INFORMATION) {
      Logger.debug(logTag, "$$PII$$: " + message);
    }
  }
}
