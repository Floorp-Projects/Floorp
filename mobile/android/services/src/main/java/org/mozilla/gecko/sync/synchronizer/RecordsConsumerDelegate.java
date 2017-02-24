/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

import java.util.concurrent.ConcurrentLinkedQueue;

import org.mozilla.gecko.sync.repositories.domain.Record;

interface RecordsConsumerDelegate {
  ConcurrentLinkedQueue<Record> getQueue();

  /**
   * Called when no more items will be processed.
   * Indicates that all items have been processed.
   */
  void consumerIsDoneFull();

  /**
   * Called when no more items will be processed.
   * Indicates that only some of the items have been processed.
   */
  void consumerIsDonePartial();

  void store(Record record);
}