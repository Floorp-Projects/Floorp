/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import java.net.URISyntaxException;

import org.mozilla.gecko.sync.CredentialsSource;

/**
 * A kind of Server11Repository that supports explicit setting of limit and sort on operations.
 *
 * @author rnewman
 *
 */
public class ConstrainedServer11Repository extends Server11Repository {

  private String sort = null;
  private long limit  = -1;

  public ConstrainedServer11Repository(String serverURI, String username, String collection, CredentialsSource credentialsSource, long limit, String sort) throws URISyntaxException {
    super(serverURI, username, collection, credentialsSource);

    this.limit = limit;
    this.sort  = sort;
  }

  @Override
  protected String getDefaultSort() {
    return sort;
  }

  @Override
  protected long getDefaultFetchLimit() {
    return limit;
  }
}
