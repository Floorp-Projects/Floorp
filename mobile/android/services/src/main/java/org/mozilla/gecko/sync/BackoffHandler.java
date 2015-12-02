/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;


public interface BackoffHandler {
  public long getEarliestNextRequest();

  /**
   * Provide a timestamp in millis before which we shouldn't sync again.
   * Overrides any existing value.
   *
   * @param next
   *          a timestamp in milliseconds.
   */
  public void setEarliestNextRequest(long next);

  /**
   * Provide a timestamp in millis before which we shouldn't sync again. Only
   * change our persisted value if it's later than the existing time.
   *
   * @param next
   *          a timestamp in milliseconds.
   */
  public void extendEarliestNextRequest(long next);

  /**
   * Return the number of milliseconds until we're allowed to sync again,
   * or 0 if now is fine.
   */
  public long delayMilliseconds();
}