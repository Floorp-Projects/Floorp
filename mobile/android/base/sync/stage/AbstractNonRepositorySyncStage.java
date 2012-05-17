/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import org.mozilla.gecko.sync.GlobalSession;

/**
 * This is simply a stage that is not responsible for synchronizing repositories.
 */
public abstract class AbstractNonRepositorySyncStage implements GlobalSyncStage {
  protected final GlobalSession session;

  public AbstractNonRepositorySyncStage(GlobalSession session) {
    this.session = session;
  }

  @Override
  public void resetLocal() {
    // Do nothing.
  }

  @Override
  public void wipeLocal() {
    // Do nothing.
  }

  public Integer getStorageVersion() {
    return null; // Never include these engines in any meta/global records.
  }
}
