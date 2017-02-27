/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import java.net.URISyntaxException;

import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.stage.ServerSyncStage;

/**
 * A kind of Server15Repository that supports explicit setting of:
 * - per-batch fetch limit
 * - batching mode (single batch vs multi-batch)
 * - sort order
 * - repository state provider (persistent vs non-persistent)
 * - whereas use of high-water-mark is allowed
 *
 * @author rnewman
 *
 */
public class ConfigurableServer15Repository extends Server15Repository {
  private final String sortOrder;
  private final long batchLimit;
  private final ServerSyncStage.MultipleBatches multipleBatches;
  private final ServerSyncStage.HighWaterMark highWaterMark;

  public ConfigurableServer15Repository(
          String collection,
          long syncDeadline,
          String storageURL,
          AuthHeaderProvider authHeaderProvider,
          InfoCollections infoCollections,
          InfoConfiguration infoConfiguration,
          long batchLimit,
          String sort,
          ServerSyncStage.MultipleBatches multipleBatches,
          ServerSyncStage.HighWaterMark highWaterMark,
          RepositoryStateProvider stateProvider) throws URISyntaxException {
    super(
            collection,
            syncDeadline,
            storageURL,
            authHeaderProvider,
            infoCollections,
            infoConfiguration,
            stateProvider
    );
    this.batchLimit = batchLimit;
    this.sortOrder  = sort;
    this.multipleBatches = multipleBatches;
    this.highWaterMark = highWaterMark;

    // Sanity check: let's ensure we're configured correctly. At this point in time, it doesn't make
    // sense to use H.W.M. with a non-persistent state provider. This might change if we start retrying
    // during a download in case of 412s.
    if (!stateProvider.isPersistent() && highWaterMark.equals(ServerSyncStage.HighWaterMark.Enabled)) {
      throw new IllegalArgumentException("Can not use H.W.M. with NonPersistentRepositoryStateProvider");
    }
  }

  @Override
  public String getSortOrder() {
    return sortOrder;
  }

  @Override
  public Long getBatchLimit() {
    return batchLimit;
  }

  @Override
  public boolean getAllowMultipleBatches() {
    return multipleBatches.equals(ServerSyncStage.MultipleBatches.Enabled);
  }

  @Override
  public boolean getAllowHighWaterMark() {
    return highWaterMark.equals(ServerSyncStage.HighWaterMark.Enabled);
  }
}
