/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.log.writers;

/**
 * An abstract object that logs information in some way.
 * <p>
 * Intended to be composed with other log writers, for example a log
 * writer could make all log entries have the same single log tag, or
 * could ignore certain log levels, before delegating to an inner log
 * writer.
 */
public abstract class LogWriter {
  public abstract void error(String tag, String message, Throwable error);
  public abstract void warn(String tag, String message, Throwable error);
  public abstract void info(String tag, String message, Throwable error);
  public abstract void debug(String tag, String message, Throwable error);
  public abstract void trace(String tag, String message, Throwable error);

  /**
   * We expect <code>close</code> to be called only by static
   * synchronized methods in class <code>Logger</code>.
   */
  public abstract void close();

  public abstract boolean shouldLogVerbose(String tag);
}
