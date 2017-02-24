/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.delegates;

import java.util.concurrent.ExecutorService;

/**
 * These methods *must* be invoked asynchronously. Use deferredStoreDelegate if you
 * need help doing this.
 *
 * @author rnewman
 *
 */
public interface RepositorySessionStoreDelegate {
  void onRecordStoreFailed(Exception ex, String recordGuid);

  // Called with a GUID when store has succeeded.
  void onRecordStoreSucceeded(String guid);
  void onStoreCompleted(long storeEnd);
  void onStoreFailed(Exception e);
  RepositorySessionStoreDelegate deferredStoreDelegate(ExecutorService executor);
}
