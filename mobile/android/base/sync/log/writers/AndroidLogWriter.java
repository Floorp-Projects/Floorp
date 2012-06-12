/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.log.writers;

import android.util.Log;

/**
 * Log to the Android log.
 */
public class AndroidLogWriter extends LogWriter {
  @Override
  public boolean shouldLogVerbose(String logTag) {
    return true;
  }

  public void error(String tag, String message, Throwable error) {
    Log.e(tag, message, error);
  }

  public void warn(String tag, String message, Throwable error) {
    Log.w(tag, message, error);
  }

  public void info(String tag, String message, Throwable error) {
    Log.i(tag, message, error);
  }

  public void debug(String tag, String message, Throwable error) {
    Log.d(tag, message, error);
  }

  public void trace(String tag, String message, Throwable error) {
    Log.v(tag, message, error);
  }

  public void close() {
  }
}
