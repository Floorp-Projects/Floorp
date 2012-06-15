/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.domain.Record;

/**
 * Consume records from a queue inside a RecordsChannel, storing them serially.
 * @author rnewman
 *
 */
class SerialRecordConsumer extends RecordConsumer {
  private static final String LOG_TAG = "SerialRecordConsumer";
  protected boolean stopEventually = false;
  private volatile long counter = 0;

  public SerialRecordConsumer(RecordsConsumerDelegate delegate) {
    this.delegate = delegate;
  }

  private Object monitor = new Object();
  @Override
  public void doNotify() {
    synchronized (monitor) {
      monitor.notify();
    }
  }

  @Override
  public void queueFilled() {
    Logger.debug(LOG_TAG, "Queue filled.");
    synchronized (monitor) {
      this.stopEventually = true;
      monitor.notify();
    }
  }

  @Override
  public void halt() {
    Logger.debug(LOG_TAG, "Halting.");
    synchronized (monitor) {
      this.stopEventually = true;
      this.stopImmediately = true;
      monitor.notify();
    }
  }

  private Object storeSerializer = new Object();
  @Override
  public void stored() {
    Logger.debug(LOG_TAG, "Record stored. Notifying.");
    synchronized (storeSerializer) {
      Logger.debug(LOG_TAG, "stored() took storeSerializer.");
      counter++;
      storeSerializer.notify();
      Logger.debug(LOG_TAG, "stored() dropped storeSerializer.");
    }
  }
  private void storeSerially(Record record) {
    Logger.debug(LOG_TAG, "New record to store.");
    synchronized (storeSerializer) {
      Logger.debug(LOG_TAG, "storeSerially() took storeSerializer.");
      Logger.debug(LOG_TAG, "Storing...");
      try {
        this.delegate.store(record);
      } catch (Exception e) {
        Logger.warn(LOG_TAG, "Got exception in store. Not waiting.", e);
        return;      // So we don't block for a stored() that never comes.
      }
      try {
        Logger.debug(LOG_TAG, "Waiting...");
        storeSerializer.wait();
      } catch (InterruptedException e) {
        // TODO
      }
      Logger.debug(LOG_TAG, "storeSerially() dropped storeSerializer.");
    }
  }

  private void consumerIsDone() {
    long counterNow = this.counter;
    Logger.info(LOG_TAG, "Consumer is done. Processed " + counterNow + ((counterNow == 1) ? " record." : " records."));
    delegate.consumerIsDone(stopImmediately);
  }

  @Override
  public void run() {
    while (true) {
      synchronized (monitor) {
        Logger.debug(LOG_TAG, "run() took monitor.");
        if (stopImmediately) {
          Logger.debug(LOG_TAG, "Stopping immediately. Clearing queue.");
          delegate.getQueue().clear();
          Logger.debug(LOG_TAG, "Notifying consumer.");
          consumerIsDone();
          return;
        }
        Logger.debug(LOG_TAG, "run() dropped monitor.");
      }
      // The queue is concurrent-safe.
      while (!delegate.getQueue().isEmpty()) {
        Logger.debug(LOG_TAG, "Grabbing record...");
        Record record = delegate.getQueue().remove();
        // Block here, allowing us to process records
        // serially.
        Logger.debug(LOG_TAG, "Invoking storeSerially...");
        this.storeSerially(record);
        Logger.debug(LOG_TAG, "Done with record.");
      }
      synchronized (monitor) {
        Logger.debug(LOG_TAG, "run() took monitor.");

        if (stopEventually) {
          Logger.debug(LOG_TAG, "Done with records and told to stop. Notifying consumer.");
          consumerIsDone();
          return;
        }
        try {
          Logger.debug(LOG_TAG, "Not told to stop but no records. Waiting.");
          monitor.wait(10000);
        } catch (InterruptedException e) {
          // TODO
        }
        Logger.debug(LOG_TAG, "run() dropped monitor.");
      }
    }
  }
}
