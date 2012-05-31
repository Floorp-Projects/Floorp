/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

public interface SynchronizerSessionDelegate {
  public void onInitialized(SynchronizerSession session);

  public void onSynchronized(SynchronizerSession session);
  public void onSynchronizeFailed(SynchronizerSession session, Exception lastException, String reason);
  public void onSynchronizeSkipped(SynchronizerSession synchronizerSession);
}
