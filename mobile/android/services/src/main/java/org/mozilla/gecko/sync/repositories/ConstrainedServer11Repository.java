/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import java.net.URISyntaxException;

import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;

/**
 * A kind of Server11Repository that supports explicit setting of per-batch fetch limit,
 * batching mode (single batch vs multi-batch), and a sort order.
 *
 * @author rnewman
 *
 */
public class ConstrainedServer11Repository extends Server11Repository {
  private final String sortOrder;
  private final long batchLimit;
  private final boolean allowMultipleBatches;

  public ConstrainedServer11Repository(
          String collection,
          String storageURL,
          AuthHeaderProvider authHeaderProvider,
          InfoCollections infoCollections,
          InfoConfiguration infoConfiguration,
          long batchLimit,
          String sort,
          boolean allowMultipleBatches) throws URISyntaxException {
    super(
            collection,
            storageURL,
            authHeaderProvider,
            infoCollections,
            infoConfiguration
    );
    this.batchLimit = batchLimit;
    this.sortOrder  = sort;
    this.allowMultipleBatches = allowMultipleBatches;
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
    return allowMultipleBatches;
  }
}
