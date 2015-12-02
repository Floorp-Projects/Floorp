/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import org.mozilla.gecko.sync.SyncException;

public class NoSyncIDException extends SyncException {
  public String engineName;
  public NoSyncIDException(String engineName) {
    this.engineName = engineName;
  }

  private static final long serialVersionUID = -4750430900197986797L;

}
