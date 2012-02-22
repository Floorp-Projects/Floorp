package org.mozilla.gecko.sync.jpake.stage;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.jpake.JPakeClient;

public class CompleteStage extends JPakeStage {

  @Override
  public void execute(JPakeClient jClient) {
    Logger.debug(LOG_TAG, "Exchange complete.");
    jClient.finished = true;
    jClient.complete(jClient.jCreds);
    jClient.runNextStage();
  }
}
