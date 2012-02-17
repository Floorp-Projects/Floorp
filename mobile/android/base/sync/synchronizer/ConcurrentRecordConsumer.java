/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Richard Newman <rnewman@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.sync.synchronizer;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.util.Log;

/**
 * Consume records from a queue inside a RecordsChannel, as fast as we can.
 * TODO: rewrite this in terms of an ExecutorService and a CompletionService.
 * See Bug 713483.
 *
 * @author rnewman
 *
 */
class ConcurrentRecordConsumer extends RecordConsumer {
  private static final String LOG_TAG = "CRecordConsumer";

  /**
   * When this is true and all records have been processed, the consumer
   * will notify its delegate.
   */
  protected boolean allRecordsQueued = false;
  private long counter = 0;

  public ConcurrentRecordConsumer(RecordsConsumerDelegate delegate) {
    this.delegate = delegate;
  }

  private static void info(String message) {
    Logger.info(LOG_TAG, message);
  }

  private static void debug(String message) {
    Logger.debug(LOG_TAG, message);
  }

  private static void trace(String message) {
    Logger.trace(LOG_TAG, message);
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
    debug("Queue filled.");
    synchronized (monitor) {
      this.allRecordsQueued = true;
      monitor.notify();
    }
  }

  @Override
  public void halt() {
    synchronized (monitor) {
      this.stopImmediately = true;
      monitor.notify();
    }
  }

  private Object countMonitor = new Object();
  @Override
  public void stored() {
    trace("Record stored. Notifying.");
    synchronized (countMonitor) {
      counter++;
    }
  }

  private void consumerIsDone() {
    info("Consumer is done. Processed " + counter + ((counter == 1) ? " record." : " records."));
    delegate.consumerIsDone(!allRecordsQueued);
  }

  @Override
  public void run() {
    while (true) {
      synchronized (monitor) {
        trace("run() took monitor.");
        if (stopImmediately) {
          debug("Stopping immediately. Clearing queue.");
          delegate.getQueue().clear();
          debug("Notifying consumer.");
          consumerIsDone();
          return;
        }
        debug("run() dropped monitor.");
      }
      // The queue is concurrent-safe.
      while (!delegate.getQueue().isEmpty()) {
        trace("Grabbing record...");
        Record record = delegate.getQueue().remove();
        trace("Storing record... " + delegate);
        try {
          delegate.store(record);
        } catch (Exception e) {
          // TODO: Bug 709371: track records that failed to apply.
          Log.e(LOG_TAG, "Caught error in store.", e);
        }
        trace("Done with record.");
      }
      synchronized (monitor) {
        trace("run() took monitor.");

        if (allRecordsQueued) {
          debug("Done with records and no more to come. Notifying consumerIsDone.");
          consumerIsDone();
          return;
        }
        if (stopImmediately) {
          debug("Done with records and told to stop immediately. Notifying consumerIsDone.");
          consumerIsDone();
          return;
        }
        try {
          debug("Not told to stop but no records. Waiting.");
          monitor.wait(10000);
        } catch (InterruptedException e) {
          // TODO
        }
        trace("run() dropped monitor.");
      }
    }
  }
}
