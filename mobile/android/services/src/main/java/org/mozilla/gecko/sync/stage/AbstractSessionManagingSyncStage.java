/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.telemetry.TelemetryStageCollector;

/**
 * A global sync stage that manages a <code>GlobalSession</code> instance. This
 * class is intended to be temporary: it should disappear as work to make
 * data-driven syncs progresses.
 * <p>
 * This class is inherently <b>thread-unsafe</b>: if <code>session</code> is
 * mutated after being set, all sorts of bad things could occur. At the time of
 * writing, every <code>GlobalSyncStage</code> created is executed (wiped,
 * reset) with the same <code>GlobalSession</code> argument.
 */
public abstract class AbstractSessionManagingSyncStage implements GlobalSyncStage {
  protected GlobalSession session;
  protected TelemetryStageCollector telemetryStageCollector;

  protected abstract void execute() throws NoSuchStageException;
  protected abstract void resetLocal();
  protected abstract void wipeLocal() throws Exception;

  @Override
  public void resetLocal(GlobalSession session) {
    this.session = session;
    resetLocal();
  }

  @Override
  public void wipeLocal(GlobalSession session) throws Exception {
    this.session = session;
    wipeLocal();
  }

  @Override
  public void execute(GlobalSession session, TelemetryStageCollector telemetryStageCollector) throws NoSuchStageException {
    this.telemetryStageCollector = telemetryStageCollector;
    this.session = session;

    execute();
  }
}
