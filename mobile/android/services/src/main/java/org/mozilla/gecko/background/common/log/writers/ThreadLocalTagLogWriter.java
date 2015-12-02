/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.common.log.writers;

/**
 * Log with a single global tag… but that tag can be different for each thread.
 *
 * Takes a @link{ThreadLocal} as a constructor parameter.
 */
public class ThreadLocalTagLogWriter extends TagLogWriter {

  private final ThreadLocal<String> tag;

  public ThreadLocalTagLogWriter(ThreadLocal<String> tag, LogWriter inner) {
    super(inner);
    this.tag = tag;
  }

  @Override
  protected String getMainTag() {
    return this.tag.get();
  }
}
