/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.common.log.writers;

/**
 * A @link{LogWriter} that logs each message under a parent tag.
 */
public abstract class TagLogWriter extends LogWriter {

  protected final LogWriter inner;

  public TagLogWriter(final LogWriter inner) {
    super();
    this.inner = inner;
  }

  protected abstract String getMainTag();

  @Override
  public void error(String tag, String message, Throwable error) {
    inner.error(this.getMainTag(), tag + " :: " + message, error);
  }

  @Override
  public void warn(String tag, String message, Throwable error) {
    inner.warn(this.getMainTag(), tag + " :: " + message, error);
  }

  @Override
  public void info(String tag, String message, Throwable error) {
    inner.info(this.getMainTag(), tag + " :: " + message, error);
  }

  @Override
  public void debug(String tag, String message, Throwable error) {
    inner.debug(this.getMainTag(), tag + " :: " + message, error);
  }

  @Override
  public void trace(String tag, String message, Throwable error) {
    inner.trace(this.getMainTag(), tag + " :: " + message, error);
  }

  @Override
  public boolean shouldLogVerbose(String tag) {
    return inner.shouldLogVerbose(this.getMainTag());
  }

  @Override
  public void close() {
    inner.close();
  }
}