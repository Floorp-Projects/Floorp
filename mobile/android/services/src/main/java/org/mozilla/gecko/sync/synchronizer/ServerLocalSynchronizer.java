/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

/**
 * A <code>SynchronizerSession</code> designed to be used between a remote
 * server and a local repository.
 * <p>
 * See <code>ServerLocalSynchronizerSession</code> for error handling details.
 */
public class ServerLocalSynchronizer extends Synchronizer {
  @Override
  public SynchronizerSession newSynchronizerSession() {
    return new ServerLocalSynchronizerSession(this, this);
  }
}
