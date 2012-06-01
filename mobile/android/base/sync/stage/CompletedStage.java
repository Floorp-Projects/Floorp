/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import org.mozilla.gecko.sync.GlobalSession;


public class CompletedStage extends AbstractNonRepositorySyncStage {

  public CompletedStage(GlobalSession session) {
    super(session);
  }

  @Override
  public void execute() throws NoSuchStageException {
    // TODO: Update tracking timestamps, close connections, etc.
    // TODO: call clean() on each Repository in the sync constellation.
    session.completeSync();
  }
}
