/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.log.writers;

import android.util.Log;

/**
 * A LogWriter that logs only if the message is as important as the specified
 * level. For example, if the specified level is <code>Log.WARN</code>, only
 * <code>warn</code> and <code>error</code> will log.
 */
public class LevelFilteringLogWriter extends LogWriter {
  protected final LogWriter inner;
  protected final int logLevel;

  public LevelFilteringLogWriter(int logLevel, LogWriter inner) {
    this.inner = inner;
    this.logLevel = logLevel;
  }

  public void close() {
    inner.close();
  }

  @Override
  public void error(String tag, String message, Throwable error) {
    if (logLevel <= Log.ERROR) {
      inner.error(tag, message, error);
    }
  }

  @Override
  public void warn(String tag, String message, Throwable error) {
    if (logLevel <= Log.WARN) {
      inner.warn(tag, message, error);
    }
  }

  @Override
  public void info(String tag, String message, Throwable error) {
    if (logLevel <= Log.INFO) {
      inner.info(tag, message, error);
    }
  }

  @Override
  public void debug(String tag, String message, Throwable error) {
    if (logLevel <= Log.DEBUG) {
      inner.debug(tag, message, error);
    }
  }

  @Override
  public void trace(String tag, String message, Throwable error) {
    if (logLevel <= Log.VERBOSE) {
      inner.trace(tag, message, error);
    }
  }

  @Override
  public boolean shouldLogVerbose(String tag) {
    return logLevel <= Log.VERBOSE;
  }
}
