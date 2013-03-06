/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.common.log;

import java.io.PrintWriter;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.Set;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.writers.AndroidLevelCachingLogWriter;
import org.mozilla.gecko.background.common.log.writers.AndroidLogWriter;
import org.mozilla.gecko.background.common.log.writers.LogWriter;
import org.mozilla.gecko.background.common.log.writers.PrintLogWriter;
import org.mozilla.gecko.background.common.log.writers.SimpleTagLogWriter;
import org.mozilla.gecko.background.common.log.writers.ThreadLocalTagLogWriter;

import android.util.Log;

/**
 * Logging helper class. Serializes all log operations (by synchronizing).
 */
public class Logger {
  public static final String LOGGER_TAG = "Logger";
  public static final String DEFAULT_LOG_TAG = "GeckoLogger";

  // For extra debugging.
  public static boolean LOG_PERSONAL_INFORMATION = false;

  /**
   * Allow each thread to use its own global log tag. This allows
   * independent services to log as different sources.
   *
   * When your thread sets up logging, it should do something like the following:
   *
   *   Logger.setThreadLogTag("MyTag");
   *
   * The value is inheritable, so worker threads and such do not need to
   * set the same log tag as their parent.
   */
  private static final InheritableThreadLocal<String> logTag = new InheritableThreadLocal<String>() {
    @Override
    protected String initialValue() {
      return DEFAULT_LOG_TAG;
    }
  };

  public static void setThreadLogTag(final String logTag) {
    Logger.logTag.set(logTag);
  }
  public static String getThreadLogTag() {
    return Logger.logTag.get();
  }

  /**
   * Current set of writers to which we will log.
   * <p>
   * We want logging to be available while running tests, so we initialize
   * this set statically.
   */
  protected final static Set<LogWriter> logWriters;
  static {
    final Set<LogWriter> defaultWriters = Logger.defaultLogWriters();
    logWriters = new LinkedHashSet<LogWriter>(defaultWriters);
  }

  /**
   * Default set of log writers to log to.
   */
  public final static Set<LogWriter> defaultLogWriters() {
    final String processedPackage = GlobalConstants.BROWSER_INTENT_PACKAGE.replace("org.mozilla.", "");

    final Set<LogWriter> defaultLogWriters = new LinkedHashSet<LogWriter>();

    final LogWriter log = new AndroidLogWriter();
    final LogWriter cache = new AndroidLevelCachingLogWriter(log);

    final LogWriter single = new SimpleTagLogWriter(processedPackage, new ThreadLocalTagLogWriter(Logger.logTag, cache));

    defaultLogWriters.add(single);
    return defaultLogWriters;
  }

  public static synchronized void startLoggingTo(LogWriter logWriter) {
    logWriters.add(logWriter);
  }

  public static synchronized void startLoggingToWriters(Set<LogWriter> writers) {
    logWriters.addAll(writers);
  }

  public static synchronized void stopLoggingTo(LogWriter logWriter) {
    try {
      logWriter.close();
    } catch (Exception e) {
      Log.e(LOGGER_TAG, "Got exception closing and removing LogWriter " + logWriter + ".", e);
    }
    logWriters.remove(logWriter);
  }

  public static synchronized void stopLoggingToAll() {
    for (LogWriter logWriter : logWriters) {
      try {
        logWriter.close();
      } catch (Exception e) {
        Log.e(LOGGER_TAG, "Got exception closing and removing LogWriter " + logWriter + ".", e);
      }
    }
    logWriters.clear();
  }

  /**
   * Write to only the default log writers.
   */
  public static synchronized void resetLogging() {
    stopLoggingToAll();
    logWriters.addAll(Logger.defaultLogWriters());
  }

  /**
   * Start writing log output to stdout.
   * <p>
   * Use <code>resetLogging</code> to stop logging to stdout.
   */
  public static synchronized void startLoggingToConsole() {
    setThreadLogTag("Test");
    startLoggingTo(new PrintLogWriter(new PrintWriter(System.out, true)));
  }

  // Synchronized version for other classes to use.
  public static synchronized boolean shouldLogVerbose(String logTag) {
    for (LogWriter logWriter : logWriters) {
      if (logWriter.shouldLogVerbose(logTag)) {
        return true;
      }
    }
    return false;
  }

  public static void error(String tag, String message) {
    Logger.error(tag, message, null);
  }

  public static void warn(String tag, String message) {
    Logger.warn(tag, message, null);
  }

  public static void info(String tag, String message) {
    Logger.info(tag, message, null);
  }

  public static void debug(String tag, String message) {
    Logger.debug(tag, message, null);
  }

  public static void trace(String tag, String message) {
    Logger.trace(tag, message, null);
  }

  public static void pii(String tag, String message) {
    if (LOG_PERSONAL_INFORMATION) {
      Logger.debug(tag, "$$PII$$: " + message);
    }
  }

  public static synchronized void error(String tag, String message, Throwable error) {
    Iterator<LogWriter> it = logWriters.iterator();
    while (it.hasNext()) {
      LogWriter writer = it.next();
      try {
        writer.error(tag, message, error);
      } catch (Exception e) {
        Log.e(LOGGER_TAG, "Got exception logging; removing LogWriter " + writer + ".", e);
        it.remove();
      }
    }
  }

  public static synchronized void warn(String tag, String message, Throwable error) {
    Iterator<LogWriter> it = logWriters.iterator();
    while (it.hasNext()) {
      LogWriter writer = it.next();
      try {
        writer.warn(tag, message, error);
      } catch (Exception e) {
        Log.e(LOGGER_TAG, "Got exception logging; removing LogWriter " + writer + ".", e);
        it.remove();
      }
    }
  }

  public static synchronized void info(String tag, String message, Throwable error) {
    Iterator<LogWriter> it = logWriters.iterator();
    while (it.hasNext()) {
      LogWriter writer = it.next();
      try {
        writer.info(tag, message, error);
      } catch (Exception e) {
        Log.e(LOGGER_TAG, "Got exception logging; removing LogWriter " + writer + ".", e);
        it.remove();
      }
    }
  }

  public static synchronized void debug(String tag, String message, Throwable error) {
    Iterator<LogWriter> it = logWriters.iterator();
    while (it.hasNext()) {
      LogWriter writer = it.next();
      try {
        writer.debug(tag, message, error);
      } catch (Exception e) {
        Log.e(LOGGER_TAG, "Got exception logging; removing LogWriter " + writer + ".", e);
        it.remove();
      }
    }
  }

  public static synchronized void trace(String tag, String message, Throwable error) {
    Iterator<LogWriter> it = logWriters.iterator();
    while (it.hasNext()) {
      LogWriter writer = it.next();
      try {
        writer.trace(tag, message, error);
      } catch (Exception e) {
        Log.e(LOGGER_TAG, "Got exception logging; removing LogWriter " + writer + ".", e);
        it.remove();
      }
    }
  }
}
