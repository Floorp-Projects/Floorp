/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;

import android.content.SyncResult;

public class AlreadySyncingException extends SyncException {
  Stage inState;
  public AlreadySyncingException(Stage currentState) {
    inState = currentState;
  }

  private static final long serialVersionUID = -5647548462539009893L;

  @Override
  public void updateStats(GlobalSession globalSession, SyncResult syncResult) {
  }
}
