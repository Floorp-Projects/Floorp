/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import org.mozilla.gecko.sync.GlobalSession;

public class UploadMetaGlobalStage extends AbstractNonRepositorySyncStage {
  public static final String LOG_TAG = "UploadMGStage";

  public UploadMetaGlobalStage(GlobalSession session) {
    super(session);
  }

  @Override
  public void execute() throws NoSuchStageException {
    if (session.hasUpdatedMetaGlobal()) {
      session.uploadUpdatedMetaGlobal();
    }
    session.advance();
  }
}
