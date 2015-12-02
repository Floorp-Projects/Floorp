/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

public abstract class RecordConsumer implements Runnable {

  public abstract void stored();

  /**
   * There are no more store items to arrive at the delegate.
   * When you're done, take care of finishing up.
   */
  public abstract void queueFilled();
  public abstract void halt();

  public abstract void doNotify();

  protected boolean stopImmediately = false;
  protected RecordsConsumerDelegate delegate;

  public RecordConsumer() {
    super();
  }
}