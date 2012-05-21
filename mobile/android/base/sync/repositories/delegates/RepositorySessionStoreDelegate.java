/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.delegates;

import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.sync.repositories.domain.Record;

/**
 * These methods *must* be invoked asynchronously. Use deferredStoreDelegate if you
 * need help doing this.
 *
 * @author rnewman
 *
 */
public interface RepositorySessionStoreDelegate {
  public void onRecordStoreFailed(Exception ex);

  // Optionally called with an equivalent (but not necessarily identical) record
  // when a store has succeeded.
  public void onRecordStoreSucceeded(Record record);
  public void onStoreCompleted(long storeEnd);
  public RepositorySessionStoreDelegate deferredStoreDelegate(ExecutorService executor);
}
