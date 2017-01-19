/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import java.net.URISyntaxException;

import org.mozilla.gecko.sync.MetaGlobalException;
import org.mozilla.gecko.sync.repositories.ConfigurableServer15Repository;
import org.mozilla.gecko.sync.repositories.PersistentRepositoryStateProvider;
import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositoryStateProvider;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserHistoryRepository;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecordFactory;
import org.mozilla.gecko.sync.repositories.domain.VersionConstants;

public class AndroidBrowserHistoryServerSyncStage extends ServerSyncStage {
  protected static final String LOG_TAG = "HistoryStage";

  // Eventually this kind of sync stage will be data-driven,
  // and all this hard-coding can go away.
  private static final String HISTORY_SORT = "oldest";
  private static final long HISTORY_BATCH_LIMIT = 500;

  @Override
  protected String getCollection() {
    return "history";
  }

  @Override
  protected String getEngineName() {
    return "history";
  }

  @Override
  public Integer getStorageVersion() {
    return VersionConstants.HISTORY_ENGINE_VERSION;
  }

  @Override
  protected Repository getLocalRepository() {
    return new AndroidBrowserHistoryRepository();
  }

  /**
   * We use a persistent state provider for this stage, because it lets us resume interrupted
   * syncs more efficiently.
   * We are able to do this because we match criteria described in {@link RepositoryStateProvider}.
   *
   * @return Persistent repository state provider.
     */
  @Override
  protected RepositoryStateProvider getRepositoryStateProvider() {
    return new PersistentRepositoryStateProvider(
            session.config.getBranch(statePreferencesPrefix())
    );
  }

  /**
   * We're downloading records oldest-first directly into live storage, forgoing any buffering other
   * than AndroidBrowserHistoryRepository's internal records queue. These conditions allow us to use
   * high-water-mark to resume downloads in case of interruptions.
   *
   * @return HighWaterMark.Enabled
   */
  @Override
  protected HighWaterMark getAllowedToUseHighWaterMark() {
    return HighWaterMark.Enabled;
  }

  /**
   * Full batching is allowed, because we want all of the records.
   *
   * @return MultipleBatches.Enabled
   */
  @Override
  protected MultipleBatches getAllowedMultipleBatches() {
    return MultipleBatches.Enabled;
  }

  @Override
  protected Repository getRemoteRepository() throws URISyntaxException {
    return new ConfigurableServer15Repository(
            getCollection(),
            session.getSyncDeadline(),
            session.config.storageURL(),
            session.getAuthHeaderProvider(),
            session.config.infoCollections,
            session.config.infoConfiguration,
            HISTORY_BATCH_LIMIT,
            HISTORY_SORT,
            getAllowedMultipleBatches(),
            getAllowedToUseHighWaterMark(),
            getRepositoryStateProvider()
    );
  }

  @Override
  protected RecordFactory getRecordFactory() {
    return new HistoryRecordFactory();
  }

  @Override
  protected boolean isEnabled() throws MetaGlobalException {
    if (session == null || session.getContext() == null) {
      return false;
    }
    return super.isEnabled();
  }
}
