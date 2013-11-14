/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.MetaGlobal;
import org.mozilla.gecko.sync.PersistedMetaGlobal;
import org.mozilla.gecko.sync.delegates.MetaGlobalDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

public class FetchMetaGlobalStage extends AbstractNonRepositorySyncStage {
  private static final String LOG_TAG = "FetchMetaGlobalStage";
  private static final String META_COLLECTION = "meta";

  public class StageMetaGlobalDelegate implements MetaGlobalDelegate {

    private GlobalSession session;
    public StageMetaGlobalDelegate(GlobalSession session) {
      this.session = session;
    }

    @Override
    public void handleSuccess(MetaGlobal global, SyncStorageResponse response) {
      Logger.trace(LOG_TAG, "Persisting fetched meta/global and last modified.");
      PersistedMetaGlobal pmg = session.config.persistedMetaGlobal();
      pmg.persistMetaGlobal(global);
      // Take the timestamp from the response since it is later than the timestamp from info/collections.
      pmg.persistLastModified(response.normalizedWeaveTimestamp());

      session.processMetaGlobal(global);
    }

    @Override
    public void handleFailure(SyncStorageResponse response) {
      session.handleHTTPError(response, "Failure fetching meta/global.");
    }

    @Override
    public void handleError(Exception e) {
      session.abort(e, "Failure fetching meta/global.");
    }

    @Override
    public void handleMissing(MetaGlobal global, SyncStorageResponse response) {
      session.processMissingMetaGlobal(global);
    }
  }

  @Override
  public void execute() throws NoSuchStageException {
    InfoCollections infoCollections = session.config.infoCollections;
    if (infoCollections == null) {
      session.abort(null, "No info/collections set in FetchMetaGlobalStage.");
      return;
    }

    long lastModified = session.config.persistedMetaGlobal().lastModified();
    if (!infoCollections.updateNeeded(META_COLLECTION, lastModified)) {
      // Try to use our local collection keys for this session.
      Logger.info(LOG_TAG, "Trying to use persisted meta/global for this session.");
      MetaGlobal global = session.config.persistedMetaGlobal().metaGlobal(session.config.metaURL(), session.credentials());
      if (global != null) {
        Logger.info(LOG_TAG, "Using persisted meta/global for this session.");
        session.processMetaGlobal(global); // Calls session.advance().
        return;
      }
      Logger.info(LOG_TAG, "Failed to use persisted meta/global for this session.");
    }

    // We need an update: fetch or upload meta/global as necessary.
    Logger.info(LOG_TAG, "Fetching fresh meta/global for this session.");
    MetaGlobal global = new MetaGlobal(session.config.metaURL(), session.credentials());
    global.fetch(new StageMetaGlobalDelegate(session));
  }
}
