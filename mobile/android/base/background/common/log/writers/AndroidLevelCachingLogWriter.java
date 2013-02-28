/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.common.log.writers;

import java.util.IdentityHashMap;
import java.util.Map;

import android.util.Log;

/**
 * Make a <code>LogWriter</code> only log when the Android log system says to.
 */
public class AndroidLevelCachingLogWriter extends LogWriter {
  protected final LogWriter inner;

  public AndroidLevelCachingLogWriter(LogWriter inner) {
    this.inner = inner;
  }

  // I can't believe we have to implement this ourselves.
  // These aren't synchronized (and neither are the setters) because
  // the logging calls themselves are synchronized.
  private Map<String, Boolean> isErrorLoggable   = new IdentityHashMap<String, Boolean>();
  private Map<String, Boolean> isWarnLoggable    = new IdentityHashMap<String, Boolean>();
  private Map<String, Boolean> isInfoLoggable    = new IdentityHashMap<String, Boolean>();
  private Map<String, Boolean> isDebugLoggable   = new IdentityHashMap<String, Boolean>();
  private Map<String, Boolean> isVerboseLoggable = new IdentityHashMap<String, Boolean>();

  /**
   * Empty the caches of log levels.
   */
  public void refreshLogLevels() {
    isErrorLoggable   = new IdentityHashMap<String, Boolean>();
    isWarnLoggable    = new IdentityHashMap<String, Boolean>();
    isInfoLoggable    = new IdentityHashMap<String, Boolean>();
    isDebugLoggable   = new IdentityHashMap<String, Boolean>();
    isVerboseLoggable = new IdentityHashMap<String, Boolean>();
  }

  private boolean shouldLogError(String logTag) {
    Boolean out = isErrorLoggable.get(logTag);
    if (out != null) {
      return out.booleanValue();
    }
    out = Log.isLoggable(logTag, Log.ERROR);
    isErrorLoggable.put(logTag, out);
    return out;
  }

  private boolean shouldLogWarn(String logTag) {
    Boolean out = isWarnLoggable.get(logTag);
    if (out != null) {
      return out.booleanValue();
    }
    out = Log.isLoggable(logTag, Log.WARN);
    isWarnLoggable.put(logTag, out);
    return out;
  }

  private boolean shouldLogInfo(String logTag) {
    Boolean out = isInfoLoggable.get(logTag);
    if (out != null) {
      return out.booleanValue();
    }
    out = Log.isLoggable(logTag, Log.INFO);
    isInfoLoggable.put(logTag, out);
    return out;
  }

  private boolean shouldLogDebug(String logTag) {
    Boolean out = isDebugLoggable.get(logTag);
    if (out != null) {
      return out.booleanValue();
    }
    out = Log.isLoggable(logTag, Log.DEBUG);
    isDebugLoggable.put(logTag, out);
    return out;
  }

  @Override
  public boolean shouldLogVerbose(String logTag) {
    Boolean out = isVerboseLoggable.get(logTag);
    if (out != null) {
      return out.booleanValue();
    }
    out = Log.isLoggable(logTag, Log.VERBOSE);
    isVerboseLoggable.put(logTag, out);
    return out;
  }

  public void error(String tag, String message, Throwable error) {
    if (shouldLogError(tag)) {
      inner.error(tag, message, error);
    }
  }

  public void warn(String tag, String message, Throwable error) {
    if (shouldLogWarn(tag)) {
      inner.warn(tag, message, error);
    }
  }

  public void info(String tag, String message, Throwable error) {
    if (shouldLogInfo(tag)) {
      inner.info(tag, message, error);
    }
  }

  public void debug(String tag, String message, Throwable error) {
    if (shouldLogDebug(tag)) {
      inner.debug(tag, message, error);
    }
  }

  public void trace(String tag, String message, Throwable error) {
    if (shouldLogVerbose(tag)) {
      inner.trace(tag, message, error);
    }
  }

  public void close() {
    inner.close();
  }
}
