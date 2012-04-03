/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import org.mozilla.gecko.sync.GlobalSession;

public interface GlobalSyncStage {
  public static enum Stage {
    idle,                       // Start state.
    checkPreconditions,         // Preparation of the basics. TODO: clear status
    ensureClusterURL,           // Setting up where we talk to.
    fetchInfoCollections,       // Take a look at timestamps.
    fetchMetaGlobal,
    ensureKeysStage,
    /*
    ensureSpecialRecords,
    updateEngineTimestamps,
    */
    syncClientsEngine,
    /*
    processFirstSyncPref,
    processClientCommands,
    updateEnabledEngines,
    */
    syncTabs,
    syncBookmarks,
    syncHistory,
    syncFormHistory,
    completed,
  }
  public void execute(GlobalSession session) throws NoSuchStageException;
}
