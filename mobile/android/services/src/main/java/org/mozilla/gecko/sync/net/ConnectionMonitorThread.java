/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import org.mozilla.gecko.background.common.log.Logger;

/**
 * Every <code>REAP_INTERVAL</code> milliseconds, wake up
 * and expire any connections that need cleaning up.
 *
 * When we're told to shut down, take the connection manager
 * with us.
 */
public class ConnectionMonitorThread extends Thread {
  private static final long REAP_INTERVAL = 5000;     // 5 seconds.
  private static final String LOG_TAG = "ConnectionMonitorThread";

  private volatile boolean stopping;

  @Override
  public void run() {
    try {
      while (!stopping) {
        synchronized (this) {
          wait(REAP_INTERVAL);
          BaseResource.closeExpiredConnections();
        }
      }
    } catch (InterruptedException e) {
      Logger.trace(LOG_TAG, "Interrupted.");
    }
    BaseResource.shutdownConnectionManager();
  }

  public void shutdown() {
    Logger.debug(LOG_TAG, "ConnectionMonitorThread told to shut down.");
    stopping = true;
    synchronized (this) {
      notifyAll();
    }
  }
}
