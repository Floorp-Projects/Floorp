/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

import java.util.concurrent.ConcurrentLinkedQueue;

import org.mozilla.gecko.sync.repositories.domain.Record;

interface RecordsConsumerDelegate {
  public abstract ConcurrentLinkedQueue<Record> getQueue();

  /**
   * Called when no more items will be processed.
   * If forced is true, the consumer is terminating because it was told to halt;
   * not all items will necessarily have been processed.
   * If forced is false, the consumer has invoked store and received an onStoreCompleted callback.
   * @param forced
   */
  public abstract void consumerIsDone(boolean forced);
  public abstract void store(Record record);
}