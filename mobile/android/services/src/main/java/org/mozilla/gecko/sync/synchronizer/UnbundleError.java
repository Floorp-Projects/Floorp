/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

import org.mozilla.gecko.sync.SyncException;
import org.mozilla.gecko.sync.repositories.RepositorySession;

public class UnbundleError extends SyncException {
  private static final long serialVersionUID = -8709503281041697522L;

  public RepositorySession failedSession;

  public UnbundleError(Exception e, RepositorySession session) {
    super(e);
    this.failedSession = session;
  }
}
