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
   *
   * @param fetchEnd
   *        A millisecond-resolution timestamp indicating the *remote* timestamp
   *        at the end of the range of records. Usually this is the timestamp at
   *        which the request was received.
   *        E.g., the (normalized) value of the X-Weave-Timestamp header.
   */
  void onFetchCompleted();

  /**
   * Called when a number of records have been returned but more are still expected to come,
   * possibly after a certain pause.
   */
  void onBatchCompleted();

  RepositorySessionFetchRecordsDelegate deferredFetchDelegate(ExecutorService executor);
}
