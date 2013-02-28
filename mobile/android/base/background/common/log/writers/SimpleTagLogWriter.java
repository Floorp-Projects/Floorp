/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.common.log.writers;

/**
 * Make a <code>LogWriter</code> only log with a single string tag.
 */
public class SimpleTagLogWriter extends TagLogWriter {
  final String tag;
  public SimpleTagLogWriter(String tag, LogWriter inner) {
    super(inner);
    this.tag = tag;
  }

  protected String getMainTag() {
    return tag;
  }
}
