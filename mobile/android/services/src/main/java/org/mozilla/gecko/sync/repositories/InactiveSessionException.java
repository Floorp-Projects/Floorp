/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import org.mozilla.gecko.sync.SyncException;

public class InactiveSessionException extends SyncException {

  private static final long serialVersionUID = 537241160815940991L;

  public InactiveSessionException() {
    super();
  }

  public InactiveSessionException(Exception ex) {
    super(ex);
  }

}
