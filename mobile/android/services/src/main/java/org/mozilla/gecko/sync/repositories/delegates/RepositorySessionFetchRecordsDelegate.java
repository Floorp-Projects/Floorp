/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.delegates;

import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.sync.repositories.domain.Record;

public interface RepositorySessionFetchRecordsDelegate {
  void onFetchFailed(Exception ex);
  void onFetchedRecord(Record record);

  /**
   * Called when all records in this fetch have been returned.
   */
  void onFetchCompleted();

  RepositorySessionFetchRecordsDelegate deferredFetchDelegate(ExecutorService executor);
}
