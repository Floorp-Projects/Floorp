/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.common.log.writers;

import java.io.PrintWriter;
import java.io.StringWriter;

public class StringLogWriter extends LogWriter {
  protected final StringWriter sw;
  protected final PrintLogWriter inner;

  public StringLogWriter() {
    sw = new StringWriter();
    inner = new PrintLogWriter(new PrintWriter(sw));
  }

  public String toString() {
    return sw.toString();
  }

  @Override
  public boolean shouldLogVerbose(String tag) {
    return true;
  }

  @Override
  public void error(String tag, String message, Throwable error) {
    inner.error(tag, message, error);
  }

  @Override
  public void warn(String tag, String message, Throwable error) {
    inner.warn(tag, message, error);
  }

  @Override
  public void info(String tag, String message, Throwable error) {
    inner.info(tag, message, error);
  }

  @Override
  public void debug(String tag, String message, Throwable error) {
    inner.debug(tag, message, error);
  }

  @Override
  public void trace(String tag, String message, Throwable error) {
    inner.trace(tag, message, error);
  }

  @Override
  public void close() {
    inner.close();
  }
}
