/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import android.content.SyncResult;

public abstract class SyncException extends Exception {
  private static final long serialVersionUID = -6928990004393234738L;

  public SyncException() {
    super();
  }

  public SyncException(final Throwable e) {
    super(e);
  }

  /**
   * Update sync result statistics with information particular to this
   * exception.
   *
   * @param globalSession
   *          current session, or null.
   * @param syncResult
   *          Android sync result to update.
   */
  public void updateStats(GlobalSession globalSession, SyncResult syncResult) {
    // Assume storage error.
    // TODO: this logic is overly simplistic.
    syncResult.databaseError = true;
  }
}
