/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;

public class ExpectFinishDelegate extends DefaultFinishDelegate {

  @Override
  public void onFinishSucceeded(RepositorySession session, RepositorySessionBundle bundle) {
    Logger.info("ExpectFinishDelegate", "Finish succeeded.");
    performNotify();
  }
}
