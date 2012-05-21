/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

public interface SynchronizerDelegate {
  public void onSynchronized(Synchronizer synchronizer);
  public void onSynchronizeFailed(Synchronizer synchronizer, Exception lastException, String reason);
  public void onSynchronizeAborted(Synchronizer synchronize);
}
