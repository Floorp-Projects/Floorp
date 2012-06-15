/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.log.writers;

/**
 * Make a <code>LogWriter</code> only log with a single tag.
 */
public class SingleTagLogWriter extends LogWriter {
  protected final String tag;
  protected final LogWriter inner;

  public SingleTagLogWriter(String tag, LogWriter inner) {
    this.tag = tag;
    this.inner = inner;
  }

  @Override
  public void error(String tag, String message, Throwable error) {
    inner.error(this.tag, tag + " :: " + message, error);
  }

  @Override
  public void warn(String tag, String message, Throwable error) {
    inner.warn(this.tag, tag + " :: " + message, error);
  }

  @Override
  public void info(String tag, String message, Throwable error) {
    inner.info(this.tag, tag + " :: " + message, error);
  }

  @Override
  public void debug(String tag, String message, Throwable error) {
    inner.debug(this.tag, tag + " :: " + message, error);
  }

  @Override
  public void trace(String tag, String message, Throwable error) {
    inner.trace(this.tag, tag + " :: " + message, error);
  }

  @Override
  public boolean shouldLogVerbose(String tag) {
    return inner.shouldLogVerbose(this.tag);
  }

  @Override
  public void close() {
    inner.close();
  }
}
