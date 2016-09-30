/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import java.net.URISyntaxException;

import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;

/**
 * A kind of Server11Repository that supports explicit setting of total fetch limit, per-batch fetch limit, and a sort order.
 *
 * @author rnewman
 *
 */
public class ConstrainedServer11Repository extends Server11Repository {

  private final String sort;
  private final long batchLimit;
  private final long totalLimit;

  public ConstrainedServer11Repository(String collection, String storageURL,
                                       AuthHeaderProvider authHeaderProvider,
                                       InfoCollections infoCollections,
                                       InfoConfiguration infoConfiguration,
                                       long batchLimit, long totalLimit, String sort)
          throws URISyntaxException {
    super(collection, storageURL, authHeaderProvider, infoCollections, infoConfiguration);
    this.batchLimit = batchLimit;
    this.totalLimit = totalLimit;
    this.sort  = sort;
  }

  @Override
  public String getDefaultSort() {
    return sort;
  }

  @Override
  public long getDefaultBatchLimit() {
    return batchLimit;
  }

  @Override
  public long getDefaultTotalLimit() {
    return totalLimit;
  }
}
