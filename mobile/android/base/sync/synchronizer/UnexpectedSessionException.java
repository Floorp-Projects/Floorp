/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

import org.mozilla.gecko.sync.SyncException;
import org.mozilla.gecko.sync.repositories.RepositorySession;

/**
 * An exception class that indicates that a session was passed
 * to a begin callback and wasn't expected.
 *
 * This shouldn't occur.
 *
 * @author rnewman
 *
 */
public class UnexpectedSessionException extends SyncException {
  private static final long serialVersionUID = 949010933527484721L;
  public RepositorySession session;

  public UnexpectedSessionException(RepositorySession session) {
    this.session = session;
  }
}
