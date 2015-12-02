/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

public class NoSuchStageException extends Exception {
  private static final long serialVersionUID = 8338484472880746971L;
  GlobalSyncStage.Stage stage;
  public NoSuchStageException(GlobalSyncStage.Stage stage) {
    this.stage = stage;
  }
}
